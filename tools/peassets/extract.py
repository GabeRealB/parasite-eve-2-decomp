import hashlib
import json
import logging
import shutil
import sys
import traceback
from argparse import ArgumentParser, FileType
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import BinaryIO
from zlib import crc32

# Allow sibling imports when invoked as `python tools/peassets/extract.py`
_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from format import validate_sector_len  # noqa: E402
from asset_decode import (  # noqa: E402
    decode_ascii_payload,
    decode_pe2pkg_payload,
    materialize_bs_asset,
    materialize_image_asset,
    materialize_spk_asset,
)
from names import (  # noqa: E402
    REQUIRED_OVERLAY_STEMS,
    asset_name_key,
    chunk_key,
    chunk_path_key,
    disk_file_rel,
    lookup,
    validate_names,
)
from parallel_util import default_jobs, run_jobs  # noqa: E402


@dataclass
class FolderListEntry:
    folder_id: int
    folder_size: int


@dataclass
class FileListEntry:
    file_id: int
    file_offset: int  # offset in .CDF (from begin of current folder)


class FileChunkType(IntEnum):
    RoomPkg = 0x0  # .pe2pkg
    Image = 0x1  # .pe2img
    Clut = 0x2  # .pe2clut
    Cap2 = 0x4  # .pe2cap2
    RoomBackground = 0x5  # .bs
    Music = 0x6  # .spk
    Ascii = 0x7  # .txt

    def get_name(self) -> str:
        return {
            FileChunkType.RoomPkg: "Room package ",
            FileChunkType.Image: "Image",
            FileChunkType.Clut: "Color lookup table",
            FileChunkType.Cap2: "Dialogue data",
            FileChunkType.RoomBackground: "PSX BS v2 MDEC bitstream image",
            FileChunkType.Music: "SPK/MPK music program",
            FileChunkType.Ascii: "Ascii text",
        }[self]

    def get_extension(self) -> str:
        return {
            FileChunkType.RoomPkg: ".pe2pkg",
            FileChunkType.Image: ".pe2img",
            FileChunkType.Clut: ".pe2clut",
            FileChunkType.Cap2: ".pe2cap2",
            FileChunkType.RoomBackground: ".bs",
            FileChunkType.Music: ".spk",
            FileChunkType.Ascii: ".txt",
        }[self]

    def is_compressed(self) -> bool:
        # On disc, room packages are LZSS-compressed; other chunk types are raw.
        return {
            FileChunkType.RoomPkg: True,
            FileChunkType.Image: False,
            FileChunkType.Clut: False,
            FileChunkType.Cap2: False,
            FileChunkType.RoomBackground: False,
            FileChunkType.Music: False,
            FileChunkType.Ascii: False,
        }[self]


class FileChunkEndFlag(IntEnum):
    Continue = 0x01
    End = 0xFF


@dataclass
class FileChunkHeader:
    chunk_type: FileChunkType
    end_flag: FileChunkEndFlag
    sector_len: int  # exclusive end offset in sector buffer; range (0x10, 0x800]
    chunk_size: int
    load_addr: int  # RAM dest for room_pkg / cap2; 0 otherwise

    def encode(self) -> dict:
        return {
            "chunk_type": self.chunk_type.get_name(),
            "end_flag": "continue"
            if self.end_flag == FileChunkEndFlag.Continue
            else "end",
            "sector_len": f"0x{self.sector_len:X}",
            "chunk_size": f"0x{self.chunk_size:X}",
            "load_addr": f"0x{self.load_addr:X}",
        }


@dataclass
class FileChunk:
    header: FileChunkHeader
    data: bytes


@dataclass
class File:
    chunks: list[FileChunk]


class StreamingListStreamType(IntEnum):
    Movie = 0x1
    Audio = 0x2


@dataclass
class StreamingListMovieEntry:
    stream_type: StreamingListStreamType
    unknown1: int
    offset_folder: int  # offset in current Folder of .CDF file (if movie_number == 0)
    offset_inter: int  # offset in INTERx.STR file (if movie_number > 0)
    unknown2: int
    stream_id: int
    stream_sub_id: int
    picture_width: int
    picture_height: int
    unknown3: int
    unknown4: int
    unknown5: int
    unknown6: int
    unknown7: int
    movie_number: int
    unknown8: int


@dataclass
class StreamingListAudioEntry:
    stream_type: StreamingListStreamType
    unknown1: int
    offset_stage: int  # offset in STAGEx.CDF file (increasing offsets)
    unknown2: int
    stage_number: int
    stream_id: int
    stream_sub_id: int
    unknown3: int
    unknown4: int
    unknown5: int
    unknown6: int
    unknown7: int


