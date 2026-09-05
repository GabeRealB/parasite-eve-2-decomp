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


def _u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


_MODEL_SPAN_CACHE: dict[str, list[tuple[int, int]]] = {}


def model_spans(name: str, data: bytes, load_addr: int | None) -> list[tuple[int, int]]:
    """Byte ranges of this package's model streams, as (start, end).

    Derived from the package, never declared: `find_sources_direct` reads each
    `TmdSource` and `walk_stream` gives the exact end, so a span cannot drift
    from the data it describes - the same rule the sha1 and the `.text` span
    follow.

    Per package, deliberately. 129 meshes are shared between overlays and the
    asset catalog keeps one entry for each, but a *split* is about this file's
    bytes: kyle_body is carved out of all twelve packages that carry it, at
    each one's own offset.
    """
    if load_addr is None:
        return []
    key = f"{name}:{load_addr:08X}"
    if key in _MODEL_SPAN_CACHE:
        return _MODEL_SPAN_CACHE[key]
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent / "peassets"))
        import pkg_model
    except Exception:
        return []
    spans: list[tuple[int, int]] = []
    try:
        found = pkg_model.find_sources_direct(data, load_addr)
        for stream_va, src in found.items():
            off = stream_va - load_addr
            walked = pkg_model.walk_stream(data, off)
            if not walked:
                continue
            # The whole model, not just its display list. A `TmdSource` bounds
            # every array it owns, and all 597 on disc are laid out
            # `skeleton < partVerts < verts < norms < stream` with the record
            # itself immediately after - one contiguous run. Carving only the
            # stream would leave the vertex and normal arrays, which are just
            # as opaque and 1.4x the bytes, sitting in the disassembly.
            head = min(
                int(src["verts_offset"], 16),
                int(src["norms_offset"], 16),
                _u32(data, int(src["source_offset"], 16) + 0x10) - load_addr,
                _u32(data, int(src["source_offset"], 16) + 0x1C) - load_addr,
                off,
            )
            # The record too: it is the model's own descriptor, not a
            # neighbour, and every stream on disc ends exactly at its source's
            # offset. Leaving it behind would strand 0x24 bytes of pointers
            # between two opaque blobs for no gain.
            src_off = int(src["source_offset"], 16)
            spans.append((head, max(walked[1], src_off + 0x24)))
    except Exception:
        return []
    spans.sort()
    merged: list[tuple[int, int]] = []
    for st, en in spans:
        if merged and st < merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], en))
        else:
            merged.append((st, en))
    _MODEL_SPAN_CACHE[key] = merged
    return merged


def data_run(
    name: str, data: bytes, start: int, stop: int, models: list[tuple[int, int]]
) -> list[str]:
    """One trailing-data run, with model streams carved out as `databin`.

    A `databin` writes the bytes to `assets/USA/incbin/` and `.incbin`s them
    back, so the build still matches while the disassembly stops pretending a
    mesh is 60,000 `.word` lines it understands. Everything between the carves
    stays ordinary `data`, because that is what we have *not* identified.
    """
    return data_subsegments(name, start, stop, [], models)


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


