#!/usr/bin/env python3
"""Replay a game trace (tools/gametrace) into what was in memory, and when.

    trace_replay.py show TRACE                     a readable summary per session
    trace_replay.py check TRACE                    the trace's room loads against the residency graph
    trace_replay.py observe [TRACE ...] [--write]  observed residency, merged into tools/peassets/observed.tsv
                            [--fresh]              (with --write: forget the traces not observed now)

TRACE is a file, or a name under local/gametrace/; `observe` without one takes every trace there.

A trace is JSON lines as the tracer's renderers wrote them (tools/gametrace/hooks/pe2.py). The replay maps each
load to the asset table - `file` (a stage-zero id) or `stage`/`folder`/`index` to the file in asset_data.TREE,
and that to its chunks - and keeps, for everything a load puts somewhere, where it is:

    video memory   a rectangle per image column and palette, moved by the load's page and row offsets
                   (asset_residency.moved, after src/main/fs.c)
    main memory    an address range: a package at its load address for its length; each slot of a caption
                   bundle at the destination and length its directory gives (cap_bundle.py); a background's
                   stored bytes in the one image workspace (`gFsImageWorkspace`, src/main/fs.c:896-907)
    sound memory   a bank's samples: the address `sndLoadPlaceScriptSamples` (src/main/sndscript.c:746-829)
                   gives its type, for the wave bytes the bank's header states

**Residency ends by overwrite, and only so.** Whatever lands on any part of a resident thing ends it, from that
event on: a partly overwritten image or palette is not valid any more. In main memory a package also clears its
whole slot - from its load address up to the next load address any package has - so nothing whose destination
lies there survives it. A movie's workspace backup lands on (704,0)-(1023,255) (src/main/stream.c:1154-1165).
A sound bank also ends at its free (`sound_bank_free` naming its id; a free of an empty slot releases nothing).
Each ended thing records what displaced it and at which event.

What a load mode leaves out is the loader's (src/main/fs.c:673-699, 781-950): mode 1 only seeks, so nothing is
loaded; modes 2 and 5 stop the load at the first package or bundle chunk; 4 skips the background; 5 and 6 skip
sound banks; 3 relocates images. A text chunk is never loaded (fs.c:932-937).

A dropped session's records count like any other - what was loaded is loaded - and are marked dropped.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_residency as residency  # noqa: E402
import asset_schema  # noqa: E402
import cap_bundle  # noqa: E402
import spk_scripts  # noqa: E402

TRACES = ROOT / "local" / "gametrace"
OBSERVED = Path(__file__).with_name("observed.tsv")
RAW = ROOT / "assets" / "USA" / "raw"
COLUMNS = ("asset", "how", "trace", "session", "stage", "room", "room_index", "variant", "view", "where", "file", "page", "row", "mode", "note", "ended")
HEADER = """# Observed residency: what the traces of the running game (tools/gametrace) showed in memory, and in which context.
# Written by `tools/peassets/trace_replay.py observe --write`, which merges: a trace observed again replaces its
# own lines, the lines of other traces stay. One line per asset and distinct context. No game data: names, numbers.
#   how         video memory | main memory | sound memory | played | caption command | view
#   where       video memory: x,y,w,h of each rectangle as landed; main and sound memory: first-last address
#   file..mode  the load that brought it: disc file, page offset, row offset, load mode
#   view        only for what a view brings (a view's file, its palette), and for `view` lines of a room
#   ended       what displaced it in this context (residency ends by overwrite), empty while it stayed
# """ + "\t".join(COLUMNS) + "\n"

MODES = {0: "default", 1: "seek only", 2: "init sound", 3: "relocate images", 4: "skip background", 5: "images only", 6: "skip sound"}
STOP_AT_PACKAGE = (2, 5)             # CD_COMMAND_LOAD_INIT_SOUND, _IMAGES_ONLY: a package or bundle chunk ends the load
MOVIE_RECT = (704, 0, 320, 256)      # streamPrepareMovieWorkspace's two backup blocks, side by side
WORKSPACE = 0x801D7000               # gFsImageWorkspace (configs/USA/sym.main.imports.txt): where a background's bytes go
DESCRIPTORS, DESCRIPTOR_SIZE = 0x8007E0D8, 0x20        # gSndBankDescriptors (configs/USA/sym.main.txt)
SPU_BLOCK = 64
RESET_TASKS = ("bootColdStartTask", "bootReloadTitleTask")   # queued by _gameMainInitialize (src/main/gamemain.c:167)


def num(r: dict, *keys, default=0) -> int:
    """A record's number under the first of these keys it has: a rendered field, or a raw register as hex text."""
    for key in keys:
        if key in r and r[key] is not None:
            return r[key] if isinstance(r[key], int) else int(str(r[key]), 0)
    return default


EXPAND_REPEATS = 256      # a repeated run of up to this many lines is replayed line for line
STRIP_ROWS = 32          # an image is uploaded in strips of this many rows (fsUploadImageStrips)


def overlap(a, b) -> bool:
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


