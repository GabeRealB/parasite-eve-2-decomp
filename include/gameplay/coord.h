#ifndef GAMEPLAY_COORD_H
#define GAMEPLAY_COORD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A task's coordinate node, as `TmdObject::coords` points at it: an array of
/// them, one per part, on a model body, and a single one on a 2D-display body.
///
/// The leading three fields are libgs `GsCOORDINATE2`'s, and the coordinate
/// pass reads a node through them: `flg` stamps the pass that last composed
/// it, `coord` is its local matrix and `workm` the world matrix composed from
/// that and the parent's.
///
/// libgs keeps a parameter pointer and a `super` link in the words after the
/// matrices, and the game uses neither that way. Most nodes hold the Euler
/// angles `coord` is rebuilt from there. The player's attached weapon models
/// use the first halfword as a flag instead, which when set makes the node's
/// first update clear the model's display flags. The parent link is `sub`.
typedef struct GpCoordExt {
    u32    flg;             // Frame stamp: low 31 bits the frame the node was composed in, bit 31 that frame's parity
    MATRIX coord;           // Local matrix: where the body sits, in the parent's space
    MATRIX workm;           // World matrix, composed from `coord` and the parent's `workm`
    union {
        SVECTOR rot;        // Euler angles `coord` is rebuilt from
        s16     clearFlags; // Attached weapon models: clear the model's display flags on the first update
    } param;
    GsCOORDINATE2* sub;     // Parent coordinate, or NULL at the root
} GpCoordExt;
STATIC_ASSERT_SIZEOF(GpCoordExt, 0x50);

#endif
