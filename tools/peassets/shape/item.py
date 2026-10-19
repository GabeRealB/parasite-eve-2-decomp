"""The item shapes' codec (shape/item.toml): an item's catalog fields, its information panel and its descriptor
row, and the catalog that assembles the descriptor tables.

The catalog is six LF-terminated lines (itemGetText): name, two description lines, then the unidentified name
and its two lines. The panel (itemMenuInfoTask) is CRLF lines ending at `\\Z`: `rows` header rows, then the list.

An effect sheet is laid out by fixed advances in pixels from one line to the next - heading to effect 12, effect
to effect 13, effect to heading 18, heading to stacked heading 9, a stacked pair starting 2 down - and each line
states its distance from its 15-pixel row as `\\U<n>` (up) or `\\D<n>` (down), which is how the stored sheets are
spaced, every one. A heading is `\\W1\\Ss<HEADING>\\W0` in the small opaque font, the first one ending `\\Sm`
(`first_heading_resets_font`); an effect is `\\W0\\U<n>\\Cw\\Sm  <action>\\Cg <value>\\Cw <unit>`, its value in
green, without the space before a value that has no action (`  \\Cg???`).
"""
from __future__ import annotations

import re
import struct
from typing import Any

ROW = 15                                        # pixels from one header row to the next (itemMenuInfoTask)
HEADINGS = {"use": "USE", "attachment": "ATTACHMENT"}
ADVANCE = {("heading", "effect"): 12, ("effect", "effect"): 13, ("effect", "heading"): 18, ("heading", "heading"): 9}
STACKED_START = 2                               # a sheet opening on two stacked headings starts this far down
EFFECT = re.compile(r"\\W0(?P<move>\\[UD]\d)?\\Cw\\Sm  (?P<action>.*?)(?:\\Cg ?(?P<value>.*?))?(?:\\Cw (?P<unit>.*))?")
HEADING = re.compile(r"\\W1(?P<move>\\[UD]\d)?\\Ss(?P<name>[A-Z]+)\\W0(?P<reset>\\Sm)?")


class Undecodable(ValueError):
    pass


# --- catalog ---

def _form(lines: list[str]) -> dict[str, Any]:
    description = lines[1:3]
    while description and not description[-1]:
        description.pop()
    return {"name": lines[0], "description": description}


def decode_catalog(text: bytes) -> dict[str, Any]:
    lines = text.decode("latin-1").split("\n")
    if len(lines) != 7 or lines[6]:
        raise Undecodable("a catalog is six lines, each ending in a line break")
    out = _form(lines[:3])
    unidentified = _form(lines[3:6])
    if unidentified["name"] or unidentified["description"]:
        out["unidentified"] = unidentified
    return out


def encode_catalog(value: dict[str, Any]) -> bytes:
    def form(f):
        return [f["name"], *(f["description"] + ["", ""])[:2]]
    return ("\n".join(form(value) + form(value["unidentified"])) + "\n").encode("latin-1")


# --- the effect sheet ---

def _sheet_lines(effects: list[dict[str, Any]], reset_first: bool) -> list[str]:
    placed = []                                 # (kind, text, y)
    y, previous = None, None
    for group in effects:
        stacked = len(group["when"]) > 1
        for kind, item in [("heading", w) for w in group["when"]] + [("effect", e) for e in group["lines"]]:
            if y is None:
                y = STACKED_START if stacked else 0
            else:
                y += ADVANCE[previous, kind]
            placed.append((kind, item, y))
            previous = kind
    out, first_heading = [], True
    for row, (kind, item, y) in enumerate(placed):
        offset = y - row * ROW
        if abs(offset) > 9:
            raise Undecodable(f"row {row} sits {offset} pixels from its row, more than one \\U or \\D moves")
        move = "" if offset == 0 else f"\\U{-offset}" if offset < 0 else f"\\D{offset}"
        if kind == "heading":
            out.append(f"\\W1{move}\\Ss{HEADINGS[item]}\\W0" + ("\\Sm" if first_heading and reset_first else ""))
            first_heading = False
        else:
            text = f"\\W0{move}\\Cw\\Sm  {item['action']}"
            if item["value"]:
                text += "\\Cg" + (" " if item["action"] else "") + item["value"]
            if item["unit"]:
                text += f"\\Cw {item['unit']}"
            out.append(text)
    return out