class World:
    """The asset table as the replay needs it: files by the ids a load names, and where things go."""

    def __init__(self):
        self.assets, self.embedded = asset_schema.load()
        self.tree = asset_schema.load_tree()
        self.zero, self.folders, self.room_of = {}, {}, {}
        for stage, body in self.tree.items():
            for key, node in (body.get("files") or {}).items():
                self.zero.setdefault(int(node["id"]), (f"{stage}/{key}", node))
            for fkey, folder in (body.get("folders") or {}).items():
                files = {int(n["id"]): (f"{stage}/{fkey}/{k}", n) for k, n in folder["files"].items()}
                self.folders[stage, int(folder["id"])] = files
                package = next((c for _p, n in files.values() for c in (n.get("chunks") or {}).values() if self.type(c) == "pe2pkg"), None)
                self.room_of[stage, int(folder["id"])] = package or fkey
        # A package's load address is in its chunk header, which the extraction keeps (assets/USA/stages.json);
        # the overlay manifest's is the fallback.
        self.address: dict[str, int] = {}
        listing = ROOT / "assets" / "USA" / "stages.json"
        if listing.is_file():
            def walk(node):
                for key, value in node.items():
                    if isinstance(value, dict):
                        if "load_addr" in value and key.endswith(".pe2pkg"):
                            self.address.setdefault(key[:-7], int(str(value["load_addr"]), 0))
                        walk(value)
            walk(json.loads(listing.read_text(encoding="utf-8")))
        try:
            import tomllib
            manifest = tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))
            for name, addr in residency.package_loads(manifest).items():
                self.address.setdefault(name, addr)
        except Exception as error:  # noqa: BLE001
            print(f"no overlay manifest: {error}", file=sys.stderr)
        self.slots = sorted(set(self.address.values()))
        self._banks: dict[str, dict | None] = {}
        self._bundles: dict[str, list] = {}
        self._filler: dict[str, int] = {}

    def type(self, key: str) -> str:
        return self.assets.get(key, {}).get("type", "")

    def filler(self, key: str) -> int:
        """How many rows at the bottom of an image are blank strips: whole upload strips of a single value.

        An image's stored height is a count of 32-row strips, and some carry a blank last strip that the next
        image of the same file is laid over (pe2img_300 and pe2img_301: each 96 rows stored, 64 drawn, 64 apart).
        Writing over that strip takes nothing from the image, so it is not part of what must stay intact. Read
        from the image's own pixels; an image that cannot be decoded has none."""
        if key not in self._filler:
            rows = 0
            try:
                import asset_decode
                rec = self.assets[key]
                image, _info, _colors = asset_decode.render_pe2img((RAW / "pe2img" / f"{key}.pe2img").read_bytes(), bpp=rec.get("bpp"))
                width, height = image.size
                while height - rows > STRIP_ROWS and len(image.crop((0, height - rows - STRIP_ROWS, width, height - rows)).getcolors(2) or ()) == 1:
                    rows += STRIP_ROWS
            except Exception:  # noqa: BLE001 - no pixels to look at: the whole rectangle counts
                rows = 0
            self._filler[key] = rows
        return self._filler[key]

    def file(self, record: dict):
        """(path, node) of the disc file a load names, or (text, None) when the disc has none."""
        if "file" in record:
            return self.zero.get(record["file"], (f"0/? id {record['file']}", None))
        if "stage" in record:
            return self.folders.get((record["stage"], record["folder"]), {}).get(record["index"], (f"{record['stage']}/? folder {record['folder']} index {record['index']}", None))
        return ("?", None)

    def length(self, package: str) -> int | None:
        path = ROOT / "assets" / "USA" / "pe2pkg" / f"{package}.pe2pkg"
        return path.stat().st_size if path.is_file() else None

    def slot_end(self, address: int) -> int:
        """The end of the slot a package at this address is loaded into: the next load address any package has."""
        return next((a for a in self.slots if a > address), address)

    def bank(self, key: str) -> dict | None:
        """A sound bank's id, wave bytes and script entries, from its stored bytes."""
        if key not in self._banks:
            path, found = RAW / "spk" / f"{key}.spk", None
            if path.is_file():
                data = path.read_bytes()
                if data[:4] == b"hSPK" and len(data) >= 0x14:
                    found = {"id": struct.unpack_from("<H", data, 4)[0], "wave": struct.unpack_from("<i", data, 0x10)[0], "scripts": spk_scripts.scripts(data)}
            self._banks[key] = found
        return self._banks[key]

    def bundle(self, key: str) -> list:
        if key not in self._bundles:
            path = RAW / "pe2cap2" / f"{key}.pe2cap2"
            self._bundles[key] = cap_bundle.slots(path.read_bytes()) if path.is_file() else []
        return self._bundles[key]

    def raw_size(self, kind: str, key: str) -> int | None:
        path = RAW / kind / f"{key}.{kind}"
        return path.stat().st_size if path.is_file() else None


def spu_address(kind: int, slot: int, wave: int, state: dict) -> int | None:
    """Where `sndLoadPlaceScriptSamples` puts a bank's samples (src/main/sndscript.c:746-829)."""
    size = (wave + SPU_BLOCK - 1) & ~(SPU_BLOCK - 1)
    if kind == 0:
        return 0x63810
    if kind == 1:
        state["type1"] = 0x63810 - size
        return state["type1"]
    if kind == 3:
        return 0x47010
    if kind == 4:
        # The first character bank starts the pool; the next follow the bank in the descriptor before theirs.
        if slot == 4:
            return 0x3D010
        before = state["character"].get(slot - 1)
        return before if before else None
    if kind == 5:
        return (state.get("type1") or 0x63810) - size
    if kind == 6:
        return 0x3D010 - size
    if kind in (2, 7):
        return 0x7B010 - size
    if kind == 14:
        return 0x6F810
    return None


