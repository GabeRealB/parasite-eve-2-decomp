"""Shared HED/CDF binary formats and helpers for extract/pack."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Any


SECTOR_SIZE = 0x800

FOLDER_LIST_ENTRY_SIZE = 0x8
FILE_LIST_ENTRY_SIZE = 0x8
FILE_CHUNK_HEADER_SIZE = 0x10
STREAMING_LIST_ENTRY_SIZE = 0x28

# sector_len: exclusive end offset within the CD sector buffer.
# Payload starts after the 0x10-byte header, so valid range is [0x10, 0x800].
SECTOR_LEN_MIN = FILE_CHUNK_HEADER_SIZE  # 0x10
SECTOR_LEN_MAX = SECTOR_SIZE  # 0x800

# STAGE0.HED layout
STAGE0_STREAMING_COUNT = 0x3
STAGE0_FILE_LIST_COUNT = 0x360  # excludes terminator entry
STAGE0_HED_SIZE = 0x1B80

# STAGE1..N.CDF folder table / per-folder tables
STAGE_N_FOLDER_LIST_COUNT = 0x100
STAGE_N_FOLDER_LIST_SIZE = STAGE_N_FOLDER_LIST_COUNT * FOLDER_LIST_ENTRY_SIZE  # 0x800
STAGE_N_FILE_LIST_COUNT = 0xA2
STAGE_N_FILE_LIST_SIZE = STAGE_N_FILE_LIST_COUNT * FILE_LIST_ENTRY_SIZE  # 0x510
STAGE_N_STREAMING_COUNT = 0x12
STAGE_N_STREAMING_SIZE = STAGE_N_STREAMING_COUNT * STREAMING_LIST_ENTRY_SIZE  # 0x2D0
# file list (0x510) + u32 zero (4) + streams (0x2D0) + u32 zero (4) = 0x7E8
STAGE_N_FOLDER_HEADER_USED = 0x7E8
STAGE_N_FOLDER_HEADER_SIZE = SECTOR_SIZE  # padded to one sector

FILE_LIST_TERMINATOR = 0xFFFFFFFF


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


# Short type ids used in stages.json content entries.
CHUNK_TYPE_IDS: dict[FileChunkType, str] = {
    FileChunkType.RoomPkg: "room_pkg",
    FileChunkType.Image: "image",
    FileChunkType.Clut: "clut",
    FileChunkType.Cap2: "cap2",
    FileChunkType.RoomBackground: "room_background",
    FileChunkType.Music: "music",
    FileChunkType.Ascii: "ascii",
}

CHUNK_TYPE_BY_NAME: dict[str, FileChunkType] = {
    t.get_name(): t for t in FileChunkType
}
CHUNK_TYPE_BY_NAME.update(
    {
        "Room package": FileChunkType.RoomPkg,  # tolerate missing trailing space
        **{v: k for k, v in CHUNK_TYPE_IDS.items()},
        # common aliases
        "pkg": FileChunkType.RoomPkg,
        "pe2pkg": FileChunkType.RoomPkg,
        "spk": FileChunkType.Music,
        "bs": FileChunkType.RoomBackground,
        "txt": FileChunkType.Ascii,
    }
)
CHUNK_TYPE_BY_EXT: dict[str, FileChunkType] = {
    t.get_extension(): t for t in FileChunkType
}


def chunk_type_id(chunk_type: FileChunkType) -> str:
    return CHUNK_TYPE_IDS[chunk_type]


class FileChunkEndFlag(IntEnum):
    Continue = 0x01
    End = 0xFF


class StreamingListStreamType(IntEnum):
    Movie = 0x1
    Audio = 0x2


@dataclass
class FolderListEntry:
    folder_id: int
    folder_size: int  # bytes


@dataclass
class FileListEntry:
    file_id: int
    file_offset: int  # bytes, relative to container start


def validate_sector_len(sector_len: int) -> int:
    """Ensure sector_len is a usable exclusive end offset in the sector buffer."""
    if not isinstance(sector_len, int):
        raise TypeError(f"sector_len must be int, got {type(sector_len).__name__}")
    if sector_len < SECTOR_LEN_MIN or sector_len > SECTOR_LEN_MAX:
        raise ValueError(
            f"sector_len 0x{sector_len:X} out of range "
            f"[0x{SECTOR_LEN_MIN:X}, 0x{SECTOR_LEN_MAX:X}] "
            f"(must cover the 0x10-byte header and fit in one CD sector)"
        )
    return sector_len


@dataclass
class FileChunkHeader:
    chunk_type: FileChunkType
    end_flag: FileChunkEndFlag
    # Exclusive end offset within the CD sector buffer (usually 0x800).
    # Loader: D_8006C4D4 = &Fs_CdSector + sector_len.
    sector_len: int
    chunk_size: int  # bytes, includes header
    unknown2: int  # loadAddr for room_pkg / cap2; 0 otherwise

    def __post_init__(self) -> None:
        self.sector_len = validate_sector_len(self.sector_len)

    def encode_json(self) -> dict[str, str]:
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
class StreamingListMovieEntry:
    stream_type: StreamingListStreamType
    unknown1: int
    offset_folder: int  # bytes
    offset_inter: int  # bytes
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
    offset_stage: int  # bytes
    unknown2: int
    stage_number: int
    stream_id: int
    stream_sub_id: int
    unknown3: int
    unknown4: int
    unknown5: int
    unknown6: int
    unknown7: int


StreamingEntry = StreamingListMovieEntry | StreamingListAudioEntry


def parse_int(value: Any) -> int:
    """Parse an int from JSON (int or 0x-prefixed / decimal string)."""
    if isinstance(value, bool):
        raise TypeError(f"unexpected bool for integer field: {value!r}")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise TypeError(f"expected int or str, got {type(value).__name__}: {value!r}")


def align_up(value: int, alignment: int = SECTOR_SIZE) -> int:
    return (value + alignment - 1) // alignment * alignment


def to_sectors(byte_count: int) -> int:
    if byte_count % SECTOR_SIZE != 0:
        raise ValueError(
            f"value 0x{byte_count:X} is not sector-aligned (0x{SECTOR_SIZE:X})"
        )
    return byte_count // SECTOR_SIZE


def encode_u8(value: int) -> bytes:
    return value.to_bytes(1, "little")


def encode_u16(value: int) -> bytes:
    return value.to_bytes(2, "little")


def encode_u32(value: int) -> bytes:
    return value.to_bytes(4, "little")


def parse_folder_list_entry(data: bytes) -> FolderListEntry:
    assert len(data) == FOLDER_LIST_ENTRY_SIZE
    return FolderListEntry(
        folder_id=int.from_bytes(data[0x0:0x4], "little"),
        folder_size=int.from_bytes(data[0x4:0x8], "little") * SECTOR_SIZE,
    )


def encode_folder_list_entry(entry: FolderListEntry) -> bytes:
    return encode_u32(entry.folder_id) + encode_u32(to_sectors(entry.folder_size))


def parse_file_list_entry(data: bytes) -> FileListEntry:
    assert len(data) == FILE_LIST_ENTRY_SIZE
    return FileListEntry(
        file_id=int.from_bytes(data[0x0:0x4], "little"),
        file_offset=int.from_bytes(data[0x4:0x8], "little") * SECTOR_SIZE,
    )


def encode_file_list_entry(entry: FileListEntry) -> bytes:
    return encode_u32(entry.file_id) + encode_u32(to_sectors(entry.file_offset))


def parse_file_chunk_header(data: bytes) -> FileChunkHeader | None:
    assert len(data) == FILE_CHUNK_HEADER_SIZE
    chunk_type = int.from_bytes(data[0x0:0x1], "little")
    if chunk_type not in (
        FileChunkType.RoomPkg,
        FileChunkType.Image,
        FileChunkType.Clut,
        FileChunkType.Cap2,
        FileChunkType.RoomBackground,
        FileChunkType.Music,
        FileChunkType.Ascii,
    ):
        return None

    if int.from_bytes(data[0xC:0x10], "little") != 0:
        return None

    return FileChunkHeader(
        chunk_type=FileChunkType(chunk_type),
        end_flag=FileChunkEndFlag(int.from_bytes(data[0x1:0x2], "little")),
        sector_len=validate_sector_len(int.from_bytes(data[0x2:0x4], "little")),
        chunk_size=int.from_bytes(data[0x4:0x8], "little") * SECTOR_SIZE,
        unknown2=int.from_bytes(data[0x8:0xC], "little"),
    )


def encode_file_chunk_header(header: FileChunkHeader) -> bytes:
    sector_len = validate_sector_len(header.sector_len)
    return (
        encode_u8(int(header.chunk_type))
        + encode_u8(int(header.end_flag))
        + encode_u16(sector_len)
        + encode_u32(to_sectors(header.chunk_size))
        + encode_u32(header.unknown2)
        + encode_u32(0)
    )


def parse_streaming_list_entry(data: bytes) -> StreamingEntry:
    assert len(data) == STREAMING_LIST_ENTRY_SIZE
    stream_type = StreamingListStreamType(
        int.from_bytes(data[0:0x2], "little")
    )
    match stream_type:
        case StreamingListStreamType.Movie:
            return StreamingListMovieEntry(
                stream_type=stream_type,
                unknown1=int.from_bytes(data[0x2:0x4], "little"),
                offset_folder=int.from_bytes(data[0x4:0x8], "little") * SECTOR_SIZE,
                offset_inter=int.from_bytes(data[0x8:0xC], "little") * SECTOR_SIZE,
                unknown2=int.from_bytes(data[0xC:0xE], "little"),
                stream_id=int.from_bytes(data[0xE:0x10], "little"),
                stream_sub_id=int.from_bytes(data[0x10:0x12], "little"),
                picture_width=int.from_bytes(data[0x12:0x14], "little"),
                picture_height=int.from_bytes(data[0x14:0x16], "little"),
                unknown3=int.from_bytes(data[0x16:0x18], "little"),
                unknown4=int.from_bytes(data[0x18:0x1A], "little"),
                unknown5=int.from_bytes(data[0x1A:0x1C], "little"),
                unknown6=int.from_bytes(data[0x1C:0x22], "little"),
                unknown7=int.from_bytes(data[0x22:0x24], "little"),
                movie_number=int.from_bytes(data[0x24:0x26], "little"),
                unknown8=int.from_bytes(data[0x26:0x28], "little"),
            )
        case StreamingListStreamType.Audio:
            return StreamingListAudioEntry(
                stream_type=stream_type,
                unknown1=int.from_bytes(data[0x2:0x4], "little"),
                offset_stage=int.from_bytes(data[0x4:0x8], "little") * SECTOR_SIZE,
                unknown2=int.from_bytes(data[0x8:0xC], "little"),
                stage_number=int.from_bytes(data[0xC:0xE], "little"),
                stream_id=int.from_bytes(data[0xE:0x10], "little"),
                stream_sub_id=int.from_bytes(data[0x10:0x14], "little"),
                unknown3=int.from_bytes(data[0x14:0x16], "little"),
                unknown4=int.from_bytes(data[0x16:0x18], "little"),
                unknown5=int.from_bytes(data[0x18:0x1C], "little"),
                unknown6=int.from_bytes(data[0x1C:0x20], "little"),
                unknown7=int.from_bytes(data[0x20:0x28], "little"),
            )
        case _:
            raise ValueError(f"unknown stream type: {int(stream_type)}")


def encode_streaming_list_entry(entry: StreamingEntry) -> bytes:
    if isinstance(entry, StreamingListMovieEntry):
        return (
            encode_u16(int(entry.stream_type))
            + encode_u16(entry.unknown1)
            + encode_u32(to_sectors(entry.offset_folder))
            + encode_u32(to_sectors(entry.offset_inter))
            + encode_u16(entry.unknown2)
            + encode_u16(entry.stream_id)
            + encode_u16(entry.stream_sub_id)
            + encode_u16(entry.picture_width)
            + encode_u16(entry.picture_height)
            + encode_u16(entry.unknown3)
            + encode_u16(entry.unknown4)
            + encode_u16(entry.unknown5)
            + entry.unknown6.to_bytes(6, "little")
            + encode_u16(entry.unknown7)
            + encode_u16(entry.movie_number)
            + encode_u16(entry.unknown8)
        )
    if isinstance(entry, StreamingListAudioEntry):
        return (
            encode_u16(int(entry.stream_type))
            + encode_u16(entry.unknown1)
            + encode_u32(to_sectors(entry.offset_stage))
            + encode_u32(entry.unknown2)
            + encode_u16(entry.stage_number)
            + encode_u16(entry.stream_id)
            + encode_u32(entry.stream_sub_id)
            + encode_u16(entry.unknown3)
            + encode_u16(entry.unknown4)
            + encode_u32(entry.unknown5)
            + encode_u32(entry.unknown6)
            + entry.unknown7.to_bytes(8, "little")
        )
    raise TypeError(type(entry))


def streaming_entry_to_json(
    entry: StreamingEntry,
    *,
    content_end: int | None = None,
    container_size: int | None = None,
) -> dict[str, Any]:
    """Serialize a streaming list entry.

    When content_end/container_size are provided, folder-relative offsets that
    fall in the post-file stream region are stored as stream-relative so pack
    can recompute them after file sizes change.
    """

    def place_offset(offset: int, field: str) -> dict[str, Any]:
        if (
            content_end is not None
            and container_size is not None
            and content_end <= offset < container_size
        ):
            return {
                field: f"0x{offset:X}",
                f"{field}_space": "stream",
                f"{field}_stream_rel": f"0x{offset - content_end:X}",
            }
        return {
            field: f"0x{offset:X}",
            f"{field}_space": "absolute",
        }

    if isinstance(entry, StreamingListMovieEntry):
        data: dict[str, Any] = {
            "stream_type": "movie",
            "unknown1": f"0x{entry.unknown1:X}",
            **place_offset(entry.offset_folder, "offset_folder"),
            "offset_inter": f"0x{entry.offset_inter:X}",
            "unknown2": f"0x{entry.unknown2:X}",
            "stream_id": entry.stream_id,
            "stream_sub_id": entry.stream_sub_id,
            "picture_width": entry.picture_width,
            "picture_height": entry.picture_height,
            "unknown3": f"0x{entry.unknown3:X}",
            "unknown4": f"0x{entry.unknown4:X}",
            "unknown5": f"0x{entry.unknown5:X}",
            "unknown6": f"0x{entry.unknown6:X}",
            "unknown7": f"0x{entry.unknown7:X}",
            "movie_number": entry.movie_number,
            "unknown8": f"0x{entry.unknown8:X}",
        }
        return data

    if isinstance(entry, StreamingListAudioEntry):
        return {
            "stream_type": "audio",
            "unknown1": f"0x{entry.unknown1:X}",
            **place_offset(entry.offset_stage, "offset_stage"),
            "unknown2": f"0x{entry.unknown2:X}",
            "stage_number": entry.stage_number,
            "stream_id": entry.stream_id,
            "stream_sub_id": entry.stream_sub_id,
            "unknown3": f"0x{entry.unknown3:X}",
            "unknown4": f"0x{entry.unknown4:X}",
            "unknown5": f"0x{entry.unknown5:X}",
            "unknown6": f"0x{entry.unknown6:X}",
            "unknown7": f"0x{entry.unknown7:X}",
        }

    raise TypeError(type(entry))


def streaming_entry_from_json(
    data: dict[str, Any], *, new_content_end: int | None = None
) -> StreamingEntry:
    stream_type = data["stream_type"]
    if isinstance(stream_type, str):
        stream_type = stream_type.lower()
    else:
        stream_type = int(stream_type)

    def resolve_offset(field: str) -> int:
        space = data.get(f"{field}_space", "absolute")
        if space == "stream":
            if new_content_end is None:
                raise ValueError(
                    f"{field}_space is 'stream' but no content_end available"
                )
            rel = parse_int(data.get(f"{field}_stream_rel", 0))
            return new_content_end + rel
        return parse_int(data[field])

    if stream_type in ("movie", StreamingListStreamType.Movie, 1):
        return StreamingListMovieEntry(
            stream_type=StreamingListStreamType.Movie,
            unknown1=parse_int(data.get("unknown1", 0)),
            offset_folder=resolve_offset("offset_folder"),
            offset_inter=parse_int(data.get("offset_inter", 0)),
            unknown2=parse_int(data.get("unknown2", 0)),
            stream_id=parse_int(data["stream_id"]),
            stream_sub_id=parse_int(data["stream_sub_id"]),
            picture_width=parse_int(data["picture_width"]),
            picture_height=parse_int(data["picture_height"]),
            unknown3=parse_int(data.get("unknown3", 0)),
            unknown4=parse_int(data.get("unknown4", 0)),
            unknown5=parse_int(data.get("unknown5", 0)),
            unknown6=parse_int(data.get("unknown6", 0)),
            unknown7=parse_int(data.get("unknown7", 0)),
            movie_number=parse_int(data["movie_number"]),
            unknown8=parse_int(data.get("unknown8", 0)),
        )

    if stream_type in ("audio", StreamingListStreamType.Audio, 2):
        return StreamingListAudioEntry(
            stream_type=StreamingListStreamType.Audio,
            unknown1=parse_int(data.get("unknown1", 0)),
            offset_stage=resolve_offset("offset_stage"),
            unknown2=parse_int(data.get("unknown2", 0)),
            stage_number=parse_int(data["stage_number"]),
            stream_id=parse_int(data["stream_id"]),
            stream_sub_id=parse_int(data["stream_sub_id"]),
            unknown3=parse_int(data.get("unknown3", 0)),
            unknown4=parse_int(data.get("unknown4", 0)),
            unknown5=parse_int(data.get("unknown5", 0)),
            unknown6=parse_int(data.get("unknown6", 0)),
            unknown7=parse_int(data.get("unknown7", 0)),
        )

    raise ValueError(f"unknown stream_type: {data.get('stream_type')!r}")


def resolve_chunk_type(value: Any) -> FileChunkType:
    if isinstance(value, FileChunkType):
        return value
    if isinstance(value, int):
        return FileChunkType(value)
    if isinstance(value, str):
        if value in CHUNK_TYPE_BY_NAME:
            return CHUNK_TYPE_BY_NAME[value]
        # allow "0x6" / "6"
        try:
            return FileChunkType(int(value, 0))
        except ValueError:
            pass
        if value.startswith(".") and value in CHUNK_TYPE_BY_EXT:
            return CHUNK_TYPE_BY_EXT[value]
    raise ValueError(f"unknown chunk_type: {value!r}")


def resolve_end_flag(value: Any) -> FileChunkEndFlag:
    if isinstance(value, FileChunkEndFlag):
        return value
    if isinstance(value, int):
        return FileChunkEndFlag(value)
    if isinstance(value, str):
        lowered = value.lower()
        if lowered in ("continue", "cont", "0x1", "1"):
            return FileChunkEndFlag.Continue
        if lowered in ("end", "0xff", "ff", "255"):
            return FileChunkEndFlag.End
        return FileChunkEndFlag(int(value, 0))
    raise ValueError(f"unknown end_flag: {value!r}")
