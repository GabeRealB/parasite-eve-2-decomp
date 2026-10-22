"""Reaching a room's data in its package from its entry points, in the bytes alone.

The images are mapped at their load addresses - gameplay at 0x80093800, the stage overlay at 0x80179950, the room at
0x8017D5C0 - so pointers resolve across them. A room's implicit entry points are its stage overlay's per-area tables
(indexed by the room's area: its folder's disc id divided by 100) and gameplay's area table (the area's placement
variants); its record names the rest (`entries`: what only code names). Symbol maps give names only.

Extents come from the bytes: sprite batches, draw areas, triggers, occluders, placements, actor resources, area
updates and vibration scripts end at their own end marks, a room's light arrays and an object only code names reach
the next object, the per-state tables share the count of those holding pointers (the view counts, halfwords, may be
followed by padding), and every other object reaches the next object the symbol maps or a pointer starts.
"""
from __future__ import annotations

import re
import struct
from pathlib import Path

from .events import payload_targets
from .layouts import GEOMETRY, MODULE_OF, SCALAR, TRIGGER_MODULE, decode_value, size

ROOT = Path(__file__).resolve().parents[3]
GAMEPLAY_LOAD, STAGE_LOAD, ROOM_LOAD = 0x80093800, 0x80179950, 0x8017D5C0

# Each game stage's overlay; gameplay points to its per-area tables, one table pointer per stage (from stage 1) for
# each kind of table - five of them through a one-word wrapper in the overlay (ViewCameraTable and the like).
STAGES = {1: ("map_akropolis",), 2: ("map_dryfield",), 3: ("map_dryfield_full",), 4: ("map_shelter",), 5: ("map_neo_ark",)}
GAMEPLAY_TABLES = {  # gameplay symbol: (the layout an entry points to, wrapped)
    "gWorldCollisionStageResources": ("WorldCollisionRoomResources", True), "gViewIndexTables": ("ViewMapPtr", True),
    "gViewCountTables": ("ViewCount", True), "gWorldCoordRoomLightingTables": ("WorldCoordRoomLighting", False),
    "gViewCameraTables": ("ViewCamera", True), "gSpriteAreaTables": ("SpriteView", True),
    "gDirectionAreaWarpEntriesByStage": ("DirectionWarpEntry", False),
    "gWorldCollisionSurfacePropertiesTables": ("SurfacePtr", False),
}
PER_STATE = ("WorldCollisionRoomResources", "ViewMapPtr", "WorldCoordRoomLighting")
END_MARKS = {
    "SpriteBatch": lambda d, o: struct.unpack_from("<H", d, o)[0] == 0xFFFF,
    "SpriteDrawArea": lambda d, o: struct.unpack_from("<H", d, o + 8)[0] == 0xFFFF,
    "WorldCollisionTrigger": lambda d, o: d[o + 0x4A] & 0x80,
    "WorldCollisionOccluder": lambda d, o: d[o + 0x3A] & 0x80,
    "AreaPlacement": lambda d, o: d[o] == 0xFF,
    "AreaResource": lambda d, o: struct.unpack_from("<H", d, o)[0] == 0xFF,
    "AreaApplyRec": lambda d, o: d[o] == 0xFF,
    "PadScriptCmd": lambda d, o: struct.unpack_from("<I", d, o)[0] == 0,
    "AreaObjectPlace": lambda d, o: struct.unpack_from("<H", d, o)[0] == 0xFFFF,
    "AreaObjectRoom": lambda d, o: struct.unpack_from("<I", d, o)[0] == 0xFFFFFFFF,
}


def symbol_map(path: Path, owner: str | None = None) -> dict[int, str]:
    """Address -> name from a symbol map; a stage map's room symbols only for `owner`."""
    out = {}
    for line in path.read_text().splitlines():
        m = re.match(r"(\w+) = (0x[0-9A-Fa-f]+);(.*)", line)
        if m:
            o = re.search(r"owner=(\w+)", m.group(3))
            if o is None or o.group(1) == owner:
                out[int(m.group(2), 16)] = m.group(1)
    return out


