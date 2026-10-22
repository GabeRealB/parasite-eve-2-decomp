"""Rooms: a room's package (asset type `room`) decoded to the room, `<group>/room.toml`, and its C written from it.

A room is reached from its entry points (walk.py): its stage overlay's per-area tables, gameplay's area table, and
the record's `entries` - what only code names, by module: `{module: [offset or {at, kind, single, rows}]}`, an
offset into the package. The instance holds every object reached, by module and symbol, in address order:

    [cameras.gAkropolisSquareViewCameras]
    kind = "ViewCamera"
    values = [ ... ]                 # an array; `value = {...}` for one object, `values` of rows for a table of rows

with the C types' fields, pointers by name (`name`, `name[k]`, 0 for NULL). Geometry is the collision asset's: its
parts are named with their `count`. The instance encodes back into the package exactly (`check`).

The build writes one include per module, `assets/<group>/<module>.inc`, defining the module's objects, and
`assets/<group>/declarations.inc`, declaring them (gen_room_inc.py). A unit includes the declarations after its
headers and each module where its block is; where a module's objects are split across units or by other data, the
unit selects its part with `#define ROOM_PART <its first object>` (and its declarations with
`#define ROOM_PART declarations_<unit>`) before the include.

    python3 tools/peassets/room decode|check [--only NAME]
"""
from __future__ import annotations

import json
import re
import tomllib
from pathlib import Path
from typing import Any

from .layouts import GEOMETRY, LAYOUTS, c_value, decode_value, encode_value, named, pointer_targets, size
from . import events as event_text
from .walk import ROLE_KINDS, ROOM_LOAD, STAGE_ROLE_KINDS, STAGES, Images, walk, walk_stage


def walk_module():
    import sys
    return sys.modules[__name__ + ".walk"]

ROLE_LISTS = frozenset({"action_triggers", "geometry", "lights", "spare_batches", "spare_draw_areas", "updates",
                        "vibrations", "markers", "view_masks", "wide_view_masks", "samples", "scripts", "animations",
                        "transforms", "actor_commands", "bank_copies", "scene_keys", "move_animations",
                        "walk_animations"})          # roles the record states: always lists

ROOT = Path(__file__).resolve().parents[3]


def declared(assets: dict) -> list[str]:
    """Rooms and stage overlays: decoded the same way, each from its own package; the stage overlays after the
    rooms, whose instances their derived per-area tables read."""
    return sorted((k for k, r in assets.items() if r.get("type") in ("room", "stage")),
                  key=lambda k: (assets[k]["type"] == "stage", k))


def is_stage(name: str, assets: dict) -> bool:
    return assets[name].get("type") == "stage"


def instance_path(name: str, assets: dict, assets_root: Path) -> Path:
    return assets_root / assets[name]["group"] / ("stage.toml" if is_stage(name, assets) else "room.toml")


