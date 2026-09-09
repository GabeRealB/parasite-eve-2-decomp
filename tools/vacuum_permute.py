#!/usr/bin/env python3
"""Pick a ≥95% scratch seed and run decomp-permuter until score 0 or timeout.

Intended as a vacuum *post-step* after the agent gives up. Prefers an unpinned
seed (register-asm pins shrink the search). Uses structural diagnostics to select several candidates within one time budget.
Always records selection and skip reasons. Prints a STATUS=
line for vacuum.sh.

Exit codes:
  0  permuter produced output-0-* (PERMUTER_HIT=...)
  1  ran and did not hit 0 (PERMUTER_MISS=...)
  2  skipped (PERMUTER_SKIP=...)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
MIN_SCORE_DEFAULT = 95.0
UNPINNED_WINDOW = 1.0
REGISTER_ASM_RE = re.compile(
    r'\bregister\s+([^;]*?)\s+(?:asm|__asm__)\s*\(\s*"[^"]*"\s*\)',
    re.S,
)


PENALTY_KEYS = ("stack", "branch", "regs", "reorder", "insert", "delete")


@dataclass
class Seed:
    path: Path
    score: float
    pinned: bool
    penalties: Optional[dict] = None
    diagnosis: Optional[dict] = None

    @property
    def source_hash(self):
        return hashlib.sha256(self.path.read_bytes()).hexdigest()

    @property
    def barriers(self):
        return len(re.findall(r'\b(?:SOFT_BARRIER|SCHED_BARRIER|asm|__asm__)\s*\(', self.path.read_text()))


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
    logs = [scratch / 'prior_match_log.txt', scratch / 'match_log.txt']
    if not any(log.is_file() for log in logs):
        return []
    observations = {}
    journal = scratch / 'attempts.jsonl'
    if journal.is_file():
        for line in journal.read_text().splitlines():
            try:
                row = json.loads(line)
                observations[row['source']] = row
            except (ValueError, KeyError):
                continue
    seeds: dict[str, Seed] = {}
    lines = [line for log in logs if log.is_file()
             for line in log.read_text(encoding='utf-8', errors='replace').splitlines()]
    for line in lines:
        parts = line.split()
        if len(parts) < 2:
            continue
        name = Path(parts[0]).name
        if not re.fullmatch(r"[\w.-]+\.c", name):
            continue
        try:
            score = float(parts[1].rstrip("%"))
        except ValueError:
            continue
        path = scratch / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        observation = observations.get(name)
        if observation:
            if observation.get('failure') or observation.get('source_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
                continue
            score = observation.get('score', score)
        diagnosis = None
        sidecar = path.with_suffix('.diagnosis.json')
        if sidecar.is_file():
            try:
                diagnosis = json.loads(sidecar.read_text())
                if diagnosis.get('source_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
                    diagnosis = None
            except (ValueError, OSError):
                pass
        seeds[name] = Seed(
                path=path,
                score=score,
                pinned=bool(REGISTER_ASM_RE.search(text)),
                penalties=parse_penalties(parts[2:]),
                diagnosis=diagnosis,
            )
    return list(seeds.values())


def eligibility(seed: Seed, min_score: float) -> tuple[bool, str]:
    if seed.pinned:
        return False, 'register-pinned source; unpin and rebuild as a separate experiment'
    if seed.score >= 100:
        return False, 'score is already zero; port and run full verification'
    if seed.score < min_score - 5:
        return False, f'score below alternate floor {min_score - 5:g}%'
    diagnosis = seed.diagnosis or {}
    target = seed.path.parent / 'target.o'
    candidate = seed.path.with_suffix('.o')
    for path, key in ((target, 'target_sha256'), (candidate, 'object_sha256')):
        if not path.is_file() or diagnosis.get(key) != hashlib.sha256(path.read_bytes()).hexdigest():
            diagnosis = {}
            break
    if diagnosis.get('topology') == 'different':
        return False, 'block connections differ; inspect .jump/.jump2'
    if diagnosis.get('topology') == 'match':
        if not diagnosis.get('predicates_match') or not diagnosis.get('calls_match'):
            return False, 'branch predicates or call targets differ or are unresolved'
        # Structure matching is evidence for a trial, not semantic validation.
        return True, 'block connections, predicates and call targets match; bounded allocation/scheduling trial'
    penalties = seed.penalties
    if seed.score >= min_score and penalties and all(penalties.get(k) == 0 for k in ('branch', 'insert', 'delete')):
        return True, 'legacy register/scheduling-only penalties; structure unavailable'
    return False, 'structure unknown; rebuild or investigate indirect control flow before search'


def search_candidates(seeds: list[Seed], min_score: float, limit: int) -> list[Seed]:
    """A bounded, source-distinct set, including lower-score alternatives."""
    pool = list({s.source_hash: s for s in seeds if s.score >= min_score - 5 and not s.pinned}.values())
    if not pool or max(s.score for s in pool) < min_score:
        return []
    first = max(pool, key=lambda s: s.score)
    chosen = [first]
    pool.remove(first)
    axes = [lambda s: sum((s.penalties or {}).get(k, 10**6) for k in ('branch', 'insert', 'delete')),
            lambda s: s.barriers, lambda s: (s.penalties or {}).get('regs', 10**6),
            lambda s: (s.penalties or {}).get('reorder', 10**6)]
    for cost in axes:
        if len(chosen) >= limit or not pool:
            break
        candidate = min(pool, key=lambda s: (cost(s), -s.score, s.source_hash))
        chosen.append(candidate)
        pool.remove(candidate)
    return chosen


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
    return REGISTER_ASM_RE.sub(r"\1", src)


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


def run_bounded(cmd, *, timeout, **kwargs):
    """A timeout also terminates compiler/search descendants, not just the shell."""
    with subprocess.Popen(cmd, start_new_session=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, **kwargs) as process:
        try:
            stdout, stderr = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
            raise
        return subprocess.CompletedProcess(cmd, process.returncode, stdout, stderr)


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
    started = time.monotonic()
    setup = run_bounded(
        ["./permute.sh", "--clean", func, str(asm_file), str(seed)],
        cwd=REPO_ROOT,
        text=True,
        timeout=max(1, min(60, timeout)),
    )
    timeout = timeout - (time.monotonic() - started)
    if timeout <= 0:
        return 'budget exhausted during setup', None
    if setup.returncode != 0:
        return (
            f"setup failed: {(setup.stderr or setup.stdout).strip()[-400:]}",
            None,
        )

    cmd = [
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
        try:
            proc = run_bounded(cmd, cwd=REPO_ROOT, text=True, env=env, timeout=timeout)
        except subprocess.TimeoutExpired:
            winner = find_zero_output(perm_dir)
            return f'timeout after {timeout:.1f}s', winner
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
    parser.add_argument('--func', required=True)
    parser.add_argument('--scratch', required=True, type=Path)
    parser.add_argument('--asm', default='')
    parser.add_argument('--min-score', type=float, default=MIN_SCORE_DEFAULT)
    parser.add_argument('--timeout', type=int, default=360, help='Total budget across rebuilds, setup and all searches')
    parser.add_argument('--jobs', type=int, default=0)
    parser.add_argument('--max-seeds', type=int, default=3)
    parser.add_argument('--setup-only', action='store_true', help='Report selection and eligibility without compiling/searching')
    args = parser.parse_args(argv)
    if args.timeout < 1 or not 1 <= args.max_seeds <= 6:
        parser.error('timeout must be positive; max-seeds must be 1..6')
    scratch = args.scratch.absolute()
    if not scratch.is_dir():
        print(f'PERMUTER_SKIP=no scratch at {scratch}')
        return 2
    report = {'func': args.func, 'budget_seconds': args.timeout, 'candidates': [], 'status': 'pending'}
    def finish(code, status):
        report['status'] = status
        (scratch / 'PERMUTER.json').write_text(json.dumps(report, indent=2) + '\n')
        lines = [f'status={status}']
        lines += [f"{r['seed']}: {r.get('status', '')} {r.get('reason', '')}" for r in report['candidates']]
        (scratch / 'PERMUTER.txt').write_text('\n'.join(lines) + '\n')
        if args.setup_only:
            for line in lines[1:]:
                print('PERMUTER_CANDIDATE=' + line)
        print(('PERMUTER_MISS=' if code == 1 else 'PERMUTER_SKIP=' if code == 2 else 'PERMUTER_DONE=') + status)
        return code
    seeds = parse_match_log(scratch)
    selected = search_candidates(seeds, args.min_score, args.max_seeds)
    if not selected:
        report['candidates'] = [{'seed': s.path.name, 'score': s.score, 'status': 'skipped',
                                 'reason': eligibility(s, args.min_score)[1]} for s in seeds]
        return finish(2, 'no unpinned seed reaches the primary score threshold')
    deadline = time.monotonic() + args.timeout
    asm_file = Path(args.asm).absolute() if args.asm else None
    if not args.setup_only and asm_file is None:
        try:
            loc = subprocess.run([sys.executable, str(REPO_ROOT / 'tools/decomp_overlay.py'), 'find', args.func, '--json'],
                                 cwd=REPO_ROOT, capture_output=True, text=True, timeout=min(30, args.timeout))
            asm_file = REPO_ROOT / json.loads(loc.stdout)['asm_file']
        except (ValueError, KeyError, subprocess.TimeoutExpired):
            return finish(2, 'cannot resolve target assembly')
    if not args.setup_only and not asm_file.is_file():
        return finish(2, 'target assembly is missing')
    ran = False
    for index, seed in enumerate(selected):
        entry = {'seed': seed.path.name, 'source_sha256': seed.source_hash, 'score': seed.score}
        report['candidates'].append(entry)
        left = deadline - time.monotonic()
        if left < 1:
            entry.update(status='skipped', reason='total budget exhausted')
            continue
        if not args.setup_only:
            # Archived scores/diagnostics describe old headers and compiler inputs.
            # Rebuild each selected source before deciding whether to search it.
            try:
                build = run_bounded(['bash', str(scratch / 'build.sh'), seed.path.name], cwd=scratch,
                                    text=True, timeout=max(1, min(60, left)))
                (scratch / (seed.path.stem + '.permute-build.log')).write_text(build.stdout + build.stderr)
                if build.returncode:
                    entry.update(status='skipped', reason=f'candidate rebuild failed ({build.returncode})')
                    continue
                seed = next(s for s in parse_match_log(scratch) if s.path == seed.path)
                entry['score'] = seed.score
                score_file = seed.path.with_suffix('.score.json')
                if score_file.is_file() and json.loads(score_file.read_text()).get('distance') == 0:
                    report['winner'] = str(seed.path)
                    entry.update(status='score zero on rebuild', reason='port and full verification required')
                    finish(0, 'score zero found; port and full build verification required')
                    print(f'PERMUTER_SEED={seed.path} score={seed.score:.3f}% pinned=0')
                    print(f'PERMUTER_HIT={seed.path}')
                    return 0
            except (subprocess.TimeoutExpired, StopIteration, OSError) as exc:
                entry.update(status='skipped', reason=f'candidate rebuild unavailable: {exc}')
                continue
        allowed, reason = eligibility(seed, args.min_score)
        entry.update(reason=reason, diagnosis=seed.diagnosis, status='eligible' if allowed else 'skipped')
        if not allowed or args.setup_only:
            continue
        remaining = deadline - time.monotonic()
        if remaining < 1:
            entry.update(status='skipped', reason='total budget exhausted')
            continue
        budget = max(1, int(remaining / (len(selected) - index)))
        print(f'PERMUTER_SEED={seed.path} score={seed.score:.3f}% pinned=0')
        print(f'PERMUTER_RUN timeout={budget}s jobs={_nproc_jobs(args.jobs)}')
        ran = True
        try:
            status, winner = run_permuter(args.func, scratch, asm_file, seed.path, budget, _nproc_jobs(args.jobs))
        except (subprocess.TimeoutExpired, OSError) as exc:
            status, winner = f'setup/search failed: {exc}', None
        entry.update(status=status, budget_seconds=budget)
        if winner:
            report['winner'] = str(winner)
            finish(0, 'score zero found; port and full build verification required')
            print(f'PERMUTER_HIT={winner}')
            return 0
    return finish(1 if ran else 2, 'search finished without score zero' if ran else 'setup-only' if args.setup_only else 'all candidates skipped')


if __name__ == '__main__':
    raise SystemExit(main())
