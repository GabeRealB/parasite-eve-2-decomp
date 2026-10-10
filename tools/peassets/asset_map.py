#!/usr/bin/env python3
"""The asset map: one local page to see and validate the assets and what they depend on.

    venv/bin/python3 tools/peassets/asset_residency.py build     the load graph (local/asset_residency.sqlite)
    venv/bin/python3 tools/peassets/asset_map.py                 local/asset_map.html

The page has the overlay tree on the left - main, then title and gameplay,
then what gameplay loads, the stage maps and their rooms - and every asset by
type. An overlay shows its textures as its own models use them, read from its
own files through the palette the faces name; what it takes from an ancestor;
what nothing in its chain supplies; what it loads and who loads it; and, for a
room, video memory under a chosen view and enemy set. An asset shows itself
with a renderer for its type, the ways it is used, the files it sits in and
the loads that bring those in.

The page is written under local/ and refers to the local extraction for
pictures and sound; the raw words of images and palettes are written beside
it as `<page>_pix/<asset>.js`, because a page opened from disk can run a
script but cannot read the pixels of a picture. Nothing here is committed
game data. Run it from the main tree: the journal and the extraction are
looked for under this tree's local/ and assets/.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_residency as residency  # noqa: E402
import asset_schema  # noqa: E402

TEMPLATE = Path(__file__).with_name("asset_map.html")
SHOWN = ("x", "y", "w", "h", "bpp", "clips", "parent", "required", "source", "vram", "size", "notes")


def spawn_bindings(assets: dict, unresolved: list | None = None) -> dict[tuple[str, str], set[int]]:
    """(package, model) -> the stage-zero files a room loads for the entry that spawns that model.

    This is read from the game's own tables, in the packages' bytes, by the
    route gameplay takes (room_scene.py): `Gp_AreaTables[stage][area]` leads to
    a room's `AreaVariant` table, and each variant to its `AreaPlacement` and
    `AreaResource` lists. A resource row names a file (`fileNumber`,
    `fileGroupIndex`) and the actor it is for: `taskTable[taskIndex]`, a
    `TaskDesc` whose `data.model` is the model's `TmdSource` when its body kind
    is a TMD model (include/gameplay/area.h, include/main/task_types.h). So the
    row says which file's textures the model spawned through it is drawn with.

    The task table is an address in an actor overlay, and the row does not say
    which: it is the package the row's own file loads when the address lies in
    it, else the one package among those the table's other files load whose
    image holds the address (each at its slot's load address, from the overlay
    manifest), else the room itself. Where several loaded packages hold it, the
    symbol maps that name the address for one of them decide; a row that still
    has no single package is left out and listed in `unresolved`.
    """
    import hashlib
    import re
    import struct
    import pkg_model
    import room_scene
    from room_scene import PLACEMENT, RESOURCE, TASK_DESC, VARIANT, sizeof
    sys.path.insert(0, str(ROOT / "tools"))
    from gen_overlay_configs import slot_packages
    project = room_scene.Project()
    # Where each package is loaded: an actor at its slot's address, not at its family's (configs/USA/overlays.toml).
    loads: dict[str, int] = {}
    for spec in project.manifest.values():
        if isinstance(spec, dict) and "overlays" in spec:
            slots = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
            for name, slot, _entry in slot_packages(spec):
                loads[name] = slots.get(slot.get("slot"), int(spec["load_addr"]))
    by_sha = {rec["sha1"]: key for key, rec in assets.items() if rec.get("type") == "model"}
    last = lambda r: r["entryId"] == 0xFF  # noqa: E731 - AREA_RESOURCE_END / AREA_PLACEMENT_END (include/gameplay/area.h)
    missed = unresolved if unresolved is not None else []

    def holds(package: str | None, address: int) -> bool:
        data = project.package(package) if package in loads else None
        return data is not None and 0 <= address - loads[package] <= len(data) - sizeof(TASK_DESC)

    out: dict[tuple[str, str], set[int]] = defaultdict(set)
    for room in sorted(project.place_of):
        scene = room_scene.Scene(project, room)
        record, _ = scene.directory("areas")
        if not record or not record["variants"]:
            continue
        variants, _ = scene.table("placements", record["variants"], VARIANT)
        # Which further file of an entry its placements ask for (`AreaPlacement.fileIdLow`): a placement with 0 uses
        # the entry's base file, one with N the file N beside it. So the entry's models are drawn with those files.
        lows: dict[tuple[int, int], set[int]] = defaultdict(set)
        tables: dict[int, list[dict]] = {}
        for variant in variants or []:
            if not variant["resources"]:
                continue
            if variant["resources"] not in tables:
                rows, _ = scene.table("placements", variant["resources"], RESOURCE, stop=last)
                tables[variant["resources"]] = [r for r in rows or [] if not last(r)]
            rows, _ = scene.table("placements", variant["placements"], PLACEMENT, stop=last) if variant["placements"] else (None, None)
            for placed in rows or []:
                if not last(placed):
                    lows[variant["resources"], placed["entryId"]].add(placed["fileIdLow"])
        for table, rows in tables.items():
            files = [scene.resolve_file(r) for r in rows]
            loaded = list(dict.fromkeys(f["package"] for f in files if f["package"]))
            for r, file in zip(rows, files):
                if file["fileId"] is None or not r["taskTable"]:
                    continue
                address = r["taskTable"] + r["taskIndex"] * sizeof(TASK_DESC)
                where = f"{room} resource table {scene.ref(table)} entry {r['entryId']}"
                if holds(file["package"], address):
                    owners = [file["package"]]
                else:
                    owners = [package for package in loaded if holds(package, address)]
                    if len(owners) > 1:
                        named = scene.foreign(r["taskTable"], loaded)[1]
                        owners = [named] if named in owners else owners
                    if not owners and scene.owner_of(address) == room and holds(room, address):
                        owners = [room]
                if len(owners) != 1:
                    if not scene.owner_of(address):
                        missed.append(f"{where}: task table 0x{r['taskTable']:08X} is in "
                                      + (f"several loaded packages ({', '.join(owners)})" if owners else "no package the table's files load"))
                    continue
                package = owners[0]
                load, data = loads[package], project.package(package)
                flags, _priority, _callback, model = struct.unpack_from("<HHII", data, address - load)
                if flags & 0xFF != 1 or not load <= model < load + len(data):
                    continue                      # not a TMD body: the descriptor names no model
                try:
                    src = pkg_model.read_source(data, load, model - load)
                    key = by_sha.get(hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest())
                except Exception:  # noqa: BLE001
                    key = None
                if key:
                    out[package, key].update(file["fileId"] + low for low in (lows.get((table, r["entryId"])) or {0}))
    # What the tables cannot say - a model some task spawns itself - is read out of the code and kept beside this
    # tool: package, model, file ids, evidence, note.
    stated = Path(__file__).with_name("model_textures.tsv")
    if stated.is_file():
        for line in stated.read_text(encoding="utf-8").splitlines():
            cells = [c.strip() for c in line.split("\t")]
            if line.strip() and not line.startswith("#") and len(cells) >= 3:
                out[cells[0], cells[1]] = {int(i) for i in re.findall(r"\d+", cells[2])}
    return out


class Supply:
    """Who can supply a texture page and palette: an overlay's own files, then its ancestors'."""

    def __init__(self, db: sqlite3.Connection, assets: dict, tree: dict) -> None:
        self.assets, self.tree = assets, tree
        self.files = {r["path"]: r for r in db.execute("SELECT * FROM file")}
        self.prints = defaultdict(list)
        for r in db.execute("SELECT * FROM footprint"):
            self.prints[r["file"]].append(tuple(r)[1:])
        self.offsets = defaultdict(set)
        for m in db.execute("SELECT m.file, m.page, m.row, m.mode FROM member m JOIN grp g ON g.id = m.grp WHERE g.kind != 'carrier'"):
            self.offsets[m["file"]].add((m["page"], m["row"], m["mode"] or ""))
        self.carrier = defaultdict(list)
        for r in self.files.values():
            if r["package"]:
                self.carrier[r["package"]].append(r["path"])
        self.zero = {r["id"]: r["path"] for r in self.files.values() if r["stage"] == 0 and r["id"] is not None}
        self._own: dict[str, list[str]] = {}

    def is_room(self, package: str) -> bool:
        return bool(self.carrier.get(package)) and bool(self.files[self.carrier[package][0]]["stage"])

    def own(self, package: str) -> list[str]:
        if package not in self._own:
            if package == asset_schema.ROOT_OVERLAY:
                out = {self.zero[1]} if 1 in self.zero else set()        # what main loads at boot
            else:
                out = set(self.carrier[package])
                for path in self.carrier[package]:
                    f = self.files[path]
                    if f["stage"]:
                        out |= {p for p, g in self.files.items() if g["stage"] == f["stage"] and g["folder"] == f["folder"]}
                    else:
                        # The entry's further files: those beside it that carry no overlay of their own. A file
                        # that does is a sibling overlay (another weapon, another companion, another stage's map).
                        out |= {p for i, p in self.zero.items() if i // 100 == f["id"] // 100 and not self.files[p]["package"]}
                        out |= {p for i, p in self.zero.items() if 60 <= i // 10000 < 70 and (i % 100000) // 100 == (f["id"] % 100000) // 100}
            self._own[package] = sorted(out)
        return self._own[package]

    def rects(self, path: str, at: tuple | None):
        return [(r[1], *(residency.moved(r, *at) if at else (r[3], r[4])), r[5], r[6], r[0], r[2]) for r in self.prints.get(path, ())]

    @staticmethod
    def geometry(page: int, clut: int):
        mode = (page >> 7) & 3
        return (4, 8, 16)[mode] if mode < 3 else 16, (page & 15) * 64, 256 if page & 16 else 0, (clut & 63) * 16, clut >> 6

    def covers(self, found, page: int, clut: int):
        """(image rect, CLUT rect) when these rectangles hold the page and the palette, else None."""
        bpp, tx, ty, cx, cy = self.geometry(page, clut)
        width = 64 if bpp == 4 else 128
        image = next((r for r in found if r[0] == "image" and r[1] < tx + width and tx < r[1] + r[3] and r[2] < ty + 256 and ty < r[2] + r[4]), None)
        palette = next((r for r in found if r[0] == "clut" and r[1] <= cx < r[1] + r[3] and r[2] <= cy < r[2] + r[4]), None)
        return (image, palette) if image and (bpp == 16 or palette) else None

    def resolve(self, package: str, page: int, clut: int, bound: set[int] | None = None):
        """("own", [(file, image, palette)], settled) | ("ancestor", name, [(file, page, row, mode)]) | None.

        Several of an overlay's own files can fit: its actors' textures are
        stored at the same place in each. Which one a model is drawn with is
        not guessed: it is the file of the entry that spawns the model
        (`bound`, from the rooms' resource tables). Without that, every file
        that fits is returned and the question is left open.
        """
        mine = [(path, *hit) for path in self.own(package) if (hit := self.covers(self.rects(path, None), page, clut))]
        # Files holding the very same image and palette are no alternatives at all.
        same = lambda found: len({(f[1][5], f[2][5] if f[2] else "") for f in found}) == 1  # noqa: E731
        settled = len(mine) <= 1 or same(mine)
        if bound and len(mine) > 1:
            # The entry's base file, or the further file its placements ask for.
            chosen = [found for found in mine if self.files[found[0]]["id"] in bound]
            if chosen:
                mine, settled = chosen + [found for found in mine if found not in chosen], len(chosen) == 1 or same(chosen)
        if mine and not self.is_room(package):
            return ("own", mine, settled)
        for holder in ([package] if self.is_room(package) else []) + asset_schema.ancestors_of(package, self.assets, self.tree):
            via = []
            for path in self.own(holder):
                for at in sorted(self.offsets.get(path) or {(0, 0, "")}):
                    if self.covers(self.rects(path, at), page, clut):
                        via.append((path, *at))
            # A room's views replace each other: the page and the palette may come from two of its files.
            if not via and self.is_room(holder):
                both = [r for path in self.own(holder) for r in self.rects(path, (0, 0, ""))]
                if self.covers(both, page, clut):
                    via = [("", 0, 0, "")]
            if via:
                return ("ancestor" if holder != package else "state", holder, via)
        return None


RAW_DIR = {"motion_curve": ("motion", ".motion"), "sprite_schedule": ("motion", ".motion"), "camera_path": ("path", ".path"),
           "movie_path": ("path", ".path"), "morph_deltas": ("morph", ".morph"), "morph_normals": ("morph", ".morph"),
           "fontglyph": ("fontglyph", ".fontglyph")}
UNIT = {"u8": 1, "u16": 2, "u32": 4}


def textured_faces(data: bytes, stream_off: int, vertex_count: int):
    """(faces, texture of each face or None, the [page, CLUT] pairs named, the part of each face).

    A face's texture is the index of its pair and one (u, v) per corner, in
    the face's own corner order. `u0` and `u1` carry a corner each in their
    low half, the third word one in its low half and, for a quad, a fourth
    in its high; a quad's corners are stored in Z order and drawn 0 1 3 2.
    """
    import struct
    import pkg_model
    import tmd_export
    walked = pkg_model.walk_stream(data, stream_off)
    faces, textures, pairs, parts = [], [], [], []
    for packet in (walked[0] if walked else ()):
        op, count, stride = packet["op"], packet["count"], packet["stride"]
        if not count or not stride or op in tmd_export.TRANSFORM_OPS:
            continue
        word = pkg_model.TEXTURE_WORD.get(op)
        body = packet["offset"] + pkg_model.HEADER_WORDS * 4
        for e in range(count):
            at = body + e * stride * 4
            if at + stride * 4 > len(data):
                break
            words = struct.unpack_from(f"<{stride}I", data, at)
            got = tmd_export.element_geometry(op, words, vertex_count)
            if not got:
                continue
            faces.append(list(got[0]))
            parts.append(packet["part"])
            if word is None or stride < word + 3:
                textures.append(None)
                continue
            u0, u1, u2 = words[word:word + 3]
            corners = [u0 & 0xFFFF, u1 & 0xFFFF, u2 & 0xFFFF]
            if len(got[0]) == 4:
                corners = [corners[0], corners[1], u2 >> 16, corners[2]]
            pair = [(u1 >> 16) & ~0x60, u0 >> 16]
            if pair not in pairs:
                pairs.append(pair)
            textures.append([pairs.index(pair)] + [c for corner in corners for c in (corner & 0xFF, corner >> 8)])
    return faces, textures, pairs, parts


def write_side_data(folder: Path, assets: dict, embedded: dict) -> dict:
    """Meshes and sampled tracks, one script each beside the page, like the pixels; returns what was found.

    A mesh is its vertices in the rest pose and its faces. A track is the raw
    numbers of an embedded curve or path, in the unit the build includes it
    with: enough to plot, not a decoding of what each number means.
    """
    import base64
    import hashlib
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import pkg_model
    import tmd_export
    folder.mkdir(parents=True, exist_ok=True)
    by_sha = {rec["sha1"]: key for key, rec in assets.items() if rec.get("type") == "model"}
    have = {"mesh": [], "track": {}, "holds": defaultdict(list)}
    for package, (load, sources) in sorted(pkg_model.declared_records(ROOT).items()):
        path = ROOT / "assets" / "USA" / "pe2pkg" / f"{package}.pe2pkg"
        if not path.is_file():
            continue
        data = path.read_bytes()
        for offset in sorted(sources):
            try:
                src = pkg_model.read_source(data, load, offset)
                key = by_sha.get(hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest())
                if key is not None and key not in have["holds"][package]:
                    have["holds"][package].append(key)       # a mesh shared with another package is this one's too
                if key is None or key in have["mesh"]:
                    continue
                verts = tmd_export.pose_vertices(tmd_export.read_vertices(data, src["verts_offset"], src["vertex_count"]), src.get("skeleton"))
                faces, textures, named, parts = textured_faces(data, src["stream_offset"], src["vertex_count"])
                # The rest skeleton: where each bone sits, and its parent.
                bones = []
                skeleton = src.get("skeleton")
                if skeleton:
                    import pkg_overlay
                    world = pkg_overlay.compose_skeleton(skeleton)
                    bones = [[round(tr[0]), round(tr[1]), round(tr[2]), skeleton["bones"][i]["parent"]] for i, (_rot, tr) in enumerate(world)]
            except Exception as exc:  # noqa: BLE001 - one model that does not decode must not stop the page
                print(f"{package}+0x{offset:X}: no mesh ({exc})", file=sys.stderr)
                continue
            if not faces:
                continue
            flat = [round(c) for v in verts for c in v]
            target = folder / f"{key}.js"
            packed = lambda value: json.dumps(value, separators=(",", ":"))  # noqa: E731
            target.write_text(f"MESH({json.dumps(key)},{packed(flat)},{packed(faces)},{packed(textures)},{packed(named)},{packed(parts)},{packed(bones)});\n", encoding="utf-8")
            have["mesh"].append(key)
    raw = ROOT / "assets" / "USA" / "raw"
    for key, rec in embedded.items():
        where = RAW_DIR.get(rec.get("type"))
        source = raw / where[0] / f"{key}{where[1]}" if where else None
        if source is None or not source.is_file():
            continue
        unit = UNIT.get(rec.get("include") if isinstance(rec.get("include"), str) else "", 2)
        (folder / f"{key}.js").write_text(f"TRACK({json.dumps(key)},{unit},{json.dumps(base64.b64encode(source.read_bytes()).decode())});\n", encoding="utf-8")
        have["track"][key] = unit
    return have


def slim_scene(scene: dict) -> dict:
    """A room's scene (room_scene.py) cut down to what the page draws: geometry, cameras, regions and placements."""
    flat = lambda points: [c for p in points for c in p]
    index = lambda tables, symbol: next((i for i, t in enumerate(tables) if t.get("table") == symbol), -1)
    grids = [g for g in scene.get("grids", []) if not g.get("patch")]
    def label(t):
        p, a = t.get("parameters") or {}, t.get("action", "")
        if a == "WARP":
            return f"to area {p.get('destinationArea')}"
        if a in ("CAP", "CAP_WEAPON"):
            return f"command {p.get('capCommand')}" if "capCommand" in p else "room message"
        if a == "CALLBACK":
            return str(p.get("callback", ""))
        if a == "ROOM":
            return f"room action {p.get('roomActionId')}"
        if a == "FACING":
            return f"stairs, class or flight {p.get('classOrFlight')}"
        return ""
    def camera(c):
        return None if not c else [flat(c["matrix"]), c["position"], c["screenDistance"], 1 if c.get("planView") else 0]
    warps = scene.get("warps") or {}
    return {
        "rooms": [{"i": r["roomIndex"], "grid": next((i for i, g in enumerate(grids) if g["id"] == r.get("grid")), -1),
                   "bnd": index(scene.get("boundaries", []), r.get("viewBoundaryTriggers")), "trg": index(scene.get("triggers", []), r.get("actionTriggers"))}
                  for r in scene.get("rooms", [])],
        "grids": [{"id": g["id"], "v": flat(g["vertices"]), "f": [f["v"] + [f["surfaceClass"], 1 if f["floor"] else 0] for f in g["faces"] if not f.get("skipped")]} for g in grids],
        "bnd": [[[flat(b["corners"]), b.get("fromView"), b.get("toView")] for b in t["items"]] for t in scene.get("boundaries", [])],
        "trg": [[[flat(b["corners"]), b.get("action", ""), label(b), 1 if b.get("automatic") else 0] for b in t["items"]] for t in scene.get("triggers", [])],
        "entries": [[e["slot"], *e["player"]["position"], e["player"]["yaw"] if e["player"].get("yawSpecial") is None else None, e.get("initialView")] for e in warps.get("entries", [])],
        "exits": [[flat(e["corners"]), e.get("destinationRoom") or f"area {e.get('destinationArea')}", e.get("departureSlot"), e.get("arrivalSlot"), e.get("roomIndices", [])] for e in warps.get("exits", [])],
        "views": [{"i": v["roomIndex"], "views": [[w["view"], w["file"], (w["background"]["bs"] or [None])[0] if w.get("fileExists") else None, camera(w.get("camera"))] for w in v["views"]]} for v in scene.get("views", [])],
        "vars": [{"n": v["variant"], "set": 1 if v.get("default") or v.get("hasSetter") else 0, "empty": 1 if v.get("empty") else 0,
                  "by": [f"{x.get('kind')} in {x.get('by')} ({x.get('file', '')}:{x.get('line', '')})" for x in v.get("setters", [])],
                  "placed": [[*p["position"], p["yaw"], 1 if p.get("atOrigin") else 0, p.get("package") or "", next((m for m in p.get("models", []) if m.endswith("_body")), (p.get("models") or [""])[0]), p.get("mode"), p.get("entryId")]
                             for p in v.get("placed", [])]}
                 for v in (scene.get("placements") or {}).get("variants", [])],
        "items": [[*o["position"], o.get("yaw", 0), ((o.get("spawn") or {}).get("modelKey")) or "", o.get("kind")] for o in (scene.get("objects") or {}).get("items", []) if not o.get("atOrigin")],
        "open": [f"{u.get('block')}: {u.get('what')} - {u.get('reason')}" for u in scene.get("unresolved", [])] + list(scene.get("notes", []))[:12],
    }


def write_scenes(folder: Path, rooms: list[str]) -> list[str]:
    """One script per room with its scene, loaded when the room's page is opened."""
    import room_scene
    project, done = room_scene.Project(), []
    folder.mkdir(parents=True, exist_ok=True)
    for room in rooms:
        if room not in project.place_of:
            continue
        try:
            scene = slim_scene(room_scene.Scene(project, room).build())
        except Exception as error:  # noqa: BLE001 - a room whose scene cannot be read has no viewer; the page says so
            print(f"room scene {room}: {type(error).__name__}: {error}", file=sys.stderr)
            continue
        (folder / f"scene_{room}.js").write_text(f"SCENE({json.dumps(room)},{json.dumps(scene, separators=(',', ':'))});\n", encoding="utf-8")
        done.append(room)
    return done


def animation_tables(embedded: dict) -> dict:
    """Per animation bank: each set's record and track counts, and the tables of the package that index it."""
    import tomllib
    import asset_journal
    import pkg_anim
    sys.path.insert(0, str(ROOT / "tools"))
    from gen_overlay_configs import declared_animations
    banks = {(rec["source"], rec["vram"]): key for key, rec in embedded.items() if rec.get("type") == "animation"}
    tables = asset_journal.set_tables(embedded)
    out = {}
    blobs: dict[str, bytes] = {}
    for row in declared_animations(tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))):
        key = banks.get((row["package"] + ".pe2pkg", row["load"] + row["offset"]))
        path = ROOT / "assets" / "USA" / "pe2pkg" / f"{row['package']}.pe2pkg"
        if key is None or not path.is_file():
            continue
        data = blobs.setdefault(row["package"], path.read_bytes())
        sets = []
        for offset in row["sets"]:
            try:
                one = pkg_anim.read_set(data, row["load"], offset)
                sets.append([one["record_count"], one["track_count"]])
            except ValueError:
                sets.append([0, 0])
        played = [[name, [e[1] if e and e[0] == key else (-2 if e else -1) for e in entries]] for name, entries in tables.get(row["package"], ())
                  if any(e and e[0] == key for e in entries)]
        out[key] = {"sets": sets, "tables": played}
    return out


