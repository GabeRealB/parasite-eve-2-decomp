#!/usr/bin/env python3
"""Build HED/CDF containers and one ISO directory tree from JSON manifests.

Inputs (two separate manifests):

  * ``stages.json`` — each file lists **contents** (decoded assets + pack fields).
  * ``iso_diskN.json`` — single-disc ISO layout (one directory_tree).

Example ``stages.json`` (flat stage map; each stage is files *or* folders)::

    {
      "stage0": {
        "files": {
          "gameplay": {
            "0.spk": { "path": "decoded/stage0/gameplay/0.spk", "type": "music" },
            "1.pe2pkg": {
              "path": "decoded/stage0/gameplay/gameplay.pe2pkg",
              "type": "room_pkg",
              "load_address": "0x80093800"
            }
          }
        }
      },
      "stage1": {
        "folders": {
          "101": {
            "file0": {
              "0.spk": { "path": "decoded/stage1/101/file0/0.spk", "type": "music" }
            }
          }
        }
      }
    }

``files`` → STAGE0-style HED+CDF. ``folders`` → STAGEn.CDF with folder table.
File/folder dict keys use friendly names from ``names.NAMES`` when set (else
``file0`` / ``101``). Chunk keys stay disc-order basenames (``1.pe2pkg``).
Key order is on-disc order. Trailers / streams / layout still come from ``raw/``.
"""

from __future__ import annotations

import json
import logging
import shutil
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
    STAGE0_HED_SIZE,
    STAGE0_STREAMING_COUNT,
    STAGE_N_FILE_LIST_COUNT,
    STAGE_N_FOLDER_HEADER_SIZE,
    STAGE_N_FOLDER_HEADER_USED,
    STAGE_N_FOLDER_LIST_COUNT,
    STAGE_N_FOLDER_LIST_SIZE,
    STAGE_N_STREAMING_COUNT,
    FileChunkHeader,
    FileChunkType,
    FileListEntry,
    FolderListEntry,
    align_up,
    encode_file_chunk_header,
    encode_file_list_entry,
    encode_folder_list_entry,
    encode_streaming_list_entry,
    encode_u32,
    parse_int,
    resolve_chunk_type,
    resolve_end_flag,
    streaming_entry_from_json,
    validate_sector_len,
)
from lzss import decode_lzss, encode_lzss  # noqa: E402
from names import (  # noqa: E402
    disk_file_rel,
    disk_folder_rel,
    resolve_file_id,
    resolve_folder_id,
)


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def resolve_path(base: Path, value: str | Path) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return (base / path).resolve()


def pad_to(buf: bytearray | bytes, size: int, fill: int = 0) -> bytes:
    data = bytearray(buf)
    if len(data) > size:
        raise ValueError(f"buffer length 0x{len(data):X} exceeds target 0x{size:X}")
    if len(data) < size:
        data.extend(bytes([fill]) * (size - len(data)))
    return bytes(data)


