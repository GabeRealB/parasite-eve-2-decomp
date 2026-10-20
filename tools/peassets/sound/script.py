"""A bank's own text: its id, the effect scripts (codec.py's chunks and slots), and any pool bytes that are not a
sample - and back.

    text = write(chunks, slots, title, bank_id, raw)
    chunks, slots, bank_id, raw = read(text)

`raw` maps a pool position to bytes kept as they are (`raw 7 0a10bfb0...`, hex): only `spk_217` has one, a slot its
tool reserved and filled with its own memory.

The chunks follow in image order, numbered from 0. An entry lists its controls, then its commands indented, a loop's
body one level deeper; an ADSR override and a pitch envelope are chunks of their own, which a voice names by number.
Fields at their default are left out. Slots map sound ids to entries (`slot 0x04 = 3`; an unlisted slot is empty),
followed by the slot's name where its bank's record names it (`slot 0x04 = 3 shot`; the record's `sounds` is where
names are kept, so reading ignores them).

    bank 0x2002
    slots 5
    slot 0x00 = 0
    entry 0 volume=127 pan=64 instances=1 retrigger=0 priority=12288 flags=0x0000
        voice program=0 layer=0 priority=16384
        loop 3
            voice program=1 layer=0 gate=20 priority=16384 adsr=1
            wait 10
        end
    adsr 1 adsr1=0x80FF adsr2=0x5FE3
    envelope 2 delay=0 attack_level=256 attack_updates=18 hold=0 decay_updates=0 sustain_level=256 ...

Voice fields: bank (0 = this bank), program, layer, delay, gate, pan, volume, reverb (-1 = the layer's), priority,
adsr / envelope (a chunk number, absent = the layer's / none), pitch (1/128 semitone over the layer's lowest key).
An envelope is its levels and durations (in pitch updates; levels in 1/256 semitone); its slopes are derived.
`unknown_4` is a byte the authoring tool wrote and nothing reads.
"""
from __future__ import annotations

ENTRY_FIELDS = (("volume", "volume", None), ("pan", "pan", None), ("instances", "max_instances", None),
                ("retrigger", "retrigger", None), ("priority", "priority", None), ("flags", "flags", None),
                ("unknown_4", "unknown_4", None))
VOICE_FIELDS = (("bank", 0), ("program", None), ("layer", None), ("delay", 0), ("gate", 0), ("pan", -1),
                ("volume", -1), ("reverb", -1), ("priority", None), ("adsr", -1), ("envelope", -1), ("pitch", 0))
ENVELOPE_FIELDS = ("delay", "attack_level", "attack_updates", "hold", "decay_updates", "sustain_level",
                   "release_level", "release_updates")


def _fields(pairs) -> str:
    return " ".join(f"{k}={v}" for k, v in pairs)


def write(chunks: list[dict], slots: list, title: str, bank_id: int, raw: dict[int, bytes] | None = None,
          names: dict[int, str] | None = None) -> str:
    out = [f"# {title}", f"bank 0x{bank_id:04X}"]
    out += [f"raw {i} {b.hex()}" for i, b in sorted((raw or {}).items())]
    out.append(f"slots {len(slots)}")
    out += [f"slot 0x{n:02X} = {s}" + (f" {names[n]}" if n in (names or {}) else "")
            for n, s in enumerate(slots) if s is not None]
    for i, ch in enumerate(chunks):
        if ch["kind"] == "entry":
            c = ch["controls"]
            head = [(name, (f"0x{c[key]:04X}" if name == "flags" else c[key])) for name, key, default in ENTRY_FIELDS
                    if default is None or c[key] != default]
            out.append(f"entry {i} " + _fields(head))
            depth = 1
            for cmd in ch["commands"]:
                if cmd["op"] == "end_loop":
                    depth -= 1
                    out.append("    " * depth + "end")
                    continue
                if cmd["op"] == "voice":
                    v = {k: (-1 if cmd[k] is None else cmd[k]) if k in ("adsr", "envelope") else cmd[k]
                         for k, _ in VOICE_FIELDS}
                    out.append("    " * depth + "voice " + _fields((k, v[k]) for k, d in VOICE_FIELDS
                                                                 if d is None or v[k] != d))
                elif cmd["op"] == "wait":
                    out.append("    " * depth + f"wait {cmd['ticks']}")
                else:
                    extra = [("delay", cmd["delay"])] if cmd["delay"] else []
                    out.append("    " * depth + f"loop {cmd['repeat']}" + (" " + _fields(extra) if extra else ""))
                    depth += 1
        elif ch["kind"] == "adsr":
            out.append(f"adsr {i} adsr1=0x{ch['adsr1']:04X} adsr2=0x{ch['adsr2']:04X}")
        else:
            out.append(f"envelope {i} " + _fields((k, ch[k]) for k in ENVELOPE_FIELDS))
    return "\n".join(out) + "\n"


def _kv(words: list[str]) -> dict[str, int]:
    out = {}
    for w in words:
        k, _, v = w.partition("=")
        out[k] = int(v, 0)
    return out


def read(text: str) -> tuple[list[dict], list, int, dict[int, bytes]]:
    chunks, slots, entry, bank_id, raw = [], [], None, None, {}
    for text_line in text.splitlines():
        line = text_line.split("#", 1)[0].strip()
        if not line:
            continue
        word, *rest = line.split()
        if word == "bank":
            bank_id = int(rest[0], 0)
        elif word == "raw":
            raw[int(rest[0])] = bytes.fromhex(rest[1])
        elif word == "slots":
            slots = [None] * int(rest[0], 0)
        elif word == "slot":
            slots[int(rest[0], 0)] = int(rest[2])
        elif word in ("entry", "adsr", "envelope"):
            if int(rest[0]) != len(chunks):
                raise ValueError(f"chunk {rest[0]} out of order (expected {len(chunks)})")
            kv = _kv(rest[1:])
            if word == "entry":
                controls = {key: kv.get(name, default if default is not None else 0) for name, key, default in ENTRY_FIELDS}
                entry = {"kind": "entry", "controls": controls, "commands": []}
                chunks.append(entry)
            elif word == "adsr":
                chunks.append({"kind": "adsr", "adsr1": kv["adsr1"], "adsr2": kv["adsr2"]}); entry = None
            else:
                chunks.append({"kind": "envelope", **{k: kv[k] for k in ENVELOPE_FIELDS}}); entry = None
        elif entry is None:
            raise ValueError(f"a command outside an entry: {line}")
        elif word == "voice":
            kv = _kv(rest)
            v = {k: kv.get(k, d if d is not None else 0) for k, d in VOICE_FIELDS}
            for k in ("adsr", "envelope"):
                v[k] = None if v[k] == -1 else v[k]
            entry["commands"].append({"op": "voice", **v})
        elif word == "wait":
            entry["commands"].append({"op": "wait", "ticks": int(rest[0], 0)})
        elif word == "loop":
            kv = _kv(rest[1:])
            entry["commands"].append({"op": "loop", "repeat": int(rest[0], 0), "delay": kv.get("delay", 0)})
        elif word == "end":
            entry["commands"].append({"op": "end_loop"})
        else:
            raise ValueError(f"unknown line: {line}")
    if bank_id is None:
        raise ValueError("no `bank` line")
    return chunks, slots, bank_id, raw