FOLDER_LIST_ENTRY_SIZE = 0x8
FILE_LIST_ENTRY_SIZE = 0x8
FILE_CHUNK_HEADER_SIZE = 0x10
STREAMING_LIST_ENTRY_SIZE = 0x28

# Asset path names live in names.py (NAMES).
# Canonical keys use disc ids (stage0/file0/1.pe2pkg); type-store stems use
# friendly names when set (pe2pkg/gameplay.pe2pkg).


def computeChecksum(exe: BinaryIO):
    exe.seek(0)
    return crc32(exe.read(4096))


def parse_folder_list_entry(data: bytes) -> FolderListEntry:
    assert len(data) == FOLDER_LIST_ENTRY_SIZE
    return FolderListEntry(
        folder_id=int.from_bytes(data[0x0:0x4], byteorder="little"),
        folder_size=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
    )


def parse_file_list_entry(data: bytes) -> FileListEntry:
    assert len(data) == FILE_LIST_ENTRY_SIZE
    return FileListEntry(
        file_id=int.from_bytes(data[0x0:0x4], byteorder="little"),
        file_offset=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
    )


def parse_file_chunk_header(data: bytes) -> FileChunkHeader | None:
    assert len(data) == FILE_CHUNK_HEADER_SIZE
    chunk_type = int.from_bytes(data[0x0:0x1], byteorder="little")
    match chunk_type:
        case (
            FileChunkType.RoomPkg
            | FileChunkType.Image
            | FileChunkType.Clut
            | FileChunkType.Cap2
            | FileChunkType.RoomBackground
            | FileChunkType.Music
            | FileChunkType.Ascii
        ):
            pass
        case _:
            return None

    assert int.from_bytes(data[0xC:0x10], byteorder="little") == 0
    return FileChunkHeader(
        chunk_type=FileChunkType(chunk_type),
        end_flag=FileChunkEndFlag.from_bytes(data[0x1:0x2], byteorder="little"),
        sector_len=validate_sector_len(
            int.from_bytes(data[0x2:0x4], byteorder="little")
        ),
        chunk_size=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
        load_addr=int.from_bytes(data[0x8:0xC], byteorder="little"),
    )


def parse_streaming_list_entry(
    data: bytes,
) -> StreamingListMovieEntry | StreamingListAudioEntry:
    assert len(data) == STREAMING_LIST_ENTRY_SIZE
    stream_type = StreamingListStreamType.from_bytes(data[0:0x2], byteorder="little")
    match stream_type:
        case StreamingListStreamType.Movie:
            return StreamingListMovieEntry(
                stream_type=stream_type,
                unknown1=int.from_bytes(data[0x2:0x4], byteorder="little"),
                offset_folder=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
                offset_inter=int.from_bytes(data[0x8:0xC], byteorder="little") * 0x800,
                unknown2=int.from_bytes(data[0xC:0xE], byteorder="little"),
                stream_id=int.from_bytes(data[0xE:0x10], byteorder="little"),
                stream_sub_id=int.from_bytes(data[0x10:0x12], byteorder="little"),
                picture_width=int.from_bytes(data[0x12:0x14], byteorder="little"),
                picture_height=int.from_bytes(data[0x14:0x16], byteorder="little"),
                unknown3=int.from_bytes(data[0x16:0x18], byteorder="little"),
                unknown4=int.from_bytes(data[0x18:0x1A], byteorder="little"),
                unknown5=int.from_bytes(data[0x1A:0x1C], byteorder="little"),
                unknown6=int.from_bytes(data[0x1C:0x22], byteorder="little"),
                unknown7=int.from_bytes(data[0x22:0x24], byteorder="little"),
                movie_number=int.from_bytes(data[0x24:0x26], byteorder="little"),
                unknown8=int.from_bytes(data[0x26:0x28], byteorder="little"),
            )
        case StreamingListStreamType.Audio:
            return StreamingListAudioEntry(
                stream_type=stream_type,
                unknown1=int.from_bytes(data[0x2:0x4], byteorder="little"),
                offset_stage=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
                unknown2=int.from_bytes(data[0x8:0xC], byteorder="little"),
                stage_number=int.from_bytes(data[0xC:0xE], byteorder="little"),
                stream_id=int.from_bytes(data[0xE:0x10], byteorder="little"),
                stream_sub_id=int.from_bytes(data[0x10:0x14], byteorder="little"),
                unknown3=int.from_bytes(data[0x14:0x16], byteorder="little"),
                unknown4=int.from_bytes(data[0x16:0x18], byteorder="little"),
                unknown5=int.from_bytes(data[0x18:0x1C], byteorder="little"),
                unknown6=int.from_bytes(data[0x1C:0x20], byteorder="little"),
                unknown7=int.from_bytes(data[0x20:0x28], byteorder="little"),
            )


