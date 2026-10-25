#!/usr/bin/env python3
"""A full, reversible decode of a CAP dialogue (doc/CAP_FORMAT.md).

    cap_decode.py deps rooms/akropolis/square/bundle       what a full decode of each of its dialogues needs
    cap_decode.py decode rooms/akropolis/square/bundle     write <dialogue>.json for each, in its group or cap/
    cap_decode.py decode akropolis_square_dialogue_1 one dialogue
    cap_decode.py encode assets/USA/cap/akropolis_square_dialogue_0.json   rebuild it and its atlas, compare

A dialogue is an asset of its own (`.cap`), one of the resources of a bundle
(bundle.py). Its stored bytes stay as extracted; `decode` writes its decoded
form, and beside it `<dialogue>.warnings.json`, a map by cell of what
recognition could not settle: a glyph or speaker plate no table names yet (kept
in the font under an `unseen` placeholder), a picture several characters share
(`cap_glyphs.tsv` lists them on one line; the dialogue's `glyphs` overrides in
asset_data.py choose, as `{cell: "I"}`, or the decoder guesses), and two cells
that came out as one character. Cells an override settles are not listed: the
map is what is left to fix, and a complete dialogue has none. `deps` lists the
same gaps before decoding.

The decode is lossless and holds nothing that can be computed. A payload is
its commands - a list whose positions are the command numbers, None where a
number has no command - each with its variant rule and its records; every
byte of a command or record has a name (`command_named`, `record_named`), and
zero bytes and false flags are left out. A record's text is written as
characters (cap_font.py's markup), its speaker by name. Offsets, counts, the
command index, terminators, the magic and the cell table are not kept: the
encoder lays them out again, numbers the cells from the text, packs the atlas
from the dialogue font (assets/USA/dialogue/font.ufo, speakers.ufo). `decode` writes nothing unless
that rebuild equals the stored dialogue and its extracted atlas exactly. The only flags the layout cannot imply are
`trailing_empty_text` (an unused empty stream after the last record, in about
half the retail payloads).

`deps` also lists what a payload refers to outside itself - placed objects,
views, room sounds, room commands, game flags, object states - which the room's
inspection explains; those do not block a decode.
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cap_font  # noqa: E402
import cap_text  # noqa: E402

PLATES = Path(__file__).resolve().parent / "cap_plates.tsv"
OUT = cap_text.ASSETS / "cap"
SLOT = 12
END_REF = 0xFFFFFFFF
OPCODES = {0: "plain", 1: "counter", 2: "flag", 3: "room", 4: "tally"}
VIEW_CONTROL, DELAYED_MESSAGE, TITLE_BANK, SCENE_CONTROL, SCENE_PHASE = 0x80, 0x08, 0x10, 0x20, 0x40
FORCE_CARET, LEFT_ALIGN = 0x04, 0x02
COMMAND_WRAP, COMMAND_PERSIST, COMMAND_BRANCH = 0x01, 0x02, 0x04
CHILD_ACTION_BASE = 100


def plate_table() -> dict[str, str]:
    out = {}
    for line in PLATES.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            key, name = line.split("\t")[:2]
            out[key] = name
    return out


def plate_numbers() -> dict[str, list[int]]:
    """Each plate's speaker numbers, as the plate table states them: fixed, whatever order dialogues decode in."""
    out = {}
    for line in PLATES.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            fields = line.split("\t")
            out[fields[1]] = [int(n) for n in fields[2].split(",")] if len(fields) > 2 and fields[2] else []
    return out


# ---- structure -----------------------------------------------------------------------------------------

def parse(cap: bytes) -> dict:
    """A payload as its parts, every byte accounted for, offsets kept raw for the check."""
    magic, field_4, glyphs, sequences, commands = struct.unpack_from("<4si3I", cap, 0)
    count, slot_size = struct.unpack_from("<hh", cap, sequences)
    (index_count,) = struct.unpack_from("<i", cap, commands)
    entries = list(struct.unpack_from(f"<{index_count}I", cap, commands + 4))
    # Slots: a command header, then records until a terminator, then the next command.
    slots, at, expect_command = [], sequences + 4, True
    while at + SLOT <= commands:
        raw = cap[at:at + SLOT]
        if expect_command:
            fields = struct.unpack("<9B3B", raw)
            slots.append({"at": at, "kind": "command", "fields": list(fields)})
            expect_command = False
        else:
            *bytes8, ref = struct.unpack("<8BI", raw)
            slots.append({"at": at, "kind": "record", "fields": list(bytes8), "ref": ref})
            if ref == END_REF:
                expect_command = True
        at += SLOT
    tail = cap[at:commands]
    # Text streams: the bytes between the glyph cells and the sequence table, each ending in 0xFFFF.
    refs = sorted({s["ref"] for s in slots if s["kind"] == "record" and s["ref"] != END_REF})
    text_start = min(refs) if refs else sequences
    cells = [list(struct.unpack_from("<4B", cap, o)) for o in range(glyphs, text_start, 4)]
    texts, at = [], text_start
    while at < sequences:
        codes = cap_text.codes_at(cap, at)
        texts.append({"at": at, "codes": codes})
        at += 2 * len(codes) + 2
    return {"magic": magic.decode("latin-1"), "field_4": field_4, "offsets": [glyphs, sequences, commands],
            "cells": cells, "texts": texts, "sequence_count": count, "slot_size": slot_size, "slots": slots,
            "slot_tail": tail.hex(), "index": entries, "length": len(cap), "trailing": cap[commands + 4 + 4 * index_count:].hex()}


