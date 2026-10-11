#ifndef MAIN_TMD_STREAM_TYPES_H
#define MAIN_TMD_STREAM_TYPES_H

#include "common.h"

/// Read-only stream prefix projecting and lighting one corner into separate XY/RGB destinations.
///
/// The lit vertex pre-pass, environment-map passes and ripple-plane pass read
/// these four unsigned halfwords. Elements are word-aligned, with a u32-word
/// stride of at least two; any following words remain outside this prefix.
/// References select complete eight-byte SVECTORs in the part's vertex/normal
/// arrays. The resident pre-pass requires eight-byte-aligned references; other
/// passes discard their low three bits, whose role is unproven. The raw vertex
/// reference keys adjacent projection reuse, and its index must fit the draw
/// walk's 1024-entry depth cache.
///
/// Both destinations are independent aligned byte offsets from
/// `TmdStreamWorkspace::preXformWrite` into its first packet region, each
/// requiring four writable bytes. Environment mapping additionally requires
/// two U/V bytes after XY; full-screen mapping writes a temporary 0/1 page
/// marker immediately before XY. Stream and destination storage are borrowed
/// for the pass; no pointer to the prefix is retained.
typedef struct {
    u16 vertexByteRef; // Vertex byte reference; value / 8 is its depth-cache index
    u16 normalByteRef; // Normal byte reference for lighting this corner
    u16 xyByteOffset;  // Packed screen XY destination; environment U/V follow, page marker may precede
    u16 rgbByteOffset; // Independent four-byte lit RGB/code destination
} TmdLitCornerRefs;
STATIC_ASSERT_SIZEOF(TmdLitCornerRefs, 2 * sizeof(u32));

#endif // MAIN_TMD_STREAM_TYPES_H
