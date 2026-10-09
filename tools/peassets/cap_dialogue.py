#!/usr/bin/env python3
"""What a room's dialogue payload does: its commands and the records each plays.

    cap_dialogue.py pe2cap2_7                 every command of every dialogue in the bundle
    cap_dialogue.py pe2cap2_7 --command 11    one command

A `CAP2` payload (doc/CAP_FORMAT.md) is a small state machine. Code runs a
command by index; the command's opcode picks a variant key - always 0, a
counter, a game flag, a tally of object states, or it hands the choice to the
room - and playback then walks the command's records whose key matches. A
record is a line of text, or, with the view-control flag, a scene step: it
selects a view and may send a message to the player, the companion or a
placed actor. Records also start room sounds and run flag or item actions.

Text is not characters: each code is a glyph cell of the payload's own table,
drawn from the room's glyph image, so a line is reported by its length. A text
stream can also carry view codes, which change the view as the line is drawn;
those are listed with the line.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cap_bundle  # noqa: E402

OPCODES = ("plain", "counter", "flag", "room decides", "tally")
END = 0xFFFFFFFF
VIEW_CONTROL, DELAYED_MESSAGE, TITLE_BANK, FORCE_CARET, LEFT_ALIGN = 0x80, 0x08, 0x10, 0x04, 0x02


def dialogues(data: bytes) -> list[tuple[dict, bytes]]:
    return [(s, data[s["offset"]:s["offset"] + s["length"]]) for s in cap_bundle.slots(data) if s["what"] == "dialogue"]


def text_length(cap: bytes, offset: int) -> tuple[int, int, list[tuple[int, bool, bool]]]:
    """(glyphs, control codes, view codes) of the code stream at an offset.

    A text stream can change the view itself: a code of the 0x8000 family under
    mask 0x9F00 selects view `code & 0xFF` - a logical view, or with 0x2000 a
    direct index - and with 0x4000 hides the player and companion
    (src/gameplay/captions.c, `CAP_TEXT_CODE_VIEW`). Each view code is
    (view, direct, hides actors).
    """
    glyphs = controls = 0
    views = []
    while offset + 2 <= len(cap):
        (code,) = struct.unpack_from("<H", cap, offset)
        if code == 0xFFFF:
            break
        if code >= 0x8000:
            controls += 1
            if code & 0x9F00 == 0x8000:
                views.append((code & 0xFF, bool(code & 0x2000), bool(code & 0x4000)))
        else:
            glyphs += 1
        offset += 2
    return glyphs, controls, views


def commands(cap: bytes) -> list[dict]:
    """Each command the index names: its number, opcode and parameters, and its records."""
    if cap[:3] != b"CAP":
        return []
    _glyphs, _sequences, index = struct.unpack_from("<3I", cap, 8)
    (count,) = struct.unpack_from("<i", cap, index)
    out = []
    for number in range(count):
        (at,) = struct.unpack_from("<I", cap, index + 4 + number * 4)
        if not at or at + 12 > len(cap):
            continue
        opcode, flags, limit, flag_lo, _counter, bit_index, bit_count, flag_hi, following = struct.unpack_from("<9B", cap, at)
        command = {"number": number, "opcode": OPCODES[opcode] if opcode < len(OPCODES) else f"opcode {opcode}",
                   "wrap": bool(flags & 1), "persist": bool(flags & 2), "branch": bool(flags & 4), "raw_flags": flags,
                   "limit": limit, "flag": flag_lo | flag_hi << 8, "tally": (bit_index, bit_count), "next": following, "records": []}
        at += 12
        while at + 12 <= len(cap):
            b0, flags, b2, b3, b4, key, action, minimum, ref = struct.unpack_from("<8BI", cap, at)
            if ref == END:
                break
            record = {"key": key, "action": action, "flags": flags}
            if flags & VIEW_CONTROL:
                record.update(kind="scene", view=b0)
                if flags & DELAYED_MESSAGE:
                    record.update(message=b2, delay=b3, recipient="player" if b4 == 0 else "companion" if b4 == 1 else f"placed actor {b4 - 2}")
            else:
                glyphs, controls, views = text_length(cap, ref) if ref < len(cap) else (0, 0, [])
                record.update(kind="text", views=views, title=b0, show=b2, pause=b3, sound=b4 >> 1, instant=bool(b4 & 1), glyphs=glyphs, controls=controls, minimum=minimum)
            command["records"].append(record)
            at += 12
        out.append(command)
    return out


def describe(command: dict) -> list[str]:
    how = command["opcode"]
    if how == "counter":
        how += f" up to {command['limit']}" + (f", kept in game flag {command['flag']}" if command["persist"] else ", kept in the file")
        how += ", wraps" if command["wrap"] else ""
    elif how == "flag":
        how += f" {command['flag']}"
    elif how == "tally":
        how += f" of object states {command['tally'][0]}..{command['tally'][0] + command['tally'][1] - 1}"
    if command["branch"]:
        how += f"; branches to command {command['next']}"
    lines = [f"  command {command['number']}: variant by {how}"]
    for record in command["records"]:
        action = "" if not record["action"] else f", action {record['action']}" if record["action"] <= 100 else f", message to scene child {record['action'] - 100}"
        if record["kind"] == "scene":
            sent = f", sends {record['message']} to the {record['recipient']} after {record['delay']} frames" if "message" in record else ""
            lines.append(f"    key {record['key']}: scene step, view {record['view']}{sent}{action}")
        else:
            sound = f", room sound {record['sound']}" if record["sound"] else ""
            title = f", title {record['title'] - 1 + (256 if record['flags'] & TITLE_BANK else 0)}" if record["title"] else ""
            views = "".join(f", selects view {view}" + (" (direct)" if direct else "") + (", hiding the actors" if hide else "") for view, direct, hide in record["views"])
            lines.append(f"    key {record['key']}: text, {record['glyphs']} glyphs{title}{sound}{action}{views}")
    return lines


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    only = int(sys.argv[sys.argv.index("--command") + 1]) if "--command" in sys.argv else None
    if only is not None:
        args = [a for a in args if a != str(only)]
    if not args:
        print(__doc__)
        return 1
    for key in args:
        path = cap_bundle.RAW / f"{key}.pe2cap2"
        if not path.is_file():
            print(f"{key}: not in the local extraction")
            continue
        for slot, cap in dialogues(path.read_bytes()):
            found = commands(cap)
            print(f"{key} slot {slot['slot']}: {len(found)} commands")
            for command in found:
                if only is None or command["number"] == only:
                    print("\n".join(describe(command)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
