#ifndef MAIN_PRIVATE_FS_TYPES_H
#define MAIN_PRIVATE_FS_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libcd.h>

#include "common.h"

/// An entry of the folder list, as stored in a .CDF file.
typedef struct _FsCdfFolderListEntry {
    u32 id;   // Folder id.
    u32 size; // Folder size.
} FsCdfFolderListEntry;

/// Number of bytes in a CD sector.
#define FS_SECTOR_BYTE_SIZE 0x800

/// A list of folders found in the header section of a .CDF file.
typedef struct _FsCdfFolderList {
    FsCdfFolderListEntry entries[0x100]; // List of folders. Zero padded.
} FsCdfFolderList;
STATIC_ASSERT_SIZEOF(FsCdfFolderList, FS_SECTOR_BYTE_SIZE);

typedef struct _FsCdfChunkHeader {
    u8 type;                // Type of data stored in the chunk.
    u8 endFlag;             // Flag indicating whether the chunk is the last in the file.
    union {
        u16 dataOffset;     // Payload byte offset in an outer chunk header.
        u16 redirectSector; // Sector count in a folder's redirect entry.
    } offset;
    u32   size;             // Chunk size (sector count for CD reads).
    void* loadAddr;         // Address to where the chunk must be loaded. Non-NULL for pkgs.
    u8*   redirectAddr;     // Optional replacement destination; zero in USA headers.
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
