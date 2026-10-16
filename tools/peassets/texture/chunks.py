"""Image and palette chunks: their content, read and encoded.

Encoding is exact before LZSS. What the loader uploads and every uncompressed header byte are reproduced; the
compressed stream itself is a choice of the encoder (doc/LZSS_ENCODER.md), and with it the bytes a final match
decodes past a strip or a palette, the 1-byte gaps the original compressor left between strips, and where its page
offsets point. `image_content` and `palette_content` are exactly the part that has to be identical.

An image chunk (fsBeginImageColumns, fsUploadImageStrips in src/main/fs.c):

    page table   (u16 x, u16 y, u32 offset) per page, then the terminator (0xFFFF, y, 0), then zeros up to `slots`
                 entries: the original tool sized the table for its source sheet, `slots` pages wide, and left some
                 pages out (the gapped chunks keep a slot for the missing middle page)
    terminator y the height of a single page: 0 for 256 rows, 0xFFFF for 64, else 0x8000 | rows; several pages are
                 always 256 rows
    offsets      the loader reads only the first page's; the others are where each page starts in the stream before
                 its sector padding was added, which the encoder writes the same way
    strips       each page in strips of 32 rows (4096 bytes), each an LZSS stream of its own ending in an end token,
                 decoded into one buffer that keeps its bytes between strips. A page's last strip sometimes stores
                 only 4095 bytes (57 pages on the disc, by no rule found): its last byte is then the one the strip
                 above left in the buffer. The page states it as `truncated`.
    sectors      a chunk starts a disc sector, after its 16-byte header, and each 2048-byte sector holds data in
                 its first `sector_len` bytes (the chunk header's; 0x800 unless the file says otherwise), the rest
                 padding the extraction removes. In the extracted chunk a sector is `sector_len` bytes, the first
                 16 less. After each strip the loader scans at most six zero bytes, and a zero run reaching six or
                 the sector's end means "continue in the next sector". The stored chunks let at most two strips
                 end in one sector; after the second, the next strip starts on the next sector, and so does the
                 encoder's.

A palette chunk (fsUploadImageChunk): u16 x, y, h, w, eight zero bytes, then an LZSS stream of w * h colors.
"""
from __future__ import annotations

import struct

import image_codec
from lzss import decode_lzss, encode_lzss

from .declaration import COLUMN, TEXELS

SECTOR = 0x800                                    # a disc sector; a chunk's data fills its first `sector_len` bytes
CHUNK_HEADER = 16                                 # the file's chunk header, before each chunk: sectors count from it
STRIP_ROWS = 32
STRIP_BYTES = COLUMN * 2 * STRIP_ROWS
END = 0xFFFF
ZERO_SCAN = 6                                     # zero bytes the loader skips between strips before a new sector


# --- reading: as the loader does ---

def read_strips(data: bytes, sector_len: int = SECTOR) -> list[tuple[int, int, bytes, int]]:
    """Each strip as fsUploadImageStrips uploads it: (x, y, 4096 bytes, bytes stored up to 4096).

    Each strip decodes into the same buffer, so a strip that stores fewer bytes leaves the previous strip's in place
    (the first strip's would be whatever the buffer held before the chunk: zeros here). After a strip at most six zero
    bytes are skipped; a run reaching six or the end of the sector moves the input to the next sector."""
    entries, term_y = image_codec.parse_work_entries(data)
    height = image_codec.height_from_work_entries(entries, term_y)
    buffer = bytearray(STRIP_BYTES)
    pos = entries[0].offset
    out = []
    for e in entries:
        for row in range(0, height, STRIP_ROWS):
            ring = bytearray(256)
            state = {"write": 0, "n": 0, "mask": 0, "byte": 0}

            def bits(k: int) -> int:
                nonlocal pos
                v = 0
                for _ in range(k):
                    if state["mask"] == 0:
                        state["byte"], state["mask"], pos = data[pos], 0x80, pos + 1
                    v = v << 1 | (1 if state["byte"] & state["mask"] else 0)
                    state["mask"] >>= 1
                return v

            def emit(v: int) -> None:
                ring[state["write"]] = v
                state["write"] = (state["write"] + 1) & 255
                if state["n"] < len(buffer):
                    buffer[state["n"]] = v
                state["n"] += 1

            while True:
                if bits(1):
                    emit(bits(8))
                    continue
                off = bits(8)
                if off == 0:
                    break
                length, src = bits(4) + 2, off - 1          # ring index + 1; 0 is the end token
                for i in range(length):
                    emit(ring[(src + i) & 255])
            out.append((e.x, e.y + row, bytes(buffer), min(state["n"], STRIP_BYTES)))
            zeros = 0
            while pos < len(data) and data[pos] == 0:
                pos, zeros = pos + 1, zeros + 1
                if zeros >= ZERO_SCAN or (pos + CHUNK_HEADER) % sector_len == 0:
                    pos += -(pos + CHUNK_HEADER) % sector_len
                    break
    return out