def read_chunk(data: BinaryIO) -> FileChunk | None:
    """Read one chunk; ``data`` payload has per-sector pad stripped via sector_len."""
    from format import unpack_chunk_payload  # local import avoids circular issues

    header_bytes = data.read(FILE_CHUNK_HEADER_SIZE)
    header = parse_file_chunk_header(header_bytes)
    if header is None:
        return None
    # Rest of the on-disc chunk (may span many CD sectors).
    rest = data.read(header.chunk_size - FILE_CHUNK_HEADER_SIZE)
    full = header_bytes + rest
    payload = unpack_chunk_payload(full, header.sector_len)
    return FileChunk(header=header, data=payload)


def read_file(data: BinaryIO, offset: int) -> File | None:
    current_pos = data.tell()
    data.seek(offset)

    chunks = []
    while True:
        chunk = read_chunk(data)
        if chunk is None:
            break

        chunks.append(chunk)
        if chunk.header.end_flag == FileChunkEndFlag.End:
            break

    data.seek(current_pos)
    return File(chunks=chunks)


# Type directory names (under raw/ and at assets root for inflated).
TYPE_DIR_BY_EXT: dict[str, str] = {
    ".pe2pkg": "pe2pkg",
    ".pe2img": "pe2img",
    ".pe2clut": "pe2clut",
    ".pe2cap2": "pe2cap2",
    ".bs": "bs",
    ".spk": "spk",
    ".txt": "txt",
}

IMAGE_EXTS = frozenset({".pe2img", ".pe2clut"})
# Types that inflate to PNG under the type store (edit form).
PNG_INFLATE_EXTS = frozenset({".pe2img", ".pe2clut", ".bs"})
# Sound banks inflate to a directory of WAV + meta.
SPK_INFLATE_EXT = ".spk"
RAW_ROOT_NAME = "raw"


