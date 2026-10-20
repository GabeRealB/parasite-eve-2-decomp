#!/usr/bin/env python3
"""What is in memory together: the load graph of the overlays.

An asset is used by code that never names it: a model's faces say "the page at
(832, 0)", a constant says "CLUT row 263". Which image or CLUT that is depends
on what was loaded there, and that is decided by which files the running
overlay has loaded. This tool builds that as a directed graph, in
local/asset_residency.sqlite:

    loader  --(always | optional, condition)-->  group  --(page, row offsets)-->  file

A **loader** is an overlay (a room, an actor, gameplay). A **group** is the
set of files one loader brings in together under one condition, so files that
always arrive together stay together. An edge is `always` when the loader
cannot run without the group - a room and the file that carries it - and
`optional` with its condition otherwise: `view 3`, `variant 2` (a room's enemy
set), `weapon`, and so on. A **file** has a footprint: the rectangles its
images and CLUTs upload to, moved by the edge's offsets.

    asset_residency.py build            rebuild the graph from the manifest, the packages and the traced loads
    asset_residency.py stats
    asset_residency.py show LOADER      a loader's groups and what each brings
    asset_residency.py at LOADER X Y    what can sit at a VRAM position while LOADER runs
    asset_residency.py dot LOADER       the loader's part of the graph, for graphviz

The page that shows all this is tools/peassets/asset_map.py.

Every edge records where it comes from (`evidence`): `carrier` (the file holds
the overlay's own package), `viewmap` (a room's view map sends that view to
that file), `placement` (an `AreaVariant`'s placement and
resource tables), `traced` (a load decided by code, read out of the sources
into `residency_loads.tsv` beside this tool), or `folder` - a file in a room's folder taken to be the view
of its number, which holds for rooms using the identity view map and is an
assumption for the rest until their maps are read.

The view maps and the variant tables are read from the package bytes, along
gameplay's own route to them and with room_scene.py's readers (`view_maps`,
`room_variants`); no C initialiser is parsed. A table that could not be read
is listed on stderr by `build`.
"""
from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_journal  # noqa: E402
import asset_schema  # noqa: E402
from model import declared  # noqa: E402

DB = ROOT / "local" / "asset_residency.sqlite"

DDL = """
DROP TABLE IF EXISTS file; DROP TABLE IF EXISTS footprint; DROP TABLE IF EXISTS grp; DROP TABLE IF EXISTS member; DROP TABLE IF EXISTS spawn;
CREATE TABLE file (path TEXT PRIMARY KEY, stage INTEGER, folder TEXT, id INTEGER, package TEXT);
CREATE TABLE footprint (file TEXT, asset TEXT, kind TEXT, part TEXT, x INTEGER, y INTEGER, w INTEGER, h INTEGER, bpp INTEGER);
CREATE TABLE grp (id INTEGER PRIMARY KEY, loader TEXT, kind TEXT, condition TEXT, evidence TEXT, source TEXT, layer TEXT);
CREATE TABLE spawn (grp INTEGER, package TEXT, page INTEGER, row INTEGER, entry INTEGER);
CREATE TABLE member (grp INTEGER, file TEXT, page INTEGER, row INTEGER, mode TEXT, note TEXT);
CREATE INDEX grp_loader ON grp(loader); CREATE INDEX member_grp ON member(grp); CREATE INDEX footprint_file ON footprint(file);
"""

# `AreaResource.fileGroupIndex` -> the base of the file's group (include/gameplay/area.h).
GROUP_BASE = (10, 20, 30, 40, 50, 60, 0, 1, 2)
SHIFTED_ROWS = range(245, 256)       # header rows the loader's row offset moves
RELOCATED_FROM = 256                 # images at or below this row move by whole pages

AREA_PLACEMENT_END = 0xFF            # ends an AreaPlacement and an AreaResource table (include/gameplay/areaplace.h)
UNUSED_PLACEMENT_ENTRY = 0           # a placement the loader steps over (src/gameplay/area_cd.c)
TEXTURES_ONLY_GROUP = 5              # `AREA_RESOURCE_FILE_GROUP_BASE_60`: loaded only for a placement of the base file


def footprints(assets: dict, node: dict) -> list[tuple]:
    """(asset, kind, part, x, y, w, h, bpp) for each rectangle a file uploads."""
    out = []
    for key in (node.get("chunks") or {}).values():
        rec = assets.get(key, {})
        if rec.get("type") == "pe2clut":
            out.append((key, "clut", "", rec["x"], rec["y"], rec["w"], rec["h"], None))
        elif rec.get("type") in asset_schema.IMAGE_TYPES:
            columns = asset_schema.image_columns(rec)
            if isinstance(rec["x"], list) or len({b for _x, _y, b in columns}) > 1:
                for n, (x, y, bpp) in enumerate(columns):
                    out.append((key, "image", f"column {n}", x, y, asset_schema.IMAGE_COLUMN_WIDTH, rec["h"], bpp))
            else:
                out.append((key, "image", "", rec["x"], rec["y"], rec["w"], rec["h"], columns[0][2]))
    return out


def moved(rect: tuple, page: int, row: int, mode: str = "") -> tuple[int, int]:
    """Where a rectangle lands when its file is loaded with these offsets (src/main/fs.c).

    The row offset moves only a CLUT whose header is at rows 245..255. The
    page offset moves an image at or below row 256, or every image when the
    mode relocates images; that mode also puts the first image column 128
    rows down and every CLUT one more row down.
    """
    _asset, kind, part, x, y, _w, _h, _bpp = rect
    relocate = "RELOCATE" in (mode or "")
    if kind == "clut":
        return x, y + (row if y in SHIFTED_ROWS else 0) + (1 if relocate else 0)
    if relocate or y >= RELOCATED_FROM:
        x += page * asset_schema.IMAGE_COLUMN_WIDTH
    if relocate and part in ("", "column 0"):
        y += 128
    return x, y