def image_pages(data: bytes, sector_len: int = SECTOR) -> tuple[int, list[tuple[int, int, bytes]]]:
    """An image chunk's height and its pages as the game has them: (x, y, little-endian words) each, `COLUMN`
    halfwords wide."""
    entries, term_y = image_codec.parse_work_entries(data)
    height = image_codec.height_from_work_entries(entries, term_y)
    strips = read_strips(data, sector_len)
    per = height // STRIP_ROWS
    return height, [(e.x, e.y, b"".join(s[2] for s in strips[n * per:(n + 1) * per])) for n, e in enumerate(entries)]


def truncated_pages(data: bytes, sector_len: int = SECTOR) -> list[bool]:
    """Which pages' last strip stores 4095 bytes."""
    entries, term_y = image_codec.parse_work_entries(data)
    per = image_codec.height_from_work_entries(entries, term_y) // STRIP_ROWS
    strips = read_strips(data, sector_len)
    return [strips[(n + 1) * per - 1][3] < STRIP_BYTES for n in range(len(entries))]


def palette_block(data: bytes) -> tuple[int, int, int, int, list[int]]:
    """A palette chunk's rectangle (x, y, w, h) and its colors, row by row."""
    x, y, h, w = struct.unpack_from("<4H", data, 0)
    raw = decode_lzss(data[16:])[: w * h * 2]
    raw += bytes(w * h * 2 - len(raw))
    return x, y, w, h, list(struct.unpack(f"<{w * h}H", raw))


def page_texels(words: bytes, bpp: int, width: int, height: int) -> list[int]:
    """Row-major texel values of the page's leftmost `width` texels: indices, or colors for 16 bits."""
    out = []
    row_bytes = COLUMN * 2
    for y in range(height):
        row = words[y * row_bytes:(y + 1) * row_bytes]
        if bpp == 4:
            texels = [n for b in row for n in (b & 15, b >> 4)]
        elif bpp == 8:
            texels = list(row)
        else:
            texels = list(struct.unpack(f"<{COLUMN}H", row))
        out += texels[:width]
    return out


def texels_words(texels: list[int], bpp: int, width: int, height: int) -> bytes:
    """The page's words from its texels; texels right of `width` are zero."""
    per_row = COLUMN * TEXELS[bpp]
    out = bytearray()
    for y in range(height):
        row = texels[y * width:(y + 1) * width] + [0] * (per_row - width)
        if bpp == 4:
            out += bytes(row[i] | row[i + 1] << 4 for i in range(0, per_row, 2))
        elif bpp == 8:
            out += bytes(row)
        else:
            out += struct.pack(f"<{per_row}H", *row)
    return bytes(out)


# --- content: what has to be identical ---

