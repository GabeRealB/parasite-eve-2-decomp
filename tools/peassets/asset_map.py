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

    def resolve(self, package: str, page: int, clut: int):
        """("own", [(file, image, palette)]) | ("ancestor", name, [(file, page, row, mode)]) | None."""
        mine = [(path, *hit) for path in self.own(package) if (hit := self.covers(self.rects(path, None), page, clut))]
        if mine and not self.is_room(package):
            return ("own", mine)
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


def build_data(db: sqlite3.Connection, out: Path, pixels: str) -> dict:
    assets, embedded = asset_schema.load()
    tree = asset_schema.load_tree()
    supply = Supply(db, assets, tree)
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
            found = supply.resolve(package, page, clut)
            bpp, tx, ty, cx, cy = supply.geometry(page, clut)
            if found is None:
                how = ["none"]
                findings[package].append(f"{key}: page ({tx}, {ty}) {bpp}-bit with palette ({cx}, {cy}) is supplied by nothing in its chain")
            elif found[0] == "own":
                how = ["own", [index[path] for path, _i, _p in found[1]]]
                for _path, image, palette in found[1]:
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
        for (package, _file, scope), found in sorted(scopes.items()):
            if 1 <= len(found["image.tpage"]) <= 2 and 1 <= len(found["clut.value"]) <= 3:
                for page_name, page in found["image.tpage"]:
                    for clut_name, clut in found["clut.value"]:
                        constants[package].append([f"{scope}: {page_name} + {clut_name}", page & ~0x60, clut])
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
    return {
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
            counts[row[4][0]] += 1
    print(f"wrote {args.out}  ({args.out.stat().st_size // 1024} KB); texture requests: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
