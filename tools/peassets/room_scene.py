#!/usr/bin/env python3
"""One JSON scene per room overlay, in world space, for the asset map's room viewer.

    room_scene.py ROOM                      print the room's scene
    room_scene.py --all --out DIR           one <room>.json each, plus _report.json / _report.txt

Everything is read, nothing is guessed. The route to each table is the one
gameplay takes: the stage's map overlay holds a directory per kind of table,
indexed by area (a room folder's disc id / 100), and `Gp_AreaTables` holds the
placement variants. From those roots the tool follows symbols into the room's
C initialisers:

    WorldCollisionRoomResources[room index]  grid, view boundaries, action triggers, occluders
    u8* [room index] + ViewCount[]           logical view -> view file
    ViewCamera[view file]                    world-to-camera matrix, negated origin, projection distance
    DirectionWarpEntry[warp slot]            arrival endpoints
    AreaVariant[variant]                     AreaPlacement + AreaResource tables
    AreaObjectRoom[area]                     placed objects (AreaObjectPlace) and how they spawn

Values come from the C initialiser (evaluated with `asset_journal.evaluate`);
the same record is then decoded from the extracted package at the symbol's
address and the two are compared, so a parser slip shows up as a mismatch in
the report instead of a wrong coordinate. Where a value is not written in C
(an expression the evaluator cannot fold, a table still in assembly) the
package bytes are used and the block says so. Collision geometry is an asset:
its arrays are generated includes, so grids are always read from the package
at the header's address (layout checked by tools/gen_collision_inc.py).

What cannot be read reliably is left out and listed under `unresolved`.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import struct
import sys
import tomllib
from collections import ChainMap, Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "tools"))
import area_variants  # noqa: E402
import asset_journal  # noqa: E402
import asset_residency  # noqa: E402
import asset_schema  # noqa: E402
import gen_collision_inc  # noqa: E402
import pkg_model  # noqa: E402
from asset_residency import GROUP_BASE, IDENTITY_MAP, IDENTITY_VIEWS  # noqa: E402

SCHEMA = 1
FLOOR_NORMAL_Y = -0xDDA          # include/gameplay/world_collision.h WORLD_COLLISION_FLOOR_NORMAL_Y; strictly below is floor
SCREEN = (320, 240)              # src/main/displaymode.c: draw offset is (width / 2, height / 2)
STAGE_NAMES = {1: "acropolis", 2: "dryfield", 3: "dryfield_night", 4: "mine_shelter", 5: "shelter_neo_ark"}
YAW_SPECIAL = {0x7800: "FACE_TRANSITION_POINT_ALT", 0x7FFE: "KEEP_FACING", 0x7FFF: "FACE_TRANSITION_POINT"}
TRIGGER_KIND = {1: "VIEW_BOUNDARY", 2: "FACING_QUAD", 3: "QUAD", 4: "NEAR_OR_FACING_QUAD"}
TRIGGER_ACTION = {0: "WARP", 1: "FACING", 2: "CAP", 3: "CALLBACK", 4: "CLEAR", 5: "ROOM", 6: "CAP_WEAPON", 0xFF: "CANCEL"}

CONVENTIONS = {
    "units": "game world units, integers exactly as stored (positions s16/s32; no fixed-point fraction)",
    "axes": "PS1 right-handed with +Y DOWN: floors have normals pointing -Y, heights above the floor are negative Y. X and Z span the ground plane.",
    "normals": "4096 = 1.0",
    "angles": "4096 units per full turn. An actor's forward axis is column 2 of RotMatrixY(yaw): forward = (sin(yaw), 0, cos(yaw)), "
              "so yaw 0 faces +Z and yaw 1024 faces +X (checked per room against warp triggers, see checks.warpYawAgreesWithTrigger).",
    "quads": "Trigger, occluder and grid quads are in STRIP order: the boundary loop is corners [1, 0, 2, 3] (gWorldCollisionFaceEdges, src/gameplay/item_use.c). "
             "A grid face whose 4th index is 65535 is a triangle. Trigger/occluder `corners` are already origin + offset, in world space.",
    "grid": "cell(x, z) = ((worldX + xBias) / cellSize, (worldZ + zBias) / cellSize); cells[x * cellCountZ + z] lists face indices. "
            "origin = (-xBias, -zBias). A face is floor when its normal's Y < -3546. A face whose first two vertex indices are both 0 is skipped by the sphere passes.",
    "camera": "ViewCamera (include/gameplay/view.h). matrix M is world->camera rotation (rows = camera right, down, forward in world axes; 4096 = 1.0). "
              "translation t is the NEGATED camera origin: p_cam = M * (p_world + t), camera position = -t (given as `position`). "
              "Projection: sx = 160 + screenDistance * Xc / Zc, sy = 120 + screenDistance * Yc / Zc on a 320x240 screen, camera +Y down, +Z forward "
              "(GTE H = screenDistance, geometry offset (0,0), draw offset = screen centre). fovY = 2*atan(120 / screenDistance).",
    "views": "View numbers are 1-based and LOGICAL; a room index's map sends a logical view to a view FILE, and the file number indexes the camera "
             "array (file - 1), the sprite table and the folder's file of that id (its `bs` chunk is the background).",
    "roomIndex": "A room overlay (an area) can hold several room indices (GameLocationKey.room, 1-based) sharing or swapping tables; blocks list the indices that use them.",
    "warps": "A WARP trigger's parameter0 is the destination area in the same stage; parameter1 = (departure slot in this area << 4) | arrival slot in the destination, both 1-based. "
             "The destination room index defaults to 1; the room's handler may rewrite area/room/warp (see warps.redirects).",
}


# --------------------------------------------------------------------------- C text

DEF = re.compile(r"^(?:static\s+)?(?:const\s+)?(?:struct\s+)?([A-Za-z_]\w*)\s*(\*+)?\s*(?:const\s+)?([A-Za-z_]\w*)\s*((?:\[[^\]\n]*\])*)\s*=\s*", re.M)
DESIGNATOR = re.compile(r"\s*((?:\.\w+|\[[^\]]*\])+)\s*=\s*")
ADDR_SUFFIX = re.compile(r"_([0-9A-F]{8})$")
SYMLINE = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*;", re.M)
CAST = re.compile(r"\(\s*(?:const\s+)?(?:struct\s+)?\w+\s*\*+\s*\)")


class Items(list):
    """A brace list of an initialiser; `desig[i]` is item i's designator text."""

    def __init__(self):
        super().__init__()
        self.desig: dict[int, str] = {}


def parse_braces(text: str, i: int) -> tuple[Items, int]:
    """Parse the brace list starting at text[i] == '{'; returns it and the index after its '}'."""
    out = Items()
    i += 1
    n = len(text)
    while i < n:
        while i < n and text[i] in " \t\r\n":
            i += 1
        c = text[i]
        if c == "}":
            return out, i + 1
        if c == ",":
            i += 1
            continue
        if c == "#":
            end = text.find("\n", i)
            line = text[i:end].strip()
            if line.startswith("#include"):
                out.append(line)
            i = end + 1
            continue
        m = DESIGNATOR.match(text, i)
        if m:
            out.desig[len(out)] = m[1]
            i = m.end()
            c = text[i]
        if c == "{":
            sub, i = parse_braces(text, i)
            out.append(sub)
            continue
        depth, start = 0, i
        while i < n:
            c = text[i]
            if c in "([":
                depth += 1
            elif c in ")]":
                depth -= 1
            elif depth == 0 and c in ",}":
                break
            i += 1
        out.append(text[start:i].strip())
    raise ValueError("unterminated initialiser")


class Def:
    __slots__ = ("symbol", "type", "stars", "dims_text", "path", "line", "owner", "text", "start", "_init")

    def __init__(self, symbol, type_, stars, dims_text, path, line, owner, text, start):
        self.symbol, self.type, self.stars, self.dims_text = symbol, type_, stars, dims_text
        self.path, self.line, self.owner, self.text, self.start = path, line, owner, text, start
        self._init = None

    @property
    def init(self):
        if self._init is None:
            if self.text[self.start] == "{":
                self._init = parse_braces(self.text, self.start)[0]
            else:
                end = self.text.find(";", self.start)
                self._init = self.text[self.start:end].strip()
        return self._init

    @property
    def is_array(self) -> bool:
        return bool(self.dims_text)

    def where(self) -> dict:
        return {"symbol": self.symbol, "file": self.path, "line": self.line}


class Unres(str):
    """A C expression the evaluator could not fold."""


# --------------------------------------------------------------------------- record layouts

SCALARS = {"s8": ("b", 1), "u8": ("B", 1), "s16": ("h", 2), "u16": ("H", 2), "s32": ("i", 4), "u32": ("I", 4), "ptr": ("I", 4), "pint": ("I", 4)}


def rec(*fields):
    return ("rec", list(fields))


def arr(t, n):
    return ("arr", t, n)


def union(t):
    return ("union", t)


def sizeof(t) -> int:
    if isinstance(t, str):
        return SCALARS[t][1]
    if t[0] == "arr":
        return sizeof(t[1]) * t[2]
    if t[0] == "union":
        return sizeof(t[1])
    off = 0
    for _name, ft in t[1]:
        a = alignof(ft)
        off = (off + a - 1) // a * a + sizeof(ft)
    a = alignof(t)
    return (off + a - 1) // a * a


def alignof(t) -> int:
    if isinstance(t, str):
        return SCALARS[t][1]
    if t[0] in ("arr", "union"):
        return alignof(t[1])
    return max(alignof(ft) for _n, ft in t[1])


def read_bytes(t, data: bytes, off: int):
    if isinstance(t, str):
        value = struct.unpack_from("<" + SCALARS[t][0], data, off)[0]
        if t == "ptr":
            return value or None
        if t == "pint":
            if value == 0:
                return None
            return value if 0x80000000 <= value < 0x80200000 else struct.unpack_from("<i", data, off)[0]
        return value
    if t[0] == "arr":
        size = sizeof(t[1])
        return [read_bytes(t[1], data, off + i * size) for i in range(t[2])]
    if t[0] == "union":
        return read_bytes(t[1], data, off)
    out, at = {}, 0
    for name, ft in t[1]:
        a = alignof(ft)
        at = (at + a - 1) // a * a
        out[name] = read_bytes(ft, data, off + at)
        at += sizeof(ft)
    return out


