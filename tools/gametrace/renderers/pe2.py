"""Renderers for tools/gametrace/events.toml: what the captured numbers mean in this game.

A renderer gets the event as captured - registers as ints, memory as bytes - and returns what the log should say.
The layouts are the decomp's own: FsFileLoadKey (src/main/fs_types.h), GameLocationKey
(include/main/session_types.h), SOUND_ID (include/main/sound_ids.h).
"""
import struct

from gametrace import renderer

SOUND_TYPES = {0: "common", 1: "footstep", 2: "weapon", 4: "character", 5: "area"}


def signed(value: int) -> int:
    return value - (1 << 32) if value & 0x80000000 else value


def sound(ident: int) -> dict:
    kind, bank, tag, entry = ident >> 28, (ident >> 16) & 0xFFF, (ident >> 8) & 0xFF, ident & 0xFF
    out = {"sound": f"{ident:#010x}", "type": SOUND_TYPES.get(kind, kind), "bank": f"{(ident >> 16) & 0xFFFF:#06x}", "entry": f"{entry:#04x}"}
    if kind == 5:
        out["stage"], out["area"] = bank >> 8, bank & 0xFF
    if tag:
        out["tag"] = tag
    return out


@renderer("file_load")
def file_load(record, trace):
    index, hundreds, group, stage = record["a0:4"]
    if stage == 0:                                   # the global library: one decimal id
        what = {"file": group * 10000 + hundreds * 100 + index}
    else:                                            # a stage's folder: the folder's id and the file's index in it
        what = {"stage": stage, "folder": group * 100 + hundreds, "index": index}
    return {"event": "file_load", **what, "mode": record["a1"], "page_offset": signed(record["a2"]), "row_offset": signed(record["a3"]),
            "from": trace.symbol(record["ra"])}


@renderer("sound_request")
def sound_request(record, trace):
    if record["phase"] == "exit":
        return {"event": "sound_request", "phase": "exit", "taken": record["v0"] != 0, "v0": f"{record['v0']:#010x}"}
    return {"event": "sound_request", "phase": "enter", **sound(record["a0"]), "pan": signed(record["a1"]), "depth": signed(record["a2"]),
            "from": trace.symbol(record["ra"])}


@renderer("variant_set")
def variant_set(record, trace):
    view, room, area, stage, warp, current = struct.unpack_from("<6B", record["a0:8"])
    return {"event": "variant_set", "stage": stage, "area": area, "room": room, "view": view, "warp": warp, "was": current,
            "variant": record["a1"], "reset_mode": signed(record["a2"]), "from": trace.symbol(record["ra"])}


def location(data: bytes) -> dict:
    view, room, area, stage, warp, variant = struct.unpack_from("<6B", data)
    return {"stage": stage, "area": area, "room": room, "view": view, "warp": warp, "variant": variant}


@renderer("location")
def at_location(record, trace):
    """An event whose only capture of interest is the live location (GameLocationKey at 0x800714C4)."""
    out = {"event": record["name"], "phase": record["phase"]}
    if "0x800714C4:8" in record:
        out.update(location(record["0x800714C4:8"]))
    if "ra" in record:
        out["from"] = trace.symbol(record["ra"])
    if "v0" in record:
        out["v0"] = record["v0"]
    return out


@renderer("sound")
def any_sound(record, trace):
    """A sound call whose first argument is a sound id."""
    rest = {key: signed(value) for key, value in record.items() if key in ("a1", "a2", "a3")}
    return {"event": record["name"], **sound(record["a0"]), **rest, "from": trace.symbol(record["ra"])}


@renderer("sound_bank_load")
def sound_bank_load(record, trace):
    if record["phase"] == "exit":
        return {"event": "sound_bank_load", "phase": "exit", "descriptor": f"{record['v0']:#010x}", "loaded": record["v0"] != 0}
    tag, bank, kind, groups, layers, _u9, wave_block, _sectors, _ud, image_bytes, wave_bytes = struct.unpack_from("<IHBBBBHBBHi", record["a0:20"])
    return {"event": "sound_bank_load", "phase": "enter", "bank": f"{bank:#06x}", "type": SOUND_TYPES.get(bank >> 12, bank >> 12),
            "groups": groups, "layers": layers, "wave_bytes": wave_bytes, "tag_ok": tag == 0x4B505368, "from": trace.symbol(record["ra"])}


@renderer("sound_bank_free")
def sound_bank_free(record, trace):
    bank = struct.unpack_from("<H", record["a0:16"], 8)[0]          # SndBank.bankId
    return {"event": "sound_bank_free", "bank": f"{bank:#06x}", "type": SOUND_TYPES.get(bank >> 12, bank >> 12), "from": trace.symbol(record["ra"])}


@renderer("rect")
def rect(record, trace):
    """LoadImage / MoveImage: where in video memory, and how much."""
    x, y, w, h = struct.unpack_from("<4h", record["a0:8"])
    out = {"event": record["name"], "x": x, "y": y, "w": w, "h": h}
    if record["name"] == "image_move":
        out["to_x"], out["to_y"] = signed(record["a1"]), signed(record["a2"])
    out["from"] = trace.symbol(record["ra"])
    return out
