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

/// Tests one ordered pair of collision bodies and records any contact.
///
/// Dispatch selects the function from `Gp_PairHandlers` with `GpPairRule.handler`
/// and passes that same index as `handler`. `first` and `second` are the two
/// bodies in the order that rule specifies; `swap` exchanges them before the
/// call. Returns 1 when a contact was recorded and 0 otherwise. Dispatch
/// discards the result, and the installed functions do not read `handler`.
///
/// The index is 0 for two proxies or two capsules, 1 for two spheres (a motion
/// sphere counts as a sphere), 2 for a sphere or motion sphere and a contact
/// proxy, 3 for a sphere or motion sphere in `first` against a capsule in
/// `second`, and 4 for a capsule and a contact proxy. Indices 0, 2 and 4 do
/// nothing.
typedef s32 (*WorldCollisionPairHandler)(WorldCollisionBody* first, WorldCollisionBody* second, s32 handler);

/// One rule of the pair-rule table `D_8010FA4C`: what the collision passes do
/// when a body of the kind its row names meets one of the kind its column
/// names.
typedef struct {
    u16 handler; // index into Gp_PairHandlers
    u16 swap;    // non-zero: run the handler with the two bodies exchanged
} GpPairRule;
STATIC_ASSERT_SIZEOF(GpPairRule, 0x4);

#endif // GAMEPLAY_PRIVATE_COLLISION_H