def encode_payload(p: dict) -> bytes:
    """Lay a decoded payload out again; offsets are recomputed from the parts."""
    glyphs = 0x14
    text_at = glyphs + 4 * len(p["cells"])
    text_offsets, body = [], b""
    for t in p["texts"]:
        text_offsets.append(text_at + len(body))
        body += struct.pack(f"<{len(t['codes'])}H", *t["codes"]) + b"\xff\xff"
    sequences = text_at + len(body)
    slots_bytes = b""
    slot_offsets = []
    for s in p["slots"]:
        slot_offsets.append(sequences + 4 + len(slots_bytes))
        if s["kind"] == "command":
            slots_bytes += struct.pack("<9B3B", *s["fields"])
        else:
            ref = END_REF if s["text"] is None and s.get("ref_raw") is None else (text_offsets[s["text"]] if s["text"] is not None else s["ref_raw"])
            slots_bytes += struct.pack("<8BI", *s["fields"], ref)
    slots_bytes += bytes.fromhex(p["slot_tail"])
    commands = sequences + 4 + len(slots_bytes)
    index = [0 if e is None else slot_offsets[e] for e in p["index"]]
    out = struct.pack("<4si3I", p["magic"].encode("latin-1"), p["field_4"], glyphs, sequences, commands)
    out += b"".join(struct.pack("<4B", *c) for c in p["cells"]) + body
    out += struct.pack("<hh", p["sequence_count"], p["slot_size"]) + slots_bytes
    out += struct.pack(f"<i{len(index)}I", len(index), *index) + bytes.fromhex(p["trailing"])
    return out


def to_references(parsed: dict) -> dict:
    """Replace raw offsets by references to parts, so `encode_payload` can rebuild them."""
    text_index = {t["at"]: n for n, t in enumerate(parsed["texts"])}
    slot_index = {s["at"]: n for n, s in enumerate(parsed["slots"])}
    for s in parsed["slots"]:
        if s["kind"] == "record":
            ref = s.pop("ref")
            s["text"] = text_index.get(ref)
            if ref != END_REF and s["text"] is None:
                s["ref_raw"] = ref
        del s["at"]
    parsed["index"] = [None if e == 0 else slot_index[e] for e in parsed["index"]]
    for t in parsed["texts"]:
        del t["at"]
    del parsed["offsets"], parsed["length"]
    return parsed


# ---- structure <-> named fields ---------------------------------------------------------------------------
#
# Every byte of a command or record has a name; zero bytes and false flags are left out, and what the layout
# implies (offsets, counts, the command index, terminators) is not written at all.

COMMAND_FIELDS = ("opcode", "flags", "limit", "flag_lo", "counter", "tally_first", "tally_count", "flag_hi", "next", "tail_0", "tail_1", "tail_2")
OPCODE_NAMES = {v: k for k, v in OPCODES.items()}
RECORD_FLAGS = (("bit0", 0x01), ("left_aligned", LEFT_ALIGN), ("caret", FORCE_CARET), ("send_message", DELAYED_MESSAGE),
                ("title_bank", TITLE_BANK), ("scene_sync", SCENE_CONTROL), ("wait_for_cue", SCENE_PHASE))
COMMAND_FLAGS = (("wraps", COMMAND_WRAP), ("persists", COMMAND_PERSIST), ("branches", COMMAND_BRANCH))


def recipient_name(value: int) -> str:
    return "player" if value == 0 else "companion" if value == 1 else f"actor {value - 2}"


def recipient_value(name: str) -> int:
    return {"player": 0, "companion": 1}.get(name) if name in ("player", "companion") else int(name.split()[1]) + 2


def speaker_name(number: int, font: cap_font.Font) -> str | int:
    """A speaker number by its plate: the plate the dialogue shows for it (`font.chosen`, two plates can stand for
    one number), `<plate>#<number>` where the plate stands for several numbers."""
    if getattr(font, "titles_are_cells", False):       # the other layout: a title is the glyph cell it shows
        return number
    token = getattr(font, "chosen", {}).get(number) or font.speaker(number)
    if token is None:
        return number
    return token if plate_numbers().get(token, [number]) == [number] else f"{token}#{number}"


def speaker_number(name: str | int, font: cap_font.Font) -> int:
    if isinstance(name, int):
        return name
    if "#" in name:
        return int(name.rsplit("#", 1)[1])
    return (plate_numbers().get(name) or font.index["speakers"][name]["numbers"])[0]


def speaker_token(name: str | int) -> str | None:
    return None if isinstance(name, int) else name.rsplit("#", 1)[0] if "#" in name else name


