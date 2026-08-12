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
# Payload starts after the 0x10-byte header, so valid range is (0x10, 0x800].
# 0x800 is the default when omitted from stages.json.
SECTOR_LEN_DEFAULT = SECTOR_SIZE  # 0x800
SECTOR_LEN_MIN_EXCLUSIVE = FILE_CHUNK_HEADER_SIZE  # 0x10 — must be strictly greater
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


# Only room packages and cap2 dialogue chunks use a non-zero RAM load address.
LOAD_ADDR_CHUNK_TYPES: frozenset[FileChunkType] = frozenset(
    {
        FileChunkType.RoomPkg,
        FileChunkType.Cap2,
    }
)


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
    """Ensure sector_len is in ``(0x10, 0x800]`` (exclusive end in sector buffer)."""
    if not isinstance(sector_len, int) or isinstance(sector_len, bool):
        raise TypeError(f"sector_len must be int, got {type(sector_len).__name__}")
    if sector_len <= SECTOR_LEN_MIN_EXCLUSIVE or sector_len > SECTOR_LEN_MAX:
        raise ValueError(
            f"sector_len 0x{sector_len:X} out of range "
            f"(0x{SECTOR_LEN_MIN_EXCLUSIVE:X}, 0x{SECTOR_LEN_MAX:X}] "
            f"(exclusive end offset after the 0x10-byte header, ≤ one CD sector)"
        )
    return sector_len


def parse_sector_len(value: object | None = None) -> int:
    """Parse stages/content sector_len; ``None`` / missing → default ``0x800``.

    Accepts int or hex/decimal strings. Validates with :func:`validate_sector_len`.
    """
    if value is None:
        return SECTOR_LEN_DEFAULT
    if isinstance(value, bool):
        raise TypeError("sector_len must not be bool")
    if isinstance(value, int):
        return validate_sector_len(value)
    s = str(value).strip()
    if not s:
        return SECTOR_LEN_DEFAULT
    return validate_sector_len(int(s, 0))


@dataclass
class FileChunkHeader:
    chunk_type: FileChunkType
    end_flag: FileChunkEndFlag
    # Exclusive end offset within the CD sector buffer (usually 0x800).
    # Loader: D_8006C4D4 = &Fs_CdSector + sector_len.
    sector_len: int
    chunk_size: int  # bytes, includes header
    # RAM dest for room_pkg / cap2 (Fs FileChunkHeader.loadAddr); 0 otherwise.
    load_addr: int

    def __post_init__(self) -> None:
        self.sector_len = validate_sector_len(self.sector_len)
        self.load_addr = parse_load_addr(
            self.load_addr, chunk_type=self.chunk_type
        )

    def encode_json(self) -> dict[str, str]:
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


def parse_load_addr(
    value: object | None, *, chunk_type: FileChunkType
) -> int:
    """Parse load_addr for a chunk; only room_pkg / cap2 may be non-zero.

    Missing / empty → 0. Non-zero on any other type raises ``ValueError``.
    """
    if value is None or value == "":
        addr = 0
    else:
        addr = parse_int(value) & 0xFFFFFFFF
    if addr != 0 and chunk_type not in LOAD_ADDR_CHUNK_TYPES:
        raise ValueError(
            f"load_addr 0x{addr:X} is only valid for room packages "
            f"(.pe2pkg) and cap2 (.pe2cap2), not {chunk_type_id(chunk_type)!r}"
        )
    if chunk_type not in LOAD_ADDR_CHUNK_TYPES:
        return 0
    return addr


# ---------------------------------------------------------------------------
# stages.json schema: allowed keys per chunk type
# ---------------------------------------------------------------------------

# Required for every non-raw chunk entry.
STAGES_CHUNK_REQUIRED_KEYS: frozenset[str] = frozenset({"path", "type"})

# Optional for every non-raw chunk.
# end_flag is never in stages.json — pack sets continue/end from chunk order.
STAGES_CHUNK_COMMON_OPTIONAL_KEYS: frozenset[str] = frozenset({"sector_len"})

# Extra keys required only on specific types (retail always has non-zero load_addr).
STAGES_CHUNK_TYPE_REQUIRED_KEYS: dict[FileChunkType, frozenset[str]] = {
    FileChunkType.RoomPkg: frozenset({"load_addr"}),
    FileChunkType.Cap2: frozenset({"load_addr"}),
}

