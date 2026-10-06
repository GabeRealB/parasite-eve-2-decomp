#!/usr/bin/env python3
"""Fail when a change adds a matching hack: a pinned register, an asm statement, a steering macro.

    check_hack_sites.py --against REV      compare the working tree with REV in the files that differ
    check_hack_sites.py PATH ...           count the sites in the given files

A matched function sometimes needs one of these to keep matching:

    pin      `register T x asm("reg")` (or `__asm__`)
    barrier  an asm statement with an empty template, or a steering macro of
             include/decomp/common.h (TOUCH_REG, USE_REG, SOFT_BARRIER, ...)
    emit     an asm statement that emits instructions

Each one says the source differs from what was written. Removing them is slow,
analytical work; adding one takes a line, and a cleanup that changed the code
it compiles to can be "fixed" that way and still pass every build check. This
compares the number of sites in the files a change touches, before and after,
and fails if it grew. A change that moves a site between files, or removes one
and needs another elsewhere, is unaffected: only the total counts.

Psy-Q's `gte_*` macros and `include/decomp/gte.h` emit GTE instructions and are
not counted; neither is `gte_RotTransLV`, a hand-written GTE routine.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PIN = re.compile(r'\bregister\b[^;={]*?\b(?:asm|__asm__)\s*\(\s*"([^"]*)"\s*\)')
STMT = re.compile(r'\b(?:__asm__|asm)\s*(?:volatile|__volatile__)?\s*\(\s*("(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*)')
MACRO = re.compile(r'\b(SCHED_BARRIER|SOFT_BARRIER|SOFT_COMPILER_BARRIER|COMPILER_BARRIER'
                   r'|SOFT_TOUCH_REG\d?(?:_USE\d?)?|TOUCH_REG\d?(?:_MEM|_USE\d?)?|TOUCH_REG2_MEM'
                   r'|SOFT_DEF_REG|DEF_REG|SOFT_USE_REG\d?|USE_REG\d?|CLOBBER_REG'
                   r'|TOUCH_MEM|SOFT_MOVE_ZERO|MOVE_ZERO|COPY_REG(?:_EC)?)\s*\(')
COMMENT = re.compile(r'//[^\n]*|/\*.*?\*/', re.S)
EXEMPT = ("include/psyq/", "include/decomp/gte.h", "include/decomp/common.h", "include/include_asm.h")


def code(text: str) -> str:
    """The text with comments and preprocessor directives blanked, line numbers kept."""
    text = COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)
    out, continued = [], False
    for line in text.split("\n"):
        directive = continued or line.lstrip().startswith("#")
        continued = directive and line.rstrip().endswith("\\")
        out.append("" if directive else line)
    return "\n".join(out)


def sites(text: str) -> list[tuple[int, str, str]]:
    found = []
    for number, line in enumerate(code(text).split("\n"), 1):
        rest = line
        for m in PIN.finditer(line):
            found.append((number, "pin", m.group(0).strip()))
            rest = rest.replace(m.group(0), "")
        for m in STMT.finditer(rest):
            if "gte_RotTransLV" in rest:
                continue
            template = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
            found.append((number, "barrier" if not template.strip() else "emit", rest.strip()[:80]))
        for m in MACRO.finditer(rest):
            found.append((number, "barrier", m.group(0).rstrip("(").strip()))
    return found


def counted(path: str) -> bool:
    return path.endswith((".c", ".h")) and path.startswith(("src/", "include/")) and not path.startswith(EXEMPT)


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True).stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--against", metavar="REV", help="compare the working tree with this revision")
    ap.add_argument("paths", nargs="*")
    args = ap.parse_args()
    if not args.against:
        total = 0
        for path in args.paths:
            for number, kind, what in sites((ROOT / path).read_text(errors="replace")):
                print(f"{path}:{number}: {kind}: {what}")
                total += 1
        print(f"{total} site(s)")
        return 0
    changed = set(git("diff", "--name-only", args.against).split())
    changed |= set(git("ls-files", "--others", "--exclude-standard", "src", "include").split())
    before, after, added = 0, 0, []
    for path in sorted(p for p in changed if counted(p)):
        old = sites(git("show", f"{args.against}:{path}"))
        new = sites((ROOT / path).read_text(errors="replace")) if (ROOT / path).exists() else []
        before += len(old)
        after += len(new)
        gone = [what for _, _, what in old]
        for number, kind, what in new:
            if what in gone:
                gone.remove(what)
            else:
                added.append(f"  {path}:{number}: {kind}: {what}")
    if after > before:
        print(f"matching hacks added: {before} site(s) before this change, {after} after, in the files it touches.")
        print("\n".join(added))
        print("A pinned register, an asm statement or a steering macro must not be added to keep a")
        print("match. Put the code back in the form that matched without it and record the cleanup")
        print("that needed it as a follow-up (NAMING.md, \"Naming-pass acceptance and follow-ups\").")
        return 1
    print(f"hack sites in the files touched: {before} before, {after} after")
    return 0


if __name__ == "__main__":
    sys.exit(main())
