#!/usr/bin/env python3
"""The disc description: each release's discs and stage containers, under declarations/disc/<release>/.

    disc.py migrate [--release USA] [--extraction DIR]   write it from today's sources
    disc.py check [--release USA] [--extraction DIR]     rebuild those sources' structure from it, and compare

The description is structure only - names, ids, offsets, sizes, chunk types and pack fields, streaming entries - which
extraction finds assets in and the packer lays out (local/project_design.md, "Declarations"). It names no assets:
assets point into it, by stage, folder, file and chunk position. What a container carries besides chunks - an opaque
file (`raw`), a file's trailer, a folder's stream data after its files - is content, which extraction writes; the
description says only that it is there.

    discs.toml        each disc: its image (name, size, sha1), the ISO's identifiers and attributes, and its files in
                      order - taken from the disc (`from_disc`), built from a stage container (`container`), or dummy
                      sectors
    stage0.toml       the HED's streaming list, and the files in disc order: name, id, offset, chunks
    stage<N>.toml     the folders in order (name, id, size, content end, streaming list, stream data), each with its
                      files: name, id, offset, chunks

A chunk is { type, load (packages and bundles), sector_len (where it is not 0x800) }, in order.

`migrate` copies today's sources: the TREE in tools/peassets/asset_data.py (the names), and a full extraction's
stages.json, sidecars and ISO manifests (default: the checkout's project game layer, project/.game) with its
project.toml's discs. `check` holds the description to the same sources field by field.
"""
from __future__ import annotations

import argparse
import json
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DECLARATIONS = ROOT / "declarations"
SECTOR = 0x800


def h(n: int) -> str:
    return f"0x{n:X}"


def num(v) -> int:
    return int(v, 0) if isinstance(v, str) else int(v)


def toml_value(v, hexed: set[str] = frozenset(), key: str = "") -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return h(v) if key in hexed else str(v)
    if isinstance(v, str):
        return json.dumps(v)
    if isinstance(v, list):
        return "[" + ", ".join(toml_value(x, hexed, key) for x in v) + "]"
    return "{ " + ", ".join(f"{k} = {toml_value(x, hexed, k)}" for k, x in v.items()) + " }"


HEX = {"offset", "size", "content_end", "load", "sector_len", "unknown1", "unknown2", "unknown3", "unknown4",
       "unknown5", "unknown6", "unknown7", "unknown8", "offset_folder", "offset_inter", "offset_stage"}


def streaming_entry(e: dict) -> dict:
    """A streaming entry as the description keeps it: numbers as numbers."""
    return {k: (num(v) if isinstance(v, str) and v.startswith("0x") else v) for k, v in e.items()}


def chunk(entry: dict) -> dict:
    out = {"type": entry["type"]}
    if "load_addr" in entry:
        out["load"] = num(entry["load_addr"])
    if "sector_len" in entry and num(entry["sector_len"]) != SECTOR:
        out["sector_len"] = num(entry["sector_len"])
    return out


def file_record(name: str, file_id: int, offset: int, contents: dict, side: Path) -> dict:
    rec = {"name": name, "id": file_id, "offset": offset}
    items = list(contents.values())
    if len(items) == 1 and items[0]["type"] == "raw":
        rec["raw"] = True                                  # an opaque file: content, written by extraction
    else:
        rec["chunks"] = [chunk(e) for e in items]
    if (side / "trailer.bin").is_file():
        rec["trailer"] = True
    return rec


# --- migrate -------------------------------------------------------------------------------------------------------

def collect(extraction: Path, release: str) -> dict:
    """The description, from a full extraction's sources."""
    sys.path.insert(0, str(ROOT / "tools" / "peassets"))
    import project
    stages = json.loads((extraction / "stages.json").read_text())
    out: dict = {}
    s0 = extraction / "stage0"
    layout0 = json.loads((s0 / "layout.json").read_text())
    files0 = stages["stage0"]["files"]
    if len(layout0) != len(files0):
        raise SystemExit("stage0: layout.json and stages.json list different files")
    out["stage0"] = {
        "streaming": [streaming_entry(e) for e in json.loads((s0 / "streaming.json").read_text())]
        if (s0 / "streaming.json").exists() else [],
        "stream_data": (s0 / "stream_data.bin").is_file(),
        "files": [file_record(name, num(lay["file_id"]), num(lay["offset"]), contents, s0 / name)
                  for (name, contents), lay in zip(files0.items(), layout0)],
    }
    for n in range(1, 6):
        folders = []
        for fname, files in stages[f"stage{n}"]["folders"].items():
            side = extraction / f"stage{n}" / fname
            lay = json.loads((side / "layout.json").read_text())
            if len(lay["files"]) != len(files):
                raise SystemExit(f"stage{n}/{fname}: layout.json and stages.json list different files")
            folders.append({
                "name": fname, "id": num(lay["folder_id"]), "size": num(lay["folder_size"]),
                "content_end": num(lay["content_end"]),
                "streaming": [streaming_entry(e) for e in json.loads((side / "streaming.json").read_text())]
                if (side / "streaming.json").exists() else [],
                "stream_data": (side / "stream_data.bin").is_file(),
                "files": [file_record(name, num(fl["file_id"]), num(fl["offset"]), contents, side / name)
                          for (name, contents), fl in zip(files.items(), lay["files"])],
            })
        out[f"stage{n}"] = {"folders": folders}
    p = project.load(extraction.parent) if project.is_project(extraction.parent) else None
    discs = {d["name"]: d for d in (p.game.releases.get(release, []) if p and p.game else [])}
    out["discs"] = []
    for iso_path in sorted(extraction.glob("iso_disk*.json")):
        iso = json.loads(iso_path.read_text())
        track = iso["track"]
        image = discs.get(iso["image_name"], {})
        files = []
        for e in track["directory_tree"]:
            if "dummy_sectors" in e:
                files.append({"dummy_sectors": e["dummy_sectors"]})
            elif "generated" in e:
                files.append({"name": e["name"], "type": e["type"], "container": e["generated"]})
            else:
                files.append({"name": e["name"], "type": e["type"], "from_disc": True})
        out["discs"].append({
            "name": iso["name"], "image": iso["image_name"], "cue": iso["cue_sheet"],
            **({"size": image["size"], "sha1": image["sha1"]} if image else {}),
            "track_type": track["type"], "xa_edc": track["xa_edc"], "new_type": track["new_type"],
            "identifiers": track["identifiers"], "license": "license" in track,
            "default_attributes": track["default_attributes"], "files": files,
        })
    return out


