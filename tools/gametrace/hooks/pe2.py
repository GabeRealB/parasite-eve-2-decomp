"""Python hooks for tools/gametrace/events.toml: what the tracer's events mean in this game.

Renderers say how an event is written: they get it as captured - registers as ints, memory as bytes - and return
what the log should say. Handlers act when one happens, with the game stopped: `package_loaded` tells the tracer
which packages a finished load has put in memory, so that the events in them can be hooked.

The layouts are the decomp's own: FsFileLoadKey (src/main/fs_types.h), GameLocationKey
(include/main/session_types.h), SOUND_ID (include/main/sound_ids.h); which packages a disc file carries comes from
the extractor's tables (tools/peassets/asset_data.py).
"""
import re
import struct

from gametrace import command, handler, image, init, renderer

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
        for name, value in re.findall(r"(?:^\s*|\benum\s*\{\s*)(%s\w*)\s*=\s*\(?(?:s32\))?\(?(-?0x[0-9A-Fa-f]+|-?\d+)\)?\s*[,}/]" % prefix, text, re.M):
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
# What a load policy makes the loader do (fsLoadFile and the chunk reader, src/main/fs.c): which chunks of the file
# are taken. Under the sound-setup and images-only policies the load ENDS at the file's first package, data or
# caption-bundle chunk, so only what is stored before that chunk is read.
LOAD_MODES = {0: "everything", 1: "nothing: the drive only seeks to the file",
              2: "the sound banks and images stored before its first package, data or bundle chunk, after the area's sound reset",
              3: "everything, with its images relocated", 4: "everything but the background",
              5: "the images stored before its first package, data or bundle chunk; no sound bank", 6: "everything but the sound bank"}
LOADS_PACKAGE = {0, 3, 4, 6}           # the policies under which a file's package is read


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
    sys.path.insert(0, str(REPO / "tools" / "peassets"))
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
    sys.path.insert(0, str(REPO / "tools" / "peassets"))
    import asset_data
    if not what or what.get("mode", 0) not in LOADS_PACKAGE:     # no load seen to begin, or one that reads no package
        return []
    if "file" in what:
        nodes = [f for f in (asset_data.TREE[0].get("files") or {}).values() if f["id"] == what["file"]]
    else:
        folders = [f for f in (asset_data.TREE.get(what["stage"], {}).get("folders") or {}).values() if f["id"] == what["folder"]]
        nodes = [f for folder in folders for f in (folder.get("files") or {}).values() if f["id"] == what["index"]]
    return [name for node in nodes for name in node["chunks"].values() if asset_data.ASSETS.get(name, {}).get("type") == "pe2pkg"]


# ---- a package arriving in memory: seen in the loader itself, where the package chunk is inflated
#
# `_fsReadChunkHeaderSector` inflates what a package chunk's first sector holds; `_fsReadChunkPayloadSector` goes on
# with the sectors after it (src/main/fs.c). A package is whole in memory at one of two places: in the first
# function where the decompressor reports the end of the stream at once (a package that fits one sector), or in
# the second where the stream ends later. Both are events with a handler, so both are breakpoints: the game stops,
# the package's id is read from the image itself, and the tracer is told the image is resident. `package_begin`,
# a hook straight after the first sector is inflated, only keeps the address the chunk's header gives, which is
# gone from the sector buffer by the time a longer package ends.

PACKAGE: dict = {}       # the package chunk being inflated: where it goes, and - once known - which package it is
PACKAGE_IDS: dict = {}   # the id every package starts with -> its name


def package_ids() -> dict:
    """Every package begins with its own id, a number no other has (the first word of the image)."""
    if not PACKAGE_IDS:
        for path in (REPO / "assets" / "USA" / "pe2pkg").glob("*.pe2pkg"):
            with path.open("rb") as handle:
                head = handle.read(4)
            if len(head) == 4:
                PACKAGE_IDS[struct.unpack("<I", head)[0]] = path.name[:-len(".pe2pkg")]
    return PACKAGE_IDS


