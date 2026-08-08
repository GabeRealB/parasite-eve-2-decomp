#!/usr/bin/env python3
"""Generate pack + ISO manifests from retail disc trees and extracted assets.

Writes two kinds of JSON under the assets root:

  * ``stages.json`` — HED/CDF content tree (stage0 + stages 1..5).
    Consumed by ``tools/peassets/pack.py``.
  * ``iso_disk1.json`` / ``iso_disk2.json`` — one ISO creation manifest
    per disc (identifiers, license, single directory_tree).
    Consumed by ``tools/build_iso.py`` (and by pack when materializing a tree).

Also back-fills data extract.py does not currently emit on its own:

  * per-file trailer.bin (bytes after the last chunk up to the next file)
  * per-file raw.bin for file-list entries that are not chunked
  * per-folder stream_data.bin (post-file stream region in STAGE1..N)

Typical usage (also invoked from extract.py)::

    python3 tools/peassets/write_manifest.py \\
        --rom rom/USA \\
        --assets assets/USA
"""

from __future__ import annotations

import json
import logging
import sys
import xml.etree.ElementTree as ET
from argparse import ArgumentParser
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from format import (  # noqa: E402
    FILE_CHUNK_HEADER_SIZE,
    FILE_LIST_TERMINATOR,
    SECTOR_SIZE,
    STAGE0_FILE_LIST_COUNT,
    STAGE0_STREAMING_COUNT,
    STAGE_N_FILE_LIST_COUNT,
    STAGE_N_FOLDER_HEADER_SIZE,
    STAGE_N_FOLDER_LIST_COUNT,
    STAGE_N_STREAMING_COUNT,
    FileChunkEndFlag,
    chunk_type_id,
    parse_file_chunk_header,
    parse_file_list_entry,
    parse_folder_list_entry,
    parse_streaming_list_entry,
    resolve_chunk_type,
    streaming_entry_to_json,
    validate_sector_len,
)


def read_chunks_size(data: bytes, offset: int) -> int | None:
    """Return byte length of concatenated chunks starting at offset, or None."""
    if offset >= len(data):
        return None
    pos = offset
    any_chunk = False
    while pos + FILE_CHUNK_HEADER_SIZE <= len(data):
        header = parse_file_chunk_header(data[pos : pos + FILE_CHUNK_HEADER_SIZE])
        if header is None:
            break
        any_chunk = True
        if header.chunk_size < FILE_CHUNK_HEADER_SIZE:
            break
        pos += header.chunk_size
        if header.end_flag == FileChunkEndFlag.End:
            return pos - offset
    return (pos - offset) if any_chunk else None


def dump_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    logging.info("Wrote %s", path)


def layout_xml_to_iso_manifest(
    layout_path: Path, disk_dirname: str
) -> dict[str, Any]:
    """Convert a dumpsxiso layout.xml into a single-disc ISO manifest."""
    tree = ET.parse(layout_path)
    root = tree.getroot()
    image_name = root.attrib.get("image_name", f"{disk_dirname}.bin")
    cue_sheet = root.attrib.get("cue_sheet", f"{disk_dirname}.cue")

    track_el = root.find("track")
    if track_el is None:
        raise ValueError(f"no <track> in {layout_path}")

    identifiers_el = track_el.find("identifiers")
    identifiers = dict(identifiers_el.attrib) if identifiers_el is not None else {}

    license_file = f"{disk_dirname}/license_data.dat"

    default_attrs_el = track_el.find("default_attributes")
    default_attrs = (
        {
            k: int(v) if v.lstrip("-").isdigit() else v
            for k, v in default_attrs_el.attrib.items()
        }
        if default_attrs_el is not None
        else {}
    )

    directory_tree: list[dict[str, Any]] = []
    dir_el = track_el.find("directory_tree")
    if dir_el is not None:
        for child in dir_el:
            if child.tag == "file":
                name = child.attrib["name"]
                entry: dict[str, Any] = {
                    "name": name,
                    "type": child.attrib.get("type", "data"),
                }
                upper = name.upper()
                if upper == "STAGE0.HED":
                    entry["generated"] = "stage0_hed"
                elif upper == "STAGE0.CDF":
                    entry["generated"] = "stage0_cdf"
                elif upper.startswith("STAGE") and upper.endswith(".CDF"):
                    num = upper[len("STAGE") : -len(".CDF")]
                    entry["generated"] = f"stage{num}_cdf"
                else:
                    # Passthrough relative to the rom/ version root.
                    entry["source"] = f"{disk_dirname}/{name}"
                directory_tree.append(entry)
            elif child.tag == "dummy":
                directory_tree.append(
                    {
                        "dummy_sectors": int(child.attrib.get("sectors", 150)),
                        "type": int(child.attrib.get("type", 0)),
                    }
                )

    # Prefer known retail image names for USA disk dirs when dumpsxiso left defaults.
    if not image_name or image_name.startswith("mkpsxiso") or image_name.startswith(
        "disk"
    ):
        if disk_dirname == "disk1":
            image_name = "SLUS-01042.bin"
            cue_sheet = "SLUS-01042.cue"
        elif disk_dirname == "disk2":
            image_name = "SLUS-01055.bin"
            cue_sheet = "SLUS-01055.cue"
        else:
            image_name = f"{disk_dirname}.bin"
            cue_sheet = f"{disk_dirname}.cue"

    return {
        "name": disk_dirname,
        "image_name": image_name,
        "cue_sheet": cue_sheet,
        "track": {
            "type": track_el.attrib.get("type", "data"),
            "xa_edc": track_el.attrib.get("xa_edc", "true").lower() == "true",
            "new_type": track_el.attrib.get("new_type", "false").lower() == "true",
            "identifiers": identifiers,
            "license": license_file,
            "default_attributes": default_attrs,
            "directory_tree": directory_tree,
        },
    }


