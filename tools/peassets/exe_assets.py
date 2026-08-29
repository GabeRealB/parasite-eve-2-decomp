"""Assets baked into the executables rather than shipped as CDF chunks.

Nothing on disc references these, so the chunk walker cannot reach them. They
are located by address instead - `asset_db.EMBEDDED_ASSETS` holds the load-time
address and exact size of each - and then handed to the normal
:class:`AssetStore`, so they land in ``raw/{type}/`` and inflate into the type
directory like any other asset. There is no separate output tree for them.

Two entry points:

``collect``
    Slice the catalogued regions out of a binary. ``extract.py`` calls this and
    feeds the bytes to ``store.put_embedded``.
``materialize_save_header_asset``
    Decode one stored ``.mcsave`` into its type-store form (meta + images).
    Called from the materialize dispatch, alongside the image/BS/SPK decoders.

``scan_for_save_headers`` stays as a discovery aid: it is how the catalogue
entry was found, and re-running it after a version change will find the block
again if it moved. It is not used by extraction, which trusts the catalogue.
"""

from __future__ import annotations

import json
import logging
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

from asset_db import embedded_for_source

# PS-X EXE header: 0x800 bytes, load address at 0x18, text begins at file 0x800.
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
class Embedded:
    """One catalogued region sliced out of a binary."""

    asset_id: str
    source: str
    vram: int
    file_offset: int
    ext: str
    data: bytes

    @property
    def canonical(self) -> str:
        """Stable key for the store map, in place of a chunk path."""
        return f"{self.source}@0x{self.vram:08X}"

    @property
    def info(self) -> dict:
        return {
            "embedded_source": self.source,
            "embedded_vram": f"0x{self.vram:08X}",
            "embedded_file_offset": f"0x{self.file_offset:06X}",
        }


def load_base(data: bytes) -> int | None:
    """VRAM address of file offset 0, or None when it cannot be derived.

    A PS-X EXE loads its text at ``t_addr`` starting at file offset 0x800, so
    offset 0 maps to ``t_addr - 0x800``. Overlays are flat and carry no header;
    their base comes from the splat config and is passed in by the caller.
    """
    if data[:8] != PSX_EXE_MAGIC:
        return None
    (t_addr,) = struct.unpack_from("<I", data, 0x18)
    return t_addr - PSX_EXE_HEADER_SIZE


def collect(path: Path, *, base: int | None = None) -> list[Embedded]:
    """Slice every catalogued asset out of one binary."""
    if not path.is_file():
        logging.debug("embedded assets: %s not present, skipping", path)
        return []

    entries = embedded_for_source(path.name)
    if not entries:
        return []

    data = path.read_bytes()
    vram_base = base if base is not None else load_base(data)
    if vram_base is None:
        logging.warning(
            "embedded assets: no load base for %s; pass one for flat overlays", path.name
        )
        return []

    out: list[Embedded] = []
    for asset_id, spec in entries:
        off = spec["vram"] - vram_base
        size = spec["size"]
        if off < 0 or off + size > len(data):
            logging.warning(
                "embedded asset %s at 0x%08X is outside %s", asset_id, spec["vram"], path.name
            )
            continue
        out.append(
            Embedded(
                asset_id=asset_id,
                source=path.name,
                vram=spec["vram"],
                file_offset=off,
                ext=spec["ext"],
                data=data[off : off + size],
            )
        )
    return out


def _decode_title(raw: bytes) -> str:
    title = raw.split(b"\0", 1)[0]
    try:
        return title.decode("shift_jis")
    except UnicodeDecodeError:
        return title.decode("shift_jis", "replace")


def _clut_rgba(entry: int) -> tuple[int, int, int, int]:
    """PS1 16-bit CLUT entry to RGBA (same 5-5-5 layout as image_codec)."""
    r = (entry & 0x1F) * 255 // 31
    g = ((entry >> 5) & 0x1F) * 255 // 31
    b = ((entry >> 10) & 0x1F) * 255 // 31
    a = 0 if (entry & 0x7FFF) == 0 else 255
    return r, g, b, a


def materialize_save_header_asset(raw_path: Path, dest_dir: Path) -> None:
    """Decode a stored ``.mcsave`` into meta.json + CLUT and icon PNGs."""
    from PIL import Image  # imported here so a Pillow-less extract still runs

    block = raw_path.read_bytes()
    if len(block) < SAVE_HEADER_SIZE:
        raise ValueError(f"{raw_path.name}: short save header ({len(block)} bytes)")

    dest_dir.mkdir(parents=True, exist_ok=True)
    frames = block[2] & 0xF
    meta = {
        "kind": "save_header",
        "magic": block[:2].decode("ascii", "replace"),
        "icon_flag": f"0x{block[2]:02X}",
        "frames": frames,
        "blocks": block[3],
        "title": _decode_title(block[SAVE_TITLE_OFFSET : SAVE_TITLE_OFFSET + SAVE_TITLE_SIZE]),
        "files": ["clut.png"],
    }

    clut_raw = struct.unpack_from(f"<{SAVE_CLUT_ENTRIES}H", block, SAVE_CLUT_OFFSET)
    palette = [_clut_rgba(c) for c in clut_raw]
    clut_img = Image.new("RGBA", (SAVE_CLUT_ENTRIES, 1))
    clut_img.putdata(palette)
    clut_img.save(dest_dir / "clut.png")

    for frame in range(frames):
        start = SAVE_ICON_OFFSET + frame * SAVE_ICON_FRAME_SIZE
        pixels = block[start : start + SAVE_ICON_FRAME_SIZE]
        if len(pixels) < SAVE_ICON_FRAME_SIZE:
            logging.warning("%s: icon frame %d truncated", raw_path.name, frame)
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
        name = f"icon{frame}.png"
        img.save(dest_dir / name)
        meta["files"].append(name)

    (dest_dir / "meta.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n")


def scan_for_save_headers(data: bytes) -> Iterator[tuple[int, dict]]:
    """Find PS1 save-header blocks by signature (catalogue discovery aid).

    Strict on purpose - ``"SC"`` alone is two common bytes, so a hit also needs
    an icon flag of 0x11-0x13, a plausible block count, and a title that decodes
    as Shift-JIS. Across main.exe and both split overlays this matches exactly
    the one catalogued block and nothing else.
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
        yield off, {"frames": icon_flag & 0xF, "blocks": blocks, "title": title}
