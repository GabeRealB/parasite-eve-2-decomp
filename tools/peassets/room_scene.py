#!/usr/bin/env python3
"""One JSON scene per room overlay, in world space, for the asset map's room viewer.

    room_scene.py ROOM                      print the room's scene
    room_scene.py --all --out DIR           one <room>.json each, plus _report.json / _report.txt

Everything is read from the extracted packages, nothing is guessed and no C
initialiser is parsed. The route to each table is the one gameplay takes:
gameplay holds, per stage, a pointer to each of the stage map overlay's
directories, every directory is indexed by area (a room folder's disc id /
100), and `gAreaRecordsByStage` holds the placement variants. Those roots are pinned
by ADDRESS (`GAMEPLAY_ROOTS`, `STAGE_DIRECTORIES`); from them the tool follows
pointers with the images mapped at their load addresses - gameplay, the
stage's map overlay and the room - and decodes each record with the layouts
below:

    WorldCollisionRoomResources[room index]  grid, view boundaries, action triggers, occluders
    u8* [room index] + ViewCount[]           logical view -> view file
    ViewCamera[view file]                    world-to-camera matrix, negated origin, projection distance
    SpriteView[view file]                    SpriteBatch ranges, SpriteSource rectangles (page, CLUT), draw areas
    DirectionWarpEntry[warp slot]            arrival endpoints
    AreaVariant[variant]                     AreaPlacement + AreaResource tables
    AreaObjectRoom[area]                     placed objects (AreaObjectPlace) and how they spawn

Names and extents come from the splat symbol maps (`configs/USA/sym.*.txt`,
`configs/USA/sym/**/*.txt`) and the overlay manifest. A table's length is, in
this order: its terminator or the count the game itself uses; its symbol's
`size:` over the record size; the distance to the next symbol. `sources` says
which one settled each table (`length`) and cites the symbol's declaration,
found by a plain search for its name. A renamed symbol or a rewritten
initialiser therefore changes names and citations, never a value.

C is read only for what is code and not data: who stores a placement variant
(`area_variants.py`, `areaSetPlacementVariant` calls, direct writes), warp
redirects, writes to a view map and a collision patch's `memCopyBytes`
targets. Collision geometry is an asset; its layout is checked against
tools/buildsteps/gen_collision_inc.py.

What cannot be read reliably is left out and listed under `unresolved`.
"""
from __future__ import annotations

import argparse
import bisect
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
sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
import area_variants  # noqa: E402
import asset_journal  # noqa: E402
import asset_schema  # noqa: E402
import gen_collision_inc  # noqa: E402
from model import tmd  # noqa: E402
from asset_residency import GROUP_BASE  # noqa: E402

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


# --------------------------------------------------------------------------- roots, by address

# Symbols get renamed; addresses do not move. Every root below is an address, and its current name is looked up in
# the symbol maps only for display.
GAMEPLAY = "gameplay"
GAMEPLAY_LOAD = 0x80093800       # configs/USA/gameplay.yaml; the resident image the stage tables live in
MAIN_LOAD = 0x80010000           # the executable: only its names are used (task callbacks)

# The stage's map overlay. Established by the directories themselves - every populated entry of stage N's camera
# directory is an address inside one of stage N's room packages - and checked on every run against the package's
# own id word (offset 0), which is 279 + stage for these five (overlays.toml ids 280-284).
STAGE_MAPS = {1: "map_akropolis", 2: "map_dryfield", 3: "map_dryfield_full", 4: "map_shelter", 5: "map_neo_ark"}
STAGE_MAP_ID_BASE = 279

# gameplay's per-stage tables: kind -> (address, bytes per stage, offset of the pointer in a stage's entry, stage of
# entry 0, whether the pointer leads to a one-word wrapper in the map overlay holding the directory's address).
# Established from src/gameplay (load_screen.c, view_image.c, scene_runtime.c, cd_loading.c) at commit 2b2fe57e4a,
# where they are gViewCameraTables, gViewCountTables, gViewIndexTables, gSpriteAreaTables, gWorldCollisionStageResources,
# gDirectionAreaWarpEntriesByStage, gWorldCollisionSurfacePropertiesTables, gAreaObjectStages and gAreaRecordsByStage, and from sym.gameplay.txt for the addresses.
GAMEPLAY_ROOTS = {
    "cameras":   (0x8010CB2C, 4, 0, 1, True),    # ViewCameraTable* [5]            -> { ViewCamera** }
    "counts":    (0x8010CB40, 4, 0, 1, True),    # ViewCountTable* [5]             -> { ViewCount** }
    "maps":      (0x8010CB54, 4, 0, 1, True),    # ViewIndexTable* [5]             -> { u8*** }
    "sprites":   (0x8010CB68, 4, 0, 1, True),    # SpriteAreaTable* [5]            -> { SpriteView** }
    "resources": (0x8010CB7C, 4, 0, 1, True),    # WorldCollisionStageResources* [5] -> { WorldCollisionRoomResources** }
    "warps":     (0x8010CB90, 4, 0, 1, False),   # DirectionWarpEntry** [5]
    "surfaces":  (0x8010CBB8, 4, 0, 1, False),   # WorldCollisionSurfaceProperties*** [5]
    "objects":   (0x8010D230, 8, 0, 0, False),   # AreaObjectStage [6]: { AreaObjectRoom*, object states }
    "areas":     (0x8010CBCC, 4, 0, 0, False),   # const AreaRecord* [6] (gAreaRecordsByStage), in gameplay itself
}

# The per-stage directories in the five map overlays (and the stage's AreaRecord array in gameplay). Established by
# reading the tables above from gameplay.pe2pkg; `Project` re-reads them on every run and reports a disagreement, so
# a pinned address that stopped being what gameplay uses cannot pass silently. All are indexed by area - 1, except
# `objects` and `areas`, which are indexed by area. The Dryfield stages' object place lists live in gameplay
# (0x8010xxxx) and need no pin: `objects` entries point at them and the pointer is followed.
STAGE_DIRECTORIES = {
    1: {"cameras": 0x8017ABC4, "counts": 0x8017AB70, "maps": 0x8017AC18, "sprites": 0x8017AACC, "resources": 0x8017AA78,
        "warps": 0x8017AB20, "surfaces": 0x8017AC6C, "objects": 0x8017A7FC, "areas": 0x8010CBE4},
    2: {"cameras": 0x8017AAC8, "counts": 0x8017A990, "maps": 0x8017AB64, "sprites": 0x8017AC00, "resources": 0x8017AA2C,
        "warps": 0x8017A8F8, "surfaces": 0x8017AC9C, "objects": 0x8017A564, "areas": 0x8010CC94},
    3: {"cameras": 0x8017A9DC, "counts": 0x8017A8A4, "maps": 0x8017AA78, "sprites": 0x8017AB14, "resources": 0x8017A940,
        "warps": 0x8017A80C, "surfaces": 0x8017ABB0, "objects": 0x8017A46C, "areas": 0x8010CDD4},
    4: {"cameras": 0x8017B3BC, "counts": 0x8017B04C, "maps": 0x8017B484, "sprites": 0x8017B54C, "resources": 0x8017B2F4,
        "warps": 0x8017AF88, "surfaces": 0x8017B614, "objects": 0x8017A998, "areas": 0x8010CF14},
    5: {"cameras": 0x8017ACA4, "counts": 0x8017AB04, "maps": 0x8017AD2C, "sprites": 0x8017ADB4, "resources": 0x8017AC1C,
        "warps": 0x8017AA80, "surfaces": 0x8017AE3C, "objects": 0x8017A6EC, "areas": 0x8010D0AC},
}
BY_AREA = ("objects", "areas")   # indexed by area; every other directory by area - 1

