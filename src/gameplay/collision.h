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
/// Dispatch selects the function from `Gp_PairHandlers` with
/// `WorldCollisionPairRule::handlerIndex` and passes that index as `handler`.
/// `first` and `second` are the bodies in the order that rule specifies;
/// `WorldCollisionPairRule::swapBodies` exchanges them before the call.
/// Returns 1 when a contact was recorded and 0 otherwise. Dispatch
/// discards the result, and the installed functions do not read `handler`.
///
/// The index is 0 for two proxies or two capsules, 1 for two spheres (a motion
/// sphere counts as a sphere), 2 for a sphere or motion sphere and a contact
/// proxy, 3 for a sphere or motion sphere in `first` against a capsule in
/// `second`, and 4 for a capsule and a contact proxy. Indices 0, 2 and 4 do
/// nothing.
typedef s32 (*WorldCollisionPairHandler)(WorldCollisionBody* first, WorldCollisionBody* second, s32 handler);

/// Handler-table indices stored in `WorldCollisionPairRule::handlerIndex`.
///
/// Motion spheres use the sphere routes. NONE, SPHERE_PROXY and CAPSULE_PROXY
/// currently select no-op handlers; the other two routes test and record contacts.
enum {
    WORLD_COLLISION_PAIR_HANDLER_NONE           = 0,
    WORLD_COLLISION_PAIR_HANDLER_SPHERES        = 1,
    WORLD_COLLISION_PAIR_HANDLER_SPHERE_PROXY   = 2,
    WORLD_COLLISION_PAIR_HANDLER_SPHERE_CAPSULE = 3,
    WORLD_COLLISION_PAIR_HANDLER_CAPSULE_PROXY  = 4
};

/// Handler selection and argument order for one ordered pair of collision-body kinds.
///
/// The matrix row and column are the respective body kind minus 1: sphere,
/// contact proxy, capsule, then motion sphere. Both bodies must have kinds 1..4.
/// Spheres include motion spheres. Mixed routes put the sphere before the
/// capsule or proxy, and the capsule before the proxy. Same-kind routes retain
/// the row body first. The dispatchers only read these rules and pass the
/// handler index as the handler's third argument.
typedef struct {
    u16 handlerIndex; // Handler slot (0 none, 1 spheres, 2 sphere/proxy, 3 sphere/capsule, 4 capsule/proxy)
    u16 swapBodies;   // Argument order (0 row body first, 1 column body first); any nonzero value exchanges them
} WorldCollisionPairRule;
STATIC_ASSERT_SIZEOF(WorldCollisionPairRule, 0x4);

#endif // GAMEPLAY_PRIVATE_COLLISION_H