class AssetStore:
    """Deduplicated per-type asset store: raw on-disc + inflated edit forms.

    Layout::

        assets/USA/
          raw/pe2pkg/gameplay.pe2pkg  # clean on-disc; NAMES or pe2pkg_N
          raw/pe2img/pe2img_0.pe2img
          raw/bs/bs_0.bs              # BS v2 MDEC on-disc
          pe2pkg/gameplay.pe2pkg      # LZSS-decoded (from unique raw only)
          pe2img/pe2img_0.png         # + meta (from unique raw only)
          bs/bs_0.png                 # + meta (from unique raw only)
          stage0/…/trailer.bin        # pack sidecars only
          stages.json                 # pack manifest (paths into type dirs)

    Dedup is by SHA-1 of the **raw** clean payload. Inflated assets are
    produced once per unique raw file via :meth:`materialize_inflated`.
    The in-memory :attr:`map` feeds ``stages.json`` during extract; it is not
    written to disk.
    """

    def __init__(self, assets_root: Path):
        self.assets_root = assets_root
        self.raw_root = assets_root / RAW_ROOT_NAME
        # type_dir → sha1 → raw-relative path (posix, under assets root)
        self._by_hash: dict[str, dict[str, str]] = {}
        # type_dir → next free index for type_N names
        self._counters: dict[str, int] = {}
        # type_dir → set of used stems (collision avoid)
        self._used_names: dict[str, set[str]] = {}
        # raw_rel → first canonical key that produced this file
        self._first_canonical: dict[str, str] = {}
        # unique raw entries to inflate: raw_rel → (type_dir, stem, ext)
        self._unique_raw: dict[str, tuple[str, str, str]] = {}
        # canonical chunk path → map entry
        self.map: dict[str, dict] = {}

    @staticmethod
    def type_dir_for(ext: str) -> str:
        ext = ext if ext.startswith(".") else f".{ext}"
        return TYPE_DIR_BY_EXT.get(ext, ext.lstrip(".") or "bin")

    def _allocate_stem(
        self,
        type_dir: str,
        *,
        stage: int,
        file_id: int,
        chunk_idx: int,
        folder_id: int | None,
    ) -> str:
        used = self._used_names.setdefault(type_dir, set())
        preferred = lookup(chunk_key(stage, file_id, chunk_idx, folder_id))
        if preferred is not None:
            stem = preferred
            n = 2
            while stem in used:
                stem = f"{preferred}_{n}"
                n += 1
            used.add(stem)
            return stem
        while True:
            i = self._counters.get(type_dir, 0)
            self._counters[type_dir] = i + 1
            stem = f"{type_dir}_{i}"
            if stem not in used:
                used.add(stem)
                return stem

    @staticmethod
    def _inflated_rel(type_dir: str, stem: str, ext: str) -> str:
        """Relative path of the edit/inflated asset under assets root."""
        if ext in PNG_INFLATE_EXTS:
            return f"{type_dir}/{stem}.png"
        if ext == SPK_INFLATE_EXT:
            # Directory of meta.json + sample_*.wav
            return f"{type_dir}/{stem}/meta.json"
        return f"{type_dir}/{stem}{ext}"

    def _map_entry(
        self,
        *,
        type_dir: str,
        stem: str,
        ext: str,
        raw_rel: str,
        digest: str,
        duplicate_of: str | None,
        header: dict,
    ) -> dict:
        # Fields used when building stages.json (path + pack metadata).
        # raw_rel / digest stay for materialize fallbacks and logging.
        return {
            "type": type_dir,
            "name": stem,
            "raw_path": raw_rel,
            "path": self._inflated_rel(type_dir, stem, ext),
            "sha1": digest,
            "duplicate_of": duplicate_of,
            "chunk_type": header.get("chunk_type"),
            "sector_len": header.get("sector_len"),
            "chunk_size": header.get("chunk_size"),
            "load_addr": header.get("load_addr"),
        }

    def put_raw(
        self,
        data: bytes,
        *,
        ext: str,
        stage: int,
        file_id: int,
        chunk_idx: int,
        folder_id: int | None,
        canonical: str,
        header: dict,
    ) -> tuple[Path, str, bool]:
        """Store unique **raw** (clean on-disc) bytes under ``raw/{type}/``.

        Returns ``(raw_path, stem, is_new)``. pe2pkg bodies are ``trim_lzss``'d
        before hash/store. Duplicates reuse the first raw file.
        """
        ext = ext if ext.startswith(".") else f".{ext}"
        type_dir = self.type_dir_for(ext)

        if ext == ".pe2pkg" and data:
            from lzss import trim_lzss

            data = trim_lzss(data)

        digest = hashlib.sha1(data).hexdigest()
        by_hash = self._by_hash.setdefault(type_dir, {})

        if digest in by_hash:
            raw_rel = by_hash[digest]
            raw_path = self.assets_root / raw_rel
            stem = Path(raw_rel).stem
            first = self._first_canonical.get(raw_rel)
            self.map[canonical] = self._map_entry(
                type_dir=type_dir,
                stem=stem,
                ext=ext,
                raw_rel=raw_rel,
                digest=digest,
                duplicate_of=first,
                header=header,
            )
            return raw_path, stem, False

        stem = self._allocate_stem(
            type_dir,
            stage=stage,
            file_id=file_id,
            chunk_idx=chunk_idx,
            folder_id=folder_id,
        )
        raw_rel = f"{RAW_ROOT_NAME}/{type_dir}/{stem}{ext}"
        raw_path = self.assets_root / raw_rel
        raw_path.parent.mkdir(parents=True, exist_ok=True)
        raw_path.write_bytes(data)

        by_hash[digest] = raw_rel
        self._first_canonical[raw_rel] = canonical
        self._unique_raw[raw_rel] = (type_dir, stem, ext)
        self.map[canonical] = self._map_entry(
            type_dir=type_dir,
            stem=stem,
            ext=ext,
            raw_rel=raw_rel,
            digest=digest,
            duplicate_of=None,
            header=header,
        )
        return raw_path, stem, True

    def materialize_inflated(
        self,
        *,
        only_pe2pkg_stems: frozenset[str] | set[str] | None = None,
        jobs: int | None = None,
    ) -> int:
        """Build inflated type-store files from unique raw assets only.

        * ``.pe2pkg`` — LZSS-decode → ``pe2pkg/{stem}.pe2pkg``
        * ``.pe2img`` / ``.pe2clut`` / ``.bs`` — PNG + meta under type dir
        * ``.spk`` — ``spk/{stem}/meta.json`` + ``sample_*.wav`` (SPU-ADPCM)
        * ``.txt`` — strip zero pad (text only, no trailing NUL)
        * other — hardlink/copy raw → type dir (same bytes)

        If ``only_pe2pkg_stems`` is set (minimal / CI mode), only those pe2pkg
        stems are inflated (typically :data:`names.REQUIRED_OVERLAY_STEMS`);
        images, ascii, and other types are skipped.

        Decode work is process-pooled (``jobs``, default min(cpu, 16)).

        Returns the number of unique assets materialized.
        """
        work: list[dict] = []
        for raw_rel, (type_dir, stem, ext) in sorted(
            self._unique_raw.items(), key=lambda kv: asset_name_key(kv[0])
        ):
            if only_pe2pkg_stems is not None:
                if type_dir != "pe2pkg" or stem not in only_pe2pkg_stems:
                    continue
            work.append(
                {
                    "assets_root": str(self.assets_root),
                    "raw_rel": raw_rel,
                    "type_dir": type_dir,
                    "stem": stem,
                    "ext": ext,
                    "out_rel": self._inflated_rel(type_dir, stem, ext),
                }
            )

        if only_pe2pkg_stems is not None:
            logging.info(
                "Materializing minimal inflated set: pe2pkg stems %s (%d jobs)",
                sorted(only_pe2pkg_stems),
                default_jobs() if jobs is None else max(1, jobs),
            )
        else:
            logging.info(
                "Materializing inflated assets from %d unique raw files "
                "(%d worker(s))",
                len(work),
                default_jobs() if jobs is None else max(1, jobs),
            )

        results = run_jobs(
            _materialize_one_job, work, jobs=jobs, label_key="raw_rel"
        )
        count = 0
        for r in results:
            raw_rel = r.get("raw_rel")
            if r.get("ok"):
                count += 1
                if r.get("log"):
                    logging.info("  %s", r["log"])
            else:
                logging.error(
                    "  FAIL %s: %s",
                    raw_rel,
                    r.get("error", "unknown"),
                )
                if r.get("traceback"):
                    logging.debug("%s", r["traceback"])
            # Apply path fallback patches from worker (decode → raw copy)
            override = r.get("path_override")
            if override and raw_rel:
                for ent in self.map.values():
                    if ent.get("raw_path") == raw_rel:
                        ent["path"] = override
        return count


