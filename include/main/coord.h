#ifndef MAIN_COORD_H
#define MAIN_COORD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// A coordinate node: one frame of the game's transform hierarchy. A model
/// body carries an array of them, one per part, and a 2D-display body a single
/// one; the view and every other attachment point are nodes too.
///
/// The layout starts like libgs's `GsCOORDINATE2`, and the game composes nodes
/// with its own pass rather than libgs's: `flg` stamps the pass that last
/// composed the node, `coord` is its local matrix and `workm` the world matrix
/// composed from that and the parent's. Where libgs keeps a parameter pointer
/// and a `super` link, the game keeps the Euler angles `coord` is rebuilt from,
/// or, on the player's attached weapon models, a flag that makes the node's
/// first update clear the model's display flags. The parent link is `sub`.
typedef struct GpCoord {
    u32    flg;             // Frame stamp: low 31 bits the frame the node was composed in, bit 31 that frame's parity
    MATRIX coord;           // Local matrix: where the body sits, in the parent's space
    MATRIX workm;           // World matrix, composed from `coord` and the parent's `workm`
    union {
        SVECTOR rot;        // Euler angles `coord` is rebuilt from
        s16     clearFlags; // Attached weapon models: clear the model's display flags on the first update
    } param;
    struct GpCoord* sub;    // Parent coordinate, or NULL at the root
} GpCoord;
STATIC_ASSERT_SIZEOF(GpCoord, 0x50);

#endif
