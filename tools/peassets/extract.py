import hashlib
import json
import logging
import shutil
import sys
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
from lzss import decode_lzss  # noqa: E402
from names import (  # noqa: E402
    chunk_filename,
    chunk_path_key,
    disk_file_rel,
    disk_folder_rel,
    ovr_name_for,
    reverse_file_id,
    reverse_folder_id,
    validate_names,
)


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
            FileChunkType.RoomBackground: "PSX MDEX bitstream image",
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
    sector_len: int  # exclusive end offset in sector buffer; range [0x10, 0x800]
    chunk_size: int
    unknown2: int  # loadAddr for room_pkg / cap2

    def encode(self) -> dict:
        return {
            "chunk_type": self.chunk_type.get_name(),
            "end_flag": "continue"
            if self.end_flag == FileChunkEndFlag.Continue
            else "end",
            "sector_len": f"0x{self.sector_len:X}",
            "chunk_size": f"0x{self.chunk_size:X}",
            "unknown2": f"0x{self.unknown2:X}",
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

# Asset path names live in names.py (NAMES / OVR_NAMES).
# Canonical keys use disc ids (stage0/file0/1.pe2pkg); on-disk paths use
# friendly names when set (stage0/gameplay/gameplay.pe2pkg).


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
        unknown2=int.from_bytes(data[0x8:0xC], byteorder="little"),
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
    header = parse_file_chunk_header(data.read(FILE_CHUNK_HEADER_SIZE))
    if header is None:
        return None
    chunk_data = data.read(header.chunk_size - FILE_CHUNK_HEADER_SIZE)
    return FileChunk(header=header, data=chunk_data)


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


# Sidecars that only belong in raw/ (metadata, opaque on-disc regions).
RAW_ONLY_NAMES = frozenset(
    {
        "stream_data.bin",
        "trailer.bin",
        "raw.bin",
        "headers.json",
        "meta.json",
        "layout.json",
        "streaming.json",
    }
)


def output_chunk(
    file_path: Path,
    chunk: FileChunk,
    chunk_idx: int,
    *,
    stage: int,
    file_id: int,
    folder_id: int | None,
):
    """Write one chunk in on-disc form (never decompress) under the raw tree."""
    chunk_type = chunk.header.chunk_type
    name = chunk_filename(
        stage, file_id, chunk_idx, chunk_type.get_extension(), folder_id
    )
    chunk_path = (file_path / name).absolute()

    logging.info(
        f"Extracting {chunk_type.get_name()}({chunk_type.get_extension()}) to {chunk_path}..."
    )
    with open(chunk_path, "wb") as f:
        f.write(chunk.data)


def output_file(
    stage_root: Path,
    entry: FileListEntry,
    entry_size: int,
    file: File,
    *,
    stage: int,
    folder_id: int | None = None,
):
    """Write one file's chunks under the (possibly named) raw tree path.

    Also writes ``meta.json`` with disc ids so later tools can reverse-map
    friendly directory names back to ``file_id`` / ``folder_id``.
    """
    rel = disk_file_rel(stage, entry.file_id, folder_id)
    # stage_root is …/raw/stageN — disk_file_rel includes stageN/…
    file_path = (stage_root.parent / rel).absolute()
    logging.info(
        "Extracting file %s (id %d) to %s...",
        rel,
        entry.file_id,
        file_path,
    )
    file_path.mkdir(parents=True, exist_ok=True)

    for idx, chunk in enumerate(file.chunks):
        output_chunk(
            file_path,
            chunk,
            idx,
            stage=stage,
            file_id=entry.file_id,
            folder_id=folder_id,
        )

    headers = [chunk.header.encode() for chunk in file.chunks]
    with open(file_path / "headers.json", "w") as f:
        json.dump(headers, f, indent=4)

    meta = {
        "stage": stage,
        "file_id": entry.file_id,
        "folder_id": folder_id,
        "canonical": f"stage{stage}/"
        + (f"{folder_id}/" if folder_id is not None else "")
        + f"file{entry.file_id}",
    }
    with open(file_path / "meta.json", "w") as f:
        json.dump(meta, f, indent=2)
        f.write("\n")


def materialize_decoded_tree(raw_root: Path, decoded_root: Path) -> None:
    """Build decoded/ from raw/: LZSS-decode .pe2pkg, copy other chunk payloads.

    headers.json and stream/trailer sidecars stay raw-only.
    """
    logging.info("Materializing decoded assets under %s", decoded_root)
    decoded_root.mkdir(parents=True, exist_ok=True)

    for path in sorted(raw_root.rglob("*")):
        if not path.is_file():
            continue
        if path.name in RAW_ONLY_NAMES:
            continue

        rel = path.relative_to(raw_root)
        dest = decoded_root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)

        if path.suffix == ".pe2pkg":
            logging.info("  decoding %s", rel)
            dest.write_bytes(decode_lzss(path.read_bytes()))
        else:
            # .spk, .pe2img, .bs, …
            try:
                if dest.exists() or dest.is_symlink():
                    dest.unlink()
                dest.hardlink_to(path)
            except OSError:
                shutil.copy2(path, dest)


