#ifndef MAIN_PRIVATE_FS_TYPES_H
#define MAIN_PRIVATE_FS_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libcd.h>

#include "common.h"

/// One record in the folder table that fills the first sector of a stage CDF.
///
/// `folderId` is the disc folder number. Dividing it by 100 gives the area
/// index, and the room folders on the disc are `area * 100 + 1`. A load
/// matches `fileGroup * 100 + fileIdHundreds`. `sectorCount` is that folder's
/// length in CD sectors, including its own file list. The first folder starts
/// at sector 1 of the CDF, and each count is the step to the next. A count of
/// zero ends the table.
typedef struct {
    u32 folderId;    // Disc folder number; the area index is this value / 100
    u32 sectorCount; // Length of this folder in CD sectors; 0 ends the table
} FsCdfFolderListEntry;
STATIC_ASSERT_SIZEOF(FsCdfFolderListEntry, 0x8);

/// Number of bytes in a CD sector.
#define FS_SECTOR_BYTE_SIZE 0x800

/// How many folder records fit in one CD sector.
#define FILE_SYSTEM_CDF_FOLDER_LIST_CAPACITY (FS_SECTOR_BYTE_SIZE / sizeof(FsCdfFolderListEntry))

/// Folder table that fills the first sector of a stage CDF (stages 1-5).
///
/// The first folder starts at sector 1. Each record's `sectorCount` is that
/// folder's length in CD sectors and the step to the next folder. A
/// `sectorCount` of zero ends the table; the rest of the sector is zero.
typedef struct {
    FsCdfFolderListEntry entries[FILE_SYSTEM_CDF_FOLDER_LIST_CAPACITY]; // On-disc order; a zero sectorCount ends the table
} FsCdfFolderList;
STATIC_ASSERT_SIZEOF(FsCdfFolderList, FS_SECTOR_BYTE_SIZE);

/// Opcodes stored in `FsCdfChunkHeader.type`.
enum {
    FILE_SYSTEM_CHUNK_PACKAGE    = 0, // LZSS room package
    FILE_SYSTEM_CHUNK_IMAGE      = 1, // Image strips
    FILE_SYSTEM_CHUNK_CLUT       = 2, // Color lookup table
    FILE_SYSTEM_CHUNK_RAW        = 3, // Raw copy of the first sector's payload
    FILE_SYSTEM_CHUNK_BUNDLE     = 4, // Resource directory, then a streamed payload
    FILE_SYSTEM_CHUNK_BACKGROUND = 5, // MDEC room background
    FILE_SYSTEM_CHUNK_MUSIC      = 6, // Music bank
    FILE_SYSTEM_CHUNK_TEXT       = 7, // Text; the loader copies no payload
};

/// Values stored in `FsCdfChunkHeader.endFlag`.
enum {
    FILE_SYSTEM_CHUNK_CONTINUES = 0x01, // Another chunk follows in the file
    FILE_SYSTEM_CHUNK_LAST      = 0xFF, // Last chunk in the file
};

/// Header of one chunk in a CDF file, at the start of that chunk's first sector.
///
/// `sectorCount` counts CD sectors, including the header sector. `sectorLen` is
/// the exclusive end of the valid bytes in each sector. A resource bundle's
/// directory is a separate record in the payload, not a second reading of this
/// header.
typedef struct {
    u8  type;        // Opcode (0 package, 1 image, 2 CLUT, 3 raw, 4 bundle, 5 background, 6 music, 7 text)
    u8  endFlag;     // 0x01 another chunk follows, 0xFF last chunk in the file
    u16 sectorLen;   // Exclusive end of valid bytes in the sector buffer, in (0x10, 0x800]
    u32 sectorCount; // Length of the chunk in CD sectors, including the header sector
    u8* loadAddr;    // Fixed RAM destination for a room package or resource bundle; NULL otherwise
    u32 unused;      // Unread by the loader. Extracted retail headers store zero
} FsCdfChunkHeader;
STATIC_ASSERT_SIZEOF(FsCdfChunkHeader, 0x10);

/// Number of words in a CD sector.
#define FS_SECTOR_WORD_SIZE 0x200