# gameplay's shared identity view map (gViewIdentityMap in sym.gameplay.txt, `size:0x32`): a room index whose map
# pointer is this address uses view N -> file N. Its 50 bytes are read from gameplay like any other map.
IDENTITY_MAP = 0x8010CAF8
IDENTITY_MAP_LENGTH = 50

OBJECT_ROOM_END = -1             # AreaObjectRoom.places.sentinel that ends a stage's room table (area_flags.h)


# --------------------------------------------------------------------------- symbol maps

SYMLINE = re.compile(r"^[ \t]*(\w+)[ \t]*=[ \t]*0x([0-9A-Fa-f]+)[ \t]*;[ \t]*(?://([^\n]*))?$", re.M)
SYMSIZE = re.compile(r"\bsize:0x([0-9A-Fa-f]+)")
SYMOWNER = re.compile(r"\bowner=(\w+)")


class Symbols:
    """One image's names and sizes, by address."""

    def __init__(self, owner: str):
        self.owner = owner
        self.names: dict[int, str] = {}
        self.sizes: dict[int, int] = {}
        self.marks: set[int] = set()        # addresses known to start something, with no name in a map (pinned roots)
        self._sized: list[int] | None = None
        self._all: list[int] | None = None

    def add(self, address: int, name: str, size: int | None) -> None:
        self.names.setdefault(address, name)
        if size and address not in self.sizes and self.names[address] == name:
            self.sizes[address] = size
        self._sized = self._all = None

    def mark(self, address: int) -> None:
        self.marks.add(address)
        self._all = None

    def default(self, address: int) -> str:
        """splat's name for an address no map names."""
        return f"D_{address:08X}" if self.owner in ("main", GAMEPLAY) else f"D_{self.owner}_{address:08X}"

    def find(self, address: int) -> tuple[str, int, int | None]:
        """(name, base, size) of the symbol at `address`, or the sized symbol containing it; else its default name."""
        if address in self.names:
            return self.names[address], address, self.sizes.get(address)
        if self._sized is None:
            self._sized = sorted(self.sizes)
        i = bisect.bisect_right(self._sized, address) - 1
        if i >= 0 and self._sized[i] + self.sizes[self._sized[i]] > address:
            base = self._sized[i]
            return self.names[base], base, self.sizes[base]
        return self.default(address), address, None

    def next_after(self, address: int) -> int | None:
        if self._all is None:
            self._all = sorted(set(self.names) | self.marks)
        i = bisect.bisect_right(self._all, address)
        return self._all[i] if i < len(self._all) else None


class SymbolMaps:
    """Every splat symbol map of the project.

    A line of an image's own map names something in that image. A line marked `absolute:True` (and every line of an
    imports file) names something elsewhere: `owner=` says in which package, and the address alone says so for main
    and gameplay. Those foreign lines are how a room's or a map overlay's table is tied to the actor it points into.
    """

    def __init__(self):
        self.own: dict[str, Symbols] = {}
        self.foreign: dict[str, dict[int, list[tuple[str, str | None]]]] = defaultdict(lambda: defaultdict(list))
        self.annotated: dict[str, dict[int, str]] = defaultdict(dict)
        self.definer: dict[str, str] = {}       # name -> the first actor map defining it
        base = ROOT / "configs/USA"
        for image in ("main", GAMEPLAY):
            self._read(base / f"sym.{image}.txt", image, False)
            self._read(base / f"sym.{image}.imports.txt", image, True)
        for path in sorted((base / "sym").glob("*.imports.txt")):
            self._read(path, path.name.removesuffix(".imports.txt") + ".imports", True)
        for path in sorted((base / "sym").glob("*/*.txt")):
            self._read(path, path.stem, False, actors=path.parent.name == "actors")
        for owner, named in self.annotated.items():
            for address in named:
                self.of(owner).mark(address)

    def _read(self, path: Path, holder: str, imports: bool, actors: bool = False) -> None:
        if not path.exists():
            return
        own = self.of(holder) if not imports else None
        for m in SYMLINE.finditer(path.read_text()):
            name, address, note = m[1], int(m[2], 16), m[3] or ""
            owner = SYMOWNER.search(note)
            size = SYMSIZE.search(note)
            if imports or "absolute:True" in note or (owner and owner[1] != holder):
                self.foreign[holder][address].append((name, owner[1] if owner else None))
                if owner:
                    self.annotated[owner[1]].setdefault(address, name)
                continue
            own.add(address, name, int(size[1], 16) if size else None)
            if actors:
                self.definer.setdefault(name, holder)

    def of(self, owner: str) -> Symbols:
        if owner not in self.own:
            self.own[owner] = Symbols(owner)
        return self.own[owner]

    def find(self, owner: str, address: int) -> tuple[str, int, int | None]:
        """As `Symbols.find`, falling back on what another map says this owner has at the address."""
        name, base, size = self.of(owner).find(address)
        if base == address and address not in self.of(owner).names and address in self.annotated.get(owner, {}):
            name = self.annotated[owner][address]
        return name, base, size


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
# include/gameplay/sprites.h. A source's rectangle is (w, h) pixels at (x0, y0) from the drawing origin, read from
# texel (u0, v0) of its page; the last four bytes are the modulation colour and the GPU code bits.
SPRITE_VIEW = rec(("sources", "ptr"), ("batches", "ptr"), ("drawAreas", "ptr"))
SPRITE_BATCH = rec(("firstSprite", "u16"), ("spriteCount", "u16"), ("hidden", "u8"), ("skipCachedPackets", "u8"), ("field_6", arr("u8", 2)))
SPRITE_SOURCE = rec(("tpage", "u16"), ("clut", "u16"), ("w", "s16"), ("h", "s16"), ("x0", "s16"), ("y0", "s16"), ("depth", "u16"),
                    ("u0", "u8"), ("v0", "u8"), ("r0", "u8"), ("g0", "u8"), ("b0", "u8"), ("codeFlags", "u8"))
SPRITE_DRAW_AREA = rec(("x", "s16"), ("y", "s16"), ("w", "s16"), ("h", "s16"), ("restoreDepth", "u16"))
SPRITE_BATCH_END = 0xFFFF        # SpriteBatch.firstSprite of the terminal record
SPRITE_DRAW_AREA_END = 0xFFFF    # SpriteDrawArea.restoreDepth of the terminal record
SPRITE_ROW = ("tpage", "clut", "w", "h", "x0", "y0", "depth", "u0", "v0", "r0", "g0", "b0", "codeFlags")
TPAGE_DEPTH = (4, 8, 16, 16)     # bits 7-8 of a texture page word (libgpu getTPage)
GRID_FACE = rec(("v", arr("u16", 4)), ("normalIndex", "u16"), ("surfaceClass", "s16"))


def v3(v: dict) -> list[int]:
    return [v["vx"], v["vy"], v["vz"]]


def is_pointer(value) -> bool:
    return isinstance(value, int) and value >= 0x80000000