# Extra optional keys allowed only on specific types (none currently).
STAGES_CHUNK_TYPE_EXTRA_KEYS: dict[FileChunkType, frozenset[str]] = {}

# Opaque raw.bin entries (non-chunked files).
STAGES_RAW_ALLOWED_KEYS: frozenset[str] = frozenset({"path", "type"})

# Stage object may only contain these top-level children.
STAGES_STAGE_ALLOWED_KEYS: frozenset[str] = frozenset({"files", "folders"})


def required_stages_chunk_keys(chunk_type: FileChunkType) -> frozenset[str]:
    """Keys that must appear on a stages.json content entry for ``chunk_type``."""
    return STAGES_CHUNK_REQUIRED_KEYS | STAGES_CHUNK_TYPE_REQUIRED_KEYS.get(
        chunk_type, frozenset()
    )


def allowed_stages_chunk_keys(chunk_type: FileChunkType) -> frozenset[str]:
    """Keys permitted on a stages.json content entry for ``chunk_type``."""
    extra = STAGES_CHUNK_TYPE_EXTRA_KEYS.get(chunk_type, frozenset())
    return (
        required_stages_chunk_keys(chunk_type)
        | STAGES_CHUNK_COMMON_OPTIONAL_KEYS
        | extra
    )


def validate_stages_chunk_entry(
    content: dict[str, Any], *, where: str
) -> None:
    """Validate one chunk (or raw) content dict in stages.json.

    Ensures only known keys are present for the declared type, required
    fields exist, and type-specific fields (``load_addr``, ``sector_len``)
    pass their value checks.
    """
    if not isinstance(content, dict):
        raise TypeError(f"{where}: content must be a dict, got {type(content).__name__}")

    keys = set(content.keys())
    if "type" not in content:
        raise ValueError(f"{where}: missing required key 'type'")

    type_val = content["type"]
    if type_val == "raw":
        unknown = keys - STAGES_RAW_ALLOWED_KEYS
        if unknown:
            raise ValueError(
                f"{where}: unknown key(s) {sorted(unknown)!r} for type 'raw'; "
                f"allowed {sorted(STAGES_RAW_ALLOWED_KEYS)!r}"
            )
        if "path" not in content:
            raise ValueError(f"{where}: missing required key 'path'")
        return

    if "path" not in content:
        raise ValueError(f"{where}: missing required key 'path'")

    try:
        chunk_type = resolve_chunk_type(type_val)
    except ValueError as e:
        raise ValueError(f"{where}: {e}") from e

    allowed = allowed_stages_chunk_keys(chunk_type)
    unknown = keys - allowed
    if unknown:
        raise ValueError(
            f"{where}: unknown key(s) {sorted(unknown)!r} for type "
            f"{chunk_type_id(chunk_type)!r}; allowed {sorted(allowed)!r}"
        )

    required = required_stages_chunk_keys(chunk_type)
    missing = required - keys
    if missing:
        raise ValueError(
            f"{where}: missing required key(s) {sorted(missing)!r} for type "
            f"{chunk_type_id(chunk_type)!r}"
        )

    # Value validation for optional/required fields when present.
    if "sector_len" in content:
        try:
            parse_sector_len(content["sector_len"])
        except (TypeError, ValueError) as e:
            raise ValueError(
                f"{where}: invalid sector_len {content['sector_len']!r}: {e}"
            ) from e
    if "load_addr" in content:
        try:
            addr = parse_load_addr(content["load_addr"], chunk_type=chunk_type)
        except (TypeError, ValueError) as e:
            raise ValueError(
                f"{where}: invalid load_addr {content['load_addr']!r}: {e}"
            ) from e
        if chunk_type in LOAD_ADDR_CHUNK_TYPES and addr == 0:
            raise ValueError(
                f"{where}: load_addr is required and must be non-zero for "
                f"{chunk_type_id(chunk_type)!r}"
            )