def _decode_sheet(lines: list[str]) -> tuple[list[dict[str, Any]], bool]:
    effects: list[dict[str, Any]] = []
    reset = None
    for line in lines:
        if m := HEADING.fullmatch(line):
            name = {v: k for k, v in HEADINGS.items()}.get(m["name"])
            if name is None:
                raise Undecodable(f"unknown heading {m['name']!r}")
            if reset is None:
                reset = bool(m["reset"])
            if effects and not effects[-1]["lines"]:
                effects[-1]["when"].append(name)
            else:
                effects.append({"when": [name], "lines": []})
        elif (m := EFFECT.fullmatch(line)) and effects:
            effects[-1]["lines"].append({k: m[k] or "" for k in ("action", "value", "unit")})
        else:
            raise Undecodable(f"not a heading or an effect: {line!r}")
    return effects, True if reset is None else reset


# --- the weapons that load an ammunition ---

COLUMN = 6                                      # characters of a first-column cell, padded
TABLE_NAME = 6                                  # names up to this long share rows, two to a row


def _loaded_by_lines(names: list[str], gaps: list[int]) -> list[str]:
    """One name a row after a space; two or more short names in two columns, each first cell padded to COLUMN
    (a lone last one too), the second starting after it unless `gaps` states the spaces between them."""
    if len(names) < 2 or any(len(n) > TABLE_NAME for n in names):
        return [f" {n}" for n in names]
    rows = [names[i:i + 2] for i in range(0, len(names), 2)]
    pairs = [r for r in rows if len(r) == 2]
    if gaps and len(gaps) != len(pairs):
        raise Undecodable("column_gaps states one gap for each two-name row")
    out, pair = [], 0
    for row in rows:
        if len(row) == 1:
            out.append(" " + row[0].ljust(COLUMN))
            continue
        gap = gaps[pair] if gaps else COLUMN - len(row[0])
        out.append(" " + row[0] + " " * gap + row[1])
        pair += 1
    return out


def _decode_loaded_by(lines: list[str]) -> tuple[list[str], list[int]]:
    """The names, read as two columns where every word is a short name, else one name a row; and the gaps, where
    the columns are not spaced by the rule."""
    rows = [line.split() for line in lines]
    names = [n for row in rows for n in row]
    if len(names) >= 2 and all(len(n) <= TABLE_NAME for n in names) and all(1 <= len(r) <= 2 for r in rows):
        if _loaded_by_lines(names, []) == lines:
            return names, []
        gaps = [len(line) - 1 - len(r[0]) - len(r[1]) for line, r in zip(lines, rows) if len(r) == 2]
        if _loaded_by_lines(names, gaps) == lines:
            return names, gaps
    names = [line[1:] for line in lines]
    if _loaded_by_lines(names, []) == lines:
        return names, []
    raise Undecodable(f"the weapon list {lines!r} follows neither layout")


# --- the panel ---

def _split_panel(text: bytes) -> tuple[list[str], list[str], int, bool]:
    t = text.decode("latin-1")
    final_newline = t.endswith("\\Z\r\n")
    if not (final_newline or t.endswith("\\Z")):
        raise Undecodable("a panel text ends at \\Z")
    lines = t[: -len("\\Z\r\n") if final_newline else -len("\\Z")].split("\r\n")
    if lines[-1]:
        raise Undecodable("\\Z starts a line")
    lines = lines[:-1]
    if any("\n" in line or "\r" in line for line in lines):
        raise Undecodable("a panel text breaks its lines with CRLF only")
    rows = min(len(lines), 5)
    return lines[:rows], lines[rows:], rows, final_newline


