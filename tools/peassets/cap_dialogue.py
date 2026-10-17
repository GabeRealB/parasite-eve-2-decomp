#!/usr/bin/env python3
"""What a room's dialogue payload does: its commands and the records each plays.

    cap_dialogue.py rooms/akropolis/square/bundle               every command of every dialogue in the bundle
    cap_dialogue.py rooms/akropolis/square/dialogue_0 --command 4   one command of one dialogue

A `CAP2` payload (doc/CAP_FORMAT.md) is a small state machine. Code runs a
command by index; the command's opcode picks a variant key - always 0, a
counter, a game flag, a tally of object states, or it hands the choice to the
room - and playback then walks the command's records whose key matches. A
record is a line of text, or, with the view-control flag, a scene step: it
selects a view - by its view-file number, which the room's view map turns
into the logical view, unless a script has switched scene steps to logical
ids (opcode 41, `SET_CAP_DIRECT_VIEW_IDS`) - and may send a message to the player, the companion or a
placed actor. Records also start room sounds and run flag or item actions.

A line is shown as its text (tools/peassets/cap_text.py), with glyphs the
glyph table does not know yet as their hashes. A text
stream can also carry view codes, which change the view as the line is drawn;
those are listed with the line, as are the choices a line offers: each choice
names the event key its reply selects.
"""
from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cap_text  # noqa: E402

OPCODES = ("plain", "counter", "flag", "room decides", "tally")
END = 0xFFFFFFFF
VIEW_CONTROL, DELAYED_MESSAGE, TITLE_BANK, FORCE_CARET, LEFT_ALIGN = 0x80, 0x08, 0x10, 0x04, 0x02


# A text stream can end a line in a choice: the code's low byte is the event key the reply selects
# (src/gameplay/cap.h, `CAP_TEXT_CHOICE_*`; src/gameplay/captions.c, the choice branch of the text drawer).
CHOICES = {0x8100: "confirm", 0x8200: "silent", 0x8300: "cursor"}
TEXT_CHOICES: dict[tuple[int, int], list[tuple[str, int]]] = {}


def offset_key(cap: bytes, offset: int) -> tuple[int, int]:
    return id(cap), offset


def text_length(cap: bytes, offset: int) -> tuple[int, int, list[tuple[int, bool, bool]]]:
    """(glyphs, control codes, view codes) of the code stream at an offset.

    A text stream can change the view itself: a code of the 0x8000 family under
    mask 0x9F00 selects view `code & 0xFF`. With 0x2000 the value is the
    logical view; without it the value is a view-file number, turned into the
    logical view by the room's view map (`viewFindLogicalIndex`). 0x4000 hides
    the player and companion (src/gameplay/captions.c, `CAP_TEXT_CODE_VIEW`).
    Each view code is (value, logical, hides actors).
    """
    glyphs = controls = 0
    views = []
    choices = TEXT_CHOICES.setdefault(offset_key(cap, offset), [])
    del choices[:]
    while offset + 2 <= len(cap):
        (code,) = struct.unpack_from("<H", cap, offset)
        if code == 0xFFFF:
            break
        if code >= 0x8000:
            controls += 1
            if code & 0x9F00 == 0x8000:
                views.append((code & 0xFF, bool(code & 0x2000), bool(code & 0x4000)))
            elif code & 0xFF00 in CHOICES:
                choices.append((CHOICES[code & 0xFF00], code & 0xFF))
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
            record = {"key": key, "action": action, "flags": flags, "ref": ref}
            if flags & VIEW_CONTROL:
                record.update(kind="scene", view=b0)
                if flags & DELAYED_MESSAGE:
                    record.update(message=b2, delay=b3, recipient="player" if b4 == 0 else "companion" if b4 == 1 else f"placed actor {b4 - 2}")
            else:
                glyphs, controls, views = text_length(cap, ref) if ref < len(cap) else (0, 0, [])
                record.update(kind="text", views=views, choices=list(TEXT_CHOICES.get(offset_key(cap, ref), [])), title=b0, show=b2, pause=b3, sound=b4 >> 1, instant=bool(b4 & 1), glyphs=glyphs, controls=controls, minimum=minimum)
            command["records"].append(record)
            at += 12
        out.append(command)
    return out


def describe(command: dict, payload=None) -> list[str]:
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
        action = "" if not record["action"] else f", placed object {record['action']}" if record["action"] <= 100 else f", message to scene child {record['action'] - 100}"
        if record["kind"] == "scene":
            sent = f", sends {record['message']} to the {record['recipient']} after {record['delay']} frames" if "message" in record else ""
            # A scene step names a view FILE, which the room's view map turns into the logical view; it names the
            # logical view itself only while a script has set direct view ids (event-script opcode 41).
            view = f"view file {record['view']}" if record["view"] else "keeps the view"
            # Flag 0x40 holds the step until the script's next WAIT_CAP_CUE (src/gameplay/captions.c:315-330).
            cue = ", waits for the script's cue" if record["flags"] & 0x40 else ""
            lines.append(f"    key {record['key']}: scene step, {view}{cue}{sent}{action}")
        else:
            sound = f", room sound {record['sound']}" if record["sound"] else ""
            # With an action, byte 0 is the action's fallback key, not a title.
            title = "" if not record["title"] else f", fallback key {record['title']}" if record["action"] else f", title {record['title'] - 1 + (256 if record['flags'] & TITLE_BANK else 0)}"
            views = "".join(f", selects {'logical view' if direct else 'view file'} {view}" + (", hiding the actors" if hide else "") for view, direct, hide in record["views"])
            choices = (", choices " + ", ".join(f"key {key}" + ("" if kind == "confirm" else f" ({kind})") for kind, key in record["choices"])) if record["choices"] else ""
            words = f"{record['glyphs']} glyphs" if record["glyphs"] else "none"
            if payload is not None and record["glyphs"]:
                spoken = re.sub(r"\{view [^}]*\}", "", payload.text(cap_text.codes_at(payload.cap, record["ref"]))).rstrip("\n")
                words = '"' + spoken.replace("\n", " / ") + '"'
            lines.append(f"    key {record['key']}: text {words}{title}{sound}{action}{views}{choices}")
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
        try:
            texts = dict(cap_text.payloads(key))
        except SystemExit:
            texts = {}
        for name in cap_text.dialogues(key):
            found = commands(cap_text.cap_bytes(name))
            print(f"{name}: {len(found)} commands")
            for command in found:
                if only is None or command["number"] == only:
                    print("\n".join(describe(command, texts.get(name))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