def _materialize_one_job(job: dict) -> dict:
    """Process-pool worker: inflate one unique raw asset."""
    assets_root = Path(job["assets_root"])
    raw_rel = job["raw_rel"]
    type_dir = job["type_dir"]
    stem = job["stem"]
    ext = job["ext"]
    out_rel = job["out_rel"]
    raw_path = assets_root / raw_rel
    out_path = assets_root / out_rel

    if not raw_path.exists():
        return {
            "ok": False,
            "raw_rel": raw_rel,
            "error": f"missing raw asset: {raw_path}",
        }

    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        path_override = None
        log = None

        if ext == ".pe2pkg":
            log = f"decode {raw_rel} → {out_rel}"
            out_path.write_bytes(decode_pe2pkg_payload(raw_path.read_bytes()))
        elif ext == ".txt":
            log = f"decode ascii {raw_rel} → {out_rel}"
            out_path.write_bytes(decode_ascii_payload(raw_path.read_bytes()))
        elif ext in IMAGE_EXTS:
            log = f"decode image {raw_rel} → {out_rel}"
            try:
                pe2_dest = out_path.with_suffix(ext)
                if pe2_dest != raw_path:
                    if pe2_dest.exists() or pe2_dest.is_symlink():
                        pe2_dest.unlink()
                    try:
                        pe2_dest.hardlink_to(raw_path)
                    except OSError:
                        pe2_dest.write_bytes(raw_path.read_bytes())
                materialize_image_asset(pe2_dest, pe2_dest)
                if pe2_dest.exists() and pe2_dest != out_path:
                    pe2_dest.unlink()
            except Exception as ex:
                fallback = assets_root / type_dir / f"{stem}{ext}"
                fallback.parent.mkdir(parents=True, exist_ok=True)
                if fallback.exists() or fallback.is_symlink():
                    fallback.unlink()
                try:
                    fallback.hardlink_to(raw_path)
                except OSError:
                    fallback.write_bytes(raw_path.read_bytes())
                path_override = f"{type_dir}/{stem}{ext}"
                log = f"image decode failed {raw_rel}: {ex}; raw pe2 copied"
        elif ext == ".bs":
            log = f"decode bs {raw_rel} → {out_rel}"
            try:
                materialize_bs_asset(raw_path, out_path.with_suffix(".bs"))
            except Exception as ex:
                fallback = assets_root / type_dir / f"{stem}{ext}"
                fallback.parent.mkdir(parents=True, exist_ok=True)
                if fallback.exists() or fallback.is_symlink():
                    fallback.unlink()
                try:
                    fallback.hardlink_to(raw_path)
                except OSError:
                    fallback.write_bytes(raw_path.read_bytes())
                path_override = f"{type_dir}/{stem}{ext}"
                log = f"BS decode failed {raw_rel}: {ex}; raw .bs copied"
        elif ext == SPK_INFLATE_EXT:
            log = f"decode spk {raw_rel} → {out_rel}"
            dest_dir = assets_root / type_dir / stem
            try:
                materialize_spk_asset(raw_path, dest_dir)
            except Exception as ex:
                fallback = assets_root / type_dir / f"{stem}{ext}"
                fallback.parent.mkdir(parents=True, exist_ok=True)
                if fallback.exists() or fallback.is_symlink():
                    fallback.unlink()
                try:
                    fallback.hardlink_to(raw_path)
                except OSError:
                    fallback.write_bytes(raw_path.read_bytes())
                path_override = f"{type_dir}/{stem}{ext}"
                log = f"SPK decode failed {raw_rel}: {ex}; raw .spk copied"
        else:
            log = f"link {raw_rel} → {out_rel}"
            if out_path.exists() or out_path.is_symlink():
                out_path.unlink()
            try:
                out_path.hardlink_to(raw_path)
            except OSError:
                out_path.write_bytes(raw_path.read_bytes())

        return {
            "ok": True,
            "raw_rel": raw_rel,
            "log": log,
            "path_override": path_override,
        }
    except Exception as ex:
        return {
            "ok": False,
            "raw_rel": raw_rel,
            "error": str(ex),
            "traceback": traceback.format_exc(),
        }

    def stats(self) -> dict[str, dict[str, int]]:
        out: dict[str, dict[str, int]] = {}
        for t, by_hash in self._by_hash.items():
            refs = sum(1 for e in self.map.values() if e.get("type") == t)
            out[t] = {"unique": len(by_hash), "refs": refs}
        return out