def stage_rooms(stage: int, assets: dict) -> list[tuple[int, str]]:
    """(area, room asset) of each room of a game stage, from its folders."""
    import asset_data
    out = []
    for folder in asset_data.TREE[stage].get("folders", {}).values():
        for node in folder["files"].values():
            for chunk in node.get("chunks", {}).values():
                if assets.get(chunk, {}).get("type") in ("room", "pe2pkg") and folder["id"] % 100 == 1:
                    out.append((folder["id"] // 100, chunk))
    return sorted(set(out))


def _place(name: str, tree: dict) -> tuple[int, int]:
    """(game stage, area) of a room package: its folder's stage, and its folder's disc id divided by 100."""
    for stage, t in tree.items():
        for folder in t.get("folders", {}).values():
            for node in folder["files"].values():
                if name in node.get("chunks", {}).values():
                    return stage, folder["id"] // 100
    raise SystemExit(f"{name} is in no room folder")


def images(name: str, assets: dict, embedded: dict, assets_root: Path) -> tuple[Images, int]:
    """The images a room or a stage overlay is decoded in, and the room's area (0 for a stage overlay). A stage
    overlay's images also know its rooms' symbols by name (their addresses coincide)."""
    import asset_data
    import shape
    ctx = shape.Context(assets, embedded, assets_root)
    if is_stage(name, assets):
        stage = next(k for k, v in STAGES.items() if v[0] == assets[name].get("package_name", name))
        im = Images(stage, shape.stored_bytes(name, ctx), None, b"", shape.stored_bytes("gameplay", ctx))
        for _area, room in stage_rooms(stage, assets):
            pkg = assets[room].get("package_name", room)
            for a, n in walk_module().symbol_map(ROOT / "configs" / "USA" / "sym" / "rooms" / f"{pkg}.txt").items():
                im.addresses.setdefault(n, a)
        return im, 0
    stage, area = _place(name, asset_data.TREE)
    overlay = STAGES[stage][0]
    overlay = next(k for k, r in assets.items() if r.get("type") in ("pe2pkg", "room", "stage")
                   and r.get("package_name", k) == overlay)   # the stage overlay's asset, by its package
    return Images(stage, shape.stored_bytes(overlay, ctx), assets[name]["package_name"],
                  shape.stored_bytes(name, ctx), shape.stored_bytes("gameplay", ctx)), area


# --- decode ----------------------------------------------------------------------------------------------------

def _name_of(im: Images, objs: dict):
    def name_of(a: int):
        if a == 0:
            return 0
        for o in objs.values():
            if o.address <= a < o.end:
                k, rest = divmod(a - o.address, size(o.kind))
                if rest:
                    raise SystemExit(f"0x{a:08X} points inside an element of {im.names[o.address]}")
                return im.names[o.address] + (f"[{k}]" if k else "")
        return im.names.get(a, a)                         # an address nothing names (stale bytes): kept as stored
    return name_of


def _stage_placements(stage: int, assets: dict, embedded: dict, assets_root: Path) -> list[int]:
    """The placement lists the stage's rooms' variants point to."""
    out = []
    for _area, room in stage_rooms(stage, assets):
        if assets[room].get("type") != "room":
            continue
        im, area = images(room, assets, embedded, assets_root)
        _states, objs, roles = walk(im, area, assets[room].get("entries", {}))
        _ROOM_ROLES[(stage, area)] = {r: im.names[p[0]] for r, p in roles.items() if r in TABLE_ROLES and p}
        for o in objs.values():
            if o.kind == "AreaVariant":
                for i in range(o.count):
                    placements = decode_value(im.room, o.address - ROOM_LOAD + 8 * i, "AreaVariant")["placements"]
                    if placements:
                        out.append(placements)
    return out


_ROOM_ROLES: dict[tuple[int, int], dict[str, str]] = {}
TABLE_ROLES = ("collision", "view_maps", "view_counts", "lighting", "cameras", "sprite_views", "warps", "surfaces")


def _table_role(kind: str) -> str:
    from .walk import IMPLICIT_ROLES
    return IMPLICIT_ROLES[kind[len("AreaTable"):]]


def _room_entry(stage: int, area: int, role: str, assets: dict, assets_root: Path):
    """A room's object in a role, from its instance's [entry] (0 where the area has no room or no such object)."""
    room = dict(stage_rooms(stage, assets)).get(area)
    if not room or assets[room].get("type") != "room":
        return 0
    path = instance_path(room, assets, assets_root)
    return tomllib.loads(path.read_text()).get("entry", {}).get(role, 0) if path.exists() else 0


def _area_names(stage: int, assets: dict) -> dict[int, dict[int, str]]:
    """{area: {address: name}} of the stage's rooms (their symbol maps and the stage map's room symbols)."""
    out = {}
    w = walk_module()
    for area, room in stage_rooms(stage, assets):
        pkg = assets[room].get("package_name", room)
        out[area] = {**w.symbol_map(ROOT / "configs" / "USA" / "sym" / "mapui" / f"{STAGES[stage][0]}.txt", pkg),
                     **w.symbol_map(ROOT / "configs" / "USA" / "sym" / "rooms" / f"{pkg}.txt")}
    return out


def decode_room(name: str, assets: dict, embedded: dict, assets_root: Path) -> dict[str, Any]:
    im, area = images(name, assets, embedded, assets_root)
    if is_stage(name, assets):
        objs, roles = walk_stage(im, _stage_placements(im.stage, assets, embedded, assets_root),
                                 assets[name].get("entries", {}))
        states = 0
        area_names = _area_names(im.stage, assets)
    else:
        states, objs, roles = walk(im, area, assets[name].get("entries", {}))
        area_names = {}
    _KINDS[name] = {im.names[a]: o.kind for a, o in objs.items() if a in im.names}
    name_of = _name_of(im, objs)
    entry = {}
    for role, points in roles.items():
        if role == "vibrations":
            entry[role] = [{"commands": im.names[c], "segments": im.names[g]} for c, g in points]
        else:
            entry[role] = [im.names[a] for a in points] if len(points) > 1 or role in ROLE_LISTS else im.names[points[0]]
    modules: dict[str, dict] = {}
    for a in sorted(objs):
        o = objs[a]
        sym = im.names.get(a)
        if sym is None:
            raise SystemExit(f"{name}: nothing names the {o.kind} at 0x{a:08X}")
        if o.kind in GEOMETRY:
            rec = {"count": o.count}
        else:
            if o.kind.startswith("AreaTable"):              # an entry an area: named through that area's room
                vals = [named(decode_value(im.own, a - im.base + 4 * i, o.kind), o.kind,
                              lambda v, i=i: area_names.get(i + 1, {}).get(v, v) if v else 0) for i in range(o.count)]
            else:
                vals = [named(decode_value(im.own, a - im.base + i * size(o.kind), o.kind), o.kind, name_of)
                        for i in range(o.count)]
            if o.kind.startswith("AreaTable") and all(
                    v == _ROOM_ROLES.get((im.stage, i + 1), {}).get(_table_role(o.kind), 0) for i, v in enumerate(vals)):
                rec = {"areas": o.count}                  # derived: each area's room's object in the table's role
            elif o.single:
                rec = {"value": vals[0]}
            elif o.rows:
                w = o.count // o.rows
                rec = {"values": [vals[r * w:(r + 1) * w] for r in range(o.rows)]}
            else:
                rec = {"values": vals}
        modules.setdefault(o.module, {})[sym] = rec
    return {"area": area, "states": states, "entry": entry, "modules": modules}


_KINDS: dict[str, dict[str, str]] = {}


def _kinds_of(name: str, assets: dict, embedded: dict, assets_root: Path) -> dict[str, str]:
    """Each object's kind as the walk found it (the last decode of the room)."""
    if name not in _KINDS:
        decode_room(name, assets, embedded, assets_root)
    return _KINDS[name]


def _toml(v: Any) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, str):
        return json.dumps(v)
    if isinstance(v, list):
        return "[" + ", ".join(_toml(e) for e in v) + "]"
    return "{ " + ", ".join(f"{k} = {_toml(e)}" for k, e in v.items()) + " }"


def to_toml(name: str, room: dict[str, Any]) -> str:
    lines = [f"# {name}: the room, decoded from its package (tools/peassets/room).", f"area = {room['area']}",
             f"states = {room['states']}", "", "[entry]"] + [f"{r} = {_toml(v)}" for r, v in room["entry"].items()]
    for module, objects in room["modules"].items():
        for sym, rec in objects.items():
            lines += ["", f"[{module}.{sym}]"]
            if "count" in rec:
                lines.append(f"count = {rec['count']}")
            elif "areas" in rec:
                lines.append(f"areas = {rec['areas']}  # each area's room's object of this role (its [entry])")
            elif "value" in rec:
                lines.append(f"value = {_toml(rec['value'])}")
            else:
                lines += ["values = ["] + [f"    {_toml(e)}," for e in rec["values"]] + ["]"]
    return "\n".join(lines) + "\n"


def read(name: str, assets: dict, assets_root: Path) -> dict[str, Any]:
    """The instance, each object's `kind` filled in: an entry point's from its role, an effect table's from its
    `type`, every other object's from the pointer field that reaches it, and a grid's cell lists the geometry no
    field names."""
    room = tomllib.loads(instance_path(name, assets, assets_root).read_text())
    modules = {k: v for k, v in room.items() if k not in ("area", "states", "entry")}
    epath = events_path(name, assets, assets_root)
    if epath.exists():
        names = event_names(name, assets, set())
        for sym, rec in event_text.read(epath.read_text(), names).items():
            modules.setdefault("updates" if rec["kind"] == "AreaApplyRec" else "events", {})[sym] = rec
    objects = {s: r for objs in modules.values() for s, r in objs.items()}
    kinds: dict[str, str] = {s: r["kind"] for s, r in objects.items() if "kind" in r}

    def assign(sym: str, kind: str) -> None:
        kind = "SpriteSource" if kind == "SpriteSources" else kind
        if kinds.get(sym, kind) != kind:
            raise SystemExit(f"{name}: {sym} is reached as {kinds[sym]} and as {kind}")
        kinds[sym] = kind

    for role, value in room.get("entry", {}).items():
        for v in value if isinstance(value, list) else [value]:
            for r, sym in (v.items() if isinstance(v, dict) else [(role, v)]):
                assign(sym, ROLE_KINDS.get(r) or STAGE_ROLE_KINDS[r])
    queue = list(kinds)
    while queue:
        sym = queue.pop()
        rec, kind = objects[sym], kinds[sym]
        if "count" in rec or "areas" in rec:              # geometry, or a derived table: nothing to follow
            continue
        values = [rec["value"]] if "value" in rec else rec["values"]
        if _is_rows(rec):
            values = [e for row in values for e in row]
        for v in values:
            for target, tkind in pointer_targets(v, kind):
                base = target.split("[")[0]
                if base in objects and base not in kinds:
                    assign(base, tkind)
                    queue.append(base)
    for sym, rec in objects.items():                     # a grid's cell lists: the geometry no field names
        if sym not in kinds and "count" in rec:
            kinds[sym] = "CollisionCells"
    missing = [s for s in objects if s not in kinds]
    if missing:
        raise SystemExit(f"{name}: nothing reaches {', '.join(missing)}")
    for sym, rec in objects.items():
        rec["kind"] = kinds[sym]
        if "areas" in rec:                                # a derived per-area table: its rooms' objects
            stage = next(k for k, v in STAGES.items() if v[0] == assets[name].get("package_name", name))
            rec["values"] = [_room_entry(stage, i + 1, _table_role(rec["kind"]), assets, assets_root)
                             for i in range(rec.pop("areas"))]
    return {"area": room["area"], "states": room["states"], "entry": room.get("entry", {}), "modules": modules}


def encode_room(name: str, room: dict[str, Any], assets: dict, embedded: dict, assets_root: Path) -> bytes:
    """The room's package image with the instance's objects written in."""
    im, _area = images(name, assets, embedded, assets_root)
    objects = {s: r for m in room["modules"].values() for s, r in m.items()}

    def address_of(ref: str) -> int:
        m = re.fullmatch(r"(\w+)(?:\[(\d+)\])?", ref)
        base = im.addresses[m.group(1)]
        return base + (int(m.group(2)) * size(objects[m.group(1)]["kind"]) if m.group(2) else 0)

    image = bytearray(im.own)
    for sym, rec in objects.items():
        if "count" in rec:
            continue                                      # the collision asset's bytes
        vals = [rec["value"]] if "value" in rec else rec["values"]
        if _is_rows(rec):
            vals = [e for row in vals for e in row]
        data = b"".join(encode_value(v, rec["kind"], address_of) for v in vals)
        at = im.addresses[sym] - im.base
        image[at:at + len(data)] = data
    return bytes(image)


def event_names(name: str, assets: dict, local: set[str]) -> "event_text.Names":
    """How the room's events text writes names: its prefix, its objects, and its own sound bank's slots by entry
    (the slots of all its sound banks, where no entry has two names)."""
    import asset_data
    stage, area = _place(name, asset_data.TREE)
    group = assets[name]["group"]
    slots: dict[int, set[str]] = {}
    for k, r in assets.items():
        if r.get("type") == "sound_bank" and k.rsplit("/", 1)[0] == group:
            for slot_name, entry in r.get("sounds", {}).items():
                slots.setdefault(entry, set()).add(slot_name)
    sounds = {e: next(iter(ns)) for e, ns in slots.items() if len(ns) == 1}
    return event_text.Names(event_text.prefix_of(assets[name]["package_name"]), local, (stage, area), sounds)


def events_path(name: str, assets: dict, assets_root: Path) -> Path:
    return assets_root / assets[name]["group"] / "events.evs"


def decode(name: str, assets: dict, embedded: dict, assets_root: Path) -> Path:
    """Write the room's instance - its events as text (events.py), the rest as room.toml; it must encode back."""
    room = decode_room(name, assets, embedded, assets_root)
    path = instance_path(name, assets, assets_root)
    path.parent.mkdir(parents=True, exist_ok=True)
    events = {**room["modules"].pop("events", {}), **room["modules"].pop("updates", {})}   # area updates: story data
    epath = events_path(name, assets, assets_root)
    if events:
        im, _area = images(name, assets, embedded, assets_root)
        kinds = _kinds_of(name, assets, embedded, assets_root)
        objs = {s: {**r, "kind": kinds[s]} for s, r in events.items()}
        names = event_names(name, assets, set(objs))
        header = f"# {name}: the room's events, decoded from its package (tools/peassets/room/events.py).\n"
        epath.write_text(header + event_text.write(objs, names, sorted(objs, key=im.addresses.get)), encoding="utf-8")
    elif epath.exists():
        epath.unlink()
    path.write_text(to_toml(name, room), encoding="utf-8")
    problems = check(name, assets, embedded, assets_root)
    if problems:
        raise SystemExit("\n".join(problems))
    return path


def check(name: str, assets: dict, embedded: dict, assets_root: Path) -> list[str]:
    im, _area = images(name, assets, embedded, assets_root)
    mine = encode_room(name, read(name, assets, assets_root), assets, embedded, assets_root)
    if mine == im.own:
        return []
    at = next(i for i in range(len(mine)) if mine[i] != im.own[i])
    return [f"{name}: the instance differs from the package at offset 0x{at:X}"]


# --- C ---------------------------------------------------------------------------------------------------------

def _units(package: str) -> list[tuple[int, int, str, str]]:
    """(start, end, unit, section) of each unit's .rodata and .data in the package, from its generated split config."""
    text = (ROOT / "configs" / "USA" / "generated" / f"{package}.yaml").read_text()
    subs = [(int(m.group(1), 16), m.group(2), m.group(3)) for m in
            re.finditer(r"^\s+- \[(0x[0-9A-Fa-f]+), ([.\w]+), ([\w/]+)\]", text, re.M)]
    ends = sorted({o for o, _t, _u in subs} | {int(m.group(1), 16) for m in re.finditer(r"^  - \[(0x[0-9A-Fa-f]+)\]", text, re.M)}
                  | {int(m.group(1), 16) for m in re.finditer(r"\{start: (0x[0-9A-Fa-f]+), type: \.bss", text)})
    return [(o, min(e for e in ends if e > o), u.rsplit("/", 1)[1], t) for o, t, u in subs if t in (".rodata", ".data")]


def _cell_macro(package: str, header: int) -> str:
    """The face-list binding a grid's cell table include calls (the manifest's collisionSource)."""
    manifest = tomllib.loads((ROOT / "configs" / "USA" / "overlays.toml").read_text())

    def find(t):
        if isinstance(t, dict):
            if isinstance(t.get(package), dict) and "objects" in t[package]:
                return t[package]
            for v in t.values():
                r = find(v)
                if r:
                    return r
    for o in find(manifest)["objects"]:
        if o.get("kind") == "collisionSource" and int(o["at"], 16) == header:
            return o.get("cell_face_list_macro", "GRID_CELL")
    raise SystemExit(f"{package}: no collisionSource at 0x{header:X}")


def c_files(name: str, assets: dict, embedded: dict, assets_root: Path) -> dict[str, str]:
    """`<module>.inc` for each module and `declarations.inc`: file name -> C."""
    room = read(name, assets, assets_root)
    im, _area = images(name, assets, embedded, assets_root)
    package = assets[name]["package_name"]
    objects = {s: (m, r) for m, objs in room["modules"].items() for s, r in objs.items()}
    at = {s: im.addresses[s] for s in objects}
    count = {s: r.get("count") or (1 if "value" in r else sum(map(len, r["values"])) if _is_rows(r) else len(r["values"]))
             for s, (m, r) in objects.items()}
    end = {s: at[s] + count[s] * size(objects[s][1]["kind"]) for s in objects}
    grids = {s: r["value"] for s, (m, r) in objects.items() if r["kind"] == "WorldCollisionGrid"}
    part_grid = {v[f]: g for g, v in grids.items() for f in ("normals", "vertices", "faces", "cellFaceIds")}

    def pointer(ref: str) -> str:
        m = re.fullmatch(r"(\w+)(?:\[(\d+)\])?", ref)
        if m.group(2):
            return f"&{m.group(1)}[{m.group(2)}]"
        if m.group(1) in objects:
            return f"&{m.group(1)}" if "value" in objects[m.group(1)][1] else m.group(1)
        return f"(void*)&{m.group(1)}"

    def declarator(s: str) -> str:
        r = objects[s][1]
        storage = ("static " if s.startswith("_") else "") + ("const " if s in rodata else "")
        if "value" in r:
            dims = ""
        elif _is_rows(r):
            dims = f"[{len(r['values'])}][{len(r['values'][0])}]"
        else:
            dims = f"[{count[s]}]"
        return f"{storage}{LAYOUTS[r['kind']][0]} {s}{dims}"

    def definition(s: str) -> list[str]:
        r = objects[s][1]
        if r["kind"] in GEOMETRY:
            part = GEOMETRY[r["kind"]]
            grid = part_grid.get(s) or next(g for g, v in grids.items() if end[v["faces"]] == at[s])   # cells follow faces
            inc = f'#include "assets/{package}_collision_{im.addresses[grid] - ROOM_LOAD:05X}_{part}.inc"'
            if part != "table":
                return [f"{declarator(s)} = {{", inc, "};"]
            cells = next(c for c, (m, v) in objects.items() if v["kind"] == "CollisionCells" and end[c] == at[s])
            macro = _cell_macro(package, im.addresses[grid] - ROOM_LOAD)
            return [f"#define {macro}(halfwordOffset) (&{cells}[(halfwordOffset)])", f"{declarator(s)} = {{", inc, "};",
                    f"#undef {macro}"]
        if "value" in r:
            return [f"{declarator(s)} = {c_value(r['value'], r['kind'], pointer)};"]
        if _is_rows(r):
            rows = ["{ " + ", ".join(c_value(v, r["kind"], pointer) for v in row) + " }" for row in r["values"]]
            return [f"{declarator(s)} = {{"] + [f"    {e}," for e in rows] + ["};"]
        return [f"{declarator(s)} = {{"] + [f"    {c_value(v, r['kind'], pointer)}," for v in r["values"]] + ["};"]

    others = sorted(a for a in im.names if im.in_room(a) and im.names[a] not in objects
                    and not any(at[s] < a < end[s] for s in objects))
    header = [f"/* {name}: written from its room instance by tools/buildsteps/gen_room_inc.py. */",
              "#define ROOM_CAT_(a, b) a##b", "#define ROOM_CAT(a, b) ROOM_CAT_(a, b)"]
    files: dict[str, list] = {}
    units = _units(package)
    rodata = {s for s in objects for start, stop, _u, sec in units if sec == ".rodata" and start <= at[s] - im.base < stop}
    parts: dict[str, list[list[str]]] = {}
    decl: dict[str, list[str]] = {}
    for start, stop, unit, _section in units:
        mine = sorted((s for s in objects if start <= at[s] - im.base < stop), key=at.get)
        if mine:
            decl.setdefault(unit, []).extend(mine)
        for s in mine:
            module = objects[s][0]
            runs = parts.setdefault(module, [])
            last = runs[-1][-1] if runs else None
            if (last and start <= at[last] - im.base < stop and objects[last][0] == module
                    and not any(at[last] < a < at[s] for a in others)
                    and not any(at[last] < at[o] < at[s] and objects[o][0] != module for o in objects)):
                runs[-1].append(s)
            else:
                runs.append([s])
    for module, runs in parts.items():
        out = list(header)
        for run in runs:
            body = [line for s in run for line in definition(s)]
            if len(runs) == 1:
                out += [""] + body
            else:
                out += ["", f"#define ROOM_IS_{run[0]} 1", f"#if ROOM_CAT(ROOM_IS_, ROOM_PART) == 1"] + body + \
                       ["#endif", f"#undef ROOM_IS_{run[0]}"]
        files[f"{module}.inc"] = out + ([] if len(runs) == 1 else ["#undef ROOM_PART"])
    out = list(header)
    for unit, mine in decl.items():
        lines = [f"{'extern ' if not s.startswith('_') else ''}{declarator(s)};" for s in mine]
        if len(decl) == 1:
            out += [""] + lines
        else:
            out += ["", f"#define ROOM_IS_declarations_{unit} 1", "#if ROOM_CAT(ROOM_IS_, ROOM_PART) == 1"] + lines + \
                   ["#endif", f"#undef ROOM_IS_declarations_{unit}"]
    files["declarations.inc"] = out + ([] if len(decl) == 1 else ["#undef ROOM_PART"])
    return {f: "\n".join(lines) + "\n" for f, lines in files.items()}


def module_parts(name: str, assets: dict, embedded: dict, assets_root: Path) -> dict[str, list[str]]:
    """Each module's parts by their first object (one part: included without a selector)."""
    out: dict[str, list[str]] = {}
    for f, text in c_files(name, assets, embedded, assets_root).items():
        firsts = re.findall(r"#define ROOM_IS_(\w+) 1", text)
        out[f[:-4]] = firsts
    return out


def _is_rows(r: dict) -> bool:
    """Whether an object's values are rows: no layout's element is itself a list."""
    return bool(r.get("values")) and isinstance(r["values"][0], list)


from . import sync_c  # noqa: E402  (bringing a room's C in line: sync_c.py)
