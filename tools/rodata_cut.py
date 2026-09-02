#!/usr/bin/env python3
"""Compute and apply the rodata ownership cuts an overlay needs, without losing work.

A compiler-generated jump table must live in the .rodata of the object whose
function uses it, so ownership is not a judgement call: walk the overlay's rodata
in address order, and every jump table forces the block to belong to the unit
holding its referencing function. Data blobs have no such constraint and stay
with whatever precedes them. That rule, applied to mine_cavern, reproduces the
cuts that freed it by hand.

Doing it safely is the harder half. CLAUDE.md says to delete the affected src/
files and re-split so splat places the INCLUDE_RODATA lines itself, which is
right when those files are all INCLUDE_ASM and destructive when they are not -
and the damage is invisible, because a function reverted to INCLUDE_ASM still
matches. Deleting mine_cavern's directory by hand silently reverted two units
that the change did not even touch, and the build passed anyway. So --apply
re-applies every decompiled body onto splat's regenerated structure and then
refuses to finish unless each file's INCLUDE_ASM count is no higher than it was.

Usage:
    python3 tools/rodata_cut.py shelter_r47              # plan only
    python3 tools/rodata_cut.py shelter_r47 --apply
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DLABEL = re.compile(r"^dlabel (\w+)", re.M)
ADDR = re.compile(r"_([0-9A-F]{8})$")
FUNC_START = re.compile(r"^[A-Za-z_][\w \*]*\b(func_\w+)\s*\(\s*[^;]*$")


def unit_dirs(overlay: str) -> list[Path]:
    out = []
    for base in ("nonmatchings", "matchings"):
        for d in ROOT.glob(f"asm/*/**/{base}/{overlay}"):
            out.extend(p for p in d.iterdir() if p.is_dir())
    return out


def rodata_symbols(overlay: str) -> list[tuple[int, str, Path]]:
    """Every rodata symbol in the overlay, in address order, with the file holding it."""
    found: dict[str, Path] = {}
    for d in ROOT.glob(f"asm/*/**/data/{overlay}"):
        for f in d.glob("*.rodata.s"):
            for sym in DLABEL.findall(f.read_text(errors="replace")):
                found.setdefault(sym, f)
    for ud in unit_dirs(overlay):           # tables splat inlined into a function
        for f in ud.glob("*.s"):
            for sym in DLABEL.findall(f.read_text(errors="replace")):
                found.setdefault(sym, f)
    out = []
    for sym, f in found.items():
        m = ADDR.search(sym)
        if m:
            out.append((int(m.group(1), 16), sym, f))
    return sorted(out)


def referencing_unit(sym: str, overlay: str) -> str | None:
    """The unit of the function that references `sym`, if exactly one does."""
    units = set()
    for ud in unit_dirs(overlay):
        for f in ud.glob("*.s"):
            if f.stem.startswith(("D_", "jtbl_", "_L")):
                continue
            txt = f.read_text(errors="replace")
            if sym in txt:
                units.add(ud.name)
    return units.pop() if len(units) == 1 else None


def plan(overlay: str) -> tuple[list[tuple[int, str]], list[str]]:
    syms = rodata_symbols(overlay)
    if not syms:
        return [], ["no rodata in this overlay"]
    base = syms[0][0]
    first_unit = min(unit_dirs(overlay), key=lambda p: p.name).name
    cuts, notes, current = [], [], first_unit
    for addr, sym, _ in syms:
        if not sym.startswith("jtbl_"):
            continue                      # data blobs stay where they fall
        owner = referencing_unit(sym, overlay)
        if owner is None:
            notes.append(f"{sym}: no single referencing function, left alone")
            continue
        if owner != current:
            cuts.append((addr - base, owner))
            current = owner
    seen = [u for _, u in cuts]
    if len(seen) != len(set(seen)):
        notes.append("a unit would own two separate blocks; needs manual work")
    return cuts, notes


def toml_value(cuts: list[tuple[int, str]]) -> str:
    inner = ", ".join(f'{{ start = "0x{off:X}", unit = "{u}" }}' for off, u in cuts)
    return f"rodata = [{inner}]"


def read_bodies(text: str):
    lines = text.splitlines(keepends=True)
    headers, bodies, i = [], {}, 0
    while i < len(lines):
        l = lines[i]
        m = FUNC_START.match(l)
        if m and not l.lstrip().startswith(("INCLUDE_", "extern")):
            start = i
            while i < len(lines) and lines[i].rstrip() != "}":
                i += 1
            bodies[m.group(1)] = "".join(lines[start:i + 1])
            i += 1
            continue
        if (l.startswith("#include") and "common.h" not in l) or \
           l.startswith(("extern ", "typedef ", "static ")):
            headers.append(l)
        i += 1
    return headers, bodies


def apply(overlay: str, cuts: list[tuple[int, str]]) -> int:
    src_dirs = [p for p in ROOT.glob(f"src/**/{overlay}") if p.is_dir()]
    if len(src_dirs) != 1:
        print(f"expected one src dir for {overlay}, found {len(src_dirs)}", file=sys.stderr)
        return 1
    src = src_dirs[0]

    manifest = ROOT / "configs/USA/overlays.toml"
    text = manifest.read_text()
    m = re.search(rf"^{re.escape(overlay)} = \{{.*$", text, re.M)
    if not m:
        print(f"no manifest entry for {overlay}", file=sys.stderr)
        return 1
    line = m.group(0)
    new_line = (re.sub(r'rodata = \[[^\]]*\]', toml_value(cuts), line)
                if "rodata = [" in line
                else line.replace("{ ", "{ " + toml_value(cuts) + ", ", 1))
    manifest.write_text(text.replace(line, new_line, 1))

    stash = Path(tempfile.mkdtemp(prefix=f"rodata-cut-{overlay}-"))
    originals = {p.name: p.read_text() for p in src.glob("*.c")}
    for name, body in originals.items():
        (stash / name).write_text(body)
    for p in src.glob("*.c"):
        p.unlink()

    r = subprocess.run([str(ROOT / "venv/bin/python3"), "ninja_config.py"],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-3000:] + r.stderr[-3000:], file=sys.stderr)
        print("split failed; originals kept in " + str(stash), file=sys.stderr)
        return 1

    for name, original in originals.items():
        fp = src / name
        if not fp.is_file():
            fp.write_text(original)          # splat no longer emits it; keep ours
            continue
        headers, bodies = read_bodies(original)
        fresh_text = fp.read_text()
        _, already = read_bodies(fresh_text)
        out, used = [], set()
        for l in fresh_text.splitlines(keepends=True):
            mm = re.search(r'INCLUDE_ASM\("[^"]+",\s*(\w+)\)', l)
            if mm and mm.group(1) in bodies and mm.group(1) not in used:
                used.add(mm.group(1)); out.append(bodies[mm.group(1)])
            else:
                out.append(l)
        missing = set(bodies) - used - set(already)
        if missing:
            print(f"{name}: could not re-apply {sorted(missing)}; "
                  f"originals in {stash}", file=sys.stderr)
            return 1
        if headers:
            for i, l in enumerate(out):
                if l.startswith('#include "common.h"'):
                    out[i+1:i+1] = headers
                    break
        fp.write_text("".join(out))

    # The guard that matters: reverting a function to INCLUDE_ASM still matches,
    # so a green build proves nothing here. Compare against what we started with.
    bad = []
    for name, original in originals.items():
        before = original.count("INCLUDE_ASM")
        after = (src / name).read_text().count("INCLUDE_ASM")
        if after > before:
            bad.append(f"{name}: INCLUDE_ASM {before} -> {after}")
    if bad:
        print("decompiled work was lost:", file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        print(f"originals kept in {stash}", file=sys.stderr)
        return 1

    # The first split ran with every .c deleted, so splat wrote a nonmatchings
    # .s for all of them and never removed the ones that are matched again now.
    # Stale entries there are not harmless: the vacuum and score_functions pick
    # work from that directory and would re-match functions already done. Wipe
    # the overlay's asm and split once more, against the restored sources.
    for d in list(ROOT.glob(f"asm/*/**/nonmatchings/{overlay}")) + \
             list(ROOT.glob(f"asm/*/**/matchings/{overlay}")) + \
             list(ROOT.glob(f"asm/*/**/data/{overlay}")):
        shutil.rmtree(d, ignore_errors=True)
    r = subprocess.run([str(ROOT / "venv/bin/python3"), "ninja_config.py"],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-2000:] + r.stderr[-2000:], file=sys.stderr)
        return 1

    stale = len(list((ROOT / f"asm/USA/rooms/nonmatchings/{overlay}").rglob("func_*.s"))) \
        if (ROOT / f"asm/USA/rooms/nonmatchings/{overlay}").is_dir() else 0
    inc = sum((src / n).read_text().count("INCLUDE_ASM") for n in originals)
    if stale and inc and stale != inc:
        print(f"warning: {stale} nonmatching .s against {inc} INCLUDE_ASM", file=sys.stderr)

    shutil.rmtree(stash, ignore_errors=True)
    print(f"applied {len(cuts)} cut(s) to {overlay}; "
          f"now run: ./tools/build-and-verify.sh --only {overlay}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("overlay")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    cuts, notes = plan(args.overlay)
    for n in notes:
        print(f"note: {n}")
    if not cuts:
        print("no cuts needed")
        return 0
    print(f"proposed for {args.overlay}:\n  {toml_value(cuts)}")
    if any("manual work" in n for n in notes):
        print("refusing to apply", file=sys.stderr)
        return 1
    return apply(args.overlay, cuts) if args.apply else 0


if __name__ == "__main__":
    sys.exit(main())