class Images:
    """Gameplay, a stage overlay and a room, mapped at their load addresses, with the names the symbol maps give.
    `own` is the image being decoded - the room's, or the stage overlay's where `room_name` is None - at `base`."""

    def __init__(self, stage: int, stage_image: bytes, room_name: str | None, room: bytes, gameplay: bytes):
        self.stage, (self.stage_name,) = stage, STAGES[stage]
        self.stage_image, self.room_name, self.room, self.gameplay = stage_image, room_name, room, gameplay
        self.own, self.base = (room, ROOM_LOAD) if room_name else (stage_image, STAGE_LOAD)
        sym = ROOT / "configs" / "USA"
        self.names = {**symbol_map(sym / "sym.gameplay.txt"),
                      **symbol_map(sym / "sym" / "mapui" / f"{self.stage_name}.txt", room_name or "-"),
                      **(symbol_map(sym / "sym" / "rooms" / f"{room_name}.txt") if room_name else {})}
        self.addresses = {n: a for a, n in self.names.items()}
        self.directories = self.stage_tables()

    def stage_tables(self) -> dict[str, tuple[int, int | None]]:
        """{layout: (the stage's per-area table, its wrapper or None)}, as gameplay points to them."""
        out = {}
        for symbol, (layout, wrapped) in GAMEPLAY_TABLES.items():
            p = self.word(self.addresses[symbol] + 4 * (self.stage - 1))
            out[layout] = (self.word(p), p) if wrapped else (p, None)
        return out

    def in_own(self, a: int) -> bool:
        return self.base <= a < self.base + len(self.own)

    in_room = in_own

    def word(self, a: int) -> int:
        for base, image in ((ROOM_LOAD, self.room), (GAMEPLAY_LOAD, self.gameplay), (STAGE_LOAD, self.stage_image)):
            if not image:
                continue
            if base <= a < base + len(image):
                return struct.unpack_from("<I", image, a - base)[0]
        raise SystemExit(f"0x{a:08X} is in no mapped image")


def _collision():
    """tools/buildsteps/gen_collision_inc.py, which cuts a grid into its parts."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("gen_collision_inc", ROOT / "tools" / "buildsteps" / "gen_collision_inc.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Object:
    """A room object: where it is, its layout, its elements, its module, and its C form (single, or rows)."""

    def __init__(self, address: int, kind: str, count: int, module: str, single: bool = False, rows: int = 0):
        self.address, self.kind, self.count, self.module, self.single, self.rows = address, kind, count, module, single, rows

    @property
    def end(self) -> int:
        return self.address + self.count * size(self.kind)


# The roles of a room's entry points: those the stage overlay's per-area tables and gameplay's area table give, by
# the layout they point to, and those a record states (`entries`), each with its kind and module.
IMPLICIT_ROLES = {"WorldCollisionRoomResources": "collision", "ViewMapPtr": "view_maps", "ViewCount": "view_counts",
                  "WorldCoordRoomLighting": "lighting", "ViewCamera": "cameras", "SpriteView": "sprite_views",
                  "DirectionWarpEntry": "warps", "SurfacePtr": "surfaces", "AreaVariant": "variants"}
STATED_ROLES = {"action_triggers": ("WorldCollisionTrigger", "action_triggers"),
                "geometry": ("WorldCollisionGrid", "geometry"), "lights": ("WorldCoordRoomLights", "lights"),
                "spare_batches": ("SpriteBatch", "sprites"), "spare_draw_areas": ("SpriteDrawArea", "sprites"),
                "updates": ("AreaApplyRec", "updates"), "markers": ("SVECTOR", "markers"),
                "samples": ("s16", "samples"), "view_masks": ("u16", "view_masks"),
                "wide_view_masks": ("u32", "view_masks"),
                # events: the scripts, and the payloads code sends or plays itself
                "scripts": ("EvsCommand", "events"), "animations": ("AnimationPlayRequest", "events"),
                "transforms": ("ActorTransform", "events"), "actor_commands": ("ActorCommand", "events"),
                "bank_copies": ("AnimationBankCopyRequest", "events"), "scene_keys": ("EvsSceneKey", "events"),
                "move_animations": ("GameActorMoveAnim", "events"), "walk_animations": ("ActorMotionWalkAnim", "events"),
                # a room's own: a jukebox's track lists, the shooting gallery's wave scripts and target scores
                "jukebox": ("JukeboxTrackLists", "jukebox"), "wave_scripts": ("_MistShootingGallerySpawn", "waves"),
                "target_scores": ("_MistShootingGalleryTargetScore", "scores")}
EVENT_PAYLOADS = frozenset({"AnimationPlayRequest", "ActorTransform", "ActorCommand", "AnimationBankCopyRequest",
                            "EvsSceneKey", "GameActorMoveAnim", "ActorMotionWalkAnim"})
ROLE_KINDS = {**{r: k for k, r in IMPLICIT_ROLES.items()}, **{r: k for r, (k, _m) in STATED_ROLES.items()},
              "commands": "PadScriptCmd", "segments": "PadScriptVibrationSegment"}


def entry_points(entries: dict) -> list[tuple[int, str, str, bool, int, str]]:
    """(offset, kind, module, single, rows, role) of a record's entries, by role: `action_triggers`, `geometry`
    (grids), `lights` (light sets), `spare_batches`, `spare_draw_areas`, `updates`, and the tables the room's code
    places effects with - `markers` (world points), `view_masks` and `wide_view_masks` (a bit per view, halfwords
    or words: which views a marker shows in) and `samples` (curves) - list offsets, or {at, single, rows}; `vibrations` lists {commands, segments, single (one
    segment)}. `view_counts` is not an entry point: it is the length of the view-count table where the C's is longer
    than the room's states need (zeros past them)."""
    out = []
    for role, items in entries.items():
        if role == "view_counts":
            continue
        for e in items:
            if role == "vibrations":
                out.append((e["commands"], "PadScriptCmd", "vibrations", False, 0, "vibrations"))
                out.append((e["segments"], "PadScriptVibrationSegment", "vibrations", bool(e.get("single")), 0, "vibrations"))
                continue
            e = e if isinstance(e, dict) else {"at": e}
            kind, module = STATED_ROLES[role]
            out.append((e["at"], kind, module, bool(e.get("single")), int(e.get("rows", 0)), role))
    return out