# What each opcode reads (CapCommand, include/gameplay/cap.h). Bytes an opcode does not read are left over from
# the tool that wrote the file - a counter's unread `tally_count` is 1 in 600 of 601 - and are kept apart, under
# `unread`, written only where they differ from that opcode's usual value.
COMMAND_READS = {
    "plain": set(),
    "counter": {"wraps", "persists", "branches", "limit", "game_flag", "counter", "next"},
    "flag": {"game_flag"},
    "room": set(),
    "tally": {"branches", "tally_first", "tally_count", "next"},
}
COMMAND_VALUES = ("wraps", "persists", "branches", "flag_bits", "game_flag", "limit", "counter", "tally_first",
                  "tally_count", "next", "tail_0", "tail_1", "tail_2")


def command_defaults(opcode: str, highest_key: int) -> dict:
    """The value each command field takes unless the file says otherwise. The tool that wrote the files set `limit`
    to the highest key of the command's records whatever the opcode (1328 of 1333 commands; a counter plays keys
    0..limit). A tally of N flags plays a key 0..N, so its `tally_count` is the highest key too (69 of 72); the tool
    left an unread `tally_count` of 1 in counter, flag and room commands, and 0 in plain ones, with no rule for
    the exceptions."""
    out = {name: 0 for name in COMMAND_VALUES} | {name: False for name, _bit in COMMAND_FLAGS}
    out["limit"] = highest_key
    if opcode == "tally":
        out["tally_count"] = highest_key
    if opcode in ("counter", "flag", "room"):
        out["tally_count"] = 1
    return out


def command_values(fields: list[int]) -> dict:
    raw = dict(zip(COMMAND_FIELDS, fields))
    out = {name: bool(raw["flags"] & bit) for name, bit in COMMAND_FLAGS}
    out["flag_bits"] = raw["flags"] & ~0x07
    out["game_flag"] = raw["flag_lo"] | raw["flag_hi"] << 8
    out.update({name: raw[name] for name in ("limit", "counter", "tally_first", "tally_count", "next", "tail_0", "tail_1", "tail_2")})
    return out


def command_named(fields: list[int], highest_key: int) -> dict:
    opcode = OPCODES.get(fields[0], fields[0])
    values, defaults = command_values(fields), command_defaults(opcode, highest_key)
    reads = COMMAND_READS.get(opcode, set())
    out, unread = {"opcode": opcode}, {}
    for name in COMMAND_VALUES:
        if values[name] != defaults[name]:
            value = f"{values[name]:#05x}" if name == "game_flag" else values[name]
            (out if name in reads else unread)[name] = value
    if unread:
        out["unread"] = unread
    return out


def command_fields(named: dict, highest_key: int) -> list[int]:
    opcode = named["opcode"]
    values = command_defaults(opcode, highest_key) | {k: v for k, v in named.items() if k in COMMAND_VALUES} | named.get("unread", {})
    if isinstance(values["game_flag"], str):
        values["game_flag"] = int(values["game_flag"], 16)
    raw = {name: values.get(name, 0) for name in COMMAND_FIELDS}
    raw["opcode"] = OPCODE_NAMES.get(opcode, opcode)
    raw["flags"] = sum(bit for name, bit in COMMAND_FLAGS if values[name]) | values["flag_bits"]
    raw["flag_lo"], raw["flag_hi"] = values["game_flag"] & 0xFF, values["game_flag"] >> 8
    return [raw[name] for name in COMMAND_FIELDS]


# What playback reads of each record type (capUpdatePlaybackTask, captions.c). A scene step (VIEW_CONTROL) changes
# the view, syncs with the caption control task (`scene_sync`), waits for the event script's cue (`wait_for_cue`) and
# can send a face message; it then moves on in the same tick, so its own text is never drawn, and the sound, instant
# and minimum-frame bytes that capApplyRecordPlaybackSettings loads for it are replaced by the next record's before
# they act. The title bank bit is passed to both caption renderers and ignored by both. What a type does not read is
# kept under `unread`, where it differs from the usual value: the tool wrote a message recipient of 1 into scene
# steps that send no message (2089 of 2111).
RECORD_READS = {
    "line": {"speaker", "fallback_key", "text", "show_frames", "pause_frames", "room_sound", "instant",
             "minimum_frames", "action", "left_aligned", "caret"},
    "scene": {"view", "scene_sync", "wait_for_cue", "send_message", "message", "message_delay", "message_to"},
}
MESSAGE_FIELDS = ("message", "message_delay", "message_to")


def record_values(fields: list[int], text: str, font: cap_font.Font) -> dict:
    """Every byte of a record by name, as the record's type reads it."""
    b0, flags, b2, b3, b4, key, action, minimum = fields
    scene = bool(flags & VIEW_CONTROL)
    out = {name: bool(flags & bit) for name, bit in RECORD_FLAGS}
    out.update(text=text, minimum_frames=minimum,
               action=({"placed_object": action} if action <= CHILD_ACTION_BASE else {"scene_child": action - CHILD_ACTION_BASE}) if action else None)
    if scene:
        out.update(view=b0, message=b2, message_delay=b3, message_to=recipient_name(b4))
    else:
        out.update(show_frames=b2, pause_frames=b3, room_sound=b4 >> 1, instant=bool(b4 & 1))
        # One byte, read as the fallback key when the line has an action and as the speaker otherwise.
        out.update(fallback_key=b0 if action else 0, speaker=speaker_name(b0 - 1, font) if b0 and not action else None)
    return out