def _join_panel(header: list[str], rows: int, body: list[str], final_newline: bool) -> bytes:
    lines = header + [""] * (rows - len(header)) + body
    return ("".join(f"{line}\r\n" for line in lines) + "\\Z" + ("\r\n" if final_newline else "")).encode("latin-1")


# --- the descriptor row (ItemDesc) ---

ROW_FORMAT = struct.Struct("<HBBI")             # price, classification, flags, catalog text address
FLAGS = {"no_discard": 0x01, "flag_02": 0x02, "no_attachment": 0x04, "flag_08": 0x08}
EMPTY_TEXT = "_gItemEmptyTextFields"            # the catalog text of every row without one of its own


def _text_address(name: str, ctx) -> int:
    """Where an item's row points: its catalog text, or the shared empty one."""
    catalog = ctx.assets[name]["outputs"].get("catalog")
    return ctx.address(catalog) if catalog else ctx.symbols[EMPTY_TEXT]


def _decode_descriptor(shape: str, row: bytes, decls: dict, ctx, name: str) -> dict[str, Any]:
    price, classification, flags, text = ROW_FORMAT.unpack(row)
    if row == ROW_FORMAT.pack(0, 0, 0, _text_address(name, ctx)) and decls[shape]["category"]:
        return {"blank_row": True}               # an unused id: the empty row, without the shape's category
    if classification >> 4 != decls[shape]["category"]:
        raise Undecodable(f"classification 0x{classification:02X} is not a {shape}'s (category {decls[shape]['category']})")
    if flags & ~sum(FLAGS.values()):
        raise Undecodable(f"flags 0x{flags:02X} hold a bit no field states")
    if text != _text_address(name, ctx):
        raise Undecodable(f"the row points at 0x{text:08X}, not at its catalog text")
    return {"price": price, **_decode_subtype(shape, classification & 0xF),
            **{k: bool(flags & bit) for k, bit in FLAGS.items()}}


def _encode_descriptor(shape: str, value: dict[str, Any], decls: dict, ctx, name: str) -> bytes:
    flags = sum(bit for k, bit in FLAGS.items() if value[k])
    category = 0 if value["blank_row"] else decls[shape]["category"]
    return ROW_FORMAT.pack(value["price"], category << 4 | _encode_subtype(shape, value), flags,
                           _text_address(name, ctx))


# The classification's low nibble, by shape: an ammunition's caliber, a recovery item's medicine mark, a weapon's
# class; a key item's is always 0.
CALIBERS = {"none": 0, "9mm": 1, "40_magnum": 2, "44_magnum": 3, "40mm": 4, "12_gauge": 5, "5_56mm": 6,
            "battery": 8, "fuel": 9}


def _decode_subtype(shape: str, subtype: int) -> dict[str, Any]:
    if shape == "ammunition":
        caliber = {v: k for k, v in CALIBERS.items()}.get(subtype)
        if caliber is None:
            raise Undecodable(f"subtype {subtype} is no caliber")
        return {"caliber": caliber}
    if shape == "effect_item" and subtype in (0, 1):
        return {"medicine": bool(subtype)}
    if shape == "weapon":
        return {"weapon_class": subtype}
    if subtype == 0:
        return {}
    raise Undecodable(f"a {shape}'s subtype is not {subtype}")


def _encode_subtype(shape: str, value: dict[str, Any]) -> int:
    if shape == "ammunition":
        return CALIBERS[value["caliber"]]
    if shape == "effect_item":
        return int(value["medicine"])
    if shape == "weapon":
        return value["weapon_class"]
    return 0


# --- the weapon sheet ---

STAT = re.compile(r"\\U2\\W1\\Ss(?P<label>[A-Z]+)\\D2\\W0\\Sm\\B9(?P<value>.*)")
FIRES_INDENT = "     "