def walk(images: Images, area: int, entries: dict) -> tuple[int, dict[int, Object], dict[str, list]]:
    """(states, {address: object}, {role: entry points}) of every object the room's entry points reach in the room;
    a vibration's entry point is its (commands, segments)."""
    entry = {}
    tables = sorted(t for t, _w in images.directories.values())
    areas = min(b - a for a, b in zip(tables, tables[1:])) // 4    # the tables are adjacent, one entry an area
    for layout, (table, _wrapper) in images.directories.items():
        p = images.word(table + 4 * (area - 1)) if area <= areas else 0
        if p:
            entry[layout] = p
    records = images.word(images.addresses["gAreaRecordsByStage"] + 4 * images.stage)
    variants = images.word(records + 8 * area)                # AreaRecord.variants
    stated = entry_points(entries)
    extra = [(images.base + at, kind, module, single, rows) for at, kind, module, single, rows, _r in stated]
    roots = [(a, k, MODULE_OF[k], False, 0) for k, a in entry.items()]
    if variants:
        roots.append((variants, "AreaVariant", "placements", False, 0))
    in_room = [k for k in PER_STATE if k in entry and images.in_own(entry[k])]

    def counts(extent):
        states = min((extent(entry[k], k) for k in in_room), default=1)
        return {(entry["ViewCount"], "ViewCount"): entries.get("view_counts", states)} if "ViewCount" in entry else {}

    objs, extent = _reach(images, roots, extra, counts)
    states = min((extent(entry[k], k) for k in in_room), default=1)
    roles: dict[str, list] = {}
    for k, a in entry.items():
        if images.in_own(a):
            roles.setdefault(IMPLICIT_ROLES[k], []).append(a)
    if variants and images.in_own(variants):
        roles["variants"] = [variants]
    for at, kind, _m, _s, _r, role in stated:
        if role == "vibrations":
            if kind == "PadScriptCmd":
                roles.setdefault(role, []).append([images.base + at])
            else:
                roles[role][-1].append(images.base + at)
        else:
            roles.setdefault(role, []).append(images.base + at)
    return states, objs, roles