def record_defaults(kind: str, sends: bool) -> dict:
    out = {name: False for name, _bit in RECORD_FLAGS}
    out.update(text="", minimum_frames=0, action=None)
    if kind == "scene":
        out.update(view=0, message=0, message_delay=0, message_to="player" if sends else "companion")
    else:
        out.update(show_frames=0, pause_frames=0, room_sound=0, instant=False, fallback_key=0, speaker=None)
    return out


def record_named(fields: list[int], text: str, font: cap_font.Font) -> dict:
    kind = "scene" if fields[1] & VIEW_CONTROL else "line"
    values = record_values(fields, text, font)
    defaults = record_defaults(kind, values["send_message"])
    reads = set(RECORD_READS[kind])
    if kind == "scene" and not values["send_message"]:
        reads -= set(MESSAGE_FIELDS)
    out, unread = {"key": fields[5], "type": kind}, {}
    for name, value in values.items():
        if value != defaults[name]:
            (out if name in reads else unread)[name] = value
    if unread:
        out["unread"] = unread
    return out


def record_fields(named: dict, font: cap_font.Font) -> list[int]:
    kind = named["type"]
    plain = {k: v for k, v in named.items() if k not in ("key", "type", "unread")}
    sends = plain.get("send_message", named.get("unread", {}).get("send_message", False))
    values = record_defaults(kind, sends) | plain | named.get("unread", {})
    flags = (VIEW_CONTROL if kind == "scene" else 0) | sum(bit for name, bit in RECORD_FLAGS if values[name])
    action = values["action"] or {}
    action = action.get("placed_object", 0) or (action["scene_child"] + CHILD_ACTION_BASE if "scene_child" in action else 0)
    if kind == "scene":
        b0, b2, b3, b4 = values["view"], values["message"], values["message_delay"], recipient_value(values["message_to"])
    else:
        b0 = values["fallback_key"] if action else (speaker_number(values["speaker"], font) + 1 if values["speaker"] is not None else 0)
        b2, b3 = values["show_frames"], values["pause_frames"]
        b4 = values["room_sound"] << 1 | (1 if values["instant"] else 0)
    return [b0, flags, b2, b3, b4, named["key"], action, values["minimum_frames"]]


def to_named(parsed: dict, font: cap_font.Font) -> dict:
    """A parsed payload (texts as markup, offsets as references) as its commands, each holding its records.

    A command's number is its position in the list, None where no command has that number. Entry 0 is None in
    every retail dialogue, and the list ends at the last command; an empty dialogue has an empty list. The
    commands are stored in number order in every retail dialogue, so the list is also their order in the file."""
    numbers = {slot: number for number, slot in enumerate(parsed["index"]) if slot is not None}
    if list(numbers.values()) != sorted(numbers.values()) or list(numbers) != sorted(numbers):
        raise SystemExit("the commands are not stored in number order")  # never in retail
    if len(parsed["index"]) != max(numbers.values(), default=0) + 1:
        raise SystemExit("the command index does not end at the last command")  # never in retail
    commands: list[dict | None] = [None] * (max(numbers.values()) + 1 if numbers else 0)
    current = None
    for n, s in enumerate(parsed["slots"]):
        if s["kind"] == "command":
            current = {"fields": s["fields"], "records": []}
            commands[numbers[n]] = current
        elif s["text"] is not None:
            current["records"].append(record_named(s["fields"], parsed["texts"][s["text"]], font))
    for number, command in enumerate(commands):
        if command is not None:
            highest = max((r["key"] for r in command["records"]), default=0)
            commands[number] = {**command_named(command.pop("fields"), highest), "records": command["records"]}
    out = {"commands": commands}
    if parsed["texts"] and parsed["texts"][-1] == "" and len(parsed["texts"]) > sum(len(c["records"]) for c in commands if c):
        out["trailing_empty_text"] = True
    return out


def from_named(cap: dict, font: cap_font.Font) -> dict:
    """The parsed form `build` lays out, from the named one."""
    font.titles_are_cells = "first_glyph" in cap.get("atlas", {})
    # The index has an entry per position of the command list, and at least entry 0.
    slots, texts, index = [], [], [None] * max(len(cap["commands"]), 1)
    for number, command in enumerate(cap["commands"]):
        if command is None:
            continue
        index[number] = len(slots)
        slots.append({"kind": "command", "fields": command_fields(command, max((r["key"] for r in command["records"]), default=0))})
        for record in command["records"]:
            slots.append({"kind": "record", "fields": record_fields(record, font), "text": len(texts)})
            texts.append(record.get("text", record.get("unread", {}).get("text", "")))
        slots.append({"kind": "record", "fields": [0] * 8, "text": None})
    if cap.get("trailing_empty_text"):
        texts.append("")
    speakers = {speaker_number(r["speaker"], font): speaker_token(r["speaker"])
                for command in cap["commands"] if command for r in command["records"]
                if r.get("speaker") is not None and speaker_token(r["speaker"])}
    return {"magic": "CAP2", "field_4": 8, "sequence_count": sum(s["kind"] == "record" for s in slots), "slot_size": SLOT,
            "slots": slots, "slot_tail": "", "index": index, "trailing": "", "texts": texts, "speakers": speakers,
            "atlas": cap.get("atlas", {})}