@renderer("package_begin")
def package_begin(record, trace):
    """The first sector of a package chunk has been inflated to the address its header gives."""
    load = struct.unpack("<I", record["0x8006C4E0:4"])[0]
    status = struct.unpack("<I", record["0x8006D748:4"])[0] & 0xFFFF
    PACKAGE.clear()
    PACKAGE.update({"load": load})
    # The decompressor's status: 0 wants more input, 0xFFFF could not run (the load is restarted), anything else is
    # the end of the stream - and then the breakpoint for a one-sector package is the next thing reached.
    if status == 0xFFFF:
        return {"event": "package_begin", "load": f"{load:#010x}", "result": "the decompressor was busy: the load restarts"}
    return None


@handler("package_loaded")
def package_arrived(record, trace):
    """A package's stream has ended: the image is whole in memory. Which package it is is read from the image
    itself - its first word is its id - and the tracer is told it is resident, so its hooks are written."""
    if "0x8006C4E0:4" in record:                     # a one-sector package: the chunk's header is still in the sector buffer
        PACKAGE.clear()
        PACKAGE.update({"load": struct.unpack("<I", record["0x8006C4E0:4"])[0], "sectors": 1})
    load = PACKAGE.get("load")
    if load is None:
        return
    ident = struct.unpack("<I", trace.read(load, 4))[0]
    name = package_ids().get(ident)
    PACKAGE.update({"id": ident, "name": name})
    if name is not None and trace.extent(name)[1] == load:
        trace.resident(name)


@renderer("package_loaded")
def package_loaded(record, trace):
    end = struct.unpack("<I", record["0x8006D4D8:4"])[0]
    load, name = PACKAGE.get("load"), PACKAGE.get("name")
    out = {"event": "package_loaded", "package": name, "id": PACKAGE.get("id"), "load": f"{load:#010x}" if load is not None else None,
           "end": f"{end:#010x}", "bank": record["0x8006ADF4:4"][0]}
    if PACKAGE.get("sectors") == 1:
        out["sectors"] = 1
    if name is None:
        out["note"] = "no package has this id"
    elif trace.extent(name)[2] == 0:
        out["note"] = "not among the images the hooks declare, so its addresses are not named"
    elif trace.extent(name)[1] != load:
        out["note"] = f"the package is declared to load at {trace.extent(name)[1]:#010x}: not marked resident"
    return out


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
    sys.path.insert(0, str(REPO / "tools" / "peassets"))
    import asset_data
    for name, folder in (asset_data.TREE.get(stage, {}).get("folders") or {}).items():
        if folder["id"] // 100 == area:
            return name
    return None


@renderer("cd_command")
def cd_command(record, trace):
    """cdCmdEnqueue(command, fileKey, commandArgs): the request's opcode; for a file load, which file and how.

    A file's id is split over the two blocks (include/main/fs_types.h): the key has the index, group and stage -
    its second byte is never read - and the arguments have the hundreds, the load policy and the image offsets."""
    opcode = record["a0"] & 0xFF
    command = constants("include/main/fs_types.h", "CD_COMMAND_").get(opcode) or constants("include/main/fs.h", "CD_COMMAND_").get(opcode)
    out = {"event": "cd_command", "command": command[len("CD_COMMAND_"):].lower().replace("_", " ") if command else str(opcode), "opcode": f"{opcode:#04x}"}
    if opcode == 0x21:
        index, _unread, group, stage = record["a1:4"]
        hundreds, mode = record["a2:4"][0], record["a2:4"][1]
        out.update({"file": group * 10000 + hundreds * 100 + index} if stage == 0 else {"stage": stage, "folder": group * 100 + hundreds, "index": index})
        out.update(named_file(out))
        out.update({"mode": mode, "policy": named(mode, "include/main/fs_types.h", "CD_COMMAND_LOAD_"),
                    "page_offset": signed(record["a2:4"][2] | (0xFFFFFF00 if record["a2:4"][2] & 0x80 else 0)),
                    "row_offset": signed(record["a2:4"][3] | (0xFFFFFF00 if record["a2:4"][3] & 0x80 else 0))})
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


