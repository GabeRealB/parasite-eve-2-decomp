#ifndef GAMEPLAY_ACTOR_SPAWN_TYPES_H
#define GAMEPLAY_ACTOR_SPAWN_TYPES_H

#include "common.h"

/// Special yaw words resolved when preparing the player's facing for a warp.
///
/// Both point-facing values aim toward world X/Z (-1473, 2497). Spawn routines
/// copy these values' low 16 bits unchanged; they do not resolve the choices.
enum {
    ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT = 0x7800,
    ACTOR_SPAWN_YAW_KEEP_FACING               = 0x7FFE,
    ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT     = 0x7FFF,
};

/// Initial world position and facing of a player or companion actor.
///
/// Coordinates are signed whole world-coordinate units. Yaw uses 4096 units
/// per turn and need not be normalized; warp tables may also store the special
/// `ACTOR_SPAWN_YAW_*` transition-facing choices.
///
/// Tables and saved-position restoration store a signed yaw word. On the
/// little-endian PS1, spawn routines read its unsigned low halfword and copy
/// those bits into the actor's signed yaw without masking. The upper halfword
/// belongs to the stored word and is not padding.
///
/// This 16-byte record has four-byte alignment. Initialize all four values;
/// spawn routines borrow it only until the call returns and retain no pointer.
typedef struct {
    union {
        s32 word;  // Stored yaw, or an ACTOR_SPAWN_YAW_* transition-facing choice
        u16 angle; // Low yaw bits consumed unchanged by actor initialization
    } yaw;         // Initial facing about Y, with word and halfword access widths
    s32 x;         // Signed world X coordinate
    s32 y;         // Signed world Y coordinate
    s32 z;         // Signed world Z coordinate
} ActorSpawnTransform;
STATIC_ASSERT_SIZEOF(ActorSpawnTransform, 0x10);

#endif // GAMEPLAY_ACTOR_SPAWN_TYPES_H
