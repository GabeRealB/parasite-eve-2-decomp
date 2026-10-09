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
import json
import os
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


def spawn_bindings(assets: dict) -> dict[tuple[str, str], set[int]]:
    """(package, model) -> the stage-zero files a room loads for the entry that spawns that model.

    This is read from the game's own tables, not inferred. A room's
    `AreaResource` row names a file (`fileNumber`, `fileGroupIndex`) and the
    actor it is for: `taskTable[taskIndex]`, a `TaskDesc` whose `data.model`
    is the model's `TmdSource` when its body kind is a TMD model
    (include/gameplay/area.h, include/main/task_types.h). So the row says
    which file's textures the model spawned through it is drawn with.
    """
    import hashlib
    import re
    import struct
    import asset_journal
    import pkg_model
    row = re.compile(r"\{\s*([\w]+)\s*,\s*([-\w]+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*\{[^{}]*\}\s*,\s*&?(\w+)\s*\}")
    table = re.compile(r"\bAreaResource\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", re.S)
    named: dict[str, list[tuple[str, int]]] = defaultdict(list)          # symbol -> (package, address)
    for sym in (ROOT / "configs/USA/sym").glob("*/*.txt"):
        for m in residency.asset_journal.SYMBOL.finditer(sym.read_text(encoding="utf-8")):
            named[m[1]].append((sym.stem, int(m[2], 16)))
    records = pkg_model.declared_records(ROOT)
    by_sha = {rec["sha1"]: key for key, rec in assets.items() if rec.get("type") == "model"}
    blobs: dict[str, bytes] = {}
    values: dict[str, set[int]] = defaultdict(set)
    sources = sorted(p for p in (ROOT / "src").rglob("*.c"))
    for path in sources:
        if "AreaResource" in path.read_text(encoding="utf-8", errors="replace"):
            for _scope, symbol, _line, _kind, _expr, value in asset_journal.constants(path):
                values[symbol].add(value)
    for header in (ROOT / "include" / "gameplay").glob("*.h"):
        for _scope, symbol, _line, _kind, _expr, value in asset_journal.constants(header):
            values[symbol].add(value)
    known = {name: next(iter(v)) for name, v in values.items() if len(v) == 1}
    # Which further file of an entry its placements ask for (`AreaPlacement.fileIdLow`): a placement with 0 uses
    # the entry's base file, one with N the file N beside it. So the entry's models are drawn with those files.
    placements, _resources, variants = residency.area_tables(sources, known)
    lows: dict[tuple[str, int], set[int]] = defaultdict(set)
    for _rel, pairs in variants.values():
        for placed, resource_table in pairs:
            for entry, low, _page, _row in placements.get(placed, ()):
                lows[resource_table, entry].add(low)
    out: dict[tuple[str, str], set[int]] = defaultdict(set)
    for path in sources:
        text = path.read_text(encoding="utf-8", errors="replace")
        if "AreaResource" not in text:
            continue
        for found in table.finditer(asset_journal.code_of(path)):
            for m in row.finditer(found[2]):
                entry, number, group, index = (asset_journal.evaluate(c, known) for c in (m[1], m[2], m[3], m[4]))
                if None in (entry, number, group, index) or not 0 <= group < len(residency.GROUP_BASE):
                    continue
                file_id = (residency.GROUP_BASE[group] + number // 100) * 10000 + (number % 100) * 100
                owners = named.get(m[5], [])
                direct = re.fullmatch(r"D_([a-z0-9_]+?)_([0-9A-F]{8})", m[5])
                if direct and not owners:
                    owners = [(direct[1], int(direct[2], 16))]
                for package, address in owners:
                    if package not in records:
                        continue
                    load, _sources = records[package]
                    if package not in blobs:
                        blob = ROOT / "assets" / "USA" / "pe2pkg" / f"{package}.pe2pkg"
                        blobs[package] = blob.read_bytes() if blob.is_file() else b""
                    data, at = blobs[package], address - load + index * 12
                    if not 0 <= at <= len(data) - 12:
                        continue
                    flags, _priority, _callback, model = struct.unpack_from("<HHII", data, at)
                    if flags & 0xFF != 1 or not load <= model < load + len(data):
                        continue                      # not a TMD body: the descriptor names no model
                    try:
                        src = pkg_model.read_source(data, load, model - load)
                        key = by_sha.get(hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest())
                    except Exception:  # noqa: BLE001
                        key = None
                    if key:
                        out[package, key].update(file_id + low for low in (lows.get((found[1], entry)) or {0}))
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
                    elif f["id"] >= 100000:
                        # An actor entry's further files; below that the hundreds are a family of separate overlays.
                        out |= {p for i, p in self.zero.items() if i // 100 == f["id"] // 100}
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
        settled = len(mine) <= 1
        if bound and len(mine) > 1:
            # The entry's base file, or the further file its placements ask for.
            chosen = [found for found in mine if self.files[found[0]]["id"] in bound]
            if chosen:
                mine, settled = chosen + [found for found in mine if found not in chosen], len(chosen) == 1
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
    have = {"mesh": [], "track": {}}
    for package, (load, sources) in sorted(pkg_model.declared_records(ROOT).items()):
        path = ROOT / "assets" / "USA" / "pe2pkg" / f"{package}.pe2pkg"
        if not path.is_file():
            continue
        data = path.read_bytes()
        for offset in sorted(sources):
            try:
                src = pkg_model.read_source(data, load, offset)
                key = by_sha.get(hashlib.sha1(data[src["stream_offset"]:src["end"]]).hexdigest())
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
    side = write_side_data(out.with_name(out.stem + "_data"), assets, embedded) if pixels else {"mesh": [], "track": {}}
    try:
        banks = animation_tables(embedded)
    except Exception as exc:  # noqa: BLE001
        print(f"no animation tables: {exc}", file=sys.stderr)
        banks = {}
    sounds = {}
    if (extracted / "spk").is_dir():
        for folder in (extracted / "spk").iterdir():
            if folder.is_dir():
                sounds[folder.name] = sum(1 for f in folder.iterdir() if f.suffix == ".wav")
    catalogue = {}
    for key, rec in assets.items():
        catalogue[key] = [rec["type"], {k: rec[k] for k in SHOWN if k in rec}]
    for key, rec in embedded.items():
        if key not in catalogue:
            catalogue[key] = [rec["type"], {k: rec[k] for k in SHOWN if k in rec}]
        else:
            catalogue[key][1].update({k: rec[k] for k in ("source", "vram", "size") if k in rec})
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
        "mesh": side["mesh"], "track": side["track"], "banks": banks, "streams": streams, "saves": saves, "side": out.stem + "_data" if pixels else "",
        "files": files, "chunks": chunks, "fp": footprint, "loads": loads, "overlays": overlays, "requests": requests,
        "consts": constants, "findings": findings, "assets": catalogue, "sounds": sounds,
        "pairs": {image: [[bpp, clut, row, pal, column, sorted(users)[:8]] for (bpp, clut, row, pal, column), users in sorted(found.items(), key=str)] for image, found in pairs.items()},
        "regions": [[r["kind"][0], r["name"], r["x"], r["y"], r["w"], r["h"], r["note"]] for r in table], "layers": residency.LAYERS,
        "stagemap": {r["id"] - 900000: index[r["path"]] for r in db.execute("SELECT path, id FROM file WHERE stage = 0 AND id BETWEEN 900001 AND 900005")},
        "pictures": os.path.relpath(extracted.resolve(), out.parent.resolve()) if (extracted / "pe2img").is_dir() else "", "pixels": pixels,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--db", type=Path, default=residency.DB)
    ap.add_argument("--out", type=Path, default=ROOT / "local" / "asset_map.html")
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
