#!/usr/bin/env python3
"""Land a whole-overlay worktree onto trunk as one `matched <fn> <attempts>` commit each.

The landing has to rewrite trunk's files rather than merge the branch, because
trunk moves under long-running overlay sessions. It also has to produce one
commit per function: tools/fit_difficulty_model.py trains on `matched <fn>
<attempts>` commit subjects, so folding a session's work into a single commit
throws away one data point per function.

So the bodies are applied forward, one at a time, onto trunk's own files. Every
intermediate state compiles by construction - each function is either its final
body or the INCLUDE_ASM line it replaces, and both forms build - so the build is
run once at the end rather than once per commit.

Usage:
    python3 tools/land_overlay.py <worktree> <overlay> --functions f1,f2,...
    python3 tools/land_overlay.py <worktree> <overlay> --functions-file list.txt
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FUNC_START = re.compile(r"^[A-Za-z_][\w \*]*\b(func_\w+)\s*\(\s*[^;]*$")
ASM_LINE = re.compile(r'INCLUDE_ASM\("[^"]+",\s*(\w+)\)')


def bodies_of(text: str) -> dict[str, str]:
    lines, out, i = text.splitlines(keepends=True), {}, 0
    while i < len(lines):
        m = FUNC_START.match(lines[i])
        if m and not lines[i].lstrip().startswith(("INCLUDE_", "extern")):
            start = i
            while i < len(lines) and lines[i].rstrip() != "}":
                i += 1
            out[m.group(1)] = "".join(lines[start:i + 1])
            i += 1
            continue
        i += 1
    return out


def scaffold(trunk_text: str, wt_text: str, funcs: set[str]) -> str:
    """The worktree file with every landing function put back as INCLUDE_ASM.

    This carries across the includes, statics and types the bodies need without
    yet claiming any function, so the first per-function commit is a real diff
    of one function rather than a mixed blob.
    """
    asm_for = {m.group(1): l for l in trunk_text.splitlines(keepends=True)
               if (m := ASM_LINE.search(l))}
    bodies = bodies_of(wt_text)
    out = wt_text
    for fn in funcs:
        if fn in bodies and fn in asm_for:
            out = out.replace(bodies[fn], asm_for[fn].rstrip("\n"), 1)
    return out


def git(*args: str) -> str:
    r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} failed:\n{r.stderr}")
    return r.stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("worktree")
    ap.add_argument("overlay")
    ap.add_argument("--functions")
    ap.add_argument("--functions-file")
    ap.add_argument("--attempts", type=int, default=1)
    ap.add_argument("--extra", default="",
                    help="comma-separated extra paths to land (headers, docs)")
    args = ap.parse_args()

    wt = Path(args.worktree)
    funcs = ([f.strip() for f in args.functions.split(",") if f.strip()]
             if args.functions
             else [l.strip() for l in Path(args.functions_file).read_text().splitlines()
                   if l.strip()])

    if git("status", "--porcelain", "--", "src", "include").strip():
        raise SystemExit("trunk has uncommitted src/include changes; refusing")

    # Which trunk file holds each function, and where its final body lives.
    src_dirs = [p for p in ROOT.glob(f"src/**/{args.overlay}") if p.is_dir()]
    if len(src_dirs) != 1:
        raise SystemExit(f"expected one src dir for {args.overlay}")
    rel = src_dirs[0].relative_to(ROOT)

    file_of, body_of = {}, {}
    for trunk_c in sorted(src_dirs[0].glob("*.c")):
        wt_c = wt / rel / trunk_c.name
        if not wt_c.is_file():
            continue
        wt_bodies = bodies_of(wt_c.read_text())
        for fn in funcs:
            if fn in wt_bodies and ASM_LINE.search(trunk_c.read_text()) and \
               re.search(rf'INCLUDE_ASM\("[^"]+",\s*{fn}\)', trunk_c.read_text()):
                file_of[fn] = trunk_c
                body_of[fn] = wt_bodies[fn]

    missing = [f for f in funcs if f not in file_of]
    if missing:
        raise SystemExit(f"no INCLUDE_ASM slot on trunk for: {missing}")

    # Stage 1: scaffolding (includes, statics, new types) plus any extra paths.
    touched = set(file_of.values())
    for trunk_c in touched:
        wt_c = wt / rel / trunk_c.name
        mine = {f for f in funcs if file_of[f] == trunk_c}
        trunk_c.write_text(scaffold(trunk_c.read_text(), wt_c.read_text(), mine))
    extras = [e.strip() for e in args.extra.split(",") if e.strip()]
    for e in extras:
        src, dst = wt / e, ROOT / e
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(src.read_text())

    # Stage 2: one function, one commit.
    for fn in funcs:
        trunk_c = file_of[fn]
        text = trunk_c.read_text()
        m = re.search(rf'^.*INCLUDE_ASM\("[^"]+",\s*{fn}\).*$', text, re.M)
        if not m:
            raise SystemExit(f"{fn}: INCLUDE_ASM line vanished from {trunk_c}")
        trunk_c.write_text(text.replace(m.group(0), body_of[fn].rstrip("\n"), 1))
        paths = [str(trunk_c.relative_to(ROOT))] + (extras if fn == funcs[0] else [])
        git("add", *paths)
        git("commit", "-q", "-m", f"matched {fn} {args.attempts}")
        print(f"  matched {fn}")

    print(f"landed {len(funcs)} function(s) from {args.overlay}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