def _stat_line(stat: dict[str, str]) -> str:
    return f"\\U2\\W1\\Ss{stat['label']}\\D2\\W0\\Sm\\B9{stat['value']}"


# --- a Parasite Energy's description in the PE menu ---

MENU_OFFSET = 4                                 # blank rows above a PE menu description


def _decode_menu(text: bytes) -> list[str]:
    header, body, rows, final_newline = _split_panel(text)
    lines = header + body
    if lines[:MENU_OFFSET] != [""] * MENU_OFFSET or not final_newline:
        raise Undecodable(f"a PE menu text is {MENU_OFFSET} blank rows, then its description")
    return lines[MENU_OFFSET:]


def _encode_menu(lines: list[str]) -> bytes:
    rows = [""] * MENU_OFFSET + lines
    return ("".join(f"{line}\r\n" for line in rows) + "\\Z\r\n").encode("latin-1")


# --- an item ---

def decode(shape: str, stored: dict[str, bytes], decls: dict, ctx, name: str) -> dict[str, Any]:
    if shape == "item_catalog":
        return decode_tables(stored, decls, ctx, name)
    value: dict[str, Any] = decode_catalog(stored["catalog"]) if "catalog" in stored else {}
    value.update(_decode_descriptor(shape, stored["descriptor"], decls, ctx, name))
    if "info" not in stored:                    # no preview file (the rescue bonuses)
        return value
    header, body, rows, final_newline = _split_panel(stored["info"])
    value.update(rows=rows, final_newline=final_newline)
    used = [line for line in header if line]
    if shape == "weapon":                       # its rows keep their places: a stat it lacks is a blank row
        while header and not header[-1]:
            header = header[:-1]
        stats = [STAT.fullmatch(line) if line else None for line in header]
        if any(line and not m for line, m in zip(header, stats)):
            raise Undecodable("a weapon's header is its stat rows")
        value["stats"] = [{"label": m["label"], "value": m["value"]} if m else {} for m in stats]
        offset = next((n for n, line in enumerate(body) if line), len(body))
        if any(not line.startswith(FIRES_INDENT) for line in body[offset:]):
            raise Undecodable(f"a weapon's list lines are indented {len(FIRES_INDENT)} spaces")
        value.update(list_offset=offset, fires=[line[len(FIRES_INDENT):] for line in body[offset:]])
        return value
    if header[:len(used)] != used:
        raise Undecodable("the header's lines start at its first row")
    if shape == "effect_item":
        value["effects"], value["first_heading_resets_font"] = _decode_sheet(used)
        value["body"] = body
        return value
    if shape == "parasite_energy":
        if used or any(body) or not final_newline:
            raise Undecodable("a Parasite Energy's shop-row text is blank rows")
        value["info_rows"] = rows + len(body)
        value.pop("rows"), value.pop("final_newline")
        value["menu_text"] = _decode_menu(stored["menu"])
        return value
    if used:
        raise Undecodable(f"a {shape}'s header is blank")
    if shape == "armor":
        offset = next((n for n, line in enumerate(body) if line), len(body))
        if any(not line.startswith(" ") for line in body[offset:]):
            raise Undecodable("an armor's feature lines start with a space")
        value.update(list_offset=offset, features=[line[1:] for line in body[offset:]])
        return value
    if shape == "ammunition":
        offset = next((n for n, line in enumerate(body) if line), len(body))
        value["list_offset"] = offset
        value["loaded_by"], value["column_gaps"] = _decode_loaded_by(body[offset:])
    elif shape == "key_item":
        value["text"] = body
    else:
        raise Undecodable(f"no codec for the shape {shape}")
    return value