def extract_stage_0(header: BinaryIO, data: BinaryIO, output: Path):
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
        output_file(
            output, entry, entry_size, file, stage=0, folder_id=None
        )

    end = parse_file_list_entry(header.read(FILE_LIST_ENTRY_SIZE))
    assert end.file_id == 0xFFFFFFFF
    assert end.file_offset == 0xFFFFFFFF * 0x800


def extract_folder(
    data: BinaryIO,
    stage_root: Path,
    root_offset: int,
    folder_size: int,
    *,
    stage: int,
    folder_id: int,
):
    """Extract one STAGE-N folder into a (possibly named) directory.

    ``stage_root`` is ``…/raw/stageN`` (the stage directory, not the folder).
    """
    folder_rel = disk_folder_rel(stage, folder_id)
    folder_path = (stage_root.parent / folder_rel).absolute()
    logging.info(
        "Extracting folder id %d → %s",
        folder_id,
        folder_path,
    )
    folder_path.mkdir(parents=True, exist_ok=True)
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
            entry_size: int = entry_size_opt

        output_file(
            stage_root,
            entry,
            entry_size,
            file,
            stage=stage,
            folder_id=folder_id,
        )

    data.seek(data_pos)


def extract_stage_n(data: BinaryIO, output: Path, *, stage: int):
    logging.info(f"Extracting stage at {data.name}")

    data.seek(0x0)
    folder_offset = 0x800
    for _ in range(0x100):
        entry = parse_folder_list_entry(data.read(FOLDER_LIST_ENTRY_SIZE))
        if entry.folder_id == 0 and entry.folder_size == 0:
            continue
        extract_folder(
            data,
            output,
            folder_offset,
            entry.folder_size,
            stage=stage,
            folder_id=entry.folder_id,
        )
        folder_offset += entry.folder_size


def _resolve_ids_from_file_dir(
    file_dir: Path, raw_root: Path
) -> tuple[int, int, int | None] | None:
    """Return (stage, file_id, folder_id) for a raw file directory.

    Prefers ``meta.json`` written by extract; otherwise parses the relative
    path with reverse name lookup (``file12`` / decimal folder ids / NAMES).
    """
    meta_path = file_dir / "meta.json"
    if meta_path.exists():
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
        stage = int(meta["stage"])
        file_id = int(meta["file_id"])
        folder_id = meta.get("folder_id")
        if folder_id is not None:
            folder_id = int(folder_id)
        return stage, file_id, folder_id

    rel = file_dir.relative_to(raw_root)
    parts = rel.parts  # stageN / [folder] / filedir
    if len(parts) < 2 or not parts[0].startswith("stage"):
        return None
    try:
        stage = int(parts[0][len("stage") :])
    except ValueError:
        return None

    if len(parts) == 2:
        # stage0/fileOrName
        file_id = reverse_file_id(stage, parts[1], None)
        if file_id is None:
            return None
        return stage, file_id, None

    if len(parts) == 3:
        folder_id = reverse_folder_id(stage, parts[1])
        if folder_id is None:
            return None
        file_id = reverse_file_id(stage, parts[2], folder_id)
        if file_id is None:
            return None
        return stage, file_id, folder_id

    return None


