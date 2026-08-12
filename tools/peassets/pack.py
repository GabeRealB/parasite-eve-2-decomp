#!/usr/bin/env python3
"""Build HED/CDF containers and one ISO directory tree from JSON manifests.

Inputs (two separate manifests):

  * ``stages.json`` — each file lists **contents** (asset paths + pack fields).
  * ``iso_diskN.json`` — single-disc ISO layout (one directory_tree).

Example ``stages.json`` (flat stage map; each stage is files *or* folders)::

    {
      "stage0": {
        "files": {
          "gameplay": {
            "0.spk": { "path": "spk/spk_0.spk", "type": "music" },
            "1.pe2pkg": {
              "path": "pe2pkg/gameplay.pe2pkg",
              "type": "room_pkg",
              "load_addr": "0x80093800"
            }
          }
        }
      },
      "stage1": {
        "folders": {
          "101": {
            "file0": {
              "0.spk": { "path": "spk/spk_12.spk", "type": "music" }
            }
          }
        }
      }
    }

``files`` → STAGE0-style HED+CDF. ``folders`` → STAGEn.CDF with folder table.
File/folder dict keys use friendly names from ``names.NAMES`` when set (else
``file0`` / ``101``). Chunk keys stay disc-order basenames (``1.pe2pkg``).
Key order is on-disc order.

Content paths point at the **type store** under the assets root
(``pe2pkg/``, ``pe2img/*.png``, …) — inflated/editable forms from extract.
Pack re-encodes LZSS on-disc types (room packages, images, CLUTs) from those
assets. Pack sidecars (``trailer.bin``, ``layout.json``, …) live under
``stage0/…`` / ``stageN/…``.
"""

from __future__ import annotations

import json
import logging
import shutil
import sys
import xml.etree.ElementTree as ET
from argparse import ArgumentParser
from enum import Enum
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
    ensure_clean_payload,
    pack_chunk_payload,
    parse_int,
    resolve_chunk_type,
    resolve_end_flag,
    streaming_entry_from_json,
    validate_sector_len,
)
from lzss import encode_lzss  # noqa: E402
from names import (  # noqa: E402
    disk_file_rel,
    disk_folder_rel,
    resolve_file_id,
    resolve_folder_id,
)

# Chunk types stored as LZSS (or LZSS-wrapped) on disc.
LZSS_ON_DISC_TYPES = frozenset(
    {
        FileChunkType.RoomPkg,
        FileChunkType.Image,
        FileChunkType.Clut,
    }
)


class PackSource(str, Enum):
    """Pack source mode.

    * **matching** (default) — on-disc blobs from ``raw/{type}/`` when present
      (LZSS types, ``.bs`` backgrounds, etc.); other assets from inflated dirs.
    * **raw** — only ``raw/{type}/`` (on-disc blobs; no re-encode).
    * **hybrid** / **decoded** — inflated type dirs; re-encode LZSS / BS
      types from PNG (BS re-encode is lossy).
    """

    MATCHING = "matching"
    RAW = "raw"
    HYBRID = "hybrid"
    DECODED = "decoded"


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def resolve_path(base: Path, value: str | Path) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return (base / path).resolve()


def path_under_raw_type_store(
    assets_dir: Path, path_str: str, chunk_type: FileChunkType
) -> Path:
    """Map an inflated type-store path to the corresponding ``raw/{type}/`` file.

    ``pe2pkg/gameplay.pe2pkg`` → ``raw/pe2pkg/gameplay.pe2pkg``
    ``pe2img/foo.png`` → ``raw/pe2img/foo.pe2img``
    """
    p = Path(path_str)
    # Already under raw/
    if p.parts and p.parts[0] == "raw":
        return resolve_path(assets_dir, p)
    type_dir = AssetStore_type_dir(chunk_type)
    stem = p.stem
    ext = chunk_type.get_extension()
    return (assets_dir / "raw" / type_dir / f"{stem}{ext}").resolve()


def AssetStore_type_dir(chunk_type: FileChunkType) -> str:
    """Type directory name for a chunk type (mirrors extract.TYPE_DIR_BY_EXT)."""
    ext = chunk_type.get_extension()
    return {
        ".pe2pkg": "pe2pkg",
        ".pe2img": "pe2img",
        ".pe2clut": "pe2clut",
        ".pe2cap2": "pe2cap2",
        ".bs": "bs",
        ".spk": "spk",
        ".txt": "txt",
    }.get(ext, ext.lstrip(".") or "bin")


