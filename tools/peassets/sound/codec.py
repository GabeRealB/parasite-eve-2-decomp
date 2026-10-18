"""A PE2 sound bank (`hSPK`, a sound-script bank) as its parts, and back, byte for byte.

    bank = decode(blob)     programs and their layers, the effect scripts, the sample pool
    blob = encode(bank)     identical for every bank on the disc (472)

A bank is, in file order (src/main/sound_types.h `SndBankPayload`, `SndBankGroup`, `SndBankLayer`; the script image
src/main/sndscript.c):

    0x000  header (0x14), the program table (4 bytes each), the layer table (0x14 each); zeros to 0x800
    0x800  the script image: `hONE`, bank id, slot count, the slot table (padded to 4 bytes), then its chunks
           (`oneC` entry headers, `oneV` voices, `Loop`/`Wait`/`endL`/`endC` flow, `oneA` ADSR overrides,
           `oneE` pitch envelopes); zeros to the next 0x800 boundary
    pool   the samples back to back, each a VAG body as Sony's encoder writes it (a zero start block, the frames,
           the end frame, and after a one-shot sound the end block); `waveBytes` long
           zeros to the file's end, the pool end's sector floor + 0x1000 - 16

The parts:

    {"bank_id": n,
     "programs": [{"volume": v, "pan": p, "layers": [{"sample": i, "volume": .., "pan": .., "root_key": ..,
                   "fine_tune": .., "priority": .., "key_min": .., "key_max": .., "bend_down": .., "bend_up": ..,
                   "adsr1": .., "adsr2": .., "reverb": ..}]}],
     "chunks": [{"kind": "entry", "controls": {..}, "commands": [{"op": "voice", ..} | {"op": "loop", ..} |
                 {"op": "wait", ..} | {"op": "end_loop"}]} | {"kind": "adsr", ..} | {"kind": "envelope", ..}],
     "slots": [chunk index of the entry, or None],
     "pool": [{"sample": bytes} | {"raw": bytes}]}

A layer's `sample` is an index into the pool; a voice's ADSR and envelope are chunk indices (None: the layer's,
none). Everything else is derived, and decoding checks it is what the derivation gives:

- counts, sizes, offsets, wave addresses, padding; the header's `transferSectors` (the sectors through the pool's
  end), byte 9 (the number of samples) and byte 0x0D (the sectors before the pool);
- a pitch envelope's slopes: it is authored as levels and durations (delay, attack level and updates, hold, decay
  updates, sustain level, release level and updates), and each slope is the change over its updates, truncated
  toward zero, at least one step when there is a change, the whole change when the updates are 0;
- bytes that are always 0 or 0xFF: a program's byte 1 (0xFF), a layer's byte 2, an entry's bytes 0x0A..0x0B, a
  voice's byte 0x0F and a loop's byte 1, the envelope's last halfword.

What is stored and read by nothing: the release updates of an envelope (the interpreter steps by the slope alone),
and an entry's byte 4 (`unknown_4`): a voice count written once and never updated. Over the three releases
(JP SLPS-02480/1, US, EU) 182 entries were edited and none changed it, including entries whose voices changed - US
`spk_157` entry 7 holds 8 with 10 voices, and the JP build of it has 8 voices; five entries of `spk_377` gained voices
(9 -> 19 at most) and kept 1. So it is the count at some early point, presumably when the entry was made: 98.7% of
entries hold 1 or their current count, long sequences (built up voice by voice) hold 1, and it never exceeds 8.

One bank, `spk_217`, has a pool slot the tool reserved and never wrote: its 8 layers use pool positions 0-6 and 8,
the header counts 8 samples, and position 7 holds 448 bytes of the tool's own memory - a big-endian heap block
(`0A10BFB0 00000000 000001C0 0A10F1E0`, its size word 0x1C0 being the whole gap) - so the bank was built on a
big-endian host. The bank is copied whole, gap included, into a movie file, so the gap is the bank as built.
It decodes as a raw pool item.

`endC` follows a non-empty command list only. The one empty entry on the three releases (EU bank 0x5101, at 0x298)
is a copy of the next entry's header at volume 110 that the PAL rework inserted: the tool laid it out (later chunks
moved 16 bytes, slot 11 re-pointed past it, imageBytes grew by 0x10) but gave it no commands, no endC and no slot.
"""
from __future__ import annotations

