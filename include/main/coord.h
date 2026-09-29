#ifndef MAIN_COORD_H
#define MAIN_COORD_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Values used to invalidate and inspect a coordinate's composition cache.
enum {
    GRAPHICS_COORD_DIRTY          = 0,
    GRAPHICS_COORD_SUPPLIED_CACHE = 1, // Nonzero stamp preserving a caller-supplied cache on a parentless node
    GRAPHICS_COORD_STAMP_MASK     = 0x7FFFFFFF,
    GRAPHICS_COORD_PARITY_BIT     = 0x80000000
};

/// A graphics transform node with a local matrix and a cached composition through its ancestors.
///
/// `coord` maps into the parent's space. `workm` maps into the space at which
/// composition stops: normally view space for models beneath `gGfxViewCoord`,
/// or the space of an explicitly excluded ancestor. Rebuilding a parentless
/// node copies its local matrix; a supplied nonzero-stamped cache is kept.
/// Changing the local matrix or parent requires clearing
/// `composeStamp`; `param.rot` is optional stored state, not automatically
/// applied by the composition pass.
///
/// Models own one node per part in their allocation; displays, lights and
/// scratch calculations also carry nodes. Parent links are borrowed, must
/// remain live during composition, and must form an acyclic chain. This is
/// the game's layout, not a libgs `GsCOORDINATE2`.
typedef struct GfxCoord {
    u32    composeStamp;     // Low 31 bits: last rebuild stamp (0 requests rebuilding); bit 31: last visiting pass's parity
    MATRIX coord;            // Local-to-parent transform; m is 12-fractional-bit fixed point, t is signed game coordinates
    MATRIX workm;            // Cached local-to-composition-root transform; includes the view for ordinary model nodes
    union {
        SVECTOR rot;         // Optional local Euler angles, 0x1000 units per turn; rotation order belongs to the caller
        s16     clearFlags;  // Attached model's first update: 0 preserves display flags, 1 clears them (any nonzero is tested)
    } param;                 // Alternative owner-managed state; lights give these bytes their own interpretation
    struct GfxCoord* parent; // Borrowed parent transform, or NULL at the top of the chain
} GfxCoord;
STATIC_ASSERT_SIZEOF(GfxCoord, 0x50);

#endif // MAIN_COORD_H