def resolve_content_path(
    assets_dir: Path,
    path_str: str,
    *,
    source: PackSource,
    chunk_type: FileChunkType,
) -> tuple[Path, bool]:
    """Resolve a stages content path.

    Returns ``(path, on_disc_blob)``. When ``on_disc_blob`` is True the file is
    already a clean on-disc payload (no LZSS/PNG re-encode).
    """
    needs_lzss = chunk_type in LZSS_ON_DISC_TYPES
    # Prefer retail raw for LZSS types and BS (re-encode is lossy).
    prefer_raw = needs_lzss or chunk_type == FileChunkType.RoomBackground
    staged = resolve_path(assets_dir, path_str)

    if source == PackSource.RAW:
        raw_path = path_under_raw_type_store(assets_dir, path_str, chunk_type)
        if not raw_path.exists():
            raise FileNotFoundError(f"missing raw content: {raw_path}")
        return raw_path, True

    if source == PackSource.MATCHING and prefer_raw:
        raw_path = path_under_raw_type_store(assets_dir, path_str, chunk_type)
        if raw_path.exists():
            return raw_path, True
        # Fall through to inflated re-encode if raw is missing.
        logging.warning(
            "matching pack: missing raw %s; re-encoding from inflated", raw_path
        )

    # hybrid / decoded / matching non-LZSS / matching LZSS/BS fallback
    path = staged
    if not path.exists() and chunk_type in (
        FileChunkType.Image,
        FileChunkType.Clut,
        FileChunkType.RoomBackground,
    ):
        for alt in (path.with_suffix(".png"), path):
            if alt.exists():
                path = alt
                break
    if not path.exists():
        raise FileNotFoundError(f"missing content: {path}")

    suffix = path.suffix.lower()
    if chunk_type == FileChunkType.RoomPkg:
        return path, False
    if chunk_type in (FileChunkType.Image, FileChunkType.Clut):
        return path, suffix in (".pe2img", ".pe2clut")
    if chunk_type == FileChunkType.RoomBackground:
        return path, suffix == ".bs"
    return path, False


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
    """Build one on-disc chunk with per-sector layout honoring sector_len.

    Valid data per sector is ``[0x10, sector_len)`` on the first sector and
    ``[0, sector_len)`` on continuation sectors; the rest of each 0x800 sector
    is zero pad (see ``format.pack_chunk_payload``).
    """
    chunk_type = resolve_chunk_type(header_json["chunk_type"])
    end_flag = resolve_end_flag(header_json["end_flag"])
    sector_len = validate_sector_len(
        parse_int(
            header_json.get("sector_len", header_json.get("unknown1", SECTOR_SIZE))
        )
    )
    load_addr = parse_int(header_json.get("load_addr", 0))

    # Prefer retail chunk_size when present so unmodified assets stay exact.
    retail_size = None
    if "chunk_size" in header_json:
        retail_size = parse_int(header_json["chunk_size"])

    header = FileChunkHeader(
        chunk_type=chunk_type,
        end_flag=end_flag,
        sector_len=sector_len,
        chunk_size=retail_size or SECTOR_SIZE,  # placeholder; pack recalculates
        load_addr=load_addr,
    )
    return pack_chunk_payload(header, payload, chunk_size=retail_size)


def _content_chunk_sizes(
    content: dict[str, Any] | None = None,
) -> tuple[int, int | None]:
    """Resolve (sector_len, chunk_size) from stages.json content fields."""
    sector_len = 0x800
    chunk_size = None
    if content:
        if "sector_len" in content:
            sector_len = parse_int(content["sector_len"])
        if "chunk_size" in content:
            chunk_size = parse_int(content["chunk_size"]) or None
    return sector_len, chunk_size