# ---- the project, for the tracer: the images of code this game is made of


@init
def project(cfg):
    """The executable, gameplay and every package the decomp builds: where each loads (the overlay manifest), its
    bytes as extracted, and its symbol map. Only the executable is in memory when the tracer sets up."""
    import struct
    import sys
    import tomllib
    sys.path.insert(0, str(REPO / "tools" / "peassets"))
    import asset_residency
    maps = {path.stem.removeprefix("sym."): path for path in sorted((REPO / "configs" / "USA").rglob("*.txt"))
            if "imports" not in path.name and not path.name.startswith("rel.")}
    exe = REPO / "assets" / "USA" / "main.exe"
    if exe.is_file():                                # a PS-X EXE: its load address at 0x18, its code after a 0x800-byte header
        with exe.open("rb") as handle:
            load = struct.unpack_from("<I", handle.read(0x20), 0x18)[0]
        # Its zeroed storage runs on after the file's bytes, up to where gameplay loads: the save images are there.
        image("main", load, exe, maps.get("main", ()), skip=0x800, start=True, size=0x80093800 - load)
    loads = {"gameplay": 0x80093800, "title": 0x80093800}       # the title screen's program takes gameplay's place
    loads.update(asset_residency.package_loads(tomllib.loads((REPO / "configs" / "USA" / "overlays.toml").read_text())))
    for name, load in loads.items():
        image(name, load, REPO / "assets" / "USA" / "pe2pkg" / f"{name}.pe2pkg", maps.get(name, ()))


# ---- where a room load and a view change end


@renderer("room_start")
def room_start(record, trace):
    """areaStartRoomRuntime(skipViewGate): the loading sequence's last step hands over to the room. Its files and
    its actors' are in memory; its first view loads after this."""
    return {"event": "room_start", **location(record["0x800714C4:8"]), "view_gate": "skipped" if record["a0"] else "shown",
            "from": trace.symbol(record["ra"])}


@renderer("view_ready")
def view_ready(record, trace):
    """In loadingUploadViewImageTask, once the CD queue is idle and the view's palette has been uploaded: the view
    change is complete but for a view movie, whose slot is read here (negative: the view has none)."""
    slot = struct.unpack("<h", record["0x80114C40:4"][:2])[0]
    return {"event": "view_ready", **location(record["0x800714C4:8"]), "movie_slot": slot if slot >= 0 else None}


# ---- commands: acting on the game from the tracer's prompt
#
# The tracer gives the means and checks nothing; each command here says where it may act.

FRAME_START = "padUpdatePort0"       # the first thing the main loop does each frame, whatever state the game is in:
                                     # no game function is half done there


def anywhere(trace) -> str | None:
    """Why the place the game is stopped at will not do even for a call that only reads or writes one value, or
    None when it will: it has to be the game's own code, with interrupts on, and not a delay slot (from which no
    call can be made at all)."""
    at = trace.where()
    if at["image"] is None:
        return f"the game is not in its own code ({at['symbol']})"
    if at["delay_slot"]:
        return "the game stands in a delay slot"
    if at["interrupts"] is False:
        return "interrupts are off (an interrupt handler, or something that must not be disturbed)"
    return None


def at_frame_start(trace) -> None:
    """Take the game to the start of its next frame, unless it is there."""
    if trace.registers()["pc"] != trace.address(FRAME_START):
        trace.run_to(FRAME_START)


