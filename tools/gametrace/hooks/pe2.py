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

REPO = __import__("pathlib").Path(__file__).resolve().parents[3]
_CONSTANTS: dict = {}


def constants(header: str, prefix: str) -> dict[int, str]:
    """value -> name, for the constants of a header whose names start with `prefix`.

    The decomp's headers are where a number's meaning is written down, and the names in them change as the naming
    work goes on, so they are read from the header each run and not copied here."""
    key = (header, prefix)
    if key not in _CONSTANTS:
        import re
        found: dict[int, str] = {}
        try:
            text = (REPO / header).read_text(errors="replace")
        except OSError:
            text = ""
        for name, value in re.findall(r"^\s*(%s\w*)\s*=\s*\(?(?:s32\))?\(?(-?0x[0-9A-Fa-f]+|-?\d+)\)?\s*[,}/]" % prefix, text, re.M):
            found.setdefault(int(value, 0) & 0xFFFFFFFF, name)
        _CONSTANTS[key] = found
    return _CONSTANTS[key]


def named(value: int, header: str, prefix: str) -> str:
    """The constant's name with its prefix dropped, lower case - or the number when the header has no name for it."""
    name = constants(header, prefix).get(value & 0xFFFFFFFF)
    return name[len(prefix):].lower().replace("_", " ") if name else str(signed(value))


# The bank types a sound id's top nibble names (include/main/sound_ids.h, src/main/sound_types.h). Type 1 is the
# room's footstep bank, 14 the parasite-energy banks, 15 the music sequences.
SOUND_TYPES = {0: "common", 1: "footstep", 2: "weapon", 4: "character", 5: "area", 6: "ambient", 14: "parasite energy", 15: "sequence"}
# What a load policy makes the loader do (fsLoadFile, src/main/fs.c): which chunks of the file are taken.
LOAD_MODES = {0: "everything", 1: "nothing: the drive only seeks to the file", 2: "everything, after the area's sound reset",
              3: "everything, with its images relocated", 4: "everything but the background", 5: "its images only", 6: "everything but the sound bank"}
LOADS_PACKAGE = {0, 2, 3, 4, 6}        # the policies under which a file's package is read


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
    mode = record["a1"] & 0xFF                       # the loader uses the low byte; an unknown value means the default
    LOADING.clear()
    LOADING.update(what)
    LOADING["mode"] = mode
    # The offsets are signed bytes: pages of 64 across for images high in video memory or relocated, rows for
    # palettes whose header says row 245..255.
    return {"event": "file_load", **what, **named_file(what), "mode": mode, "policy": named(mode, "include/main/fs_types.h", "CD_COMMAND_LOAD_"),
            "takes": LOAD_MODES.get(mode, LOAD_MODES[0]), "page_offset": signed(record["a2"]), "row_offset": signed(record["a3"]),
            "from": trace.symbol(record["ra"])}


LOADING: dict = {}       # the file whose load was last begun; files load one at a time


def tree():
    """The extractor's table of the disc: stages, their folders and files, by the names given to them so far."""
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "peassets"))
    import asset_data
    return asset_data.TREE


def named_file(what: dict) -> dict:
    """The names of the stage, folder and file a load's numbers stand for. They come from the extractor's table
    and the area header each run, so they follow as things are renamed; a part with no name yet is left out."""
    out: dict = {}
    if "file" in what:                               # stage zero: the library every stage shares, one flat list of files
        out["stage_name"] = "library"
        name = next((n for n, f in (tree()[0].get("files") or {}).items() if f["id"] == what["file"]), None)
    elif "stage" in what:
        stage = constants("include/main/areas.h", "GAME_STAGE_").get(what["stage"])
        if stage:
            out["stage_name"] = stage[len("GAME_STAGE_"):].lower()
        folder = next(((n, f) for n, f in (tree().get(what["stage"], {}).get("folders") or {}).items() if f["id"] == what["folder"]), None)
        if folder:
            out["folder_name"] = folder[0]
        name = next((n for n, f in ((folder[1].get("files") or {}).items() if folder else ()) if f["id"] == what["index"]), None)
    else:
        return out
    if name:
        out["file_name"] = name
        out["path"] = "/".join(part for part in (out.get("stage_name"), out.get("folder_name"), name) if part)
    else:
        out["file_name"] = None                      # the number names no file on the disc: such a load reads nothing
    return out


def packages_of(what: dict) -> list[str]:
    """The packages a disc file carries, from the extractor's tables: the images that are in memory once it is loaded."""
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "peassets"))
    import asset_data
    if not what or what.get("mode", 0) not in LOADS_PACKAGE:     # no load seen to begin, or one that reads no package
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
    return {"event": "file_loaded", **LOADING, **named_file(LOADING), "policy": named(LOADING.get("mode", 0), "include/main/fs_types.h", "CD_COMMAND_LOAD_"), "packages": packages_of(LOADING)}