def encode(shape: str, value: dict[str, Any], decls: dict, ctx, name: str) -> dict[str, bytes]:
    if shape == "item_catalog":
        return encode_tables(value, decls, ctx)
    out = {"catalog": encode_catalog(value), "descriptor": _encode_descriptor(shape, value, decls, ctx, name)}
    if shape == "effect_item":
        header, body = _sheet_lines(value["effects"], value["first_heading_resets_font"]), value["body"]
    elif shape == "weapon":
        header = [_stat_line(stat) if stat["label"] else "" for stat in value["stats"]]
        body = [""] * value["list_offset"] + [FIRES_INDENT + line for line in value["fires"]]
    elif shape == "parasite_energy":
        out["menu"] = _encode_menu(value["menu_text"])
        out["info"] = ("\r\n" * value["info_rows"] + "\\Z\r\n").encode("latin-1")
        return out
    elif shape == "armor":
        header, body = [], [""] * value["list_offset"] + [f" {line}" for line in value["features"]]
    elif shape == "ammunition":
        header, body = [], [""] * value["list_offset"] + _loaded_by_lines(value["loaded_by"], value["column_gaps"])
    elif shape == "key_item":
        header, body = [], value["text"]
    else:
        raise Undecodable(f"no codec for the shape {shape}")
    out["info"] = _join_panel(header, value["rows"], body, value["final_newline"])
    return out


# --- the catalog: gItemDescriptors and gItemKeyDescriptors ---

TABLES = {"items": (0x00, 0xC0), "key_items": (0x100, 0x80)}       # output: first id, rows
# The instance lists ids by kind; each kind is its ranges of one table.
SECTIONS = {
    "items": ("items", [(0x00, 0x0F), (0x33, 0x60)]),                 # used or attached
    "parasite_energy": ("items", [(0x0F, 0x33)]),                     # twelve energies, three levels each
    "armor": ("items", [(0x60, 0x80)]),
    "weapons": ("items", [(0x80, 0xA0)]),
    "ammunition": ("items", [(0xA0, 0xC0)]),
    "key_items": ("key_items", [(0x100, 0x180)]),
}


def _section_of(item_id: int, **default) -> str | None:
    found = [k for k, (_t, ranges) in SECTIONS.items() if any(a <= item_id < b for a, b in ranges)]
    if not found and "default" in default:
        return default["default"]
    return found[0]


def _rows_by_address(ctx) -> dict[int, str]:
    """Each item's descriptor row address -> the item."""
    out = {}
    for key in ctx.shaped():
        row = ctx.assets[key]["outputs"].get("descriptor")
        if row is not None:
            out[ctx.address(row)] = key
    return out


def _empty_row(ctx) -> bytes:
    return ROW_FORMAT.pack(0, 0, 0, ctx.symbols[EMPTY_TEXT])


EMPTY_CATALOG = b"\n" * 6                    # the catalog text of every row without an item


def decode_tables(stored: dict[str, bytes], decls: dict, ctx, name: str) -> dict[str, Any]:
    if stored["empty"] != EMPTY_CATALOG:
        raise Undecodable("the empty catalog text is six empty lines")
    owners = _rows_by_address(ctx)
    value: dict[str, Any] = {"junk": list(stored["junk"])}
    for output, (first, count) in TABLES.items():
        data, base = stored[output], ctx.address(ctx.assets[name]["outputs"][output])
        if len(data) != count * ROW_FORMAT.size:
            raise Undecodable(f"{output} is {count} rows of {ROW_FORMAT.size} bytes")
        entries = {}
        for n in range(count):
            row = data[n * ROW_FORMAT.size:(n + 1) * ROW_FORMAT.size]
            key = f"0x{first + n:02X}"
            item = owners.get(base + n * ROW_FORMAT.size)
            if item is not None:
                entries[key] = item
            elif row != _empty_row(ctx):
                price, classification, flags, text = ROW_FORMAT.unpack(row)
                entries[key] = {"price": price, "classification": classification, "flags": flags,
                                "text": ctx.symbol_at(text)}
        for key, entry in entries.items():
            value.setdefault(_section_of(int(key, 16)), {})[key] = entry
    return value


