#ifndef GAMEPLAY_PRIVATE_COLLISION_H
#define GAMEPLAY_PRIVATE_COLLISION_H

#include "common.h"

#include "gameplay/actor.h"

/// Ordered local corner indices defining an edge of a collision polygon.
///
/// Indices address the polygon's transformed corners, not the grid's vertex
/// pool. The edge vector is the end corner minus the start corner. Crossing
/// the face normal with that vector gives the edge's separating-plane normal.
/// `Gp_FaceEdgePairs` entries 0..2 form a triangle's edges (0..2 corners), and
/// entries 1..4 form a quad's edges (0..3 corners). No index is a sentinel.
typedef struct {
    s16 endCornerIndex;   // End index in the transformed corner array (0..2 triangle, 0..3 quad)
    s16 startCornerIndex; // Start index in the same array; subtracted from the end corner
} WorldCollisionFaceEdge;
STATIC_ASSERT_SIZEOF(WorldCollisionFaceEdge, 0x4);

/// Pair-dispatch callback from `Gp_PairHandlers`. `kind` is the `handler` of
/// the `GpPairRule` that selected it.
typedef s32 (*GpPairFn)(WorldCollisionBody* a, WorldCollisionBody* b, s32 kind);

/// One rule of the pair-rule table `D_8010FA4C`: what the collision passes do
/// when a body of the kind its row names meets one of the kind its column
/// names.
typedef struct {
    u16 handler; // index into Gp_PairHandlers
    u16 swap;    // non-zero: run the handler with the two bodies exchanged
} GpPairRule;
STATIC_ASSERT_SIZEOF(GpPairRule, 0x4);

#endif // GAMEPLAY_PRIVATE_COLLISION_H