def output_chunk(
    chunk: FileChunk,
    chunk_idx: int,
    *,
    stage: int,
    file_id: int,
    folder_id: int | None,
    store: AssetStore,
):
    """Store one unique raw chunk under ``raw/{type}/``."""
    chunk_type = chunk.header.chunk_type
    ext = chunk_type.get_extension()
    canonical = chunk_path_key(stage, file_id, chunk_idx, ext, folder_id)
    raw_path, _stem, is_new = store.put_raw(
        chunk.data,
        ext=ext,
        stage=stage,
        file_id=file_id,
        chunk_idx=chunk_idx,
        folder_id=folder_id,
        canonical=canonical,
        header=chunk.header.encode(),
    )
    raw_rel = str(raw_path.relative_to(store.assets_root))
    first = store.map[canonical].get("duplicate_of")
    logging.info(
        "  %s %s → %s%s",
        "raw" if is_new else "dedup",
        canonical,
        raw_rel,
        "" if is_new else f" (of {first})",
    )


def output_file(
    entry: FileListEntry,
    file: File,
    *,
    stage: int,
    folder_id: int | None = None,
    store: AssetStore,
):
    """Store every chunk of one stage file into the type directories."""
    rel = disk_file_rel(stage, entry.file_id, folder_id)
    logging.info(
        "Extracting file %s (id %d)…",
        rel,
        entry.file_id,
    )
    for idx, chunk in enumerate(file.chunks):
        output_chunk(
            chunk,
            idx,
            stage=stage,
            file_id=entry.file_id,
            folder_id=folder_id,
            store=store,
        )


def extract_stage_0(header: BinaryIO, data: BinaryIO, *, store: AssetStore):
    logging.info(f"Extracting stage 0 at {header.name}")
    data.seek(0, 2)
    data_size = data.tell()

    streaming_list: list[StreamingListMovieEntry | StreamingListAudioEntry] = []
    header.seek(0x0)
    for _ in range(0x3):  # 0x0 - 0x78
        streaming_list.append(
            parse_streaming_list_entry(header.read(STREAMING_LIST_ENTRY_SIZE))
        )

    for i, entry in enumerate(streaming_list):
        logging.info(entry)
        # TODO: Extract entry

    file_list: list[FileListEntry] = []
    header.seek(0x78)
    for _ in range(0x360):  # 0x78 - 0x1B78
        file_list.append(parse_file_list_entry(header.read(FILE_LIST_ENTRY_SIZE)))

    for i, entry in enumerate(file_list):
        file = read_file(data, entry.file_offset)
        if file is None:
            continue

        if entry.file_id == 21000:
            entry_size = 0
            for stream_entry in streaming_list:
                match stream_entry.stream_type:
                    case StreamingListStreamType.Movie:
                        assert isinstance(stream_entry, StreamingListMovieEntry)
                        if stream_entry.movie_number != 0:
                            continue
                        entry_size = stream_entry.offset_folder - entry.file_offset
                        break
                    case StreamingListStreamType.Audio:
                        assert isinstance(stream_entry, StreamingListAudioEntry)
                        entry_size = stream_entry.offset_stage - entry.file_offset
                        break
                    case _:
                        assert False, f"Unknown stream type: {stream_entry.stream_type}"
        elif i < len(file_list) - 1:
            entry_size = file_list[i + 1].file_offset - entry.file_offset
        else:
            entry_size = data_size - entry.file_offset
        del entry_size  # size used only for streaming span; chunks drive store
        output_file(
            entry,
            file,
            stage=0,
            folder_id=None,
            store=store,
        )

    end = parse_file_list_entry(header.read(FILE_LIST_ENTRY_SIZE))
    assert end.file_id == 0xFFFFFFFF
    assert end.file_offset == 0xFFFFFFFF * 0x800