def build_data(db: sqlite3.Connection, out: Path, pixels: str) -> dict:
    assets, embedded = asset_schema.load()
    tree = asset_schema.load_tree()
    supply = Supply(db, assets, tree)
    bindings = spawn_bindings(assets)
    files = [r["path"] for r in db.execute("SELECT path FROM file ORDER BY path")]
    index = {path: n for n, path in enumerate(files)}
    packages = {k for k, r in assets.items() if r["type"] == "pe2pkg"}

    chunks = {}
    for stage, folder, fkey, node in asset_schema.tree_files(tree):
        path = f"{stage}/{fkey}" if folder is None else f"{stage}/{folder}/{fkey}"
        if path in index:
            chunks[index[path]] = [[name, key] for name, key in (node.get("chunks") or {}).items()]
    footprint = defaultdict(list)
    for r in db.execute("SELECT * FROM footprint"):
        footprint[index[r["file"]]].append([r["asset"], r["kind"][0], r["part"], r["x"], r["y"], r["w"], r["h"], r["bpp"] or 0])

    members, spawns = defaultdict(list), defaultdict(list)
    for r in db.execute("SELECT * FROM member"):
        members[r["grp"]].append([index[r["file"]], r["page"], r["row"], int("RELOCATE" in (r["mode"] or "")), residency.brief(r["note"])])
    for r in db.execute("SELECT * FROM spawn"):
        spawns[r["grp"]].append([r["package"], r["page"], r["row"]])
    loads = defaultdict(list)
    for g in db.execute("SELECT * FROM grp WHERE kind != 'carrier' ORDER BY loader, id"):
        layer = residency.LAYERS.index(g["layer"]) if g["layer"] in residency.LAYERS else 0
        loads[g["loader"]].append([layer, g["condition"], g["kind"][0], g["evidence"], members[g["id"]], spawns[g["id"]]])

    # Texture requests, each with who supplies it, and the pairings that follow for images.
    requests = defaultdict(list)
    pairs: dict[str, dict[tuple, set]] = defaultdict(lambda: defaultdict(set))
    findings = defaultdict(list)
    for key, rec in sorted(embedded.items()):
        package = rec.get("source", "").removesuffix(".pe2pkg")
        if rec.get("type") != "model" or package not in supply.carrier or not assets.get(key, {}).get("textures"):
            continue
        bounds = residency.model_bounds(package, rec)
        for page, clut in assets[key]["textures"]:
            box = bounds.get((page, clut), [])
            page &= ~0x60
            found = supply.resolve(package, page, clut, bindings.get((package, key)))
            bpp, tx, ty, cx, cy = supply.geometry(page, clut)
            if found is None:
                how = ["none"]
                findings[package].append(f"{key}: page ({tx}, {ty}) {bpp}-bit with palette ({cx}, {cy}) is supplied by nothing in its chain")
            elif found[0] == "own":
                how = ["own", [index[path] for path, _i, _p in found[1]], int(found[2])]
                if not found[2]:
                    findings[package].append(f"{key}: {len(found[1])} of the overlay's files fit its page ({tx}, {ty}) and palette ({cx}, {cy}), "
                                             "and no resource entry spawning this model says which it is drawn with")
                for _path, image, palette in found[1][: 1 if found[2] else 0]:      # only an established file says how the image is read
                    column = max(0, (tx - image[1]) // 64)
                    if palette:
                        pairs[image[5]][bpp, palette[5], cy - palette[2], (cx - palette[1]) // 16, column].add(key)
                    else:
                        pairs[image[5]][bpp, "", 0, 0, column].add(key)
            else:
                how = [found[0], found[1], [[index[path], p, r, int("RELOCATE" in m)] if path else None for path, p, r, m in found[2][:6]]]
            requests[package].append([key, page, clut, box, how])

    constants = defaultdict(list)
    journal = ROOT / "local" / "asset_journal.sqlite"
    if journal.is_file():
        scopes = defaultdict(lambda: {"image.tpage": [], "clut.value": []})
        for package, file, scope, symbol, attribute, value in sqlite3.connect(journal).execute(
                "SELECT package, file, scope, symbol, attribute, value FROM evidence WHERE present = 1 AND status != 'rejected' "
                "AND attribute IN ('image.tpage', 'clut.value') AND package != '' AND scope != '' AND kind != 'faces'"):
            scopes[package, file, scope][attribute].append((symbol, value))
        # At file scope, a page and a palette belong together when they are named alike.
        import re
        alike = re.compile(r"_(CLUT|PALETTE|TPAGE|TEXTURE_PAGE|TEX_PAGE)(_ID|_VALUE)?$")
        named = defaultdict(lambda: {"image.tpage": [], "clut.value": []})
        for package, file, symbol, attribute, value in sqlite3.connect(journal).execute(
                "SELECT package, file, symbol, attribute, value FROM evidence WHERE present = 1 AND status != 'rejected' "
                "AND attribute IN ('image.tpage', 'clut.value') AND package != '' AND scope = '' AND kind != 'faces'"):
            named[package, file, alike.sub("", symbol)][attribute].append((symbol, value))
        for (package, file, stem), found in named.items():
            if found["image.tpage"] and found["clut.value"]:
                scopes[package, file, stem] = found
        for (package, _file, scope), found in sorted(scopes.items()):
            if 1 <= len(found["image.tpage"]) <= 2 and 1 <= len(found["clut.value"]) <= 3:
                for page_name, page in found["image.tpage"]:
                    for clut_name, clut in found["clut.value"]:
                        page &= ~0x60
                        label = f"{scope}: {page_name} + {clut_name}"
                        # A constant states where something is in memory, so it is held against files as loaded.
                        via = None
                        for holder in [package] + asset_schema.ancestors_of(package, assets, tree):
                            for path in supply.own(holder) if holder in supply.carrier or holder == asset_schema.ROOT_OVERLAY else ():
                                for at in sorted(supply.offsets.get(path) or {(0, 0, "")}):
                                    hit = supply.covers(supply.rects(path, at), page, clut)
                                    if hit and via is None:
                                        via = (holder, path, at, hit)
                        bpp, tx, ty, cx, cy = supply.geometry(page, clut)
                        if via is None:
                            if package in supply.carrier and not supply.is_room(package):
                                findings[package].append(f"{label}: page ({tx}, {ty}) {bpp}-bit with palette ({cx}, {cy}) is supplied by nothing in its chain")
                            how = ["none"]
                        else:
                            holder, path, at, (image, palette) = via
                            how = ["own" if holder == package else "ancestor", holder, [index[path], at[0], at[1], int("RELOCATE" in at[2])]]
                            column = max(0, (tx - image[1]) // 64)
                            pairs[image[5]][bpp, palette[5] if palette else "", (cy - palette[2]) if palette else 0,
                                            ((cx - palette[1]) // 16) if palette else 0, column].add(f"{page_name} + {clut_name}")
                        constants[package].append([label, page, clut, how])
    else:
        print(f"no journal at {journal}: constants are left out", file=sys.stderr)

    # Region violations, per loader.
    table = residency.region_table()
    kinds = {r["path"]: residency.family_of(r["path"], r["stage"], r["id"]) for r in db.execute("SELECT * FROM file")}
    seen = set()
    for m in db.execute("SELECT DISTINCT m.file, m.page, m.row, m.mode, g.loader, g.condition FROM member m JOIN grp g ON g.id = m.grp WHERE g.kind != 'carrier'"):
        for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
            x, y = residency.moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
            if (m["file"], f["asset"], f["part"], x, y) in seen:
                continue
            seen.add((m["file"], f["asset"], f["part"], x, y))
            region = residency.region_of(table, f["kind"], x, y)
            if region is None or kinds[m["file"]] not in region["families"]:
                findings[m["loader"]].append(f"{f['asset']} of {m['file']} lands at ({x}, {y}), in {region['name'] if region else 'no region'}, "
                                             f"which is not for {kinds[m['file']]} ({m['condition'] or 'always'})")

    import tomllib
    sys.path.insert(0, str(ROOT / "tools"))
    from gen_overlay_configs import slot_packages
    address = {}
    for _family, spec in tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8")).items():
        if isinstance(spec, dict) and "overlays" in spec:
            own = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
            for name, slot, _entry in slot_packages(spec):
                address[name] = own.get(slot.get("slot"), int(spec["load_addr"]))

    overlays = {asset_schema.ROOT_OVERLAY: {"family": "the executable", "parents": [], "own": [index[p] for p in supply.own(asset_schema.ROOT_OVERLAY)], "room": 0}}
    for package in sorted(packages):
        if package not in supply.carrier:
            continue
        first = supply.files[supply.carrier[package][0]]
        overlays[package] = {
            "family": "rooms" if first["stage"] else residency.family_of(first["path"], first["stage"], first["id"]),
            "parents": asset_schema.parents_of(package, assets, tree), "own": [index[p] for p in supply.own(package)],
            "room": int(bool(first["stage"])), "stage": first["stage"] or 0, "addr": address.get(package),
            "carrier": [index[p] for p in supply.carrier[package]]}

    extracted = ROOT / "assets" / "USA"
    side = write_side_data(out.with_name(out.stem + "_data"), assets, embedded) if pixels else {"mesh": [], "track": {}, "holds": {}}
    try:
        banks = animation_tables(embedded)
    except Exception as exc:  # noqa: BLE001
        print(f"no animation tables: {exc}", file=sys.stderr)
        banks = {}
    sounds, sample_ids, sample_homes = {}, {}, {}
    if (extracted / "spk").is_dir():
        for folder in (extracted / "spk").iterdir():
            if folder.is_dir():
                sounds[folder.name] = len((assets.get(folder.name) or {}).get("samples") or ()) or sum(1 for f in folder.iterdir() if f.suffix == ".wav")
    # A bank is a container of samples (tools/peassets/asset_data.py): each slot names one, and one sample can sit in many banks.
    for key, rec in assets.items():
        if rec.get("type") == "spk" and rec.get("samples"):
            sample_ids[key] = rec["samples"]
            for n, name in enumerate(rec["samples"]):
                sample_homes.setdefault(name, []).append([key, n])
    catalogue = {}
    for key, rec in assets.items():
        catalogue[key] = [rec["type"], {k: rec[k] for k in SHOWN if k in rec}]
    for key, rec in embedded.items():
        if key not in catalogue:
            catalogue[key] = [rec["type"], {k: rec[k] for k in SHOWN if k in rec}]
        else:
            catalogue[key][1].update({k: rec[k] for k in ("source", "vram", "size") if k in rec})
    # A room's caption bundle holds a palette per view, in the slot before that view's number, besides the dialogue.
    import base64
    import cap_bundle
    import cap_dialogue
    import spk_scripts
    dialogue = {}
    viewpal = {}
    for stage, folder, fkey, node in asset_schema.tree_files(tree):
        if folder is None:
            continue
        room = next((c for c in (node.get("chunks") or {}).values() if c in overlays), None)
        for key in (node.get("chunks") or {}).values():
            if assets.get(key, {}).get("type") != "pe2cap2" or room is None:
                continue
            raw = extracted / "raw" / "pe2cap2" / f"{key}.pe2cap2"
            if not raw.is_file():
                continue
            data = raw.read_bytes()
            for slot in cap_bundle.slots(data):
                name = f"{key}_slot_{slot['slot']:02d}"
                if slot["what"] == "palette":
                    # Only a palette for (0, 255) is a view's; one stored elsewhere is something the room's code uploads itself.
                    of_view = (slot["x"], slot["y"]) == (0, 255)
                    note = (f"the palette of the view mapped to file {slot['slot'] + 1} of {room}" if of_view
                            else f"a palette in the bundle {room} loads, for ({slot['x']}, {slot['y']}); not a view's, so some code uploads it")
                    catalogue[name] = ["clut", {"source": key, "x": slot["x"], "y": slot["y"], "w": slot["w"], "h": slot["h"], "notes": note}]
                    files.append(f"bundle:{name}")
                    footprint[len(files) - 1].append([name, "c", "", slot["x"], slot["y"], slot["w"], slot["h"], 0])
                    # The slot is the mapped view less one (src/gameplay/view_image.c).
                    if of_view:
                        viewpal.setdefault(room, {})[slot["slot"] + 1] = len(files) - 1
                    if pixels:
                        (out.with_name(pixels) / f"{name}.js").write_text(
                            f"PIX({json.dumps(name)},{json.dumps([[slot['x'], slot['y'], slot['w'], slot['h'], base64.b64encode(cap_bundle.palette_words(data, slot)).decode()]])});\n", encoding="utf-8")
                else:
                    catalogue[name] = [slot["what"], {"source": key, "size": slot["length"], "vram": slot["dest"], "notes": f"slot {slot['slot']} of the bundle {room} loads"}]
                    if slot["what"] == "dialogue":
                        found = cap_dialogue.commands(data[slot["offset"]:slot["offset"] + slot["length"]])
                        dialogue[name] = [line for command in found for line in cap_dialogue.describe(command)]
    # A room's collision grid is an asset too: the overlay manifest declares where it sits in the package.
    import tomllib as _toml
    from gen_overlay_configs import declared_collisions
    for grid in declared_collisions(_toml.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))):
        catalogue[f"{grid['package']}_collision_{grid['grid']:05X}"] = ["collision", {
            "source": grid["package"] + ".pe2pkg", "vram": grid["load"] + grid["grid"],
            "notes": "the room's collision grid: normals, vertices, faces and per-cell face lists; its header follows at "
                     f"0x{grid['load'] + grid['source']:08X}"}]
    # A collision patch has no header: its pieces are arrays some code copies into a room's live grid.
    from gen_overlay_configs import declared_collision_patches
    for patch in declared_collision_patches(_toml.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))):
        catalogue[f"{patch['package']}_collision_{patch['offset']:05X}"] = ["collision", {
            "source": patch["package"] + ".pe2pkg", "vram": patch["load"] + patch["offset"],
            "notes": "a collision patch (" + ", ".join(patch["pieces"]) + "): arrays the overlay's code copies into a room's live grid"}]
    scripts = {key: [found["bank"], [[list(n) for n in notes] if notes is not None else None for notes in found["entries"]]] for key, found in spk_scripts.all_banks().items()}
    scenes = write_scenes(out.with_name(out.stem + "_data"), sorted(n for n, o in overlays.items() if o.get("room"))) if pixels else []
    # Names proposed for a later bulk rename (tools/peassets/asset_names.tsv: current name, new name); the page edits them.
    names = {}
    names_path = Path(__file__).with_name("asset_names.tsv")
    if names_path.is_file():
        for line in names_path.read_text(encoding="utf-8").splitlines():
            cells = line.split("\t")
            if not line.startswith("#") and len(cells) >= 2 and cells[0] and cells[1]:
                names[cells[0]] = cells[1]
    # How the code uses each asset, as read out of the sources (tools/peassets/usage/).
    usage = defaultdict(list)
    by_overlay = defaultdict(lambda: [0, 0])
    remarks = defaultdict(list)
    # What the uses show each asset or package to be, in a sentence, and whether its name is in question
    # (tools/peassets/asset_notes.tsv: key, summary, name question).
    notes = {}
    notes_path = Path(__file__).with_name("asset_notes.tsv")
    if notes_path.is_file():
        for line in notes_path.read_text(encoding="utf-8").splitlines():
            cells = line.split("\t")
            if not line.startswith("#") and len(cells) >= 2:
                notes[cells[0]] = [cells[1], cells[2] if len(cells) > 2 else ""]
    for path in sorted((Path(__file__).with_name("usage")).glob("*.tsv")):
        for line in path.read_text(encoding="utf-8").splitlines():
            cells = line.split("\t")
            if line.startswith("#"):
                # An overlay-level remark of the sweep: what no single asset line carries.
                if not line.startswith("# asset\t") and line.strip("# "):
                    remarks[path.stem].append(line.lstrip("# ").replace("\t", " | "))
                continue
            if len(cells) < 8:
                continue
            # The asset field can name several assets, and ranges of them ("bs_41, bs_43..bs_49"): the use is each one's.
            named = []
            for token in re.findall(r"[A-Za-z0-9_]+(?:\.\.[A-Za-z0-9_]+)?", cells[0]):
                first, _, last = token.partition("..")
                a, b = re.fullmatch(r"(.*?)(\d+)", first), re.fullmatch(r"(.*?)(\d+)", last) if last else None
                span = [f"{a[1]}{n}" for n in range(int(a[2]), int(b[2]) + 1)] if a and b and a[1] == b[1] and 0 <= int(b[2]) - int(a[2]) < 500 else [first]
                named += [k for k in span if k in catalogue and k not in named]
            for key in named or [cells[0]]:
                usage[key].append([path.stem] + cells[1:8])
            by_overlay[path.stem][0 if cells[6] == "established" else 1] += 1
    # Movies and streamed audio are not chunks of the stage files; the extraction lists them.
    streams = {}
    movies = extracted / "movie" / "movies.json"
    if movies.is_file():
        for m in json.loads(movies.read_text(encoding="utf-8")):
            catalogue[m["stem"]] = ["movie", {"w": m.get("width"), "h": m.get("height"), "frames": m.get("frame_count"), "fps": m.get("fps"),
                                              "seconds": m.get("audio_duration_sec"), "container": m.get("container"), "disk": m.get("disk")}]
            streams[m["stem"]] = m.get("mp4", "")
    if (extracted / "audio").is_dir():
        for meta in sorted((extracted / "audio").glob("*.json")):
            info = json.loads(meta.read_text(encoding="utf-8"))
            if meta.with_suffix(".wav").is_file():
                catalogue[meta.stem] = ["stream", {"seconds": info.get("duration_sec"), "rate": info.get("sample_rate"), "channels": info.get("channels")}]
                streams[meta.stem] = f"audio/{meta.stem}.wav"
    saves = sorted(p.name for p in (extracted / "mcsave" / "mc_save_header").glob("*.png")) if (extracted / "mcsave" / "mc_save_header").is_dir() else []
    return {
        "holds": side["holds"], "viewpal": viewpal, "scripts": scripts, "scenes": scenes, "names": names, "usage": usage, "swept": by_overlay, "remarks": remarks, "notes": notes, "dialogue": dialogue,
        "mesh": side["mesh"], "track": side["track"], "banks": banks, "streams": streams, "saves": saves, "side": out.stem + "_data" if pixels else "",
        "files": files, "chunks": chunks, "fp": footprint, "loads": loads, "overlays": overlays, "requests": requests,
        "consts": constants, "findings": findings, "assets": catalogue, "sounds": sounds, "sampleIds": sample_ids, "sampleHomes": sample_homes,
        "pairs": {image: [[bpp, clut, row, pal, column, sorted(users)[:8]] for (bpp, clut, row, pal, column), users in sorted(found.items(), key=str)] for image, found in pairs.items()},
        "regions": [[r["kind"][0], r["name"], r["x"], r["y"], r["w"], r["h"], r["note"]] for r in table], "layers": residency.LAYERS,
        "stagemap": {r["id"] - 900000: index[r["path"]] for r in db.execute("SELECT path, id FROM file WHERE stage = 0 AND id BETWEEN 900001 AND 900005")},
        "pictures": os.path.relpath(extracted.resolve(), out.parent.resolve()) if (extracted / "pe2img").is_dir() else "", "pixels": pixels,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", type=Path, default=residency.DB)
    ap.add_argument("--out", type=Path, default=ROOT / "local" / "asset_map.html")
    ap.add_argument("--checklists", type=Path, help="also write, per overlay, the list of assets it owns: one line each, for a usage sweep")
    args = ap.parse_args()
    if not args.db.is_file():
        print(f"{args.db} is missing: run asset_residency.py build first", file=sys.stderr)
        return 1
    db = sqlite3.connect(args.db)
    db.row_factory = sqlite3.Row
    args.out.parent.mkdir(parents=True, exist_ok=True)
    folder = args.out.with_name(args.out.stem + "_pix")
    pixels = ""
    try:
        n = residency.write_pixels(db, folder)
        pixels = folder.name
        print(f"{n} pixel files written to {folder}" if n else f"pixel files in {folder} are current")
    except ImportError as exc:
        print(f"no pixels, so no rendered textures: {exc} (run with venv/bin/python3 for the image decoders)", file=sys.stderr)
    data = build_data(db, args.out, pixels)
    if args.checklists:
        args.checklists.mkdir(parents=True, exist_ok=True)
        for name, o in data["overlays"].items():
            lines, seen = [], set()
            def add(key, where):
                kind = data["assets"].get(key, ["?"])[0]
                if kind != "pe2pkg" and key not in seen:
                    seen.add(key)
                    lines.append(f"{key}\t{kind}\t{where}")
            for f in o["own"]:
                for chunk, key in data["chunks"].get(f, []):
                    add(key, f"in file {data['files'][f]} as {chunk}")
                    if data["assets"].get(key, [""])[0] == "pe2cap2":
                        for other, (kind, attrs) in data["assets"].items():
                            if attrs.get("source") == key:
                                add(other, f"in the bundle {key}: {attrs.get('notes', '')}")
            for key in data["holds"].get(name, ()):
                add(key, "a model in the package")
            for key, (kind, attrs) in data["assets"].items():
                if attrs.get("source") == name + ".pe2pkg":
                    add(key, "embedded in the package")
            (args.checklists / f"{name}.txt").write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
        print(f"checklists for {len(data['overlays'])} overlays written to {args.checklists}")
    template = TEMPLATE.read_text(encoding="utf-8")
    if template.count("/*DATA*/") != 1:
        raise SystemExit(f"{TEMPLATE}: expected one /*DATA*/ marker")
    page = template.replace("/*DATA*/", json.dumps(data, separators=(",", ":")).replace("</", "<\\/"))
    args.out.write_text(residency.DOCUMENT % page, encoding="utf-8")
    counts = defaultdict(int)
    for rows in data["requests"].values():
        for row in rows:
            counts[row[4][0] if row[4][0] != "own" or row[4][2] else "own, file not established"] += 1
    print(f"wrote {args.out}  ({args.out.stat().st_size // 1024} KB); texture requests: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
