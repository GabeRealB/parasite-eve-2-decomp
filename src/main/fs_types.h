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

/// A list of folders found in the header section of a .CDF file.
typedef struct _FsCdfFolderList {
    FsCdfFolderListEntry entries[0x100]; // List of folders. Zero padded.
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

/// Start of a file chunk in a CDF file.
///
/// All chunks start with a header, followed with the actual chunk data.
/// A chunk is always padded to the next multiple of a cd sector size.
typedef struct _FsCdfChunk {
    FsCdfChunkHeader header;
    union {
        u8  bytes[FS_SECTOR_BYTE_SIZE - sizeof(FsCdfChunkHeader)];
        u32 words[FS_SECTOR_WORD_SIZE - (sizeof(FsCdfChunkHeader) / 4)];
    } data;
} FsCdfChunk;
STATIC_ASSERT_SIZEOF(FsCdfChunk, FS_SECTOR_BYTE_SIZE);

/// Contents of a CD sector.
typedef union _FsSector {
    u8              bytes[FS_SECTOR_BYTE_SIZE];
    u32             words[FS_SECTOR_WORD_SIZE];
    FsCdfFolderList folderList;
    FsCdfChunk      chunk;
    CdlLOC          location; // First bytes of the raw three-word CD sector header.
} FsSector;
STATIC_ASSERT_SIZEOF(FsSector, FS_SECTOR_BYTE_SIZE);

/// 8-byte work entry used by the FS load path (`Fs_WorkEntries`).
typedef struct _FsWorkEntry {
    u16 field_0;
    u16 field_2;
    u32 field_4;
} FsWorkEntry;
STATIC_ASSERT_SIZEOF(FsWorkEntry, 0x8);

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
