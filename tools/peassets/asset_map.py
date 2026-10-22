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

An image is paired with its palette first by its declaration - a declared file's record names the texture and
variant each of its image and palette chunks uploads (`declared_pairs`) - and otherwise by inference: a model's
faces, page and palette constants, views' sprite tables, and the draw-code lines of image_palettes.tsv, a line
naming one image applying to every image of the same bytes (`same_bytes`).

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
# The extraction whose decoded files the page shows (pictures, sounds, texts). Its packages and stored bytes are
# read from ROOT/assets/USA whatever this is.
EXTRACTED = ROOT / "assets" / "USA"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import asset_residency as residency  # noqa: E402
import asset_schema  # noqa: E402
from model import declared  # noqa: E402

TEMPLATE = Path(__file__).with_name("asset_map.html")
SHOWN = ("x", "y", "w", "h", "bpp", "clips", "parent", "required", "source", "vram", "size", "notes", "group")


def spawn_bindings(assets: dict, unresolved: list | None = None) -> dict[tuple[str, str], set[int]]:
    """(package, model) -> the stage-zero files a room loads for the entry that spawns that model.

    This is read from the game's own tables, in the packages' bytes, by the
    route gameplay takes (room_scene.py): `gAreaRecordsByStage[stage][area]` leads to
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
    from model import tmd
    import room_scene
    from room_scene import PLACEMENT, RESOURCE, TASK_DESC, VARIANT, sizeof
    sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
    from gen_overlay_configs import slot_packages
    project = room_scene.Project()
    # Where each package is loaded: an actor at its slot's address, not at its family's (configs/USA/overlays.toml).
    loads: dict[str, int] = {}
    for spec in project.manifest.values():
        if isinstance(spec, dict) and "overlays" in spec:
            slots = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
            for name, slot, _entry in slot_packages(spec):
                loads[name] = slots.get(slot.get("slot"), int(spec["load_addr"]))
    by_sha = {rec["sha1"]: key for key, rec in assets.items() if rec.get("type") in ("model", "tmd")}
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
                    src = tmd.read_source(data, load, model - load)
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


VIEW_PALETTE_AT = (0, 255)       # where a caption bundle's view palette is stored for (bundle.py)


def view_sprite_pairs(db: sqlite3.Connection, assets: dict, tree: dict, extracted: Path, findings: dict | None = None):
    """(image, (bpp, palette, row, palette index, column), user) for each column a room view's sprites read, and the
    count of what was looked at.

    Read from the package bytes (room_scene.py `Scene.sprites`): each view file has a `SpriteView`, whose sources
    name a texture page and a CLUT id. The image is whatever lands on the page's texels the sources read - the
    view file's own image, else something the room itself loads there. The palette is whatever is at the CLUT's
    place while that view is shown:

    - slot (view file - 1) of the room's caption bundle, which gameplay uploads on every change of view when the
      slot is an image resource (`_loadingUploadCurrentViewImage`, src/gameplay/view_image.c:45-60). It is uploaded
      once the CD queue is idle, so after the view's file, which is queued just before
      (`loadingBeginViewLoadTask`, src/gameplay/view_load.c:86-90; view_image.c:70-72): where the file carries a
      palette for the same place, the slot's lands over it;
    - else a palette the view's own file carries for that place;
    - else, when no view of the room uploads anything there, the one palette the room's own loads leave there.

    A view with neither of the first two, in a room where other views upload to that place, is drawn with what
    the last view left: nothing settles it, so it is left out and listed.
    """
    import bundle as bundles
    import room_scene
    project = room_scene.Project()
    prints = defaultdict(list)
    for r in db.execute("SELECT * FROM footprint"):
        prints[r["file"]].append(tuple(r)[1:])
    loads = defaultdict(list)                   # room -> (condition, file, page, row, mode) of what it loads besides its views
    for m in db.execute("SELECT g.loader, g.kind, g.condition, g.evidence, m.file, m.page, m.row, m.mode FROM member m JOIN grp g ON g.id = m.grp "
                        "WHERE g.evidence NOT IN ('viewmap', 'folder')"):
        loads[m["loader"]].append((m["condition"] or ("the room's base file" if m["kind"] == "carrier" else ""), m["file"], m["page"], m["row"], m["mode"] or ""))
    packages = asset_schema.package_names(assets)      # package asset -> overlay name
    out, found_by = [], findings if findings is not None else defaultdict(list)
    seen = defaultdict(int)

    def miss(room, text):
        if text not in found_by[room]:           # one view after another can say the same thing
            found_by[room].append(text)

    def landed(path, at):
        """(asset, kind, x, y, w, h, column, bpp) of each rectangle of a file, where it lands; an image by column."""
        found = []
        for rect in prints.get(path, ()):
            asset, kind, part, _x, _y, w, h, bpp = rect
            if kind == "clut" or path.startswith("code:"):
                found.append((asset, kind, *residency.moved(rect, *at), w, h, 0, bpp))
                continue
            for n, (x, y, depth) in enumerate(asset_schema.image_columns(assets[asset])):
                name = f"column {n}" if n else "column 0"
                x, y = residency.moved((asset, kind, name, x, y, w, h, depth), *at)
                found.append((asset, kind, x, y, asset_schema.IMAGE_COLUMN_WIDTH, h, n, depth))
        return list(dict.fromkeys(found))       # a file can carry one chunk twice

    for stage, body in tree.items():
        for folder, fnode in (body.get("folders") or {}).items():
            chunks = [c for n in fnode["files"].values() for c in (n.get("chunks") or {}).values()]
            room = next((packages[c] for c in chunks if c in packages), None)
            if room is None or room not in project.place_of:
                continue
            slots = {}
            for key in chunks:
                raw = extracted / "raw" / "pe2bundle" / f"{key}.pe2bundle"
                if assets.get(key, {}).get("type") == "pe2bundle" and raw.is_file():
                    for slot in bundles.slots(raw.read_bytes()):
                        if slot["what"] == "palette":
                            slots[slot["slot"]] = (bundles.name_of(key, slot['slot']), "clut", slot["x"], slot["y"], slot["w"], slot["h"], 0, None)
            by_id = {int(n["id"]): f"{stage}/{folder}/{k}" for k, n in fnode["files"].items()}
            views = room_scene.Scene(project, room).sprites()["views"]
            around = [(condition, r) for condition, path, page, row, mode in loads.get(room, ()) for r in landed(path, (page, row, mode))]
            has = lambda r, x, y, w=1: r[2] <= x and x + w <= r[2] + r[4] and r[3] <= y < r[3] + r[5]  # noqa: E731
            # The places some view of the room uploads a palette to: there, a view that uploads none shows the last one's.
            uploaded = [r for v in views for r in [slots.get(v["file"] - 1)] + [q for q in landed(by_id.get(v["file"], ""), (0, 0, "")) if q[1] == "clut"] if r]
            for view in views:
                if not view["pages"]:
                    continue
                seen["views"] += 1
                own = landed(by_id.get(view["file"], ""), (0, 0, ""))
                where = f"{room} view file {view['file']}"
                user = f"{room}: view file {view['file']} sprites"
                texels = defaultdict(dict)          # (tpage, clut) -> the video-memory columns its drawn rectangles read, and their rows
                drawn = {n for b in view["batchList"] for n in range(b["firstSprite"], b["firstSprite"] + b["spriteCount"])}
                for n, row in enumerate(view["rows"]):
                    if n in drawn:
                        tpage, clut, w, h, _x0, _y0, _depth, u, v = row[:9]
                        per = 16 // room_scene.TPAGE_DEPTH[(tpage >> 7) & 3]         # texels in a video-memory word
                        for x in range(u // per // 64, (u + max(w, 1) - 1) // per // 64 + 1):
                            lo, hi = texels[tpage, clut].get(x, (255, 0))
                            texels[tpage, clut][x] = (min(lo, v), max(hi, v + max(h, 1) - 1))
                for page in view["pages"]:
                    seen["pages"] += 1
                    bpp, cx, cy = page["bpp"], page["clutX"], page["clutY"]
                    spot = f"page ({page['pageX']}, {page['pageY']}) {bpp}-bit, CLUT 0x{page['clut']:04X} at ({cx}, {cy})"
                    colours = 256 if bpp == 8 else 16
                    palette = None
                    if bpp != 16:
                        slot = slots.get(view["file"] - 1)
                        slot = slot if slot and has(slot, cx, cy, colours) else None
                        mine = [r for r in own if r[1] == "clut" and has(r, cx, cy, colours)]
                        rest = {r[0]: (condition, r) for condition, r in around if r[1] == "clut" and has(r, cx, cy, colours)}
                        if slot or mine:
                            palette, source = (slot or mine[0]), ("its bundle slot" if slot else "the view's file")
                        elif any(has(r, cx, cy, colours) for r in uploaded):
                            miss(room, f"{where}: {spot} is uploaded by neither its bundle slot nor its file; other views of the room upload there, so it shows what the last one left")
                        elif len(rest) == 1:
                            (condition, palette), = rest.values()
                            source = f"the room's load ({condition})"
                        elif rest:
                            miss(room, f"{where}: {spot} is where {len(rest)} of the room's loads put a palette ({', '.join(sorted(rest))})")
                        else:
                            miss(room, f"{where}: {spot} is uploaded by nothing of the room: not its bundle slot, its file, or a load of the room")
                        if palette is None:
                            seen["pages without a palette"] += 1
                            continue
                    for column, (top, bottom) in sorted(texels[page["tpage"], page["clut"]].items()):
                        x, y = page["pageX"] + column * 64, page["pageY"] + top
                        images = [r for r in own if r[1] == "image" and has(r, x, y) and has(r, x, page["pageY"] + bottom)]
                        via = "its file"
                        if not images:
                            found = {(r[0], r[6]): (condition, r) for condition, r in around if r[1] == "image" and has(r, x, y)}
                            if len(found) == 1:
                                (condition, image), = found.values()
                                images, via = [image], f"the room's load ({condition})"
                            elif found:
                                miss(room, f"{where}: {spot}: column ({x}, {page['pageY']}) is where {len(found)} of the room's loads put an image ({', '.join(sorted(k[0] for k in found))})")
                        if len(images) != 1:
                            if not images:
                                miss(room, f"{where}: {spot}: no image of the view's file or of the room's loads is at column ({x}, {page['pageY']}), row {top}")
                            else:
                                miss(room, f"{where}: {spot}: {len(images)} images of the view's file are at column ({x}, {page['pageY']})")
                            seen["columns without an image"] += 1
                            continue
                        image = images[0]
                        if image[7] and image[7] != bpp:
                            miss(room, f"{image[0]} column {image[6]} is cataloged as {image[7]}-bit, and the sprites of view file {view['file']} read it {bpp}-bit")
                        if bpp == 16:
                            key = (16, "", 0, 0, image[6])
                        else:
                            key = (bpp, palette[0], cy - palette[3], (cx - palette[2]) // 16, image[6])
                        note = "the room's sprite table (SpriteSource page and CLUT, package bytes)" + ("" if via == "its file" else f"; image from {via}") + ("" if bpp == 16 else f"; palette from {source}")
                        out.append((image[0], key, user, note))
                        seen["columns paired"] += 1
    return out, dict(seen)


STATED = Path(__file__).with_name("image_palettes.tsv")


def stated_pairs(assets: dict, embedded: dict, path: Path = STATED):
    """((image, (bpp, palette, row, palette index, column), user, how known), problems) from image_palettes.tsv.

    The file records what was read out of draw code: image, column, depth, palette asset (empty for direct
    colour), row, palette index, user, evidence. Nothing is taken on trust: the image and the palette must be
    cataloged, the column must be one the image has, and the row and index must lie inside the palette.
    """
    out, problems = [], []
    if not path.is_file():
        return out, problems
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        cells = [c.strip() for c in line.split("\t")]
        if not line.strip() or line.startswith("#"):
            continue
        if len(cells) < 8:
            problems.append(f"line {n}: {len(cells)} fields, 8 expected")
            continue
        image, column, depth, palette, row, index, user, evidence = cells[:8]
        rec = assets.get(image) or embedded.get(image)
        try:
            column, depth, row, index = int(column), int(depth), int(row or 0), int(index or 0)
        except ValueError:
            problems.append(f"line {n}: column, depth, row and palette index are numbers")
            continue
        if rec is None or rec.get("type") not in (*asset_schema.IMAGE_TYPES, "image"):
            problems.append(f"line {n}: {image} is not a cataloged image")
        elif rec.get("type") in asset_schema.IMAGE_TYPES and not 0 <= column < len(asset_schema.image_columns(rec)):
            problems.append(f"line {n}: {image} has no column {column}")
        elif depth not in (4, 8, 16) or (depth == 16) != (not palette):
            problems.append(f"line {n}: a palette goes with 4 or 8 bits, none with 16")
        elif palette and (assets.get(palette) or embedded.get(palette) or {}).get("type") not in ("pe2clut", "clut"):
            problems.append(f"line {n}: {palette} is not a cataloged palette")
        elif palette and not (0 <= row < (assets.get(palette) or embedded[palette])["h"]
                              and 0 <= index * 16 + (256 if depth == 8 else 16) <= (assets.get(palette) or embedded[palette])["w"]):
            problems.append(f"line {n}: row {row}, palette {index} is outside {palette}")
        elif not evidence:
            problems.append(f"line {n}: no evidence")
        else:
            how = ("direct colour, " if not palette else "") + f"draw code: {evidence}"
            # Every image holding the same bytes is drawn the same way: a line names one of them (the canonical
            # name, asset_schema.canonical_names), and each other takes its own group's copy of the palette.
            for name in same_bytes(image):
                own = next((n for n in same_bytes(palette) if (assets.get(n) or {}).get("group") == (assets.get(name) or {}).get("group")),
                           palette) if palette else palette
                out.append((name, (depth, own, row, index, column), user, how))
    return out, problems


def declared_pairs(assets: dict):
    """(image, (bpp, palette, row, palette index, column), user, how known) of every declared texture variant: the
    image chunk whose page uploads it, at that page's column, and the palette chunk that uploads the variant's
    palette, at the row and the palette index it is placed at."""
    from texture import files
    from texture.declaration import place_at
    out = []
    for name, rec in sorted(assets.items()):
        if rec.get("type") != "file":
            continue
        # An atlas's sprites are textures of its page (the UI sheet's), each drawn with its own palettes.
        pages = [(e["chunk"], n, placed) for e in rec["contents"] for n, p in enumerate(e.get("pages", ()))
                 for placed in ([p] if "texture" in p else assets[p["atlas"]]["sprites"] if "atlas" in p else [])]
        palettes = [(e["chunk"], p) for e in rec["contents"] for p in e.get("palettes", ())]
        for image, column, page in pages:
            texture = assets[page["texture"]]
            for variant in texture["variants"]:
                found = [(chunk, p) for chunk, p in palettes if p.get("texture") == page["texture"] and p.get("variant") == variant]
                if not found:
                    continue
                chunk, palette = found[0]
                x, y = place_at(palette, assets)
                clut = assets[chunk]
                at = (texture["bpp"], chunk, y - clut["y"], (x - clut["x"]) // 16, column)
                out.append((image, at, f"declared texture {page['texture']}" + ("" if variant == "default" else f".{variant}"),
                            f"declared: the file record {name} uploads this page and palette for one texture",
                            files.variant_path(Path(), assets, page["texture"], variant).as_posix()))
    return out


_SAME_BYTES: dict[int, dict[str, list[str]]] = {}


def same_bytes(name: str, assets: dict | None = None) -> list[str]:
    """An asset and every other asset holding the same stored bytes, the asset first."""
    if assets is None:
        assets = asset_schema.load()[0]
    index = _SAME_BYTES.get(id(assets))
    if index is None:
        index = {}
        for key, rec in assets.items():
            if isinstance(rec.get("sha1"), str):
                index.setdefault(rec["sha1"], []).append(key)
        _SAME_BYTES[id(assets)] = index
    sha1 = (assets.get(name) or {}).get("sha1")
    return [name] + [n for n in index.get(sha1, []) if n != name] if sha1 else [name]


RAW_DIR = {"motion_curve": ("motion", ".motion"), "sprite_schedule": ("motion", ".motion"), "camera_path": ("path", ".path"),
           "movie_path": ("path", ".path"), "morph_deltas": ("morph", ".morph"), "morph_normals": ("morph", ".morph"),
           "fontglyph": ("fontglyph", ".fontglyph")}
UNIT = {"u8": 1, "u16": 2, "u32": 4}


def write_side_data(folder: Path, assets: dict, embedded: dict) -> dict:
    """Meshes and sampled tracks, one script each beside the page, like the pixels; returns what was found.

    A mesh is its vertices in the rest pose and its faces. A track is the raw
    numbers of an embedded curve or path, in the unit the build includes it
    with: enough to plot, not a decoding of what each number means.
    """
    import base64
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from model import mesh, tmd
    folder.mkdir(parents=True, exist_ok=True)
    have = {"mesh": [], "track": {}, "holds": defaultdict(list)}
    for package, (load, sources) in sorted(tmd.declared_records(ROOT).items()):
        path = ROOT / "assets" / "USA" / "pe2pkg" / f"{package}.pe2pkg"
        if not path.is_file():
            continue
        data = path.read_bytes()
        for offset in sorted(sources):
            try:
                src = tmd.read_source(data, load, offset)
                key = tmd.catalog_name(data, load, offset)    # by the whole model's sha1, else the stream's
                if key is not None and key not in have["holds"][package]:
                    have["holds"][package].append(key)       # a mesh shared with another package is this one's too
                if key is None or key in have["mesh"]:
                    continue
                # Faces, their textures ([page, CLUT] pair, then (u, v) per corner) and parts: model/mesh.py.
                model, placement = mesh.from_source(data, load, offset)
                verts = mesh.posed(model)
                faces, textures, named, parts = mesh.faces_of(model, placement)
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
            target = folder / f"{key}.js"               # a grouped asset's name is a path
            target.parent.mkdir(parents=True, exist_ok=True)
            packed = lambda value: json.dumps(value, separators=(",", ":"))  # noqa: E731
            target.write_text(f"MESH({json.dumps(key)},{packed(flat)},{packed(faces)},{packed(textures)},{packed(named)},{packed(parts)},{packed(bones)});\n", encoding="utf-8")
            have["mesh"].append(key)
    raw = ROOT / "assets" / "USA" / "raw"
    for key, rec in embedded.items():
        where = RAW_DIR.get(rec.get("type"))
        source = raw / where[0] / f"{key}{where[1]}" if where else None
        if source is not None and not source.is_file():
            try:                                    # a name sharing its bytes is stored under another (raw_rel)
                source = raw.parent / asset_schema.raw_rel(key, rec)
            except KeyError:
                pass
        if source is None or not source.is_file():
            continue
        unit = UNIT.get(rec.get("include") if isinstance(rec.get("include"), str) else "", 2)
        (folder / f"{key}.js").parent.mkdir(parents=True, exist_ok=True)
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


def write_traces(folder: Path) -> list:
    """One script per game trace (local/gametrace), replayed by trace_replay.py: the playback's data, loaded when a
    session is opened. Returns [trace, [[session, number, started, dropped, first record, last record], ...]] each."""
    import trace_replay
    paths = trace_replay.all_traces()
    for old in folder.glob("trace_*.js") if folder.is_dir() else ():       # a trace that is gone leaves no playback behind
        if old.stem.removeprefix("trace_") not in {p.stem for p in paths}:
            old.unlink()
    if not paths:
        return []
    folder.mkdir(parents=True, exist_ok=True)
    world, listed = trace_replay.World(), []
    for path in paths:
        try:
            data = trace_replay.Replay(path, world).page_data()
        except Exception as error:  # noqa: BLE001 - a trace that does not replay must not stop the page
            print(f"trace {path.name}: {type(error).__name__}: {error}", file=sys.stderr)
            continue
        (folder / f"trace_{path.stem}.js").write_text(f"TRACE({json.dumps(path.stem)},{json.dumps(data, separators=(',', ':'))});\n", encoding="utf-8")
        listed.append([path.stem, data["sessions"]])
    return listed


def observed_residency() -> dict:
    """asset -> what the traces showed of it (tools/peassets/observed.tsv, written by trace_replay.py observe)."""
    import trace_replay
    out = defaultdict(list)
    for row in trace_replay.read_observed():
        out[row[0]].append(list(row[1:]))
    return out


def animation_tables(embedded: dict) -> dict:
    """Per animation bank: each set's record and track counts, and the tables of the package that index it."""
    import tomllib
    import asset_journal
    import pkg_anim
    sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
    from gen_overlay_configs import declared_animations
    rows = declared_animations(tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8")))
    loads = {row["package"] + ".pe2pkg": row["load"] for row in rows}
    banks = {(rec["source"], asset_schema.embedded_offset(rec, loads.get(rec["source"]))): key for key, rec in embedded.items()
             if rec.get("type") == "animation" and ("offset" in rec or rec["source"] in loads)}
    tables = asset_journal.set_tables(embedded)
    out = {}
    blobs: dict[str, bytes] = {}
    for row in rows:
        key = banks.get((row["package"] + ".pe2pkg", row["offset"]))
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
    # The files view lists folders and files by their disc ids, the order the disc has them in.
    file_ids = {r["path"]: r["id"] for r in db.execute("SELECT path, id FROM file")}
    folder_ids = {f"{stage}/{key}": int(node["id"]) for stage, body in tree.items()
                  for key, node in (body.get("folders") or {}).items()}
    index = {path: n for n, path in enumerate(files)}
    packages = asset_schema.package_names(assets)      # package asset -> overlay name

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
    # How each pairing is known: a model's faces, constants named alike, a view's sprite table, a draw call.
    known: dict[str, dict[tuple, set]] = defaultdict(lambda: defaultdict(set))
    findings = defaultdict(list)
    for key, rec in sorted(embedded.items()):
        package = rec.get("source", "").removesuffix(".pe2pkg")
        if rec.get("type") not in ("model", "tmd") or package not in supply.carrier:
            continue
        bounds = residency.model_bounds(package, rec)
        if rec["type"] == "model":          # its binding names the pages, without the blend rate
            texture_pairs = declared.texture_pairs(key, assets)
            masked: dict = {}
            for (p, c), (u0, v0, u1, v1) in bounds.items():
                box = masked.setdefault((p & ~0x60, c), [u0, v0, u1, v1])
                box[:] = [min(box[0], u0), min(box[1], v0), max(box[2], u1), max(box[3], v1)]
            bounds = masked
        else:
            texture_pairs = assets.get(key, {}).get("textures", ())
        for page, clut in texture_pairs:
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
                    at = (bpp, palette[5], cy - palette[2], (cx - palette[1]) // 16, column) if palette else (bpp, "", 0, 0, column)
                    pairs[image[5]][at].add(key)
                    known[image[5]][at].add("a model's faces name this page and palette")
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
                            at = (bpp, palette[5] if palette else "", (cy - palette[2]) if palette else 0, ((cx - palette[1]) // 16) if palette else 0, column)
                            pairs[image[5]][at].add(f"{page_name} + {clut_name}")
                            known[image[5]][at].add("a page constant and a palette constant of one scope in the source")
                        constants[package].append([label, page, clut, how])
    else:
        print(f"no journal at {journal}: constants are left out", file=sys.stderr)

    # What a room view's sprites read, from the rooms' sprite tables.
    sprite_pairs, sprite_counts = view_sprite_pairs(db, assets, tree, EXTRACTED, findings)
    for image, at, user, how in sprite_pairs:
        pairs[image][at].add(user)
        known[image][at].add(how)
    # What a draw call reads, as read out of the code (tools/peassets/image_palettes.tsv).
    # A declared file states its pairings outright: each image chunk's pages and each palette chunk's palettes name
    # the texture and variant they upload for (texture/declaration.py). They take precedence over every inference.
    declared_images = set()
    texture_pictures: dict[str, str] = {}      # "<image>|<bpp>|<palette>|<row>|<index>|<column>" -> its decoded PNG
    for image, at, user, how, png in declared_pairs(assets):
        pairs[image][at].add(user)
        known[image][at].add(how)
        declared_images.add(image)
        texture_pictures["|".join(map(str, (image, *at)))] = png
    stated, stated_problems = stated_pairs(assets, embedded)
    for image, at, user, how in stated:
        if image in declared_images:
            continue
        pairs[image][at].add(user)
        known[image][at].add(how)
    for line in stated_problems:
        print(f"image_palettes.tsv: {line}", file=sys.stderr)
    paired = {k for k in pairs if assets.get(k, {}).get("type") in asset_schema.IMAGE_TYPES}
    print(f"images shown as used: {len(paired)} of {sum(1 for r in assets.values() if r['type'] in asset_schema.IMAGE_TYPES)} "
          f"({len(declared_images)} declared by their files, {sprite_counts.get('columns paired', 0)} columns from "
          f"{sprite_counts.get('views', 0)} views' sprite tables, {len(stated)} lines from image_palettes.tsv)", file=sys.stderr)

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
    sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
    from gen_overlay_configs import slot_packages
    address = {}
    for _family, spec in tomllib.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8")).items():
        if isinstance(spec, dict) and "overlays" in spec:
            own = {int(k): int(v) for k, v in (spec.get("slots") or {}).items()}
            for name, slot, _entry in slot_packages(spec):
                address[name] = own.get(slot.get("slot"), int(spec["load_addr"]))

    overlays = {asset_schema.ROOT_OVERLAY: {"family": "the executable", "parents": [], "own": [index[p] for p in supply.own(asset_schema.ROOT_OVERLAY)], "room": 0}}
    for package in sorted(packages.values()):
        if package not in supply.carrier:
            continue
        first = supply.files[supply.carrier[package][0]]
        overlays[package] = {
            "family": "rooms" if first["stage"] else residency.family_of(first["path"], first["stage"], first["id"]),
            "parents": asset_schema.parents_of(package, assets, tree), "own": [index[p] for p in supply.own(package)],
            "room": int(bool(first["stage"])), "stage": first["stage"] or 0, "addr": address.get(package),
            "carrier": [index[p] for p in supply.carrier[package]]}

    extracted = EXTRACTED
    side = write_side_data(out.with_name(out.stem + "_data"), assets, embedded) if pixels else {"mesh": [], "track": {}, "holds": {}}
    try:
        banks = animation_tables(embedded)
    except Exception as exc:  # noqa: BLE001
        print(f"no animation tables: {exc}", file=sys.stderr)
        banks = {}
    sounds, sample_ids, sample_homes = {}, {}, {}
    # Where the extraction filed each asset is asset_schema's to say: under its group, else under its type's directory.
    # The page builds `<type>/<name>` itself, so it is told only of the assets that are somewhere else - and only of
    # those the extraction has in fact put there, since a group given since the last extraction or regroup.py run
    # has moved nothing yet.
    dirs, unmoved = {}, 0
    for records in (assets, embedded):
        for key, rec in records.items():
            group = asset_schema.url_dir(key, rec)
            if group:
                here = (extracted / asset_schema.decoded_rel(key, rec)).exists()
                dirs.update({key: group} if here else {})
                unmoved += not here and any((extracted / f).exists() for f in asset_schema.decoded_files(key, rec, where=asset_schema.type_dir_of(rec)))
    if unmoved:
        print(f"{unmoved} grouped assets are still under their type's directory in {extracted}: run tools/peassets/regroup.py", file=sys.stderr)
    bank_folders = [p for p in (extracted / "spk").iterdir()] if (extracted / "spk").is_dir() else []
    bank_folders += [extracted / dirs[key] / key for key, rec in assets.items() if rec.get("type") == "spk" and key in dirs]
    for folder in bank_folders:
        if folder.is_dir():
            sounds[folder.name] = len((assets.get(folder.name) or {}).get("samples") or ()) or sum(1 for f in folder.iterdir() if f.suffix == ".wav")
    # A bank is a container of samples (tools/peassets/asset_data.py): each slot names one, and one sample can sit in many banks.
    for key, rec in assets.items():
        if rec.get("type") == "spk" and rec.get("samples"):
            sample_ids[key] = rec["samples"]
            for n, name in enumerate(rec["samples"]):
                sample_homes.setdefault(name, []).append([key, n])
    catalog = {}
    for key, rec in assets.items():
        catalog[key] = [rec["type"], {k: rec[k] for k in SHOWN if k in rec}]
    for key, rec in embedded.items():
        if key not in catalog:
            catalog[key] = [rec["type"], {k: rec[k] for k in SHOWN if k in rec}]
        else:
            catalog[key][1].update({k: rec[k] for k in ("source", "vram", "size") if k in rec})
    # A room's caption bundle holds a palette per view, in the slot before that view's number, besides the dialogue.
    import base64
    import bundle as bundles
    import cap_dialogue
    import spk_scripts
    dialogue = {}
    viewpal = {}
    for stage, folder, fkey, node in asset_schema.tree_files(tree):
        if folder is None:
            continue
        room = next((c for c in (node.get("chunks") or {}).values() if c in overlays), None)
        for key in (node.get("chunks") or {}).values():
            if assets.get(key, {}).get("type") != "pe2bundle" or room is None:
                continue
            raw = extracted / "raw" / "pe2bundle" / f"{key}.pe2bundle"
            if not raw.is_file():
                continue
            data = raw.read_bytes()
            for slot in bundles.slots(data):
                name = bundles.name_of(key, slot['slot'])
                if slot["what"] == "palette":
                    # Only a palette for (0, 255) is a view's; one stored elsewhere is something the room's code uploads itself.
                    of_view = (slot["x"], slot["y"]) == (0, 255)
                    note = (f"the palette of the view mapped to file {slot['slot'] + 1} of {room}" if of_view
                            else f"a palette in the bundle {room} loads, for ({slot['x']}, {slot['y']}); not a view's, so some code uploads it")
                    catalog[name] = ["clut", {"source": key, "x": slot["x"], "y": slot["y"], "w": slot["w"], "h": slot["h"], "notes": note}]
                    files.append(f"bundle:{name}")
                    footprint[len(files) - 1].append([name, "c", "", slot["x"], slot["y"], slot["w"], slot["h"], 0])
                    # The slot is the mapped view less one (src/gameplay/view_image.c).
                    if of_view:
                        viewpal.setdefault(room, {})[slot["slot"] + 1] = len(files) - 1
                    if pixels:
                        (out.with_name(pixels) / f"{name}.js").write_text(
                            f"PIX({json.dumps(name)},{json.dumps([[slot['x'], slot['y'], slot['w'], slot['h'], base64.b64encode(bundles.palette_words(data, slot)).decode()]])});\n", encoding="utf-8")
                else:
                    catalog[name] = [slot["what"], {"source": key, "size": slot["length"], "vram": slot["dest"], "notes": f"slot {slot['slot']} of the bundle {room} loads"}]
                    if slot["what"] == "dialogue":
                        found = cap_dialogue.commands(data[slot["offset"]:slot["offset"] + slot["length"]])
                        dialogue[name] = [line for command in found for line in cap_dialogue.describe(command)]
    # A scene's audio stream carries scene images between its audio chunks, each with up to three palettes that
    # `_mdecStepSceneImageDecode` uploads when the view it is for arrives (trace_replay.scene_images reads them from
    # the stream's bytes). A trace's playback names one as the file `scene:<name>`, as it names a bundle's palette.
    import trace_replay
    for stem, image, chunk in trace_replay.all_scene_palettes():
        name = trace_replay.scene_palette_name(stem, image, chunk)
        catalog[name] = ["clut", {"source": stem, "x": chunk["x"], "y": chunk["y"], "w": chunk["w"], "h": chunk["h"],
                                    "notes": f"image chunk {chunk['k']} of the scene image for view {image['view']} in the stream {stem} (sector {image['sector']}): uploaded by the scene image decoder when that view arrives"
                                             + (", relocated: three rows up and one down" if chunk["relocated"] else "")}]
        files.append(f"scene:{name}")
        footprint[len(files) - 1].append([name, "c", "", chunk["x"], chunk["y"], chunk["w"], chunk["h"], 0])
        if pixels:
            (out.with_name(pixels) / f"{name}.js").write_text(
                f"PIX({json.dumps(name)},{json.dumps([[chunk['x'], chunk['y'], chunk['w'], chunk['h'], base64.b64encode(trace_replay.scene_palette_words(chunk)).decode()]])});\n", encoding="utf-8")
    # A room's collision grid is an asset too: the overlay manifest declares where it sits in the package.
    import tomllib as _toml
    from gen_overlay_configs import declared_collisions
    for grid in declared_collisions(_toml.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))):
        catalog[f"{grid['package']}_collision_{grid['grid']:05X}"] = ["collision", {
            "source": grid["package"] + ".pe2pkg", "vram": grid["load"] + grid["grid"],
            "notes": "the room's collision grid: normals, vertices, faces and per-cell face lists; its header follows at "
                     f"0x{grid['load'] + grid['source']:08X}"}]
    # A collision patch has no header: its pieces are arrays some code copies into a room's live grid.
    from gen_overlay_configs import declared_collision_patches
    for patch in declared_collision_patches(_toml.loads((ROOT / "configs/USA/overlays.toml").read_text(encoding="utf-8"))):
        catalog[f"{patch['package']}_collision_{patch['offset']:05X}"] = ["collision", {
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
            # An editor that strips trailing white space drops an empty note, and with it the line's last tab.
            if len(cells) == 7 and cells[6] in ("established", "open"):
                cells.append("")
            if len(cells) < 8:
                continue
            # The asset field can name several assets, and ranges of them ("bs_41, bs_43..bs_49", or
            # "room_view3_background..room_view9_background", the number between the same words): the use is each one's.
            named = []
            for token in re.findall(r"[A-Za-z0-9_]+(?:\.\.[A-Za-z0-9_]+)?", cells[0]):
                first, _, last = token.partition("..")
                a, b = re.fullmatch(r"(.*?)(\d+)(\D*)", first), re.fullmatch(r"(.*?)(\d+)(\D*)", last) if last else None
                same = a and b and a[1] == b[1] and a[3] == b[3] and 0 <= int(b[2]) - int(a[2]) < 500
                span = [f"{a[1]}{n}{a[3]}" for n in range(int(a[2]), int(b[2]) + 1)] if same else [first]
                named += [k for k in span if k in catalog and k not in named]
            for key in named or [cells[0]]:
                usage[key].append([path.stem] + cells[1:8])
            by_overlay[path.stem][0 if cells[6] == "established" else 1] += 1
    # Movies and streamed audio are not chunks of the stage files; the extraction lists them.
    streams = {}
    movies = extracted / "movie" / "movies.json"
    if movies.is_file():
        for m in json.loads(movies.read_text(encoding="utf-8")):
            catalog[m["stem"]] = ["movie", {"w": m.get("width"), "h": m.get("height"), "frames": m.get("frame_count"), "fps": m.get("fps"),
                                              "seconds": m.get("audio_duration_sec"), "container": m.get("container"), "disk": m.get("disk")}]
            streams[m["stem"]] = m.get("mp4", "")
    if (extracted / "audio").is_dir():
        for meta in sorted((extracted / "audio").glob("*.json")):
            info = json.loads(meta.read_text(encoding="utf-8"))
            if meta.with_suffix(".wav").is_file():
                catalog[meta.stem] = ["stream", {"seconds": info.get("duration_sec"), "rate": info.get("sample_rate"), "channels": info.get("channels")}]
                streams[meta.stem] = f"audio/{meta.stem}.wav"
    save_dir = extracted / dirs.get("mc_save_header", "mcsave") / "mc_save_header"
    saves = sorted(p.name for p in save_dir.glob("*.png")) if save_dir.is_dir() else []
    return {
        "traces": write_traces(out.with_name(out.stem + "_data")) if pixels else [], "observed": observed_residency(),
        "holds": side["holds"], "viewpal": viewpal, "scripts": scripts, "scenes": scenes, "names": names, "usage": usage, "swept": by_overlay, "remarks": remarks, "notes": notes, "dialogue": dialogue,
        "mesh": side["mesh"], "track": side["track"], "banks": banks, "streams": streams, "saves": saves, "side": out.stem + "_data" if pixels else "",
        "texturePictures": texture_pictures, "files": files, "fileIds": [file_ids.get(f) for f in files], "folderIds": folder_ids, "chunks": chunks, "fp": footprint, "loads": loads, "overlays": overlays, "requests": requests,
        "consts": constants, "findings": findings, "assets": catalog, "sounds": sounds, "sampleIds": sample_ids, "sampleHomes": sample_homes,
        "pairs": {image: [[bpp, clut, row, pal, column, sorted(users)[:8], sorted(known[image][bpp, clut, row, pal, column])]
                          for (bpp, clut, row, pal, column), users in sorted(found.items(), key=str)] for image, found in pairs.items()},
        "regions": [[r["kind"][0], r["name"], r["x"], r["y"], r["w"], r["h"], r["note"]] for r in table], "layers": residency.LAYERS,
        "stagemap": {r["id"] - 900000: index[r["path"]] for r in db.execute("SELECT path, id FROM file WHERE stage = 0 AND id BETWEEN 900001 AND 900005")},
        "pictures": os.path.relpath(extracted.resolve(), out.parent.resolve()) if (extracted / "pe2img").is_dir() or dirs else "", "pixels": pixels,
        "dirs": dirs,
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
                if kind not in ("pe2pkg", "room", "stage") and key not in seen:
                    seen.add(key)
                    lines.append(f"{key}\t{kind}\t{where}")
            for f in o["own"]:
                for chunk, key in data["chunks"].get(f, []):
                    add(key, f"in file {data['files'][f]} as {chunk}")
                    if data["assets"].get(key, [""])[0] == "pe2bundle":
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