def prepare_on_disc_payload(
    payload: bytes,
    *,
    chunk_type: FileChunkType,
    content_path: Path | None = None,
    content: dict[str, Any] | None = None,
    on_disc_blob: bool = False,
) -> bytes:
    """Turn an asset into the on-disc clean chunk payload.

    When ``on_disc_blob`` is True, ``payload`` is already clean pe2/raw on-disc
    data (pass-through pe2 image) — only strip legacy sector pad if needed.

    Otherwise:

    * Room packages: LZSS-encode the inflated package
    * Images / CLUTs: encode PNG + meta (or pass-through pe2)
    * Ascii: strip trailing NULs; sector zero-fill restores retail pad
    * Other types: clean bytes as-is

    Returned payload is always *clean* (no per-sector pad);
    ``build_chunk_blob`` re-applies ``sector_len`` padding.
    """
    sector_len, chunk_size = _content_chunk_sizes(content)

    if on_disc_blob:
        if not payload and content_path is not None and content_path.exists():
            payload = content_path.read_bytes()
        if not payload:
            raise FileNotFoundError(
                f"missing on-disc payload ({content_path})"
            )
        return ensure_clean_payload(
            payload, sector_len=sector_len, chunk_size=chunk_size
        )

    if chunk_type == FileChunkType.RoomPkg:
        if not payload:
            raise FileNotFoundError(
                f"room package missing decoded payload ({content_path})"
            )
        return encode_lzss(payload)

    # Note: Image/Clut re-encode paths below; CLUT PNG encode uses encode_lzss(kind="clut")
    # inside image_codec.

    if chunk_type in (FileChunkType.Image, FileChunkType.Clut):
        return _prepare_image_payload(
            chunk_type=chunk_type,
            content_path=content_path,
            decoded_payload=payload,
            sector_len=sector_len,
            chunk_size=chunk_size,
        )

    if chunk_type == FileChunkType.RoomBackground:
        return _prepare_bs_payload(
            content_path=content_path,
            decoded_payload=payload,
            sector_len=sector_len,
            chunk_size=chunk_size,
        )

    if not payload and content_path is not None:
        raise FileNotFoundError(f"missing payload: {content_path}")

    if chunk_type == FileChunkType.Ascii:
        # Inflated form is text only; strip any trailing NULs. Sector pack
        # zero-fills the rest of the clean payload / chunk.
        end = len(payload)
        while end > 0 and payload[end - 1] == 0:
            end -= 1
        return payload[:end]

    return ensure_clean_payload(
        payload, sector_len=sector_len, chunk_size=chunk_size
    )


def _image_meta_candidates(
    content_path: Path, chunk_type: FileChunkType
) -> list[Path]:
    stem_meta = content_path.with_suffix("")
    if chunk_type == FileChunkType.Image:
        return [
            Path(str(stem_meta) + ".pe2img.json"),
            content_path.with_name(content_path.stem + ".pe2img.json"),
        ]
    return [
        Path(str(stem_meta) + ".pe2clut.json"),
        content_path.with_name(content_path.stem + ".pe2clut.json"),
    ]


def _encode_image_from_png(
    content_path: Path, chunk_type: FileChunkType
) -> bytes | None:
    """Encode PNG + meta to pe2 payload, or None if meta is missing."""
    if content_path.suffix.lower() != ".png":
        return None
    # Lazy import: matching/raw packs do not need Pillow.
    from image_codec import png_files_to_pe2clut, png_files_to_pe2img

    for mp in _image_meta_candidates(content_path, chunk_type):
        if mp.exists():
            if chunk_type == FileChunkType.Image:
                return png_files_to_pe2img(content_path, mp)
            return png_files_to_pe2clut(content_path, mp)
    return None


def _prepare_image_payload(
    *,
    chunk_type: FileChunkType,
    content_path: Path | None,
    decoded_payload: bytes,
    sector_len: int,
    chunk_size: int | None,
) -> bytes:
    """PNG (+ meta) or opaque pe2 bytes → clean pe2 payload."""
    if content_path is not None:
        encoded = _encode_image_from_png(content_path, chunk_type)
        if encoded is not None:
            return encoded
        if content_path.suffix.lower() == ".png":
            candidates = _image_meta_candidates(content_path, chunk_type)
            raise FileNotFoundError(
                f"missing image meta for {content_path} (tried {candidates})"
            )
        # Recovery: pe2 blob left in the type store when PNG decode failed.
        if content_path.suffix.lower() in (".pe2img", ".pe2clut"):
            return ensure_clean_payload(
                content_path.read_bytes(),
                sector_len=sector_len,
                chunk_size=chunk_size,
            )

    if decoded_payload:
        return ensure_clean_payload(
            decoded_payload, sector_len=sector_len, chunk_size=chunk_size
        )
    raise FileNotFoundError(
        f"missing image payload ({content_path})"
    )


