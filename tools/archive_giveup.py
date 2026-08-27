#!/usr/bin/env python3
"""Save the best scratch seed so a give-up can be retried later.

Vacuum deletes nonmatchings/<func>/ after each iteration. This copies the
best-scoring C (plus match_log / brief / permuter notes) to
tools/giveups/<func>/, replacing an older archive only when the new score
is higher.

Exit 0 on write, 2 if skipped.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vacuum_permute as vp  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent
GIVEUPS = REPO_ROOT / "tools" / "giveups"


def _rel(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def load_meta(dest: Path) -> Optional[dict]:
    meta = dest / "meta.json"
    if not meta.is_file():
        return None
    try:
        return json.loads(meta.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def pick_any_seed(scratch: Path) -> Optional[vp.Seed]:
    seeds = vp.parse_match_log(scratch)
    if seeds:
        return max(seeds, key=lambda s: (s.score, not s.pinned, s.path.name))
    numbered = sorted(
        scratch.glob("base_*.c"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    for path in numbered + ([scratch / "base.c"] if (scratch / "base.c").is_file() else []):
        if path.is_file() and path.stat().st_size > 0:
            text = path.read_text(encoding="utf-8", errors="replace")
            return vp.Seed(
                path=path,
                score=0.0,
                pinned=bool(vp.ASM_PIN_RE.search(text)),
            )
    return None


def overlay_info(func: str) -> dict:
    loc = {}
    try:
        import decomp_overlay as ov

        found = ov.find_function(func)
        if found is not None:
            loc = ov.loc_to_dict(found)
    except Exception:
        pass
    return loc


def archive(func: str, scratch: Path) -> tuple[int, str]:
    if not scratch.is_dir():
        return 2, f"GIVEUP_SKIP=no scratch at {scratch}"

    seed = pick_any_seed(scratch)
    if seed is None:
        return 2, "GIVEUP_SKIP=no C seed in scratch"

    dest = GIVEUPS / func
    prev = load_meta(dest)
    if prev is not None:
        try:
            old_score = float(prev.get("score", 0))
        except (TypeError, ValueError):
            old_score = 0.0
        if old_score >= seed.score and (dest / "base.c").is_file():
            return 2, (
                f"GIVEUP_SKIP=keep existing {old_score:.3f}% "
                f"(new {seed.score:.3f}%)"
            )

    dest.mkdir(parents=True, exist_ok=True)
    for leftover in dest.iterdir():
        if leftover.is_file():
            leftover.unlink()

    shutil.copy2(seed.path, dest / seed.path.name)
    shutil.copy2(seed.path, dest / "base.c")
    if (scratch / "match_log.txt").is_file():
        shutil.copy2(scratch / "match_log.txt", dest / "match_log.txt")
    else:
        (dest / "match_log.txt").write_text(
            f"{seed.path.name} {seed.score:.3f}%\n", encoding="utf-8"
        )
    for extra in ("BRIEF.md", "PERMUTER.txt", "permute_seed.c"):
        src = scratch / extra
        if src.is_file():
            shutil.copy2(src, dest / extra)

    loc = overlay_info(func)
    meta = {
        "func": func,
        "score": seed.score,
        "seed_name": seed.path.name,
        "pinned": seed.pinned,
        "attempts": sum(1 for _ in scratch.glob("base_*.c")) + 1,
        "archived_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "overlay": loc.get("overlay"),
        "asm_file": loc.get("asm_file"),
        "c_file": loc.get("c_file"),
        "scratch": _rel(scratch),
    }
    (dest / "meta.json").write_text(
        json.dumps(meta, indent=2) + "\n", encoding="utf-8"
    )
    rel = _rel(dest)
    return 0, f"GIVEUP_SAVED={rel} score={seed.score:.3f}% seed={seed.path.name}"


def clear(func: str) -> str:
    dest = GIVEUPS / func
    if dest.is_dir():
        shutil.rmtree(dest)
        return f"GIVEUP_CLEARED={dest.relative_to(REPO_ROOT)}"
    return f"GIVEUP_SKIP=no archive for {func}"


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True)
    parser.add_argument("--scratch", type=Path, default=None)
    parser.add_argument(
        "--clear",
        action="store_true",
        help="Remove tools/giveups/<func>/ (after a successful match)",
    )
    args = parser.parse_args(argv)

    if args.clear:
        print(clear(args.func))
        return 0
    if args.scratch is None:
        print("GIVEUP_SKIP=--scratch is required unless --clear", file=sys.stderr)
        return 2
    scratch = args.scratch if args.scratch.is_absolute() else REPO_ROOT / args.scratch
    code, msg = archive(args.func, scratch)
    print(msg)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
