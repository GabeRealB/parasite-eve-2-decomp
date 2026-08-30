#!/usr/bin/env python3
"""Generate splat configs for the overlay families listed in overlays.toml.

main / gameplay / title stay hand-written: each has quirks a template cannot
express. Every other overlay is a flat `.pe2pkg` with the same shape, and there
are 446 of them, so they are generated instead - from one template plus a
manifest that holds only the decisions a person actually made (the overlay's
name, and what it is).

Everything checkable is derived from the package rather than written down:

``sha1`` / ``size``
    Read off the file, so a config cannot drift from the data it describes.
An overlay **is** an extracted package. The manifest key is the package's name
and the config is built from ``assets/USA/pe2pkg/<key>.pe2pkg``; file ids do not
appear. Several stage-0 ids load one package - the extractor dedups by SHA-1, so
`10500` and `10600` are a single file - but that is the extractor's business,
and naming the package there (``tools/peassets/asset_data.py``) is what makes it
a single thing here. One package, one entry, one split, one set of symbols.

``.text`` span
    Found the same way ``doc/OVERLAYS.md`` §4.3 does - first stack-frame
    prologue through the last ``jr $ra`` plus its delay slot. The detector
    reproduces the known title and gameplay spans exactly. A wrong span shows
    up as a failed build, not as silent corruption, and an overlay can pin it
    with `text = [start, end]` in the manifest if the detector ever misses.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
import tomllib
from pathlib import Path

MANIFEST = Path("configs/USA/overlays.toml")
PACKAGE_DIR = Path("assets/USA/pe2pkg")
TEMPLATE = Path("configs/USA/overlay.template.yaml")
OUT_DIR = Path("configs/USA/generated")

ADDIU_SP = 0x27BD0000  # addiu $sp, $sp, -N  (negative immediate)
JR_RA = 0x03E00008


def text_span(data: bytes) -> tuple[int, int] | None:
    """Byte range of the code region, or None for a data-only package."""
    n = len(data) // 4
    words = struct.unpack_from(f"<{n}I", data, 0)
    starts = [i for i, w in enumerate(words) if (w & 0xFFFF0000) == ADDIU_SP and w & 0x8000]
    if not starts:
        return None
    ends = [i for i, w in enumerate(words) if w == JR_RA]
    if not ends:
        return None
    return min(starts) * 4, (max(ends) + 2) * 4


def trailing_segment(name: str, data: bytes) -> list[str]:
    """Cover a package whose size is not word-aligned.

    Package sizes are byte counts, not word counts, so a file can end part-way
    through a word. Without this the linker stops at the last whole word and the
    build comes out a couple of bytes short with every other byte identical -
    which is what `title.yaml` spells out by hand for its own 0x14AA size.

    Which segment depends on what those bytes are. `pad` emits zeros, so it is
    only correct when the tail *is* zeros (tonfa_baton). Two packages end on
    real content - map_dryfield on b' &' and nmc_names on the last bytes of its
    ASCII ramp - and those need `databin`, which writes the bytes out and
    .incbin's them back rather than inventing them.
    """
    tail = data[len(data) & ~3 :]
    if not tail:
        return []
    start = f"0x{len(data) & ~3:X}"
    if not any(tail):
        return [f"      - [{start}, pad]"]
    return [f"      - [{start}, databin, {name}_tail]"]


def subsegments(
    name: str,
    data: bytes,
    span: tuple[int, int] | None,
    shared: list[dict],
    rodata: list[dict],
    rodata_head: str | int | None,
    units: list[str],
) -> str:
    """The package layout as splat subsegment lines.

    `src_path` is the family root, so a subsegment is named `<overlay>/<unit>`
    and its object lands in the overlay's own directory. A **shared** body is
    named `lib/<unit>` instead: several overlays name the same path, splat puts
    the same `build/.../src/<family>/lib/<unit>` object in each of their linker
    scripts, and one object relocates into all of them. That is the whole
    mechanism - a body that appears in 55 rooms is built and matched once.

    `units` adds plain `.text` cuts on top of that. A package is one object per
    original translation unit, and the only visible trace of a boundary is which
    unit owns the rodata ahead of it, so a cut is needed exactly when a `rodata`
    cut is: a jump table GCC emits mid-object gets an `.align 3` pad, while one
    that starts an object's `.rodata` is re-aligned by the linker script's
    `SUBALIGN(4)`. Runs keep the same `<name>`, `<name>_2`, ... numbering the
    shared cuts produce, so a `rodata` cut names the unit it pairs with.

    A data-only package still gets one `data` subsegment covering the file.
    """
    lines: list[str] = []
    if span is None:
        lines.append(f"      - [0x0, data, {name}]")
        lines.extend(trailing_segment(name, data))
        return "\n".join(lines)

    start, end = span
    if start:
        # Leading rodata: the package header that sits ahead of the first
        # function. One subsegment per owning unit, named to match its `c`
        # sibling so splat pairs them: a unit's `.rodata` appears once in the
        # linker script, at the offset its subsegment names, so a jump table a
        # decompiled function emits from C only lands at the right address if
        # the rodata is cut where ownership changes.
        head = int(str(rodata_head), 16) if rodata_head else 0
        if head:
            if not 0 < head < start:
                raise SystemExit(
                    f"{name}: rodata_head 0x{head:X} is outside the leading "
                    f"rodata (0x0..0x{start:X})"
                )
            lines.append(f"      - [0x0, rodata, {name}_hdr]")
        lines.append(f"      - [0x{head:X}, .rodata, {name}/{name}]")
        for cut in sorted(rodata, key=lambda r: int(str(r["start"]), 16)):
            cut_start = int(str(cut["start"]), 16)
            if not head < cut_start < start:
                raise SystemExit(
                    f"{name}: rodata cut {cut['unit']} at 0x{cut_start:X} is "
                    f"outside the leading rodata (0x{head:X}..0x{start:X})"
                )
            lines.append(f"      - [0x{cut_start:X}, .rodata, {name}/{cut['unit']}]")
    elif rodata or rodata_head:
        raise SystemExit(f"{name}: rodata cuts but the package has no leading rodata")

    # Walk the code region, cutting out each shared body as its own subsegment
    # and giving the runs between them numbered overlay-local units.
    cuts = sorted(shared, key=lambda s: int(str(s["start"]), 16))
    breaks = sorted(int(str(u), 16) for u in units)
    for brk in breaks:
        if not start < brk < end:
            raise SystemExit(f"{name}: unit cut 0x{brk:X} is outside .text (0x{start:X}..0x{end:X})")
    pos, unit = start, 0

    def emit_run(run_start: int, run_end: int) -> None:
        nonlocal unit
        for point in [run_start] + [b for b in breaks if run_start < b < run_end]:
            unit += 1
            suffix = "" if unit == 1 else f"_{unit}"
            lines.append(f"      - [0x{point:X}, c, {name}/{name}{suffix}]")

    for cut in cuts:
        cut_start, cut_end = int(str(cut["start"]), 16), int(str(cut["end"]), 16)
        if not (start <= cut_start < cut_end <= end):
            raise SystemExit(f"{name}: shared span {cut['unit']} is outside .text")
        if cut_start > pos:
            emit_run(pos, cut_start)
        lines.append(f"      - [0x{cut_start:X}, c, lib/{cut['unit']}]")
        pos = cut_end
    if pos < end:
        emit_run(pos, end)

    if end < len(data):
        # Trailing data: models, animation banks, scripts - not code.
        lines.append(f"      - [0x{end:X}, data, {name}_data]")
    lines.extend(trailing_segment(name, data))
    return "\n".join(lines)


def overlay_label(name: str, entry: dict) -> str:
    """Human label for the config's title line.

    Families describe their overlays with different keys - `weapon` names the
    gun, `note` says what an unnamed one is - so the label falls back through
    them to the overlay name rather than requiring any particular field. A
    family that adds a new descriptive key needs it listed here, nothing more.
    """
    for key in ("weapon", "note"):
        value = entry.get(key)
        if value:
            return str(value)
    return name


def generate(family: str, spec: dict, template: str, out_dir: Path) -> list[Path]:
    written: list[Path] = []
    seen: dict[bytes, str] = {}
    for name, entry in sorted(spec["overlays"].items()):
        target = PACKAGE_DIR / f"{name}.pe2pkg"
        if not target.is_file():
            raise SystemExit(
                f"{family}/{name}: {target} not found.\n"
                f"  Package names come from tools/peassets/asset_data.py; extract with\n"
                f"  `python3 ninja_config.py -iso_min` to materialise the required set."
            )
        data = target.read_bytes()

        # Two entries on one package would split the same file twice and give
        # its functions two symbol prefixes. Distinct names sharing content mean
        # the extractor's naming disagrees with this manifest.
        digest = hashlib.sha1(data).digest()
        clash = seen.get(digest)
        if clash is not None:
            raise SystemExit(
                f"{family}/{name} is the same package as {family}/{clash}.\n"
                f"  Splitting it again would disassemble the same file twice.\n"
                f"  Keep one entry, and give the package one name in asset_data.py."
            )
        seen[digest] = name

        span = tuple(entry["text"]) if "text" in entry else text_span(data)
        title = f"{spec['description']} - {overlay_label(name, entry)}"

        text = template
        for key, val in {
            "TITLE": title,
            "NAME": name,
            "FAMILY": family,
            "TARGET": target.as_posix(),
            "SHA1": hashlib.sha1(data).hexdigest(),
            "SIZE": str(len(data)),
            "SIZE_HEX": f"0x{len(data):X}",
            "VRAM": f"0x{spec['load_addr']:08X}",
            "GLOBAL_VRAM_START": f"0x{spec['global_vram_start']:08X}",
            "GLOBAL_VRAM_END": f"0x{spec['global_vram_end']:08X}",
            "IMPORTS": spec["imports"],
            "SUBSEGMENTS": subsegments(
                name,
                data,
                span,
                entry.get("shared", []),
                entry.get("rodata", []),
                entry.get("rodata_head"),
                entry.get("units", []),
            ),
        }.items():
            text = text.replace(f"@@{key}@@", val)
        if "@@" in text:
            raise SystemExit(f"{family}/{name}: unsubstituted placeholder in template")

        dest = out_dir / f"{name}.yaml"
        dest.write_text(text, encoding="utf-8")
        written.append(dest)
    return written


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--family", help="only generate this family")
    ap.add_argument("--list", action="store_true", help="print names and exit")
    args = ap.parse_args()

    manifest = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))
    template = TEMPLATE.read_text(encoding="utf-8")
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    total = 0
    produced: set[Path] = set()
    for family, spec in manifest.items():
        if args.family and family != args.family:
            continue
        if args.list:
            for name, e in sorted(spec["overlays"].items()):
                print(f"{family}\t{name}\t{overlay_label(name, e)}")
            continue
        written = generate(family, spec, template, OUT_DIR)
        produced.update(written)
        total += len(written)
        print(f"{family}: wrote {len(written)} config(s) to {OUT_DIR}")

    # Drop configs the manifest no longer produces. Without this, renaming an
    # overlay or marking one as a duplicate leaves its old config behind and the
    # build keeps splitting it.
    if not args.list and not args.family:
        stale = sorted(p for p in OUT_DIR.glob("*.yaml") if p not in produced)
        for path in stale:
            path.unlink()
        if stale:
            print(f"removed {len(stale)} stale config(s): "
                  + ", ".join(p.stem for p in stale))
    return 0 if args.list or total else 1


if __name__ == "__main__":
    sys.exit(main())