def data_subsegments(
    name: str,
    start: int,
    size: int,
    cuts: list[dict],
    models: list[tuple[int, int]] | None = None,
) -> list[str]:
    """The trailing data region as splat subsegment lines.

    The region is one `data` object by default: models, animation banks and
    scripts, which stay as split assembly. A `data` cut hands one run of it to
    a C unit instead, for the handful of symbols the decompiled code actually
    references - muzzle offset vectors, the zeroed trail/work arrays at the end
    of a package. Entries are `{ start = "<offset>", unit = "<unit>" }` for a C
    run and `{ start = "<offset>" }` to hand the rest back to assembly; the
    asm runs keep the `<name>_data`, `<name>_data_2`, ... numbering.

    Order matters and is the whole point. splat lists each object in the linker
    script at the offset of its *first* subsegment, and concatenates a section's
    contributions in that order, so a C unit only lands after the asm data if
    its subsegment is declared after it. A run at the end of a package
    therefore needs its own unit rather than the overlay's first one.
    """
    models = models or []
    lines: list[str] = []
    asm = 0

    def emit_plain(off: int) -> None:
        nonlocal asm
        asm += 1
        suffix = "" if asm == 1 else f"_{asm}"
        lines.append(f"      - [0x{off:X}, data, {name}_data{suffix}]")

    def emit_asm(off: int, stop: int) -> None:
        """One assembly run, with any model in it carved out as `databin`.

        The two mechanisms compose here rather than competing: a `data` cut
        hands a run to C because the decompiled code references it, and a model
        is content nothing references by name. Carving inside the runs the cuts
        leave behind keeps both, and keeps the `<name>_data`, `_data_2`, ...
        numbering continuous across them.
        """
        pos = off
        for m_start, m_end in models:
            if m_start < off or m_end > stop or m_end <= m_start:
                continue
            if m_start > pos:
                emit_plain(pos)
            lines.append(f"      - [0x{m_start:X}, databin, {name}_model_{m_start:05X}]")
            pos = m_end
        if pos < stop:
            emit_plain(pos)

    if not cuts:
        emit_asm(start, size)
        return lines
    ordered = sorted(cuts, key=lambda c: int(str(c["start"]), 16))
    bounds = [int(str(c["start"]), 16) for c in ordered] + [size]

    prev = -1
    if int(str(ordered[0]["start"]), 16) != start:
        emit_asm(start, bounds[0])
        prev = start
    for cut in ordered:
        off = int(str(cut["start"]), 16)
        if not start <= off < size:
            raise SystemExit(
                f"{name}: data cut 0x{off:X} is outside the trailing data "
                f"(0x{start:X}..0x{size:X})"
            )
        if off <= prev:
            raise SystemExit(f"{name}: data cuts must be strictly increasing")
        unit = cut.get("unit")
        if unit:
            unit = str(unit)
            path = unit if unit.startswith("lib/") else f"{name}/{unit}"
            lines.append(f"      - [0x{off:X}, .data, {path}]")
        else:
            emit_asm(off, bounds[ordered.index(cut) + 1])
        prev = off
    return lines


