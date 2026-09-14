#!/usr/bin/env python3
"""Resolve a conflicted configs/USA/overlays.toml during a replay, entry by entry.

`vacuum_overlay.sh` lands a finished sweep by replaying the branch's own
commits. That replay conflicts on the manifest far more often than it should,
and the conflict is almost never a real disagreement: the manifest is one line
per overlay, git resolves it by hunk, and a hunk covers several *adjacent*
lines. So a commit that edits `actor_400100` conflicts with a trunk that edited
`actor_161500` two lines above it, purely because the two entries are
neighbours.

Taking a side wholesale is what makes that dangerous. `--theirs` on the
actor_400100_text replay would have built green while silently reverting two
shared units another lane had just landed in `actor_161500`; `--ours` would
have dropped the promotion the replay exists to carry. Both losses are
invisible to the checksum, because a reverted `shared` span just means the body
stays in assembly.

Resolving per entry removes the guesswork. For each key in the conflict, compare
both sides against the same key in the picked commit's own parent:

  * the commit did not touch this key  -> keep trunk's line
  * trunk did not touch this key       -> take the commit's line
  * both touched it                    -> refuse, and say which

The third case is the only one needing judgement, and it is rare: it means two
sweeps promoted different bodies out of the same third overlay. Exit 2 leaves
the conflict in place for a human or an agent, rather than guessing.

Exit codes: 0 resolved, 1 nothing to do, 2 needs a human.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

MANIFEST = "configs/USA/overlays.toml"
CONFLICT = re.compile(
    r"^<<<<<<< [^\n]*\n(.*?)^=======\n(.*?)^>>>>>>> [^\n]*\n", re.S | re.M)
ENTRY = re.compile(r"^(\w+) = ")


def git(*args: str, root: Path, check: bool = True) -> str:
    r = subprocess.run(["git", "-C", str(root), *args],
                       capture_output=True, text=True)
    if check and r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} failed:\n{r.stderr}")
    return r.stdout


def picked_parent(root: Path, override: str) -> str:
    """The commit the conflicting change is measured against."""
    if override:
        return override
    for ref in ("CHERRY_PICK_HEAD", "MERGE_HEAD", "REVERT_HEAD"):
        out = git("rev-parse", "--verify", "--quiet", ref, root=root, check=False)
        if out.strip():
            return git("rev-parse", f"{out.strip()}^", root=root).strip()
    raise SystemExit("no cherry-pick, merge or revert in progress, and no "
                     "--parent given; nothing to measure against")


def entry_in(root: Path, rev: str, key: str) -> str | None:
    text = git("show", f"{rev}:{MANIFEST}", root=root, check=False)
    for line in text.splitlines():
        if line.startswith(key + " = "):
            return line
    return None


def resolve(root: Path, parent: str) -> int:
    path = root / MANIFEST
    src = path.read_text()
    if "<<<<<<<" not in src:
        print(f"{MANIFEST}: no conflict markers", file=sys.stderr)
        return 1

    undecidable: list[str] = []
    regions = 0

    def fix(m: re.Match) -> str:
        nonlocal regions
        regions += 1
        ours = m.group(1).splitlines()
        theirs = m.group(2).splitlines()
        order, seen = [], set()
        for line in ours + theirs:
            em = ENTRY.match(line)
            if not em:
                # Anything that is not one `key = {...}` line is outside what
                # this tool can reason about; leave the whole region alone.
                undecidable.append(f"non-entry line in conflict: {line[:60]!r}")
                return m.group(0)
            if em.group(1) not in seen:
                seen.add(em.group(1))
                order.append(em.group(1))
        out = []
        for key in order:
            o = next((l for l in ours if l.startswith(key + " = ")), None)
            t = next((l for l in theirs if l.startswith(key + " = ")), None)
            b = entry_in(root, parent, key)
            if t is None:
                out.append(o)
            elif o is None:
                out.append(t)
            elif t == b:
                out.append(o)
            elif o == b:
                out.append(t)
            else:
                undecidable.append(key)
                return m.group(0)
        return "\n".join(out) + "\n"

    new, n = CONFLICT.subn(fix, src)
    if undecidable:
        print("cannot resolve automatically; both sides changed: "
              + ", ".join(sorted(set(undecidable))), file=sys.stderr)
        return 2
    if "<<<<<<<" in new or ">>>>>>>" in new:
        print("conflict markers remain after resolution", file=sys.stderr)
        return 2
    path.write_text(new)
    print(f"  resolved {n} manifest conflict region(s) against {parent[:9]}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=".", help="repository or worktree to act in")
    ap.add_argument("--parent", default="",
                    help="commit to compare both sides against; defaults to the "
                         "parent of the in-progress cherry-pick/merge")
    args = ap.parse_args()
    root = Path(args.root).resolve()
    return resolve(root, picked_parent(root, args.parent))


if __name__ == "__main__":
    sys.exit(main())