def payload_plate(payload: "cap_text.Payload", index: int) -> str | None:
    img = cap_text.open_atlas(payload.atlas, payload.column) if payload.atlas else None
    return cap_text.glyph_hash(img, payload.cell(index)) if img else None


# ---- dependencies ----------------------------------------------------------------------------------------

def dependencies(key: str) -> list[dict]:
    """What a full decode of each dialogue a key names still needs, and what each refers to in its room."""
    plates = plate_table()
    out = []
    for name, payload in cap_text.payloads(key):
        parsed = parse(payload.cap)
        missing_glyphs = sorted({h for i, h in payload.hashes.items() if payload.chars.get(i) is None and h and h != "space"})
        records = [s for s in parsed["slots"] if s["kind"] == "record" and s["ref"] != END_REF]
        commands = [s for s in parsed["slots"] if s["kind"] == "command"]
        titles = sorted({s["fields"][0] - 1 + (256 if s["fields"][1] & TITLE_BANK else 0) for s in records
                         if s["fields"][0] and not s["fields"][1] & VIEW_CONTROL})
        plate_hashes = {t: payload_plate(payload, t) for t in titles}
        missing_plates = sorted({h for h in plate_hashes.values() if h and h != "space" and h not in plates})
        views = sorted({s["fields"][0] for s in records if s["fields"][1] & VIEW_CONTROL and s["fields"][0]}
                       | {c & 0xFF for t in parsed["texts"] for c in t["codes"] if c & 0x9F00 == 0x8000 and not c & 0x2000})
        flags = sorted({(c["fields"][3] | c["fields"][7] << 8) for c in commands
                        if c["fields"][0] == 2 or (c["fields"][0] == 1 and c["fields"][1] & COMMAND_PERSIST)})
        rebuilt = encode_payload(to_references(parse(payload.cap)))
        out.append({
            "name": name,
            "atlas": payload.atlas.relative_to(cap_text.ASSETS).as_posix() if payload.atlas else None,
            "missing": {"glyphs": missing_glyphs, "plates": missing_plates,
                        "overrides": [f"cell {n['cell']} ({' or '.join(map(repr, n['candidates']))}, guessed {n['guessed']!r})"
                                      for n in payload.notes if n["kind"] == "ambiguous"],
                        **({"atlas": True} if payload.atlas is None else {}),
                        **({"round_trip": True} if rebuilt != payload.cap else {})},
            "refers_to": {
                "placed_objects": sorted({s["fields"][6] for s in records if 0 < s["fields"][6] <= CHILD_ACTION_BASE}),
                "scene_children": sorted({s["fields"][6] - CHILD_ACTION_BASE for s in records if s["fields"][6] > CHILD_ACTION_BASE}),
                "view_files": views,
                "room_sounds": sorted({s["fields"][4] >> 1 for s in records if not s["fields"][1] & VIEW_CONTROL and s["fields"][4] >> 1}),
                "room_commands": [n for n, e in enumerate(parsed["index"]) if e and payload.cap[e] == 3],
                "game_flags": [f"{f:#05x}" for f in flags],
                "object_states": sorted({c["fields"][5] + i for c in commands if c["fields"][0] == 4 for i in range(c["fields"][6])}),
                "face_messages_to": sorted({("player" if s["fields"][4] == 0 else "companion" if s["fields"][4] == 1 else f"placed actor {s['fields'][4] - 2}")
                                            for s in records if s["fields"][1] & VIEW_CONTROL and s["fields"][1] & DELAYED_MESSAGE}),
            },
        })
    return out


def complete(deps: list[dict]) -> bool:
    return all(not any(d["missing"].values()) for d in deps)


# ---- decode / encode ------------------------------------------------------------------------------------

def is_title(fields: list[int]) -> bool:
    """A text record naming a speaker plate (byte 0 is a fallback key when there is an action)."""
    return bool(fields[0]) and not fields[1] & VIEW_CONTROL and not fields[6]


def first_titles(cap: dict) -> dict[int, int | None]:
    """Text stream -> the speaker number of the first record showing it, where that record has one."""
    out = {}
    for s in cap["slots"]:
        if s["kind"] == "record" and s["text"] is not None and s["text"] not in out:
            out[s["text"]] = s["fields"][0] - 1 if is_title(s["fields"]) else None
    return out