def validate_stages_manifest(stages_manifest: dict[str, Any]) -> None:
    """Validate the full stages.json shape and every chunk entry's keys."""
    if not isinstance(stages_manifest, dict):
        raise TypeError(
            f"stages.json must be an object, got {type(stages_manifest).__name__}"
        )

    for stage_name, stage_spec in stages_manifest.items():
        if not str(stage_name).startswith("stage"):
            raise ValueError(
                f"unknown top-level key {stage_name!r}; expected 'stageN'"
            )
        if not isinstance(stage_spec, dict):
            raise TypeError(
                f"{stage_name}: must be an object, got {type(stage_spec).__name__}"
            )
        unknown_stage = set(stage_spec.keys()) - STAGES_STAGE_ALLOWED_KEYS
        if unknown_stage:
            raise ValueError(
                f"{stage_name}: unknown key(s) {sorted(unknown_stage)!r}; "
                f"allowed {sorted(STAGES_STAGE_ALLOWED_KEYS)!r}"
            )
        has_files = "files" in stage_spec
        has_folders = "folders" in stage_spec
        if has_files and has_folders:
            raise ValueError(
                f"{stage_name} must have either 'files' or 'folders', not both"
            )
        if not has_files and not has_folders:
            raise ValueError(f"{stage_name} needs 'files' or 'folders'")

        if has_files:
            files_map = stage_spec["files"]
            if not isinstance(files_map, dict):
                raise TypeError(f"{stage_name}.files must be an object")
            for file_name, chunks in files_map.items():
                _validate_stages_file_chunks(
                    chunks, where=f"{stage_name}.files[{file_name!r}]"
                )
        else:
            folders_map = stage_spec["folders"]
            if not isinstance(folders_map, dict):
                raise TypeError(f"{stage_name}.folders must be an object")
            for folder_name, files_map in folders_map.items():
                if not isinstance(files_map, dict):
                    raise TypeError(
                        f"{stage_name}.folders[{folder_name!r}] must be an object"
                    )
                for file_name, chunks in files_map.items():
                    _validate_stages_file_chunks(
                        chunks,
                        where=(
                            f"{stage_name}.folders[{folder_name!r}]"
                            f"[{file_name!r}]"
                        ),
                    )


def _validate_stages_file_chunks(chunks: object, *, where: str) -> None:
    if not isinstance(chunks, dict):
        raise TypeError(
            f"{where}: must be an object of chunk_name → fields, "
            f"got {type(chunks).__name__}"
        )
    for chunk_name, content in chunks.items():
        if not isinstance(content, dict):
            raise TypeError(
                f"{where}[{chunk_name!r}]: must be an object, "
                f"got {type(content).__name__}"
            )
        validate_stages_chunk_entry(
            content, where=f"{where}[{chunk_name!r}]"
        )


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
        load_addr=int.from_bytes(data[0x8:0xC], "little"),
    )


def encode_file_chunk_header(header: FileChunkHeader) -> bytes:
    sector_len = validate_sector_len(header.sector_len)
    return (
        encode_u8(int(header.chunk_type))
        + encode_u8(int(header.end_flag))
        + encode_u16(sector_len)
        + encode_u32(to_sectors(header.chunk_size))
        + encode_u32(header.load_addr)
        + encode_u32(0)
    )


def unpack_chunk_payload(chunk_bytes: bytes, sector_len: int) -> bytes:
    """Extract contiguous valid payload from an on-disc multi-sector chunk.

    Disc layout (matches the CD loader)::

        sector 0:  [0x10 header][data … sector_len)[pad to 0x800]
        sector 1+: [data … sector_len)[pad to 0x800]

    ``chunk_bytes`` is the full chunk including the leading 0x10 header.
    Returns only the concatenated valid data (header stripped, pad stripped).

    When ``sector_len == 0x800`` this is simply ``chunk_bytes[0x10:]``.
    """
    sector_len = validate_sector_len(sector_len)
    if len(chunk_bytes) < FILE_CHUNK_HEADER_SIZE:
        return b""
    if len(chunk_bytes) % SECTOR_SIZE != 0:
        raise ValueError(
            f"chunk length 0x{len(chunk_bytes):X} is not a multiple of "
            f"sector size 0x{SECTOR_SIZE:X}"
        )

    out = bytearray()
    n_sectors = len(chunk_bytes) // SECTOR_SIZE
    for i in range(n_sectors):
        sec = chunk_bytes[i * SECTOR_SIZE : (i + 1) * SECTOR_SIZE]
        if i == 0:
            out += sec[FILE_CHUNK_HEADER_SIZE:sector_len]
        else:
            out += sec[0:sector_len]
    return bytes(out)