def _prepare_bs_payload(
    *,
    content_path: Path | None,
    decoded_payload: bytes,
    sector_len: int,
    chunk_size: int | None,
) -> bytes:
    """PNG (+ meta) or raw ``.bs`` → clean BS payload."""
    if content_path is not None:
        suf = content_path.suffix.lower()
        if suf == ".png":
            from asset_decode import encode_bs_from_png_path

            encoded = encode_bs_from_png_path(content_path)
            if encoded is None:
                raise FileNotFoundError(
                    f"failed to encode BS from {content_path}"
                )
            return ensure_clean_payload(
                encoded, sector_len=sector_len, chunk_size=chunk_size
            )
        if suf == ".bs":
            return ensure_clean_payload(
                content_path.read_bytes(),
                sector_len=sector_len,
                chunk_size=chunk_size,
            )
    if decoded_payload:
        return ensure_clean_payload(
            decoded_payload, sector_len=sector_len, chunk_size=chunk_size
        )
    raise FileNotFoundError(f"missing BS payload ({content_path})")


def build_file_blob_from_contents(
    contents: dict[str, dict[str, Any]] | list[dict[str, Any]],
    assets_dir: Path,
    *,
    file_dir: Path | None,
    source: PackSource = PackSource.MATCHING,
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
        path_str = content.get("path")
        if not path_str:
            raise ValueError("type=raw content missing path")
        path = resolve_path(assets_dir, path_str)
        if not path.exists():
            raise FileNotFoundError(f"missing raw.bin payload: {path}")
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
        header_json: dict[str, Any] = {
            "chunk_type": chunk_type.get_name(),
            "end_flag": end_flag,
            "sector_len": sector_len,
            "load_addr": content.get("load_addr", "0x0"),
        }
        if "chunk_size" in content:
            header_json["chunk_size"] = content["chunk_size"]
        if "path" in content:
            payload_path, on_disc_blob = resolve_content_path(
                assets_dir,
                content["path"],
                source=source,
                chunk_type=chunk_type,
            )
        else:
            # Derive path from name when omitted: file_dir/name
            if file_dir is None:
                raise ValueError(f"content {name!r} missing path and file_dir")
            payload_path = file_dir / name
            on_disc_blob = False
            if not payload_path.exists():
                if chunk_type in (FileChunkType.Image, FileChunkType.Clut):
                    alt = payload_path.with_suffix(".png")
                    if alt.exists():
                        payload_path = alt
                if not payload_path.exists():
                    raise FileNotFoundError(
                        f"missing content payload: {payload_path}"
                    )
        # PNG is not the on-disc payload; prepare_on_disc_payload encodes it.
        if payload_path.suffix.lower() == ".png":
            file_bytes = b""
        else:
            file_bytes = payload_path.read_bytes()
        on_disc = prepare_on_disc_payload(
            file_bytes,
            chunk_type=chunk_type,
            content_path=payload_path,
            content=content if isinstance(content, dict) else None,
            on_disc_blob=on_disc_blob,
        )
        out.extend(build_chunk_blob(header_json, on_disc))

    # Optional post-chunk trailer next to the file (raw or decoded per source).
    if file_dir is not None:
        trailer_path = file_dir / "trailer.bin"
        if trailer_path.exists():
            out.extend(trailer_path.read_bytes())

    return pad_to(bytes(out), align_up(len(out), SECTOR_SIZE))


def resolve_file_dir(
    contents: dict[str, dict[str, Any]] | list[dict[str, Any]],
    assets_dir: Path,
    *,
    file_name: str,
    stage: int | None = None,
    folder_id: int | None = None,
    stage_rel: str | None = None,
    source: PackSource = PackSource.MATCHING,
) -> Path:
    """Infer stage-tree directory for trailers / sidecars.

    Sidecars live under ``stage0/…`` / ``stageN/…`` (not type dirs). Prefer
    :func:`disk_file_rel` when stage/file ids are known.
    """
    del contents, source  # content paths point at type store, not stage tree
    if stage is not None:
        try:
            file_id = resolve_file_id(file_name, stage=stage, folder_id=folder_id)
        except ValueError:
            file_id = None
        if file_id is not None:
            return assets_dir / disk_file_rel(stage, file_id, folder_id)
    if stage_rel is not None:
        return assets_dir / stage_rel / file_name
    return assets_dir / "stage0" / file_name


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
    stage_name: str,
    stage_spec: dict[str, Any],
    assets_dir: Path,
    out_dir: Path,
    *,
    source: PackSource = PackSource.MATCHING,
) -> tuple[Path, Path]:
    """Rebuild a flat files stage (STAGE0.HED + STAGE0.CDF style)."""
    logging.info("Building %s (files → HED+CDF) source=%s", stage_name, source.value)

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
        file_dir = resolve_file_dir(
            chunks,
            assets_dir,
            file_name=file_name,
            stage=stage_index,
            folder_id=None,
            stage_rel=stage_name,
            source=source,
        )
        blob = build_file_blob_from_contents(
            chunks, assets_dir, file_dir=file_dir, source=source
        )
        built_files.append((file_id, blob))

    stage_side = assets_dir / stage_name
    layout_path = stage_side / "layout.json"
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
    stream_path = stage_side / "stream_data.bin"
    if stream_path.exists():
        cdf.extend(stream_path.read_bytes())

    streaming = load_streaming_json(stage_side / "streaming.json")
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
    *,
    source: PackSource = PackSource.MATCHING,
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
        file_dir = resolve_file_dir(
            chunks,
            assets_dir,
            file_name=file_name,
            stage=stage_index,
            folder_id=folder_id,
            source=source,
        )
        built.append(
            (
                file_id,
                build_file_blob_from_contents(
                    chunks,
                    assets_dir,
                    file_dir=file_dir,
                    source=source,
                ),
            )
        )

    folder_side = assets_dir / disk_folder_rel(stage_index, folder_id)
    layout_path = folder_side / "layout.json"
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

    stream_path = folder_side / "stream_data.bin"
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

    streaming = load_streaming_json(folder_side / "streaming.json")
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
    stage_index: int,
    stage_spec: dict[str, Any],
    assets_dir: Path,
    out_dir: Path,
    *,
    source: PackSource = PackSource.MATCHING,
) -> Path:
    """Rebuild STAGEn.CDF from an ordered folders dict."""
    logging.info("Building STAGE%d.CDF source=%s", stage_index, source.value)

    folders_map = stage_spec.get("folders")
    if not isinstance(folders_map, dict):
        raise TypeError(
            f"stage{stage_index}.folders must be an ordered dict of folder_id → files"
        )

    built_folders: list[tuple[int, bytes]] = []
    for folder_key, files_map in folders_map.items():
        folder_id = resolve_folder_id(str(folder_key), stage=stage_index)
        built_folders.append(
            build_stage_n_folder(
                folder_id,
                files_map,
                assets_dir,
                stage_index,
                source=source,
            )
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
    stages_manifest: dict[str, Any],
    assets_dir: Path,
    out_dir: Path,
    *,
    source: PackSource = PackSource.MATCHING,
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
                str(stage_name),
                stage_spec,
                assets_dir,
                out_dir,
                source=source,
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
            path = build_stage_n(
                stage_index,
                stage_spec,
                assets_dir,
                out_dir,
                source=source,
            )
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
    source: PackSource = PackSource.MATCHING,
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
    source:
        Asset source mode (``matching`` / ``raw`` / ``hybrid`` / ``decoded``).
    """
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    logging.info("Pack source mode: %s", source.value)

    if iso_manifest is None:
        # Stages only: write containers straight into output_dir.
        containers_dir = stage_dir or output_dir
        build_stage_containers(
            stages_manifest, assets_dir, containers_dir, source=source
        )
        logging.info("Stage containers written to %s", containers_dir)
        return

    # One directory tree: build stages into staging (or reuse --stage-dir),
    # then materialize output_dir as the disc tree.
    if stage_dir is not None:
        containers_dir = stage_dir.resolve()
        if not any(containers_dir.glob("STAGE*")):
            build_stage_containers(
                stages_manifest, assets_dir, containers_dir, source=source
            )
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
        built = build_stage_containers(
            stages_manifest, assets_dir, containers_dir, source=source
        )
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
        help="Asset root (pe2pkg/, pe2img/, stage0/, …). Defaults to stages.json parent",
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
    parser.add_argument(
        "--source",
        choices=[s.value for s in PackSource],
        default=PackSource.MATCHING.value,
        help=(
            "Asset source mode (default: matching). "
            "matching = raw/{type}/ LZSS blobs + inflated non-LZSS. "
            "raw = only raw/{type}/. "
            "hybrid/decoded = inflated type dirs (re-encode LZSS)."
        ),
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
        source=PackSource(args.source),
    )
    logging.info("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