def build(cap: dict, font: cap_font.Font) -> tuple[bytes, Image.Image]:
    """A payload and its atlas from the decoded script and the font.

    Glyph cells are numbered from 25 as the text first uses each character; a
    speaker's cell is its number. The atlas places them in that first-use order,
    a speaker after the glyphs of the first line shown with it. What the rule does
    not imply is in `atlas`: `placed`, cells the atlas holds somewhere else; `cast`,
    the cells of speakers no record shows ([x, y, w, h] as stored: the tool laid out
    a plate for every speaker of a cast its own way, CAP_FORMAT.md §12); and
    `drawn`, those of them whose plate is on the atlas, by plate.

    The other layout tool (CAP_FORMAT.md §9) numbers glyph cells from `first_glyph`, draws them in its own `face`,
    has no speaker plates - a title is the glyph cell it shows - and leaves `marks` on the atlas, glyphs of its face
    no cell reaches, by position."""
    extra = cap.get("atlas", {})
    face = extra.get("face", "glyphs")
    first_glyph = extra.get("first_glyph", cap_font.FIRST_GLYPH_CELL)
    cell_titles = "first_glyph" in extra
    titles = {} if cell_titles else first_titles(cap)
    cells: dict[int, tuple[str, str]] = {}
    glyph_cell: dict[str, int] = {}
    order: list[tuple[str, str]] = []
    texts = []
    for n, text in enumerate(cap["texts"]):
        codes = []
        for item in cap_font.parse_markup(text):
            if item[0] == "code":
                codes.append(item[1])
                continue
            kind, token, palette, delay = item
            if kind == "glyph":
                if token not in glyph_cell:
                    glyph_cell[token] = first_glyph + len(glyph_cell)
                    cells[glyph_cell[token]] = (face, token)
                    order.append((face, token))
                cell = glyph_cell[token]
            else:
                cell = token
            codes.append(cell | palette << cap_font.PALETTE_SHIFT | delay << cap_font.DELAY_SHIFT)
        texts.append({"codes": codes})
        number = titles.get(n)
        speaker = (cap.get("speakers", {}).get(number) or font.speaker(number)) if number is not None else None
        if speaker is not None and number not in cells:
            cells[number] = ("speakers", speaker)
            order.append(("speakers", speaker))
    moved = {int(k): tuple(v) for k, v in extra.get("placed", {}).items()}
    ruled = {(kind, token): (x, y) for kind, token, x, y in cap_font.layout(order, font)}
    # By cell number: one plate can stand for two speakers of a dialogue, each with a cell of its own.
    at = {number: moved.get(number, ruled[item]) for number, item in cells.items()}
    cast = {int(k): v for k, v in extra.get("cast", {}).items()}
    table = [[0, 0, 0, 0] for _ in range(max(max([*cells, *cast]) + 1 if cells or cast else 0, extra.get("cells", 0)))]
    if "stored_cells" in extra:             # slices of an image the dialogue does not generate, as stored
        table = [list(c) for c in extra["stored_cells"]]
    for number, (kind, token) in cells.items():
        table[number] = [*at[number], *font.size(kind, token)]
    for number, rect in cast.items():
        table[number] = list(rect)
    script = {k: v for k, v in cap.items() if k not in ("texts", "speakers", "atlas")}
    payload = encode_payload({**script, "cells": table, "texts": texts})
    first = {}
    for number, item in sorted(cells.items(), key=lambda kv: order.index(kv[1])):
        first.setdefault(item, number)
    items = [(k, t, *at[first[(k, t)]]) for k, t in order]
    items += [(kind, token, *at[number]) for number, (kind, token) in cells.items() if first[(kind, token)] != number]
    drawn = [("speakers", token, *cast[int(number)][:2]) for number, token in extra.get("drawn", {}).items()]
    marks = [(face, token, x, y) for token, (x, y) in extra.get("marks", {}).items()]
    return payload, cap_font.draw_atlas(items + drawn + marks, font)




def own_atlas(name: str, payload: "cap_text.Payload") -> bool:
    """Whether the atlas a payload was read against is the one the dialogue generates (its font_atlas)."""
    mine = cap_text.atlas_of(name)
    return mine is not None and payload.atlas is not None and Path(payload.atlas).resolve() == mine.resolve()


def build_cells(cap: dict, font: cap_font.Font) -> tuple[bytes, Image.Image, list[list[int]]]:
    """`build`, with the cell table it lays out."""
    payload, atlas = build(cap, font)
    return payload, atlas, parse(payload)["cells"]


