#ifndef GAMEPLAY_PRIVATE_TMD_STREAM_TYPES_H
#define GAMEPLAY_PRIVATE_TMD_STREAM_TYPES_H

#include "common.h"

/// Read-only stream prefix for one corner shared by a texture layer and its base.
///
/// Environment and offset-layer projection use the same four unsigned halfwords.
/// Elements are word-aligned, with a u32-word stride of at least two; this type
/// describes only the prefix, not any remaining words. Geometry loads discard
/// the low three reference bits, whose role is unproven. Masked references must
/// select complete SVECTORs, and vertexByteRef >> 3 must fit the draw walk's
/// 1024-entry depth cache. The raw vertex reference keys adjacent projection reuse.
///
/// Both destinations are aligned byte offsets from
/// `TmdStreamWorkspace::preXformWrite` into its first packet region. Each starts
/// at a four-byte RGB/code group, followed by four-byte screen XY. Environment
/// mapping also writes the layer's following U/V bytes, requiring ten bytes
/// there; full-screen mapping uses the code byte as a temporary 0/1 page marker.
/// The base requires eight bytes. Stream storage is borrowed only during the
/// projection pass, independently of the writable destination packets.
typedef struct {
    u16 vertexByteRef;        // Vertex byte reference; value & 0xFFF8 loads geometry, value >> 3 indexes depth
    u16 normalByteRef;        // Normal byte reference; value & 0xFFF8 selects the loaded SVECTOR
    u16 layerColorByteOffset; // Layer RGB/code destination; XY follows and environment U/V follows XY
    u16 baseColorByteOffset;  // Base RGB/code destination; the same projected XY follows
} TmdLayerCornerRefs;
STATIC_ASSERT_SIZEOF(TmdLayerCornerRefs, 2 * sizeof(u32));

#endif // GAMEPLAY_PRIVATE_TMD_STREAM_TYPES_H
