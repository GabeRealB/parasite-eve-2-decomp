#!/usr/bin/env python3
"""Pick a ≥95% scratch seed and run decomp-permuter until score 0 or timeout.

Intended as a vacuum *post-step* after the agent gives up. Prefers an unpinned
seed (register-asm pins shrink the search). Skips when match_log penalties
show insert/delete/branch dominating regs/reorder/stack. Prints a STATUS=
line for vacuum.sh.

Exit codes:
  0  permuter produced output-0-* (PERMUTER_HIT=...)
  1  ran and did not hit 0 (PERMUTER_MISS=...)
  2  skipped (PERMUTER_SKIP=...)
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
MIN_SCORE_DEFAULT = 95.0
UNPINNED_WINDOW = 1.0
ASM_PIN_RE = re.compile(r'(?<!_)\basm\s*\(\s*"[^"]*"\s*\)')
REGISTER_ASM_RE = re.compile(
    r'\bregister\s+([^;]*?)\s+asm\s*\(\s*"[^"]*"\s*\)',
    re.S,
)


PENALTY_KEYS = ("stack", "branch", "regs", "reorder", "insert", "delete")
# Same weights as tools/claude-decomp-env/dist.py
PENALTY_WEIGHTS = {
    "stack": 1,
    "branch": 1,
    "regs": 5,
    "reorder": 60,
    "insert": 100,
    "delete": 100,
}


@dataclass
class Seed:
    path: Path
    score: float
    pinned: bool
    penalties: Optional[dict] = None


def parse_penalties(fields: list[str]) -> Optional[dict]:
    out: dict[str, int] = {}
    for field in fields:
        if "=" not in field:
            continue
        key, _, raw = field.partition("=")
        if key not in PENALTY_KEYS:
            continue
        try:
            out[key] = int(raw)
        except ValueError:
            return None
    return out or None


def parse_match_log(scratch: Path) -> list[Seed]:
    log = scratch / "match_log.txt"
    if not log.is_file():
        return []
    seeds: list[Seed] = []
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        name = Path(parts[0]).name
        if not re.match(r"base_\d+\.c$", name):
            continue
        try:
            score = float(parts[1].rstrip("%"))
        except ValueError:
            continue
        path = scratch / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        seeds.append(
            Seed(
                path=path,
                score=score,
                pinned=bool(ASM_PIN_RE.search(text)),
                penalties=parse_penalties(parts[2:]),
            )
        )
    return seeds


def cfg_dominates(penalties: Optional[dict]) -> bool:
    """True when insert/delete/branch outweigh regs/reorder/stack.

    Missing penalties (old match_log lines) must not skip the permuter.
    """
    if not penalties:
        return False
    cfg = sum(penalties.get(k, 0) * PENALTY_WEIGHTS[k] for k in ("branch", "insert", "delete"))
    rest = sum(penalties.get(k, 0) * PENALTY_WEIGHTS[k] for k in ("stack", "regs", "reorder"))
    return cfg > rest


def format_penalties(penalties: Optional[dict]) -> str:
    if not penalties:
        return ""
    return " ".join(f"{k}={penalties.get(k, 0)}" for k in PENALTY_KEYS)


def pick_seed(seeds: list[Seed], min_score: float) -> Optional[Seed]:
    eligible = [s for s in seeds if s.score >= min_score]
    if not eligible:
        return None
    best = max(s.score for s in eligible)
    unpinned = [s for s in eligible if not s.pinned and s.score >= best - UNPINNED_WINDOW]
    pool = unpinned or eligible
    pool.sort(key=lambda s: (-s.score, s.path.name))
    return pool[0]


def strip_register_asm(src: str) -> str:
    """Drop GNU `register ... asm("reg")` pins. Leave `__asm__` / GTE macros."""
    out = REGISTER_ASM_RE.sub(r"\1", src)
    out = ASM_PIN_RE.sub("", out)
    return out


def _python() -> str:
    venv = REPO_ROOT / "venv" / "bin" / "python"
    return str(venv) if venv.is_file() else sys.executable


def _nproc_jobs(requested: int) -> int:
    if requested > 0:
        return requested
    return max(1, min(os.cpu_count() or 4, 8))


def find_zero_output(perm_dir: Path) -> Optional[Path]:
    hits = sorted(perm_dir.glob("output-0-*"))
    for hit in hits:
        src = hit / "source.c"
        if src.is_file():
            return src
    return None


def run_permuter(
    func: str,
    scratch: Path,
    asm_file: Path,
    seed: Path,
    timeout: int,
    jobs: int,
) -> tuple[str, Optional[Path]]:
    """Set up permuter/<func>/ from seed and search. Returns (status, winner)."""
    perm_dir = REPO_ROOT / "permuter" / func
    setup = subprocess.run(
        ["./permute.sh", "--clean", func, str(asm_file), str(seed)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )
    if setup.returncode != 0:
        return (
            f"setup failed: {(setup.stderr or setup.stdout).strip()[-400:]}",
            None,
        )

    cmd = [
        "timeout",
        "--kill-after=20s",
        str(timeout),
        _python(),
        str(REPO_ROOT / "tools" / "decomp-permuter" / "permuter.py"),
        f"-j{jobs}",
        "--better-only",
        "--stop-on-zero",
        "--no-context-output",
        "--algorithm",
        "levenshtein",
        str(perm_dir),
    ]
    # Private TMPDIR that goes away with the run: the permuter leaves a scratch
    # file per candidate compile behind, and a shared /tmp fills up (12G in one
    # long search here). permute.sh's cleaner does not cover this call path.
    with tempfile.TemporaryDirectory(prefix=f"permuter-{func}-") as tmpdir:
        env = {**os.environ, "TMPDIR": tmpdir}
        proc = subprocess.run(
            cmd, cwd=REPO_ROOT, capture_output=True, text=True, env=env
        )
    winner = find_zero_output(perm_dir)
    log_tail = (proc.stdout or "")[-1500:]
    if winner is not None:
        return "hit", winner
    if proc.returncode == 124:
        return f"timeout after {timeout}s\n{log_tail}", None
    if proc.returncode != 0:
        return f"permuter exit {proc.returncode}\n{log_tail}", None
    return f"finished without score 0\n{log_tail}", None


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True)
    parser.add_argument("--scratch", required=True, type=Path)
    parser.add_argument("--asm", default="", help="Override asm .s path")
    parser.add_argument("--min-score", type=float, default=MIN_SCORE_DEFAULT)
    parser.add_argument("--timeout", type=int, default=360)
    parser.add_argument("--jobs", type=int, default=0)
    parser.add_argument(
        "--setup-only",
        action="store_true",
        help="Pick the seed and print it; do not run the permuter",
    )
    args = parser.parse_args(argv)

    scratch = args.scratch if args.scratch.is_absolute() else REPO_ROOT / args.scratch
    if not scratch.is_dir():
        print(f"PERMUTER_SKIP=no scratch at {scratch}")
        return 2

    seeds = parse_match_log(scratch)
    seed = pick_seed(seeds, args.min_score)
    if seed is None:
        best = max((s.score for s in seeds), default=None)
        why = (
            f"best {best:.3f}% below {args.min_score:g}%"
            if best is not None
            else "no match_log scores"
        )
        print(f"PERMUTER_SKIP={why}")
        return 2

    print(
        f"PERMUTER_SEED={seed.path.relative_to(REPO_ROOT) if seed.path.is_relative_to(REPO_ROOT) else seed.path} "
        f"score={seed.score:.3f}% pinned={int(seed.pinned)}"
        + (f" {format_penalties(seed.penalties)}" if seed.penalties else "")
    )

    if cfg_dominates(seed.penalties):
        print(
            "PERMUTER_SKIP=cfg leftovers dominate "
            f"({format_penalties(seed.penalties)})"
        )
        return 2

    work_seed = seed.path
    if seed.pinned:
        stripped = strip_register_asm(
            seed.path.read_text(encoding="utf-8", errors="replace")
        )
        if stripped != seed.path.read_text(encoding="utf-8", errors="replace"):
            work_seed = scratch / "permute_seed.c"
            work_seed.write_text(stripped, encoding="utf-8")
            print(f"PERMUTER_STRIPPED={work_seed}")

    if args.setup_only:
        print("PERMUTER_SKIP=setup-only")
        return 2

    asm_file = Path(args.asm) if args.asm else None
    if asm_file is None or not str(asm_file):
        overlay = REPO_ROOT / "tools" / "decomp_overlay.py"
        loc = subprocess.run(
            [sys.executable, str(overlay), "find", args.func, "--json"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )
        if loc.returncode != 0:
            print(f"PERMUTER_SKIP=cannot resolve asm for {args.func}")
            return 2
        import json

        asm_file = REPO_ROOT / json.loads(loc.stdout)["asm_file"]

    if not asm_file.is_file():
        print(f"PERMUTER_SKIP=missing asm {asm_file}")
        return 2

    jobs = _nproc_jobs(args.jobs)
    print(f"PERMUTER_RUN timeout={args.timeout}s jobs={jobs}")
    status, winner = run_permuter(
        args.func, scratch, asm_file, work_seed, args.timeout, jobs
    )
    report = scratch / "PERMUTER.txt"
    report.write_text(
        f"seed={seed.path}\nscore={seed.score}\npinned={seed.pinned}\n"
        f"work_seed={work_seed}\nstatus={status}\nwinner={winner}\n",
        encoding="utf-8",
    )
    if winner is not None:
        rel = winner.relative_to(REPO_ROOT) if winner.is_relative_to(REPO_ROOT) else winner
        print(f"PERMUTER_HIT={rel}")
        return 0
    print(f"PERMUTER_MISS={status.splitlines()[0]}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