def decode(name: str, payload: "cap_text.Payload", font: cap_font.Font, warnings: dict) -> dict:
    """One dialogue asset in its named form; nothing is returned unless that form rebuilds it and its atlas."""
    plates = plate_table()
    parsed_cells = parse(payload.cap)
    draws = any(c < 0x8000 for t in parsed_cells["texts"] for c in t["codes"]) or any(
        s["kind"] == "record" and s["ref"] != END_REF and is_title(s["fields"]) for s in parsed_cells["slots"])
    if not draws:                                   # it draws no glyph and no plate: it has no atlas
        payload.atlas, payload.column = None, 0
        atlas = Image.new("L", (cap_font.ATLAS_SIZE, cap_font.ATLAS_SIZE), 0)
    elif payload.atlas is None:
        raise SystemExit(f"{name}: no glyph atlas: the asset table names none it generates, and its folder holds no "
                         "4-bit image (CAP_FORMAT.md §12); not written")
    else:
        atlas = cap_text.open_atlas(payload.atlas, payload.column)
    # Cells that slice an image the dialogue does not generate, across its pictures (Dryfield r04: a page laid out for
    # other text, CAP_FORMAT.md §12): the game draws exactly those slices, so they are kept as stored and the text
    # refers to them by number - no character can be read from them.
    stored_cells = payload.atlas is not None and not own_atlas(name, payload) and any(n["kind"] == "unseen" for n in payload.notes)
    parsed = to_references(parse(payload.cap))
    # Which tool laid it out: the house tool numbers glyph cells from 25, after the speaker plates; the other one
    # (CAP_FORMAT.md §9) from 0, in a bolder face, with no plates - its titles are the glyph cells they show.
    used = sorted({c & cap_font.CELL_MASK for t in parsed["texts"] for c in t["codes"] if c < 0x8000})
    layout = {"face": "bold", "first_glyph": used[0]} if used and used[0] < cap_font.FIRST_GLYPH_CELL else {}
    face = layout.get("face", "glyphs")
    first_glyph = layout.get("first_glyph", cap_font.FIRST_GLYPH_CELL)
    font.titles_are_cells = bool(layout)
    # The font: every nonempty cell, by the character or speaker it shows. A picture no table
    # names yet is kept under a placeholder and reported, so the decode still rebuilds.
    for note in payload.notes:
        warnings.setdefault(f"cell {note['cell']}", []).append({k: v for k, v in note.items() if k != "cell"})
    token_of, cells = {}, parsed["cells"]
    shown = {s["fields"][0] - 1 for s in parsed["slots"] if s["kind"] == "record" and s.get("text") is not None
             and is_title(s["fields"])}
    font.chosen, cast, drawn = {}, {}, {}
    for number, cell in enumerate(cells):
        if not cell[2] or not cell[3] or stored_cells:
            continue
        if number < first_glyph:
            speaker = plates.get(cap_text.glyph_hash(atlas, cell))
            if number not in shown:     # a speaker of the cast no record shows: its cell, and its plate if drawn
                cast[str(number)] = list(cell)
                if speaker is not None:
                    font.add("speakers", speaker, cap_font.crop(atlas, cell), number)
                    drawn[str(number)] = speaker
                continue
            if speaker is None:
                speaker = f"unseen speaker {number}"
                warnings.setdefault(f"cell {number}", []).append({"kind": "unseen speaker", "hash": cap_text.glyph_hash(atlas, cell)})
            font.add("speakers", speaker, cap_font.crop(atlas, cell), number)
            font.chosen[number] = speaker
        else:
            token_of[number] = payload.chars.get(number) or f"unseen {name} {number}"
            font.add(face, token_of[number], cap_font.crop(atlas, cell))
    parsed["texts"] = [cap_font.to_markup(t["codes"], token_of) for t in parsed["texts"]]
    parsed["speakers"] = dict(font.chosen)
    parsed["atlas"] = {**layout, **{k: v for k, v in (("cast", cast), ("drawn", drawn)) if v}}
    if stored_cells:
        parsed["atlas"]["stored_cells"] = [list(c) for c in cells]
    # What the atlas shows outside every cell: a mark of the face (the other tool's ▼), kept by its position.
    if layout and payload.atlas is not None and own_atlas(name, payload):
        bare = atlas.copy()
        for cell in cells:
            if cell[2] and cell[3]:
                bare.paste(0, (cell[0], cell[1], cell[0] + cell[2], cell[1] + cell[3]))
        box = bare.getbbox()
        if box:
            rect = (box[0], box[1], box[2] - box[0], box[3] - box[1])
            token = (cap_text.glyph_table().get(cap_text.glyph_hash(atlas, rect)) or [None])[0] or f"unseen {name} mark"
            font.add(face, token, cap_font.crop(atlas, rect))
            parsed["atlas"]["marks"] = {token: list(box[:2])}
    # Where the atlas holds a cell somewhere else than the rule puts it, say where.
    _payload, _atlas, ruled = build_cells({**parsed, "atlas": dict(layout)}, font) if not stored_cells else (None, None, cells)
    moved = {str(n): cells[n][:2] for n in range(len(cells)) if n < len(ruled) and n >= 0 and ruled[n] != cells[n]
             and str(n) not in cast and any(cells[n])}
    if moved:
        parsed["atlas"]["placed"] = moved
    if len(cells) > len(ruled):                  # a table longer than its cells need, empty at its end
        parsed["atlas"]["cells"] = len(cells)
    del parsed["cells"]
    rebuilt, rebuilt_atlas = build(parsed, font)
    if rebuilt != payload.cap:
        raise SystemExit(f"{name}: the decode does not rebuild the dialogue; not written")
    # A dialogue drawing on another's atlas (it generates none of its own) is checked on its cells' pixels, which
    # the font compares as they are added; only its own atlas has to rebuild whole.
    if payload.atlas is not None and own_atlas(name, payload) and rebuilt_atlas.tobytes() != atlas.tobytes():
        raise SystemExit(f"{name}: the decode does not rebuild the atlas {payload.atlas.name}; not written")
    named = to_named(parsed, font)
    if parsed["atlas"]:
        named["atlas"] = parsed["atlas"]
    if build(from_named(named, font), font)[0] != payload.cap:
        raise SystemExit(f"{name}: the named fields do not rebuild the dialogue; not written")
    return named