@command("flag")
def flag_command(trace, text):
    """flag ID [VALUE]   show a game flag, or set it (0..15); ID is a number or a GAME_FLAG_ name"""
    parts = text.split()
    if not parts:                                    # no arguments: show the flags and ask
        named = constants("include/main/gameflag_ids.h", "GAME_FLAG_")
        picked = trace.choose(f"{len(named)} flags have names", [(name[len("GAME_FLAG_"):], f"{value:#05x}") for value, name in sorted(named.items())])
        if picked is None:
            return "nothing done"
        value = trace.ask(f"set {picked} to (0..15; nothing to only show it): ")
        parts = [picked] + ([value] if value else [])
    by_name = {name: value for value, name in constants("include/main/gameflag_ids.h", "GAME_FLAG_").items()}
    ident = by_name.get(parts[0].upper(), by_name.get("GAME_FLAG_" + parts[0].upper()))
    if ident is None:
        ident = int(parts[0], 0)
    name = constants("include/main/gameflag_ids.h", "GAME_FLAG_").get(ident, f"{ident:#05x}")
    if len(parts) > 1 and not 0 <= int(parts[1], 0) <= 15:
        return "a flag holds 0..15"
    # The flag functions read or write one nibble, so they may be called wherever the game stands - short of the
    # places no call can be made from, and of being inside one of them. A delay slot is left by stepping; from
    # the others the game goes to the frame start. A call leaves the game exactly where it was, so the place is
    # judged once.
    for _ in range(3):                               # out of a delay slot: one instruction on, the game has taken its jump
        if not trace.where()["delay_slot"]:
            break
        trace.step()
    reason = anywhere(trace)                         # wherever that left it - a step is also where an interrupt gets in
    if reason is None and any(trace.where()["symbol"].startswith(f) for f in ("gameFlagGetNibble+", "gameFlagSetNibble+")):
        reason = "the game is inside a flag function"
    if reason:
        trace.say(f"{reason}: going to the start of the next frame first")
        at_frame_start(trace)
    before = trace.call("gameFlagGetNibble", ident)
    if len(parts) == 1:
        return f"{name} is {before}"
    trace.call("gameFlagSetNibble", ident, int(parts[1], 0))
    return f"{name}: {before} -> {trace.call('gameFlagGetNibble', ident)}"


# ---- states: the whole saved game, kept in a file and put back
#
# Everything the game keeps between rooms is in the images a memory-card save is made from (`_gMcSaveSections`,
# src/main/mc.c): the save record, the player's status and the flag banks, 3,404 bytes. They are the game's working
# state, not a copy: play reads and writes them. So a state is those bytes, and loading one is what the game does
# after reading a card - or for an attract demo, which is the same thing with a stored state.

STATES = REPO / "local" / "gametrace" / "states"
SECTIONS = ["card file header", "save", "player", "acropolis", "dryfield", "dryfield_night", "mine_shelter", "neo_ark", "flags"]
GAME_MODE, GAME_MODE_RESTART = 0x11E, 1      # DisplayState.gameMode: the main loop reinitialises the game when it reads 1
SPRITE_VARIANT = 0x10E                       # DisplayState.spriteVariant: 1-based row of the sprite tables and CD resources; 0 is unset
PENDING_POSITION = 0x101                     # DisplayState.control.flags.pendingPlayerPos: the room start puts the player where the state has her
TITLE_SPAWN = ("bootReloadTitleTask", 0xCC)  # where the title's files are loaded after a restart and its own startup is about to be spawned
LAUNCH_SESSION = (0, 3, 1)                   # taskSpawn(bank 0, slot 3, 1): gameFlowLaunchSessionTask, told to keep the save images as they are
PLAYER_SLOT = 3                              # GAME_TASK_SLOT_PLAYER


def save_sections(trace) -> list[tuple[str, int, int]]:
    """(name, address, length) of each saved section's live copy, from the game's own table; its backup follows it."""
    table = trace.read(trace.address("_gMcSaveSections"), 12 * len(SECTIONS))
    rows = [struct.unpack_from("<3I", table, 12 * n) for n in range(len(SECTIONS))]
    return [(SECTIONS[n], buffer, length) for n, (buffer, length, _sectors) in enumerate(rows) if n]       # not the card's file header


SCRIPTED = (0x01, 0x66)                      # GameSession.eventState, .cutsceneHold: a scripted event has the game or the player
AUTO_NAME = re.compile(r"(\d{3,})_(.+)")     # a run's automatic states: their number in the run, then the room