def subsegments(
    name: str,
    data: bytes,
    span: tuple[int, int] | None,
    shared: list[dict],
    rodata: list[dict],
    rodata_head: str | int | None,
    units: list[str],
    data_cuts: list[dict],
    models: list[tuple[int, int]] | None = None,
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
    shared cuts produce, so a `rodata` cut names the unit it pairs with. A cut
    whose `unit` matches a `shared` span (or is already `lib/...`) is emitted
    as `lib/<unit>` so a compiler-generated jump table from the shared C
    object lands at that offset in every overlay that carries the body.

    A data-only package still gets one `data` subsegment covering the file.
    """
    lines: list[str] = []
    models = models or []
    if span is None:
        if data_cuts:
            raise SystemExit(f"{name}: data cuts on a data-only package")
        # A data-only package - mappic, and the empty actor slots. `data_run`
        # emits the whole-file `data` subsegment when there is nothing to carve,
        # so it replaces the plain append rather than following it: emitting
        # both covers the region twice and the link fails on duplicate symbols.
        lines.extend(data_run(name, data, 0, len(data), models))
        lines.extend(trailing_segment(name, data))
        return "\n".join(lines)

    start, end = span
    cuts_pre = sorted(shared, key=lambda s: int(str(s["start"]), 16))
    # When the whole text is shared - a relocated actor copy, where the slot-2
    # package is the slot-1 package with every address shifted - there is no
    # overlay-local `c` unit for the leading rodata to pair with. It must not
    # join the shared object either: it holds this overlay's own dispatch
    # pointers and, in word 0, the package id, which is the one word the twins
    # genuinely differ in. Emit it as this overlay's own data instead.
    text_all_shared = bool(cuts_pre) and int(str(cuts_pre[0]["start"]), 16) == start \
        and int(str(cuts_pre[0]["end"]), 16) == end
    if start and text_all_shared:
        if rodata_head:
            raise SystemExit(
                f"{name}: rodata_head makes no sense when the whole text is shared"
            )
        # `rodata`, not `data`: section_order puts .rodata before .text and
        # .data after it, so a `data` header would link after the code.
        # A compiler-generated jump table still needs a `.rodata` cut so
        # GCC's table lands at the overlay offset; the overlay-local header
        # (package id, dispatch pointers, other asm tables) stays ahead of
        # the first cut and must not join the shared object.
        if rodata:
            shared_units = {s["unit"] for s in shared}
            prev = 0
            for cut in sorted(rodata, key=lambda r: int(str(r["start"]), 16)):
                cut_start = int(str(cut["start"]), 16)
                if not 0 < cut_start < start:
                    raise SystemExit(
                        f"{name}: rodata cut {cut['unit']} at 0x{cut_start:X} is "
                        f"outside the leading rodata (0x0..0x{start:X})"
                    )
                if cut_start <= prev:
                    raise SystemExit(f"{name}: rodata cuts must be strictly increasing")
                unit = str(cut["unit"])
                if prev == 0:
                    lines.append(f"      - [0x0, rodata, {name}_header]")
                if unit.startswith("lib/"):
                    lines.append(f"      - [0x{cut_start:X}, .rodata, {unit}]")
                elif unit in shared_units:
                    lines.append(f"      - [0x{cut_start:X}, .rodata, lib/{unit}]")
                elif prev == 0:
                    raise SystemExit(
                        f"{name}: the first rodata cut ({unit}) must name a shared "
                        f"unit when the whole text is shared"
                    )
                else:
                    # Tail after a jump table that sits *inside* the header.
                    # `.rodata` contributions are concatenated in subsegment
                    # order and each object appears once, so the bytes after the
                    # table cannot go back to `<name>_header` - they need an
                    # overlay-local asm unit emitted later. It must stay
                    # overlay-local: those words are this slot's own addresses,
                    # which is exactly what the shared object may not carry.
                    lines.append(f"      - [0x{cut_start:X}, rodata, {unit}]")
                prev = cut_start
        else:
            lines.append(f"      - [0x0, rodata, {name}_header]")
    elif start:
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
        shared_units = {s["unit"] for s in shared}

        def unit_path(unit: str) -> str:
            if unit.startswith("lib/"):
                return unit
            if unit in shared_units:
                return f"lib/{unit}"
            return f"{name}/{unit}"

        # The leading block belongs to the overlay's first unit by default, but
        # a jump table at the very start belongs to whichever unit holds the
        # function that uses it, and that need not be the first: both of
        # actor_800200's tables are actor_800200_2's. A cut at exactly `head`
        # renames this block instead of adding a second subsegment at the same
        # offset. Nothing already in the manifest can carry one, because until
        # now a cut had to be strictly greater than head.
        ordered = sorted(rodata, key=lambda r: int(str(r["start"]), 16))
        lead = f"{name}/{name}"
        if ordered and int(str(ordered[0]["start"]), 16) == head:
            lead = unit_path(str(ordered.pop(0)["unit"]))
        lines.append(f"      - [0x{head:X}, .rodata, {lead}]")
        for cut in ordered:
            cut_start = int(str(cut["start"]), 16)
            if not head < cut_start < start:
                raise SystemExit(
                    f"{name}: rodata cut {cut['unit']} at 0x{cut_start:X} is "
                    f"outside the leading rodata (0x{head:X}..0x{start:X})"
                )
            lines.append(f"      - [0x{cut_start:X}, .rodata, {unit_path(str(cut['unit']))}]")
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
        # Trailing data: models, animation banks, scripts - not code, except
        # for the runs a `data` cut hands to C. Models are carved out of what
        # is left. Rebasing the carve onto the data-cut work once left this
        # call unreachable after the raise below, so every overlay with a text
        # span silently kept its meshes as .word lines while the build passed.
        lines.extend(data_subsegments(name, end, len(data), data_cuts, models))
    elif data_cuts:
        raise SystemExit(f"{name}: data cuts but the package has no trailing data")
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
        # The title becomes the config's quoted `name:`, so a note containing a
        # quote would still break the yaml.
        title = f"{spec['description']} - {overlay_label(name, entry)}".replace('"', "'")

        # A family may span several RAM slots. The actors do: one actor is
        # issued as three packages at three addresses, and they have to be one
        # family so the relocated copies can share source out of
        # src/<family>/lib/.
        load = int(entry.get("load_addr", spec["load_addr"]))
        vram_end = int(entry.get("global_vram_end", spec["global_vram_end"]))

        text = template
        for key, val in {
            "TITLE": title,
            "NAME": name,
            "FAMILY": family,
            "TARGET": target.as_posix(),
            "SHA1": hashlib.sha1(data).hexdigest(),
            "SIZE": str(len(data)),
            "SIZE_HEX": f"0x{len(data):X}",
            "VRAM": f"0x{load:08X}",
            "GLOBAL_VRAM_START": f"0x{spec['global_vram_start']:08X}",
            "GLOBAL_VRAM_END": f"0x{vram_end:08X}",
            "IMPORTS": spec["imports"],
            "RELOCS": (
                f'[{entry.get("relocs", spec.get("relocs", ""))}]'
                if entry.get("relocs", spec.get("relocs"))
                else "[]"
            ),
            "SUBSEGMENTS": subsegments(
                name,
                data,
                span,
                entry.get("shared", []),
                entry.get("rodata", []),
                entry.get("rodata_head"),
                entry.get("units", []),
                entry.get("data", []),
                model_spans(name, data, load),
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