def scenes():
    """room_scene.py, whose `Project` and `Scene` read every room's tables from the package bytes.

    Imported on use: it imports `GROUP_BASE` from this module.
    """
    import room_scene
    return room_scene


def package_loads(manifest: dict) -> dict[str, int]:
    """package -> its load address; an actor's follows from the slot it is linked for (configs/USA/overlays.toml)."""
    sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
    from gen_overlay_configs import slot_packages
    out: dict[str, int] = {}
    for spec in manifest.values():
        if isinstance(spec, dict) and "overlays" in spec:
            own = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
            for name, slot, _entry in slot_packages(spec):
                out[name] = own.get(slot.get("slot"), int(spec["load_addr"]))
    return out


def unread(scene, blocks: tuple[str, ...]) -> list[str]:
    """What a room's scene could not read of these blocks, one line each."""
    return [f"{scene.room}: {u['what']} - {u['reason']}" for u in scene.unresolved if u["block"] in blocks]


def room_variants(project=None, problems: list[str] | None = None) -> dict[str, dict]:
    """room package -> its `AreaVariant` table, decoded from the bytes.

    The route is gameplay's: `gAreaRecordsByStage[stage][area].variants` leads into the room's package, each slot to an
    `AreaPlacement` and an `AreaResource` table, both ended by `AREA_PLACEMENT_END` in `entryId`. A resource row
    gains `file` (its base file's stage-zero id, None when its group selector is outside `GROUP_BASE`) and `owner`,
    the package its task table is in: the image mapped at that address while the room runs - gameplay, the stage's
    map overlay or the room itself - else the package, of those the variant's own files carry, whose image holds the
    address, the row's own files first; else what a symbol map says is there (`owner=`).
    """
    rs = scenes()
    project = project or rs.Project()
    loads = package_loads(project.manifest)
    out: dict[str, dict] = {}
    for room in sorted(project.place_of):
        scene = rs.Scene(project, room)
        record, _directory = scene.directory("areas")
        slots, info = scene.table("placements", record["variants"], rs.VARIANT) if record and record["variants"] else (None, None)
        variants = []
        for slot in slots or []:
            read = {}
            for field, layout in (("resources", rs.RESOURCE), ("placements", rs.PLACEMENT)):
                rows, _info = scene.table("placements", slot[field], layout, stop=lambda r: r["entryId"] == AREA_PLACEMENT_END) if slot[field] else (None, None)
                read[field] = [r for r in rows or [] if r["entryId"] != AREA_PLACEMENT_END]
            carried: dict[int, list[str]] = defaultdict(list)       # resource row -> the packages its files carry
            for n, r in enumerate(read["resources"]):
                lows = dict.fromkeys([0] + [p["fileIdLow"] for p in read["placements"] if p["entryId"] == r["entryId"]])
                files = [scene.resolve_file(r, low) for low in lows]
                r["file"] = files[0]["fileId"]
                carried[n] = list(dict.fromkeys(f["package"] for f in files if f["package"]))
            for n, r in enumerate(read["resources"]):
                address, r["owner"] = r["taskTable"], None
                if not address:
                    continue
                candidates = list(dict.fromkeys(carried[n] + [p for packages in carried.values() for p in packages]))
                held = [p for p in candidates if p in loads and project.package(p) is not None and 0 <= address - loads[p] < len(project.package(p))]
                r["owner"] = scene.owner_of(address) or (held[0] if held else scene.foreign(address, candidates)[1])
                r["ownerBy"] = "mapped" if scene.owner_of(address) else "loaded" if held else "symbol map" if r["owner"] else None
                if r["owner"] is None and problems is not None:
                    problems.append(f"{room}: entry {r['entryId']}'s task table 0x{address:08X} is in no image the variant loads and no symbol map says whose it is")
            variants.append({"placements": scene.ref(slot["placements"], rs.sizeof(rs.PLACEMENT)), "resources": scene.ref(slot["resources"], rs.sizeof(rs.RESOURCE)),
                             "placed": read["placements"], "rows": read["resources"]})
        if problems is not None:
            problems += unread(scene, ("placements",))
        if slots is not None:
            where = info["where"]
            out[room] = {"table": info["symbol"], "source": f"{where.get('file') or where['address']}:{where['symbol']}", "variants": variants}
    return out


# Loads decided by code, read out of the sources and kept as a table beside
# this tool: loader, kind, condition, files, page, row, mode, evidence, note.
# `files` is one stage-zero id, a comma list (loaded together), a range
# `A..B by <what>` (one of them at a time), or `stage/folder-id/index`.
TRACED = Path(__file__).with_name("residency_loads.tsv")