def extract_folder(
    data: BinaryIO,
    root_offset: int,
    folder_size: int,
    *,
    stage: int,
    folder_id: int,
    store: AssetStore,
):
    """Extract one STAGE-N folder's chunks into the type store."""
    logging.info(
        "Extracting folder id %d (stage %d)",
        folder_id,
        stage,
    )
    data_pos = data.tell()

    file_list: list[FileListEntry] = []
    data.seek(root_offset + 0x0)
    for _ in range(0xA2):  # root_offset + 0x0 - root_offset + 0x510
        entry = parse_file_list_entry(data.read(FILE_LIST_ENTRY_SIZE))
        if entry.file_id == 0 and entry.file_offset == 0:
            continue
        file_list.append(entry)

    assert data.tell() == root_offset + 0x510
    assert int.from_bytes(data.read(4), "little") == 0x0

    streaming_list: list[StreamingListMovieEntry | StreamingListAudioEntry] = []
    assert data.tell() == root_offset + 0x514
    for _ in range(0x12):
        entry_data = data.read(STREAMING_LIST_ENTRY_SIZE)
        if entry_data == b"\x00" * STREAMING_LIST_ENTRY_SIZE:
            continue
        entry = parse_streaming_list_entry(entry_data)
        streaming_list.append(entry)

    assert data.tell() == root_offset + 0x7E4
    assert int.from_bytes(data.read(4), "little") == 0x0

    for i, entry in enumerate(file_list):
        file = read_file(data, root_offset + entry.file_offset)
        if file is None:
            continue

        if i < len(file_list) - 1:
            entry_size: int = file_list[i + 1].file_offset - entry.file_offset
        else:
            entry_size_opt: int | None = None
            for stream_entry in streaming_list:
                match stream_entry.stream_type:
                    case StreamingListStreamType.Movie:
                        assert isinstance(stream_entry, StreamingListMovieEntry)
                        if stream_entry.movie_number != 0:
                            continue
                        entry_size_opt = stream_entry.offset_folder - entry.file_offset
                        break
                    case StreamingListStreamType.Audio:
                        assert isinstance(stream_entry, StreamingListAudioEntry)
                        entry_size_opt = stream_entry.offset_stage - entry.file_offset
                        break
                    case _:
                        assert False, f"Unknown stream type: {stream_entry.stream_type}"

            if entry_size_opt is None:
                entry_size_opt = folder_size - entry.file_offset
            entry_size = entry_size_opt
        del entry_size

        output_file(
            entry,
            file,
            stage=stage,
            folder_id=folder_id,
            store=store,
        )

    data.seek(data_pos)


def extract_stage_n(data: BinaryIO, *, stage: int, store: AssetStore):
    logging.info(f"Extracting stage at {data.name}")

    data.seek(0x0)
    folder_offset = 0x800
    for _ in range(0x100):
        entry = parse_folder_list_entry(data.read(FOLDER_LIST_ENTRY_SIZE))
        if entry.folder_id == 0 and entry.folder_size == 0:
            continue
        extract_folder(
            data,
            folder_offset,
            entry.folder_size,
            stage=stage,
            folder_id=entry.folder_id,
            store=store,
        )
        folder_offset += entry.folder_size


