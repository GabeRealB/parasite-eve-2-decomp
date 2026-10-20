#!/usr/bin/env python3
"""Writes tools/peassets/image_palettes.tsv: the class rules read out of the draw code, applied to each image that
meets the rule's own test against the catalog (positions, counts), plus the hand-read lines of image_palettes_manual.tsv beside
this script. Run from the repository root with venv/bin/python3. Names, numbers and citations only."""
import re, sys
from collections import defaultdict
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/peassets"))
import asset_schema, asset_residency as residency
assets, embedded = asset_schema.load()
tree = asset_schema.load_tree()
lines, skipped = [], []

def add(image, column, depth, palette, row, index, user, evidence):
    lines.append((image, column, depth, palette, row, index, user, evidence))

# ---- A. item preview pictures: stage 0, group 2, hundreds 1..7
EV_A = ("src/gameplay/item_stats.c:72-75, :1085-1120 itemMenuDrawPreview (POLY_FT4, page 0x87 or 0x8F, CLUT 0x3F40 or 0x3F80); "
        "src/gameplay/menu_armor.c:446-559 itemMenuEnqueuePreviewLoad (group 2, hundreds 1..7); src/main/loadui.c:121-135 (the profile's offsets); "
        "tools/peassets/asset_residency.py moved() (src/main/fs.c:473-490, :1770-1776)")
PROFILES = {"MENU": ((-8, -3, "RELOCATE"), 0x87, 0x80, 0x3F40), "RELOCATED_PREVIEW": ((0, -2, "RELOCATE"), 0x8F, 0x80, 0x3F80),
            "PREVIEW": ((0, -2, ""), 0x8F, 0, 0x3F40)}
from texture.declaration import texture_chunks
declared = texture_chunks(assets)       # a declared file states its pairing itself (asset_map.declared_pairs)
for stage, folder, fkey, node in asset_schema.tree_files(tree):
    fid = int(node.get("id") or 0)
    if stage != 0 or folder is not None or not (20100 <= fid < 20800):
        continue
    if any(c in declared for c in (node.get("chunks") or {}).values()):
        continue
    chunks = list((node.get("chunks") or {}).values())
    images = [c for c in chunks if assets[c]["type"] in asset_schema.IMAGE_TYPES]
    cluts = [c for c in chunks if assets[c]["type"] == "pe2clut"]
    if len(images) != 1 or len(cluts) != 1 or len(asset_schema.image_columns(assets[images[0]])) != 1:
        skipped.append(f"0/{fkey}: {len(images)} images, {len(cluts)} palettes: not the one-picture, one-palette shape of a preview file")
        continue
    image, clut = assets[images[0]], assets[cluts[0]]
    ok = True
    for name, (at, page, v, value) in PROFILES.items():
        ix, iy = residency.moved((images[0], "image", "", image["x"], image["y"], image["w"], image["h"], 8), *at)
        cx, cy = residency.moved((cluts[0], "clut", "", clut["x"], clut["y"], clut["w"], clut["h"], None), *at)
        ok &= (ix, iy) == ((page & 15) * 64, v) and (cx, cy) == ((value & 63) * 16, value >> 6) and clut["w"] >= 256 and clut["h"] == 1
    if not ok:
        skipped.append(f"0/{fkey}: its picture or palette does not land where itemMenuDrawPreview reads in every load profile")
        continue
    add(images[0], 0, 8, cluts[0], 0, 0, f"gameplay: itemMenuDrawPreview, preview file {fid}", EV_A)

# ---- B. caption glyph atlases
EV_B = ("src/gameplay/captions.c:662-666, :761-762, :893-905 _capDrawTextStream (glyph CLUT 0x3D50 | palette 0..3 of the text code, highlight 0x3D52, "
        "title 0x3D93; 4-bit page from capSetTexturePage) and src/shared/room/cap_captions.inc.c:315-318, :405, :473; pe2clut_0 is at (256, 243) from boot (0/title)")