@renderer("sound_request")
def sound_request(record, trace):
    if record["phase"] == "exit":
        # The id comes back when the request was queued; zero when the bank is not loaded, the entry is out of range
        # or empty, or the event pool is full (src/main/sndscript.c).
        return {"event": "sound_request", "phase": "exit", "taken": record["v0"] != 0, "v0": f"{record['v0']:#010x}"}
    return {"event": "sound_request", "phase": "enter", **sound(record["a0"]), "pan": signed(record["a1"]), "depth": signed(record["a2"]),
            "from": trace.symbol(record["ra"])}


@renderer("variant_set")
def variant_set(record, trace):
    view, room, area, stage, warp, current = struct.unpack_from("<6B", record["a0:8"])
    return {"event": "variant_set", "stage": stage, "area": area, "room": room, "view": view, "warp": warp, "was": current,
            "variant": record["a1"], "reset_mode": signed(record["a2"]),
            "saved_poses": named(record["a2"], "include/gameplay/scene_runtime.h", "AREA_VARIANT_"), "from": trace.symbol(record["ra"])}


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
            "image": {0: "sequence", 2: "script"}.get(kind & 3, kind & 3), "groups": groups, "layers": layers, "program_bytes": image_bytes,
            "wave_bytes": wave_bytes, "wave_at": wave_block * 64, "tag_ok": tag == 0x4B505368, "from": trace.symbol(record["ra"])}


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
        status = record["v0"] & 0xFF
        return {"event": "image_chunk_upload", "phase": "exit", "status": status, "result": named(status, "include/main/fs.h", "FILE_SYSTEM_IMAGE_UPLOAD_")}
    x, y, h, w = struct.unpack_from("<4H", record["a0:8"])
    return {"event": "image_chunk_upload", "phase": "enter", "x": x, "y": y, "w": w, "h": h, "waits_for_gpu": bool(record["a1"] & 0xFF), "kind": "palette" if h <= 16 and w in (16, 256) else "image",
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


# ---- the events that were logged as bare registers: what the arguments mean, from the functions' own documentation


def room_of(stage: int, area: int) -> str | None:
    """The room a stage and area number name: a room folder's disc id is its area times 100, plus one."""
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "peassets"))
    import asset_data
    for name, folder in (asset_data.TREE.get(stage, {}).get("folders") or {}).items():
        if folder["id"] // 100 == area:
            return name
    return None


@renderer("cd_command")
def cd_command(record, trace):
    """cdCmdEnqueue(command, fileKey, commandArgs): the request's opcode; a file load's key says which file."""
    out = {"event": "cd_command", "command": named(record["a0"], "include/main/fs_types.h", "CD_COMMAND_"), "opcode": f"{record['a0'] & 0xFF:#04x}"}
    if record["a0"] & 0xFF == 0x21 and any(record["a1:4"]):
        index, hundreds, group, stage = record["a1:4"]
        out.update({"file": group * 10000 + hundreds * 100 + index} if stage == 0 else {"stage": stage, "folder": group * 100 + hundreds, "index": index})
        out.update(named_file(out))
    out["from"] = trace.symbol(record["ra"])
    return out


@renderer("flag")
def flag(record, trace):
    """gameFlagSetNibble(flagId, value): the flag by its name in include/main/gameflag_ids.h."""
    name = constants("include/main/gameflag_ids.h", "GAME_FLAG_").get(record["a0"])
    return {"event": "flag_set", "flag": name or f"{record['a0']:#05x}", "id": f"{record['a0']:#05x}", "value": record["a1"] & 0xF, "from": trace.symbol(record["ra"])}


@renderer("flag_byte")
def flag_byte(record, trace):
    """gameFlagSetPackedByte(nibbleIndex, packedValue): two nibbles at once; either index of a pair names the byte."""
    low = record["a0"] & ~1
    names = [constants("include/main/gameflag_ids.h", "GAME_FLAG_").get(low + n) or f"{low + n:#05x}" for n in (0, 1)]
    return {"event": "flag_byte_set", "flags": names, "value": f"{record['a1'] & 0xFF:#04x}", "from": trace.symbol(record["ra"])}


@renderer("companion")
def companion(record, trace):
    """companionSelectForArea: 0 when no companion resources are needed, else the family to load (1..3)."""
    if record["phase"] == "exit":
        family = record["v0"]
        return {"event": "companion_select", "phase": "exit", "family": family, "result": "none" if not family else f"family {family} to load"}
    return {"event": "companion_select", "phase": "enter", **location(record["0x800714C4:8"]), "from": trace.symbol(record["ra"])}


