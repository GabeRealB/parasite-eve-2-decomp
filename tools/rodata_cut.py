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


def rodata_head(overlay: str) -> int:
    """The file offset the leading rodata starts at (manifest `rodata_head`).

    Manifest cut offsets are file offsets into the package, not offsets from the
    first rodata symbol, and gen_overlay_configs requires head < cut < text
    start. Nine overlays carry a non-zero head; mine_cavern's is 0, which is why
    treating the first symbol as offset 0 worked there and produced a cut at
    0x0 - rejected as "outside the leading rodata (0x1F4..0x268)" - elsewhere.
    """
    text = (ROOT / "configs/USA/overlays.toml").read_text()
    m = re.search(rf"^{re.escape(overlay)} = \{{.*$", text, re.M)
    if not m:
        return 0
    h = re.search(r'rodata_head = "(0x[0-9A-Fa-f]+)"', m.group(0))
    return int(h.group(1), 16) if h else 0


def plan(overlay: str) -> tuple[list[tuple[int, str]], list[str]]:
    syms = rodata_symbols(overlay)
    if not syms:
        return [], ["no rodata in this overlay"]
    base = syms[0][0]
    head = rodata_head(overlay)
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
            off = head + (addr - base)
            if off <= head:
                # The leading rodata always belongs to the overlay's first unit
                # (gen_overlay_configs emits it as `<name>/<name>`), so "the very
                # first table belongs to a later unit" is not expressible as a cut.
                notes.append(f"{sym} would need the block start itself, needs manual work")
                continue
            cuts.append((off, owner))
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


def src_dir(overlay: str) -> Path | None:
    dirs = [p for p in ROOT.glob(f"src/**/{overlay}") if p.is_dir()]
    return dirs[0] if len(dirs) == 1 else None


def write_manifest(overlay: str, cuts: list[tuple[int, str]]) -> bool:
    manifest = ROOT / "configs/USA/overlays.toml"
    text = manifest.read_text()
    m = re.search(rf"^{re.escape(overlay)} = \{{.*$", text, re.M)
    if not m:
        return False
    line = m.group(0)
    new_line = (re.sub(r"rodata = \[[^\]]*\]", toml_value(cuts), line)
                if "rodata = [" in line
                else line.replace("{ ", "{ " + toml_value(cuts) + ", ", 1))
    manifest.write_text(text.replace(line, new_line, 1))
    return True