class Replay:
    """One trace, replayed. `things` is everything that was put somewhere; `events` one readable line per record."""

    def __init__(self, path: Path, world: World):
        self.path, self.name, self.w = path, path.stem, world
        self.sessions: list[dict] = []
        self.events: list[list] = []          # [kind, text, extra]
        self.loads: list[dict] = []           # n, path, page, row, mode, only, by
        self.things: list[dict] = []          # asset, mem, where, load, n0, n1, by, view
        self.live: list[dict] = []
        self.places: list[list] = []          # [n, session, stage, area, room index, view, warp, variant, room, view file, background]
        self.marks: list[tuple] = []          # observations that are not residency: played, caption command, view
        self.rooms: list[dict] = []           # room loads, for `check`
        self.uploads: list[dict] = []
        self.unknown: list[str] = []
        self.movies: list[int] = []
        self.spu = {"type1": 0, "character": {}}
        self.loc = {"stage": 0, "area": 0, "room": 0, "view": 0, "warp": 0, "variant": 0, "name": "", "file": None, "bg": None}
        self.session = ""
        self.cap_file = 0
        self.loading: dict | None = None
        self.pending_bank = None
        self.pending_sound = None
        self.room: dict | None = None
        self.run()

    # ---- state

    def miss(self, n: int, text: str) -> None:
        self.unknown.append(f"event {n}: {text}")

    def place(self, n: int) -> None:
        row = [n, self.session, self.loc["stage"], self.loc["area"], self.loc["room"], self.loc["view"], self.loc["warp"], self.loc["variant"],
               self.loc["name"], self.loc["file"], self.loc["bg"]]
        if not self.places or self.places[-1][1:] != row[1:]:
            self.places.append(row)

    def end(self, thing: dict, n: int, by: str) -> None:
        thing["n1"], thing["by"] = n, by
        self.live.remove(thing)

    def put(self, n: int, asset: str, mem: str, where, load: dict | None, note: str = "", clears=None, part=None, lands=None) -> dict:
        """Land something; whatever it lands on stops being resident. `part` names a whole whose parts do not displace each other.
        `lands` is everything an image writes in video memory where that is more than `where`, the part of it that
        has to stay intact for it to be resident (see `filler`)."""
        label = asset + (f" of {load['path']}" if load else "")
        for other in list(self.live):
            if other["mem"] != mem or (part is not None and other.get("part") == part):
                continue
            if mem == "v":
                hit = any(overlap(a, b) for a in (lands or where) for b in other["where"])
            else:
                lo, hi = clears or where
                hit = other["where"][0] < hi and lo < other["where"][1]
            if hit:
                self.end(other, n, f"overwritten by {label}")
        thing = {"asset": asset, "mem": mem, "where": where, "load": load, "n0": n, "n1": -1, "by": "", "note": note, "part": part,
                 "view": self.loc["view"] if load and load.get("view") else 0}
        self.things.append(thing)
        self.live.append(thing)
        return thing

    def resident(self, asset: str, mem: str | None = None) -> dict | None:
        return next((t for t in reversed(self.live) if t["asset"] == asset and (mem is None or t["mem"] == mem)), None)

    def bank_by_id(self, ident: int, kind_only: bool = False) -> dict | None:
        for t in reversed(self.live):
            if t["mem"] == "s" and (t["bank"] >> 12 == ident >> 12 if kind_only else t["bank"] == ident):
                return t
        return None

    # ---- the records

    def run(self) -> None:
        records = []
        for line in self.path.read_text(encoding="utf-8").splitlines():
            try:
                record = json.loads(line)
            except ValueError:
                record = {"text": line}
            record = record if isinstance(record, dict) else {"text": line}
            # The tracer writes a run of lines that repeats only once, followed by a count (its `repeats`
            # filter): the `last` lines before the marker came `times` more times. A short run is put back
            # line for line. A long one - something said every frame - stays one record: by then it repeats
            # what has already been replayed, and a load can never be part of one.
            if record.get("event") == "repeated" and isinstance(record.get("last"), int) and isinstance(record.get("times"), int):
                if 0 < record["last"] <= len(records) and record["last"] * record["times"] <= EXPAND_REPEATS:
                    records += records[-record["last"]:] * record["times"]
                    continue
                record = {"text": f"the last {record['last']} records came {record['times']} more times", **record}
            records.append(record)
        self.records = records
        for n, r in enumerate(records):
            kind, text, extra = self.step(n, r)
            self.events.append([kind, text, extra])
            self.place(n)
        for s in self.sessions:
            s["last"] = next((t["first"] - 1 for t in self.sessions if t["first"] > s["first"]), len(records) - 1)

    def step(self, n: int, r: dict):
        name = r.get("event") or r.get("name")
        frm = (r.get("from") or "").split("+")[0]
        if "session" in r:
            self.session = r["session"]
            self.sessions.append({"name": r["session"], "number": r.get("number"), "started": r.get("started", ""), "dropped": False, "first": n})
            return "mark", f"session {r['session']} begins", None
        if "dropped" in r:
            for s in self.sessions:
                if s["number"] == r.get("number"):
                    s["dropped"] = True
            return "mark", f"session {r['dropped']} was dropped: its records stay, since what was loaded is loaded", None
        if name is None:
            return "raw", r.get("text") or json.dumps(r), None
        handler = getattr(self, "on_" + name, None)
        if handler:
            return handler(n, r, frm)
        args = " ".join(f"{k}={v}" for k, v in r.items() if re.fullmatch(r"[av]\d(:\d+)?", k))
        return "raw", f"{name}{' return' if r.get('phase') == 'exit' else ''} {args}".strip() + (f" · from {frm}" if frm else ""), None

    def on_cd_command(self, n, r, frm):
        if frm in RESET_TASKS:
            # The run was initialised again (src/main/gamemain.c:167): the player is nowhere until the next room.
            self.loc.update(stage=0, area=0, room=0, view=0, warp=0, variant=0, name="", file=None, bg=None)
            self.room = None
            return "mark", f"the run is initialised: {frm} queues the title's file", None
        what = r.get("command") or r.get("a0")
        return "dim", f"cd command {what} · from {frm}", None

    def on_file_load(self, n, r, frm):
        path, node = self.w.file(r)
        mode, page, row = r.get("mode", 0) & 0xFF, r.get("page_offset", 0), r.get("row_offset", 0)
        mode = mode if mode in MODES else 0          # an unknown policy is the default (fs.c:696)
        in_room = "stage" in r and r["stage"] == self.loc["stage"] and r["folder"] == self.loc["area"] * 100 + 1
        load = {"n": n, "path": path, "page": page, "row": row, "mode": mode, "only": None, "by": frm, "view": bool(in_room and r.get("index")),
                "id": len(self.loads), "assets": [], "cluts": []}
        self.loads.append(load)
        self.loading = load
        offsets = (f" at {page:+d}/{row:+d}" if page or row else "") + (f", mode {mode} ({MODES.get(mode, '?')})" if mode else "")
        if node is None:
            self.miss(n, f"a load of {path}, which the asset table does not have")
            return "load", f"load {path}{offsets}: not in the asset table", None
        if mode == 1:
            load["only"] = []
            return "load", f"seek to {path} (mode 1: nothing is loaded)", {"load": load["id"]}
        relocate = "RELOCATE" if mode == 3 else ""
        prints = defaultdict(list)
        for rect in residency.footprints(self.w.assets, node):
            prints[rect[0]].append(rect)
        stopped = False
        for key in (node.get("chunks") or {}).values():
            kind = self.w.type(key)
            if kind in ("pe2pkg", "pe2cap2") and mode in STOP_AT_PACKAGE:
                stopped = True
                break
            if kind in ("pe2img", "pe2clut"):
                where = [(*residency.moved(rect, page, row, relocate), rect[5], rect[6]) for rect in prints.get(key, ())]
                if where:
                    blank = self.w.filler(key) if kind == "pe2img" else 0
                    kept = [(x, y, w, h - blank) for x, y, w, h in where] if blank else where
                    self.put(n, key, "v", kept, load, lands=where, note=f"its last {blank} rows are blank filler, not counted as part of it" if blank else "")
                    load["assets"].append(key)
                    if kind == "pe2clut":
                        load["cluts"].append(key)
            elif kind == "pe2pkg":
                address, length = self.w.address.get(key), self.w.length(key)
                if address is None or length is None:
                    self.miss(n, f"package {key}: " + ("no load address" if address is None else "its length is unknown (the package is not extracted)") + ", so it is not placed")
                    continue
                self.put(n, key, "m", (address, address + length), load, clears=(address, max(address + length, self.w.slot_end(address))))
                load["assets"].append(key)
            elif kind == "pe2cap2":
                slots = self.w.bundle(key)
                if not slots:
                    self.miss(n, f"bundle {key}: not in the local extraction, so its slots are not placed")
                if slots:                                  # the bundle is its slots: they do not displace each other
                    self.put(n, key, "m", (min(s["dest"] for s in slots), max(s["dest"] + s["length"] for s in slots)), load, note="bundle", part=(n, key))
                    load["assets"].append(key)
                for s in slots:
                    self.put(n, f"{key}_slot_{s['slot']:02d}", "m", (s["dest"], s["dest"] + s["length"]), load, note=s["what"], part=(n, key))
            elif kind == "bs" and mode != 4:
                length = self.w.raw_size("bs", key)
                if length is None:
                    self.miss(n, f"background {key}: its stored length is unknown, so it is not placed")
                    continue
                self.put(n, key, "m", (WORKSPACE, WORKSPACE + length), load)
                load["assets"].append(key)
                if load["view"]:
                    self.loc["bg"] = key
        if stopped or mode in (4, 5, 6):
            load["only"] = list(load["assets"])
        if load["view"]:
            self.loc["file"] = r["index"]
            if not any(self.w.type(k) == "bs" for k in (node.get("chunks") or {}).values()):
                self.loc["bg"] = None
            if self.room is not None:
                self.room["views"].append({"n": n, "room": self.loc["room"], "view": self.loc["view"], "path": path})
            self.marks.append((n, self.loc["name"], "view", f"file {r['index']}" + (f", background {self.loc['bg']}" if self.loc["bg"] else ""), self.resident(self.loc["name"], "m")))
        elif "stage" in r and r.get("index") == 0 and mode == 0:
            self.room = {"n": n, "session": self.session, "name": self.w.room_of.get((r["stage"], r["folder"]), path), "stage": r["stage"], "area": r["folder"] // 100,
                         "variant": None, "files": [], "views": [], "path": path}
            self.rooms.append(self.room)
        elif self.room is not None and not self.room["views"] and "file" in r:
            self.room["files"].append((path, page, row, mode))
        held = ", ".join(load["assets"]) or "nothing placed"
        return "load", f"load {path}{offsets}: {held}", {"load": load["id"]}

    def on_file_loaded(self, n, r, frm):
        path, _node = self.w.file(r)
        self.loading = None
        return "dim", f"{path} is loaded" + (f": {', '.join(r['packages'])} in memory" if r.get("packages") else ""), None

    def on_image_chunk_upload(self, n, r, frm):
        if r.get("phase") == "exit":
            return "dim", f"upload done (status {r.get('status')})", None
        stored = (r["x"], r["y"], r["w"], r["h"])
        if self.loading is not None:
            # A palette chunk of the file being loaded: its stored header must be that of the file's next palette.
            key = self.loading["cluts"].pop(0) if self.loading["cluts"] else None
            rec = self.w.assets.get(key, {})
            ok = key is not None and (rec.get("x"), rec.get("y"), rec.get("w"), rec.get("h")) == stored
            self.uploads.append({"n": n, "path": self.loading["path"], "asset": key, "stored": stored, "ok": ok})
            return "dim", f"upload {key or 'a palette the file does not list'} stored for ({stored[0]}, {stored[1]}), {stored[2]} x {stored[3]}", None
        # Outside a load: the palette of the view whose file has just been loaded, out of the room's bundle - the slot
        # before that file's number (src/gameplay/view_image.c:45-60).
        bundle = next((t for t in reversed(self.live) if t["note"] == "bundle"), None)
        ahead = {"index": self.loc["file"]} if self.loc["file"] else None
        slot = None
        if bundle and ahead:
            slot = next((s for s in self.w.bundle(bundle["asset"]) if s["slot"] == ahead["index"] - 1 and s["kind"] == 2), None)
        if slot is None or (slot["x"], slot["y"], slot["w"], slot["h"]) != stored:
            self.uploads.append({"n": n, "path": "", "asset": None, "stored": stored, "ok": False})
            self.miss(n, f"an upload for ({stored[0]}, {stored[1]}), {stored[2]} x {stored[3]} from {frm} that no slot of the resident bundle accounts for; what it overwrote is not ended")
            return "load", f"upload for ({stored[0]}, {stored[1]}), {stored[2]} x {stored[3]} from {frm}: source not identified", None
        name = f"{bundle['asset']}_slot_{slot['slot']:02d}"
        load = {"n": n, "path": f"bundle:{name}", "page": 0, "row": 0, "mode": 0, "only": None, "by": frm, "view": True, "id": len(self.loads), "assets": [name], "cluts": []}
        self.loads.append(load)
        self.put(n, name, "v", [stored], load)
        self.uploads.append({"n": n, "path": load["path"], "asset": name, "stored": stored, "ok": True})
        return "load", f"upload {name}, the palette of view file {ahead['index']}, to ({stored[0]}, {stored[1]})", {"load": load["id"]}

    def on_movie_start(self, n, r, frm):
        gone = [t for t in self.live if t["mem"] == "v" and any(overlap(MOVIE_RECT, b) for b in t["where"])]
        for t in gone:
            self.end(t, n, "overwritten by a movie's workspace backup")
        self.movies.append(n)
        return "load", f"movie slot {num(r, 'slot', 'a0')} starts: (704,0)-(1023,255) is overwritten" + (f", ending {', '.join(t['asset'] for t in gone)}" if gone else ""), {"movie": 1}

    def on_image_move(self, n, r, frm):
        return "dim", f"MoveImage ({r['x']}, {r['y']}) {r['w']} x {r['h']} to ({r['to_x']}, {r['to_y']}) · from {frm}", None

    def on_sound_bank_load(self, n, r, frm):
        if r.get("phase") != "exit":
            self.pending_bank = r
            return "dim", f"bank {r['bank']} ({r['type']}) begins loading: {r['wave_bytes']} wave bytes", None
        begun, self.pending_bank = self.pending_bank, None
        if begun is None or not r.get("loaded"):
            return "load", "a bank was not loaded", None
        ident, wave = int(begun["bank"], 16), begun["wave_bytes"]
        node = self.w.file({k: v for k, v in self.records[self.loading["n"]].items() if k in ("file", "stage", "folder", "index")})[1] if self.loading else None
        keys = [k for k in ((node or {}).get("chunks") or {}).values() if self.w.type(k) == "spk" and (self.w.bank(k) or {}).get("id") == ident]
        asset = keys[0] if keys else f"bank {begun['bank']}"
        if not keys:
            self.miss(n, f"bank {begun['bank']} loaded, and the file being loaded lists no bank with that id")
        slot = (int(r["descriptor"], 16) - DESCRIPTORS) // DESCRIPTOR_SIZE
        ruled = spu_address(ident >> 12, slot, wave, self.spu)
        address = begun.get("wave_at") or ruled       # a newer trace records where the samples go; an older one leaves the loader's rule
        if begun.get("wave_at") and ruled and ruled != begun["wave_at"]:
            self.miss(n, f"{asset}: the trace puts its samples at {begun['wave_at']:#x}, the loader's rule as read here at {ruled:#x}; the trace is used")
        if ident >> 12 == 4:
            self.spu["character"][slot] = address + wave if address else None
        if address is None:
            self.miss(n, f"{asset} (bank {begun['bank']}, descriptor {slot}): the loader's rule gives no sample address, so it is not placed in sound memory")
            return "load", f"{asset} loaded as bank {begun['bank']}; sample address not established", None
        size = (wave + SPU_BLOCK - 1) & ~(SPU_BLOCK - 1)
        thing = self.put(n, asset, "s", (address, address + size), self.loading, note=f"bank {begun['bank']} descriptor {slot}")
        thing["bank"] = ident
        if self.loading:
            self.loading["assets"].append(asset)
        return "load", f"{asset} loaded as bank {begun['bank']} ({begun['type']}): samples at {address:#07x}-{address + size - 1:#07x}", None

    def on_sound_bank_free(self, n, r, frm):
        if not r.get("bank"):
            return "dim", f"free of an empty bank slot · from {frm}", None
        ident = int(r["bank"], 16)
        gone = [t for t in self.live if t["mem"] == "s" and t["bank"] == ident]
        for t in gone:
            self.end(t, n, f"freed by {frm}")
        return "load", f"bank {r['bank']} freed by {frm}" + (f": {', '.join(t['asset'] for t in gone)}" if gone else " (no resident bank has that id)"), None

    def on_sound_reset_for_area(self, n, r, frm):
        self.spu["type1"] = 0                       # gSndLoadType1SampleOrigin = 0 (src/main/sndscript.c:712)
        self.spu["character"] = {}
        return "mark", f"sound reset for stage {num(r, 'stage', 'a0')} area {num(r, 'area', 'a1')} · from {frm}", None

    def samples(self, ident: int) -> tuple[dict | None, int, list[str], str]:
        kind, entry = ident >> 28, ident & 0xFF
        bank = self.bank_by_id(0x1000, kind_only=True) if kind == 1 else self.bank_by_id((ident >> 16) & 0xFFFF)
        if bank is None:
            return None, entry, [], "no resident bank has that id"
        found = (self.w.bank(bank["asset"]) or {}).get("scripts")
        names = self.w.assets.get(bank["asset"], {}).get("samples") or []
        if not found or entry >= len(found["entries"]):
            return bank, entry, [], "the bank has no such entry"
        if found["entries"][entry] is None:
            return bank, entry, [], "the entry is empty"
        out = []
        for other, sample, program, layer in found["entries"][entry]:
            if sample is not None and sample < len(names):
                out.append(names[sample])
            elif other:
                lender = self.bank_by_id(0 if other == 0xFFFF else other)
                lent = (self.w.bank(lender["asset"]) or {}).get("scripts") if lender else None
                index = sum(lent["sizes"][:program]) + layer if lent and program < len(lent["sizes"]) else None
                theirs = self.w.assets.get(lender["asset"], {}).get("samples") or [] if lender else []
                if index is not None and index < len(lent["layers"]) and lent["layers"][index] < len(theirs):
                    out.append(theirs[lent["layers"][index]])
        return bank, entry, list(dict.fromkeys(out)), ""

    def on_sound_request(self, n, r, frm):
        if r.get("phase") == "exit":
            begun, self.pending_sound = self.pending_sound, None
            if begun and r.get("taken"):
                bank, entry, names, _why = begun
                self.marks.append((n, bank["asset"], "played", f"entry {entry:#04x}", bank))
                for name in names:
                    self.marks.append((n, name, "played", f"{bank['asset']} entry {entry:#04x}", bank))
            return "dim", "taken" if r.get("taken") else "rejected (bank not loaded, entry out of range or empty, or the event pool full)", None
        ident = int(r["sound"], 16)
        bank, entry, names, why = self.samples(ident)
        self.pending_sound = (bank, entry, names, why) if bank else None
        what = f"{bank['asset']} entry {entry:#04x}" + (f": {', '.join(names)}" if names else f" ({why or 'no sample resolved'})") if bank else why
        return "sound", f"sound {r['sound']} ({r['type']}) → {what} · from {frm}", {"play": names} if names else None

    def on_sound_stop(self, n, r, frm):
        return "dim", f"stop sound {r['sound']}" + (f" ({r['which']}, {r['how']})" if "which" in r else "") + f" · from {frm}", None

    def on_stage_sound_request(self, n, r, frm):
        return "sound", f"stage sound {r['sound']} · from {frm}", None

    def on_music_start(self, n, r, frm):
        if r.get("phase") == "exit":
            return "dim", f"music start returned {r.get('result', r.get('v0'))}", None
        return "sound", f"music sequence {num(r, 'sequence', 'a0')} requested · from {frm}", None

    def locate(self, r: dict) -> None:
        moved = (r["stage"], r["area"]) != (self.loc["stage"], self.loc["area"])
        self.loc.update(stage=r["stage"], area=r["area"], room=r["room"], view=r["view"], warp=r["warp"], variant=r["variant"])
        self.loc["name"] = self.w.room_of.get((r["stage"], r["area"] * 100 + 1), f"stage {r['stage']} area {r['area']}")
        if moved:
            self.loc["file"], self.loc["bg"] = None, None

    def on_companion_select(self, n, r, frm):
        if r.get("phase") == "exit":
            return "dim", f"companion family {num(r, 'family', 'v0')} (0 is none)", None
        self.locate(r)
        return "where", f"companion chosen for {self.loc['name']} (stage {r['stage']} area {r['area']}), room index {r['room']}, variant {r['variant']}", None

    def on_variant_set(self, n, r, frm):
        self.locate({**r, "variant": r["variant"]})
        if self.room is not None and self.room["variant"] is None:
            self.room["variant"] = r["variant"]
        return "where", f"variant {r['variant']} set for {self.loc['name']} (was {r['was']}) · from {frm}", None

    def on_view_load(self, n, r, frm):
        self.locate(r)
        if self.room is not None and self.room["variant"] is None:
            self.room["variant"] = r["variant"]
        return "where", f"view {r['view']} of {self.loc['name']}, room index {r['room']}, variant {r['variant']}", None

    def on_caption_file(self, n, r, frm):
        self.cap_file = num(r, "data_resource", "a0")
        return "dim", f"caption data resource {self.cap_file} selected · from {frm}", None

    def caption(self, n, r, frm, what):
        command = num(r, "command", "a0")
        bundle = next((t for t in reversed(self.live) if t["note"] == "bundle"), None)
        data = [s for s in self.w.bundle(bundle["asset"]) if s["kind"] == 3] if bundle else []
        if self.cap_file < len(data):
            name = f"{bundle['asset']}_slot_{data[self.cap_file]['slot']:02d}"
            self.marks.append((n, name, "caption command", f"command {command}", self.resident(name, "m")))
            return "where", f"{what} {command} of {name} · from {frm}", None
        return "where", f"{what} {command}: no resident bundle has data resource {self.cap_file} · from {frm}", None

    def on_caption_command(self, n, r, frm):
        return self.caption(n, r, frm, "caption command")

    def on_caption_command_transition(self, n, r, frm):
        return self.caption(n, r, frm, "caption command (with transition)")

    def on_caption_sequence(self, n, r, frm):
        return "dim", f"caption sequence command {num(r, 'command', 'a0')} playback {r.get('playback', r.get('a1'))} · from {frm}", None

    def on_flag_nibble(self, n, r, frm):
        return "where", f"flag {num(r, 'a0'):#x} = {num(r, 'a1')} · from {frm}", None

    def on_flag_set(self, n, r, frm):
        return "where", f"flag {r.get('flag')} = {r.get('value')} · from {frm}", None

    def on_flag_byte_set(self, n, r, frm):
        return "where", f"flags {r.get('flags')} = {r.get('value')} · from {frm}", None

    def on_stream_scene(self, n, r, frm):
        return "dim", f"stream scene group {r.get('group', r.get('a0'))} id {r.get('id', r.get('a1'))} · from {frm}", None

    def on_task_message(self, n, r, frm):
        if "message" in r:
            return "dim", f"message {r['message']}" + (f" ({', '.join(map(str, r.get('names') or []))})" if r.get("names") else "") + f" to {r.get('receiver')} · from {frm}", None
        return "dim", f"message {r.get('a1')} to task {r.get('a0')} ({r.get('a2')}, {r.get('a3')}) · from {frm}", None

    # ---- what was observed

    def session_of(self, n: int) -> str:
        s = next((s for s in reversed(self.sessions) if s["first"] <= n), None)
        return "" if s is None else s["name"] + (" (dropped)" if s["dropped"] else "")

    def observed(self) -> list[tuple]:
        """One line per asset and distinct context it was seen in."""
        lines = set()
        last = len(self.events)

        def context(place, view):
            return (self.session_of(place[0]), str(place[2] or ""), place[8] or "", str(place[4] or ""), str(place[7] or ""), str(view or ""))

        def where(t):
            if t["mem"] == "v":
                return ";".join(",".join(map(str, rect)) for rect in t["where"])
            return f"{t['where'][0]:#x}-{t['where'][1] - 1:#x}"

        how = {"v": "video memory", "m": "main memory", "s": "sound memory"}
        for t in self.things:
            end = t["n1"] if t["n1"] >= 0 else last
            spans = [p for i, p in enumerate(self.places) if p[0] < end and (i + 1 == len(self.places) or self.places[i + 1][0] > t["n0"])]
            load = t["load"] or {}
            seen = {}
            for p in spans:
                seen[context(p, t["view"])] = p        # the last place of each context decides whether it ended there
            for ctx, p in seen.items():
                final = t["n1"] >= 0 and p is spans[-1]
                lines.add((t["asset"], how[t["mem"]], self.name, *ctx, where(t), load.get("path", ""), str(load.get("page", "")), str(load.get("row", "")),
                           str(load.get("mode", "")), t["note"], f"{t['by']} at event {t['n1']}" if final else ""))
        for n, asset, what, note, thing in self.marks:
            p = next(p for p in reversed(self.places) if p[0] <= n)
            load = (thing or {}).get("load") or {}
            view = p[5] if what == "view" else 0
            lines.add((asset, what, self.name, *context(p, view), "", load.get("path", ""), str(load.get("page", "")), str(load.get("row", "")), str(load.get("mode", "")), note, ""))
        return sorted(lines)

    def page_data(self) -> dict:
        """What the asset map's playback needs, compact: a side file of the page."""
        return {
            "sessions": [[s["name"], s["number"], s["started"], int(s["dropped"]), s["first"], s["last"]] for s in self.sessions],
            "ev": [[k, t] + ([x] if x else []) for k, t, x in self.events],
            "loads": [[l["n"], l["path"], l["page"], l["row"], int(l["mode"] == 3), l["only"], l["mode"]] for l in self.loads],
            "things": [[t["asset"], t["mem"], t["load"]["id"] if t["load"] else -1, t["n0"], t["n1"], t["by"],
                        [list(r) for r in t["where"]] if t["mem"] == "v" else list(t["where"]), t["note"]] for t in self.things],
            "places": [p[:1] + p[2:] for p in self.places],
            "movies": self.movies, "unknown": self.unknown,
        }