# --------------------------------------------------------------------------- the project

# A file-scope definition: the declared name is the last identifier before the dimensions and the `=`.
DECLARATION = re.compile(r"^(?!(?:extern|typedef|return)\b)[A-Za-z_][^\n;(){}=]*?\b([A-Za-z_]\w*)[ \t]*(?:\[[^\]\n]*\][ \t]*)*"
                         r"(?:(?:__attribute__\s*\(\(.*?\)\)|[A-Z][A-Z0-9_]*\s*\([^()\n]*\))\s*)*=(?!=)", re.M)
SOURCE_DIRS = {"rooms": "src/rooms/{}", "mapui": "src/mapui/{}", GAMEPLAY: "src/gameplay", "main": "src/main"}


class Project:
    def __init__(self):
        self.manifest = tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text())
        self.family: dict[str, str] = {GAMEPLAY: GAMEPLAY, "main": "main"}
        self.load_of: dict[str, int] = {GAMEPLAY: GAMEPLAY_LOAD}
        for family, body in self.manifest.items():
            for key, overlay in (body.get("overlays") or {}).items():
                for package in [key] + [slot["package"] for slot in overlay.get("slots", ()) if "package" in slot]:
                    self.family.setdefault(package, family)
                    self.load_of.setdefault(package, body["load_addr"])
        # An actor's address follows from the slot it is linked for, not from its family: slots 2 and 3 load higher.
        import asset_residency
        self.load_of.update(asset_residency.package_loads(self.manifest))
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
            if r.get("type") in ("model", "tmd"):
                package = r.get("source", "").removesuffix(".pe2pkg")
                self.models_of[package].append(key)
                # Code reaches a model by its address once the package is loaded (stated for a model).
                self.model_at[package, asset_schema.embedded_address(r)] = key
        self.maps = SymbolMaps()
        self._pkg: dict[str, bytes | None] = {}
        self._code: dict[str, dict[str, str]] = {}
        self._declared: dict[str, dict[str, tuple[str, int]]] = {}
        self._setters = None
        self._globals: dict[str, int] | None = None
        self.root_problems: list[str] = []
        self.stages = self._stage_directories()

    # ---- images

    def package(self, owner: str) -> bytes | None:
        if owner not in self._pkg:
            path = ROOT / "assets/USA/pe2pkg" / f"{owner}.pe2pkg"
            self._pkg[owner] = path.read_bytes() if path.exists() else None
        return self._pkg[owner]

    def word(self, owner: str, address: int) -> int | None:
        data, off = self.package(owner), address - self.load_of.get(owner, 0)
        return struct.unpack_from("<I", data, off)[0] if data is not None and 0 <= off <= len(data) - 4 else None

    def extent(self, owner: str, address: int) -> tuple[int | None, str | None]:
        """(bytes from `address` to the end of its object, what settled it): its symbol's size, else the next symbol."""
        _name, base, size = self.maps.find(owner, address)
        if size is not None:
            return base + size - address, "symbol size"
        nxt = self.maps.of(owner).next_after(address)
        data = self.package(owner)
        end = self.load_of[owner] + len(data) if data is not None and owner in self.load_of else None
        if end is not None and (nxt is None or nxt > end):
            return end - address, "end of package"      # the last object of the image
        return (nxt - address, "next symbol") if nxt is not None else (None, None)

    def actor_of(self, symbol: str) -> str | None:
        """The overlay a task table belongs to: by its D_ name, or the first actor symbol map that defines it."""
        m = asset_journal.OWNER.match(symbol)
        return m[1] if m else self.maps.definer.get(symbol)

    # ---- stage directories

    def _stage_directories(self) -> dict[int, dict]:
        """stage -> its map overlay and, per kind, the directory's address, owner, entry count and what settled it."""
        out: dict[int, dict] = {}
        derived: dict[int, dict[str, int | None]] = defaultdict(dict)
        for stage, owner in STAGE_MAPS.items():
            data = self.package(owner)
            if data is None or self.package(GAMEPLAY) is None:
                continue
            if struct.unpack_from("<H", data, 0)[0] != STAGE_MAP_ID_BASE + stage:
                self.root_problems.append(f"{owner}: package id is not {STAGE_MAP_ID_BASE + stage}, so it is not stage {stage}'s map overlay")
            symbols = self.maps.of(owner)
            for kind, (table, stride, offset, first, wrapped) in GAMEPLAY_ROOTS.items():
                pointer = self.word(GAMEPLAY, table + (stage - first) * stride + offset)
                if wrapped and pointer:
                    symbols.mark(pointer)               # the one-word wrapper ends whatever precedes it
                    pointer = self.word(owner, pointer)
                derived[stage][kind] = pointer
                pinned = STAGE_DIRECTORIES[stage][kind]
                (self.maps.of(GAMEPLAY) if kind == "areas" else symbols).mark(pinned)
                if pointer != pinned:
                    self.root_problems.append(f"stage {stage} {kind}: pinned 0x{pinned:08X}, gameplay's table at 0x{table:08X} leads to "
                                              + (f"0x{pointer:08X}" if pointer else "nothing"))
        for stage, owner in STAGE_MAPS.items():
            if stage not in derived:
                continue
            entry: dict = {"map": owner}
            for kind, address in STAGE_DIRECTORIES[stage].items():
                holder = GAMEPLAY if kind == "areas" else owner
                size = sizeof(OBJECT_ROOM) if kind == "objects" else sizeof(AREA_RECORD) if kind == "areas" else 4
                count, how = None, None
                if kind == "objects":
                    data, off, n = self.package(holder), address - self.load_of[holder], 0
                    while 0 <= off + n * size <= len(data) - size:
                        n += 1
                        if struct.unpack_from("<i", data, off + (n - 1) * size)[0] == OBJECT_ROOM_END:
                            count, how = n, "terminator"
                            break
                if count is None:
                    span, how = self.extent(holder, address)
                    count = span // size if span is not None else None
                entry[kind] = {"address": address, "owner": holder, "size": size, "count": count, "length": how,
                               "symbol": self.maps.find(holder, address)[0]}
            out[stage] = entry
        # What a directory entry points at starts an object: splat names it D_<owner>_<ADDR> whether or not a map
        # does, so it bounds the object before it like any other symbol.
        for stage, entry in out.items():
            for kind, d in entry.items():
                if kind == "map" or d["count"] is None:
                    continue
                data, off = self.package(d["owner"]), d["address"] - self.load_of[d["owner"]]
                for index in range(d["count"]):
                    room = self.room_at.get((stage, index if kind in BY_AREA else index + 1))
                    for word in struct.unpack_from(f"<{d['size'] // 4}I", data, off + index * d["size"]):
                        owner = next((o for o in (GAMEPLAY, entry["map"], room) if o and self.package(o) is not None
                                      and 0 <= word - self.load_of[o] < len(self.package(o))), None)
                        if owner:
                            self.maps.of(owner).mark(word)
        return out

    # ---- sources: citations, and the code that is read as code

    def code(self, owner: str) -> dict[str, str]:
        """The C files of an image's source directory, comments blanked, by repository path."""
        if owner not in self._code:
            folder = SOURCE_DIRS.get(self.family.get(owner, ""), "")
            texts = {}
            if folder:
                for path in sorted((ROOT / folder.format(owner)).glob("*.c")):
                    texts[path.relative_to(ROOT).as_posix()] = asset_journal.code_of(path)
            self._code[owner] = texts
        return self._code[owner]

    def declaration(self, owner: str, symbol: str) -> tuple[str, int] | None:
        """(file, line) declaring `symbol` in its image's sources: its definition, else the first line naming it."""
        if owner not in self._declared:
            found: dict[str, tuple[str, int]] = {}
            for rel, text in self.code(owner).items():
                for m in DECLARATION.finditer(text):
                    found.setdefault(m[1], (rel, text.count("\n", 0, m.start()) + 1))
            self._declared[owner] = found
        found = self._declared[owner]
        if symbol not in found:
            word = re.compile(rf"\b{re.escape(symbol)}\b")
            for rel, text in self.code(owner).items():
                m = word.search(text) if symbol in text else None
                if m:
                    found[symbol] = (rel, text.count("\n", 0, m.start()) + 1)
                    break
            else:
                found[symbol] = None
        return found[symbol]

    # Constants are collected for the readers of CODE only (a setter's `areaSetPlacementVariant(&key, VARIANT, ...)`,
    # a patch's `memCopyBytes(..., &pool[INDEX], ...)`); no table value passes through them.

    @staticmethod
    def _collect(text: str, base) -> dict[str, int]:
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

    def code_constants(self, text: str):
        if self._globals is None:
            self._globals = {"NULL": 0}
            texts = [asset_journal.code_of(p) for p in sorted((ROOT / "include").rglob("*.h")) if "psyq" not in p.parts]
            for _ in range(3):
                seen: dict[str, set[int]] = defaultdict(set)
                for header in texts:
                    for name, value in self._collect(header, self._globals).items():
                        seen[name].add(value)
                self._globals.update({name: next(iter(v)) for name, v in seen.items() if len(v) == 1})
        return ChainMap(self._collect(text, self._globals), self._globals)

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
            consts = self.code_constants(text)
            lines = text.splitlines()
            for n, line in enumerate(lines, 1):
                target, value, kind = None, None, None
                m = call.search(line)
                if m:
                    kind, value = "areaSetPlacementVariant", m[2]
                    key = m[1]
                    if "location" in key:
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
                        kind, value, target = "direct write of location.variant", m[1], "current"
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