def split() -> bool:
    r = subprocess.run([str(ROOT / "venv/bin/python3"), "ninja_config.py"],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print((r.stdout[-2000:] + r.stderr[-2000:]), file=sys.stderr)
    return r.returncode == 0


RODATA_LINE = re.compile(r'INCLUDE_RODATA\("[^"]+",\s*(\w+)\)')
ASM_LINE = re.compile(r'INCLUDE_ASM\("[^"]+",\s*(\w+)\)')


def rodata_anchors(fresh: str) -> list[tuple[str | None, str]]:
    """splat's INCLUDE_RODATA lines, each tagged with the function it follows."""
    out, last = [], None
    for l in fresh.splitlines(keepends=True):
        m = ASM_LINE.search(l)
        if m:
            last = m.group(1)
            continue
        fm = FUNC_START.match(l)
        if fm and not l.lstrip().startswith(("INCLUDE_", "extern")):
            last = fm.group(1)
            continue
        if RODATA_LINE.search(l):
            out.append((last, l))
    return out


def reposition(original: str, anchors: list[tuple[str | None, str]]) -> str:
    """Rewrite `original`'s INCLUDE_RODATA lines to splat's new set and order.

    Only rodata ownership changed, so everything else in the file - decompiled
    bodies, multi-line statics, includes, comments - is left exactly as it was.
    An earlier version rebuilt the file from splat's regenerated one and copied
    declarations across line by line, which truncated multi-line statics and
    produced `parse error before 'extern'`.
    """
    lines = [l for l in original.splitlines(keepends=True) if not RODATA_LINE.search(l)]

    # where each function ends, so a rodata line can follow it
    ends: dict[str, int] = {}
    i = 0
    while i < len(lines):
        m = ASM_LINE.search(lines[i])
        if m:
            ends[m.group(1)] = i
            i += 1
            continue
        fm = FUNC_START.match(lines[i])
        if fm and not lines[i].lstrip().startswith(("INCLUDE_", "extern")):
            j = i
            while j < len(lines) and lines[j].rstrip() != "}":
                j += 1
            ends[fm.group(1)] = min(j, len(lines) - 1)
            i = j + 1
            continue
        i += 1

    top = max((k for k, l in enumerate(lines) if l.startswith("#include")), default=0)
    for anchor, line in reversed(anchors):
        at = ends.get(anchor, top) if anchor else top
        lines[at + 1:at + 1] = ["\n", line]
    return "".join(lines)


def restore(overlay: str, originals: dict[str, str], stash: Path) -> list[str]:
    """Put the decompiled sources back, with rodata lines moved to the new owner."""
    src = src_dir(overlay)
    problems = []
    for name, original in originals.items():
        fp = src / name
        anchors = rodata_anchors(fp.read_text()) if fp.is_file() else []
        fp.write_text(reposition(original, anchors))

    # A function reverted to INCLUDE_ASM still matches, so the build cannot tell
    # us this went wrong. Compare against what we started with.
    for name, original in originals.items():
        before, after = original.count("INCLUDE_ASM"), (src / name).read_text().count("INCLUDE_ASM")
        if after != before:
            problems.append(f"{overlay}/{name}: INCLUDE_ASM {before} -> {after}")
    if problems:
        problems.append(f"originals kept in {stash}")
    return problems


def wipe_asm(overlay: str) -> None:
    for kind in ("nonmatchings", "matchings", "data"):
        for d in ROOT.glob(f"asm/*/**/{kind}/{overlay}"):
            shutil.rmtree(d, ignore_errors=True)


def apply_many(jobs: list[tuple[str, list[tuple[int, str]]]]) -> int:
    """Apply cuts for several overlays, sharing the two full splits."""
    stashes: dict[str, tuple[Path, dict[str, str]]] = {}
    for overlay, cuts in jobs:
        src = src_dir(overlay)
        if src is None:
            print(f"{overlay}: no single src dir, skipping", file=sys.stderr)
            continue
        if not write_manifest(overlay, cuts):
            print(f"{overlay}: no manifest entry, skipping", file=sys.stderr)
            continue
        stash = Path(tempfile.mkdtemp(prefix=f"rodata-cut-{overlay}-"))
        originals = {p.name: p.read_text() for p in src.glob("*.c")}
        for name, body in originals.items():
            (stash / name).write_text(body)
        for p in src.glob("*.c"):
            p.unlink()
        stashes[overlay] = (stash, originals)

    if not stashes:
        return 1
    print(f"splitting with {len(stashes)} overlay(s) emptied ...")
    if not split():
        return 1

    problems = []
    for overlay, (stash, originals) in stashes.items():
        problems += restore(overlay, originals, stash)
    if problems:
        for p in problems:
            print("  " + p, file=sys.stderr)
        return 1

    # The first split saw every .c deleted, so splat wrote a nonmatchings .s for
    # all of them and left the ones matched again behind. The vacuum picks work
    # from that directory, so stale entries there mean re-matching done work.
    for overlay in stashes:
        wipe_asm(overlay)
    print("re-splitting against the restored sources ...")
    if not split():
        return 1

    for overlay, (stash, _) in stashes.items():
        shutil.rmtree(stash, ignore_errors=True)
    print(f"applied cuts to {len(stashes)} overlay(s)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("overlay", nargs="+")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    jobs = []
    for overlay in args.overlay:
        cuts, notes = plan(overlay)
        for n in notes:
            print(f"note: {overlay}: {n}")
        if any("manual work" in n for n in notes):
            print(f"{overlay}: skipping, needs manual work", file=sys.stderr)
            continue
        if not cuts:
            print(f"{overlay}: no cuts needed")
            continue
        print(f"{overlay}:\n  {toml_value(cuts)}")
        jobs.append((overlay, cuts))

    if not args.apply or not jobs:
        return 0
    return apply_many(jobs)


if __name__ == "__main__":
    sys.exit(main())
