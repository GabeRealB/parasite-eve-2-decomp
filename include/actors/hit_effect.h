#ifndef ACTORS_HIT_EFFECT_H
#define ACTORS_HIT_EFFECT_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// A hit-effect attachment: local XYZ and the actor model part containing it.
///
/// XYZ are signed integer game-coordinate units in `coords[partIndex]`'s local
/// space. The selector indexes the actor's `TmdObject::coords` and must be in
/// [0, partCount); it has no sentinel value and identifies no model on its own.
///
/// Both members view the same eight bytes. `position` supplies the SDK vector
/// accepted by the effect API, which uses only XYZ; `position.pad` aliases
/// `anchor.partIndex`. Copy the complete record to preserve the selector.
typedef union {
    SVECTOR position;  // Effect API's local XYZ view; pad carries the model-part selector
    struct {
        s16 x;         // X in the selected part's local game-coordinate units
        s16 y;         // Y in the selected part's local game-coordinate units
        s16 z;         // Z in the selected part's local game-coordinate units
        s16 partIndex; // Nonnegative index into the actor model's live coordinates
    } anchor;          // Complete attachment record used by hit-position presets
} ActorHitAnchor;
STATIC_ASSERT_SIZEOF(ActorHitAnchor, sizeof(SVECTOR));

#endif // ACTORS_HIT_EFFECT_H
