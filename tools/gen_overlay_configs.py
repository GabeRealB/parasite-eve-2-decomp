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
import json
import re
import struct
import sys
import tomllib
from pathlib import Path

MANIFEST = Path("configs/USA/overlays.toml")
STAGES = Path("assets/USA/stages.json")
TEMPLATE = Path("configs/USA/overlay.template.yaml")
OUT_DIR = Path("configs/USA/generated")

ADDIU_SP = 0x27BD0000  # addiu $sp, $sp, -N  (negative immediate)
JR_RA = 0x03E00008


def package_paths(root: Path) -> dict[str, Path]:
    """stage-0 file id -> inflated package, straight out of stages.json.

    The inflated names (`pe2pkg_2`, `pe2pkg_3`, …) run in file-id order, so an
    id could be turned into a path arithmetically - but that is a coincidence
    of the extractor's numbering, not a fact about the game, and it would break
    silently if the extraction order ever changed. The manifest keys on the
    file id because that is what the game uses; resolve it the same way the
    game's own table does.
    """
    out: dict[str, Path] = {}

    def walk(node, path: str = "") -> None:
        if isinstance(node, dict):
            if "path" in node and str(node["path"]).endswith(".pe2pkg"):
                m = re.search(r"file(\d+)", path)
                if m:
                    out[m.group(1)] = root / node["path"]
            for k, v in node.items():
                walk(v, f"{path}/{k}")
        elif isinstance(node, list):
            for i, v in enumerate(node):
                walk(v, f"{path}[{i}]")

    walk(json.loads(STAGES.read_text(encoding="utf-8")))
    return out


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


def trailing_pad(data: bytes) -> list[str]:
    """A `pad` subsegment for a package whose size is not word-aligned.

    Package sizes are byte counts, not word counts, so a file can end part-way
    through a word. Without this the linker stops at the last whole word and the
    build comes out a couple of bytes short with every other byte identical -
    which is what `title.yaml` spells out by hand for its own 0x14AA size.
    """
    if len(data) % 4 == 0:
        return []
    return [f"      - [0x{len(data) & ~3:X}, pad]"]


def subsegments(name: str, data: bytes, span: tuple[int, int] | None) -> str:
    """The three-part package layout as splat subsegment lines.

    A data-only package still gets one `data` subsegment covering the file;
    six of the 32 weapon overlays are unused slots with no code at all.
    """
    lines: list[str] = []
    if span is None:
        lines.append(f"      - [0x0, data, {name}]")
        lines.extend(trailing_pad(data))
        return "\n".join(lines)

    start, end = span
    if start:
        # Leading rodata: the package header (task tables, jtbls) that sits
        # ahead of the first function.
        lines.append(f"      - [0x0, .rodata, {name}]")
    lines.append(f"      - [0x{start:X}, c, {name}]")
    if end < len(data):
        # Trailing data is the bulk of a weapon overlay: model streams and
        # animation banks, not code.
        lines.append(f"      - [0x{end:X}, data, {name}_data]")
    lines.extend(trailing_pad(data))
    return "\n".join(lines)


def generate(
    family: str, spec: dict, template: str, out_dir: Path, packages: dict[str, Path]
) -> list[Path]:
    written: list[Path] = []
    for file_id, entry in sorted(spec["overlays"].items()):
        name = entry["name"]
        target = packages.get(file_id)
        if target is None:
            raise SystemExit(f"{family}/{name}: file {file_id} not in {STAGES}")
        if not target.is_file():
            raise SystemExit(f"{family}/{name}: {target} not found")
        data = target.read_bytes()
        span = tuple(entry["text"]) if "text" in entry else text_span(data)
        label = entry.get("weapon") or f"unused slot {entry['item']:#04x}"
        title = f"{spec['description']} - {label} (file {file_id})"

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
            "SUBSEGMENTS": subsegments(name, data, span),
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
    packages = {} if args.list else package_paths(Path("assets/USA"))

    total = 0
    for family, spec in manifest.items():
        if args.family and family != args.family:
            continue
        if args.list:
            for fid, e in sorted(spec["overlays"].items()):
                print(f"{family}\t{fid}\t{e['name']}")
            continue
        written = generate(family, spec, template, OUT_DIR, packages)
        total += len(written)
        print(f"{family}: wrote {len(written)} config(s) to {OUT_DIR}")
    return 0 if args.list or total else 1


if __name__ == "__main__":
    sys.exit(main())
