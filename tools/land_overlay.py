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


def sections(text: str) -> dict[str, str]:
    parts, title, buf = {}, None, []
    for line in text.splitlines(keepends=True):
        if line.startswith("## "):
            if title is not None:
                parts[title] = "".join(buf)
            title, buf = line.strip(), [line]
        elif title is not None:
            buf.append(line)
    if title is not None:
        parts[title] = "".join(buf)
    return parts


def merge_sections(current: str, incoming: str) -> str:
    """Append incoming `##` sections the current file does not already have."""
    have, new = sections(current), sections(incoming)
    missing = [t for t in new if t not in have]
    if not missing:
        return current
    add = "".join(new[t].rstrip() + "\n\n" for t in missing)
    return current.rstrip() + "\n\n" + add.rstrip() + "\n"


def attempts_from_worktree(wt: Path, func: str) -> int | None:
    """The attempt count the worktree's own commit recorded for `func`.

    vacuum.sh writes `matched <fn> <attempts>`, and that number is training data
    for fit_difficulty_model.py. Landing with a hardcoded 1 quietly rewrites a
    3-attempt match as a 1-attempt one, which biases the scorer toward thinking
    these functions are easier than they were.
    """
    r = subprocess.run(["git", "log", "--format=%s", "--all"],
                       cwd=wt, capture_output=True, text=True)
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[0] == "matched" and parts[1] == func:
            try:
                return int(parts[2])
            except ValueError:
                return None
    return None


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
    ap.add_argument("--attempts", type=int,
                    help="override the attempt count; by default it is read "
                         "from the worktree's own `matched <fn> <n>` commit")
    ap.add_argument("--base", default="",
                    help="commit the worktree was cut from; needed to merge "
                         "configs/USA/overlays.toml by entry instead of wholesale")
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
    base_rev = args.base
    extras = [e.strip() for e in args.extra.split(",") if e.strip()]
    for e in extras:
        src, dst = wt / e, ROOT / e
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.is_file() and e.endswith("difficult_functions"):
            # Line-based and append-only, like the learnings file: two overlay
            # worktrees cut from the same commit both add entries, and copying
            # the second one's file over trunk drops the first one's give-ups.
            # Keyed by function name so a re-run's better score replaces an
            # older line rather than duplicating it.
            have = {l.split()[0]: l for l in dst.read_text().splitlines() if l.strip()}
            for l in src.read_text().splitlines():
                if l.strip():
                    have[l.split()[0]] = l
            dst.write_text("\n".join(have[k] for k in sorted(have)) + "\n")
        elif dst.is_file() and e.endswith("overlays.toml"):
            # One line per overlay, so merge by key and take only the entries the
            # worktree actually changed relative to the commit it was cut from.
            # Copying it wholesale reverts every entry another session landed in
            # the meantime: mist_shooting_gallery's landing silently dropped
            # mist_r18's promotion span and shelter_b1_sterilization_room's,
            # because its worktree predated them. The build caught it only
            # because a missing shared unit breaks the link - a lost `rodata`
            # cut would have gone through green.
            import re as _re
            base_txt = git("show", f"{base_rev}:{e}") if base_rev else ""
            def _entries(text):
                return {m.group(1): m.group(0)
                        for m in _re.finditer(r"^(\w+) = \{.*$", text, _re.M)}
            base_e, wt_e, cur = _entries(base_txt), _entries(src.read_text()), dst.read_text()
            changed = [k for k, v in wt_e.items() if base_e.get(k) != v]
            for k in changed:
                m = _re.search(rf"^{k} = \{{.*$", cur, _re.M)
                cur = cur[:m.start()] + wt_e[k] + cur[m.end():] if m else cur + wt_e[k] + "\n"
            dst.write_text(cur)
            print(f"  merged {len(changed)} manifest entr(y/ies): {', '.join(changed) or 'none'}")
        elif dst.is_file() and e.endswith(".md"):
            # Append-only docs must be merged, not copied. Every overlay
            # worktree is cut from the same trunk commit, so two sessions both
            # append to DECOMPILATION_LEARNINGS.md and the second landing
            # silently erases the first one's entries - which is exactly what
            # happened landing dryfield_motel_balcony after acropolis_cafeteria.
            dst.write_text(merge_sections(dst.read_text(), src.read_text()))
        else:
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
        n = args.attempts if args.attempts else (attempts_from_worktree(wt, fn) or 1)
        git("commit", "-q", "-m", f"matched {fn} {n}")
        print(f"  matched {fn}")

    print(f"landed {len(funcs)} function(s) from {args.overlay}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
