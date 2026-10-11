#ifndef MAIN_PRIVATE_DISPLAY_TYPES_H
#define MAIN_PRIVATE_DISPLAY_TYPES_H

/// Total bytes in the resident primitive arena shared by both frame halves.
enum { DISPLAY_STATIC_PRIMITIVE_ARENA_BYTES = 0x6000 };

/// Tags in each resident frame buffer, including the reserved foreground range.
///
/// Counts four-byte DMA tags, not bytes or a `GsOT` depth exponent. All 1088 tags
/// are cleared together: 32 precede the 1024 depth slots and 32 follow them.
/// Game submission begins at the final tag; task presentation can use the first
/// 1024 tags without the game's 32-tag bias.
#define GPU_ORDERING_TABLE_BUFFER_ENTRIES 0x440

/// Tags before the depth-sorted base; foreground drawing uses negative indices.
enum { GPU_ORDERING_TABLE_RESERVED_ENTRIES = 0x20 };

/// Gs ordering-table lengths are base-2 exponents; the small display uses 64 tags.
enum {
    GPU_ORDERING_TABLE_DEPTH_BITS       = 10,
    GPU_SMALL_ORDERING_TABLE_DEPTH_BITS = 6,
    GPU_SMALL_ORDERING_TABLE_ENTRIES    = 1 << GPU_SMALL_ORDERING_TABLE_DEPTH_BITS
};

#endif // MAIN_PRIVATE_DISPLAY_TYPES_H