SVEC = rec(("vx", "s16"), ("vy", "s16"), ("vz", "s16"), ("pad", "s16"))
MATRIX = rec(("m", arr(arr("s16", 3), 3)), ("t", arr("s32", 3)))
VIEW_CAMERA = rec(("transform", MATRIX), ("screenDistance", "u32"))
TRIGGER = rec(("next", "ptr"), ("prevLink", "ptr"), ("coord", "ptr"), ("origin", SVEC), ("vertices", arr(SVEC, 4)), ("normal", SVEC),
              ("facingNormal", SVEC), ("radius", "u16"), ("control", "u16"), ("parameter0", "u8"), ("parameter1", "u8"), ("flags", "u8"), ("hit", "s8"))
OCCLUDER = rec(("next", "ptr"), ("prevLink", "ptr"), ("origin", SVEC), ("vertices", arr(SVEC, 4)), ("normal", SVEC), ("radius", "u16"),
               ("flags", "u8"), ("unknown_3B", "s8"))
GRID = rec(("viewCoord", "ptr"), ("normals", "ptr"), ("vertices", "ptr"), ("faces", "ptr"), ("cellFaceIds", "ptr"), ("xBias", "s32"), ("zBias", "s32"),
           ("cellCountX", "u16"), ("cellCountZ", "u16"), ("cellSize", "u16"), ("faceCount", "u16"))
ROOM_RESOURCES = rec(("grid", "ptr"), ("viewBoundaryTriggers", "ptr"), ("actionTriggers", "ptr"), ("occluders", "ptr"))
SPAWN_TRANSFORM = rec(("yaw", "s32"), ("x", "s32"), ("y", "s32"), ("z", "s32"))
WARP = rec(("player", SPAWN_TRANSFORM), ("unknown_10", arr("u8", 4)), ("companion", SPAWN_TRANSFORM), ("unknown_24", arr("u8", 4)),
           ("arrivalSound", "s32"), ("departureSound", "s32"), ("blockedSound", "s32"), ("initialView", "u8"), ("flags", "u8"), ("mapFlagId", "u16"))
PLACEMENT = rec(("entryId", "u8"), ("variant", "u8"), ("mode", "u16"), ("x", "s16"), ("y", "s16"), ("z", "s16"), ("yaw", "s16"), ("fileIdLow", "u8"),
                ("texturePageOffset", "s8"), ("clutRowOffset", "s8"), ("rowIndex", "u8"))
RESOURCE = rec(("entryId", "u16"), ("fileNumber", "s16"), ("fileGroupIndex", "u8"), ("taskIndex", "u8"), ("unknown_6", arr("u8", 2)), ("taskTable", "ptr"))
VARIANT = rec(("placements", "ptr"), ("resources", "ptr"))
AREA_RECORD = rec(("variants", "ptr"), ("savedState", "ptr"))
OBJECT_PLACE = rec(("flagIndex", "u16"), ("kind", "u16"), ("placeKeyHigh", "u16"), ("state", "u16"), ("x", "s16"), ("y", "s16"), ("z", "s16"), ("yaw", "u16"))
TASK_DESC = rec(("header", union(rec(("flags", "u16"), ("priority", "u16")))), ("callback", "ptr"), ("data", "pint"))
OBJECT_SPAWN = rec(("kind", "u16"), ("taskDesc", TASK_DESC))
OBJECT_ROOM = rec(("places", "pint"), ("spawns", "ptr"))
SURFACE = rec(("field_0", "u8"), ("probePassThrough", "u8"), ("weaponImpactEnabled", "u8"), ("suppressPushback", "u8"), ("footstepSounds", "ptr"))
FOOTSTEPS = rec(("walk", "s32"), ("run", "s32"), ("scriptedJump", "s32"))
GRID_FACE = rec(("v", arr("u16", 4)), ("normalIndex", "u16"), ("surfaceClass", "s16"))


def wrap(t: str, value: int) -> int:
    code, size = SCALARS[t]
    bits = size * 8
    value &= (1 << bits) - 1
    if code.islower() and value >= 1 << (bits - 1):
        value -= 1 << bits
    return value


class Cursor:
    def __init__(self, items):
        self.items, self.i = items, 0

    def peek(self):
        return self.items[self.i] if self.i < len(self.items) else None

    def take(self):
        item = self.peek()
        self.i += 1
        return item


def from_c(t, cur: Cursor, consts) -> object:
    """Consume one object of layout `t` from an initialiser cursor, with C's brace-elision rules."""
    if isinstance(t, str):
        item = cur.take()
        while isinstance(item, list):
            item = item[0] if item else None
        if item is None:
            return None if t in ("ptr", "pint") else 0
        if item.startswith("#include"):
            raise ValueError("generated include")
        text = CAST.sub("", item).strip()
        value = asset_journal.evaluate(text, consts)
        if t in ("ptr", "pint"):
            if value is not None:
                return None if value == 0 else (wrap("s32", value) if t == "pint" else Unres(item))
            return text.lstrip("&").strip()
        return Unres(item) if value is None else wrap(t, value)
    if t[0] == "union":
        if isinstance(cur.peek(), list):
            sub = cur.take()
            if sub.desig.get(0) == ".word":
                raise ValueError("union initialised through .word")
            return from_c(t[1], Cursor(sub), consts)
        return from_c(t[1], cur, consts)
    src = Cursor(cur.take()) if isinstance(cur.peek(), list) else cur
    if t[0] == "arr":
        out = []
        for index in range(t[2]):
            if isinstance(src.items, Items) and src.items.desig.get(src.i, "").startswith("["):
                raise ValueError("array designator")
            out.append(from_c(t[1], src, consts))
        return out
    # A record: positional, except that `.field = value` moves to the field of that name (the rest stay zero).
    names = [name for name, _ft in t[1]]
    out, at = {}, 0
    while at < len(names):
        designator = src.items.desig.get(src.i, "") if isinstance(src.items, Items) else ""
        if designator.startswith(".") and designator[1:] in names:
            at = names.index(designator[1:])
        elif src.peek() is None:
            break
        out[names[at]] = from_c(t[1][at][1], src, consts)
        at += 1
    for name, ft in t[1]:
        if name not in out:
            out[name] = zero(ft)
    return out


def zero(t):
    if isinstance(t, str):
        return None if t in ("ptr", "pint") else 0
    if t[0] == "union":
        return zero(t[1])
    if t[0] == "arr":
        return [zero(t[1]) for _ in range(t[2])]
    return {name: zero(ft) for name, ft in t[1]}


def v3(v: dict) -> list[int]:
    return [v["vx"], v["vy"], v["vz"]]


# --------------------------------------------------------------------------- the project