def install_file(src: Path, dst: Path) -> None:
    """Copy src -> dst, replacing any existing file. Uses hardlink when possible."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists() or dst.is_symlink():
        dst.unlink()
    try:
        os_link = getattr(Path, "hardlink_to", None)
        if os_link is not None:
            dst.hardlink_to(src)
            return
    except OSError:
        pass
    try:
        import os

        os.link(src, dst)
        return
    except OSError:
        pass
    shutil.copy2(src, dst)


def build_chunk_blob(header_json: dict[str, Any], payload: bytes) -> bytes:
    """Build one chunk: 0x10 header + payload, sector-aligned total size."""
    chunk_type = resolve_chunk_type(header_json["chunk_type"])
    end_flag = resolve_end_flag(header_json["end_flag"])
    sector_len = validate_sector_len(
        parse_int(
            header_json.get("sector_len", header_json.get("unknown1", SECTOR_SIZE))
        )
    )
    unknown2 = parse_int(
        header_json.get("unknown2", header_json.get("load_address", 0))
    )

    total_unpadded = FILE_CHUNK_HEADER_SIZE + len(payload)
    # Always size from actual payload (edited assets may grow/shrink).
    chunk_size = align_up(total_unpadded, SECTOR_SIZE)

    payload_padded = pad_to(payload, chunk_size - FILE_CHUNK_HEADER_SIZE)
    header = FileChunkHeader(
        chunk_type=chunk_type,
        end_flag=end_flag,
        sector_len=sector_len,
        chunk_size=chunk_size,
        unknown2=unknown2,
    )
    return encode_file_chunk_header(header) + payload_padded


def prepare_on_disc_payload(
    decoded_payload: bytes,
    *,
    chunk_type: FileChunkType,
    raw_payload_path: Path | None,
) -> bytes:
    """Compress decoded payload for disc when needed.

    Prefer the original raw .pe2pkg when the decoded bytes still match a
    decode of that raw blob (avoids re-encoding noise). Otherwise LZSS-encode.
    """
    if chunk_type != FileChunkType.RoomPkg:
        return decoded_payload

    if raw_payload_path is not None and raw_payload_path.exists():
        raw_bytes = raw_payload_path.read_bytes()
        try:
            if decode_lzss(raw_bytes) == decoded_payload:
                return raw_bytes
        except Exception:
            logging.warning(
                "failed to decode raw package %s; re-encoding from decoded",
                raw_payload_path,
            )
    return encode_lzss(decoded_payload)


def raw_sidecar_for_content(content_path: Path, assets_dir: Path) -> Path | None:
    """Map a decoded (or raw) content path to the sibling raw/ payload if any."""
    try:
        rel = content_path.resolve().relative_to((assets_dir / "decoded").resolve())
        return assets_dir / "raw" / rel
    except ValueError:
        pass
    try:
        rel = content_path.resolve().relative_to((assets_dir / "raw").resolve())
        return assets_dir / "raw" / rel
    except ValueError:
        return None


def build_file_blob_from_contents(
    contents: dict[str, dict[str, Any]] | list[dict[str, Any]],
    assets_dir: Path,
    *,
    raw_file_dir: Path | None,
) -> bytes:
    """Build one CDF file from an ordered contents dict (or legacy list)."""
    if isinstance(contents, dict):
        items = list(contents.items())  # preserve insertion order
    elif isinstance(contents, list):
        items = [(f"_{i}", c) for i, c in enumerate(contents)]
    else:
        raise TypeError("file contents must be a dict (name → fields) or list")

    if not items:
        # Shared-offset alias entry with no private payload (see layout.json).
        return b""

    # Opaque non-chunked file
    if len(items) == 1 and items[0][1].get("type") == "raw":
        content = items[0][1]
        path = resolve_path(assets_dir, content["path"])
        data = path.read_bytes()
        return pad_to(data, align_up(len(data), SECTOR_SIZE)) if data else b""

    out = bytearray()
    for i, (name, content) in enumerate(items):
        ctype = content.get("type") or content.get("chunk_type")
        if ctype == "raw":
            raise ValueError("type=raw cannot be mixed with chunked contents")
        chunk_type = resolve_chunk_type(ctype)
        # Last chunk is End; earlier chunks Continue (matches on-disc files).
        end_flag = content.get("end_flag")
        if end_flag is None:
            end_flag = "end" if i == len(items) - 1 else "continue"
        # sector_len: exclusive end offset in the CD sector buffer (default 0x800).
        sector_len = validate_sector_len(
            parse_int(
                content.get(
                    "sector_len", content.get("unknown1", f"0x{SECTOR_SIZE:X}")
                )
            )
        )
        header_json = {
            "chunk_type": chunk_type.get_name(),
            "end_flag": end_flag,
            "sector_len": sector_len,
            "unknown2": content.get(
                "load_address", content.get("unknown2", "0x0")
            ),
        }
        if "path" in content:
            payload_path = resolve_path(assets_dir, content["path"])
        else:
            # Derive path from name when omitted: decoded/…/fileN/name
            if raw_file_dir is None:
                raise ValueError(f"content {name!r} missing path and raw_file_dir")
            # raw_file_dir is …/raw/stage…/fileN → decoded sibling
            try:
                rel = raw_file_dir.resolve().relative_to(
                    (assets_dir / "raw").resolve()
                )
                payload_path = assets_dir / "decoded" / rel / name
            except ValueError:
                payload_path = raw_file_dir / name
        if not payload_path.exists():
            raise FileNotFoundError(f"missing content payload: {payload_path}")
        decoded_payload = payload_path.read_bytes()
        raw_payload = raw_sidecar_for_content(payload_path, assets_dir)
        on_disc = prepare_on_disc_payload(
            decoded_payload,
            chunk_type=chunk_type,
            raw_payload_path=raw_payload,
        )
        out.extend(build_chunk_blob(header_json, on_disc))

    # Optional post-chunk trailer lives under raw/ next to the file.
    if raw_file_dir is not None:
        trailer_path = raw_file_dir / "trailer.bin"
        if trailer_path.exists():
            out.extend(trailer_path.read_bytes())

    return pad_to(bytes(out), align_up(len(out), SECTOR_SIZE))


def resolve_raw_file_dir(
    contents: dict[str, dict[str, Any]] | list[dict[str, Any]],
    assets_dir: Path,
    *,
    file_name: str,
    stage: int | None = None,
    folder_id: int | None = None,
    stage_rel: str | None = None,
) -> Path:
    """Infer raw/… file directory for trailers from content paths or names.

    Prefer the ``path`` field (already uses friendly names). Fall back to
    :func:`disk_file_rel` when stage/file ids are known, else legacy stage_rel.
    """
    items = contents.values() if isinstance(contents, dict) else contents
    for content in items:
        path = content.get("path")
        if not path:
            continue
        p = Path(path)
        parts = p.parts
        if "decoded" in parts:
            idx = parts.index("decoded")
            rest = parts[idx + 1 : -1]
            if rest:
                return assets_dir / "raw" / Path(*rest)
        if parts and parts[-1] == "raw.bin" and "raw" in parts:
            # raw/stage0/gameplay/raw.bin -> raw/stage0/gameplay
            return assets_dir / Path(*parts[:-1])
    if stage is not None:
        try:
            file_id = resolve_file_id(file_name, stage=stage, folder_id=folder_id)
        except ValueError:
            file_id = None
        if file_id is not None:
            return assets_dir / "raw" / disk_file_rel(stage, file_id, folder_id)
    if stage_rel is not None:
        return assets_dir / "raw" / stage_rel / file_name
    return assets_dir / "raw" / "stage0" / file_name


def load_streaming_json(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        return []
    data = load_json(path)
    if not isinstance(data, list):
        raise ValueError(f"streaming.json must be a list: {path}")
    return data


def encode_stage0_hed(
    streaming_json: list[dict[str, Any]],
    file_entries: list[tuple[int, int]],
    *,
    content_end_for_streams: int | None = None,
) -> bytes:
    """Build STAGE0.HED (fixed 0x1B80 bytes)."""
    if len(streaming_json) > STAGE0_STREAMING_COUNT:
        raise ValueError(
            f"stage0 streaming_list has {len(streaming_json)} entries, "
            f"max {STAGE0_STREAMING_COUNT}"
        )
    if len(file_entries) > STAGE0_FILE_LIST_COUNT:
        raise ValueError(
            f"stage0 file list has {len(file_entries)} entries, "
            f"max {STAGE0_FILE_LIST_COUNT}"
        )

    out = bytearray()

    for i in range(STAGE0_STREAMING_COUNT):
        if i < len(streaming_json):
            entry = streaming_entry_from_json(
                streaming_json[i], new_content_end=content_end_for_streams
            )
            out.extend(encode_streaming_list_entry(entry))
        else:
            out.extend(b"\x00" * 0x28)

    for file_id, file_offset in file_entries:
        out.extend(
            encode_file_list_entry(
                FileListEntry(file_id=file_id, file_offset=file_offset)
            )
        )
    remaining = STAGE0_FILE_LIST_COUNT - len(file_entries)
    out.extend(b"\x00" * (remaining * 8))
    out.extend(encode_u32(FILE_LIST_TERMINATOR) + encode_u32(FILE_LIST_TERMINATOR))

    if len(out) != STAGE0_HED_SIZE:
        raise RuntimeError(
            f"STAGE0.HED size 0x{len(out):X}, expected 0x{STAGE0_HED_SIZE:X}"
        )
    return bytes(out)


def build_files_stage(
    stage_name: str, stage_spec: dict[str, Any], assets_dir: Path, out_dir: Path
) -> tuple[Path, Path]:
    """Rebuild a flat files stage (STAGE0.HED + STAGE0.CDF style)."""
    logging.info("Building %s (files → HED+CDF)", stage_name)

    files_map = stage_spec.get("files")
    if not isinstance(files_map, dict):
        raise TypeError(f"{stage_name}.files must be an ordered dict of file_name → chunks")

    stage_index = (
        parse_int(str(stage_name).replace("stage", ""))
        if str(stage_name).startswith("stage")
        else None
    )

    if stage_index is None:
        raise ValueError(f"cannot parse stage index from {stage_name!r}")

    built_files: list[tuple[int, bytes]] = []
    for file_name, chunks in files_map.items():
        file_id = resolve_file_id(file_name, stage=stage_index, folder_id=None)
        if not isinstance(chunks, (dict, list)):
            raise TypeError(
                f"{stage_name}.files[{file_name!r}] must be a dict of chunk_name → fields"
            )
        raw_dir = resolve_raw_file_dir(
            chunks,
            assets_dir,
            file_name=file_name,
            stage=stage_index,
            folder_id=None,
            stage_rel=stage_name,
        )
        blob = build_file_blob_from_contents(chunks, assets_dir, raw_file_dir=raw_dir)
        built_files.append((file_id, blob))

    raw_stage = assets_dir / "raw" / stage_name
    layout_path = raw_stage / "layout.json"
    retail_offsets: dict[int, int] | None = None
    if layout_path.exists():
        retail_offsets = {
            parse_int(e["file_id"]): parse_int(e["offset"]) for e in load_json(layout_path)
        }

    cdf = bytearray()
    file_list: list[tuple[int, int]] = []
    for file_id, blob in built_files:
        if retail_offsets is not None and file_id in retail_offsets:
            offset = retail_offsets[file_id]
            end = offset + len(blob)
            if end > len(cdf):
                cdf.extend(b"\x00" * (end - len(cdf)))
            # Only write if this extends/fills; overlapping ids share regions.
            cdf[offset : offset + len(blob)] = blob
        else:
            offset = len(cdf)
            cdf.extend(blob)
        file_list.append((file_id, offset))

    content_end = len(cdf)
    stream_path = raw_stage / "stream_data.bin"
    if stream_path.exists():
        cdf.extend(stream_path.read_bytes())

    streaming = load_streaming_json(raw_stage / "streaming.json")
    needs_stream_map = any(
        e.get("offset_folder_space") == "stream"
        or e.get("offset_stage_space") == "stream"
        for e in streaming
    )
    hed = encode_stage0_hed(
        streaming,
        file_list,
        content_end_for_streams=content_end if needs_stream_map else None,
    )

    # STAGE0.HED / STAGE0.CDF naming from stage0, stage1, …
    stage_num = stage_name.replace("stage", "") if stage_name.startswith("stage") else stage_name
    hed_name = f"STAGE{stage_num}.HED"
    cdf_name = f"STAGE{stage_num}.CDF"
    hed_path = out_dir / hed_name
    cdf_path = out_dir / cdf_name
    out_dir.mkdir(parents=True, exist_ok=True)
    hed_path.write_bytes(hed)
    cdf_path.write_bytes(bytes(cdf))
    logging.info(
        "  %s 0x%X bytes, %s 0x%X bytes, %d files",
        hed_name,
        len(hed),
        cdf_name,
        len(cdf),
        len(file_list),
    )
    return hed_path, cdf_path


def build_stage_n_folder(
    folder_id: int,
    files_map: dict[str, Any],
    assets_dir: Path,
    stage_index: int,
) -> tuple[int, bytes]:
    """Build one STAGE-N folder from an ordered files dict."""
    if not isinstance(files_map, dict):
        raise TypeError(
            f"folder {folder_id} must be an ordered dict of file_name → chunks"
        )

    built: list[tuple[int, bytes]] = []
    for file_name, chunks in files_map.items():
        file_id = resolve_file_id(
            file_name, stage=stage_index, folder_id=folder_id
        )
        if not isinstance(chunks, (dict, list)):
            raise TypeError(
                f"folder {folder_id}[{file_name!r}] must be a dict of chunk_name → fields"
            )
        raw_dir = resolve_raw_file_dir(
            chunks,
            assets_dir,
            file_name=file_name,
            stage=stage_index,
            folder_id=folder_id,
        )
        built.append(
            (
                file_id,
                build_file_blob_from_contents(
                    chunks, assets_dir, raw_file_dir=raw_dir
                ),
            )
        )

    raw_folder = assets_dir / "raw" / disk_folder_rel(stage_index, folder_id)
    layout_path = raw_folder / "layout.json"
    retail_offsets: dict[int, int] | None = None
    retail_folder_size: int | None = None
    if layout_path.exists():
        meta = load_json(layout_path)
        if "files" in meta:
            retail_offsets = {
                parse_int(e["file_id"]): parse_int(e["offset"]) for e in meta["files"]
            }
        if "folder_size" in meta:
            retail_folder_size = parse_int(meta["folder_size"])

    body = bytearray()
    file_list: list[FileListEntry] = []
    cursor = STAGE_N_FOLDER_HEADER_SIZE

    for file_id, blob in built:
        if retail_offsets is not None and file_id in retail_offsets:
            offset = retail_offsets[file_id]
        else:
            offset = cursor
        if offset < STAGE_N_FOLDER_HEADER_SIZE:
            raise ValueError(
                f"folder {folder_id} file {file_id} offset 0x{offset:X} "
                f"overlaps folder header"
            )
        body_index = offset - STAGE_N_FOLDER_HEADER_SIZE
        if body_index > len(body):
            body.extend(b"\x00" * (body_index - len(body)))
        end = body_index + len(blob)
        if end > len(body):
            body.extend(b"\x00" * (end - len(body)))
        body[body_index : body_index + len(blob)] = blob
        file_list.append(FileListEntry(file_id=file_id, file_offset=offset))
        cursor = max(cursor, offset + len(blob))

    content_end = STAGE_N_FOLDER_HEADER_SIZE + len(body)

    stream_path = raw_folder / "stream_data.bin"
    if stream_path.exists():
        body.extend(stream_path.read_bytes())

    folder_size = STAGE_N_FOLDER_HEADER_SIZE + len(body)
    if retail_folder_size is not None and retail_folder_size >= folder_size:
        folder_size = retail_folder_size
        body = bytearray(pad_to(body, folder_size - STAGE_N_FOLDER_HEADER_SIZE))

    header = bytearray()
    if len(file_list) > STAGE_N_FILE_LIST_COUNT:
        raise ValueError(
            f"folder {folder_id} has {len(file_list)} files, "
            f"max {STAGE_N_FILE_LIST_COUNT}"
        )
    for entry in file_list:
        header.extend(encode_file_list_entry(entry))
    header.extend(b"\x00" * ((STAGE_N_FILE_LIST_COUNT - len(file_list)) * 8))
    header.extend(encode_u32(0))  # 0x510

    streaming = load_streaming_json(raw_folder / "streaming.json")
    if len(streaming) > STAGE_N_STREAMING_COUNT:
        raise ValueError(
            f"folder {folder_id} has {len(streaming)} stream entries, "
            f"max {STAGE_N_STREAMING_COUNT}"
        )
    for i in range(STAGE_N_STREAMING_COUNT):
        if i < len(streaming):
            entry = streaming_entry_from_json(
                streaming[i], new_content_end=content_end
            )
            header.extend(encode_streaming_list_entry(entry))
        else:
            header.extend(b"\x00" * 0x28)
    header.extend(encode_u32(0))  # 0x7E4

    if len(header) != STAGE_N_FOLDER_HEADER_USED:
        raise RuntimeError(
            f"folder {folder_id} header used size 0x{len(header):X}, "
            f"expected 0x{STAGE_N_FOLDER_HEADER_USED:X}"
        )
    header_sector = pad_to(header, STAGE_N_FOLDER_HEADER_SIZE)

    folder_bytes = header_sector + bytes(body)
    if len(folder_bytes) != folder_size:
        folder_bytes = pad_to(folder_bytes, folder_size)

    if len(folder_bytes) % SECTOR_SIZE != 0:
        folder_bytes = pad_to(folder_bytes, align_up(len(folder_bytes), SECTOR_SIZE))

    return folder_id, folder_bytes


def build_stage_n(
    stage_index: int, stage_spec: dict[str, Any], assets_dir: Path, out_dir: Path
) -> Path:
    """Rebuild STAGEn.CDF from an ordered folders dict."""
    logging.info("Building STAGE%d.CDF", stage_index)

    folders_map = stage_spec.get("folders")
    if not isinstance(folders_map, dict):
        raise TypeError(
            f"stage{stage_index}.folders must be an ordered dict of folder_id → files"
        )

    built_folders: list[tuple[int, bytes]] = []
    for folder_key, files_map in folders_map.items():
        folder_id = resolve_folder_id(str(folder_key), stage=stage_index)
        built_folders.append(
            build_stage_n_folder(folder_id, files_map, assets_dir, stage_index)
        )

    if len(built_folders) > STAGE_N_FOLDER_LIST_COUNT:
        raise ValueError(
            f"stage {stage_index} has {len(built_folders)} folders, "
            f"max {STAGE_N_FOLDER_LIST_COUNT}"
        )

    folder_table = bytearray()
    for folder_id, folder_bytes in built_folders:
        folder_table.extend(
            encode_folder_list_entry(
                FolderListEntry(folder_id=folder_id, folder_size=len(folder_bytes))
            )
        )
    folder_table.extend(
        b"\x00" * ((STAGE_N_FOLDER_LIST_COUNT - len(built_folders)) * 8)
    )
    if len(folder_table) != STAGE_N_FOLDER_LIST_SIZE:
        raise RuntimeError(
            f"folder table size 0x{len(folder_table):X}, "
            f"expected 0x{STAGE_N_FOLDER_LIST_SIZE:X}"
        )

    cdf = bytearray(folder_table)
    for _, folder_bytes in built_folders:
        cdf.extend(folder_bytes)

    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"STAGE{stage_index}.CDF"
    out_path.write_bytes(bytes(cdf))
    logging.info(
        "  STAGE%d.CDF 0x%X bytes, %d folders",
        stage_index,
        len(cdf),
        len(built_folders),
    )
    return out_path


def generated_artifact_path(
    generated: str, built: dict[str, Path], staging_dir: Path
) -> Path:
    """Map a manifest 'generated' key to a built file path."""
    key = generated.lower().replace("-", "_")
    aliases = {
        "stage0_hed": "STAGE0.HED",
        "stage0_cdf": "STAGE0.CDF",
        "stage1_cdf": "STAGE1.CDF",
        "stage2_cdf": "STAGE2.CDF",
        "stage3_cdf": "STAGE3.CDF",
        "stage4_cdf": "STAGE4.CDF",
        "stage5_cdf": "STAGE5.CDF",
    }
    if key in built:
        return built[key]
    if key in aliases:
        path = staging_dir / aliases[key]
        if path.exists():
            return path
    raise KeyError(f"unknown or missing generated artifact: {generated!r}")


def write_disk_layout_xml(
    disk: dict[str, Any],
    disk_dir: Path,
    *,
    image_name: str,
    cue_name: str,
) -> Path:
    """Write layout.xml with sources relative to disk_dir (mkpsxiso style)."""
    track = disk.get("track", disk)
    identifiers = track.get("identifiers", {})
    default_attrs = track.get("default_attributes", {})

    iso_project = ET.Element(
        "iso_project",
        {
            "image_name": image_name,
            "cue_sheet": cue_name,
        },
    )
    track_el = ET.SubElement(
        iso_project,
        "track",
        {
            "type": str(track.get("type", "data")),
            "xa_edc": str(track.get("xa_edc", True)).lower(),
            "new_type": str(track.get("new_type", False)).lower(),
        },
    )

    id_attrs = {
        "system": str(identifiers.get("system", "PLAYSTATION")),
        "application": str(identifiers.get("application", "PLAYSTATION")),
    }
    if "volume" in identifiers:
        id_attrs["volume"] = str(identifiers["volume"])
    if "creation_date" in identifiers:
        id_attrs["creation_date"] = str(identifiers["creation_date"])
    ET.SubElement(track_el, "identifiers", id_attrs)

    license_name = "license_data.dat"
    if (disk_dir / license_name).exists():
        ET.SubElement(track_el, "license", {"file": f"./{license_name}"})

    if default_attrs:
        ET.SubElement(
            track_el,
            "default_attributes",
            {k: str(v) for k, v in default_attrs.items()},
        )

    tree_el = ET.SubElement(track_el, "directory_tree")
    for entry in track.get("directory_tree", disk.get("directory_tree", [])):
        if "dummy_sectors" in entry or entry.get("name") == "dummy":
            attrib = {
                "sectors": str(entry.get("dummy_sectors", entry.get("sectors", 150))),
            }
            if "type" in entry and entry["type"] != "dummy":
                attrib["type"] = str(entry["type"])
            elif "dummy_type" in entry:
                attrib["type"] = str(entry["dummy_type"])
            else:
                attrib["type"] = "0"
            ET.SubElement(tree_el, "dummy", attrib)
            continue

        name = entry["name"]
        ftype = str(entry.get("type", "data"))
        if not (disk_dir / name).exists():
            raise FileNotFoundError(
                f"directory tree file missing in {disk_dir}: {name}"
            )
        ET.SubElement(
            tree_el,
            "file",
            {
                "name": name,
                "source": name,
                "type": ftype,
            },
        )

    xml_path = disk_dir / "layout.xml"
    tree = ET.ElementTree(iso_project)
    ET.indent(tree, space="    ")
    tree.write(xml_path, encoding="utf-8", xml_declaration=True)
    logging.info("Wrote %s", xml_path)
    return xml_path


def materialize_disk_tree(
    disk: dict[str, Any],
    *,
    rom_dir: Path,
    staging_dir: Path,
    built: dict[str, Path],
    disk_dir: Path,
) -> Path:
    """Populate disk_dir with every ISO file + license + layout.xml."""
    track = disk.get("track", disk)
    disk_dir.mkdir(parents=True, exist_ok=True)

    # License
    license_rel = track.get("license", disk.get("license"))
    if license_rel:
        license_src = resolve_path(rom_dir, license_rel)
        if not license_src.exists():
            # Also accept license already beside the disk sources
            alt = rom_dir / disk.get("name", "") / "license_data.dat"
            if alt.exists():
                license_src = alt
        if license_src.exists():
            install_file(license_src, disk_dir / "license_data.dat")
        else:
            logging.warning("license file not found: %s", license_rel)

    for entry in track.get("directory_tree", disk.get("directory_tree", [])):
        if "dummy_sectors" in entry or entry.get("name") == "dummy":
            continue

        name = entry["name"]
        if "generated" in entry:
            source = generated_artifact_path(entry["generated"], built, staging_dir)
        elif "source" in entry:
            source = resolve_path(rom_dir, entry["source"])
            if not source.exists():
                alt = resolve_path(staging_dir, entry["source"])
                if alt.exists():
                    source = alt
        else:
            raise ValueError(f"directory entry {name!r} needs source or generated")

        if not source.exists():
            raise FileNotFoundError(f"ISO source missing for {name}: {source}")

        dest = disk_dir / name
        if source.resolve() != dest.resolve():
            install_file(source, dest)
        logging.info("  %s <- %s", dest, source)

    name = disk.get("name", "disk")
    image_name = disk.get("image_name", f"{name}.bin")
    cue_name = disk.get("cue_sheet", f"{name}.cue")
    write_disk_layout_xml(
        disk,
        disk_dir,
        image_name=Path(image_name).name,
        cue_name=Path(cue_name).name,
    )
    return disk_dir


def build_stage_containers(
    stages_manifest: dict[str, Any], assets_dir: Path, out_dir: Path
) -> dict[str, Path]:
    """Build STAGE*.HED/CDF from a flat stage map into out_dir."""
    out_dir.mkdir(parents=True, exist_ok=True)
    built: dict[str, Path] = {}

    # Support legacy nested shape once: { "stage0": …, "stages": { "1": … } }
    if "stages" in stages_manifest and isinstance(stages_manifest["stages"], dict):
        legacy = dict(stages_manifest)
        nested = legacy.pop("stages")
        flat: dict[str, Any] = {}
        if "stage0" in legacy:
            flat["stage0"] = legacy["stage0"]
        for k, v in nested.items():
            name = k if str(k).startswith("stage") else f"stage{k}"
            flat[name] = v
        stages_manifest = flat

    for stage_name, stage_spec in stages_manifest.items():
        if not isinstance(stage_spec, dict):
            continue
        if not str(stage_name).startswith("stage"):
            # Skip non-stage keys if any
            continue

        has_files = "files" in stage_spec
        has_folders = "folders" in stage_spec
        if has_files and has_folders:
            raise ValueError(
                f"{stage_name} must have either 'files' or 'folders', not both"
            )
        if not has_files and not has_folders:
            raise ValueError(f"{stage_name} needs 'files' or 'folders'")

        if has_files:
            hed, cdf = build_files_stage(
                str(stage_name), stage_spec, assets_dir, out_dir
            )
            built["stage0_hed" if stage_name == "stage0" else f"{stage_name}_hed"] = hed
            built["stage0_cdf" if stage_name == "stage0" else f"{stage_name}_cdf"] = cdf
            built[hed.name] = hed
            built[cdf.name] = cdf
            # Keep generated-key aliases used by ISO manifests
            if stage_name == "stage0":
                built["stage0_hed"] = hed
                built["stage0_cdf"] = cdf
        else:
            stage_index = parse_int(str(stage_name).replace("stage", ""))
            path = build_stage_n(stage_index, stage_spec, assets_dir, out_dir)
            built[f"stage{stage_index}_cdf"] = path
            built[f"STAGE{stage_index}.CDF"] = path

    return built


def pack(
    stages_manifest: dict[str, Any],
    assets_dir: Path,
    rom_dir: Path,
    output_dir: Path,
    *,
    iso_manifest: dict[str, Any] | None = None,
    stage_dir: Path | None = None,
):
    """Build HED/CDF from stages.json; optionally materialize one ISO tree.

    Parameters
    ----------
    stages_manifest:
        Content of ``stages.json`` (stage0 + stages only).
    iso_manifest:
        Optional single-disc ISO manifest (``iso_diskN.json``). When set,
        ``output_dir`` becomes that disc's directory tree.
    stage_dir:
        Where to write/read built STAGE*.HED/CDF. Defaults to a temporary
        ``.staging`` under ``output_dir`` when materializing a tree, or to
        ``output_dir`` itself when only building containers.
    """
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    if iso_manifest is None:
        # Stages only: write containers straight into output_dir.
        containers_dir = stage_dir or output_dir
        build_stage_containers(stages_manifest, assets_dir, containers_dir)
        logging.info("Stage containers written to %s", containers_dir)
        return

    # One directory tree: build stages into staging (or reuse --stage-dir),
    # then materialize output_dir as the disc tree.
    if stage_dir is not None:
        containers_dir = stage_dir.resolve()
        if not any(containers_dir.glob("STAGE*")):
            build_stage_containers(stages_manifest, assets_dir, containers_dir)
        else:
            logging.info("Reusing stage containers in %s", containers_dir)
        built: dict[str, Path] = {}
        for path in containers_dir.iterdir():
            if path.is_file():
                built[path.name] = path
                key = path.name.lower().replace(".", "_")
                if key.endswith("_hed") or key.endswith("_cdf"):
                    built[key] = path
                # also stage0_hed style
                if path.name == "STAGE0.HED":
                    built["stage0_hed"] = path
                elif path.name == "STAGE0.CDF":
                    built["stage0_cdf"] = path
                elif path.name.startswith("STAGE") and path.name.endswith(".CDF"):
                    num = path.name[len("STAGE") : -len(".CDF")]
                    built[f"stage{num}_cdf"] = path
        cleanup_staging = False
    else:
        containers_dir = output_dir / ".staging"
        built = build_stage_containers(stages_manifest, assets_dir, containers_dir)
        cleanup_staging = True

    logging.info("Materializing ISO directory tree %s", output_dir)
    materialize_disk_tree(
        iso_manifest,
        rom_dir=rom_dir,
        staging_dir=containers_dir,
        built=built,
        disk_dir=output_dir,
    )

    if cleanup_staging:
        shutil.rmtree(containers_dir, ignore_errors=True)
    logging.info("ISO directory tree written to %s", output_dir)


def main(argv: list[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    parser = ArgumentParser(description=__doc__)
    parser.add_argument(
        "--stages",
        "-s",
        type=Path,
        required=True,
        help="Path to stages.json (HED/CDF content tree)",
    )
    parser.add_argument(
        "--iso",
        "-i",
        type=Path,
        help="Path to a single iso_diskN.json (materialize one directory tree)",
    )
    parser.add_argument(
        "--assets",
        "-a",
        type=Path,
        help="Asset root (raw/stage0/, raw/stage1/, ...). Defaults to stages.json parent",
    )
    parser.add_argument(
        "--rom",
        "-r",
        type=Path,
        help="ROM root with disk1/ disk2/ passthrough files (SYSTEM.CNF, INTER*.STR, license, ...)",
    )
    parser.add_argument(
        "--stage-dir",
        type=Path,
        help="Directory for built STAGE*.HED/CDF (default: <output> or <output>/.staging)",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        help="Output dir: stage containers (no --iso) or one disc tree (with --iso)",
    )
    args = parser.parse_args(argv)

    stages_path = args.stages.resolve()
    stages_manifest = load_json(stages_path)

    # Reject legacy combined manifests early.
    if "disks" in stages_manifest or "directory_tree" in stages_manifest:
        logging.error(
            "%s looks like a combined/ISO manifest. "
            "Pass HED/CDF structure via --stages (stages.json) and a single disc "
            "layout via --iso (iso_diskN.json).",
            stages_path,
        )
        return 1

    assets_dir = (args.assets or stages_path.parent).resolve()
    if args.rom:
        rom_dir = args.rom.resolve()
    else:
        guess = assets_dir.parent.parent / "rom" / assets_dir.name
        rom_dir = guess if guess.exists() else assets_dir

    iso_manifest = None
    if args.iso:
        iso_path = args.iso.resolve()
        iso_manifest = load_json(iso_path)
        if "disks" in iso_manifest:
            logging.error(
                "%s contains multiple disks; pass one iso_diskN.json per invocation",
                iso_path,
            )
            return 1
        if "track" not in iso_manifest and "directory_tree" not in iso_manifest:
            logging.error("%s is missing track/directory_tree", iso_path)
            return 1

    if args.output:
        output_dir = args.output.resolve()
    elif iso_manifest is not None:
        disk_name = iso_manifest.get("name")
        if not disk_name:
            raise SystemExit("ISO manifest missing required 'name' (e.g. disk1)")
        output_dir = Path("build") / assets_dir.name / "rom" / str(disk_name)
    else:
        output_dir = Path("build") / assets_dir.name / "rom" / "stages"

    pack(
        stages_manifest,
        assets_dir,
        rom_dir,
        output_dir,
        iso_manifest=iso_manifest,
        stage_dir=args.stage_dir.resolve() if args.stage_dir else None,
    )
    logging.info("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