def image_content(data: bytes, sector_len: int = SECTOR) -> tuple:
    """Everything in an image chunk the encoder must reproduce: the page table less its later offsets, the slots it
    was sized for with their padding, and each strip as the game has it with the bytes it stores up to 4096."""
    entries, term_y = image_codec.parse_work_entries(data)
    strips = read_strips(data, sector_len)
    return ([(e.x, e.y) for e in entries], term_y, entries[0].offset, data[8 * (len(entries) + 1):entries[0].offset],
            [(uploaded, stored) for _x, _y, uploaded, stored in strips])


def palette_content(data: bytes) -> tuple:
    x, y, w, h, colors = palette_block(data)
    return data[:16], colors


# --- encoding ---

def height_word(height: int, pages: int) -> int:
    if pages > 1 and height != 256:
        raise ValueError(f"several pages are 256 rows, not {height}")
    return 0 if height == 256 else END if height == 64 else 0x8000 | height


def encode_image(pages: list[tuple[int, int, bytes]], height: int, slots: int | None = None,
                 sector_len: int = SECTOR, truncated: list[bool] | None = None) -> bytes:
    """An image chunk from its pages: (x, y, words) each, `height` rows of `COLUMN` halfwords. A page marked
    `truncated` stores its last strip without its last byte, which must then be the byte the strip above leaves in
    the loader's buffer."""
    slots = len(pages) if slots is None else slots
    truncated = truncated or [False] * len(pages)
    if slots < len(pages):
        raise ValueError(f"{len(pages)} pages do not fit a table of {slots} slots")
    if height % STRIP_ROWS:
        raise ValueError(f"a page is whole strips of {STRIP_ROWS} rows, not {height}")
    strips: list[list[bytes]] = []
    previous = bytes(STRIP_BYTES)                 # the buffer before the chunk, as read_strips assumes
    for (_x, _y, words), short in zip(pages, truncated):
        if len(words) != COLUMN * 2 * height:
            raise ValueError(f"a page of {height} rows is {COLUMN * 2 * height} bytes, not {len(words)}")
        page = [words[i:i + STRIP_BYTES] for i in range(0, len(words), STRIP_BYTES)]
        if short:
            above = page[-2] if len(page) > 1 else previous
            if page[-1][-1] != above[-1]:
                raise ValueError("a truncated page ends in the byte the strip above leaves; this one does not")
            page[-1] = page[-1][:-1]
        strips.append(page)
        previous = words[-STRIP_BYTES:]
    head = 8 * (slots + 1)
    encoded = [[encode_lzss(s) for s in page] for page in strips]
    offsets, at = [], head                        # where each page starts before the sector padding
    for page in encoded:
        offsets.append(at)
        at += sum(map(len, page))
    table = b"".join(struct.pack("<HHI", x, y, off) for (x, y, _w), off in zip(pages, offsets))
    table += struct.pack("<HHI", END, height_word(height, len(pages)), 0)
    out = bytearray(table.ljust(head, b"\0"))
    ends: dict[int, int] = {}
    for strip in (s for page in encoded for s in page):
        if ends.get((len(out) + CHUNK_HEADER) // sector_len, 0) >= 2:    # two strips ended here: next sector
            out += bytes(-(len(out) + CHUNK_HEADER) % sector_len)
        out += strip
        last = (len(out) - 1 + CHUNK_HEADER) // sector_len
        ends[last] = ends.get(last, 0) + 1
    return bytes(out)


def encode_palette(x: int, y: int, w: int, h: int, colors: list[int]) -> bytes:
    if len(colors) != w * h:
        raise ValueError(f"a {w}x{h} palette chunk holds {w * h} colors, not {len(colors)}")
    return struct.pack("<4H", x, y, h, w) + bytes(8) + encode_lzss(struct.pack(f"<{len(colors)}H", *colors), kind="clut")


def pad_to_sector(chunk: bytes, sector_len: int = SECTOR) -> bytes:
    """A chunk filled with zeros to the end of its last sector, as the file lays most of them out."""
    return chunk + bytes(-(len(chunk) + CHUNK_HEADER) % sector_len)
