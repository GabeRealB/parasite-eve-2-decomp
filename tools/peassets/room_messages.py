#!/usr/bin/env python3
"""Every message a room's event scripts send to a scene slot, with its payload.

    room_messages.py mist_parking [--to N]

A room's scripts drive its placed actors with `SEND_MESSAGE` commands addressed
to `GAME_TASK_SLOT_SCENE`: which clip to play, where to walk, which command to
run. The payload is usually a pointer to a small record defined elsewhere in
the room (`AnimationPlayRequest`, `ActorCommand`, ...). This prints one line
per command - the script and the command's index in it, with the file and line
the script is declared at, the recipient, the message, each record the payload
names with its fields, and the literal words - so a clip request can be tied to
a script command without reading the script row by row.

Everything printed is read from the room's package, no C initialiser is parsed
(the rule and the helpers are room_scene.py's). A script is an `EvsCommand`
array (include/gameplay/evs.h): six words a command, the opcode first. C is
read for two things only, both a declaration's type and name: which symbols are
`EvsCommand` arrays, and what type a payload record has. Each symbol's address
and extent come from the room's symbol map (its `size:`, else the next symbol),
and a script another script jumps to or calls is read from its target address
even where nothing declares it.

Not listed: a script that is neither declared as an `EvsCommand` array in the
room's C nor the target of a listed script, a payload built at run time and a
message sent from C code. A recipient or message is the number stored, not the
constant the source writes for it.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import room_scene  # noqa: E402
from room_scene import SVEC, arr, rec, sizeof  # noqa: E402

# include/gameplay/evs.h, EvsCommand: the opcode and five operand words.
EVS_COMMAND = rec(("opcode", "s32"), ("operands", arr("s32", 5)))
OPCODE_SEND_MESSAGE = 1             # EVENT_SCRIPT_OPCODE_SEND_MESSAGE: 0 task slot, 1 selector, 2 message id, 3/4 argument words
OPCODE_TARGETS = (43, 44, 46)       # JUMP, CALL_SCRIPT, SET_SKIP_TARGET: operand 0 is a command in a script
SLOT_SCENE = 4                      # GAME_TASK_SLOT_SCENE, include/main/session.h

# The payload records, as include/gameplay/message.h lays them out (each has a STATIC_ASSERT_SIZEOF there).
VECTOR = rec(("vx", "s32"), ("vy", "s32"), ("vz", "s32"), ("pad", "s32"))
RECORDS = {
    "AnimationPlayRequest": rec(("source", "pint"), ("animationId", "s32"), ("blend", "s32"), ("blendFrames", "s32"), ("enableWorldCollision", "s32")),
    "ActorCommand": rec(("stage", "u8"), ("area", "u8"), ("command", "u16")),
    "ActorMotionWalkAnim": rec(("animationId", "s32"), ("nextAnimId", "u8")),
    "ActorTransform": rec(("pos", VECTOR), ("rot", SVEC)),
}
assert [sizeof(RECORDS[t]) for t in ("AnimationPlayRequest", "ActorCommand", "ActorMotionWalkAnim", "ActorTransform")] == [0x14, 4, 8, 0x18]

RAW_BYTES = 0x20                    # how much of a payload object with no layout here is shown

SCRIPT = re.compile(r"\bEvsCommand\s+(\w+)\s*\[")
# A declaration of one symbol: qualifiers, the type's name, the symbol, its dimensions, then attributes or
# attribute macros (`SECTION(".data")`) before the `=` or `;`. Only the type's name is taken.
TYPE_OF = (r"^[ \t]*(?:(?:static|extern|const|volatile)\s+)*(?:struct\s+)?(\w+)\s+{}\s*(?:\[[^\]\n]*\]\s*)*"
           r"(?:(?:__attribute__\s*\(\(.*?\)\)|[A-Z][A-Z0-9_]*\s*\([^()\n]*\))\s*)*(?:=|;)")


def show(value) -> str:
    """A decoded record as text: `{field=value, ...}`, a vector as its components, a pointer in hex."""
    if isinstance(value, dict):
        if set(value) >= {"vx", "vy", "vz"}:
            return "(" + ", ".join(str(value[k]) for k in ("vx", "vy", "vz")) + ")"
        return "{" + ", ".join(f"{k}={show(v)}" for k, v in value.items()) + "}"
    if value is None:
        return "0"
    return f"0x{value:08X}" if room_scene.is_pointer(value) else str(value)


class Room:
    def __init__(self, room: str, project: room_scene.Project | None = None):
        self.p = project or room_scene.Project()
        if room not in self.p.place_of:
            raise SystemExit(f"unknown room {room!r}; rooms are the folders of tools/peassets/asset_data.py TREE")
        self.room = room
        self.scene = room_scene.Scene(self.p, room)
        self.problems: list[str] = []
        self._types: dict[str, str | None] = {}

    def type_of(self, symbol: str) -> str | None:
        """The declared type of a symbol of this room: its declaration's first word, nothing of its initialiser."""
        if symbol not in self._types:
            pattern = re.compile(TYPE_OF.format(re.escape(symbol)), re.M)
            found = None
            for text in self.p.code(self.room).values():
                m = pattern.search(text) if symbol in text else None
                if m:
                    found = m[1]
                    break
            self._types[symbol] = found
        return self._types[symbol]

    def scripts(self) -> list[tuple[int, list[dict], dict]]:
        """(address, commands, info) of every script: the declared `EvsCommand` arrays, then what they jump to."""
        symbols = self.p.maps.of(self.room)
        address_of = {name: address for address, name in symbols.names.items()}
        elsewhere = {name for named in self.p.maps.foreign.get(self.room, {}).values() for name, _owner in named}
        elsewhere |= {name for named in self.p.maps.foreign.get("rooms.imports", {}).values() for name, _owner in named}
        starts: list[int] = []
        for rel, text in self.p.code(self.room).items():
            for m in SCRIPT.finditer(text):
                if m[1] in address_of:
                    if address_of[m[1]] not in starts:
                        starts.append(address_of[m[1]])
                elif m[1] not in elsewhere:      # one the maps place in another image is an actor's script, not this room's
                    problem = f"{rel}: EvsCommand array {m[1]} is in no symbol map of the room; not read"
                    if problem not in self.problems:
                        self.problems.append(problem)
        out, covered, queue = [], [], sorted(starts)
        while queue:
            address = queue.pop(0)
            if any(a <= address < b for a, b in covered):
                continue
            rows, info = self.scene.table("scripts", address, EVS_COMMAND)
            if rows is None:
                continue
            covered.append((address, address + len(rows) * sizeof(EVS_COMMAND)))
            out.append((address, rows, info))
            for row in rows:
                target = row["operands"][0] & 0xFFFFFFFF
                if row["opcode"] in OPCODE_TARGETS and self.scene.owner_of(target) == self.room and target not in queue:
                    queue.append(target)
        for entry in self.scene.unresolved:
            if f"{entry['what']}: {entry['reason']}" not in self.problems:
                self.problems.append(f"{entry['what']}: {entry['reason']}")
        return sorted(out, key=lambda item: item[0])

    def payload(self, word: int) -> tuple[str | None, str | None]:
        """(the record a payload word points at, as `symbol=Type{fields}`; the literal) - one of the two is None.

        The type is the symbol's declared one; a type with no layout in `RECORDS` is shown as the object's bytes."""
        address = word & 0xFFFFFFFF
        owner = self.scene.owner_of(address) if room_scene.is_pointer(address) else None
        if owner is None or owner == "main":
            return None, str(word)
        text = self.scene.ref(address)
        symbol, base, _size = self.p.maps.find(owner, address)
        kind = self.type_of(symbol) if owner == self.room else None
        layout = RECORDS.get(kind)
        data = self.p.package(owner)
        off = address - self.p.load_of[owner]
        if layout is None or (address - base) % sizeof(layout) or off + sizeof(layout) > len(data):
            # No layout for its type: the object's own bytes, to the end of its symbol (at most RAW_BYTES of them).
            span, _how = self.p.extent(owner, address)
            raw = data[off:off + min(span or 0, RAW_BYTES)].hex(" ", 4)
            return f"{text}=({kind or '?'}) bytes {raw}" + (" ..." if (span or 0) > RAW_BYTES else ""), None
        if address != base:
            text = f"{symbol}[{(address - base) // sizeof(layout)}]"
        return f"{text}={kind}{show(room_scene.read_bytes(layout, data, off))}", None

    def messages(self):
        """(file, line, script, index, recipient, message, [payload records], [literal words]) for each scene message.

        `file` and `line` are where the script's symbol is declared, `index` the command's position in the symbol.
        """
        for _address, rows, info in self.scripts():
            where = info["where"]
            for i, row in enumerate(rows):
                slot, recipient, message, *words = row["operands"]
                if row["opcode"] != OPCODE_SEND_MESSAGE or slot != SLOT_SCENE:
                    continue
                found = [self.payload(word) for word in words]
                yield (where.get("file"), where.get("line"), info["symbol"], info["start"] + i, recipient, message,
                       [record for record, _literal in found if record], [literal for _record, literal in found if literal])


def messages(room: str, project: room_scene.Project | None = None):
    return Room(room, project).messages()


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    only = args[args.index("--to") + 1] if "--to" in args else None
    room = Room(args[0])
    count = 0
    for file, line, script, index, recipient, message, payload, values in room.messages():
        if only is None or str(recipient) == only:
            at = f"{Path(file).name}:{line}" if file else "?"
            print(f"{at}\t{script}[{index}]\tto {recipient}\t{message}\t{' ; '.join(payload)}\t{' '.join(values)}")
            count += 1
    for problem in room.problems:
        print(f"{args[0]}: {problem}", file=sys.stderr)
    if not count:
        print(f"{args[0]}: no scene message" + (f" to {only}" if only is not None else "") + " in its event scripts (its scenes may live in an actor package)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