def find(name: str) -> Path:
    for path in (Path(name), TRACES / name, TRACES / f"{name}.jsonl"):
        if path.is_file():
            return path
    raise SystemExit(f"no trace {name} (looked in {TRACES})")


def all_traces() -> list[Path]:
    return sorted(TRACES.glob("trace-*.jsonl")) if TRACES.is_dir() else []


def show(replay: Replay) -> None:
    print(f"{replay.name}: {len(replay.events)} records, {len(replay.loads)} loads, {len(replay.things)} things placed")
    for s in replay.sessions:
        inside = lambda n: s["first"] <= n <= s["last"]  # noqa: E731
        print(f"\nsession {s['number']} {s['name']}{' (dropped)' if s['dropped'] else ''}, records {s['first']}-{s['last']}, started {s['started']}")
        for room in replay.rooms:
            if inside(room["n"]):
                views = list(dict.fromkeys(f"{v['view']} of room index {v['room']} ({v['path'].rsplit('/', 1)[-1]})" for v in room["views"]))
                print(f"  in {room['name']} (stage {room['stage']} area {room['area']}), variant {room['variant']}: views {', '.join(views) or 'none loaded'}")
        for l in replay.loads:
            if inside(l["n"]):
                offsets = f" at {l['page']:+d}/{l['row']:+d}" if l["page"] or l["row"] else ""
                print(f"  {l['n']:>6} load {l['path']}{offsets}{(' mode %d' % l['mode']) if l['mode'] else ''}: {', '.join(l['assets']) or 'nothing placed'}")
        ended = [t for t in replay.things if t["n1"] >= 0 and inside(t["n1"])]
        print(f"  {len(ended)} things displaced, {sum(1 for m in replay.marks if inside(m[0]) and m[2] == 'played' and m[4] and m[1] == m[4]['asset'])} sound requests taken, "
              f"{sum(1 for m in replay.marks if inside(m[0]) and m[2] == 'caption command')} caption commands")
        for t in ended[:400]:
            print(f"  {t['n1']:>6} {t['asset']} ({'video' if t['mem'] == 'v' else 'main' if t['mem'] == 'm' else 'sound'} memory, since {t['n0']}) {t['by']}")
    left = [t for t in replay.things if t["n1"] < 0]
    print(f"\nresident at the end: {len(left)}: " + ", ".join(sorted({t['asset'] for t in left})))
    if replay.unknown:
        print(f"\nnot established ({len(replay.unknown)}):")
        for line in replay.unknown:
            print(f"  {line}")