def _reach(images: Images, roots: list, extra: list, counts_of) -> tuple[dict[int, "Object"], object]:
    """Every object the roots and the entry points (`extra`, which only code names: they reach the next object)
    reach in the image being decoded, and the extent function its last pass used. `counts_of(extent)` gives the
    counts the holders state (the view counts)."""
    own, base = images.own, images.base
    code_named = {a for a, *_ in extra}
    starts = {a for a in images.names if images.in_own(a)} | {a for a, *_ in roots + extra}
    own_end = base + len(own)
    while True:
        def extent(a: int, kind: str) -> int:
            return (min([s for s in starts if s > a] + [own_end]) - a) // size(kind)
        counts = counts_of(extent)
        queue = list(roots) + list(extra)
        objs: dict[int, Object] = {}
        while queue:
            a, kind, module, single, rows = queue.pop(0)
            if kind == "ref" or not images.in_own(a) or a in objs:
                continue
            if kind == "SpriteSources":
                kind = "SpriteSource"
            mark = END_MARKS.get(kind)
            if single:
                n = 1
            elif (a, kind) in counts:
                n = counts[(a, kind)]
            elif mark and a not in code_named:
                k = 0
                while not mark(own, a - base + k * size(kind)):
                    k += 1
                n = min(k + 1, extent(a, kind))
            elif kind == "WorldCoordRoomAmbientEntry":
                n = struct.unpack_from("<h", own, a - base)[0] + 1
            else:
                n = extent(a, kind)
            single = single or kind in SCALAR or (kind in EVENT_PAYLOADS and n == 1)   # a payload: one, unless several
            objs[a] = Object(a, kind, n, module, single, rows)
            if kind == "EvsCommand":                          # a script: what its commands point to
                for i in range(n):
                    words = struct.unpack_from("<6i", own, a - base + 24 * i)
                    for target, tkind in payload_targets(words):
                        if tkind == "EvsCommand" and target not in images.names:
                            continue                          # a command inside a script, not a script of its own
                        queue.append((target, tkind, MODULE_OF.get(tkind, "events"), False, 0))
                continue
            if kind in GEOMETRY:
                continue
            if kind == "WorldCollisionGrid":                  # its geometry, as the collision asset cuts it
                for part in _collision().components(own, base, a - base):
                    pk = next(k for k, v in GEOMETRY.items() if v == part["name"])
                    objs[base + part["offset"]] = Object(base + part["offset"], pk, part["size"] // size(pk), "geometry")
            for i in range(n):
                refs: list = []
                value = decode_value(own, a - base + i * size(kind), kind, refs)
                for field, target, tkind in refs:
                    if not target or tkind == "ref":
                        continue
                    if kind == "WorldCoordRoomLights" and tkind != "WorldCoordRoomLights":
                        count = value[{"directionalLights": "directionalLightCount", "pointLights": "pointLightCount",
                                       "coneLights": "coneLightCount"}[field]]
                        if extent(target, tkind) < count:
                            raise SystemExit(f"0x{target:08X}: {count} lights do not fit before the next object")
                    if kind == "SpriteView" and field == "sources" and target == value["batches"]:
                        continue                              # no sources: the slot keeps the view's batch list
                    queue.append((target, tkind, TRIGGER_MODULE.get(field) or MODULE_OF.get(tkind, module), False, 0))
        new = set(objs) - starts
        if not new:
            return objs, extent
        starts |= new


# The stage overlay: its per-area tables and their wrappers (gameplay's roots), its area objects, the placement lists
# its rooms' variants name, and what its record states (`entries`), by role.
WRAPPERS = {"WorldCollisionRoomResources": "WorldCollisionStageResources", "ViewMapPtr": "ViewIndexTable",
            "ViewCount": "ViewCountTable", "ViewCamera": "ViewCameraTable", "SpriteView": "SpriteAreaTable"}
STAGE_ROLES = {"map_areas": ("MenuMapArea", "map"), "map_shapes": ("MenuMapAreaShape", "map"),
               "map_markers": ("MenuMapMarker", "map"), "map_icons": ("MenuMapIcon", "map"),
               "map_names": ("MenuMapAreaName", "map"), "page_flags": ("u8", "map"), "marker_flags": ("u16", "map"),
               "image_slots": ("GfxImageSlot", "memory"), "movie_workspaces": ("s32", "memory"),
               "music": ("StageMusicEntry", "music"), "rewards": ("InventoryBattleReward", "rewards")}
STAGE_ROLE_KINDS = {**{f"{r}_table": f"AreaTable{k}" for k, r in IMPLICIT_ROLES.items() if k != "AreaVariant"},
                    **{f"{IMPLICIT_ROLES[k]}_wrapper": w for k, w in WRAPPERS.items()},
                    "objects": "AreaObjectRoom", "placements": "AreaPlacement",
                    **{r: k for r, (k, _m) in STAGE_ROLES.items()}}


def walk_stage(images: Images, placements: list[int], entries: dict) -> tuple[dict[int, Object], dict[str, list]]:
    """({address: object}, {role: entry points}) of what the stage overlay's roots reach in it; `placements` are the
    placement lists its rooms' variants point to."""
    tables = sorted(t for t, _w in images.directories.values())
    areas = min(b - a for a, b in zip(tables, tables[1:])) // 4
    roots, roles, counts = [], {}, {}
    for layout, (table, wrapper) in images.directories.items():
        role = IMPLICIT_ROLES[layout]
        roots.append((table, f"AreaTable{layout}", "tables", False, 0))
        counts[(table, f"AreaTable{layout}")] = areas
        roles[f"{role}_table"] = [table]
        if wrapper:
            roots.append((wrapper, WRAPPERS[layout], "tables", True, 0))
            roles[f"{role}_wrapper"] = [wrapper]
    objects = images.word(images.addresses["gAreaObjectStages"] + 8 * images.stage)
    if objects and images.in_own(objects):
        roots.append((objects, "AreaObjectRoom", "objects", False, 0))
        roles["objects"] = [objects]
    lists = sorted({p for p in placements if images.in_own(p)})
    roots += [(p, "AreaPlacement", "placements", False, 0) for p in lists]
    if lists:
        roles["placements"] = lists
    extra = []
    for role, items in entries.items():
        kind, module = STAGE_ROLES[role]
        for e in items:
            e = e if isinstance(e, dict) else {"at": e}
            extra.append((images.base + e["at"], kind, module, bool(e.get("single")), int(e.get("rows", 0))))
            roles.setdefault(role, []).append(images.base + e["at"])
    objs, _extent = _reach(images, roots, extra, lambda extent: counts)
    return objs, roles