import struct

HEADER = 0x14
IMAGE_AT = 0x800
SECTOR = 0x800
END_BLOCK = bytes([0x00, 0x07]) + bytes([0x77]) * 14      # EncVagFin's end block after a one-shot sound
GROUP_BYTE1 = 0xFF                                          # every program's second byte; no reader
LAYER_BYTE2 = 0x00                                          # every layer's third byte; no reader
CHUNK_SIZE = {b"oneC": 0x10, b"oneV": 0x18, b"Loop": 0x08, b"Wait": 0x08, b"endL": 0x04, b"endC": 0x04,
              b"oneA": 0x08, b"oneE": 0x1C}
CONTROLS = ("unknown_4", "volume", "pan", "max_instances", "retrigger", "priority", "flags")
VOICE = ("bank", "program", "layer", "delay", "gate", "pan", "volume", "reverb", "priority", "adsr", "pitch", "envelope")
# The authored envelope; `_SndPitchEnvelope` (src/main/sndscript.c) holds the slopes, and the disc's chunk (28 bytes,
# the struct 24) adds the release updates and a zero halfword.
ENVELOPE = ("delay", "attack_level", "attack_updates", "hold", "decay_updates", "sustain_level", "release_level",
            "release_updates")


def _slope(delta: int, updates: int) -> int:
    """A ramp's step per update, as the authoring tool computed it."""
    if updates == 0:
        return delta
    step = int(delta / updates)
    return step if step or not delta else (1 if delta > 0 else -1)


def _envelope_words(e: dict) -> tuple:
    return (e["delay"], e["attack_level"], _slope(e["attack_level"], e["attack_updates"]), e["attack_updates"],
            e["hold"], e["decay_updates"], _slope(e["sustain_level"] - e["attack_level"], e["decay_updates"]),
            e["sustain_level"], e["release_level"], _slope(e["release_level"] - e["sustain_level"],
                                                            e["release_updates"]), e["release_updates"], 0)


def _expect(what: str, got, want) -> None:
    if got != want:
        raise ValueError(f"{what} is {got!r}, not the derived {want!r}")


def _align(n: int, a: int) -> int:
    return (n + a - 1) & ~(a - 1)


def _pool(blob: bytes, start: int, size: int) -> list[dict]:
    """The pool as VAG bodies, and raw bytes where something else sits between them."""
    items, at, end = [], start, start + size
    while at < end:
        if blob[at:at + 16] == bytes(16):
            q = at + 16
            while q < end and not blob[q + 1] & 1:
                q += 16
            q += 16
            if blob[q:q + 16] == END_BLOCK:
                q += 16
            if q <= end:
                items.append({"sample": blob[at:q]})
                at = q
                continue
        nxt = next((o for o in range(at + 16, end, 16) if blob[o:o + 16] == bytes(16)), end)
        items.append({"raw": blob[at:nxt]})
        at = nxt
    return items