def create_pkg_summary(output_path: Path) -> dict:
    """Build OVR/map.json keyed by *canonical* chunk paths.

    Walks every ``raw/**/headers.json`` file directory. Room packages get an
    ``ovr_name`` (friendly, path-derived, or ``<DUPLICATE>`` when the payload
    matches an earlier package). ``path`` is the on-disk relative location
    under decoded/raw (may use friendly names).
    """
    summary: dict = {}
    seen_hash: dict[str, str] = {}
    ovr_copies: list[tuple[str, str]] = []

    raw_root = output_path / "raw"
    decoded_root = output_path / "decoded"

    for headers_path in sorted(raw_root.rglob("headers.json")):
        file_dir = headers_path.parent
        ids = _resolve_ids_from_file_dir(file_dir, raw_root)
        if ids is None:
            logging.warning("cannot resolve disc ids for %s", file_dir)
            continue
        stage, file_id, folder_id = ids
        headers = json.loads(headers_path.read_text(encoding="utf-8"))

        # headers.json is ordered by on-disc chunk index; only RoomPkg rows
        # produce .pe2pkg files (and OVR aliases).
        for idx, header in enumerate(headers):
            chunk_type_name = str(header.get("chunk_type", ""))
            if not chunk_type_name.startswith("Room package"):
                continue

            ext = ".pe2pkg"
            canonical = chunk_path_key(stage, file_id, idx, ext, folder_id)
            disk_name = chunk_filename(stage, file_id, idx, ext, folder_id)
            disk_rel = f"{disk_file_rel(stage, file_id, folder_id)}/{disk_name}"
            pkg_path = file_dir / disk_name
            if not pkg_path.exists():
                # Fall back to default index name (unmigrated tree).
                alt = file_dir / f"{idx}{ext}"
                if alt.exists():
                    pkg_path = alt
                    disk_name = alt.name
                    disk_rel = (
                        f"{disk_file_rel(stage, file_id, folder_id)}/{disk_name}"
                    )
                else:
                    logging.warning(
                        "missing pe2pkg for %s (expected %s)", canonical, pkg_path
                    )
                    continue

            payload = pkg_path.read_bytes()
            digest = hashlib.sha1(payload).hexdigest()
            ovr = ovr_name_for(stage, file_id, idx, ext, folder_id)
            if digest in seen_hash:
                ovr = "<DUPLICATE>"
            else:
                seen_hash[digest] = canonical
                ovr_copies.append((disk_rel, ovr))

            summary[canonical] = {
                "ovr_name": ovr,
                "path": disk_rel,
                "chunk_size": header["chunk_size"],
                "load_address": header["unknown2"],
            }

    ovr_dir = output_path / "OVR"
    ovr_dir.mkdir(parents=True, exist_ok=True)
    with open(ovr_dir / "map.json", "w") as f:
        json.dump(summary, f, indent=4)
        f.write("\n")

    # Materialize OVR aliases from decoded payloads when available.
    logging.info("Copying decoded packages into OVR/")
    for disk_rel, ovr in ovr_copies:
        if ovr == "<DUPLICATE>":
            continue
        src = decoded_root / disk_rel
        if not src.exists():
            src = raw_root / disk_rel
        if not src.exists():
            logging.warning("OVR source missing for %s → %s", disk_rel, ovr)
            continue
        dst = ovr_dir / ovr
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

    return summary


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
    args = parser.parse_args()

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

    # raw/     — on-disc stage trees (pkgs still LZSS-compressed)
    # decoded/ — same layout with .pe2pkg LZSS-decoded (edit/rebuild source)
    # OVR/     — named overlay aliases for the decomp build (from decoded/)
    # Path components use names.NAMES when set (see names.py).
    raw_dir = output_path / "raw"
    decoded_dir = output_path / "decoded"
    raw_dir.mkdir(parents=True, exist_ok=True)

    extract_stage_0(args.stage0_header, args.stage0_data, raw_dir / "stage0")
    extract_stage_n(args.stage1, raw_dir / "stage1", stage=1)
    extract_stage_n(args.stage2, raw_dir / "stage2", stage=2)
    extract_stage_n(args.stage3, raw_dir / "stage3", stage=3)
    extract_stage_n(args.stage4, raw_dir / "stage4", stage=4)
    extract_stage_n(args.stage5, raw_dir / "stage5", stage=5)

    materialize_decoded_tree(raw_dir, decoded_dir)

    logging.info(f"Copying main executable {executable_disk1.name}")
    shutil.copy(f"{executable_disk1.name}", f"{output_path / 'main.exe'}")

    logging.info("Creating package summary + OVR aliases")
    create_pkg_summary(output_path)

    # Emit stages.json + iso_disk*.json and trailer/stream sidecars under raw/.
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