def _table_rows(output: str, value: dict[str, Any], decls: dict, ctx) -> list[bytes]:
    import shape as shapes
    first, count = TABLES[output]
    rows = []
    listed = {k: v for section, (table, _r) in SECTIONS.items() if table == output for k, v in value[section].items()}
    for n in range(count):
        entry = listed.get(f"0x{first + n:02X}")
        if entry is None:
            rows.append(_empty_row(ctx))
        elif isinstance(entry, str):
            rec = ctx.assets[entry]
            rows.append(shapes.encode_value(rec["shape"], ctx.instance(entry), decls, ctx, entry)["descriptor"])
        else:
            rows.append(ROW_FORMAT.pack(entry["price"], entry["classification"], entry["flags"],
                                        ctx.symbols[entry["text"]]))
    return rows


def encode_tables(value: dict[str, Any], decls: dict, ctx) -> dict[str, bytes]:
    for section in SECTIONS:
        for key in value[section]:
            if not (re.fullmatch(r"0x[0-9A-F]+", key) and _section_of(int(key, 16), default=None) == section):
                raise Undecodable(f"{section}: {key!r} is not one of its ids")
    return {**{output: b"".join(_table_rows(output, value, decls, ctx)) for output in TABLES}, "empty": EMPTY_CATALOG,
            "junk": bytes(value["junk"])}


def _table_c(output: str, value: dict[str, Any], decls: dict, ctx) -> list[str]:
    """A descriptor table's rows as C, each pointing at its text object by name (gameplay's symbol map)."""
    lines = []
    for row in _table_rows(output, value, decls, ctx):
        price, classification, flags, text = ROW_FORMAT.unpack(row)
        lines.append(f"    {{ {price}, 0x{classification:02X}, 0x{flags:02X}, {ctx.symbol_at(text)} }},")
    return lines


def c_source(shape: str, value: dict[str, Any], decls: dict, ctx, name: str) -> str | None:
    """The catalog's whole C: every item's catalog text, in address order, then both descriptor tables.

    The texts stay separate arrays - string literals would merge the identical ones (the four batteries' are four
    objects) and be placed after the tables in reverse order - named by gameplay's symbol map. src/gameplay/item_catalog.c
    includes it, an object of its own."""
    if shape != "item_catalog":
        return None
    import shape as shapes
    texts = {ctx.address(ctx.assets[name]["outputs"]["empty"]): EMPTY_CATALOG}
    for key in ctx.shaped():
        catalog = ctx.assets[key]["outputs"].get("catalog")
        if catalog is not None and ctx.assets[key]["shape"] != "item_catalog":
            rec = ctx.assets[key]
            texts[ctx.address(catalog)] = shapes.encode_value(rec["shape"], ctx.instance(key), decls, ctx, key)["catalog"]
    lines = []
    for address in sorted(texts):
        data = texts[address]
        lines.append(f"static const u8 {ctx.symbol_at(address)}[] =")
        lines += [f"    {literal}" for literal in shapes.c_string(data)]
        lines[-1] += ";"
    if value["junk"]:                           # the object's own last bytes, past its last text
        # Bytes, not an array: GCC word-aligns any array, and these follow the text unaligned.
        address = ctx.address(ctx.assets[name]["outputs"]["junk"])
        if len(value["junk"]) != 1:
            raise Undecodable("the catalog's junk is one byte")
        lines.append(f"static const u8 {ctx.symbol_at(address)} = 0x{value['junk'][0]:02X};")
    for output, table, count in (("items", "gItemDescriptors", "ITEM_DESCRIPTOR_COUNT"),
                                 ("key_items", "gItemKeyDescriptors", "ITEM_KEY_DESCRIPTOR_COUNT")):
        lines += ["", f"ItemDesc {table}[{count}] = {{", *_table_c(output, value, decls, ctx), "};"]
    return "\n".join(lines) + "\n"
