#!/usr/bin/env python3
"""Every message a room's event scripts send to a scene slot, with its payload.

    room_messages.py mist_parking [--to N]

A room's scripts drive its placed actors with `SEND_MESSAGE` commands addressed
to `GAME_TASK_SLOT_SCENE`: which clip to play, where to walk, which command to
run. The payload is usually a pointer to a small record defined elsewhere in
the room (`AnimationPlayRequest`, `ActorCommand`, ...). This prints one line
per command - file:line, the recipient, the message, each record the payload
names with its initialiser, and the literal values - so a clip request can be
tied to a script line without reading the script row by row.

It reads the C text and nothing else: a script still in assembly, a payload
built at run time and a message sent from C code are not listed.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECORD = r"(AnimationPlayRequest|ActorCommand|ActorMotionWalkAnim|ActorTransform)"
SEND = re.compile(r"SCENE \}, \{ \.value = (\w+) \}, \{ \.value = (\w+) \}, (.*)$")


def records(lines: list[tuple[str, int, str]]) -> dict[str, str]:
    """{symbol: initialiser text} for the payload records a room defines."""
    found, array = {}, None
    for _, _, text in lines:
        one = re.match(RECORD + r"\s+(D_\w+)\s*=\s*(.*);", text)
        if one:
            found[one.group(2)] = one.group(3)
            continue
        start = re.match(RECORD + r"\s+(D_\w+)\[(\d+)\]\s*=\s*\{", text)
        if start:
            array = (start.group(2), [])
        elif array and text.startswith("};"):
            found[array[0]] = " | ".join(array[1])
            array = None
        elif array:
            array[1].append(text.strip())
    return found


def messages(room: str):
    """(file, line, recipient, message, [payload records], [literal values]) for each scene message."""
    lines = [(path.name, n, text.rstrip()) for path in sorted((ROOT / "src" / "rooms" / room).glob("*.c"))
             for n, text in enumerate(path.read_text(encoding="utf-8").splitlines(), 1)]
    known = records(lines)
    for name, n, text in lines:
        if "SEND_MESSAGE" not in text or "GAME_TASK_SLOT_SCENE" not in text:
            continue
        sent = SEND.search(text)
        if not sent:
            continue
        recipient, message, rest = sent.groups()
        payload = [f"{sym}{index}={known[sym]}" if sym in known else sym + index for sym, index in re.findall(r"&?(D_\w+)(\[\d+\])?", rest)]
        yield name, n, recipient, message, payload, re.findall(r"\.value = (-?\w+)", rest)


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    only = args[args.index("--to") + 1] if "--to" in args else None
    count = 0
    for name, n, recipient, message, payload, values in messages(args[0]):
        if only is None or recipient == only:
            print(f"{name}:{n}\tto {recipient}\t{message}\t{' ; '.join(payload)}\t{' '.join(values)}")
            count += 1
    if not count:
        print(f"{args[0]}: no scene message in its C event scripts (its scenes may live in an actor package)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