class Project:
    def __init__(self):
        self.manifest = tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text())
        self.load = {"rooms": self.manifest["rooms"]["load_addr"], "mapui": self.manifest["mapui"]["load_addr"]}
        self.assets, self.embedded = asset_schema.load()
        self.tree = asset_schema.load_tree()
        self.room_at: dict[tuple[int, int], str] = {}
        self.place_of: dict[str, tuple[int, int, int]] = {}
        for stage in range(1, 6):
            for name, node in self.tree[stage]["folders"].items():
                self.room_at[stage, node["id"] // 100] = name
                self.place_of[name] = (stage, node["id"] // 100, node["id"])
        self.common = {int(n["id"]): n for n in self.tree[0]["files"].values()}
        self.models_of: dict[str, list[str]] = defaultdict(list)
        self.model_at: dict[tuple[str, int], str] = {}
        for key, r in self.embedded.items():
            if r.get("type") == "model":
                package = r.get("source", "").removesuffix(".pe2pkg")
                self.models_of[package].append(key)
                self.model_at[package, r["vram"]] = key
        self.defs: dict[str, list[Def]] = defaultdict(list)
        self.texts: dict[str, str] = {}
        self.owner_files: dict[str, list[str]] = defaultdict(list)
        self.file_consts: dict[str, dict] = {}
        self._pkg: dict[str, bytes | None] = {}
        self._sym: dict[str, dict[str, int]] = {}
        self._actor_names: dict[str, str] | None = None
        self._setters = None
        self._residency_maps = None
        self._gameplay_scanned = False
        self.globals = {"NULL": 0}
        self._load_constants()
        for base in ("src/rooms", "src/mapui"):
            for path in sorted((ROOT / base).glob("*/*.c")):
                self._scan(path, path.parent.name)
        for name in ("cd_loading.c",):
            self._scan(ROOT / "src/gameplay" / name, "gameplay")
        self.stages = self._stage_directories()

    # ---- constants

    def _collect(self, text: str, base) -> dict[str, int]:
        local: dict[str, int] = {}
        env = ChainMap(local, base)
        for m in asset_journal.DEFINE.finditer(text):
            value = asset_journal.evaluate(m[2], env)
            if value is not None:
                local[m[1]] = value
        for m in asset_journal.ENUM.finditer(text):
            nxt: int | None = 0
            for piece in asset_journal.split_args(m[1]):
                item = asset_journal.ITEM_WHOLE.match(piece)
                if item is None:
                    continue
                value = asset_journal.evaluate(item[2], env) if item[2] else nxt
                if value is None:
                    nxt = None
                    continue
                local[item[1]] = value
                nxt = value + 1
        return local

    def _load_constants(self) -> None:
        headers = [p for p in sorted((ROOT / "include").rglob("*.h")) if "psyq" not in p.parts]
        texts = [asset_journal.code_of(p) for p in headers]
        for _ in range(3):
            seen: dict[str, set[int]] = defaultdict(set)
            for text in texts:
                for name, value in self._collect(text, self.globals).items():
                    seen[name].add(value)
            self.globals.update({name: next(iter(v)) for name, v in seen.items() if len(v) == 1})

    def consts_for(self, rel: str):
        owner_dir = str(Path(rel).parent)
        chain = [self.file_consts.get(rel, {})]
        chain += [self.file_consts[f] for f in self.owner_files.get(owner_dir, ()) if f != rel]
        return ChainMap(*chain, self.globals)

    # ---- sources

    def _scan(self, path: Path, owner: str) -> None:
        rel = path.relative_to(ROOT).as_posix()
        text = asset_journal.code_of(path)
        self.texts[rel] = text
        self.owner_files[str(Path(rel).parent)].append(rel)
        self.file_consts[rel] = self._collect(text, self.globals)
        for h in sorted(path.parent.glob("*.h")):
            hrel = h.relative_to(ROOT).as_posix()
            if hrel not in self.file_consts:
                self.file_consts[hrel] = self._collect(asset_journal.code_of(h), self.globals)
                self.owner_files[str(Path(rel).parent)].append(hrel)
        for m in DEF.finditer(text):
            if m[1] in ("return", "typedef", "extern", "enum", "else", "case", "goto"):
                continue
            line = text.count("\n", 0, m.start()) + 1
            self.defs[m[3]].append(Def(m[3], m[1], m[2] or "", m[4], rel, line, owner, text, m.end()))

    def find(self, symbol: str, owner: str | None = None) -> Def | None:
        found = self.defs.get(symbol)
        if not found and not self._gameplay_scanned and not symbol.startswith(("D_map_", "_g")) and asset_journal.OWNER.match(symbol) is None:
            # A table gameplay owns (the Dryfield stages' object lists live in src/gameplay/item_placement.c).
            self._gameplay_scanned = True
            for path in sorted((ROOT / "src/gameplay").glob("*.c")):
                if path.name != "cd_loading.c":
                    self._scan(path, "gameplay")
            found = self.defs.get(symbol)
        if not found:
            return None
        return next((d for d in found if d.owner == owner), found[0])

    def owner_of(self, symbol: str) -> str | None:
        d = self.find(symbol)
        if d:
            return d.owner
        m = asset_journal.OWNER.match(symbol)
        return m[1] if m else None

    def package(self, owner: str) -> bytes | None:
        if owner not in self._pkg:
            path = ROOT / "assets/USA/pe2pkg" / f"{owner}.pe2pkg"
            self._pkg[owner] = path.read_bytes() if path.exists() else None
        return self._pkg[owner]

    def family(self, owner: str) -> str | None:
        return "mapui" if owner.startswith("map_") else ("rooms" if owner in self.place_of else None)

    def symbols(self, owner: str) -> dict[str, int]:
        if owner not in self._sym:
            out = {}
            family = self.family(owner)
            path = ROOT / "configs/USA/sym" / str(family) / f"{owner}.txt"
            if path.exists():
                out = {m[1]: int(m[2], 16) for m in SYMLINE.finditer(path.read_text())}
            self._sym[owner] = out
        return self._sym[owner]

    def address(self, symbol: str, owner: str | None) -> int | None:
        m = ADDR_SUFFIX.search(symbol)
        if m and symbol.startswith("D_"):
            return int(m[1], 16)
        return self.symbols(owner).get(symbol) if owner else None

    def actor_of(self, symbol: str) -> str | None:
        """The overlay a task table belongs to: by its D_ name, or the actor symbol map that lists it."""
        m = asset_journal.OWNER.match(symbol)
        if m:
            return m[1]
        if self._actor_names is None:
            self._actor_names = {}
            for path in sorted((ROOT / "configs/USA/sym/actors").glob("*.txt")):
                for s in SYMLINE.finditer(path.read_text()):
                    self._actor_names.setdefault(s[1], path.stem)
        return self._actor_names.get(symbol)

    def _actor_symbols(self, actor: str) -> dict[str, int]:
        if actor not in self._sym:
            path = ROOT / "configs/USA/sym/actors" / f"{actor}.txt"
            self._sym[actor] = {m[1]: int(m[2], 16) for m in SYMLINE.finditer(path.read_text())} if path.exists() else {}
        return self._sym[actor]

    # ---- stage directories

    def _stage_directories(self) -> dict[int, dict]:
        kinds = {("WorldCollisionRoomResources", "*"): "resources", ("SpriteView", "*"): "sprites", ("DirectionWarpEntry", "*"): "warps",
                 ("ViewCount", "*"): "counts", ("ViewCamera", "*"): "cameras", ("u8", "**"): "maps", ("AreaObjectRoom", ""): "objects",
                 ("WorldCollisionSurfaceProperties", "**"): "surfaces"}
        out: dict[int, dict] = {}
        by_owner: dict[str, dict] = defaultdict(dict)
        for found in self.defs.values():
            for d in found:
                kind = kinds.get((d.type, d.stars))
                if kind and d.owner.startswith("map_") and d.is_array:
                    by_owner[d.owner].setdefault(kind, []).append(d)
        for owner, tables in by_owner.items():
            cameras = tables.get("cameras", [])
            votes = Counter()
            if len(cameras) == 1:
                for item in cameras[0].init:
                    room = self.owner_of(item.lstrip("&")) if isinstance(item, str) else None
                    if room in self.place_of:
                        votes[self.place_of[room][0]] += 1
            if not votes:
                continue
            stage = votes.most_common(1)[0][0]
            out[stage] = {"map": owner, **{k: (v[0] if len(v) == 1 else None) for k, v in tables.items()},
                          "ambiguous": sorted(k for k, v in tables.items() if len(v) != 1)}
        tables = self.find("Gp_AreaTables")
        if tables:
            for stage, name in enumerate(tables.init):
                if stage in out and isinstance(name, str) and name != "NULL":
                    out[stage]["areas"] = self.find(name)
        return out

    # ---- variant setters

    def setters(self) -> dict:
        """Who stores a placement variant: AreaApplyRec lists, areaSetPlacementVariant calls and direct writes."""
        if self._setters is not None:
            return self._setters
        by_room: dict[str, list[dict]] = defaultdict(list)
        by_actor: dict[str, list[dict]] = defaultdict(list)
        for overlay, path, line, symbol, room, variant, policy in area_variants.records():
            if isinstance(room, str):
                by_room[room].append({"kind": "AreaApplyRec", "variant": variant, "by": overlay, "file": str(path), "line": line, "list": symbol,
                                      "policy": area_variants.modes(policy)})
        call = re.compile(r"\bareaSetPlacementVariant\s*\(\s*&?\s*([^,]+?)\s*,\s*([^,]+?)\s*,")
        write = re.compile(r"\bloc\.variant\s*=(?!=)\s*([^;]+);")
        for path in sorted((ROOT / "src").rglob("*.c")):
            raw = path.read_text(encoding="utf-8", errors="replace")
            if "areaSetPlacementVariant" not in raw and "loc.variant" not in raw:
                continue
            rel = path.relative_to(ROOT).as_posix()
            if rel.startswith("src/gameplay/") or rel.startswith("src/main/"):
                continue        # the generic appliers (AreaApplyRec, save restore, new game), not a room's own setter
            overlay = path.parent.name
            text = asset_journal.code_of(path)
            consts = ChainMap(self._collect(text, self.globals), self.globals)
            lines = text.splitlines()
            for n, line in enumerate(lines, 1):
                target, value, kind = None, None, None
                m = call.search(line)
                if m:
                    kind, value = "areaSetPlacementVariant", m[2]
                    key = m[1]
                    if "location.loc" in key:
                        target = "current"
                    else:
                        stage = area = None
                        for back in lines[max(0, n - 30):n]:
                            s = re.search(r"\b" + re.escape(key) + r"\.stage\s*=\s*([^;]+);", back)
                            a = re.search(r"\b" + re.escape(key) + r"\.area\s*=\s*([^;]+);", back)
                            stage = asset_journal.evaluate(s[1], consts) if s else stage
                            area = asset_journal.evaluate(a[1], consts) if a else area
                        target = self.room_at.get((stage, area))
                else:
                    m = write.search(line)
                    if m and "==" not in line.split("loc.variant")[0][-3:]:
                        kind, value, target = "direct write of location.loc.variant", m[1], "current"
                if kind is None:
                    continue
                number = asset_journal.evaluate(value, consts)
                entry = {"kind": kind, "variant": number if number is not None else value.strip(), "by": overlay, "file": rel, "line": n}
                if target == "current":
                    (by_room if overlay in self.place_of else by_actor)[overlay].append(entry)
                elif target:
                    by_room[target].append(entry)
                else:
                    by_room["?"].append(entry)
        self._setters = {"room": by_room, "actor": by_actor}
        return self._setters

    def residency_maps(self) -> dict:
        if self._residency_maps is None:
            files = sorted(p for p in (ROOT / "src/rooms").rglob("*.c"))
            packages = {k for k, r in self.assets.items() if r["type"] == "pe2pkg"}
            self._residency_maps = asset_residency.view_maps(files, packages)
        return self._residency_maps


# --------------------------------------------------------------------------- one room

class Scene:
    def __init__(self, project: Project, room: str):
        self.p, self.room = project, room
        self.stage, self.area, self.folder_id = project.place_of[room]
        self.unresolved: list[dict] = []
        self.sources: dict[str, list[dict]] = defaultdict(list)
        self.notes: list[str] = []
        self.mismatches: list[str] = []
        self.byte_filled = 0
        self.byte_checked = 0

    # ---- helpers

    def miss(self, block: str, what: str, reason: str) -> None:
        entry = {"block": block, "what": what, "reason": reason}
        if entry not in self.unresolved:
            self.unresolved.append(entry)

    def cite(self, block: str, d: Def | dict) -> dict:
        where = d.where() if isinstance(d, Def) else d
        if where not in self.sources[block]:
            self.sources[block].append(where)
        return where

    @staticmethod
    def ref(text) -> tuple[str | None, int]:
        """A pointer initialiser as (symbol, element index)."""
        if not isinstance(text, str) or isinstance(text, Unres):
            return None, 0
        m = re.fullmatch(r"&?\s*(\w+)\s*(?:\[\s*(\w+)\s*\])?", text.strip())
        if m is None:
            m = re.fullmatch(r"(\w+)\s*\+\s*(\w+)", text.strip())
        if m is None:
            return None, 0
        index = asset_journal.evaluate(m[2], {}) if m[2] else 0
        return m[1], index or 0

    def _merge(self, c, b, path: str, t):
        """C value checked against (and completed from) the package bytes."""
        if isinstance(t, str):
            if b is None and c is None:
                return c
            if isinstance(c, Unres):
                if t in ("ptr", "pint") or b is None:
                    return c
                self.byte_filled += 1
                return b
            if t in ("ptr", "pint") and isinstance(c, str):
                sym, index = self.ref(c)
                if sym is None or index or not isinstance(b, int):
                    if b is None:
                        self.mismatches.append(f"{path}: C names {c}, package holds NULL")
                    return c
                addr = self._addr_any(sym)
                if addr is not None and addr != b:
                    self.mismatches.append(f"{path}: C names {c} (0x{addr:X}), package holds 0x{b:X}")
                return c
            if c != b:
                self.mismatches.append(f"{path}: C gives {c}, package holds {b}")
                return b
            return c
        if t[0] == "union":
            return self._merge(c, b, path, t[1])
        if t[0] == "arr":
            return [self._merge(ci, bi, f"{path}[{i}]", t[1]) for i, (ci, bi) in enumerate(zip(c, b))]
        return {name: self._merge(c[name], b[name], f"{path}.{name}", ft) for name, ft in t[1]}

    def _addr_any(self, symbol: str) -> int | None:
        m = ADDR_SUFFIX.search(symbol)
        if m and symbol.startswith("D_"):
            return int(m[1], 16)
        for owner in (self.room, self.p.stages.get(self.stage, {}).get("map")):
            if owner and symbol in self.p.symbols(owner):
                return self.p.symbols(owner)[symbol]
        return None

    def table(self, block: str, symbol: str, layout, *, count: int | None = None, start: int = 0, stop=None):
        """Rows of the table `symbol`: (rows, Def or None). `stop(row)` ends a list on its last record."""
        d = self.p.find(symbol, self.room) or self.p.find(symbol, self.p.stages.get(self.stage, {}).get("map"))
        owner = d.owner if d else (self.p.owner_of(symbol) or self.room)
        address = self.p.address(symbol, owner)
        data = self.p.package(owner) if self.p.family(owner) else None
        base = self.p.load.get(self.p.family(owner) or "", 0)
        size = sizeof(layout)
        rows = None
        declared = None
        if d is not None:
            consts = self.p.consts_for(d.path)
            n = 1
            if d.is_array:
                first = re.match(r"\[([^\]]*)\]", d.dims_text)[1].strip()
                n = asset_journal.evaluate(first, consts) if first else len(d.init)
            try:
                if n is None:
                    raise ValueError(f"array length {d.dims_text} not evaluable")
                declared = n
                init = d.init
                if not isinstance(init, list):
                    raise ValueError("not a brace initialiser")
                rows = from_c(arr(layout, n), Cursor([init]), consts) if d.is_array else [from_c(layout, Cursor([init]), consts)]
            except (ValueError, IndexError, AttributeError, TypeError) as error:
                self.notes.append(f"{block}: {symbol} read from the package, its C initialiser was not parsed ({error})")
                rows = None
            self.cite(block, d)
        if data is not None and address is not None and 0 <= address - base < len(data):
            off = address - base
            if rows is not None:
                fits = off + len(rows) * size <= len(data)
                if fits:
                    raw = [read_bytes(layout, data, off + i * size) for i in range(len(rows))]
                    self.byte_checked += len(rows)
                    rows = [self._merge(c, b, f"{symbol}[{i}]", layout) for i, (c, b) in enumerate(zip(rows, raw))]
            else:
                rows, i = [], 0
                if count is None and declared is not None:
                    count = declared - start
                limit = count if count is not None else (len(data) - off) // size
                while i < limit and off + (start + i + 1) * size <= len(data):
                    row = read_bytes(layout, data, off + (start + i) * size)
                    rows.append(row)
                    i += 1
                    if stop and stop(row):
                        break
                if count is None and stop is None:
                    self.miss(block, symbol, "not defined in C and its length is not stored; nothing read")
                    return None, d
                if d is None:
                    self.cite(block, {"symbol": symbol, "file": f"assets/USA/pe2pkg/{owner}.pe2pkg", "line": None, "offset": f"0x{off:X}"})
                return rows, d
        if rows is None:
            self.miss(block, symbol, "no C definition found and no package bytes at its address")
            return None, d
        rows = rows[start:]
        if stop:
            for i, row in enumerate(rows):
                if stop(row):
                    rows = rows[:i + 1]
                    break
        if count is not None:
            rows = rows[:count]
        return rows, d

    def directory_entry(self, kind: str, block: str):
        """The stage directory's entry for this area: (text or None, directory Def)."""
        stage = self.p.stages.get(self.stage)
        if stage is None:
            self.miss(block, "stage directory", f"no map overlay identified for stage {self.stage}")
            return None, None
        d = stage.get(kind)
        if d is None:
            self.miss(block, f"{kind} directory", f"map overlay {stage['map']} declares {'several' if kind in stage['ambiguous'] else 'no'} such table")
            return None, None
        index = self.area if kind == "objects" else self.area - 1
        init = d.init
        if index >= len(init):
            self.miss(block, d.symbol, f"the stage directory has {len(init)} entries; area {self.area} is past its end, so gameplay has no table for this room")
            return None, d
        self.cite(block, {**d.where(), "index": index})
        return init[index], d

    # ---- blocks

    def build(self) -> dict:
        scene: dict = {"schema": SCHEMA, "room": self.room, "stage": self.stage, "stageName": STAGE_NAMES[self.stage], "area": self.area,
                       "folderId": self.folder_id, "package": self.room, "mapOverlay": self.p.stages.get(self.stage, {}).get("map"),
                       "conventions": CONVENTIONS}
        owner_check, _ = self.directory_entry("cameras", "views")
        if isinstance(owner_check, str) and owner_check != "NULL":
            named = self.p.owner_of(owner_check.lstrip("&"))
            if named != self.room:
                self.miss("views", owner_check, f"the stage directory's entry for area {self.area} belongs to {named}, not {self.room}")
        self.folder = {int(n["id"]): n for n in self.p.tree[self.stage]["folders"][self.room]["files"].values()}
        rooms = self.room_resources()
        scene["rooms"] = rooms
        scene["grids"] = self.grids(rooms)
        scene["surfaceClasses"] = self.surfaces()
        scene["boundaries"] = self.trigger_lists(rooms, "viewBoundaryTriggers", "boundaries")
        scene["triggers"] = self.trigger_lists(rooms, "actionTriggers", "triggers")
        scene["occluders"] = self.occluders(rooms)
        scene["warps"] = self.warps(scene["triggers"])
        scene["views"] = self.views(rooms, scene["boundaries"], scene["warps"])
        scene["placements"] = self.placements()
        scene["objects"] = self.objects()
        scene["sources"] = dict(self.sources)
        scene["checks"] = self.checks(scene)
        scene["notes"] = self.notes
        scene["unresolved"] = self.unresolved
        scene["verification"] = {"recordsComparedWithPackage": self.byte_checked, "valuesTakenFromPackage": self.byte_filled, "mismatches": self.mismatches}
        return scene

    def room_resources(self) -> list[dict]:
        entry, _ = self.directory_entry("resources", "rooms")
        rows = []
        if isinstance(entry, str) and entry != "NULL":
            symbol, start = self.ref(entry)
            got, d = self.table("rooms", symbol, ROOM_RESOURCES, start=start) if symbol else (None, None)
            for i, row in enumerate(got or []):
                rows.append({"roomIndex": i + 1, "table": symbol, "grid": row["grid"], "viewBoundaryTriggers": row["viewBoundaryTriggers"],
                             "actionTriggers": row["actionTriggers"], "occluders": row["occluders"]})
        maps_entry, _ = self.directory_entry("maps", "views")
        counts_entry, _ = self.directory_entry("counts", "views")
        names, counts = [], []
        if isinstance(maps_entry, str) and maps_entry != "NULL":
            symbol, _i = self.ref(maps_entry)
            d = self.p.find(symbol, self.room) if symbol else None
            if d is not None and isinstance(d.init, list):
                names = [x.lstrip("&") if isinstance(x, str) else None for x in d.init]
                self.cite("views", d)
            else:
                self.miss("views", maps_entry, "view-map directory not defined in C")
        if isinstance(counts_entry, str) and counts_entry != "NULL":
            symbol, _i = self.ref(counts_entry)
            d = self.p.find(symbol, self.room) if symbol else None
            if d is not None:
                init = d.init if isinstance(d.init, list) else [d.init]
                counts = [asset_journal.evaluate(x, self.p.consts_for(d.path)) for x in init]
                self.cite("views", d)
            else:
                self.miss("views", counts_entry, "view-count table not defined in C")
        for i in range(max(len(rows), len(names))):
            if i >= len(rows):
                rows.append({"roomIndex": i + 1, "table": None, "grid": None, "viewBoundaryTriggers": None, "actionTriggers": None, "occluders": None})
            rows[i]["viewMap"] = names[i] if i < len(names) else None
            rows[i]["viewCount"] = counts[i] if i < len(counts) else None
        return rows

    def grids(self, rooms: list[dict]) -> list[dict]:
        used: dict[str, list[int]] = defaultdict(list)
        for row in rooms:
            if row["grid"]:
                used[row["grid"]].append(row["roomIndex"])
        data = self.p.package(self.room)
        load = self.p.load["rooms"]
        manifest = self.p.manifest["rooms"]["overlays"].get(self.room, {}).get("objects", [])
        declared = {int(o["at"], 16) for o in manifest if o.get("kind") == "collisionSource"}
        out, seen = [], set()
        for text, indices in used.items():
            symbol, index = self.ref(text)
            address = self._addr_any(symbol) if symbol else None
            d = self.p.find(symbol, self.room) if symbol else None
            if address is None or data is None:
                self.miss("grids", text, "the grid header's address is not known (no D_ address, not in the room's symbol map)")
                continue
            address += index * sizeof(GRID)
            if d:
                self.cite("grids", d)
            grid = self.read_grid(data, load, address - load, symbol if not index else f"{symbol}[{index}]")
            if grid is None:
                continue
            grid.update({"roomIndices": indices, "patch": False, "source": d.where() if d else None,
                         "inManifest": (address - load) in declared})
            seen.add(address - load)
            out.append(grid)
        for off in sorted(declared - seen):
            # A grid the manifest declares that no room index names: code installs it (a second floor, a changed layout).
            names = [s for s, a in self.p.symbols(self.room).items() if a == load + off] or [f"D_{self.room}_{load + off:08X}"]
            d = self.p.find(names[0], self.room)
            grid = self.read_grid(data, load, off, names[0]) if data else None
            if grid is None:
                continue
            if d:
                self.cite("grids", d)
            grid.update({"roomIndices": [], "patch": False, "source": d.where() if d else None, "inManifest": True,
                         "note": "declared in the overlay manifest (collisionSource) but named by no room-resource row: installed or swapped by code"})
            out.append(grid)
        for o in manifest:
            if o.get("kind") == "collisionPatch" and data:
                out.append(self.read_patch(data, load, int(o["at"], 16), o["pieces"]))
        return out

    def read_grid(self, data: bytes, load: int, off: int, symbol: str) -> dict | None:
        if off < 0 or off + 0x24 > len(data):
            self.miss("grids", symbol, "header address outside the package")
            return None
        header = read_bytes(GRID, data, off)
        note = None
        try:
            gen_collision_inc.components(data, load, off)
        except ValueError as error:
            note = f"layout differs from the generated-include layout ({error}); pool sizes taken from the indices in use"
        at = {k: (header[k] or 0) - load for k in ("normals", "vertices", "faces", "cellFaceIds")}
        count = header["faceCount"]
        if not all(0 <= a < len(data) for a in at.values()) or at["faces"] + count * 12 > len(data):
            self.miss("grids", symbol, "a pool pointer leaves the package")
            return None
        faces_raw = [read_bytes(GRID_FACE, data, at["faces"] + i * 12) for i in range(count)]
        if note is None:
            n_normals, n_vertices = (at["vertices"] - at["normals"]) // 8, (at["faces"] - at["vertices"]) // 8
        else:
            n_normals = max((f["normalIndex"] for f in faces_raw), default=-1) + 1
            n_vertices = max((v for f in faces_raw for v in f["v"] if v != 0xFFFF), default=-1) + 1
        normals = [list(struct.unpack_from("<3h", data, at["normals"] + i * 8)) for i in range(n_normals)]
        vertices = [list(struct.unpack_from("<3h", data, at["vertices"] + i * 8)) for i in range(n_vertices)]
        cells, linked = [], set()
        for ci in range(header["cellCountX"] * header["cellCountZ"]):
            pointer = struct.unpack_from("<I", data, at["cellFaceIds"] + ci * 4)[0]
            ids = []
            if pointer:
                q = pointer - load
                while 0 <= q < len(data) - 1:
                    value = struct.unpack_from("<h", data, q)[0]
                    if value < 0:
                        break
                    ids.append(value)
                    q += 2
            linked.update(ids)
            cells.append(ids)
        faces = []
        for i, f in enumerate(faces_raw):
            quad = f["v"][3] != 0xFFFF
            idx = f["v"] if quad else f["v"][:3]
            normal = normals[f["normalIndex"]] if f["normalIndex"] < len(normals) else None
            faces.append({"v": idx, "normalIndex": f["normalIndex"], "normal": normal, "surfaceClass": f["surfaceClass"],
                          "floor": bool(normal and normal[1] < FLOOR_NORMAL_Y), "inCell": i in linked,
                          "skipped": f["v"][0] == 0 and f["v"][1] == 0})
        used = [vertices[v] for f in faces if not f["skipped"] for v in f["v"] if v < len(vertices)] or vertices
        bbox = {"min": [min(v[k] for v in used) for k in range(3)], "max": [max(v[k] for v in used) for k in range(3)]} if used else None
        out = {"id": symbol, "address": f"0x{load + off:08X}", "packageOffset": f"0x{off:X}", "xBias": header["xBias"], "zBias": header["zBias"],
               "origin": [-header["xBias"], -header["zBias"]], "cellCountX": header["cellCountX"], "cellCountZ": header["cellCountZ"],
               "cellSize": header["cellSize"], "faceCount": count, "vertexCount": n_vertices, "normalCount": n_normals,
               "vertices": vertices, "normals": normals, "faces": faces, "cells": cells, "bbox": bbox}
        if note:
            out["layoutNote"] = note
        return out

    def read_patch(self, data: bytes, load: int, at: int, pieces: list[str]) -> dict:
        by_addr = {a: s for s, a in self.p.symbols(self.room).items()}
        code = {f: self.p.texts[f] for f in self.p.owner_files.get(f"src/rooms/{self.room}", ()) if f.endswith(".c")}
        out = []
        for part in gen_collision_inc.patch_pieces(at, pieces):
            symbol = by_addr.get(load + part["offset"])
            if part["name"] == "vectors":
                values = [list(struct.unpack_from("<3h", data, part["offset"] + i * 8)) for i in range(part["size"] // 8)]
            else:
                values = []
                for i in range(part["size"] // 12):
                    f = read_bytes(GRID_FACE, data, part["offset"] + i * 12)
                    values.append({"v": f["v"] if f["v"][3] != 0xFFFF else f["v"][:3], "normalIndex": f["normalIndex"], "surfaceClass": f["surfaceClass"]})
            piece = {"kind": part["name"], "symbol": symbol, "packageOffset": f"0x{part['offset']:X}", "values": values, "copiedTo": []}
            d = self.p.find(symbol, self.room) if symbol else None
            if d:
                piece["source"] = self.cite("grids", d)
                consts = self.p.consts_for(d.path)
                for rel, text in code.items():
                    for m in re.finditer(r"memCopyBytes\s*\(\s*" + re.escape(symbol) + r"\s*,\s*&?\s*(\w+)\s*(?:\[([^\]]+)\])?", text):
                        piece["copiedTo"].append({"pool": m[1], "index": asset_journal.evaluate(m[2], consts) if m[2] else 0,
                                                  "file": rel, "line": text.count("\n", 0, m.start()) + 1})
            out.append(piece)
        return {"id": f"{self.room}_collision_patch_{at:05X}", "patch": True, "packageOffset": f"0x{at:X}", "pieces": out,
                "note": "headerless geometry the room's code copies into a live grid's pools (indices in a face piece are indices in that live grid); "
                        "`copiedTo` names the destination pool and first element where a memCopyBytes call in the room's C states it"}

    def surfaces(self) -> list[dict]:
        entry, _ = self.directory_entry("surfaces", "surfaceClasses")
        symbol, _i = self.ref(entry) if isinstance(entry, str) and entry != "NULL" else (None, 0)
        d = self.p.find(symbol, self.room) if symbol else None
        if d is None or not isinstance(d.init, list):
            return []
        self.cite("surfaceClasses", d)
        out = []
        for index, item in enumerate(d.init):
            name, _i = self.ref(item)
            if not name or name == "NULL":
                out.append({"surfaceClass": index, "record": None})
                continue
            rows, _d = self.table("surfaceClasses", name, SURFACE)
            row = rows[0] if rows else None
            entry = {"surfaceClass": index, "record": name}
            if row:
                entry.update({k: row[k] for k in ("field_0", "probePassThrough", "weaponImpactEnabled", "suppressPushback")})
                entry["footstepSounds"] = row["footstepSounds"]
                fname, _i = self.ref(row["footstepSounds"])
                if fname:
                    sounds, _d = self.table("surfaceClasses", fname, FOOTSTEPS)
                    if sounds:
                        entry["footsteps"] = {k: f"0x{v & 0xFFFFFFFF:08X}" if isinstance(v, int) else v for k, v in sounds[0].items()}
            out.append(entry)
        return out

    def quad(self, row: dict) -> dict:
        origin = v3(row["origin"])
        offsets = [v3(v) for v in row["vertices"]]
        return {"origin": origin, "offsets": offsets, "corners": [[origin[k] + o[k] for k in range(3)] for o in offsets], "normal": v3(row["normal"]),
                "radius": row["radius"]}

    def trigger_lists(self, rooms: list[dict], field: str, block: str) -> list[dict]:
        used: dict[str, list[int]] = defaultdict(list)
        for row in rooms:
            if row.get(field):
                used[row[field]].append(row["roomIndex"])
        out = []
        for text, indices in used.items():
            symbol, start = self.ref(text)
            if symbol is None:
                self.miss(block, str(text), "trigger pointer is not a plain symbol")
                continue
            rows, d = self.table(block, symbol, TRIGGER, start=start, stop=lambda r: bool(r["flags"] & 0x80))
            if rows is None:
                continue
            if rows and not rows[-1]["flags"] & 0x80:
                self.notes.append(f"{block}: {symbol} has no record with the LAST flag; all {len(rows)} declared records listed")
            items = []
            for i, row in enumerate(rows):
                item = {"index": start + i, **self.quad(row), "facingNormal": v3(row["facingNormal"]), "flags": row["flags"],
                        "geometryKind": TRIGGER_KIND.get(row["flags"] & 7, row["flags"] & 7), "last": bool(row["flags"] & 0x80),
                        "control": row["control"], "parameter0": row["parameter0"], "parameter1": row["parameter1"]}
                if block == "boundaries":
                    item["fromView"], item["toView"] = row["parameter0"], row["parameter1"]
                else:
                    item.update(self.action(row))
                items.append(item)
            out.append({"table": text.lstrip("&"), "roomIndices": indices, "source": d.where() if d else None, "items": items})
        return out

    @staticmethod
    def action(row: dict) -> dict:
        control, p0, p1 = row["control"], row["parameter0"], row["parameter1"]
        code = control & 0xFF
        name = TRIGGER_ACTION.get(code, f"action {code}")
        out = {"action": name, "automatic": bool(control & 0x8000), "outsideBattle": bool(control & 0x4000), "controlBits8to13": (control >> 8) & 0x3F}
        if code == 0:
            out["parameters"] = {"destinationArea": p0, "departureSlot": p1 >> 4, "arrivalSlot": p1 & 15}
        elif code == 1:
            out["parameters"] = {"selector": p0, "indexedSurface": bool(p0 & 0x80), "classOrFlight": (p0 & 0x70) >> 4, "steps": p0 & 15,
                                 "yaw": p1 * 16, "alternate": bool(control & 0x100)}
        elif code in (2, 6):
            if p1 == 0xFF:
                out["parameters"] = {"roomMessage0x13F0": p0}
            else:
                out["parameters"] = {"capCommand": p0, "flags": p1, "prepareWeaponAndPauseActors": bool(p1 & 1), "hidePlayer": bool(p1 & 2),
                                     "presentationMode": 2 if p1 & 4 else (0 if p1 & 1 else 3)}
        elif code == 3:
            out["parameters"] = {"callback": (control >> 8) & 0x7F, "arguments": [p0, p1]}
        elif code == 5:
            out["parameters"] = {"roomActionId": p0, "argument": p1}
        else:
            out["parameters"] = {}
        return out

    def occluders(self, rooms: list[dict]) -> list[dict]:
        used: dict[str, list[int]] = defaultdict(list)
        for row in rooms:
            if row.get("occluders"):
                used[row["occluders"]].append(row["roomIndex"])
        out = []
        for text, indices in used.items():
            symbol, start = self.ref(text)
            rows, d = self.table("occluders", symbol, OCCLUDER, start=start, stop=lambda r: bool(r["flags"] & 0x80)) if symbol else (None, None)
            if rows is None:
                continue
            out.append({"table": text.lstrip("&"), "roomIndices": indices, "source": d.where() if d else None,
                        "items": [{"index": start + i, **self.quad(r), "flags": r["flags"], "last": bool(r["flags"] & 0x80)} for i, r in enumerate(rows)]})
        return out

    @staticmethod
    def endpoint(t: dict) -> dict:
        word = t["yaw"]
        return {"position": [t["x"], t["y"], t["z"]], "yaw": word, "yawSpecial": YAW_SPECIAL.get(word)}

    def warp_entries(self, room: str, block: str | None) -> tuple[list[dict] | None, Def | None, str | None]:
        """A room's arrival endpoints, through its own stage directory entry."""
        stage, area, _folder = self.p.place_of[room]
        directory = self.p.stages.get(stage, {}).get("warps")
        if directory is None or area - 1 >= len(directory.init):
            return None, None, None
        entry = directory.init[area - 1]
        symbol, start = self.ref(entry)
        if not symbol or symbol == "NULL":
            return None, None, None
        if block:
            rows, d = self.table(block, symbol, WARP, start=start)
        else:
            other = Scene(self.p, room)
            rows, d = other.table("warps", symbol, WARP, start=start)
        return rows, d, symbol

    def warps(self, triggers: list[dict]) -> dict:
        self.directory_entry("warps", "warps")
        rows, d, symbol = self.warp_entries(self.room, "warps")
        out = {"table": symbol, "source": d.where() if d else None, "entries": [], "exits": [], "redirects": []}
        for i, row in enumerate(rows or []):
            out["entries"].append({"slot": i + 1, "player": self.endpoint(row["player"]), "companion": self.endpoint(row["companion"]),
                                   "unknown_10": row["unknown_10"], "unknown_24": row["unknown_24"],
                                   "arrivalSound": f"0x{row['arrivalSound'] & 0xFFFFFFFF:08X}" if row["arrivalSound"] else None,
                                   "departureSound": f"0x{row['departureSound'] & 0xFFFFFFFF:08X}" if row["departureSound"] else None,
                                   "blockedSound": f"0x{row['blockedSound'] & 0xFFFFFFFF:08X}" if row["blockedSound"] else None,
                                   "initialView": row["initialView"], "flags": row["flags"], "scriptedPlayer": bool(row["flags"] & 1),
                                   "fadeDeparture": bool(row["flags"] & 2), "mapFlagId": row["mapFlagId"] or None})
        cache: dict[str, list | None] = {}
        for group in triggers:
            for item in group["items"]:
                if item["action"] != "WARP":
                    continue
                params = item["parameters"]
                dest = self.p.room_at.get((self.stage, params["destinationArea"]))
                exit_ = {"triggerTable": group["table"], "triggerIndex": item["index"], "roomIndices": group["roomIndices"], "origin": item["origin"],
                         "corners": item["corners"], "facingNormal": item["facingNormal"], "automatic": item["automatic"],
                         "destinationStage": self.stage, "destinationArea": params["destinationArea"], "destinationRoom": dest,
                         "destinationRoomIndex": 1, "departureSlot": params["departureSlot"], "arrivalSlot": params["arrivalSlot"],
                         "departure": None, "arrival": None}
                slot = params["departureSlot"]
                if 1 <= slot <= len(out["entries"]):
                    here = out["entries"][slot - 1]
                    exit_["departure"] = {"slot": slot, "position": here["player"]["position"], "yaw": here["player"]["yaw"],
                                          "departureSound": here["departureSound"], "blockedSound": here["blockedSound"]}
                else:
                    self.miss("warps", f"{group['table']}[{item['index']}]", f"departure slot {slot} is not in this room's {len(out['entries'])}-entry warp table")
                if dest is None:
                    self.miss("warps", f"{group['table']}[{item['index']}]", f"destination area {params['destinationArea']} has no room folder in stage {self.stage}")
                else:
                    if dest not in cache:
                        cache[dest] = self.warp_entries(dest, None)[0] if dest != self.room else rows
                    there = cache[dest]
                    arrival = params["arrivalSlot"]
                    if there and 1 <= arrival <= len(there):
                        e = there[arrival - 1]
                        exit_["arrival"] = {"slot": arrival, **self.endpoint(e["player"]), "initialView": e["initialView"],
                                            "arrivalSound": f"0x{e['arrivalSound'] & 0xFFFFFFFF:08X}" if e["arrivalSound"] else None}
                    else:
                        self.miss("warps", f"{group['table']}[{item['index']}]",
                                  f"arrival slot {arrival} is not in {dest}'s warp table ({len(there) if there else 'no'} entries)")
                out["exits"].append(exit_)
        pattern = re.compile(r"\breply->(areaId|room|warp|stage)\s*=(?!=)")
        for rel in self.p.owner_files.get(f"src/rooms/{self.room}", ()):
            if not rel.endswith(".c"):
                continue
            for n, line in enumerate(self.p.texts[rel].splitlines(), 1):
                if pattern.search(line):
                    out["redirects"].append({"file": rel, "line": n, "text": line.strip()})
        return out

    def views(self, rooms: list[dict], boundaries: list[dict], warps: dict) -> list[dict]:
        entry, _ = self.directory_entry("cameras", "views")
        cameras, camera_def = None, None
        if isinstance(entry, str) and entry != "NULL":
            symbol, start = self.ref(entry)
            cameras, camera_def = self.table("views", symbol, VIEW_CAMERA, start=start) if symbol else (None, None)
        sprite_entry, _ = self.directory_entry("sprites", "views")
        sprite_symbol = self.ref(sprite_entry)[0] if isinstance(sprite_entry, str) else None
        if sprite_symbol and self.p.find(sprite_symbol, self.room):
            self.cite("views", self.p.find(sprite_symbol, self.room))
        residency = self.p.residency_maps().get(self.room)
        out = []
        for row in rooms:
            name, count = row.get("viewMap"), row.get("viewCount")
            index = row["roomIndex"]
            if not name or name == "NULL":
                continue
            block = {"roomIndex": index, "map": name, "viewCount": count, "cameraTable": camera_def.symbol if camera_def else None,
                     "spriteTable": sprite_symbol, "mapSource": None, "mapWriters": [], "views": []}
            if name == IDENTITY_MAP:
                mapping = list(IDENTITY_VIEWS)
                block["mapSource"] = {"symbol": IDENTITY_MAP, "file": "include/gameplay/view.h", "line": None, "note": "gameplay's shared identity map, 50 entries"}
            else:
                d = self.p.find(name, self.room)
                if d is None or not isinstance(d.init, list):
                    self.miss("views", name, "view map not defined in C")
                    continue
                mapping = [asset_journal.evaluate(x, self.p.consts_for(d.path)) if isinstance(x, str) else None for x in d.init]
                block["mapSource"] = self.cite("views", d)
                address, data = self._addr_any(name), self.p.package(self.room)
                if address and data and list(data[address - self.p.load["rooms"]:][:len(mapping)]) != mapping:
                    self.mismatches.append(f"{name}: view map in C differs from the package")
                writer = re.compile(r"\b" + re.escape(name) + r"\s*\[[^\]]+\]\s*[|&+-]?=(?!=)")
                for rel in self.p.owner_files.get(f"src/rooms/{self.room}", ()):
                    for n, line in enumerate(self.p.texts.get(rel, "").splitlines(), 1):
                        if writer.search(line) and not line.lstrip().startswith(("u8", "static")):
                            block["mapWriters"].append({"file": rel, "line": n, "text": line.strip()})
            if residency and index - 1 < len(residency) and count:
                if list(residency[index - 1][:count]) != mapping[:count]:
                    self.mismatches.append(f"room index {index}: view map differs from asset_residency.view_maps")
            reach = count or 0
            for group in boundaries:
                if index in group["roomIndices"]:
                    reach = max([reach] + [v for item in group["items"] for v in (item["fromView"], item["toView"])])
            reach = max([reach] + [e["initialView"] for e in warps["entries"]])
            reach = min(reach, len(mapping))
            for view in range(1, reach + 1):
                file = mapping[view - 1]
                item = {"view": view, "file": file, "withinViewCount": bool(count and view <= count), "fileExists": None, "background": None, "camera": None}
                if not file:
                    item["note"] = "map entry is 0: no file"
                    block["views"].append(item)
                    continue
                node = self.folder.get(file)
                item["fileExists"] = node is not None
                if node is not None:
                    chunks = list((node.get("chunks") or {}).values())
                    kind = lambda k: [c for c in chunks if self.p.assets.get(c, {}).get("type") == k]  # noqa: E731
                    item["background"] = {"fileKey": f"{self.stage}/{self.room}/file{file}", "bs": kind("bs"), "images": kind("pe2img"), "cluts": kind("pe2clut")}
                    if not item["background"]["bs"]:
                        item["note"] = "the view's file has no bs chunk: no background picture of its own"
                else:
                    item["note"] = f"the room's folder has no file {file}: nothing is loaded for this view (src/main/fs.c), the camera still applies"
                if cameras is not None and 1 <= file <= len(cameras):
                    item["camera"] = self.camera(cameras[file - 1], camera_def, file - 1)
                elif cameras is not None:
                    self.miss("views", f"room index {index} view {view}", f"file {file} is past the {len(cameras)}-entry camera array")
                block["views"].append(item)
            out.append(block)
        return out

    @staticmethod
    def camera(row: dict, d: Def | None, index: int) -> dict:
        m, t = row["transform"]["m"], row["transform"]["t"]
        h = row["screenDistance"]
        unit = lambda r: [round(c / 4096, 6) for c in r]  # noqa: E731
        return {"index": index, "matrix": m, "translation": t, "position": [-t[0], -t[1], -t[2]], "screenDistance": h,
                "right": unit(m[0]), "down": unit(m[1]), "forward": unit(m[2]),
                "fovYDegrees": round(math.degrees(2 * math.atan2(SCREEN[1] / 2, h)), 3) if h else None,
                "fovXDegrees": round(math.degrees(2 * math.atan2(SCREEN[0] / 2, h)), 3) if h else None,
                "planView": m[2][0] == 0 and m[2][2] == 0 and m[2][1] > 0}

    def resolve_file(self, row: dict, low: int = 0) -> dict:
        group, number = row["fileGroupIndex"], row["fileNumber"]
        out = {"fileId": None, "fileExists": None, "package": None, "models": []}
        if not isinstance(group, int) or not isinstance(number, int) or not 0 <= group < len(GROUP_BASE):
            return out
        file_id = (GROUP_BASE[group] + number // 100) * 10000 + (number % 100) * 100 + low
        node = self.p.common.get(file_id)
        out["fileId"], out["fileExists"] = file_id, node is not None
        if node:
            out["chunks"] = list((node.get("chunks") or {}).values())
            package = next((c for c in out["chunks"] if self.p.assets.get(c, {}).get("type") == "pe2pkg"), None)
            out["package"] = package
            out["models"] = list(self.p.models_of.get(package, [])) if package else []
        return out

    def placements(self) -> dict:
        out = {"table": None, "source": None, "variants": [], "defaultVariant": 1}
        areas = self.p.stages.get(self.stage, {}).get("areas")
        if areas is None:
            self.miss("placements", "Gp_AreaTables", f"no area table found for stage {self.stage}")
            return out
        if self.area >= len(areas.init):
            self.miss("placements", areas.symbol, f"the stage's area table has {len(areas.init)} entries; area {self.area} is past its end")
            return out
        record = areas.init[self.area]
        self.cite("placements", {**areas.where(), "index": self.area})
        symbol, _i = self.ref(record[0] if isinstance(record, list) and record else None)
        if not symbol or symbol == "NULL":
            self.notes.append(f"placements: Gp_AreaTables[{self.stage}][{self.area}] has no variant table")
            return out
        variants, d = self.table("placements", symbol, VARIANT)
        out["table"], out["source"] = symbol, d.where() if d else None
        setters = self.p.setters()
        mine = list(setters["room"].get(self.room, []))
        actors_here = set()
        for index, row in enumerate(variants or []):
            if index == 0 and not row["placements"] and not row["resources"]:
                continue        # slot 0 is never a layout (include/gameplay/area.h)
            # A slot storing NULL/NULL is still a selectable variant: the room with nothing placed.
            entry = {"variant": index, "default": index == 1, "empty": not row["placements"] and not row["resources"],
                     "placementsTable": row["placements"], "resourcesTable": row["resources"],
                     "resources": [], "placed": [], "setters": [], "hasSetter": False}
            resources = {}
            rsym, rstart = self.ref(row["resources"])
            rrows, rd = self.table("placements", rsym, RESOURCE, start=rstart, stop=lambda r: r["entryId"] == 0xFF) if rsym else (None, None)
            entry["resourcesSource"] = rd.where() if rd else None
            for r in rrows or []:
                if r["entryId"] == 0xFF:
                    break
                task = self.ref(r["taskTable"])[0]
                actor = self.p.actor_of(task) if task else None
                item = {"entryId": r["entryId"], "fileNumber": r["fileNumber"], "fileGroupIndex": r["fileGroupIndex"],
                        "groupBase": GROUP_BASE[r["fileGroupIndex"]] if isinstance(r["fileGroupIndex"], int) and r["fileGroupIndex"] < len(GROUP_BASE) else None,
                        "taskIndex": r["taskIndex"], "taskTable": r["taskTable"], "actor": actor, **self.resolve_file(r)}
                if item["fileExists"] is False:
                    item["note"] = "the disc has no file with this id: nothing is loaded for it (src/main/fs.c)"
                if actor:
                    actors_here.add(actor)
                resources.setdefault(r["entryId"], (r, item))
                entry["resources"].append(item)
            psym, pstart = self.ref(row["placements"])
            prows, pd = self.table("placements", psym, PLACEMENT, start=pstart, stop=lambda r: r["entryId"] == 0xFF) if psym else (None, None)
            entry["placementsSource"] = pd.where() if pd else None
            for i, r in enumerate(prows or []):
                if r["entryId"] == 0xFF:
                    break
                position = [r["x"], r["y"], r["z"]]
                item = {"index": i, "entryId": r["entryId"], "spawnVariant": r["variant"], "mode": r["mode"], "spawnArgument": (r["variant"] << 16) | r["mode"],
                        "position": position, "atOrigin": position == [0, 0, 0], "yaw": r["yaw"], "fileIdLow": r["fileIdLow"],
                        "texturePageOffset": r["texturePageOffset"], "clutRowOffset": r["clutRowOffset"], "rowIndex": r["rowIndex"],
                        "fileId": None, "package": None, "actor": None, "taskTable": None, "taskIndex": None, "models": []}
                if r["entryId"] in resources:
                    rr, ritem = resources[r["entryId"]]
                    item.update({k: ritem[k] for k in ("fileId", "fileExists", "package", "actor", "taskTable", "taskIndex", "models")})
                    if r["fileIdLow"]:
                        extra = self.resolve_file(rr, r["fileIdLow"])
                        item["extraFile"] = {k: extra[k] for k in ("fileId", "fileExists", "package")}
                        if extra["package"] and extra["package"] != item["package"]:
                            item["extraFile"]["models"] = extra["models"]
                    if not item["models"] and item["actor"] and self.p.models_of.get(item["actor"]):
                        item["models"] = list(self.p.models_of[item["actor"]])
                        item["modelsFrom"] = "the task table's overlay (the entry's own file carries no model)"
                else:
                    item["note"] = f"entry id {r['entryId']} is not in resource table {rsym}: no task recipe, nothing spawns"
                    self.notes.append(f"placements: {psym}[{i}] names entry id {r['entryId']}, which resource table {rsym} does not have")
                entry["placed"].append(item)
            entry["setters"] = [s for s in mine if s["variant"] == index]
            out["variants"].append(entry)
        # A variant written by a placed actor's code applies to whichever room that actor runs in.
        for actor in sorted(actors_here):
            for s in setters["actor"].get(actor, []):
                mine.append({**s, "note": f"written by {actor}, which this room's resources load"})
        for entry in out["variants"]:
            entry["setters"] = [s for s in mine if s["variant"] == entry["variant"]]
            entry["hasSetter"] = bool(entry["setters"])
        known = {e["variant"] for e in out["variants"]}
        # A stored value with no slot in this room's table: Dryfield by day and by night share saved slots, so a
        # record aimed at the twin room is listed here too (its policy says "shared slot").
        out["settersOutsideTable"] = [s for s in mine if s["variant"] not in known]
        return out

    def objects(self) -> dict:
        out = {"placesTable": None, "spawnsTable": None, "items": [], "spawns": [], "roomModels": []}
        entry, _ = self.directory_entry("objects", "objects")
        placed_models = set()
        if isinstance(entry, list) and entry:
            consts = self.p.consts_for(self.p.stages[self.stage]["objects"].path)
            row = from_c(OBJECT_ROOM, Cursor([entry]), consts)
            spawns = {}
            ssym = self.ref(row["spawns"])[0]
            if ssym:
                srows, sd = self.table("objects", ssym, OBJECT_SPAWN, stop=lambda r: r["kind"] == 0xFFFF)
                out["spawnsTable"] = ssym
                for r in srows or []:
                    if r["kind"] == 0xFFFF:
                        break
                    task = r["taskDesc"]
                    flags = task["header"]["flags"]
                    model = task["data"] if isinstance(task["data"], str) else None
                    key = self.model_key(model) if model and (flags & 0xFF) == 1 else None
                    if key:
                        placed_models.add(key)
                    spawn = {"kind": r["kind"], "body": {0: "none", 1: "tmd", 2: "coord"}.get(flags & 0xFF, flags & 0xFF) if isinstance(flags, int) else flags,
                             "bodyFlags": flags, "priority": task["header"]["priority"], "callback": task["callback"], "model": model, "modelKey": key,
                             "data": task["data"] if not isinstance(task["data"], str) else None}
                    spawns.setdefault(r["kind"], spawn)
                    out["spawns"].append(spawn)
            psym = self.ref(row["places"])[0] if isinstance(row["places"], str) else None
            if psym:
                prows, pd = self.table("objects", psym, OBJECT_PLACE, stop=lambda r: r["flagIndex"] == 0xFFFF)
                out["placesTable"] = psym
                for i, r in enumerate(prows or []):
                    if r["flagIndex"] == 0xFFFF:
                        break
                    position = [r["x"], r["y"], r["z"]]
                    out["items"].append({"index": i, "flagIndex": r["flagIndex"], "kind": r["kind"], "bank": r["kind"] >> 8, "subtype": r["kind"] & 0xFF,
                                         "placeKeyHigh": r["placeKeyHigh"], "state": r["state"], "initialFlag": r["state"] & 3,
                                         "prompt": bool(r["state"] & 0x200), "position": position, "atOrigin": position == [0, 0, 0], "yaw": r["yaw"],
                                         "spawn": spawns.get(r["kind"])})
        for key in self.p.models_of.get(self.room, []):
            out["roomModels"].append({"key": key, "address": f"0x{self.p.embedded[key]['vram']:08X}", "spawnedByObjectTable": key in placed_models})
        return out

    def model_key(self, symbol: str) -> str | None:
        """The asset key of the model a TmdSource symbol draws: its stream, matched to the catalogue."""
        symbol = symbol.lstrip("&")
        owner = self.room if symbol in self.p.symbols(self.room) else self.p.actor_of(symbol)
        if owner is None:
            self.miss("objects", symbol, "model source is in no symbol map (this room's or an actor's)")
            return None
        if owner != self.room:
            # An actor overlay's model: its symbol map gives the stream's address, which is the catalogue's.
            stream = self.p._actor_symbols(owner).get(f"_{symbol}Stream")
            key = self.p.model_at.get((owner, stream))
            if key is None:
                self.miss("objects", symbol, f"its stream in {owner} is not a catalogued model")
            return key
        address, data, load = self.p.symbols(self.room)[symbol], self.p.package(self.room), self.p.load["rooms"]
        if data is None or not 0 <= address - load + 0x24 <= len(data):
            self.miss("objects", symbol, "model source address is outside this room's package")
            return None
        stream = struct.unpack_from("<I", data, address - load + 0x20)[0]
        key = self.p.model_at.get((self.room, stream))
        if key is None:
            # A mesh several packages carry is catalogued once, under its SHA-1 (asset_data.MODELS).
            try:
                key = pkg_model.catalogue_name(data, load, address - load)
            except Exception:  # noqa: BLE001 - an unreadable stream is reported below
                key = None
        if key is None:
            self.miss("objects", symbol, f"its stream at 0x{stream:08X} is not a catalogued model")
        return key

    # ---- checks

    def checks(self, scene: dict) -> dict:
        boxes = [g["bbox"] for g in scene["grids"] if not g.get("patch") and g.get("bbox")]
        out: dict = {"gridBounds": None}
        if boxes:
            lo = [min(b["min"][k] for b in boxes) for k in range(3)]
            hi = [max(b["max"][k] for b in boxes) for k in range(3)]
            out["gridBounds"] = {"min": lo, "max": hi}
            size = max(hi[0] - lo[0], hi[2] - lo[2], 1)

            def outside(p):
                dx = max(lo[0] - p[0], 0, p[0] - hi[0])
                dz = max(lo[2] - p[2], 0, p[2] - hi[2])
                return round(math.hypot(dx, dz))

            def summary(points):
                far = [{"what": w, "position": p, "outsideXZ": outside(p)} for w, p in points if outside(p) > 0]
                return {"count": len(points), "insideXZ": len(points) - len(far), "outsideXZ": far, "worstOverExtent": round(max([f["outsideXZ"] for f in far] or [0]) / size, 3)}

            cams = {}
            for block in scene["views"]:
                for v in block["views"]:
                    if v["camera"]:
                        cams[v["file"]] = v["camera"]["position"]
            out["cameras"] = summary([(f"camera of file {f}", p) for f, p in sorted(cams.items())])
            out["warpEndpoints"] = summary([(f"warp slot {e['slot']}", e["player"]["position"]) for e in scene["warps"]["entries"]])
            out["placements"] = summary([(f"variant {v['variant']} placement {p['index']}", p["position"])
                                         for v in scene["placements"]["variants"] for p in v["placed"] if not p["atOrigin"]])
            out["objects"] = summary([(f"object {o['index']}", o["position"]) for o in scene["objects"]["items"] if not o["atOrigin"]])
            out["triggers"] = summary([(f"{g['table']}[{i['index']}]", i["origin"]) for key in ("boundaries", "triggers") for g in scene[key] for i in g["items"]])
        files = [(b["roomIndex"], v["view"], v["file"], v["fileExists"]) for b in scene["views"] for v in b["views"] if v["file"]]
        out["viewFiles"] = {"referenced": len({f for _r, _v, f, _e in files}), "missing": sorted({f for _r, _v, f, e in files if e is False})}
        out["camerasResolved"] = {"views": len(files), "withCamera": sum(1 for b in scene["views"] for v in b["views"] if v["camera"])}
        # Arrival yaw against the door's facing normal: forward(yaw) should point along the trigger's facingNormal.
        agree = total = 0
        differ: list[dict] = []
        for e in scene["warps"]["exits"]:
            dep, n = e["departure"], e["facingNormal"]
            if not dep or dep["yaw"] in YAW_SPECIAL or (n[0] == 0 and n[2] == 0):
                continue
            a = dep["yaw"] * math.tau / 4096
            dot = (math.sin(a) * n[0] + math.cos(a) * n[2]) / math.hypot(n[0], n[2])
            total += 1
            agree += dot > 0.7
            if dot <= 0.7:
                differ.append({"trigger": f"{e['triggerTable']}[{e['triggerIndex']}]", "departureSlot": dep["slot"], "yaw": dep["yaw"], "facingNormal": n})
        out["warpYawAgreesWithTrigger"] = {"compared": total, "agree": agree, "differ": differ}
        # Quads read as strips must be simple (non self-intersecting) polygons in the loop order 1, 0, 2, 3.
        simple = quads = 0
        for g in scene["grids"]:
            if g.get("patch"):
                continue
            for f in g["faces"]:
                if len(f["v"]) == 4 and not f["skipped"] and f["normal"] and all(v < len(g["vertices"]) for v in f["v"]):
                    quads += 1
                    simple += self.convex([g["vertices"][f["v"][k]] for k in (1, 0, 2, 3)], f["normal"])
        out["gridQuadsConvexInStripOrder"] = {"quads": quads, "convex": simple}
        return out

    @staticmethod
    def convex(loop: list[list[int]], normal: list[int]) -> bool:
        signs = set()
        for i in range(4):
            a, b, c = loop[i], loop[(i + 1) % 4], loop[(i + 2) % 4]
            u = [b[k] - a[k] for k in range(3)]
            w = [c[k] - b[k] for k in range(3)]
            cross = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
            dot = sum(cross[k] * normal[k] for k in range(3))
            if dot:
                signs.add(dot > 0)
        return len(signs) <= 1


# --------------------------------------------------------------------------- report

BLOCKS = ("grids", "views", "boundaries", "warps", "triggers", "placements", "objects")


def block_state(scene: dict, block: str) -> str:
    """resolved (has content), empty (the room has none), or unresolved (something of the block could not be read)."""
    missing = any(u["block"] == block for u in scene["unresolved"])
    if block == "grids":
        has = any(not g.get("patch") for g in scene["grids"])
    elif block == "views":
        has = any(b["views"] for b in scene["views"])
        missing = missing or any(v["file"] and not v["camera"] for b in scene["views"] for v in b["views"])
    elif block in ("boundaries", "triggers"):
        has = any(g["items"] for g in scene[block])
    elif block == "warps":
        has = bool(scene["warps"]["entries"])
    elif block == "placements":
        has = any(not v["empty"] for v in scene["placements"]["variants"])
    else:
        has = bool(scene["objects"]["items"] or scene["objects"]["spawns"])
    if missing:
        return "partial" if has else "unresolved"
    return "resolved" if has else "empty"


def report(scenes: dict[str, dict], errors: dict[str, str]) -> tuple[dict, str]:
    table = {b: Counter() for b in BLOCKS}
    failures = []
    for room, scene in scenes.items():
        for b in BLOCKS:
            state = block_state(scene, b)
            table[b][state] += 1
            if state in ("partial", "unresolved"):
                failures.append((room, b, [u for u in scene["unresolved"] if u["block"] == b]))
    lines = [f"room scenes: {len(scenes)} written, {len(errors)} failed", "",
             f"{'block':<12}{'resolved':>9}{'empty':>7}{'partial':>9}{'unresolved':>12}"]
    for b in BLOCKS:
        c = table[b]
        lines.append(f"{b:<12}{c['resolved']:>9}{c['empty']:>7}{c['partial']:>9}{c['unresolved']:>12}")
    lines += ["", "failures (partial / unresolved):"]
    for room, b, items in failures:
        for u in items or [{"what": "", "reason": "a view has no camera"}]:
            lines.append(f"  {room}  {b}: {u['what']} - {u['reason']}")
    for room, error in errors.items():
        lines.append(f"  {room}  TOOL ERROR: {error}")
    other = [(room, u) for room, s in scenes.items() for u in s["unresolved"] if u["block"] not in BLOCKS]
    if other:
        lines += ["", "unresolved outside the seven blocks:"] + [f"  {room}  {u['block']}: {u['what']} - {u['reason']}" for room, u in other]
    checks: dict = {}
    for key in ("cameras", "warpEndpoints", "placements", "objects", "triggers"):
        count = inside = 0
        far = []
        for room, s in scenes.items():
            c = s["checks"].get(key)
            if not c:
                continue
            count += c["count"]
            inside += c["insideXZ"]
            far += [(c["worstOverExtent"], room, f) for f in c["outsideXZ"]]
        bins = Counter()
        for room, s in scenes.items():
            c = s["checks"].get(key)
            if not c:
                continue
            extent = max(s["checks"]["gridBounds"]["max"][0] - s["checks"]["gridBounds"]["min"][0], s["checks"]["gridBounds"]["max"][2] - s["checks"]["gridBounds"]["min"][2], 1)
            for f in c["outsideXZ"]:
                r = f["outsideXZ"] / extent
                bins["within 25% of the grid's extent" if r <= 0.25 else "within 100%" if r <= 1 else "beyond 100%"] += 1
        checks[key] = {"count": count, "insideGridBoundsXZ": inside, "outside": dict(bins),
                       "farthest": [{"room": room, **f} for _w, room, f in sorted(far, key=lambda x: -x[2]["outsideXZ"])[:12]]}
    files = sum(s["checks"]["viewFiles"]["referenced"] for s in scenes.values())
    missing = {room: s["checks"]["viewFiles"]["missing"] for room, s in scenes.items() if s["checks"]["viewFiles"]["missing"]}
    cams = Counter()
    yaw = Counter()
    quads = Counter()
    verify = Counter()
    mismatches = []
    for room, s in scenes.items():
        cams["views"] += s["checks"]["camerasResolved"]["views"]
        cams["withCamera"] += s["checks"]["camerasResolved"]["withCamera"]
        yaw["compared"] += s["checks"]["warpYawAgreesWithTrigger"]["compared"]
        yaw["agree"] += s["checks"]["warpYawAgreesWithTrigger"]["agree"]
        quads["quads"] += s["checks"]["gridQuadsConvexInStripOrder"]["quads"]
        quads["convex"] += s["checks"]["gridQuadsConvexInStripOrder"]["convex"]
        verify["compared"] += s["verification"]["recordsComparedWithPackage"]
        verify["fromPackage"] += s["verification"]["valuesTakenFromPackage"]
        mismatches += [f"{room}: {m}" for m in s["verification"]["mismatches"]]
    lines += ["", "sanity checks (XZ distance outside the union of the room's grid bounds):"]
    for key, c in checks.items():
        lines.append(f"  {key:<14} {c['count']:>5} points, {c['insideGridBoundsXZ']:>5} inside; outside: {c['outside'] or 'none'}")
        for f in c["farthest"][:5]:
            lines.append(f"      {f['room']}  {f['what']}  at {f['position']}  {f['outsideXZ']} units outside")
    lines += [f"  view files     {files} referenced, missing in {len(missing)} rooms: {missing or 'none'}",
              f"  cameras        {cams['withCamera']} of {cams['views']} mapped views have a camera record",
              f"  warp yaw       {yaw['agree']} of {yaw['compared']} warp triggers agree with forward(yaw) = (sin, 0, cos) of their departure endpoint",
              f"  grid quads     {quads['convex']} of {quads['quads']} are convex in strip order (loop 1, 0, 2, 3)",
              f"  C vs package   {verify['compared']} records compared, {verify['fromPackage']} values taken from the package, {len(mismatches)} mismatches"]
    lines += [f"      {m}" for m in mismatches[:40]]
    summary = {"rooms": len(scenes), "errors": errors, "coverage": {b: dict(table[b]) for b in BLOCKS},
               "failures": [{"room": r, "block": b, "unresolved": u} for r, b, u in failures], "checks": checks,
               "viewFilesMissing": missing, "cameras": dict(cams), "warpYaw": dict(yaw), "gridQuads": dict(quads), "verification": dict(verify),
               "mismatches": mismatches}
    return summary, "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("room", nargs="?")
    parser.add_argument("--all", action="store_true", help="every room overlay")
    parser.add_argument("--out", type=Path, help="directory for --all")
    parser.add_argument("--indent", type=int, default=None, help="pretty-print with this indent")
    args = parser.parse_args()
    project = Project()
    if args.all:
        if args.out is None:
            parser.error("--all needs --out DIR")
        args.out.mkdir(parents=True, exist_ok=True)
        scenes, errors = {}, {}
        for room in sorted(project.place_of):
            try:
                scenes[room] = Scene(project, room).build()
            except Exception as error:  # noqa: BLE001 - one room must not stop the sweep; it is reported
                errors[room] = f"{type(error).__name__}: {error}"
                continue
            (args.out / f"{room}.json").write_text(json.dumps(scenes[room], indent=args.indent, separators=None if args.indent else (",", ":")) + "\n")
        summary, text = report(scenes, errors)
        (args.out / "_report.json").write_text(json.dumps(summary, indent=1) + "\n")
        (args.out / "_report.txt").write_text(text)
        sys.stdout.write(text)
        return 1 if errors else 0
    if args.room not in project.place_of:
        parser.error(f"unknown room {args.room!r}; rooms are the folders of tools/peassets/asset_data.py TREE")
    json.dump(Scene(project, args.room).build(), sys.stdout, indent=args.indent if args.indent is not None else 1)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