def check(replay: Replay, db_path: Path) -> int:
    """The trace's room loads against what the residency graph predicts for the room and its variant."""
    import sqlite3
    if not db_path.is_file():
        print(f"{db_path} is missing: run asset_residency.py build first", file=sys.stderr)
        return 2
    db = sqlite3.connect(db_path)

    def members(loader, condition):
        return {(f, p, r) for f, p, r in db.execute("SELECT m.file, m.page, m.row FROM member m JOIN grp g ON g.id = m.grp WHERE g.loader = ? AND g.condition = ?", (loader, condition))}

    differences = agreed = 0
    for room in replay.rooms:
        name, variant = room["name"], room["variant"]
        print(f"\nroom load at record {room['n']} ({room['session']}): {name}, variant {variant}")
        base = members("gameplay", f"room: {name}")
        if (room["path"], 0, 0) in base:
            agreed += 1
            print(f"  agrees   base file {room['path']}")
        else:
            differences += 1
            print(f"  DIFFERS  base file {room['path']}: the graph has {sorted(base) or 'no room load'} for gameplay's \"room: {name}\"")
        want, got = members(name, f"variant {variant}"), {(f, p, r) for f, p, r, _m in room["files"]}
        for item in sorted(want & got):
            agreed += 1
            print(f"  agrees   variant {variant} file {item[0]} at {item[1]:+d}/{item[2]:+d}")
        for item in sorted(got - want):
            differences += 1
            other = [o for o in want if o[0] == item[0]]
            elsewhere = [r[0] for r in db.execute("SELECT DISTINCT g.loader || ' \"' || g.condition || '\"' FROM member m JOIN grp g ON g.id = m.grp WHERE m.file = ? AND g.kind != 'carrier' LIMIT 4", (item[0],))]
            print(f"  DIFFERS  the trace loaded {item[0]} at {item[1]:+d}/{item[2]:+d}; " + (f"the graph has it at {other[0][1]:+d}/{other[0][2]:+d}" if other
                  else f"the graph's variant {variant} does not have it" + (f" (it is in {', '.join(elsewhere)})" if elsewhere else "")))
        for item in sorted(want - got):
            if not any(o[0] == item[0] for o in got):
                differences += 1
                print(f"  DIFFERS  the graph predicts {item[0]} at {item[1]:+d}/{item[2]:+d}, which the trace did not see loaded")
        if not want and not got:
            print(f"  agrees   no files for variant {variant}, in the graph or in the trace")
        several = db.execute("SELECT COUNT(*) FROM grp WHERE loader = ? AND condition LIKE 'room % view %'", (name,)).fetchone()[0]
        for v in room["views"]:
            label = f"room {v['room']} view {v['view']}" if several else f"view {v['view']}"
            predicted = {m[0] for m in members(name, label)}
            if predicted == {v["path"]}:
                agreed += 1
                print(f"  agrees   {label}: {v['path']}")
            else:
                differences += 1
                print(f"  DIFFERS  {label} at record {v['n']}: the trace loaded {v['path']}, the graph predicts {sorted(predicted) or 'nothing'}")
    bad = [u for u in replay.uploads if not u["ok"]]
    print(f"\npalette uploads: {len(replay.uploads) - len(bad)} of {len(replay.uploads)} carry the stored header of the palette the replay took them for")
    for u in bad:
        print(f"  DIFFERS  record {u['n']}: an upload stored for {u['stored']} during {u['path'] or 'no load'}, taken for {u['asset']}")
    differences += len(bad)
    print(f"\n{replay.name}: {len(replay.rooms)} room loads; {agreed} agreements, {differences} differences")
    return 1 if differences else 0


