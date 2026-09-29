#ifndef MAIN_PRIVATE_DISPLAY_TYPES_H
#define MAIN_PRIVATE_DISPLAY_TYPES_H

/// Tags in each resident frame buffer, including the reserved foreground range.
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