def traced_loads(common: dict[int, str], folders: dict[tuple[int, int], dict[int, str]]):
    """(loader, kind, condition, evidence, [(file, page, row, note)]) per traced group."""
    if not TRACED.is_file():
        return
    together: dict[tuple, list] = defaultdict(list)
    number = lambda text: asset_journal.evaluate(text, {}) or 0  # noqa: E731
    for n, line in enumerate(TRACED.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        cells = [c.strip() for c in (line.split("\t") + [""] * 9)[:9]]
        loader, kind, condition, spec, page, row, mode, evidence, note = cells
        if kind not in ("always", "optional"):
            print(f"{TRACED.name}:{n}: kind is {kind!r}", file=sys.stderr)
            continue
        # Offsets that depend on the caller ("-8 or 0") are not one number;
        # the file is then placed unshifted and the note keeps the text.
        fixed = re.fullmatch(r"[-+]?\w+", page) and re.fullmatch(r"[-+]?\w+", row)
        offsets = (number(page), number(row), mode) if fixed else (0, 0, "")
        if not fixed:
            note = f"offsets {page} / {row} ({mode}); " + note
        placed = re.match(r"(\d)/(\d+)/(\d+|\*)$", spec)
        if spec.startswith("stage/"):
            continue                       # a rule about the current room, not an edge
        if placed:
            files = folders.get((int(placed[1]), int(placed[2])), {})
            hit = [path for index, path in sorted(files.items()) if placed[3] in ("*", str(index))]
            if not hit:
                print(f"{TRACED.name}:{n}: no file {spec}", file=sys.stderr)
            for path in hit:
                together[loader, kind, condition, evidence].append((path,) + offsets + (note,))
            continue
        what = spec.split(" by ", 1)
        ids: list[int] = []
        for part in re.split(r"\s*,\s*", what[0]):
            span = re.fullmatch(r"(\d+)\s*\.\.\s*(\d+)", part)
            if span:
                ids += sorted(i for i in common if int(span[1]) <= i <= int(span[2]))
            elif part.isdigit() and int(part) in common:
                ids.append(int(part))
            elif part.isdigit():
                print(f"{TRACED.name}:{n}: no stage-zero file {part}", file=sys.stderr)
        if len(what) == 2 or ".." in what[0]:
            # One of a family at a time: a group of its own for each file.
            for fid in ids:
                name = common[fid].split("/", 1)[1]
                yield loader, "optional", f"{condition}: {name}" if condition else name, evidence, [(common[fid],) + offsets + (note,)]
        else:
            for fid in ids:
                together[loader, kind, condition, evidence].append((common[fid],) + offsets + (note,))
    merged: dict[tuple, list] = defaultdict(list)
    evidence_of: dict[tuple, str] = {}
    for (loader, kind, condition, evidence), members in together.items():
        merged[loader, kind, condition] += members
        evidence_of.setdefault((loader, kind, condition), evidence)
    for (loader, kind, condition), members in merged.items():
        yield loader, kind, condition, evidence_of[loader, kind, condition], members


def view_maps(project=None, problems: list[str] | None = None) -> dict[str, list[list[int]]]:
    """room package -> for each of its room indices, the file index of each view (1-based views).

    Read from the packages along gameplay's route (room_scene.py): the stage's map overlay holds, per area, a
    directory of byte maps, one per room index (`ViewIndexTable` in include/gameplay/view.h), and
    `map[view - 1]` is the index of the view's file in the room's folder. Most room indices borrow gameplay's
    identity map, whose 50 bytes are read like any other.

    A map is taken at its symbol's size, padding included (a zero is no file). A room's `ViewCount` only bounds the
    reverse search (`viewFindLogicalIndex`); the forward lookup is not bounded by it, so a boundary trigger can
    select a view past the count and that view's file is a real view (dryfield_motel_room_3 walks views 7 and 8
    with a count of 6). The count cuts a map only where the symbol map gives the map no size, so that its extent is
    a guess at where the next object starts.
    """
    rs = scenes()
    project = project or rs.Project()
    out: dict[str, list[list[int]]] = {}
    for room in sorted(project.place_of):
        scene = rs.Scene(project, room)
        maps = []
        for row in scene.room_resources():
            name, count, mapping = row["viewMap"], row["viewCount"], None
            if name:
                identity = scene.addresses[name] == rs.IDENTITY_MAP
                mapping, info = scene.table("views", scene.addresses[name], "u8", count=rs.IDENTITY_MAP_LENGTH if identity else None)
                if mapping is not None and count and info["length"] not in ("count", "symbol size"):
                    mapping = mapping[:count]
            maps.append(mapping or [])
        while maps and not maps[-1]:
            maps.pop()                  # a room index with collision but no map of its own
        if maps:
            out[room] = maps
        if problems is not None:
            problems += unread(scene, ("views",))
    return out


def build(db: sqlite3.Connection) -> None:
    db.executescript(DDL)
    assets, _embedded = asset_schema.load()
    tree = asset_schema.load_tree()
    packages = asset_schema.package_names(assets)      # package asset -> overlay name
    common: dict[int, str] = {}          # stage-zero file id -> path
    carriers: dict[str, list[str]] = defaultdict(list)
    for stage, folder, fkey, node in asset_schema.tree_files(tree):
        path = f"{stage}/{fkey}" if folder is None else f"{stage}/{folder}/{fkey}"
        package = next((packages[c] for c in (node.get("chunks") or {}).values() if c in packages), None)
        db.execute("INSERT INTO file VALUES (?,?,?,?,?)", (path, stage, folder, int(node["id"]), package))
        db.executemany("INSERT INTO footprint VALUES (?,?,?,?,?,?,?,?,?)", [(path,) + r for r in footprints(assets, node)])
        if stage == 0 and folder is None:
            common[int(node["id"])] = path
        if package:
            carriers[package].append(path)

    def group(loader, kind, condition, evidence, source, members):
        layer = "" if kind == "carrier" else layer_of(loader, kind, condition, evidence, [m[0] for m in members])
        gid = db.execute("INSERT INTO grp (loader, kind, condition, evidence, source, layer) VALUES (?,?,?,?,?,?)",
                         (loader, kind, condition, evidence, source, layer)).lastrowid
        db.executemany("INSERT INTO member VALUES (?,?,?,?,?,?)", [(gid,) + m for m in members])
        return gid

    # An overlay and the file that carries it. This is identity, not a load:
    # the file is loaded by whoever brings the overlay in, with their offsets.
    for package, paths in sorted(carriers.items()):
        for path in paths:
            group(package, "carrier", "", "carrier", path, [(path, 0, 0, "", "")])
    # A room's other files are its views, one loaded at a time; which file a
    # view is comes from the room's view map.
    project = scenes().Project()
    problems: list[str] = list(project.root_problems)
    maps = view_maps(project, problems)
    rooms: dict[str, str] = {}
    unmapped = 0
    for stage, body in tree.items():
        for folder, fnode in (body.get("folders") or {}).items():
            room = next((packages[c] for n in fnode["files"].values() for c in (n.get("chunks") or {}).values() if c in packages), None)
            if room is None:
                continue
            rooms[room] = f"{stage}/{folder}"
            by_index = {int(n["id"]): k for k, n in fnode["files"].items()}
            reached = set()
            for number, mapping in enumerate(maps.get(room, ()), 1):
                for view, index in enumerate(mapping, 1):
                    if index in by_index and index:
                        reached.add(index)
                        label = f"view {view}" if len(maps[room]) == 1 else f"room {number} view {view}"
                        group(room, "optional", label, "viewmap", f"{stage}/{folder}",
                              [(f"{stage}/{folder}/{by_index[index]}", 0, 0, "", "" if index == view else f"file {index}")])
            for fkey, node in fnode["files"].items():
                if int(node["id"]) not in reached and not any(c in packages for c in (node.get("chunks") or {}).values()):
                    unmapped += 1
                    group(room, "optional", f"file {node['id']}", "folder", f"{stage}/{folder}",
                          [(f"{stage}/{folder}/{fkey}", 0, 0, "", "no view maps to it")])

    # Gameplay loads the room the player is in: its base file, which carries
    # the room's overlay (loadingEnqueueAreaAndCompanionResourcesTask).
    for room, where in sorted(rooms.items()):
        for path in carriers[room]:
            group("gameplay", "optional", f"room: {room}", "rule", "src/gameplay/load_screen.c", [(path, 0, 0, "", "")])
    # What an overlay uploads itself, from an image or CLUT embedded in it,
    # where its upload record fixes the position.
    for key, rec in sorted(_embedded.items()):
        package = rec.get("source", "").removesuffix(".pe2pkg")
        if rec.get("type") in ("image", "clut") and "x" in rec and carriers.get(package):
            path = carriers[package][0]
            db.execute("INSERT INTO footprint VALUES (?,?,?,?,?,?,?,?,?)",
                       (f"code:{key}", key, rec["type"], "", rec["x"], rec["y"], rec["w"], rec["h"], rec.get("bpp")))
            db.execute("INSERT OR IGNORE INTO file VALUES (?,?,?,?,?)", (f"code:{key}", None, None, None, None))
            group(package, "optional", f"upload: {key}", "embedded", path, [(f"code:{key}", 0, 0, "", "uploaded by the overlay's own code")])

    # A room's variants: each pairs placements with the resources they name.
    missing: set[int] = set()
    for room, table in sorted(room_variants(project, problems).items()):
        for index, variant in enumerate(table["variants"]):
            rows, placed = variant["rows"], variant["placed"]
            # (loadingPollAreaBaseResources / loadingPollAreaPlacementFiles.) Every resource row is walked, and a
            # placement says with what offsets and which further file of its entry.
            loads: dict[tuple, str] = {}
            for r in rows:
                if r["file"] is None:
                    problems.append(f"{room}: variant {index} entry {r['entryId']} has file group selector {r['fileGroupIndex']}, outside the loader's table")
                    continue
                entry = r["entryId"]
                named = [p for p in placed if p["entryId"] == entry]
                # The row's base file is loaded once, with the offsets of the first placement that names the entry
                # without a further file, else unshifted - except a texture-only row, loaded only for such a placement.
                first = next((p for p in named if not p["fileIdLow"]), None)
                if first is not None:
                    loads[r["file"], first["texturePageOffset"], first["clutRowOffset"]] = f"entry {entry}"
                elif r["fileGroupIndex"] != TEXTURES_ONLY_GROUP:
                    loads[r["file"], 0, 0] = f"entry {entry}, " + ("placed only with a further file" if named else "not placed")
            # Each placement's further file comes in addition, with that placement's own offsets, beside the base
            # file of the first resource row with its entry id.
            for p in placed:
                if not p["fileIdLow"] or p["entryId"] == UNUSED_PLACEMENT_ENTRY:
                    continue
                r = next((r for r in rows if r["entryId"] == p["entryId"]), None)
                if r is None:
                    problems.append(f"{room}: variant {index} places entry {p['entryId']} with further file {p['fileIdLow']}, and no resource row has that entry")
                elif r["file"] is not None:
                    loads[r["file"] + p["fileIdLow"], p["texturePageOffset"], p["clutRowOffset"]] = f"entry {p['entryId']}, file {p['fileIdLow']}"
            members = []
            for (fid, page, row), note in sorted(loads.items()):
                if fid in common:
                    members.append((common[fid], page, row, "", note))
                else:
                    missing.add(fid)
            if members:
                gid = group(room, "optional", f"variant {index}", "placement", table["source"], members)
                # What a placement spawns is the actor its resource's task table belongs to, displaced by that
                # placement's offsets - which need not be the offsets its package's file came with.
                spawned = set()
                for p in placed:
                    r = next((r for r in rows if r["entryId"] == p["entryId"]), None)
                    if r is not None and r["owner"] in packages.values():
                        spawned.add((r["owner"], p["texturePageOffset"], p["clutRowOffset"], p["entryId"]))
                db.executemany("INSERT INTO spawn VALUES (?,?,?,?,?)", [(gid,) + sp for sp in sorted(spawned)])
    folders: dict[tuple[int, int], dict[int, str]] = {}
    for stage, body in tree.items():
        for folder, fnode in (body.get("folders") or {}).items():
            folders[stage, int(fnode["id"])] = {int(n["id"]): f"{stage}/{folder}/{k}" for k, n in fnode["files"].items()}
    for loader, kind, condition, evidence, members in traced_loads(common, folders):
        group(loader, kind, condition, "traced", evidence, members)
    db.commit()
    print(f"{db.execute('SELECT COUNT(*) FROM file').fetchone()[0]} files, "
          f"{db.execute('SELECT COUNT(*) FROM footprint').fetchone()[0]} uploaded rectangles, "
          f"{db.execute('SELECT COUNT(*) FROM grp').fetchone()[0]} groups"
          + (f"; {unmapped} room files no view maps to" if unmapped else "")
          + (f"; placements name {len(missing)} files the disc does not have, so nothing is loaded for them: {sorted(missing)}" if missing else "")
          + (f"; {len(problems)} tables or records not read (below)" if problems else ""))
    for line in problems:
        print(f"  {line}", file=sys.stderr)


# The template is the page's content only, as an artifact host wraps it; a
# local file needs the document around it.
DOCUMENT = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            '<style>body{margin:0}[hidden]{display:none!important}</style>\n</head>\n<body>\n%s\n</body>\n</html>\n')