usage = defaultdict(list)
for path in sorted((ROOT / "tools/peassets/usage").glob("*.tsv")):
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        cells = line.split("\t")
        if line.startswith("#") or len(cells) < 7:
            continue
        for token in re.findall(r"[A-Za-z0-9_]+", cells[0]):
            usage[token].append((path.stem, n, cells))
def paired_already(key):
    return False
caption_done = set()
for key, rec in sorted(assets.items(), key=lambda kv: int(kv[0].rsplit("_", 1)[1]) if kv[0].startswith("pe2img_") else 0):
    if rec["type"] != "pe2img":
        continue
    columns = asset_schema.image_columns(rec)
    for stem, n, cells in usage.get(key, ()):
        # A record that says the image is the page the caption drawer reads, and cites the call that makes it so.
        if (cells[6] != "established" or not re.search(r"glyph atlas|glyph sheet|caption texture page|texture page of the conversation caption", cells[3], re.I)
                or not re.search(r"capReset|capSetTexturePage|cap_captions\.inc|CaptionResource|room_cutscene_task\.inc", "\t".join(cells))):
            continue
        # The page the record names: among the image's own columns.
        named = {(int(a), int(b)) for a, b in re.findall(r"\((\d+),\s*(\d+)\)", cells[1])} | {(int(a), int(b)) for a, b in re.findall(r"at (\d+),\s*(\d+)\)", cells[1])}
        hit = [i for i, (x, y, _b) in enumerate(columns) if (x, y) in named]
        if len(hit) != 1:
            skipped.append(f"{key}: usage/{stem}.tsv:{n} names a caption page and not one of its columns ({cells[1][:60]})")
            continue
        if (key, hit[0]) in caption_done:
            continue
        caption_done.add((key, hit[0]))
        where = f"; the page: tools/peassets/usage/{stem}.tsv:{n}"
        for p in range(4):
            add(key, hit[0], 4, "pe2clut_0", 2, p, f"gameplay: caption glyphs, palette {p} of the text code" + (" and the highlighted choice" if p == 2 else ""), EV_B + where)
        add(key, hit[0], 4, "pe2clut_0", 3, 3, "gameplay: caption title plate", EV_B + where)

# The gantry's view files 26, 28 and 43 each replace the caption page while its scene packages keep it at (384, 0).
EV_G = ("; the page: src/actors/actor_560800/actor_560800.c:4588-4589, :6234, :6787, :6800 and src/actors/actor_160900/actor_160900.c:2060-2061, :2093 "
        "capSetTexturePage(384, 0); tools/peassets/usage/shelter_b1_pod_service_gantry.tsv:13-15")
for key in ("shelter_b1_pod_service_gantry_dialogue_1_atlas", "shelter_b1_pod_service_gantry_dialogue_2_atlas", "shelter_b1_pod_service_gantry_dialogue_3_atlas"):
    if (key, 0) not in caption_done and asset_schema.image_columns(assets[key])[0][:2] == (384, 0):
        for p in range(4):
            add(key, 0, 4, "pe2clut_0", 2, p, f"gameplay: caption glyphs, palette {p} of the text code" + (" and the highlighted choice" if p == 2 else ""), EV_B + EV_G)
        add(key, 0, 4, "pe2clut_0", 3, 3, "gameplay: caption title plate", EV_B + EV_G)