def describe(state: dict) -> str:
    at = state.get("location", {})
    place = f"{state.get('room') or 'stage %s area %s' % (at.get('stage'), at.get('area'))}, view {at.get('view')}, variant {at.get('variant')}"
    return place + ("; taken during a scripted event" if state.get("scripted") else "") + f"; made {state.get('made', '?')}"


# A run is one playthrough: a directory of states. Those named `<number>_<room>` are its line - taken
# automatically at each room, in order, the last of them where the run stands. Anything else in the directory is a
# state kept on purpose under a label: it is not on the line, is never loaded automatically and has no number.

def runs() -> list[str]:
    """The runs there are, the one last added to first."""
    found = [d for d in STATES.iterdir() if d.is_dir()] if STATES.is_dir() else []
    return [d.name for d in sorted(found, key=lambda d: max((f.stat().st_mtime for f in d.glob("*.json")), default=d.stat().st_mtime), reverse=True)]


def line_of(run: str) -> list[tuple[int, object]]:
    """A run's automatic states in order, as (number, file)."""
    found = [(AUTO_NAME.fullmatch(f.stem), f) for f in (STATES / run).glob("*.json")]
    return sorted((int(match.group(1)), f) for match, f in found if match)


def kept_in(run: str) -> list:
    """A run's states kept on purpose: every file that is not on its line."""
    return sorted(f for f in (STATES / run).glob("*.json") if not AUTO_NAME.fullmatch(f.stem))