def render_files(files: list[dict], prefix: str) -> list[str]:
    lines = []
    for f in files:
        lines += ["", f"[[{prefix}]]"]
        lines += [f"{k} = {toml_value(v, HEX, k)}" for k, v in f.items() if k != "chunks"]
        if "chunks" in f:
            lines += ["chunks = ["] + [f"    {toml_value(c, HEX)}," for c in f["chunks"]] + ["]"]
    return lines


def render_streaming(entries: list[dict], key: str) -> list[str]:
    return [f"{key} = ["] + [f"    {toml_value(e, HEX)}," for e in entries] + ["]"] if entries else []


def write(desc: dict, release: str) -> list[Path]:
    out_dir = DECLARATIONS / "disc" / release
    out_dir.mkdir(parents=True, exist_ok=True)
    head = "# The disc description (tools/declarations/disc.py): structure only, no game data."
    written = []
    lines = [head, ""]
    for d in desc["discs"]:
        lines += ["[[disc]]"] + [f"{k} = {toml_value(v)}" for k, v in d.items() if k != "files"]
        lines += ["files = ["] + [f"    {toml_value(f)}," for f in d["files"]] + ["]", ""]
    (out_dir / "discs.toml").write_text("\n".join(lines).rstrip("\n") + "\n")
    written.append(out_dir / "discs.toml")
    s0 = desc["stage0"]
    lines = [head, "# STAGE0.HED and STAGE0.CDF: the HED's streaming list, the files in disc order.", ""]
    lines += render_streaming(s0["streaming"], "streaming")
    lines += [f"stream_data = {toml_value(s0['stream_data'])}"]
    lines += render_files(s0["files"], "file")
    (out_dir / "stage0.toml").write_text("\n".join(lines) + "\n")
    written.append(out_dir / "stage0.toml")
    for n in range(1, 6):
        lines = [head, f"# STAGE{n}.CDF: the folders in order, each with its files.", ""]
        for f in desc[f"stage{n}"]["folders"]:
            lines += ["", "[[folder]]"] + [f"{k} = {toml_value(v, HEX, k)}" for k, v in f.items()
                                            if k not in ("files", "streaming")]
            lines += render_streaming(f["streaming"], "streaming")
            lines += render_files(f["files"], "folder.file")
        (out_dir / f"stage{n}.toml").write_text("\n".join(lines) + "\n")
        written.append(out_dir / f"stage{n}.toml")
    return written


# --- read and check ------------------------------------------------------------------------------------------------

def load(release: str) -> dict:
    """The description as written: { discs, stage0, stage1..5 }."""
    d = DECLARATIONS / "disc" / release
    out = {"discs": tomllib.loads((d / "discs.toml").read_text())["disc"]}
    s0 = tomllib.loads((d / "stage0.toml").read_text())
    out["stage0"] = {"streaming": s0.get("streaming", []), "stream_data": s0["stream_data"], "files": s0["file"]}
    for n in range(1, 6):
        folders = tomllib.loads((d / f"stage{n}.toml").read_text()).get("folder", [])
        for f in folders:
            f["files"] = f.pop("file", [])
            f.setdefault("streaming", [])
        out[f"stage{n}"] = {"folders": folders}
    return out


def check(release: str, extraction: Path) -> list[str]:
    """Where the description and today's sources disagree."""
    want, got = collect(extraction, release), load(release)
    problems = []

    def compare(path: str, a, b) -> None:
        if isinstance(a, dict) and isinstance(b, dict):
            for k in sorted(set(a) | set(b)):
                compare(f"{path}.{k}", a.get(k), b.get(k))
        elif isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
            for i, (x, y) in enumerate(zip(a, b)):
                compare(f"{path}[{i}]", x, y)
        elif a != b:
            problems.append(f"{path}: sources {a!r}, description {b!r}")
    compare(release, want, got)
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("migrate", "check"))
    ap.add_argument("--release", default="USA")
    ap.add_argument("--extraction", type=Path, default=ROOT / "project" / ".game")
    args = ap.parse_args()
    if args.command == "migrate":
        for p in write(collect(args.extraction, args.release), args.release):
            print(p.relative_to(ROOT))
        return 0
    problems = check(args.release, args.extraction)
    print("\n".join(problems[:20]) or f"disc/{args.release}: the description holds every source's structure")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
