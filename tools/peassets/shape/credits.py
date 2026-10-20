"""The credits codec (shape/credits.toml): the ending's STF document to its instance and back.

The document is laid out in one order, which the encoder repeats: the header (magic, field_4, the four tables'
offsets), the params (scroll speed and holds, the eight palette colors, the eight columns), the glyph table, the row
table (a count, then each row's command offset and bottom y), the rows' commands back to back, each list ended by
0xFF 0xFF, and the sprite table.
"""
from __future__ import annotations

import re
import struct
from typing import Any

import text_encoding

MAGIC = b"STF1"
HEADER, PARAMS, GLYPH, SPRITE = 0x18, 0x64, 4, 16
COLUMNS = 8
GLYPH_OP, PALETTE, CENTER, LEFT, RIGHT, PICTURE, SPRITE_OP, END = 0, 3, 4, 5, 6, 7, 8, 0xFF
ANCHORS = {CENTER: "center", LEFT: "left", RIGHT: "right"}
PALETTE_NAMES = ["gray", "violet", "red", "magenta", "blue", "aqua", "yellow", "light_gray"]
FACE_NAMES = ["serif", "sans", "display"]
FACE = 60                                       # glyphs of each face, in the glyph table's order
TOKEN = re.compile(r"\{([^}]*)\}|(.)", re.S)


def _faces(ctx, name: str) -> list[str]:
    """The fonts the document's glyph table holds, by their place in it."""
    table = ctx.assets[name]["outputs"]["document"]
    return sorted((k for k, r in ctx.assets.items() if r.get("type") == "font" and r.get("table") == table),
                  key=lambda k: ctx.assets[k].get("start", 0))


def _row_text(cmds: list[tuple[int, int]]) -> str:
    chars = text_encoding.CREDITS
    out, face = [], 0
    for op, arg in cmds:
        if op == GLYPH_OP and arg < FACE * len(FACE_NAMES) and arg % FACE in chars:
            if arg // FACE != face:
                face = arg // FACE
                out.append("{" + FACE_NAMES[face] + "}")
            out.append(chars[arg % FACE])
        elif op == PALETTE and arg < len(PALETTE_NAMES):
            out.append(f"{{palette {PALETTE_NAMES[arg]}}}")
        elif op in ANCHORS:
            out.append(f"{{{ANCHORS[op]} {arg}}}")
        elif op == PICTURE:
            out.append(f"{{picture {arg}}}")
        elif op == SPRITE_OP:
            out.append(f"{{sprite {arg}}}")
        else:
            out.append(f"{{op {op} {arg}}}")
    return "".join(out)


def _row_cmds(text: str) -> list[tuple[int, int]]:
    index = {c: i for i, c in text_encoding.CREDITS.items()}
    out, face = [], 0
    for m in TOKEN.finditer(text):
        tag, char = m.group(1), m.group(2)
        if char is not None:
            if char not in index:
                raise ValueError(f"credits: {char!r} is in none of the credits faces")
            out.append((GLYPH_OP, face * FACE + index[char]))
            continue
        word, *args = tag.split()
        if word in FACE_NAMES:
            face = FACE_NAMES.index(word)
        elif word == "palette":
            out.append((PALETTE, PALETTE_NAMES.index(args[0])))
        elif word in ANCHORS.values():
            out.append(({v: k for k, v in ANCHORS.items()}[word], int(args[0])))
        elif word == "picture":
            out.append((PICTURE, int(args[0])))
        elif word == "sprite":
            out.append((SPRITE_OP, int(args[0])))
        elif word == "op":
            out.append((int(args[0]), int(args[1])))
        else:
            raise ValueError(f"credits: unknown markup {{{tag}}}")
    return out


SPRITE_FIELDS = ("tpage_x", "tpage_y", "clut_x", "clut_y", "u", "v", "pixel_mode", "field_b", "width", "height")


def decode(shape: str, stored: dict[str, bytes], decls: dict, ctx, name: str) -> dict[str, Any]:
    d = stored["document"]
    magic, field_4, params, glyphs, lines, sprites = struct.unpack_from("<4si4i", d, 0)
    if magic != MAGIC or params != HEADER or glyphs != params + PARAMS or lines <= glyphs:
        raise ValueError(f"{name}: not laid out as the credits are (header, params, glyphs, rows, commands, sprites)")
    speed, start_hold, end_hold = struct.unpack_from("<HBB", d, params)
    colors = []
    for i in range(8):
        r, g, b, zero = d[params + 4 + 4 * i:params + 8 + 4 * i]
        if zero:
            raise ValueError(f"{name}: palette color {i} has a fourth byte")
        colors.append(f"{r:02X}{g:02X}{b:02X}")
    columns = [dict(zip(("center", "left", "right", "field_6"), struct.unpack_from("<4H", d, params + 36 + 8 * c)))
               for c in range(COLUMNS)]
    count = struct.unpack_from("<i", d, lines)[0]
    rows, previous, at = [], 0, lines + 4 + 8 * count
    for i in range(count):
        offset, y = struct.unpack_from("<ii", d, lines + 4 + 8 * i)
        if offset != at:
            raise ValueError(f"{name}: row {i}'s commands are not where the previous row's end")
        cmds = []
        while d[at] != END:
            cmds.append((d[at], d[at + 1])); at += 2
        if d[at + 1] != END:
            raise ValueError(f"{name}: row {i} ends with an operand of {d[at + 1]}")
        at += 2
        rows.append({"gap": y - previous, "text": _row_text(cmds)})
        previous = y
    if at != sprites or (len(d) - sprites) % SPRITE:
        raise ValueError(f"{name}: the sprite table does not follow the rows, or is not whole records")
    sprite_list = []
    for i in range((len(d) - sprites) // SPRITE):
        values = struct.unpack_from("<4H4B2H", d, sprites + SPRITE * i)
        sprite_list.append(dict(zip(SPRITE_FIELDS, values)))
    return {"field_4": field_4, "scroll_speed": speed, "start_hold": start_hold, "end_hold": end_hold,
            "palette_colors": colors, "columns": columns, "glyph_slots": (lines - glyphs) // GLYPH,
            "rows": rows, "sprites": sprite_list}


def encode(shape: str, value: dict[str, Any], decls: dict, ctx, name: str) -> dict[str, bytes]:
    from font import declared as fonts
    glyphs = b"".join(fonts.encode(face, ctx.assets, ctx.assets_root) for face in _faces(ctx, name))
    glyphs += bytes(GLYPH * value["glyph_slots"] - len(glyphs))
    params = struct.pack("<HBB", value["scroll_speed"], value["start_hold"], value["end_hold"])
    params += b"".join(bytes.fromhex(c) + b"\0" for c in value["palette_colors"])
    params += b"".join(struct.pack("<4H", c["center"], c["left"], c["right"], c["field_6"]) for c in value["columns"])
    lines_at = HEADER + len(params) + len(glyphs)
    cmds_at = lines_at + 4 + 8 * len(value["rows"])
    table, body, y = struct.pack("<i", len(value["rows"])), b"", 0
    for row in value["rows"]:
        y += row["gap"]
        table += struct.pack("<ii", cmds_at + len(body), y)
        body += b"".join(bytes(c) for c in _row_cmds(row["text"])) + bytes([END, END])
    sprites_at = cmds_at + len(body)
    sprites = b"".join(struct.pack("<4H4B2H", *(s[f] for f in SPRITE_FIELDS)) for s in value["sprites"])
    header = MAGIC + struct.pack("<i4i", value["field_4"], HEADER, HEADER + len(params), lines_at, sprites_at)
    return {"document": header + params + glyphs + table + body + sprites}