@renderer("sound_stop")
def sound_stop(record, trace):
    """sndEvtRequestScriptStop(selector, stopControl) (include/main/sound.h)."""
    selector, control = record["a0"], record["a1"] & 0xFFFF
    if selector & 0xFF:                                   # an entry: that id, or every instance of it
        what = {**sound(selector), "which": "every instance" if (selector >> 8) & 0xFF == 0xFF else "that instance"}
    elif selector == 0x80000000:
        what = {"sound": f"{selector:#010x}", "which": "every bank type except ambient"}
    else:
        what = {"sound": f"{selector:#010x}", "which": f"every script of type {SOUND_TYPES.get(selector >> 28, selector >> 28)}"}
    how = "no fade" if control == 0 else "no fade, keeping the voices' release" if control == 1 else f"fade over about {control} audio updates"
    return {"event": "sound_stop", **what, "how": how, "from": trace.symbol(record["ra"])}


@renderer("music_start")
def music_start(record, trace):
    """sndEvtRequestMidiStart(sequenceId, fadeTicks): zero when queued, or why not."""
    if record["phase"] == "exit":
        result = {0: "queued", 0xFFFFFFFE: "event pool full", 0xFFFFFFFD: "invalid sequence"}.get(record["v0"], f"{signed(record['v0'])}")
        return {"event": "music_start", "phase": "exit", "result": result}
    return {"event": "music_start", "phase": "enter", "sequence": record["a0"] & 0xFF, "fade_updates": record["a1"] & 0xFFFF, "from": trace.symbol(record["ra"])}


@renderer("sound_reset")
def sound_reset(record, trace):
    """sndScriptResetForArea(stage, area): the banks of the room being left are released here."""
    return {"event": "sound_reset_for_area", "stage": record["a0"], "area": record["a1"], "room": room_of(record["a0"], record["a1"]), "from": trace.symbol(record["ra"])}


@renderer("caption_command")
def caption_command(record, trace):
    return {"event": record["name"], "command": signed(record["a0"]), "from": trace.symbol(record["ra"])}


@renderer("caption_file")
def caption_file(record, trace):
    """capSelectLoadedFile(ordinal): the Nth data resource of the room's bundle, counting from zero."""
    return {"event": "caption_file", "data_resource": record["a0"], "from": trace.symbol(record["ra"])}


@renderer("caption_sequence")
def caption_sequence(record, trace):
    """capStartSequenceSlot(commandIndex, playbackMode, variantKey) (include/gameplay/captions.h)."""
    modes = {0: "in the current display", 1: "after a display transition", 2: "after a transition, with room-effect messages around it",
             3: "after a transition, only if no sequence has started"}
    mode = record["a1"] & 0xFFFF
    return {"event": "caption_sequence", "command": record["a0"] & 0xFFFF, "playback": modes.get(mode, f"mode {mode}: after a transition"),
            "variant_key": record["a2"] & 0xFFFF, "from": trace.symbol(record["ra"])}


@renderer("movie_start")
def movie_start(record, trace):
    """streamPrepareMoviePlayback(slotIndex): the movie slot, 0..14."""
    return {"event": "movie_start", "slot": record["a0"] & 0xFFFF, "from": trace.symbol(record["ra"])}


@renderer("stream_scene")
def stream_scene(record, trace):
    return {"event": "stream_scene", "group": record["a0"] & 0xFFFF, "id": record["a1"] & 0xFFFF, "sub_id": record["a2"] & 0xFFFF,
            "sub_id_2": record["a3"] & 0xFFFF, "from": trace.symbol(record["ra"])}


def message_names() -> dict[int, list[str]]:
    """value -> the message constants of include/gameplay/message.h with that value (several enums share numbers)."""
    if "messages" not in _CONSTANTS:
        import re
        found: dict[int, list[str]] = {}
        try:
            text = (REPO / "include/gameplay/message.h").read_text(errors="replace")
        except OSError:
            text = ""
        for name, value in re.findall(r"^\s*(\w*MESSAGE\w*)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*[,}/]", text, re.M):
            found.setdefault(int(value, 0), []).append(name)
        _CONSTANTS["messages"] = found
    return _CONSTANTS["messages"]


@renderer("task_message")
def task_message(record, trace):
    """taskMessageDispatch(receiver, messageId, firstArg, secondArg). The receiver is a task; which names apply to
    a message number depends on the receiver's kind, so every constant with that value is given."""
    ident = record["a1"]
    return {"event": "task_message", "message": ident, "names": message_names().get(ident, []), "receiver": f"{record['a0']:#010x}",
            "arg1": f"{record['a2']:#010x}", "arg2": f"{record['a3']:#010x}", "arg1_at": trace.symbol(record["a2"]) if record["a2"] >> 24 == 0x80 else None,
            "from": trace.symbol(record["ra"])}