def raw_columns(kind: str, data: bytes) -> list[tuple[int, int, int, int, bytes]]:
    """(x, y, w, h, little-endian VRAM words) for each rectangle a raw image or CLUT uploads."""
    import struct
    import image_codec          # needs Pillow, like the rest of the image tools
    from lzss import decode_lzss
    if kind == "clut":
        x, y, h, w = struct.unpack_from("<4H", data, 0)
        words = decode_lzss(data[16:])[: w * h * 2]
        return [(x, y, w, h, words + bytes(w * h * 2 - len(words)))]
    entries, term_y = image_codec.parse_work_entries(data)
    height = image_codec.height_from_work_entries(entries, term_y)
    per_column = max(1, height // image_codec.STRIP_H)
    strips = list(image_codec.iter_lzss_strips(data[entries[0].offset:]))
    out = []
    for n, entry in enumerate(entries):
        words = b"".join(strips[n * per_column:(n + 1) * per_column])
        need = asset_schema.IMAGE_COLUMN_WIDTH * height * 2
        out.append((entry.x, entry.y, asset_schema.IMAGE_COLUMN_WIDTH, height, words[:need] + bytes(max(0, need - len(words)))))
    return out


def write_pixels(db: sqlite3.Connection, folder: Path) -> int:
    """One small script per image and CLUT holding its VRAM words, for the page to load on demand.

    A page opened from disk may not read the pixels of a picture it shows, but
    it may run a script beside it; so the words go in scripts. They are game
    data and stay under local/. A file newer than its source is left alone.
    """
    import base64
    import json
    raw = ROOT / "assets" / "USA" / "raw"
    folder.mkdir(parents=True, exist_ok=True)
    written = 0
    for asset, kind, code, x, y, w, h in db.execute("SELECT DISTINCT asset, kind, file LIKE 'code:%', x, y, w, h FROM footprint"):
        source = raw / ("pe2clut" if kind == "clut" else "pe2img") / f"{asset}.{'pe2clut' if kind == 'clut' else 'pe2img'}"
        if code:
            source = raw / ("clut" if kind == "clut" else "img") / f"{asset}.{'clut' if kind == 'clut' else 'img'}"
        try:                                    # a name sharing its bytes is stored under another (raw_rel)
            source = raw.parent / asset_schema.raw_rel(asset)
        except KeyError:
            pass
        target = folder / f"{asset}.js"         # a grouped asset's name is a path
        target.parent.mkdir(parents=True, exist_ok=True)
        if not source.is_file() or (target.is_file() and target.stat().st_mtime >= source.stat().st_mtime):
            continue
        try:
            # An embedded upload is bare words, the size its record gives.
            columns = [(x, y, w, h, source.read_bytes()[: w * h * 2].ljust(w * h * 2, b"\0"))] if code else raw_columns(kind, source.read_bytes())
        except Exception as exc:  # noqa: BLE001 - one undecodable asset must not stop the page
            print(f"{asset}: {exc}", file=sys.stderr)
            continue
        payload = [[x, y, w, h, base64.b64encode(words).decode()] for x, y, w, h, words in columns]
        target.write_text(f"PIX({json.dumps(asset)},{json.dumps(payload, separators=(',', ':'))});\n", encoding="utf-8")
        written += 1
    return written


def brief(note: str) -> str:
    """A traced load's note runs to a paragraph; the page has room for its first clause."""
    note = (note or "").split(". ")[0]
    return note if len(note) <= 72 else note[:71].rstrip() + "…"


_LOADS: dict[str, tuple[int, bytes]] = {}


def model_bounds(package: str, rec: dict) -> dict[tuple[int, int], list[int]]:
    """(page, CLUT) -> [u0, v0, u1, v1]: the part of the page a model's faces cover."""
    from model import tmd
    if not _LOADS:
        for name, (load, _sources) in tmd.declared_records(ROOT).items():
            path = ROOT / "assets" / "USA" / "pe2pkg" / f"{name}.pe2pkg"
            if path.is_file():
                _LOADS[name] = (load, path.read_bytes())
    if package not in _LOADS:
        return {}
    load, data = _LOADS[package]
    try:
        return {pair: list(box) for pair, box in (tmd.stream_texture_bounds(data, asset_schema.embedded_offset(rec, load)) or {}).items()}
    except Exception:  # noqa: BLE001 - bounds are a nicety
        return {}


FAMILIES = {0: "boot and load screens", 1: "costume, weapon, ammunition", 2: "menus and previews", 3: "map pictures",
            4: "music", 5: "spells", 80: "companions", 90: "stage map"}


def family_of(path: str, stage, file_id) -> str:
    """The kind of file, by where it sits on the disc."""
    if path.startswith("code:"):
        return "uploads by code"
    if stage:
        return "room base file" if not file_id else "room view file"
    group = (file_id or 0) // 10000
    if group in FAMILIES:
        return FAMILIES[group]
    if group // 10 in (8, 9):
        return FAMILIES[group // 10 * 10]
    return "actor textures only" if group >= 60 else "actors"


def regions(db: sqlite3.Connection) -> None:
    """Per family of files: where their uploads land, over every load in the graph.

    This is the question of whether video memory is carved up by purpose. A
    landing is counted once per (file, offsets) the graph loads it with.
    """
    kinds = {r["path"]: family_of(r["path"], r["stage"], r["id"]) for r in db.execute("SELECT * FROM file")}
    images: dict[str, dict[tuple, int]] = defaultdict(lambda: defaultdict(int))
    cluts: dict[str, dict[tuple, int]] = defaultdict(lambda: defaultdict(int))
    seen = set()
    for m in db.execute("SELECT DISTINCT file, page, row, mode FROM member"):
        for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
            x, y = moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
            key = (m["file"], f["asset"], f["part"], x, y)
            if key in seen:
                continue
            seen.add(key)
            if f["kind"] == "clut":
                for row in range(f["h"]):
                    cluts[kinds[m["file"]]][y + row, f["w"]] += 1
            else:
                images[kinds[m["file"]]][x // 64 * 64, y // 64 * 64, f["bpp"]] += 1
    for family in sorted(set(images) | set(cluts)):
        print(family)
        by_band: dict[tuple, list] = defaultdict(list)
        for (x, y, bpp), n in sorted(images[family].items()):
            by_band[y, bpp].append((x, n))
        for (y, bpp), pages in sorted(by_band.items()):
            print(f"  images at row {y:>3}, {bpp}-bit: " + ", ".join(f"x {x} ({n})" for x, n in pages))
        rows: dict[int, int] = defaultdict(int)
        for (row, _w), n in cluts[family].items():
            rows[row] += n
        if rows:
            print("  CLUT rows: " + ", ".join(f"{row} ({n})" for row, n in sorted(rows.items())))


# When a load happens, coarsely: a later layer replaces an earlier one in the
# slots it touches. `event` is a load some scene makes while the room runs;
# `transient` is up only for a while (a menu, a load screen) and is undone.
LAYERS = ("boot", "session", "stage", "room", "variant", "view", "event", "transient")


def layer_of(loader: str, kind: str, condition: str, evidence: str, files: list[str]) -> str:
    if evidence in ("viewmap", "folder"):
        return "view"
    if evidence == "placement":
        return "variant"
    if evidence == "rule":
        return "room"
    if evidence != "traced":
        return "event"
    c = condition.lower()
    if loader in ("main", "title"):
        if kind == "always":
            return "boot"
        return "session" if "session" in c else "stage" if "music" in c else "transient" if ("load screen" in c or "disc" in c) else "event"
    if loader == "gameplay":
        if any(f.startswith("0/map_") or "/file9000" in f for f in files) or c.startswith("stage"):
            return "stage"
        if any(w in c for w in ("menu", "preview", "game over", "death", "ending", "map help")):
            return "transient"
        if any(w in c for w in ("costume", "weapon", "ammo", "spell", "healing", "companion", "parasite")):
            return "session"
    return "event"


REGIONS = Path(__file__).with_name("residency_regions.tsv")


def region_table() -> list[dict]:
    out = []
    for line in REGIONS.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        kind, name, x, y, w, h, families, note = (line.split("\t") + [""] * 8)[:8]
        out.append({"kind": kind, "name": name, "x": int(x), "y": int(y), "w": int(w), "h": int(h),
                    "families": {f.strip() for f in families.split(";") if f.strip()}, "note": note})
    return out


def region_of(table: list[dict], kind: str, x: int, y: int) -> dict | None:
    return next((r for r in table if r["kind"] == kind and r["x"] <= x < r["x"] + r["w"] and r["y"] <= y < r["y"] + r["h"]), None)


def check(db: sqlite3.Connection, verbose: bool) -> int:
    """Hold every load in the graph against the region table and the load addresses."""
    table = region_table()
    kinds = {r["path"]: family_of(r["path"], r["stage"], r["id"]) for r in db.execute("SELECT * FROM file")}
    counts: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    wrong: dict[tuple, list[str]] = defaultdict(list)
    seen = set()
    for m in db.execute("SELECT DISTINCT m.file, m.page, m.row, m.mode, g.loader, g.condition FROM member m JOIN grp g ON g.id = m.grp "
                        "WHERE g.kind != 'carrier'"):
        for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
            x, y = moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
            key = (m["file"], f["asset"], f["part"], x, y)
            if key in seen:
                continue
            seen.add(key)
            region = region_of(table, f["kind"], x, y)
            name = region["name"] if region else "outside every region"
            family = kinds[m["file"]]
            counts[name][family] += 1
            if region is None or family not in region["families"]:
                wrong[name, family].append(f"{m['file']} {f['asset']} at ({x},{y}) by {m['loader']} [{m['condition'] or 'always'}]")
    for region in table + [{"name": "outside every region", "kind": "", "families": set()}]:
        if region["name"] not in counts:
            continue
        parts = [f"{family} {n}" + ("" if family in region["families"] else " !") for family, n in sorted(counts[region["name"]].items(), key=lambda kv: -kv[1])]
        print(f"  {region['kind']:<5} {region['name']:<20} " + ", ".join(parts))
    total = sum(len(v) for v in wrong.values())
    print(f"{len(seen)} uploads placed; {total} land where the table does not expect their family")
    for (name, family), lines in sorted(wrong.items()):
        print(f"  {family} in {name}: {len(lines)}")
        for line in lines[: (20 if verbose else 3)]:
            print(f"      {line}")
    return 1 if total else 0


def chain_check(db: sqlite3.Connection, verbose: bool) -> int:
    """Does every texture a model names come from its own overlay's files or an ancestor's?

    An overlay's own files are the one that carries it, the further files of
    its entry, its texture-only file, and for a room its view files. A model
    and its file are displaced together, so a face's stored page and CLUT are
    held against the file's stored positions first, then against where the
    file lands under the offsets it is loaded with.
    """
    assets, embedded = asset_schema.load()
    tree = asset_schema.load_tree()
    files = {r["path"]: r for r in db.execute("SELECT * FROM file")}
    prints = defaultdict(list)
    for r in db.execute("SELECT * FROM footprint"):
        prints[r["file"]].append(tuple(r)[1:])
    offsets = defaultdict(set)
    for m in db.execute("SELECT m.file, m.page, m.row, m.mode FROM member m JOIN grp g ON g.id = m.grp WHERE g.kind != 'carrier'"):
        offsets[m["file"]].add((m["page"], m["row"], m["mode"] or ""))
    carrier = defaultdict(list)
    for r in files.values():
        if r["package"]:
            carrier[r["package"]].append(r["path"])
    zero = {r["id"]: r["path"] for r in files.values() if r["stage"] == 0 and r["id"] is not None}

    def own_files(package: str) -> set[str]:
        if package == asset_schema.ROOT_OVERLAY:
            return {zero[1]} if 1 in zero else set()         # what main loads at boot
        out = set(carrier[package])
        for path in carrier[package]:
            f = files[path]
            if f["stage"]:
                out |= {p for p, g in files.items() if g["stage"] == f["stage"] and g["folder"] == f["folder"]}
            else:
                out |= {p for i, p in zero.items() if i // 100 == f["id"] // 100 and not files[p]["package"]}
                out |= {p for i, p in zero.items() if 60 <= i // 10000 < 70 and (i % 100000) // 100 == (f["id"] % 100000) // 100}
        return out

    def rects(paths, landed: bool):
        out = []
        for path in paths:
            for at in (offsets.get(path) or {(0, 0, "")}) if landed else {(0, 0, "")}:
                for r in prints.get(path, ()):
                    x, y = moved(r, *at) if landed else (r[3], r[4])
                    out.append((r[1], x, y, r[5], r[6]))
        return out

    def resolves(found, page: int, clut: int) -> bool:
        bpp = (4, 8, 16)[(page >> 7) & 3] if (page >> 7) & 3 < 3 else 16
        tx, ty, cx, cy = (page & 15) * 64, 256 if page & 16 else 0, (clut & 63) * 16, clut >> 6
        covers = lambda kind, x, y, w, h: any(k == kind and rx < x + w and x < rx + rw and ry < y + h and y < ry + rh for k, rx, ry, rw, rh in found)  # noqa: E731
        return covers("image", tx, ty, 64 if bpp == 4 else 128, 256) and (bpp == 16 or covers("clut", cx, cy, 1, 1))

    cache: dict[tuple, list] = {}
    supply = lambda package, landed: cache.setdefault((package, landed), rects(own_files(package), landed))  # noqa: E731
    tally: dict[str, int] = defaultdict(int)
    outside = []
    for key, rec in sorted(embedded.items()):
        package = rec.get("source", "").removesuffix(".pe2pkg")
        if rec.get("type") not in ("model", "tmd") or package not in carrier:
            continue
        chain = asset_schema.ancestors_of(package, assets, tree)
        texture_pairs = (declared.texture_pairs(key, assets) if rec["type"] == "model"
                         else assets.get(key, {}).get("textures", ()))
        for page, clut in texture_pairs:
            page &= ~0x60
            if resolves(supply(package, False), page, clut) or resolves(supply(package, True), page, clut):
                tally["its own files"] += 1
                continue
            by = next((a for a in chain if resolves(supply(a, True), page, clut)), None)
            if by:
                tally[f"an ancestor ({'room' if files[carrier[by][0]]['stage'] else by})" if by in carrier else f"an ancestor ({by})"] += 1
            else:
                tally["nothing in its chain"] += 1
                outside.append(f"{key} in {package}: page ({(page & 15) * 64},{256 if page & 16 else 0}), CLUT ({(clut & 63) * 16},{clut >> 6}); chain {' > '.join(chain)}")
    print("textures named by models, by who supplies them: " + ", ".join(f"{k} {v}" for k, v in sorted(tally.items(), key=lambda kv: -kv[1])))
    for line in outside[: (200 if verbose else 12)]:
        print(f"  {line}")
    if len(outside) > 12 and not verbose:
        print(f"  ... {len(outside) - 12} more (--verbose)")
    return 1 if outside else 0


def slots(db: sqlite3.Connection) -> None:
    """Which overlays share a load address, so cannot be in memory together."""
    import tomllib
    sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
    from gen_overlay_configs import slot_packages
    manifest = tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))
    at: dict[int, list[str]] = defaultdict(list)
    families: dict[int, set[str]] = defaultdict(set)
    for family, spec in manifest.items():
        if not isinstance(spec, dict) or "overlays" not in spec:
            continue
        own = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
        for name, slot, _entry in slot_packages(spec):
            address = own.get(slot.get("slot"), int(spec["load_addr"]))
            at[address].append(name)
            families[address].add(family)
    for address in sorted(at):
        print(f"  {address:#010x}  {len(at[address]):>3} overlays  {', '.join(sorted(families[address]))}")


def resident(db: sqlite3.Connection, loader: str):
    """(group row, member row, footprint row, x, y) for everything LOADER can bring in."""
    # A room's base file is loaded by gameplay, by rule, and is resident whenever the room is.
    base = db.execute("SELECT g.* FROM grp g JOIN member m ON m.grp = g.id WHERE g.evidence = 'rule' AND m.file IN "
                      "(SELECT m2.file FROM member m2 JOIN grp g2 ON g2.id = m2.grp WHERE g2.loader = ? AND g2.kind = 'carrier')", (loader,)).fetchall()
    own = db.execute("SELECT * FROM grp WHERE loader = ? AND kind != 'carrier' ORDER BY kind, id", (loader,)).fetchall()
    for g in base + own:
        for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
            for f in db.execute("SELECT * FROM footprint WHERE file = ?", (m["file"],)):
                x, y = moved(tuple(f)[1:], m["page"], m["row"], m["mode"])
                yield g, m, f, x, y


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", type=Path, default=DB)
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("build")
    sub.add_parser("stats")
    sub.add_parser("regions", help="where each family of files lands in video memory")
    sub.add_parser("slots", help="the load addresses overlays compete for")
    p = sub.add_parser("check", help="hold every load against the region table")
    p.add_argument("--verbose", action="store_true")
    for name in ("show", "dot"):
        p = sub.add_parser(name); p.add_argument("loader")
    p = sub.add_parser("at"); p.add_argument("loader"); p.add_argument("x", type=lambda v: int(v, 0)); p.add_argument("y", type=lambda v: int(v, 0))
    args = ap.parse_args()
    args.db.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(args.db)
    db.row_factory = sqlite3.Row

    if args.command == "build":
        build(db)
    elif args.command == "regions":
        regions(db)
    elif args.command == "check":
        return check(db, args.verbose) | chain_check(db, args.verbose)
    elif args.command == "slots":
        slots(db)
    elif args.command == "stats":
        for row in db.execute("SELECT kind, evidence, COUNT(*), COUNT(DISTINCT loader) FROM grp GROUP BY 1, 2"):
            print(f"  {row[0]:<9} {row[1]:<10} {row[2]:>5} groups, {row[3]:>4} loaders")
        sizes = defaultdict(int)
        for (n,) in db.execute("SELECT COUNT(*) FROM member GROUP BY grp"):
            sizes[min(n, 6)] += 1
        print("  files per group: " + ", ".join(f"{k}{'+' if k == 6 else ''}: {v}" for k, v in sorted(sizes.items())))
    elif args.command == "show":
        for g in db.execute("SELECT * FROM grp WHERE loader = ? ORDER BY kind, id", (args.loader,)):
            print(f"{g['kind']:<8} {g['condition'] or '-':<12} [{g['evidence']}]")
            for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
                offsets = f" page {m['page']:+d}, row {m['row']:+d}" if m["page"] or m["row"] else ""
                n = db.execute("SELECT COUNT(*) FROM footprint WHERE file = ?", (m["file"],)).fetchone()[0]
                print(f"    {m['file']}{offsets}  ({n} uploads)" + (f"  {m['note']}" if m["note"] else ""))
        # Who brings this loader's own files in: a package's carrier line is its identity, not a load.
        own = [r[0] for r in db.execute("SELECT m.file FROM member m JOIN grp g ON g.id = m.grp WHERE g.loader = ? AND g.kind = 'carrier'", (args.loader,))]
        if own:
            marks = ",".join("?" * len(own))
            rows = db.execute(f"SELECT g.loader, g.kind, g.condition, g.evidence, g.layer, m.file, m.page, m.row FROM member m JOIN grp g ON g.id = m.grp "
                              f"WHERE m.file IN ({marks}) AND g.kind != 'carrier' ORDER BY g.loader, g.condition, m.file", own).fetchall()
            print("loaded by" + ("" if rows else ": nothing in the graph loads these files"))
            for r in rows:
                offsets = f" page {r['page']:+d}, row {r['row']:+d}" if r["page"] or r["row"] else ""
                print(f"    {r['loader']:<36} {r['condition'] or '-':<12} {r['file']}{offsets}  [{r['evidence']}{', ' + r['layer'] if r['layer'] else ''}]")
    elif args.command == "at":
        seen = set()
        for g, m, f, x, y in resident(db, args.loader):
            if x <= args.x < x + f["w"] and y <= args.y < y + f["h"]:
                line = (f"{g['kind']:<8} {g['condition'] or '-':<12} {f['asset']} {f['part']}".rstrip()
                        + f"  at ({x},{y}) {f['w']}x{f['h']}" + (f" {f['bpp']}-bit" if f["bpp"] else "") + f"  from {m['file']}")
                if line not in seen:
                    seen.add(line)
                    print(line)
        if not seen:
            print(f"nothing {args.loader} loads covers ({args.x},{args.y})")
            return 1
    elif args.command == "dot":
        print("digraph residency {\n  rankdir=LR; node [shape=box, fontsize=10];")
        print(f'  "{args.loader}" [shape=ellipse];')
        for g in db.execute("SELECT * FROM grp WHERE loader = ?", (args.loader,)):
            style = "solid" if g["kind"] == "always" else "dashed"
            print(f'  g{g["id"]} [shape=point]; "{args.loader}" -> g{g["id"]} [style={style}, label="{g["condition"]}"];')
            for m in db.execute("SELECT * FROM member WHERE grp = ?", (g["id"],)):
                label = f"{m['page']:+d}/{m['row']:+d}" if m["page"] or m["row"] else ""
                print(f'  g{g["id"]} -> "{m["file"]}" [label="{label}"];')
        print("}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
