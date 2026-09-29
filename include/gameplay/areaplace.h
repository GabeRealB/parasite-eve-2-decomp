#ifndef GAMEPLAY_AREAPLACE_H
#define GAMEPLAY_AREAPLACE_H

#include "common.h"

/// End marker for an area placement's resource-entry ID.
enum { AREA_PLACEMENT_END = 0xFF };

/// An actor placement and its resource-loading parameters in an area layout.
///
/// `GpAreaVariant.field_0` points to a table terminated by `AREA_PLACEMENT_END`.
/// Area spawning matches `entryId` against `GpAreaTmdRec` task descriptors.
/// The actor receives `(variant << 16) | mode` as its first spawn argument;
/// both values and `rowIndex` have actor-specific interpretations.
///
/// The loader combines `fileIdLow` with the resource entry's file ID and applies
/// the same signed texture offsets to its images that the model uses for its
/// primitives. Positions are in world-coordinate units and yaw uses 4096 units
/// per turn. Saved actor poses can override this initial transform.
///
/// Actors borrow these records from the loaded area resource, or from their
/// own placement tables; the table must outlive every actor that refers to it.
typedef struct {
    u8  entryId;           // Resource-entry ID; AREA_PLACEMENT_END terminates the table
    u8  variant;           // Actor-defined spawn argument, bits 16..23
    u16 mode;              // Actor-defined spawn argument, bits 0..15
    s16 x;                 // Initial world X coordinate
    s16 y;                 // Initial world Y coordinate
    s16 z;                 // Initial world Z coordinate
    s16 yaw;               // Initial Y rotation, 4096 units per turn
    u8  fileIdLow;         // Low base-100 file-ID component (0 base resource, nonzero additional file)
    s8  texturePageOffset; // Signed texture-page offset, in 64-word VRAM columns
    s8  clutRowOffset;     // Signed CLUT row offset, added to the encoded CLUT as offset << 6
    u8  rowIndex;          // Actor-specific parameter-table row; must fit that actor's tables
} AreaPlacement;
STATIC_ASSERT_SIZEOF(AreaPlacement, 0x10);

#endif // GAMEPLAY_AREAPLACE_H