def good_name(text: str) -> bool:
    return bool(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", text))


def read_state(path) -> dict:
    import json
    return json.loads(path.read_text())


def capture_state(trace):
    """The game's state as it is now, as a state's dict - or a sentence saying why there is none to take."""
    import datetime
    if struct.unpack("<I", trace.read(0x80093800, 4))[0] != next((i for i, n in package_ids().items() if n == "gameplay"), -1):
        return "nothing to save: the game is not in play (gameplay is not in memory)"
    at_frame_start(trace)                            # the player's position is taken from her model: between frames, not while it moves
    if not trace.call("gameGetTaskSlot", PLAYER_SLOT):
        return "nothing to save: there is no player in the world just now"
    trace.call("playerCaptureSaveState")             # what a telephone does before saving: position, experience, BP
    parts = save_sections(trace)
    session = trace.address("_gGameSessionState")
    location = trace.read(session + 4, 8)
    trace.poke(parts[0][1] + 4, location)            # the save record's location is where the session is now, as a save here would have it
    scripted = any(trace.read(session + offset, 1)[0] for offset in SCRIPTED)
    return {"made": datetime.datetime.now().isoformat(timespec="seconds"), "location": location_of(location),
            "room": room_of(location[3], location[2]), "scripted": scripted,
            "sections": {n: trace.read(address, length).hex() for n, address, length in parts}}


def write_state(run: str, name: str, state: dict):
    import json
    (STATES / run).mkdir(parents=True, exist_ok=True)
    path = STATES / run / f"{name}.json"
    path.write_text(json.dumps(state, indent=1) + "\n")
    return path


def load_state(trace, path) -> str:
    """Restart the game into the state in `path`. Says what happened."""
    state, label = read_state(path), f"{path.parent.name}/{path.stem}"
    parts = save_sections(trace)
    if any(len(bytes.fromhex(state["sections"].get(n, ""))) != length for n, _a, length in parts):
        return f"{label} does not fit this game's save images: not loaded"
    site = trace.address(TITLE_SPAWN[0]) + TITLE_SPAWN[1]
    want = 0x0C000000 | ((trace.address("taskSpawnFromTable") >> 2) & 0x03FFFFFF)
    if struct.unpack("<I", trace.read(site, 4))[0] != want:
        return "the place where a restart starts the title is not where it is expected: not loaded"
    display = trace.address("gDisplayState")
    # 1. The game's own way out of a session, from anywhere: the main loop reinitialises everything.
    trace.poke(display + GAME_MODE, bytes([GAME_MODE_RESTART]))
    trace.say(f"restarting the game into {label}: {describe(state)}")
    # 2. It reloads the title's file - the font, its palette and the common sound bank are in it, and a restart
    #    clears them - and is about to spawn the title's startup. That is as early as a session can begin.
    trace.run_to(site)
    # 3. What a card load does around its read. Before it, the game resets the save data (`mcResetSaveData`),
    #    and that reset is also the only place that sets the display's sprite variant - which the restart
    #    cleared, and which every room's sprite tables, the stage music, the movies and the loading pictures are
    #    selected by. Without it the session runs with variant 0 and indexes those tables one row before their
    #    start. Then the state goes into the save images, live copy and backup, and the play clock's minute
    #    ticks are reset as after a successful read.
    trace.call("mcResetSaveData")
    for section, address, length in parts:
        data = bytes.fromhex(state["sections"][section])
        trace.poke(address, data)
        trace.poke(address + length, data)
    trace.call("playClockResetMinuteTicks")
    if not struct.unpack("<H", trace.read(display + SPRITE_VARIANT, 2))[0]:
        return "the game's reset left the sprite variant unset: not continuing (the session would read its sprite tables out of bounds)"
    # 4. In place of the title: the session launch task, told to keep the save images (what a card load ends in).
    #    One thing the title screen does for everything after it has to be done here: the font's palettes are
    #    uploaded by it, from the main program's own data, and the restart cleared them.
    trace.set_registers(pc=site + 8)
    trace.call("textUploadPalettes")
    trace.call("taskSpawn", *LAUNCH_SESSION, 0)
    # 5. The launch clears the "put the player where the save has her" request; it is made again where the room
    #    starts and reads it.
    trace.run_to("areaStartRoomRuntime")
    trace.poke(display + PENDING_POSITION, b"\x01")
    trace.note(f"state loaded: {label} ({describe(state)})", state=label)
    return f"loaded {label}"


# ---- a run kept up automatically: a state at every room

_auto: dict = {"run": None, "armed": False, "skip": False}       # the run being added to; a room has started; that start is a load's own


def on_room_start(trace, record, line):
    if _auto["skip"]:
        _auto["skip"] = False                        # the room a state load starts is the one already saved
    else:
        _auto["armed"] = True


def on_view_ready(trace, record, line):
    """The first view of a room is up: the room is complete as it is seen, and its state is taken."""
    if _auto["armed"] and _auto["run"]:
        _auto["armed"] = False
        trace.attend(auto_save)


def auto_save(trace):
    run = _auto["run"]
    if run is None:
        return
    state = capture_state(trace)
    if isinstance(state, str):
        trace.say(f"{run}: no state at this room ({state})")
        return
    line = line_of(run)
    number = line[-1][0] + 1 if line else 1
    path = write_state(run, f"{number:03d}_{state['room'] or 'room'}", state)
    trace.note(f"state saved: {run}/{path.stem} ({describe(state)})", state=f"{run}/{path.stem}")
    trace.say(f"{run}/{path.stem}" + ("   (during a scripted event)" if state["scripted"] else ""))


def auto_set(trace, run: str | None, loaded: bool = False) -> None:
    """Begin keeping `run` up, or with None stop. `loaded` says a state load has just been made: its own room
    start is not a new room."""
    _auto.update(run=run, armed=False, skip=loaded and run is not None)
    for name, function in (("room_start", on_room_start), ("view_ready", on_view_ready)):
        (trace.on if run else trace.off)(name, function)


def pick_run(trace, title: str, new: str | None = None) -> str | None:
    """Have a run picked from the list; with `new`, an entry of that wording stands for none of them ("")."""
    listed = [(run, f"{len(line_of(run))} on its line" + (f", the last {line_of(run)[-1][1].stem}" if line_of(run) else "") + (f"; {len(kept_in(run))} kept" if kept_in(run) else ""))
              for run in runs()]
    if new:
        listed.append((new, ""))
    if not listed:
        return None
    picked = trace.choose(title, listed)
    return "" if new and picked == new else picked


def ask_run_name(trace, prompt: str) -> str | None:
    name = (trace.ask(prompt + "\n> ") or "").strip()
    if not name:
        return None
    if not good_name(name) or (STATES / name).exists():
        trace.say(f"`{name}` cannot be a run's name (letters, digits, _ . - ; and not one there already is)")
        return None
    return name


def auto_on(trace) -> str:
    NEW = "a new run, from where the game is now"
    run = pick_run(trace, "continue which run?", NEW)
    if run is None:
        return "nothing done"
    if run == "":                                    # the one way of switching on that loads nothing
        name = ask_run_name(trace, "The new run's name?")
        if name is None:
            return "nothing done"
        state = capture_state(trace)
        if isinstance(state, str):
            return state
        path = write_state(name, f"001_{state['room'] or 'room'}", state)
        trace.note(f"state saved: {name}/{path.stem} ({describe(state)})", state=f"{name}/{path.stem}")
        auto_set(trace, name)
        return f"{name} begun at {path.stem}; a state is taken at every room from here"
    line = line_of(run)
    if not line:
        return f"{run} has no states on its line yet: nothing to continue from"
    options = [(f"{f.stem}", ("the latest; " if n == line[-1][0] else "") + describe(read_state(f))) for n, f in reversed(line)]
    picked = trace.choose(f"{run}: continue from which state?", options)
    if picked is None:
        return "nothing done"
    number, path = next((n, f) for n, f in line if f.stem == picked)
    later = [f for n, f in line if n > number]
    if later:
        how = trace.choose(f"{picked} is not the latest: {len(later)} come after it",
                           [("fork", f"a new run with the states up to {picked}; {run} stays as it is"),
                            ("roll back", f"delete the {len(later)} later states of {run} and go on from {picked}")])
        if how is None:
            return "nothing done"
        if how == "fork":
            name = ask_run_name(trace, f"The fork's name? (it gets the {len(line) - len(later)} states up to {picked})")
            if name is None:
                return "nothing done"
            import shutil
            (STATES / name).mkdir(parents=True)
            for n, f in line:
                if n <= number:
                    shutil.copy2(f, STATES / name / f.name)
            run, path = name, STATES / name / path.name
        else:
            if trace.choose(f"delete {len(later)} states of {run}, {later[0].stem} to {later[-1].stem}?", [("no", "leave them"), ("yes, delete them", "")]) != "yes, delete them":
                return "nothing done"
            for f in later:
                f.unlink()
    told = load_state(trace, path)
    if not told.startswith("loaded"):
        return told
    auto_set(trace, run, loaded=True)
    return f"{told}; a state is taken at every room from here, into {run}"


@command("state")
def state_command(trace, text):
    """state auto on | off, save [RUN LABEL], load [RUN [STATE]], list [RUN]   keep the saved game in runs of states, and restart into one"""
    word, _, rest = text.strip().partition(" ")
    words = rest.split()
    if word == "auto":
        if words[:1] == ["on"]:
            return auto_on(trace)
        if words[:1] == ["off"]:
            was = _auto["run"]
            auto_set(trace, None)
            return f"no longer adding to {was}" if was else "no run was being kept up"
        return f"adding a state to {_auto['run']} at every room" if _auto["run"] else "off (`state auto on` picks a run and goes on from it)"
    if word == "list" or not word:
        if words:
            if words[0] not in runs():
                return f"there is no run called {words[0]}"
            rows = [f"{f.stem:<40} {describe(read_state(f))}" for _n, f in line_of(words[0])] + [f"{f.stem:<40} kept: {describe(read_state(f))}" for f in kept_in(words[0])]
            return "\n".join(rows) or "empty"
        return "\n".join(f"{run:<24} {len(line_of(run))} on its line" + (f", the last {line_of(run)[-1][1].stem}" if line_of(run) else "") + (f"; {len(kept_in(run))} kept" if kept_in(run) else "")
                         for run in runs()) or f"no runs yet (they are kept in {STATES}; `state auto on` begins one)"
    if word == "save":                               # a state kept on purpose: into a run, under a label, off its line
        run = words[0] if words else pick_run(trace, "save into which run?")
        if not run or run not in runs():
            return f"there is no run called {run}" if run else "state save RUN LABEL (a run is begun with `state auto on`)"
        label = "_".join(words[1:]) if len(words) > 1 else (trace.ask(f"A label for this state in {run}?\n> ") or "").strip().replace(" ", "_")
        if not label:
            return "nothing done"
        if not good_name(label) or AUTO_NAME.fullmatch(label):
            return f"`{label}` cannot be a label (letters, digits, _ . - ; and not a number then a name, which is how a run's own states are called)"
        if (STATES / run / f"{label}.json").exists():
            return f"{run} already has a state called {label}"
        state = capture_state(trace)
        if isinstance(state, str):
            return state
        write_state(run, label, state)
        trace.note(f"state saved: {run}/{label} ({describe(state)})", state=f"{run}/{label}")
        return f"saved {run}/{label}: {describe(state)}"
    if word == "load":
        run = words[0] if words else pick_run(trace, "load from which run?")
        if not run or run not in runs():
            return f"there is no run called {run}" if run else "nothing done"
        files = [f for _n, f in reversed(line_of(run))] + kept_in(run)
        name = words[1] if len(words) > 1 else trace.choose(f"{run}: which state?", [(f.stem, ("" if AUTO_NAME.fullmatch(f.stem) else "kept: ") + describe(read_state(f))) for f in files])
        if name is None:
            return "nothing done"
        path = STATES / run / f"{name}.json"
        if not path.is_file():
            return f"{run} has no state called {name} (see `state list {run}`)"
        was = _auto["run"]
        auto_set(trace, None)                        # a load by hand leaves the line: the run is not added to from here
        return load_state(trace, path) + (f"; no longer adding to {was}" if was else "")
    return "state auto on | off, save [RUN LABEL], load [RUN [STATE]], list [RUN]"


def location_of(data: bytes) -> dict:
    return location(data)


# ---- the player's numbers
#
# They are plain values in the player's status (PlayerStatus, include/main/wipsys_types.h), which the game reads
# every frame, so they are read and written as memory: no call into the game, and so from wherever it is stopped.

PLAYER_NUMBERS = {      # name -> (offset in gPlayerStatus, struct format, offset of its maximum or None, what it is)
    "hp": (0x18, "<h", 0x1A, "hit points"),
    "mp": (0x1C, "<h", 0x1E, "Parasite Energy points"),
    "exp": (0x08, "<i", None, "unspent experience"),
    "bp": (0x0C, "<i", None, "bounty points"),
}


def player_number(which: str):
    offset, form, most, about = PLAYER_NUMBERS[which]
    size = struct.calcsize(form)

    def run(trace, text):
        status = trace.address("gPlayerStatus")
        now = struct.unpack(form, trace.read(status + offset, size))[0]
        limit = struct.unpack("<h", trace.read(status + most, 2))[0] if most is not None else None
        of = f" of {limit}" if limit is not None else ""
        word = text.strip().lower()
        if not word:
            return f"{which} is {now}{of}"
        if word == "max" and limit is not None:
            value = limit
        else:
            try:
                value = int(word, 0)
            except ValueError:
                return f"{which} [NUMBER" + (" | max]" if limit is not None else "]")
        low, high = (-(1 << (8 * size - 1)), (1 << (8 * size - 1)) - 1)
        if not low <= value <= high:
            return f"{which} holds {low}..{high}"
        trace.poke(status + offset, struct.pack(form, value))
        trace.note(f"{which} set to {value} (was {now})", player=which, value=value, was=now)
        return f"{which}: {now} -> {value}{of}" + (f"   (above its maximum of {limit}: the game may bring it down)" if limit is not None and value > limit else "")

    run.__doc__ = f"{which} [NUMBER{' | max' if most is not None else ''}]   show the player's {about}, or set them"
    return run


for _name in PLAYER_NUMBERS:
    command(_name)(player_number(_name))