# ---- C. map pictures: stage 0, group 3, loaded unshifted by _menuMapLoadPage
EV_C = "src/gameplay/menu_actions.c:1791-1824 _menuMapLoadPage (group 3, no offsets, default mode); "
MAP = [(8, 1, 0, (0, 1), "gameplay: _menuMapDrawPicture, the map picture, and the visited or marked area shapes", ":1402-1406 (getClut(0, 0x100), GetTPage(1, 0, 0x380, 0x20), u 1..255); :3486-3488"),
       (8, 0, 0, (0,), "gameplay: map area shapes filled with the pattern", ":3461-3470, :3488 (getClut(0, 0xFF), getTPage(1, 1, 0x380, 0), u and v from 0..31 at the shape's corner)"),
       (4, 2, 0, (0,), "gameplay: map player cursor", ":1361 (GetClut(0, 0x101))"),
       (4, 2, 1, (0,), "gameplay: map telephone icon", ":1756-1760 (GetClut(0x10, 0x101), u 0x40)"),
       (4, 2, 2, (0,), "gameplay: map fixed-picture icon", ":1749-1753 (GetClut(0x20, 0x101), u 0x50)"),
       (4, 2, 3, (0,), "gameplay: map flag marker", ":1561-1591 (GetClut(0x30, 0x101), u 0x60, page getTPage(0, 0, 0x380, 0))"),
       (4, 2, 4, (0,), "gameplay: map objective icon", ":1763-1768 (GetClut(0x40, 0x101), u 0x70)"),
       (4, 2, 5, (0,), "gameplay: map page arrows", ":1918-1947 (GetClut(0x50, 0x101), u 0x80 and 0x88, page getTPage(0, 0, 0x380, 0))"),
       (4, 2, 6, (0,), "gameplay: map flag marker, alternate picture", ":1561-1595 (GetClut(0x60, 0x101), u 0x90)"),
       (4, 2, 7, (0,), "gameplay: map corner sprite", ":1420-1432 (GetClut(0x70, 0x101), u 0xC0, page word 0xE)")]
for stage, folder, fkey, node in asset_schema.tree_files(tree):
    fid = int(node.get("id") or 0)
    if stage != 0 or folder is not None or not (30100 <= fid < 30600):
        continue
    chunks = list((node.get("chunks") or {}).values())
    images = [c for c in chunks if assets[c]["type"] in asset_schema.IMAGE_TYPES]
    cluts = [c for c in chunks if assets[c]["type"] == "pe2clut"]
    if not images and not cluts:
        continue
    ok = len(images) == 1 and len(cluts) == 1
    if ok:
        image, clut = assets[images[0]], assets[cluts[0]]
        ok = [c[:2] for c in asset_schema.image_columns(image)] == [(896, 0), (960, 0)] and (clut["x"], clut["y"]) == (0, 255) and clut["w"] >= 256 and clut["h"] >= 3
    if not ok:
        skipped.append(f"0/{fkey}: not one picture at (896, 0) and (960, 0) with one palette of three rows or more at (0, 255)")
        continue
    if any(l[0] == images[0] and l[3] == cluts[0] and l[6].startswith("gameplay: _menuMap") for l in lines):
        continue                    # the same picture and palette in another file
    for depth, row, index, cols, user, cite in MAP:
        for column in cols:
            add(images[0], column, depth, cluts[0], row, index, user, EV_C + "src/gameplay/menu_actions.c" + cite)

manual = Path(__file__).with_name("image_palettes_manual.tsv")
body = manual.read_text(encoding="utf-8") if manual.is_file() else ""
out = ["# Which palette an image is drawn with, where that is read out of draw code and not out of a model's faces or a",
       "# room's sprite table (those are computed: tools/peassets/asset_map.py). One line per way an image is read.",
       "# image\tcolumn\tdepth\tpalette (empty: direct colour)\trow\tpalette index\tuser\tevidence",
       "# Written by tools/peassets/gen_image_palettes.py for the rules that cover a family (item previews,",
       "# caption glyph pages); the lines after them were read one by one. asset_map.py checks every line against the catalog."]
out += ["\t".join(str(c) for c in line) for line in lines]
out += [l for l in body.splitlines() if l.strip()]
(ROOT / "tools/peassets/image_palettes.tsv").write_text("\n".join(out) + "\n", encoding="utf-8")
print(len(lines), "generated lines;", len({l[0] for l in lines}), "images;", len(skipped), "skipped")
for s in skipped: print("  ", s)
