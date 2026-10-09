#ifndef ACTORS_HIT_EFFECT_H
#define ACTORS_HIT_EFFECT_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// A part-local hit-effect position and the model coordinate it belongs to.
///
/// The effect API reads XYZ through the SDK vector view and ignores its fourth
/// halfword. The actor reads that halfword as the coordinate index instead.
/// Each eight-byte record is copied intact; the index must address a live part.
typedef union {
    SVECTOR position;  // SDK XYZ view; the fourth halfword carries the part selector
    struct {
        s16 x;         // Local X in model-coordinate units
        s16 y;         // Local Y in model-coordinate units
        s16 z;         // Local Z in model-coordinate units
        s16 partIndex; // Model coordinate index (2, 7 or 9 in the hit tables)
    } anchor;
} ActorHitAnchor;
STATIC_ASSERT_SIZEOF(ActorHitAnchor, sizeof(SVECTOR));

#endif // ACTORS_HIT_EFFECT_H