def _dump_streaming_json(path: Path, entries: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(entries, f, indent=2)
        f.write("\n")


def _contents_from_raw_file(
    assets_dir: Path, *, stage_rel: str, file_id: int
) -> dict[str, dict[str, Any]]:
    """Build an ordered contents dict for one file (name → pack fields).

    Keys are asset basenames (``0.spk``, ``1.pe2pkg``, ``raw.bin``); order is
    on-disc chunk order.
    """
    raw_file_dir = assets_dir / "raw" / stage_rel / f"file{file_id}"
    headers_path = raw_file_dir / "headers.json"
    decoded_file_dir = f"decoded/{stage_rel}/file{file_id}"

    headers: list[Any] = []
    if headers_path.exists():
        headers = json.loads(headers_path.read_text(encoding="utf-8")) or []

    if not headers:
        raw_bin = raw_file_dir / "raw.bin"
        if raw_bin.exists():
            return {
                "raw.bin": {
                    "path": f"raw/{stage_rel}/file{file_id}/raw.bin",
                    "type": "raw",
                }
            }
        return {}

    contents: dict[str, dict[str, Any]] = {}
    for idx, header in enumerate(headers):
        chunk_type = resolve_chunk_type(header["chunk_type"])
        ext = chunk_type.get_extension()
        type_id = chunk_type_id(chunk_type)
        name = f"{idx}{ext}"
        entry: dict[str, Any] = {
            "path": f"{decoded_file_dir}/{name}",
            "type": type_id,
        }
        # sector_len: exclusive end offset in the CD sector buffer (default 0x800).
        sector_len = validate_sector_len(
            int(str(header.get("sector_len", header.get("unknown1", "0x800"))), 0)
        )
        if sector_len != SECTOR_SIZE:
            entry["sector_len"] = f"0x{sector_len:X}"
        # load_address: RAM dest; only room packages and Cap2 use it.
        load_addr = header.get("unknown2", "0x0")
        if type_id in ("room_pkg", "cap2") and int(str(load_addr), 0) != 0:
            entry["load_address"] = load_addr
        contents[name] = entry
    return contents


def collect_stage0(
    hed_path: Path, cdf_path: Path, assets_dir: Path
) -> dict[str, Any]:
    """Write raw sidecars; return files as lists of content dicts."""
    hed = hed_path.read_bytes()
    cdf = cdf_path.read_bytes()

    streaming_list = []
    for i in range(STAGE0_STREAMING_COUNT):
        raw = hed[i * 0x28 : (i + 1) * 0x28]
        if raw == b"\x00" * 0x28:
            continue
        entry = parse_streaming_list_entry(raw)
        streaming_list.append(streaming_entry_to_json(entry))

    raw_stage0 = assets_dir / "raw" / "stage0"
    raw_stage0.mkdir(parents=True, exist_ok=True)
    if streaming_list:
        _dump_streaming_json(raw_stage0 / "streaming.json", streaming_list)

    file_list = []
    for i in range(STAGE0_FILE_LIST_COUNT):
        off = 0x78 + i * 8
        entry = parse_file_list_entry(hed[off : off + 8])
        if entry.file_id == FILE_LIST_TERMINATOR:
            break
        file_list.append(entry)

    term_off = 0x78 + len(file_list) * 8
    if term_off + 8 <= len(hed):
        term = parse_file_list_entry(hed[term_off : term_off + 8])
        if term.file_id != FILE_LIST_TERMINATOR:
            logging.warning("STAGE0.HED missing expected file-list terminator")

    # Retail file offsets (for exact repack when assets are unmodified).
    layout = [
        {"file_id": e.file_id, "offset": f"0x{e.file_offset:X}"} for e in file_list
    ]
    with (raw_stage0 / "layout.json").open("w", encoding="utf-8") as f:
        json.dump(layout, f, indent=2)
        f.write("\n")

    # Ordered dict: insertion order matches HED file list (JSON object order).
    contents_map: dict[str, dict[str, dict[str, Any]]] = {}
    for i, entry in enumerate(file_list):
        j = i + 1
        while j < len(file_list) and file_list[j].file_offset == entry.file_offset:
            j += 1
        if j < len(file_list):
            span_end = file_list[j].file_offset
        else:
            span_end = len(cdf)

        raw_file_dir = assets_dir / "raw" / "stage0" / f"file{entry.file_id}"
        chunk_size = read_chunks_size(cdf, entry.file_offset)

        if chunk_size is None:
            if i == 0 or file_list[i - 1].file_offset != entry.file_offset:
                raw_file_dir.mkdir(parents=True, exist_ok=True)
                (raw_file_dir / "raw.bin").write_bytes(
                    cdf[entry.file_offset : span_end]
                )
        else:
            trailer_start = entry.file_offset + chunk_size
            if trailer_start < span_end:
                if i == 0 or file_list[i - 1].file_offset != entry.file_offset:
                    trailer = cdf[trailer_start:span_end]
                    if trailer:
                        raw_file_dir.mkdir(parents=True, exist_ok=True)
                        (raw_file_dir / "trailer.bin").write_bytes(trailer)

        contents_map[f"file{entry.file_id}"] = _contents_from_raw_file(
            assets_dir, stage_rel="stage0", file_id=entry.file_id
        )

    return {"files": contents_map}


def collect_stage_n(
    stage_index: int, cdf_path: Path, assets_dir: Path
) -> dict[str, Any]:
    """Write raw sidecars; return ordered folders dict (id → files dict)."""
    cdf = cdf_path.read_bytes()
    folders_map: dict[str, dict[str, dict[str, dict[str, Any]]]] = {}

    folder_offset = STAGE_N_FOLDER_LIST_COUNT * 8  # 0x800

    for fi in range(STAGE_N_FOLDER_LIST_COUNT):
        fl_entry = parse_folder_list_entry(cdf[fi * 8 : fi * 8 + 8])
        if fl_entry.folder_id == 0 and fl_entry.folder_size == 0:
            continue

        root = folder_offset
        folder_size = fl_entry.folder_size
        raw_folder = assets_dir / "raw" / f"stage{stage_index}" / str(fl_entry.folder_id)
        raw_folder.mkdir(parents=True, exist_ok=True)

        file_list = []
        for i in range(STAGE_N_FILE_LIST_COUNT):
            entry = parse_file_list_entry(cdf[root + i * 8 : root + i * 8 + 8])
            if entry.file_id == 0 and entry.file_offset == 0:
                continue
            file_list.append(entry)

        streaming_list = []
        stream_base = root + 0x514
        for i in range(STAGE_N_STREAMING_COUNT):
            raw = cdf[stream_base + i * 0x28 : stream_base + (i + 1) * 0x28]
            if raw == b"\x00" * 0x28:
                continue
            streaming_list.append(parse_streaming_list_entry(raw))

        if file_list:
            last = file_list[-1]
            abs_off = root + last.file_offset
            chunk_size = read_chunks_size(cdf, abs_off)
            if chunk_size is None:
                content_end = last.file_offset
            else:
                content_end = last.file_offset + chunk_size
        else:
            content_end = STAGE_N_FOLDER_HEADER_SIZE

        if content_end < folder_size:
            stream_bytes = cdf[root + content_end : root + folder_size]
            if stream_bytes and any(stream_bytes):
                (raw_folder / "stream_data.bin").write_bytes(stream_bytes)

        if streaming_list:
            _dump_streaming_json(
                raw_folder / "streaming.json",
                [
                    streaming_entry_to_json(
                        e, content_end=content_end, container_size=folder_size
                    )
                    for e in streaming_list
                ],
            )

        layout = [
            {"file_id": e.file_id, "offset": f"0x{e.file_offset:X}"} for e in file_list
        ]
        meta = {
            "folder_size": f"0x{folder_size:X}",
            "content_end": f"0x{content_end:X}",
            "files": layout,
        }
        with (raw_folder / "layout.json").open("w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
            f.write("\n")

        # Ordered dict: insertion order matches CDF file list.
        folder_files: dict[str, dict[str, dict[str, Any]]] = {}
        stage_rel = f"stage{stage_index}/{fl_entry.folder_id}"
        for i, entry in enumerate(file_list):
            raw_file_dir = raw_folder / f"file{entry.file_id}"
            abs_off = root + entry.file_offset
            next_off = (
                file_list[i + 1].file_offset
                if i + 1 < len(file_list)
                else content_end
            )
            chunk_size = read_chunks_size(cdf, abs_off)
            if chunk_size is None:
                raw_file_dir.mkdir(parents=True, exist_ok=True)
                (raw_file_dir / "raw.bin").write_bytes(cdf[abs_off : root + next_off])
            else:
                trailer_start = abs_off + chunk_size
                trailer_end = root + next_off
                if trailer_start < trailer_end:
                    trailer = cdf[trailer_start:trailer_end]
                    if trailer:
                        raw_file_dir.mkdir(parents=True, exist_ok=True)
                        (raw_file_dir / "trailer.bin").write_bytes(trailer)
            folder_files[f"file{entry.file_id}"] = _contents_from_raw_file(
                assets_dir, stage_rel=stage_rel, file_id=entry.file_id
            )

        folders_map[str(fl_entry.folder_id)] = folder_files
        folder_offset += folder_size

    return {"folders": folders_map}


def write_manifest(
    rom_dir: Path,
    assets_dir: Path,
    *,
    disk1_name: str = "disk1",
    disk2_name: str = "disk2",
    stage0_hed: Path | None = None,
    stage0_cdf: Path | None = None,
    stage_cdfs: dict[int, Path] | None = None,
    stages_path: Path | None = None,
    iso_dir: Path | None = None,
) -> dict[str, Path]:
    """Write stages.json + per-disk ISO manifests. Returns paths written."""
    rom_dir = rom_dir.resolve()
    assets_dir = assets_dir.resolve()
    assets_dir.mkdir(parents=True, exist_ok=True)

    disk1 = rom_dir / disk1_name
    disk2 = rom_dir / disk2_name

    stage0_hed = stage0_hed or disk1 / "STAGE0.HED"
    stage0_cdf = stage0_cdf or disk1 / "STAGE0.CDF"
    if stage_cdfs is None:
        stage_cdfs = {
            1: disk1 / "STAGE1.CDF",
            2: disk1 / "STAGE2.CDF",
            3: disk1 / "STAGE3.CDF",
            4: disk2 / "STAGE4.CDF",
            5: disk2 / "STAGE5.CDF",
        }

    # Flat map: each stage is either { "files": … } or { "folders": … }.
    stages_manifest: dict[str, Any] = {}

    logging.info("Collecting STAGE0 structure from %s", stage0_hed)
    stages_manifest["stage0"] = collect_stage0(stage0_hed, stage0_cdf, assets_dir)

    for idx, path in sorted(stage_cdfs.items()):
        if not path.exists():
            logging.warning("missing %s — skipping stage %d", path, idx)
            continue
        logging.info("Collecting STAGE%d structure from %s", idx, path)
        stages_manifest[f"stage{idx}"] = collect_stage_n(idx, path, assets_dir)

    stages_path = stages_path or (assets_dir / "stages.json")
    dump_json(stages_path, stages_manifest)

    written: dict[str, Path] = {"stages": stages_path}
    iso_dir = iso_dir or assets_dir

    for name in (disk1_name, disk2_name):
        layout = rom_dir / name / "layout.xml"
        if not layout.exists():
            logging.warning("missing %s — skipping ISO manifest for %s", layout, name)
            continue
        iso_manifest = layout_xml_to_iso_manifest(layout, name)
        iso_path = iso_dir / f"iso_{name}.json"
        dump_json(iso_path, iso_manifest)
        written[name] = iso_path

    # Remove legacy combined manifest if present.
    legacy = assets_dir / "manifest.json"
    if legacy.exists():
        legacy.unlink()
        logging.info("Removed legacy combined manifest %s", legacy)

    return written


def main(argv: list[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--rom", "-r", type=Path, required=True, help="rom/USA directory")
    parser.add_argument(
        "--assets", "-a", type=Path, required=True, help="assets/USA directory"
    )
    parser.add_argument(
        "--stages-out",
        type=Path,
        help="Path for stages.json (default: <assets>/stages.json)",
    )
    parser.add_argument(
        "--iso-out-dir",
        type=Path,
        help="Directory for iso_disk*.json (default: <assets>/)",
    )
    args = parser.parse_args(argv)
    write_manifest(
        args.rom,
        args.assets,
        stages_path=args.stages_out,
        iso_dir=args.iso_out_dir,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
