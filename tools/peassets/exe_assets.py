"""Decode assets embedded in the executables themselves.

Most PE2 content lives in CDF chunks and is handled by ``extract.py``. A few
things are baked into ``SLUS_010.42`` and the overlays instead, and those never
reach the type store: nothing on disc references them, so the chunk walker
cannot see them.

This module scans a loaded binary for such assets by signature and writes the
decoded form under ``<out>/embedded/decoded/<binary>/<asset>/``. The raw bytes
also reach the build separately, as splat ``databin`` segments under
``<out>/embedded/`` - that path is what keeps the game data out of git, while
this one exists so the content is inspectable.

Currently recognised:

``save_header``
    A PlayStation memory-card save header block (512 bytes): ``"SC"`` magic, an
    icon-frame count, the block count, a Shift-JIS title, a 16-entry CLUT and
    up to three 16x16 4bpp icon frames. PE2 embeds one - the entry it writes
    for a card whose data failed to validate.

Adding a type means writing a ``find_*`` scanner and a ``decode_*`` writer and
listing the pair in :data:`SCANNERS`.
"""

from __future__ import annotations

import json
import logging
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterator

try:
    from PIL import Image
except ImportError as e:  # pragma: no cover
    raise ImportError(
        "Pillow is required for embedded asset decode (pip install pillow)"
    ) from e

# PS-X EXE header: 0x800 bytes, load address at 0x18, text starts at file 0x800.
PSX_EXE_MAGIC = b"PS-X EXE"
PSX_EXE_HEADER_SIZE = 0x800

SAVE_HEADER_SIZE = 0x200
SAVE_TITLE_OFFSET = 0x04
SAVE_TITLE_SIZE = 0x40 - SAVE_TITLE_OFFSET
SAVE_CLUT_OFFSET = 0x60
SAVE_CLUT_ENTRIES = 16
SAVE_ICON_OFFSET = 0x80
SAVE_ICON_FRAME_SIZE = 0x80
SAVE_ICON_DIM = 16


@dataclass
class Find:
    """One recognised asset inside a binary."""

    kind: str
    offset: int
    size: int
    name: str
    info: dict = field(default_factory=dict)

    def vram(self, base: int | None) -> int | None:
        return None if base is None else base + self.offset


def load_base(data: bytes) -> int | None:
    """VRAM address of file offset 0, or None when it cannot be derived.

    For a PS-X EXE the text segment loads at ``t_addr`` and begins at file
    offset 0x800, so offset 0 maps to ``t_addr - 0x800``. Overlays are flat and
    carry no header; their base comes from the splat config, so callers pass it
    in rather than expecting it here.
    """
    if data[:8] != PSX_EXE_MAGIC:
        return None
    (t_addr,) = struct.unpack_from("<I", data, 0x18)
    return t_addr - PSX_EXE_HEADER_SIZE


def _decode_title(raw: bytes) -> str:
    title = raw.split(b"\0", 1)[0]
    try:
        return title.decode("shift_jis")
    except UnicodeDecodeError:
        return title.decode("shift_jis", "replace")


def find_save_headers(data: bytes) -> Iterator[Find]:
    """Locate PS1 save-header blocks.

    The signature is deliberately strict - ``"SC"`` alone is two common bytes.
    A real header also has an icon flag of 0x11/0x12/0x13 (1-3 frames), a
    plausible block count, and a title that decodes as Shift-JIS.
    """
    for off in range(0, max(0, len(data) - SAVE_HEADER_SIZE) + 1):
        if data[off : off + 2] != b"SC":
            continue
        icon_flag = data[off + 2]
        blocks = data[off + 3]
        if icon_flag not in (0x11, 0x12, 0x13) or not 1 <= blocks <= 15:
            continue
        raw_title = data[off + SAVE_TITLE_OFFSET : off + SAVE_TITLE_OFFSET + SAVE_TITLE_SIZE]
        try:
            title = raw_title.split(b"\0", 1)[0].decode("shift_jis")
        except UnicodeDecodeError:
            continue
        if len(title) < 2:
            continue
        frames = icon_flag & 0xF
        yield Find(
            kind="save_header",
            offset=off,
            size=SAVE_HEADER_SIZE,
            name="save_header",
            info={"frames": frames, "blocks": blocks, "title": title},
        )


