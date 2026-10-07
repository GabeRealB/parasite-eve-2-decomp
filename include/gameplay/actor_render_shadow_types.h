#ifndef GAMEPLAY_ACTOR_RENDER_SHADOW_TYPES_H
#define GAMEPLAY_ACTOR_RENDER_SHADOW_TYPES_H

#include "common.h"

/// Scratch-stack reservation for a model root's ground-shadow centre.
///
/// `centre` holds the three signed translation words of the composed root,
/// in its composition space: view space for ordinary view-parented models.
/// The ground-shadow renderer borrows it for one call. Reserve and release
/// the full 24-byte block in stack order; only the first 12 bytes are accessed
/// by these drawers, and the second half's role is unproven.
typedef struct {
    VECTOR3 centre;       // Composed root translation supplied to the ground-shadow renderer
    byte    field_C[0xC]; // Reserved but never accessed by these drawers; role unproven
} ActorRenderGroundShadowCentreScratch;
STATIC_ASSERT_SIZEOF(ActorRenderGroundShadowCentreScratch, 0x18);

#endif // GAMEPLAY_ACTOR_RENDER_SHADOW_TYPES_H