def decoded_dir(name: str) -> Path:
    """Where a dialogue's decode goes: its group's directory, else `cap/`. The extractor stores a dialogue raw only
    (asset_schema.group_of answers None for that), so this decode is the one form a group moves."""
    import asset_data
    return cap_text.ASSETS / (asset_data.ASSETS.get(name, {}).get("group") or "cap")


def encode(path: Path) -> int:
    """Rebuild a decoded dialogue and its atlas, and compare them with the extracted ones."""
    # A decode is named after its dialogue: in cap/ by the dialogue's name, in a group's directory by its local name.
    stem = path.name.removesuffix(".json")
    group = path.resolve().parent.relative_to(cap_text.ASSETS.resolve()).as_posix()
    name = stem if group == "cap" else f"{group}/{stem}"
    named = json.loads(path.read_text(encoding="utf-8"))
    payload, atlas = build(from_named(named, cap_font.Font()), cap_font.Font())
    same = payload == cap_text.cap_bytes(name)
    stored = cap_text.atlas_of(name)
    if stored is None:          # it draws on another dialogue's atlas, or draws nothing: only the dialogue rebuilds
        print(f"{name}: dialogue {'rebuilt' if same else 'DIFFERS'}, no atlas of its own")
        return 0 if same else 1
    same_atlas = atlas.tobytes() == cap_text.open_atlas(stored, cap_text.atlas_column(name)).tobytes()
    print(f"{name}: dialogue {'rebuilt' if same else 'DIFFERS'}, atlas {stored.stem} {'rebuilt' if same_atlas else 'DIFFERS'}")
    return 0 if same and same_atlas else 1


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 1
    action, target = argv[0], argv[1]
    if action == "deps":
        deps = dependencies(target)
        for d in deps:
            missing = {k: v for k, v in d["missing"].items() if v}
            print(f"{d['name']} ({d['atlas'] or 'no atlas'}): {'complete' if not missing else 'missing ' + ', '.join(f'{k} {len(v) if isinstance(v, list) else ""}'.strip() for k, v in missing.items())}")
            for k, v in missing.items():
                if isinstance(v, list):
                    for h in v:
                        print(f"    missing {k[:-1]} {h}")
            for k, v in d["refers_to"].items():
                if v:
                    print(f"    {k.replace('_', ' ')}: {', '.join(map(str, v))}")
        print("decodable" if complete(deps) else "not decodable yet")
        return 0 if complete(deps) else 2
    if action == "decode":
        return decode_dialogues(cap_text.payloads(target))
    if action == "encode":
        return encode(Path(target))
    print(__doc__)
    return 1


def decode_all() -> tuple[int, int]:
    """Decode every dialogue the asset table holds (the extraction's step): (written, not written)."""
    import asset_data
    names = sorted(k for k, r in asset_data.ASSETS.items() if r.get("type") == "cap")
    table = cap_text.glyph_table()
    payloads = [(n, cap_text.Payload(cap_text.cap_bytes(n), cap_text.folder_of(n), table, cap_text.overrides(n),
                                     cap_text.atlas_of(n), cap_text.atlas_column(n))) for n in names]
    failed = decode_dialogues(payloads, quiet=True)
    return len(names) - failed, failed


def decode_dialogues(payloads, quiet: bool = False) -> int:
    """Decode each (name, payload), writing its named form and adding its pictures to the font; how many failed."""
    status = 0
    font = cap_font.Font()
    for name, payload in payloads:
        warnings = {}
        where = decoded_dir(name)
        where.mkdir(parents=True, exist_ok=True)
        local = name.rsplit("/", 1)[-1]          # a grouped dialogue is named by its group's path
        report = where / f"{local}.warnings.json"
        kept = font.snapshot()
        try:
            named = decode(name, payload, font, warnings)
        except SystemExit as failure:
            print(failure)
            status += 1
            named = None
            font.restore(kept)                      # a failed decode adds nothing to the font
        if warnings:  # what recognition could not settle; it also explains most failed rebuilds
            report.write_text(json.dumps(warnings, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
            for cell, notes in warnings.items():
                for what in notes:
                    guess = f" ({' or '.join(map(repr, what['candidates']))}, guessed {what['guessed']!r})" if what["kind"] == "ambiguous" else ""
                    if not quiet:
                        print(f"warning: {name} {cell}: {what['kind']}{guess}")
        elif report.exists():
            report.unlink()
        if named is None:
            continue
        out = where / f"{local}.json"
        out.write_text(json.dumps(named, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        if not quiet:
            print(f"wrote {out.relative_to(cap_text.ROOT)}")
    font.save()
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