def decode(blob: bytes) -> dict:
    if blob[:4] != b"hSPK" or blob[6] != 2:
        raise ValueError("not a sound-script bank (hSPK, image kind 2)")
    bank_id, _kind, groups, layers, sample_count, wave_block, sectors, head_sectors, image_bytes = struct.unpack_from(
        "<HBBBBHBBH", blob, 4)
    wave_bytes = struct.unpack_from("<i", blob, 0x10)[0]
    if wave_block:
        raise ValueError("a pool uploaded at an offset (waveBlockOffset) is not handled")
    pool_at = _align(IMAGE_AT + image_bytes, SECTOR)
    pool = _pool(blob, pool_at, wave_bytes)
    _expect("header byte 9 (samples)", sample_count, sum(1 for item in pool if "sample" in item))
    _expect("header byte 0x0D (sectors before the pool)", head_sectors, pool_at // SECTOR)
    _expect("transferSectors", sectors, _align(pool_at + wave_bytes, SECTOR) // SECTOR)
    offset_of, at = {}, 0
    for i, item in enumerate(pool):
        offset_of[at] = i
        at += len(item.get("sample") or item.get("raw"))

    programs, lt = [], HEADER + 4 * groups
    k = 0
    for g in range(groups):
        count, byte1, volume, pan = blob[HEADER + 4 * g:HEADER + 4 * g + 4]
        if byte1 != GROUP_BYTE1:
            raise ValueError(f"program {g}: byte 1 is {byte1}")
        prog = {"volume": volume, "pan": pan, "layers": []}
        for _ in range(count):
            o = lt + 0x14 * k
            reverb, lpan, byte2, lvol, root, fine, prio, kmin, kmax, bd, bu, a1, a2, wave = struct.unpack_from(
                "<BBBBBBHBBBBHHi", blob, o)
            if byte2 != LAYER_BYTE2:
                raise ValueError(f"layer {k}: byte 2 is {byte2}")
            prog["layers"].append({"sample": offset_of[wave], "volume": lvol, "pan": lpan, "root_key": root,
                                   "fine_tune": fine, "priority": prio, "key_min": kmin, "key_max": kmax,
                                   "bend_down": bd, "bend_up": bu, "adsr1": a1, "adsr2": a2, "reverb": reverb})
            k += 1
        programs.append(prog)
    if k != layers:
        raise ValueError(f"programs hold {k} layers, the header says {layers}")

    image = blob[IMAGE_AT:IMAGE_AT + image_bytes]
    if image[:4] != b"hONE":
        raise ValueError("no hONE script image")
    img_bank, count = struct.unpack_from("<HH", image, 4)
    if img_bank != bank_id:
        raise ValueError("the script image names another bank")
    table = struct.unpack_from(f"<{count}H", image, 8)
    chunks, starts, at = [], {}, _align(8 + 2 * count, 4)
    entry = None
    while at < len(image):
        tag = image[at:at + 4]
        if tag not in CHUNK_SIZE:
            raise ValueError(f"image offset {at:#x}: unknown chunk {tag!r}")
        body = image[at + 4:at + CHUNK_SIZE[tag]]
        if tag in (b"oneC", b"oneA", b"oneE") and entry is not None:
            if entry["commands"]:
                raise ValueError(f"image offset {at:#x}: an entry with commands and no endC")
            entry = None                                    # an empty entry: the tool writes no endC
        if tag == b"oneC":
            u4, vol, pan, inst, retrig, zero_a, prio, flags = struct.unpack("<BBBBhHHH", body)
            _expect("an entry's bytes 0x0A..0x0B", zero_a, 0)
            entry = {"kind": "entry", "controls": dict(zip(CONTROLS, (u4, vol, pan, inst, retrig, prio, flags))),
                     "commands": []}
            starts[at] = len(chunks)
            chunks.append(entry)
        elif tag in (b"oneA", b"oneE"):
            starts[at] = len(chunks)
            if tag == b"oneA":
                chunks.append({"kind": "adsr", "adsr1": struct.unpack_from("<H", body, 0)[0],
                               "adsr2": struct.unpack_from("<H", body, 2)[0]})
            else:
                words = struct.unpack("<hhhHhHhhhhHH", body)
                env = {"kind": "envelope", **dict(zip(ENVELOPE, (words[0], words[1], words[3], words[4], words[5],
                                                                 words[7], words[8], words[10])))}
                _expect("an envelope's words", words, _envelope_words(env))
                chunks.append(env)
        elif entry is None:
            raise ValueError(f"image offset {at:#x}: a command outside an entry")
        elif tag == b"oneV":
            *head, pad, prio, adsr, pitch, env = struct.unpack("<HBBHHbbbBHhHh", body)
            _expect("a voice's byte 0x0F", pad, 0)
            entry["commands"].append({"op": "voice", **dict(zip(VOICE, (*head, prio, adsr, pitch, env)))})
        elif tag == b"Loop":
            repeat, pad, delay = struct.unpack("<BBH", body)
            _expect("a loop's byte 1", pad, 0)
            entry["commands"].append({"op": "loop", "repeat": repeat, "delay": delay})
        elif tag == b"Wait":
            entry["commands"].append({"op": "wait", "ticks": struct.unpack("<i", body)[0]})
        elif tag == b"endL":
            entry["commands"].append({"op": "end_loop"})
        else:                                               # endC
            entry = None
        at += CHUNK_SIZE[tag]
    for ch in chunks:                                       # references by chunk, not by offset
        for cmd in ch.get("commands", ()):
            if cmd["op"] == "voice":
                for field in ("adsr", "envelope"):
                    cmd[field] = None if cmd[field] == -1 else starts[cmd[field]]
    slots = [None if off == 0 else starts[off] for off in table]
    bank = {"bank_id": bank_id, "programs": programs, "chunks": chunks,
            "slots": slots, "pool": pool}
    return bank


def encode(bank: dict) -> bytes:
    programs, chunks, pool = bank["programs"], bank["chunks"], bank["pool"]
    # the script image: lay the chunks out after the slot table, then resolve offsets
    count = len(bank["slots"])
    at = _align(8 + 2 * count, 4)
    offset = []
    for ch in chunks:
        offset.append(at)
        if ch["kind"] == "entry":
            at += 0x10
            for cmd in ch["commands"]:
                at += {"voice": 0x18, "loop": 0x08, "wait": 0x08, "end_loop": 0x04}[cmd["op"]]
            at += 4 if ch["commands"] else 0                # endC, after a non-empty command list
        else:
            at += 0x08 if ch["kind"] == "adsr" else 0x1C
    image = bytearray(struct.pack("<4sHH", b"hONE", bank["bank_id"], count))
    image += struct.pack(f"<{count}H", *(0 if s is None else offset[s] for s in bank["slots"]))
    image += bytes(_align(len(image), 4) - len(image))
    for ch in chunks:
        if ch["kind"] == "entry":
            c = ch["controls"]
            image += b"oneC" + struct.pack("<BBBBhHHH", c["unknown_4"], c["volume"], c["pan"], c["max_instances"],
                                           c["retrigger"], 0, c["priority"], c["flags"])
            for cmd in ch["commands"]:
                if cmd["op"] == "voice":
                    v = dict(cmd)
                    for field in ("adsr", "envelope"):
                        v[field] = -1 if v[field] is None else offset[v[field]]
                    image += b"oneV" + struct.pack("<HBBHHbbbBHhHh", v["bank"], v["program"], v["layer"], v["delay"],
                                                   v["gate"], v["pan"], v["volume"], v["reverb"], 0, v["priority"],
                                                   v["adsr"], v["pitch"], v["envelope"])
                elif cmd["op"] == "loop":
                    image += b"Loop" + struct.pack("<BBH", cmd["repeat"], 0, cmd["delay"])
                elif cmd["op"] == "wait":
                    image += b"Wait" + struct.pack("<i", cmd["ticks"])
                else:
                    image += b"endL"
            if ch["commands"]:
                image += b"endC"
        elif ch["kind"] == "adsr":
            image += b"oneA" + struct.pack("<HH", ch["adsr1"], ch["adsr2"])
        else:
            image += b"oneE" + struct.pack("<hhhHhHhhhhHH", *_envelope_words(ch))
    # the pool: wave addresses from the order
    address, at = [], 0
    for item in pool:
        address.append(at)
        at += len(item.get("sample") or item.get("raw"))
    wave_bytes = at
    layers = [layer for p in programs for layer in p["layers"]]
    image_bytes = len(image)
    pool_at = _align(IMAGE_AT + image_bytes, SECTOR)
    sectors = _align(pool_at + wave_bytes, SECTOR) // SECTOR
    samples = sum(1 for item in pool if "sample" in item)
    out = bytearray(b"hSPK" + struct.pack("<HBBBBHBBHi", bank["bank_id"], 2, len(programs), len(layers), samples, 0,
                                          sectors, pool_at // SECTOR, image_bytes, wave_bytes))
    for p in programs:
        out += bytes([len(p["layers"]), GROUP_BYTE1, p["volume"], p["pan"]])
    for L in layers:
        out += struct.pack("<BBBBBBHBBBBHHi", L["reverb"], L["pan"], LAYER_BYTE2, L["volume"], L["root_key"],
                           L["fine_tune"], L["priority"], L["key_min"], L["key_max"], L["bend_down"], L["bend_up"],
                           L["adsr1"], L["adsr2"], address[L["sample"]])
    out += bytes(IMAGE_AT - len(out))
    out += image + bytes(pool_at - IMAGE_AT - image_bytes)
    for item in pool:
        out += item.get("sample") or item.get("raw")
    end = ((pool_at + wave_bytes) & ~(SECTOR - 1)) + 2 * SECTOR - 16
    out += bytes(end - len(out))
    return bytes(out)