/// Opening CD sector of one chunk in a CDF file.
///
/// The header is stored only in this first sector. `data` is the rest of
/// the sector. The payload starts immediately after the header; bytes at
/// and beyond `header.sectorLen` are pad. Raw and background opcodes copy
/// all of `data`. Decompressors stop at `header.sectorLen`. A later sector
/// of the same chunk is ordinary sector data and is not this type.
/// `bytes` and `words` are that whole remainder.
typedef struct {
    FsCdfChunkHeader header; // Opcode, end flag, valid-byte end and load address
    union {
        u8  bytes[FS_SECTOR_BYTE_SIZE - sizeof(FsCdfChunkHeader)];
        u32 words[FS_SECTOR_WORD_SIZE - (sizeof(FsCdfChunkHeader) / 4)];
    } data; // Remainder of this sector, including pad past header.sectorLen
} FsCdfChunk;
STATIC_ASSERT_SIZEOF(FsCdfChunk, FS_SECTOR_BYTE_SIZE);
STATIC_ASSERT(sizeof(((FsCdfChunk*)0)->data.bytes) == sizeof(((FsCdfChunk*)0)->data.words), fs_cdf_chunk_data_widths);

/// One 2048-byte CD sector.
///
/// The members read the same storage. `bytes` and `words` are the whole
/// sector. `folderList` is the folder table that fills the first sector of a
/// stage CDF. `chunk` is the opening sector of a CDF chunk; a later sector of
/// that chunk is ordinary sector data and is read as `bytes`. `location` is
/// the BCD minute, second and sector at the start of the 12-byte header
/// delivered ahead of the payload. Those four bytes stay meaningful only until
/// a payload read reuses the buffer.
///
/// A folder's file list is an array of file-id and folder-relative offset
/// records from the start of the sector, ending at a zero offset, with stream
/// descriptors later in that same sector. The stage-zero header is walked
/// through `words`, and ISO directory records through `bytes`.
typedef union {
    u8              bytes[FS_SECTOR_BYTE_SIZE]; // Whole sector as bytes
    u32             words[FS_SECTOR_WORD_SIZE]; // Whole sector as 32-bit words
    FsCdfFolderList folderList;                 // Folder table from a stage CDF's first sector
    FsCdfChunk      chunk;                      // Opening sector of a CDF chunk
    CdlLOC          location;                   // BCD disc position from the 12-byte header
} FsSector;
STATIC_ASSERT_SIZEOF(FsSector, FS_SECTOR_BYTE_SIZE);

/// One VRAM column of a strip-coded image.
///
/// A type-1 image chunk, and each strip list a scene stream carries, opens with
/// a table of these records ending at one whose `x` is
/// `FILE_SYSTEM_IMAGE_COLUMN_END`. Each record names the top-left corner of a
/// column 64 halfwords wide, filled downward in 32-row LZSS strips. The strips
/// of every column follow one another in a single compressed stream that
/// starts `dataOffset` bytes after the table's first record; later records'
/// offsets are not read.
///
/// Columns are `FILE_SYSTEM_IMAGE_COLUMN_ROWS` tall. When the terminator is
/// the second record, its `y` selects the height of the single column instead:
/// `FILE_SYSTEM_IMAGE_COLUMN_END` selects `FILE_SYSTEM_IMAGE_COLUMN_SHORT_ROWS`,
/// and a value with `FILE_SYSTEM_IMAGE_COLUMN_ROWS_GIVEN` set gives the row
/// count in its low 15 bits.
typedef struct {
    u16 x;          // Destination X in VRAM, in halfwords (FILE_SYSTEM_IMAGE_COLUMN_END ends the table)
    u16 y;          // Destination row of the column's top; a terminator's height selector
    u32 dataOffset; // Byte offset of the strip stream from the table's start; read from the first record only
} FsImageColumn;
STATIC_ASSERT_SIZEOF(FsImageColumn, 0x8);

/// Markers and heights of an `FsImageColumn` table.
enum {
    FILE_SYSTEM_IMAGE_COLUMN_END        = 0xFFFF, // `x` of the record ending the table
    FILE_SYSTEM_IMAGE_COLUMN_ROWS       = 0x100,  // Default column height in rows
    FILE_SYSTEM_IMAGE_COLUMN_SHORT_ROWS = 0x40,   // Single-column height selected by a terminator `y` of 0xFFFF
    FILE_SYSTEM_IMAGE_COLUMN_ROWS_GIVEN = 0x8000, // Terminator `y` flag: the low 15 bits are the row count
};

/// Compact image/load params stored during FS setup (`Fs_LoadParams`).
typedef struct _FsLoadParams {
    byte unknown_0[0x2];
    u8   field_2;
    u8   field_3;
} FsLoadParams;
STATIC_ASSERT_SIZEOF(FsLoadParams, 0x4);

/// Number of stage CDF files.
#define FS_CDF_STAGE_COUNT 6

#endif // MAIN_PRIVATE_FS_TYPES_H