def _clut_rgba(entry: int) -> tuple[int, int, int, int]:
    """PS1 16-bit CLUT entry to RGBA.

    Same 5-5-5 layout ``image_codec`` uses: index 0 of a save icon palette is
    the transparent slot, and an all-zero entry is transparent regardless.
    """
    r = (entry & 0x1F) * 255 // 31
    g = ((entry >> 5) & 0x1F) * 255 // 31
    b = ((entry >> 10) & 0x1F) * 255 // 31
    a = 0 if (entry & 0x7FFF) == 0 else 255
    return r, g, b, a


def decode_save_header(data: bytes, find: Find, out_dir: Path) -> list[Path]:
    """Write the title/CLUT/icon frames of one save header."""
    out_dir.mkdir(parents=True, exist_ok=True)
    block = data[find.offset : find.offset + find.size]
    written: list[Path] = []

    clut_raw = struct.unpack_from(f"<{SAVE_CLUT_ENTRIES}H", block, SAVE_CLUT_OFFSET)
    palette = [_clut_rgba(c) for c in clut_raw]

    clut_img = Image.new("RGBA", (SAVE_CLUT_ENTRIES, 1))
    clut_img.putdata(palette)
    clut_path = out_dir / "clut.png"
    clut_img.save(clut_path)
    written.append(clut_path)

    for frame in range(find.info["frames"]):
        start = SAVE_ICON_OFFSET + frame * SAVE_ICON_FRAME_SIZE
        pixels = block[start : start + SAVE_ICON_FRAME_SIZE]
        if len(pixels) < SAVE_ICON_FRAME_SIZE:
            logging.warning("save header at 0x%X: frame %d truncated", find.offset, frame)
            break
        img = Image.new("RGBA", (SAVE_ICON_DIM, SAVE_ICON_DIM))
        # 4bpp, two pixels per byte, low nibble is the left pixel.
        img.putdata(
            [
                palette[(pixels[y * 8 + x // 2] >> 4) if x % 2 else (pixels[y * 8 + x // 2] & 0xF)]
                for y in range(SAVE_ICON_DIM)
                for x in range(SAVE_ICON_DIM)
            ]
        )
        path = out_dir / f"icon{frame}.png"
        img.save(path)
        written.append(path)

    return written


SCANNERS: dict[str, tuple[Callable[[bytes], Iterator[Find]], Callable[[bytes, Find, Path], list[Path]]]] = {
    "save_header": (find_save_headers, decode_save_header),
}


def extract_from_binary(
    path: Path, out_root: Path, *, base: int | None = None, kinds: list[str] | None = None
) -> list[Find]:
    """Scan one binary and write every recognised asset under ``out_root``."""
    if not path.is_file():
        logging.debug("embedded assets: %s not present, skipping", path)
        return []

    data = path.read_bytes()
    vram_base = base if base is not None else load_base(data)
    stem = path.name.split(".")[0]
    found: list[Find] = []

    for kind, (scan, decode) in SCANNERS.items():
        if kinds is not None and kind not in kinds:
            continue
        for find in scan(data):
            out_dir = out_root / stem / f"{find.name}_{find.offset:06X}"
            files = decode(data, find, out_dir)
            meta = {
                "kind": find.kind,
                "source": path.name,
                "file_offset": f"0x{find.offset:06X}",
                "size": find.size,
                "vram": None if find.vram(vram_base) is None else f"0x{find.vram(vram_base):08X}",
                **find.info,
                "files": [f.name for f in files],
            }
            (out_dir / "meta.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n")
            logging.info(
                "  embedded %s in %s @ 0x%06X -> %s",
                find.kind,
                path.name,
                find.offset,
                out_dir,
            )
            found.append(find)

    return found


def extract_embedded_assets(output_path: Path, *, overlay_bases: dict[str, int] | None = None) -> int:
    """Scan the split targets and decode what is recognised.

    The targets are exactly what the build needs, so this works after a minimal
    extraction too: ``main.exe`` plus the required ``pe2pkg`` overlays.
    """
    overlay_bases = overlay_bases or {}
    out_root = output_path / "embedded" / "decoded"
    targets: list[tuple[Path, int | None]] = [(output_path / "main.exe", None)]
    pkg_dir = output_path / "pe2pkg"
    if pkg_dir.is_dir():
        targets += [(p, overlay_bases.get(p.stem)) for p in sorted(pkg_dir.glob("*.pe2pkg"))]

    total = 0
    for path, base in targets:
        total += len(extract_from_binary(path, out_root, base=base))

    if total:
        logging.info("Decoded %d embedded asset(s) under %s", total, out_root)
    else:
        logging.info("No embedded assets recognised in the split targets")
    return total