# --------------------------------------------------------------------------- one room

class Scene:
    def __init__(self, project: Project, room: str):
        self.p, self.room = project, room
        self.stage, self.area, self.folder_id = project.place_of[room]
        self.map = STAGE_MAPS.get(self.stage)
        self.images = [owner for owner in (GAMEPLAY, self.map, room) if owner]      # what is mapped while the room runs
        self.unresolved: list[dict] = []
        self.sources: dict[str, list[dict]] = defaultdict(list)
        self.notes: list[str] = []
        self.addresses: dict[str, int] = {}     # displayed pointer -> its address
        self.records_read = 0
        self.lengths: Counter = Counter()

    # ---- helpers

    def miss(self, block: str, what: str, reason: str) -> None:
        entry = {"block": block, "what": what, "reason": reason}
        if entry not in self.unresolved:
            self.unresolved.append(entry)

    def owner_of(self, address: int) -> str | None:
        """The mapped image holding `address` (main, below gameplay, holds only names)."""
        for owner in self.images:
            data = self.p.package(owner)
            if data is not None and 0 <= address - self.p.load_of[owner] < len(data):
                return owner
        return "main" if MAIN_LOAD <= address < GAMEPLAY_LOAD else None

    def foreign(self, address: int, prefer=()) -> tuple[str | None, str | None]:
        """(name, package) of an address outside the mapped images, from what this room's and its map's symbol maps name there."""
        found = []
        for holder in (self.room, self.map, "rooms.imports", "mapui.imports"):
            found += self.p.maps.foreign.get(holder, {}).get(address, [])
        for want in prefer:         # a map overlay's table names one address in several actors: the loaded one is meant
            for name, owner in found:
                if want and owner == want:
                    return name, owner
        if found:
            return found[0]
        for want in prefer:         # no map of ours names it: the loaded package's own map does
            if want and want in self.p.maps.own and address in self.p.maps.own[want].names:
                return self.p.maps.own[want].names[address], want
        return None, None

    def ref(self, address: int | None, record: int | None = None) -> str | None:
        """A pointer as the symbol map names it: `symbol`, `symbol[index]` into an array, or the raw address."""
        if not address:
            return None
        owner = self.owner_of(address)
        if owner is None:
            text = self.foreign(address)[0] or f"0x{address:08X}"
        else:
            name, base, _size = self.p.maps.find(owner, address)
            delta = address - base
            text = name if not delta else f"{name}[{delta // record}]" if record and delta % record == 0 else f"{name}+0x{delta:X}"
        self.addresses[text] = address
        return text

    def cite(self, block: str, address: int, length: str | None = None, **extra) -> dict:
        """The citation of the symbol at (or containing) `address`: where its declaration is, else its address."""
        owner = self.owner_of(address)
        symbol, base, _size = self.p.maps.find(owner, address) if owner else (self.ref(address), address, None)
        found = self.p.declaration(owner, symbol) if owner else None
        where = {"symbol": symbol, "file": found[0], "line": found[1]} if found else {"symbol": symbol, "address": f"0x{base:08X}"}
        entry = {**where, **extra}
        if length:
            entry["length"] = length
        if entry not in self.sources[block]:
            self.sources[block].append(entry)
        return where

    def table(self, block: str, address: int, layout, *, count: int | None = None, stop=None):
        """Records of `layout` from `address`: (rows, info), or (None, None) when its length is not settled.

        The length is `count` when the caller knows the game's own; else the list's terminator (`stop(row)` is true on
        the last record), kept inside its symbol's size; else its symbol's size; else the distance to the next symbol.
        `info` has the symbol, the index in it the rows start at, the pointer as displayed and the citation.
        """
        owner = self.owner_of(address)
        data = self.p.package(owner) if owner and owner != "main" else None
        text = self.ref(address, sizeof(layout))
        if data is None:
            self.miss(block, text, "its address is in no mapped image (gameplay, the stage's map overlay, this room)")
            return None, None
        size, off = sizeof(layout), address - self.p.load_of[owner]
        symbol, base, _symbol_size = self.p.maps.find(owner, address)
        span, how = self.p.extent(owner, address)
        fits = (len(data) - off) // size
        rows = []
        if count is not None:
            how, limit = "count", min(count, fits)
        elif stop is not None:
            limit = min(span // size, fits) if how == "symbol size" else fits
        elif span is None:
            self.miss(block, text, "no terminator, no size in the symbol map and no later symbol: its length is not settled")
            return None, None
        else:
            limit = min(span // size, fits)
        ended = False
        for i in range(limit):
            row = read_bytes(layout, data, off + i * size)
            rows.append(row)
            if stop is not None and stop(row):
                ended = True
                break
        if stop is not None and count is None:
            if ended:
                how = "terminator"
            elif how != "symbol size":
                self.miss(block, text, "no terminator before the end of the package and no size in the symbol map")
                return None, None
        self.records_read += len(rows)
        self.lengths[how] += 1
        where = self.cite(block, address, how)
        return rows, {"symbol": symbol, "start": (address - base) // size, "table": text, "where": where, "length": how}

    def directory(self, kind: str, room: str | None = None):
        """This area's entry of a stage directory: (value, directory) - a pointer, None for NULL or past the end, or
        the AreaObjectRoom / AreaRecord record; `directory` is None when the stage has none."""
        stage, area, _folder = self.p.place_of[room or self.room]
        d = self.p.stages.get(stage, {}).get(kind)
        if d is None or d["count"] is None:
            return None, d
        index = area if kind in BY_AREA else area - 1
        if index >= d["count"]:
            return None, d
        data, off = self.p.package(d["owner"]), d["address"] - self.p.load_of[d["owner"]] + index * d["size"]
        if kind in BY_AREA:
            return read_bytes(OBJECT_ROOM if kind == "objects" else AREA_RECORD, data, off), d
        return read_bytes("ptr", data, off), d

    def directory_entry(self, kind: str, block: str):
        """`directory`, cited, with the reasons it has no entry listed under `unresolved`."""
        value, d = self.directory(kind)
        index = self.area if kind in BY_AREA else self.area - 1
        if d is None:
            self.miss(block, f"{kind} directory", f"no map overlay identified for stage {self.stage}")
        elif d["count"] is None:
            self.miss(block, d["symbol"], "the directory's length is not settled (no size in the symbol map, no later symbol)")
        elif index >= d["count"]:
            what = "the stage's area table" if kind == "areas" else "the stage directory"
            self.miss(block, d["symbol"], f"{what} has {d['count']} entries; area {self.area} is past its end"
                      + ("" if kind == "areas" else ", so gameplay has no table for this room"))
        else:
            self.cite(block, d["address"], d["length"], index=index)
        return value, d

    # ---- blocks

    def build(self) -> dict:
        scene: dict = {"schema": SCHEMA, "room": self.room, "stage": self.stage, "stageName": STAGE_NAMES[self.stage], "area": self.area,
                       "folderId": self.folder_id, "package": self.room, "mapOverlay": self.p.stages.get(self.stage, {}).get("map"),
                       "conventions": CONVENTIONS}
        cameras, _ = self.directory("cameras")
        if cameras and self.owner_of(cameras) != self.room:
            self.miss("views", self.ref(cameras), f"the stage directory's entry for area {self.area} is not an address in {self.room}'s package")
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
        scene["sprites"] = self.sprites()
        scene["sources"] = dict(self.sources)
        scene["checks"] = self.checks(scene)
        scene["notes"] = self.notes
        scene["unresolved"] = self.unresolved
        scene["verification"] = {"recordsReadFromPackage": self.records_read, "tableLengthsBy": dict(self.lengths), "mismatches": self.p.root_problems}
        return scene

    def room_resources(self) -> list[dict]:
        entry, _ = self.directory_entry("resources", "rooms")
        rows = []
        if entry:
            got, info = self.table("rooms", entry, ROOM_RESOURCES)
            for i, row in enumerate(got or []):
                rows.append({"roomIndex": i + 1, "table": info["symbol"], "grid": self.ref(row["grid"], sizeof(GRID)),
                             "viewBoundaryTriggers": self.ref(row["viewBoundaryTriggers"], sizeof(TRIGGER)),
                             "actionTriggers": self.ref(row["actionTriggers"], sizeof(TRIGGER)), "occluders": self.ref(row["occluders"], sizeof(OCCLUDER))})
        maps_entry, _ = self.directory_entry("maps", "views")
        counts_entry, _ = self.directory_entry("counts", "views")
        names, counts = [], []
        if maps_entry:
            got, _info = self.table("views", maps_entry, "ptr")
            names = [self.ref(x) for x in got or []]
        if counts_entry:
            counts, _info = self.table("views", counts_entry, "s16")
            counts = counts or []
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
            address = self.addresses[text]
            if data is None or self.owner_of(address) != self.room:
                self.miss("grids", text, "the grid header's address is not in this room's package")
                continue
            where = self.cite("grids", address)
            grid = self.read_grid(data, load, address - load, text)
            if grid is None:
                continue
            grid.update({"roomIndices": indices, "patch": False, "source": where, "inManifest": (address - load) in declared})
            seen.add(address - load)
            out.append(grid)
        for off in sorted(declared - seen):
            # A grid the manifest declares that no room index names: code installs it (a second floor, a changed layout).
            grid = self.read_grid(data, load, off, self.ref(load + off)) if data else None
            if grid is None:
                continue
            grid.update({"roomIndices": [], "patch": False, "source": self.cite("grids", load + off), "inManifest": True,
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
        named = self.p.maps.of(self.room).names
        code = self.p.code(self.room)
        out = []
        for part in gen_collision_inc.patch_pieces(at, pieces):
            symbol = named.get(load + part["offset"])
            if part["name"] == "vectors":
                values = [list(struct.unpack_from("<3h", data, part["offset"] + i * 8)) for i in range(part["size"] // 8)]
            else:
                values = []
                for i in range(part["size"] // 12):
                    f = read_bytes(GRID_FACE, data, part["offset"] + i * 12)
                    values.append({"v": f["v"] if f["v"][3] != 0xFFFF else f["v"][:3], "normalIndex": f["normalIndex"], "surfaceClass": f["surfaceClass"]})
            piece = {"kind": part["name"], "symbol": symbol, "packageOffset": f"0x{part['offset']:X}", "values": values, "copiedTo": []}
            if symbol:
                piece["source"] = self.cite("grids", load + part["offset"], "manifest")
                # Code, not data: where the room's own memCopyBytes call puts the piece.
                for rel, text in code.items():
                    for m in re.finditer(r"memCopyBytes\s*\(\s*" + re.escape(symbol) + r"\s*,\s*&?\s*(\w+)\s*(?:\[([^\]]+)\])?", text):
                        piece["copiedTo"].append({"pool": m[1], "index": asset_journal.evaluate(m[2], self.p.code_constants(text)) if m[2] else 0,
                                                  "file": rel, "line": text.count("\n", 0, m.start()) + 1})
            out.append(piece)
        return {"id": f"{self.room}_collision_patch_{at:05X}", "patch": True, "packageOffset": f"0x{at:X}", "pieces": out,
                "note": "headerless geometry the room's code copies into a live grid's pools (indices in a face piece are indices in that live grid); "
                        "`copiedTo` names the destination pool and first element where a memCopyBytes call in the room's C states it"}

    def surfaces(self) -> list[dict]:
        entry, _ = self.directory_entry("surfaces", "surfaceClasses")
        classes, _info = self.table("surfaceClasses", entry, "ptr") if entry else (None, None)
        out = []
        for index, pointer in enumerate(classes or []):
            if not pointer:
                out.append({"surfaceClass": index, "record": None})
                continue
            rows, _info = self.table("surfaceClasses", pointer, SURFACE, count=1)       # a class is one record
            row = rows[0] if rows else None
            entry = {"surfaceClass": index, "record": self.ref(pointer)}
            if row:
                entry.update({k: row[k] for k in ("field_0", "probePassThrough", "weaponImpactEnabled", "suppressPushback")})
                entry["footstepSounds"] = self.ref(row["footstepSounds"])
                if row["footstepSounds"]:
                    sounds, _info = self.table("surfaceClasses", row["footstepSounds"], FOOTSTEPS, count=1)
                    if sounds:
                        entry["footsteps"] = {k: f"0x{v & 0xFFFFFFFF:08X}" for k, v in sounds[0].items()}
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
            rows, info = self.table(block, self.addresses[text], TRIGGER, stop=lambda r: bool(r["flags"] & 0x80))
            if rows is None:
                continue
            if rows and not rows[-1]["flags"] & 0x80:
                self.notes.append(f"{block}: {info['symbol']} has no record with the LAST flag; all {len(rows)} records of its symbol listed")
            items = []
            for i, row in enumerate(rows):
                item = {"index": info["start"] + i, **self.quad(row), "facingNormal": v3(row["facingNormal"]), "flags": row["flags"],
                        "geometryKind": TRIGGER_KIND.get(row["flags"] & 7, row["flags"] & 7), "last": bool(row["flags"] & 0x80),
                        "control": row["control"], "parameter0": row["parameter0"], "parameter1": row["parameter1"]}
                if block == "boundaries":
                    item["fromView"], item["toView"] = row["parameter0"], row["parameter1"]
                else:
                    item.update(self.action(row))
                items.append(item)
            out.append({"table": text, "roomIndices": indices, "source": info["where"], "items": items})
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
            rows, info = self.table("occluders", self.addresses[text], OCCLUDER, stop=lambda r: bool(r["flags"] & 0x80))
            if rows is None:
                continue
            out.append({"table": text, "roomIndices": indices, "source": info["where"],
                        "items": [{"index": info["start"] + i, **self.quad(r), "flags": r["flags"], "last": bool(r["flags"] & 0x80)} for i, r in enumerate(rows)]})
        return out

    @staticmethod
    def endpoint(t: dict) -> dict:
        word = t["yaw"]
        return {"position": [t["x"], t["y"], t["z"]], "yaw": word, "yawSpecial": YAW_SPECIAL.get(word)}

    def warp_entries(self, room: str, block: str | None) -> tuple[list[dict] | None, dict | None]:
        """A room's arrival endpoints, through its own stage directory entry (read with that room mapped)."""
        scene = self if block else Scene(self.p, room)
        entry, _ = scene.directory("warps")
        if not entry:
            return None, None
        return scene.table(block or "warps", entry, WARP)

    def warps(self, triggers: list[dict]) -> dict:
        self.directory_entry("warps", "warps")
        rows, info = self.warp_entries(self.room, "warps")
        out = {"table": info["symbol"] if info else None, "source": info["where"] if info else None, "entries": [], "exits": [], "redirects": []}
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
        # Code, not data: the room's handler rewriting where a warp leads.
        pattern = re.compile(r"\breply->(areaId|room|warp|stage)\s*=(?!=)")
        for rel, text in self.p.code(self.room).items():
            for n, line in enumerate(text.splitlines(), 1):
                if pattern.search(line):
                    out["redirects"].append({"file": rel, "line": n, "text": line.strip()})
        return out

    def views(self, rooms: list[dict], boundaries: list[dict], warps: dict) -> list[dict]:
        entry, _ = self.directory_entry("cameras", "views")
        cameras, camera_info = self.table("views", entry, VIEW_CAMERA) if entry else (None, None)
        sprite_entry, _ = self.directory_entry("sprites", "views")
        sprite_symbol = self.ref(sprite_entry)
        if sprite_entry:
            self.cite("views", sprite_entry)
        out = []
        for row in rooms:
            name, count = row.get("viewMap"), row.get("viewCount")
            index = row["roomIndex"]
            if not name:
                continue
            block = {"roomIndex": index, "map": name, "viewCount": count, "cameraTable": camera_info["symbol"] if camera_info else None,
                     "spriteTable": sprite_symbol, "mapSource": None, "mapWriters": [], "views": []}
            identity = self.addresses[name] == IDENTITY_MAP
            mapping, info = self.table("views", self.addresses[name], "u8", count=IDENTITY_MAP_LENGTH if identity else None)
            if mapping is None:
                continue
            block["mapSource"] = info["where"]
            if identity:
                block["mapSource"] = {**info["where"], "note": f"gameplay's shared identity map, {IDENTITY_MAP_LENGTH} entries"}
            else:
                # Code, not data: the room's own writes to the map.
                writer = re.compile(r"\b" + re.escape(info["symbol"]) + r"\s*\[[^\]]+\]\s*[|&+-]?=(?!=)")
                for rel, text in self.p.code(self.room).items():
                    for n, line in enumerate(text.splitlines(), 1):
                        if writer.search(line) and not line.lstrip().startswith(("u8", "static")):
                            block["mapWriters"].append({"file": rel, "line": n, "text": line.strip()})
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
                    item["background"] = {"fileKey": f"{self.stage}/{self.room}/file{file}", "bs": kind("bs"), "images": kind("pe2img") + kind("font_atlas"), "cluts": kind("pe2clut")}
                    if not item["background"]["bs"]:
                        item["note"] = "the view's file has no bs chunk: no background picture of its own"
                else:
                    item["note"] = f"the room's folder has no file {file}: nothing is loaded for this view (src/main/fs.c), the camera still applies"
                if cameras is not None and 1 <= file <= len(cameras):
                    item["camera"] = self.camera(cameras[file - 1], file - 1)
                elif cameras is not None:
                    self.miss("views", f"room index {index} view {view}", f"file {file} is past the {len(cameras)}-entry camera array")
                block["views"].append(item)
            out.append(block)
        return out

    def sprites(self) -> dict:
        """The area's `SpriteView` array, one record per view file (the mapped view index, 1-based, less one).

        The array has no count or terminator: its length is its symbol's size. A view's batch list ends at
        `firstSprite == SPRITE_BATCH_END`; its source array has no sentinel, so its length is the highest
        `firstSprite + spriteCount` of the nonterminal batches, held against the array's own extent. A draw-area
        list ends at `restoreDepth == SPRITE_DRAW_AREA_END`. A view whose source slot is NULL or repeats its
        batch-list address has no sources (`SpriteView.sources.empty`).

        Its records are counted here (`recordsRead`, `tableLengthsBy`) and not in the scene's `verification`.
        """
        before, lengths = self.records_read, Counter(self.lengths)
        out: dict = {"table": None, "source": None, "fields": list(SPRITE_ROW), "views": []}
        entry, _ = self.directory("sprites")
        if entry:
            if self.owner_of(entry) != self.room:
                self.miss("sprites", self.ref(entry), f"the stage directory's entry for area {self.area} is not an address in {self.room}'s package")
            rows, info = self.table("sprites", entry, SPRITE_VIEW)
            if info:
                out["table"], out["source"] = info["table"], info["where"]
            for i, row in enumerate(rows or []):
                what = f"{out['table']}[{i}]"
                view = {"file": i + 1, "sources": None, "batches": self.ref(row["batches"], sizeof(SPRITE_BATCH)),
                        "drawAreas": self.ref(row["drawAreas"], sizeof(SPRITE_DRAW_AREA)), "background": None, "batchList": [],
                        "drawAreaList": [], "sourceCount": 0, "pages": [], "rows": []}
                batches = None
                if row["batches"]:
                    batches, _i = self.table("sprites", row["batches"], SPRITE_BATCH, stop=lambda b: b["firstSprite"] == SPRITE_BATCH_END)
                if batches:
                    # The first record's count selects the background, terminal or not (sprites.h).
                    view["background"] = "decoded strips" if batches[0]["spriteCount"] == 0 else "suppressed"
                    view["batchList"] = [{"firstSprite": b["firstSprite"], "spriteCount": b["spriteCount"], "hidden": b["hidden"],
                                          "skipCachedPackets": b["skipCachedPackets"], "field_6": b["field_6"]}
                                         for b in batches if b["firstSprite"] != SPRITE_BATCH_END]
                elif row["batches"]:
                    self.miss("sprites", what, "its batch list could not be read")
                else:
                    self.miss("sprites", what, "its batch list is NULL")
                if row["drawAreas"]:
                    areas, _i = self.table("sprites", row["drawAreas"], SPRITE_DRAW_AREA, stop=lambda a: a["restoreDepth"] == SPRITE_DRAW_AREA_END)
                    view["drawAreaList"] = [[a["x"], a["y"], a["w"], a["h"], a["restoreDepth"]] for a in areas or [] if a["restoreDepth"] != SPRITE_DRAW_AREA_END]
                count = max([b["firstSprite"] + b["spriteCount"] for b in view["batchList"]] or [0])
                has_sources = bool(row["sources"]) and row["sources"] != row["batches"]
                if count and not has_sources:
                    self.miss("sprites", what, f"its batches reach source {count} and its source slot is " + ("its batch list" if row["sources"] else "NULL"))
                elif count:
                    view["sources"] = self.ref(row["sources"], sizeof(SPRITE_SOURCE))
                    owner = self.owner_of(row["sources"])
                    span, how = self.p.extent(owner, row["sources"]) if owner and owner != "main" else (None, None)
                    if how == "symbol size" and count * sizeof(SPRITE_SOURCE) > span:
                        self.miss("sprites", what, f"its batches reach source {count}, past the {span // sizeof(SPRITE_SOURCE)} sources of {view['sources']}")
                        count = span // sizeof(SPRITE_SOURCE)
                    sources, _i = self.table("sprites", row["sources"], SPRITE_SOURCE, count=count)
                    drawn = set()
                    for b in view["batchList"]:
                        drawn.update(range(b["firstSprite"], b["firstSprite"] + b["spriteCount"]))
                    pages: dict[tuple[int, int], dict] = {}
                    for n, src in enumerate(sources or []):
                        view["rows"].append([src[k] for k in SPRITE_ROW])
                        if n not in drawn:
                            continue                # between two batches: in the array, drawn by no range
                        depth = TPAGE_DEPTH[(src["tpage"] >> 7) & 3]
                        page = pages.setdefault((src["tpage"], src["clut"]), {
                            "tpage": src["tpage"], "clut": src["clut"], "bpp": depth, "pageX": (src["tpage"] & 15) * 64,
                            "pageY": 256 if src["tpage"] & 16 else 0, "blend": (src["tpage"] >> 5) & 3, "clutX": (src["clut"] & 63) * 16,
                            "clutY": src["clut"] >> 6, "count": 0, "codeFlags": [], "texels": [255, 255, 0, 0]})
                        page["count"] += 1
                        if src["codeFlags"] not in page["codeFlags"]:
                            page["codeFlags"] = sorted(page["codeFlags"] + [src["codeFlags"]])
                        t = page["texels"]          # the texels the page's rectangles span: u, v of the first and the last
                        page["texels"] = [min(t[0], src["u0"]), min(t[1], src["v0"]), max(t[2], src["u0"] + src["w"] - 1), max(t[3], src["v0"] + src["h"] - 1)]
                    view["sourceCount"] = len(view["rows"])
                    view["pages"] = [pages[k] for k in sorted(pages)]
                out["views"].append(view)
        out["recordsRead"] = self.records_read - before
        out["tableLengthsBy"] = dict(Counter(self.lengths) - lengths)
        self.records_read, self.lengths = before, lengths
        return out

    @staticmethod
    def camera(row: dict, index: int) -> dict:
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
            package = next((self.p.assets[c].get("package_name", c) for c in out["chunks"] if self.p.assets.get(c, {}).get("type") in ("pe2pkg", "room", "stage")), None)
            out["package"] = package
            out["models"] = list(self.p.models_of.get(package, [])) if package else []
        return out

    def task_table(self, address: int | None, package: str | None, loaded: list) -> tuple[str | None, str | None]:
        """(name, actor) of a resource's task table: an address in the actor overlay the resource's file loads."""
        if not address:
            return None, None
        if self.owner_of(address):
            return self.ref(address, sizeof(TASK_DESC)), None
        name, owner = self.foreign(address, [package, *loaded])
        if name is None:
            text = f"0x{address:08X}"
            self.miss("placements", text, "task table address is named by no symbol map (this room's, its map overlay's, the loaded actors')")
            return text, None
        return name, self.p.actor_of(name) or (owner if owner and owner.startswith("actor_") else None)

    def placements(self) -> dict:
        out = {"table": None, "source": None, "variants": [], "defaultVariant": 1}
        record, _ = self.directory_entry("areas", "placements")
        if record is None:
            return out
        if not record["variants"]:
            self.notes.append(f"placements: gAreaRecordsByStage[{self.stage}][{self.area}] has no variant table")
            return out
        variants, info = self.table("placements", record["variants"], VARIANT)
        out["table"], out["source"] = self.ref(record["variants"]), info["where"] if info else None
        setters = self.p.setters()
        mine = list(setters["room"].get(self.room, []))
        actors_here = set()
        for index, row in enumerate(variants or []):
            if index == 0 and not row["placements"] and not row["resources"]:
                continue        # slot 0 is never a layout (include/gameplay/area.h)
            # A slot storing NULL/NULL is still a selectable variant: the room with nothing placed.
            rsym, psym = self.ref(row["resources"], sizeof(RESOURCE)), self.ref(row["placements"], sizeof(PLACEMENT))
            entry = {"variant": index, "default": index == 1, "empty": not row["placements"] and not row["resources"],
                     "placementsTable": psym, "resourcesTable": rsym,
                     "resources": [], "placed": [], "setters": [], "hasSetter": False}
            resources = {}
            rrows, rinfo = self.table("placements", row["resources"], RESOURCE, stop=lambda r: r["entryId"] == 0xFF) if rsym else (None, None)
            entry["resourcesSource"] = rinfo["where"] if rinfo else None
            rrows = [r for r in rrows or [] if r["entryId"] != 0xFF]
            files = [self.resolve_file(r) for r in rrows]
            for r, file in zip(rrows, files):
                task, actor = self.task_table(r["taskTable"], file["package"], [f["package"] for f in files])
                item = {"entryId": r["entryId"], "fileNumber": r["fileNumber"], "fileGroupIndex": r["fileGroupIndex"],
                        "groupBase": GROUP_BASE[r["fileGroupIndex"]] if r["fileGroupIndex"] < len(GROUP_BASE) else None,
                        "taskIndex": r["taskIndex"], "taskTable": task, "actor": actor, **file}
                if item["fileExists"] is False:
                    item["note"] = "the disc has no file with this id: nothing is loaded for it (src/main/fs.c)"
                if actor:
                    actors_here.add(actor)
                resources.setdefault(r["entryId"], (r, item))
                entry["resources"].append(item)
            prows, pinfo = self.table("placements", row["placements"], PLACEMENT, stop=lambda r: r["entryId"] == 0xFF) if psym else (None, None)
            entry["placementsSource"] = pinfo["where"] if pinfo else None
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
        row, _ = self.directory_entry("objects", "objects")
        placed_models = set()
        if row:
            spawns = {}
            if row["spawns"]:
                srows, sinfo = self.table("objects", row["spawns"], OBJECT_SPAWN, stop=lambda r: r["kind"] == 0xFFFF)
                out["spawnsTable"] = self.ref(row["spawns"], sizeof(OBJECT_SPAWN))
                for r in srows or []:
                    if r["kind"] == 0xFFFF:
                        break
                    task = r["taskDesc"]
                    flags = task["header"]["flags"]
                    model = self.ref(task["data"]) if is_pointer(task["data"]) else None
                    key = self.model_key(task["data"]) if model and (flags & 0xFF) == 1 else None
                    if key:
                        placed_models.add(key)
                    spawn = {"kind": r["kind"], "body": {0: "none", 1: "tmd", 2: "coord"}.get(flags & 0xFF, flags & 0xFF),
                             "bodyFlags": flags, "priority": task["header"]["priority"], "callback": self.ref(task["callback"]), "model": model, "modelKey": key,
                             "data": task["data"] if model is None else None}
                    spawns.setdefault(r["kind"], spawn)
                    out["spawns"].append(spawn)
            if is_pointer(row["places"]):
                prows, pinfo = self.table("objects", row["places"], OBJECT_PLACE, stop=lambda r: r["flagIndex"] == 0xFFFF)
                out["placesTable"] = self.ref(row["places"], sizeof(OBJECT_PLACE))
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

    def model_key(self, address: int) -> str | None:
        """The asset key of the model a TmdSource record draws: its stream, matched to the catalog."""
        text = self.ref(address)
        owner = self.owner_of(address)
        if owner is None:
            # An actor overlay's model: the map naming it says whose, and the record's stream word is the catalog's address.
            _name, owner = self.foreign(address)
            stream = self.p.word(owner, address + 0x20) if owner in self.p.load_of else None
            key = self.p.model_at.get((owner, stream))
            if owner is None:
                self.miss("objects", text, "model source is in no mapped image and no symbol map says whose it is")
            elif key is None:
                self.miss("objects", text, f"its stream in {owner} is not a cataloged model")
            return key
        data, load = self.p.package(self.room), self.p.load["rooms"]
        if owner != self.room or address - load + 0x24 > len(data):
            self.miss("objects", text, "model source address is outside this room's package")
            return None
        stream = struct.unpack_from("<I", data, address - load + 0x20)[0]
        key = self.p.model_at.get((self.room, stream))
        if key is None:
            # A mesh several packages carry is cataloged once, under its SHA-1 (asset_data.MODELS).
            try:
                key = tmd.catalog_name(data, load, address - load)
            except Exception:  # noqa: BLE001 - an unreadable stream is reported below
                key = None
        if key is None:
            self.miss("objects", text, f"its stream at 0x{stream:08X} is not a cataloged model")
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
    lengths = Counter()
    mismatches = []
    sprite, sprite_lengths = Counter(), Counter()
    for room, s in scenes.items():
        sprite["rooms"] += bool(s["sprites"]["views"])
        sprite["views"] += len(s["sprites"]["views"])
        sprite["withSources"] += sum(1 for v in s["sprites"]["views"] if v["sourceCount"])
        sprite["sources"] += sum(v["sourceCount"] for v in s["sprites"]["views"])
        sprite["pages"] += sum(len(v["pages"]) for v in s["sprites"]["views"])
        sprite["read"] += s["sprites"]["recordsRead"]
        sprite_lengths.update(s["sprites"]["tableLengthsBy"])
        cams["views"] += s["checks"]["camerasResolved"]["views"]
        cams["withCamera"] += s["checks"]["camerasResolved"]["withCamera"]
        yaw["compared"] += s["checks"]["warpYawAgreesWithTrigger"]["compared"]
        yaw["agree"] += s["checks"]["warpYawAgreesWithTrigger"]["agree"]
        quads["quads"] += s["checks"]["gridQuadsConvexInStripOrder"]["quads"]
        quads["convex"] += s["checks"]["gridQuadsConvexInStripOrder"]["convex"]
        verify["read"] += s["verification"]["recordsReadFromPackage"]
        lengths.update(s["verification"]["tableLengthsBy"])
        mismatches = s["verification"]["mismatches"]        # the pinned roots, the same list in every scene
    lines += ["", "sanity checks (XZ distance outside the union of the room's grid bounds):"]
    for key, c in checks.items():
        lines.append(f"  {key:<14} {c['count']:>5} points, {c['insideGridBoundsXZ']:>5} inside; outside: {c['outside'] or 'none'}")
        for f in c["farthest"][:5]:
            lines.append(f"      {f['room']}  {f['what']}  at {f['position']}  {f['outsideXZ']} units outside")
    lines += [f"  view files     {files} referenced, missing in {len(missing)} rooms: {missing or 'none'}",
              f"  cameras        {cams['withCamera']} of {cams['views']} mapped views have a camera record",
              f"  warp yaw       {yaw['agree']} of {yaw['compared']} warp triggers agree with forward(yaw) = (sin, 0, cos) of their departure endpoint",
              f"  grid quads     {quads['convex']} of {quads['quads']} are convex in strip order (loop 1, 0, 2, 3)",
              f"  sprites        {sprite['views']} view records in {sprite['rooms']} rooms, {sprite['withSources']} with sources, {sprite['sources']} sources "
              f"on {sprite['pages']} (page, CLUT) pairs; {sprite['read']} records read; table lengths by {dict(sprite_lengths)}",
              f"  package bytes  {verify['read']} records read; table lengths by {dict(lengths)}; {len(mismatches)} pinned roots disagree with gameplay"]
    lines += [f"      {m}" for m in mismatches[:40]]
    summary = {"rooms": len(scenes), "errors": errors, "coverage": {b: dict(table[b]) for b in BLOCKS},
               "failures": [{"room": r, "block": b, "unresolved": u} for r, b, u in failures], "checks": checks,
               "viewFilesMissing": missing, "cameras": dict(cams), "warpYaw": dict(yaw), "gridQuads": dict(quads),
               "verification": {**dict(verify), "tableLengthsBy": dict(lengths)}, "mismatches": mismatches,
               "sprites": {**dict(sprite), "tableLengthsBy": dict(sprite_lengths)}}
    return summary, "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("room", nargs="?")
    parser.add_argument("--all", action="store_true", help="every room overlay")
    parser.add_argument("--out", type=Path, help="directory for --all")
    parser.add_argument("--indent", type=int, default=None, help="pretty-print with this indent")
    args = parser.parse_args()
    project = Project()
    roots = {"gameplay": {kind: {"address": f"0x{root[0]:08X}", "symbol": project.maps.find(GAMEPLAY, root[0])[0]} for kind, root in GAMEPLAY_ROOTS.items()},
             "identityMap": {"address": f"0x{IDENTITY_MAP:08X}", "symbol": project.maps.find(GAMEPLAY, IDENTITY_MAP)[0]},
             "stages": {stage: {kind: ({**d, "address": f"0x{d['address']:08X}"} if isinstance(d, dict) else d) for kind, d in entry.items()}
                        for stage, entry in project.stages.items()}}
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
        summary["roots"] = roots
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