def read_observed(path: Path = OBSERVED) -> list[tuple]:
    if not path.is_file():
        return []
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        cells = line.split("\t")
        if line and not line.startswith("#") and cells[0] != COLUMNS[0]:
            rows.append(tuple((cells + [""] * len(COLUMNS))[:len(COLUMNS)]))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("command", choices=("show", "check", "observe"))
    ap.add_argument("traces", nargs="*")
    ap.add_argument("--write", action="store_true", help="observe: merge into tools/peassets/observed.tsv")
    ap.add_argument("--fresh", action="store_true", help="observe --write: drop the lines of every trace not observed now, instead of keeping them")
    ap.add_argument("--out", type=Path, default=OBSERVED, help="observe --write: the file to merge into")
    ap.add_argument("--db", type=Path, default=residency.DB)
    args = ap.parse_args()
    paths = [find(t) for t in args.traces] or (all_traces() if args.command == "observe" else [])
    if not paths:
        raise SystemExit("name a trace" if args.command != "observe" else f"no traces in {TRACES}")
    world = World()
    status = 0
    if args.command == "observe":
        rows = []
        for path in paths:
            replay = Replay(path, world)
            found = replay.observed()
            rows += found
            print(f"{replay.name}: {len(found)} lines for {len({r[0] for r in found})} assets" + (f"; {len(replay.unknown)} things not established" if replay.unknown else ""))
        if args.write:
            names = {p.stem for p in paths}
            kept = [] if args.fresh else [r for r in read_observed(args.out) if r[2] not in names]
            merged = sorted(set(kept) | set(rows))
            args.out.write_text(HEADER + "".join("\t".join(r) + "\n" for r in merged), encoding="utf-8")
            print(f"wrote {args.out}: {len(merged)} lines ({len(kept)} kept from other traces)")
        return 0
    for path in paths:
        replay = Replay(path, world)
        if args.command == "show":
            show(replay)
        else:
            status |= check(replay, args.db)
    return status


if __name__ == "__main__":
    sys.exit(main())