def main():
    logging.basicConfig(level=logging.INFO)
    parser = ArgumentParser()
    parser.add_argument("--executable_disk1", "-exe_d1", type=FileType("rb"))
    parser.add_argument("--executable_disk2", "-exe_d2", type=FileType("rb"))
    parser.add_argument("--stage0_header", "-s0_hdr", type=FileType("rb"))
    parser.add_argument("--stage0_data", "-s0_dat", type=FileType("rb"))
    parser.add_argument("--stage1", "-s1", type=FileType("rb"))
    parser.add_argument("--stage2", "-s2", type=FileType("rb"))
    parser.add_argument("--stage3", "-s3", type=FileType("rb"))
    parser.add_argument("--stage4", "-s4", type=FileType("rb"))
    parser.add_argument("--stage5", "-s5", type=FileType("rb"))
    parser.add_argument("--output", "-o", type=Path, default=".")
    parser.add_argument("--checksum", "-c", action="store_true")
    parser.add_argument(
        "--raw-only",
        action="store_true",
        help=(
            "Write raw/{type}/ only (deduplicated on-disc payloads). "
            "Skip inflated type dirs, stages/ISO manifests."
        ),
    )
    parser.add_argument(
        "--minimal-inflate",
        action="store_true",
        help=(
            "After raw extract, inflate only required decomp overlays "
            f"({', '.join(sorted(REQUIRED_OVERLAY_STEMS))}) under pe2pkg/. "
            "Skip images/ascii/full inflate and stages/ISO manifests. "
            "Intended for CI / matching builds."
        ),
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=None,
        help=(
            "Parallel workers for inflated decode "
            f"(default: min(cpu_count, 16) = {default_jobs()})"
        ),
    )
    args = parser.parse_args()
    if args.raw_only and args.minimal_inflate:
        parser.error("--raw-only and --minimal-inflate are mutually exclusive")

    executable_disk1: BinaryIO = args.executable_disk1
    executable_disk2: BinaryIO = args.executable_disk2
    checksum_disk1 = computeChecksum(executable_disk1)
    checksum_disk2 = computeChecksum(executable_disk2)

    if args.checksum:
        logging.info(f"Checksum of {executable_disk1.name}: {checksum_disk1:08X}")
        logging.info(f"Checksum of {executable_disk2.name}: {checksum_disk2:08X}")
        return
    else:
        pass

    output_path: Path = args.output
    output_path.mkdir(parents=True, exist_ok=True)
    validate_names()

    # raw/{type}/     — unique clean on-disc payloads (NAMES or type_N)
    # pe2pkg/ pe2img/… — inflated edit forms (one per unique raw)
    # stages.json     — pack manifest (paths into inflated type dirs)
    # stage0/…        — pack sidecars only (trailers, layout, streaming)
    store = AssetStore(output_path)

    extract_stage_0(args.stage0_header, args.stage0_data, store=store)
    extract_stage_n(args.stage1, stage=1, store=store)
    extract_stage_n(args.stage2, stage=2, store=store)
    extract_stage_n(args.stage3, stage=3, store=store)
    extract_stage_n(args.stage4, stage=4, store=store)
    extract_stage_n(args.stage5, stage=5, store=store)

    for type_dir, st in sorted(store.stats().items()):
        logging.info(
            "  raw/%s: %d unique / %d refs",
            type_dir,
            st["unique"],
            st["refs"],
        )

    logging.info(f"Copying main executable {executable_disk1.name}")
    shutil.copy(f"{executable_disk1.name}", f"{output_path / 'main.exe'}")

    if args.raw_only:
        logging.info("raw-only: skipped inflate, stages/ISO manifests")
        logging.info("All done! (raw at %s)", store.raw_root)
        return

    if args.minimal_inflate:
        n = store.materialize_inflated(
            only_pe2pkg_stems=REQUIRED_OVERLAY_STEMS, jobs=args.jobs
        )
        logging.info(
            "Minimal inflate: materialized %d overlay(s) %s",
            n,
            sorted(REQUIRED_OVERLAY_STEMS),
        )
        missing = [
            s
            for s in sorted(REQUIRED_OVERLAY_STEMS)
            if not (output_path / "pe2pkg" / f"{s}.pe2pkg").exists()
        ]
        if missing:
            logging.warning("Required overlay(s) not found in store: %s", missing)
        logging.info("minimal-inflate: skipped stages/ISO manifests")
        logging.info("All done! (raw + pe2pkg overlays under %s)", output_path)
        return

    n = store.materialize_inflated(jobs=args.jobs)
    logging.info("Materialized %d inflated assets", n)

    # Emit stages.json + iso_disk*.json and pack sidecars under stage*/.
    # Best-effort: requires the rom/ tree that holds layout.xml next to the CDF paths.
    try:
        from write_manifest import write_manifest

        rom_guess = None
        s0_hdr = Path(args.stage0_header.name).resolve()
        # .../rom/USA/disk1/STAGE0.HED -> rom/USA
        if s0_hdr.parent.name.lower().startswith("disk"):
            rom_guess = s0_hdr.parent.parent
        if rom_guess is not None and (rom_guess / "disk1" / "layout.xml").exists():
            logging.info("Writing stages + ISO manifests for %s", output_path)
            write_manifest(
                rom_guess,
                output_path,
                chunk_map=store.map,
                stage0_hed=Path(args.stage0_header.name),
                stage0_cdf=Path(args.stage0_data.name),
                stage_cdfs={
                    1: Path(args.stage1.name),
                    2: Path(args.stage2.name),
                    3: Path(args.stage3.name),
                    4: Path(args.stage4.name),
                    5: Path(args.stage5.name),
                },
            )
        else:
            logging.warning(
                "Skipping manifest generation (could not locate rom layout.xml near %s)",
                s0_hdr,
            )
    except Exception:
        logging.exception("Failed to write pack/ISO manifests")

    logging.info("All done!")


if __name__ == "__main__":
    main()
