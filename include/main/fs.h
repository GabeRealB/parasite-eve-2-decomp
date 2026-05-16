#ifndef FS_H
#define FS_H

#include "common.h"

/// Number of bytes in a CD sector.
#define FS_SECTOR_BYTE_SIZE 0x800

/// Number of words in a CD sector.
#define FS_SECTOR_WORD_SIZE 0x200

/// Resolution in bytes of the sizes/offsets in CDF/HED files.
#define FS_CDF_RESULUTION FS_SECTOR_BYTE_SIZE

/// Number of stage CDF files.
#define FS_CDF_STAGE_COUNT 6

typedef enum _FsCdfStreamType {
    FS_CDF_STREAM_TYPE_MOVIE = 1,
    FS_CDF_STREAM_TYPE_AUDIO = 2,
} FsCdfStreamType;

typedef enum _FsCdfChunkType {
    FS_CDF_CHUNK_TYPE_PKG   = 0,
    FS_CDF_CHUNK_TYPE_IMG   = 1,
    FS_CDF_CHUNK_TYPE_CLUT  = 2,
    FS_CDF_CHUNK_TYPE_CAP   = 4,
    FS_CDF_CHUNK_TYPE_BG    = 5,
    FS_CDF_CHUNK_TYPE_MUSIC = 6,
    FS_CDF_CHUNK_TYPE_ASCII = 7,
} FsCdfChunkType;

typedef enum _FsCdfChunkEndFlag {
    FS_CDF_CHUNK_END_FLAG_END      = -1, // Chunk is last in file.
    FS_CDF_CHUNK_END_FLAG_CONTINUE = 1,  // File contains more chunks.
} FsCdfChunkEndFlag;

typedef struct _FsCdfStream {
    u16 type;    // Stream type.
    u16 field_2; // Unknown. The final msb is set in STAGE0.HED to differentiate streams and files.
    u32 offset;  // Movie: Offset in folder. Audio: Offset in CDF.
    union {
        struct {
            u32 interOffset;
            u16 field_c;
            u16 id;
            u16 subId;
            u16 width;
            u16 height;
            u16 field_16;
            u16 field_18;
            u16 field_1a;
            u8  field_1c[6];
            u16 field_22;
            u16 field_24;
            u16 field_26;
        } movie;
        struct {
            u32 field_8;
            u16 stageIdx;
            u16 id;
            u32 subId;
            u16 field_14;
            u16 field_16;
            u32 field_18;
            u16 field_1c;
            u16 field_1e;
            u8  field_20[8];
        } audio;
    } data;
} FsCdfStream;
STATIC_ASSERT_SIZEOF(FsCdfStream, 0x28);

typedef struct _FsCdfFolder {
    u32 id;   // Folder id.
    u32 size; // Folder size.
} FsCdfFolder;

typedef struct _FsCdfFile {
    u32 id;     // File id.
    u32 offset; // Offset from the beginning of the folder.
} FsCdfFile;

typedef struct _FsCdfFileSmall {
    u16 id;     // File id.
    u16 offset; // Offset from the beginning of the folder.
} FsCdfFileSmall;

typedef struct _FsCdfChunkHeader {
    u8    type;     // Type of data stored in the chunk.
    u8    endFlag;  // Flag indicating whether the chunk is the last in the file.
    u16   field_2;  // Unknown.
    u32   size;     // Chunk size.
    void* loadAddr; // Address to where the chunk must be loaded. Is not NULL for pkgs.
    u32   padding;  // Padding bytes. Are always 0 in the USA version.
} FsCdfChunkHeader;
STATIC_ASSERT_SIZEOF(FsCdfChunkHeader, 0x10);

/// Start of a file chunk in a CDF file.
///
/// All chunks start with a header, followed with the actual chunk data.
/// A chunk is always padded to the next multiple of a cd sector size.
typedef struct _FsCdfChunk {
    FsCdfChunkHeader header;
    union {
        u8  bytes[FS_SECTOR_BYTE_SIZE - sizeof(FsCdfChunkHeader)];       // Raw chunk data.
        u32 words[FS_SECTOR_WORD_SIZE - (sizeof(FsCdfChunkHeader) / 4)]; // Raw chunk data as words.
    } data;
} FsCdfChunk;
STATIC_ASSERT_SIZEOF(FsCdfChunk, FS_SECTOR_BYTE_SIZE);

/// Contents of a CD sector.
typedef union _FsSector {
    u8               bytes[FS_SECTOR_BYTE_SIZE]; // Raw sector data.
    u32              words[FS_SECTOR_WORD_SIZE]; // Raw sector data as words.
    FsCdfChunkHeader chunk;                      // A file chunk.
} FsSector;

#endif
