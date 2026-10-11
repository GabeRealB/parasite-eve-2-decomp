"""Python hooks for tools/gametrace/events.toml: what the tracer's events mean in this game.

Renderers say how an event is written: they get it as captured - registers as ints, memory as bytes - and return
what the log should say. Handlers act when one happens, with the game stopped: `file_loaded` tells the tracer
which packages a finished load has put in memory, so that the events in them can be hooked.

The layouts are the decomp's own: FsFileLoadKey (src/main/fs_types.h), GameLocationKey
(include/main/session_types.h), SOUND_ID (include/main/sound_ids.h); which packages a disc file carries comes from
the extractor's tables (tools/peassets/asset_data.py).
"""
import struct

from gametrace import handler, renderer

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
    LOADING.clear()
    LOADING.update(what)
    return {"event": "file_load", **what, "mode": record["a1"], "page_offset": signed(record["a2"]), "row_offset": signed(record["a3"]),
            "from": trace.symbol(record["ra"])}


LOADING: dict = {}       # the file whose load was last begun; files load one at a time


def packages_of(what: dict) -> list[str]:
    """The packages a disc file carries, from the extractor's tables: the images that are in memory once it is loaded."""
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "peassets"))
    import asset_data
    if not what:                                     # a load finished that the trace did not see begin
        return []
    if "file" in what:
        nodes = [f for f in (asset_data.TREE[0].get("files") or {}).values() if f["id"] == what["file"]]
    else:
        folders = [f for f in (asset_data.TREE.get(what["stage"], {}).get("folders") or {}).values() if f["id"] == what["folder"]]
        nodes = [f for folder in folders for f in (folder.get("files") or {}).values() if f["id"] == what["index"]]
    return [name for node in nodes for name in node["chunks"].values() if asset_data.ASSETS.get(name, {}).get("type") == "pe2pkg"]


@handler("file_loaded")
def file_loaded(record, trace):
    """The file whose load began last is in memory, every chunk of it: its packages are resident from here."""
    for package in packages_of(LOADING):
        trace.resident(package)


@renderer("file_loaded")
def file_loaded_line(record, trace):
    return {"event": "file_loaded", **LOADING, "packages": packages_of(LOADING)}


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
    if bank == 0xFFFF:                                                # the descriptor was free already: nothing is released
        return {"event": "sound_bank_free", "bank": None, "note": "an empty slot", "from": trace.symbol(record["ra"])}
    return {"event": "sound_bank_free", "bank": f"{bank:#06x}", "type": SOUND_TYPES.get(bank >> 12, bank >> 12), "from": trace.symbol(record["ra"])}


@renderer("image_chunk")
def image_chunk(record, trace):
    """fsUploadImageChunk: the chunk's header gives where it goes. Height is stored before width (FsImageChunk)."""
    if record["phase"] == "exit":
        return {"event": "image_chunk_upload", "phase": "exit", "status": record["v0"] & 0xFF}
    x, y, h, w = struct.unpack_from("<4H", record["a0:8"])
    return {"event": "image_chunk_upload", "phase": "enter", "x": x, "y": y, "w": w, "h": h, "kind": "palette" if h <= 16 and w in (16, 256) else "image",
            "from": trace.symbol(record["ra"])}


@renderer("rect")
def rect(record, trace):
    """LoadImage / MoveImage: where in video memory, and how much."""
    x, y, w, h = struct.unpack_from("<4h", record["a0:8"])
    out = {"event": record["name"], "x": x, "y": y, "w": w, "h": h}
    if record["name"] == "image_move":
        out["to_x"], out["to_y"] = signed(record["a1"]), signed(record["a2"])
    out["from"] = trace.symbol(record["ra"])
    return out