def strip_naive_payload_padding(payload: bytes, sector_len: int) -> bytes:
    """Recover valid data from a payload extracted as ``chunk_size - 0x10``.

    Older extract stored ``sec0[0x10:0x800] + sec1[0:0x800] + …``, which
    embeds ``(0x800 - sector_len)`` pad bytes per sector when
    ``sector_len < 0x800``.
    """
    sector_len = validate_sector_len(sector_len)
    if sector_len >= SECTOR_SIZE:
        return payload

    first_cap = SECTOR_SIZE - FILE_CHUNK_HEADER_SIZE  # 0x7F0 stored from sec0
    valid_first = sector_len - FILE_CHUNK_HEADER_SIZE  # 0x4F0 / 0x5F0 / …
    if len(payload) <= first_cap:
        return payload[:valid_first]

    out = bytearray(payload[:valid_first])
    pos = first_cap
    while pos < len(payload):
        sec = payload[pos : pos + SECTOR_SIZE]
        out += sec[:sector_len]
        pos += SECTOR_SIZE
    return bytes(out)


def expected_clean_payload_size(chunk_size: int, sector_len: int) -> int:
    """Byte length of valid payload for a chunk with the given disc sizes."""
    sector_len = validate_sector_len(sector_len)
    n_sectors = chunk_size // SECTOR_SIZE
    if n_sectors <= 0:
        return 0
    return (sector_len - FILE_CHUNK_HEADER_SIZE) + (n_sectors - 1) * sector_len


def ensure_clean_payload(
    payload: bytes, *, sector_len: int, chunk_size: int | None = None
) -> bytes:
    """Return sector-pad-free payload, stripping legacy naive extracts if needed."""
    sector_len = validate_sector_len(sector_len)
    if sector_len >= SECTOR_SIZE:
        return payload
    if chunk_size is not None:
        clean_len = expected_clean_payload_size(chunk_size, sector_len)
        naive_len = chunk_size - FILE_CHUNK_HEADER_SIZE
        if len(payload) == clean_len:
            return payload
        if len(payload) == naive_len:
            return strip_naive_payload_padding(payload, sector_len)
    # Heuristic: if longer than a clean single-sector body and looks naive.
    if len(payload) > (sector_len - FILE_CHUNK_HEADER_SIZE):
        # Only strip when payload length matches a naive multi-sector size.
        # naive = 0x7F0 + (n-1)*0x800  for some n
        first = SECTOR_SIZE - FILE_CHUNK_HEADER_SIZE
        if len(payload) >= first and (len(payload) - first) % SECTOR_SIZE == 0:
            return strip_naive_payload_padding(payload, sector_len)
    return payload


def pack_chunk_payload(
    header: FileChunkHeader, payload: bytes, *, chunk_size: int | None = None
) -> bytes:
    """Build on-disc chunk bytes: header + sector-strided payload with padding.

    If ``chunk_size`` is given (retail size), the result is padded out to that
    many bytes so unmodified assets repack byte-identically.
    """
    sector_len = validate_sector_len(header.sector_len)
    first_cap = sector_len - FILE_CHUNK_HEADER_SIZE
    cont_cap = sector_len

    if len(payload) <= first_cap:
        n_sectors = 1
    else:
        rem = len(payload) - first_cap
        n_sectors = 1 + (rem + cont_cap - 1) // cont_cap

    if chunk_size is not None:
        if chunk_size % SECTOR_SIZE != 0:
            raise ValueError(f"chunk_size 0x{chunk_size:X} not sector-aligned")
        n_from_size = chunk_size // SECTOR_SIZE
        # Retail chunk_size is a *minimum* pad (byte-identical when payload still
        # fits). Re-encoding from inflated assets may grow the payload; allow expansion.
        if n_from_size > n_sectors:
            n_sectors = n_from_size

    out = bytearray()
    pos = 0
    hdr_bytes = encode_file_chunk_header(
        FileChunkHeader(
            chunk_type=header.chunk_type,
            end_flag=header.end_flag,
            sector_len=sector_len,
            chunk_size=n_sectors * SECTOR_SIZE,
            load_addr=header.load_addr,
        )
    )
    for i in range(n_sectors):
        sec = bytearray(SECTOR_SIZE)
        if i == 0:
            sec[0:FILE_CHUNK_HEADER_SIZE] = hdr_bytes
            take = min(first_cap, max(0, len(payload) - pos))
            if take:
                sec[FILE_CHUNK_HEADER_SIZE : FILE_CHUNK_HEADER_SIZE + take] = (
                    payload[pos : pos + take]
                )
            pos += take
        else:
            take = min(cont_cap, max(0, len(payload) - pos))
            if take:
                sec[0:take] = payload[pos : pos + take]
            pos += take
        out += sec
    return bytes(out)


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
