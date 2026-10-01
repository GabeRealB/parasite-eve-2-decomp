#ifndef GAMEPLAY_PRIVATE_COLLISION_H
#define GAMEPLAY_PRIVATE_COLLISION_H

#include "common.h"

#include "gameplay/actor.h"

/// Edge endpoint pair at `Gp_FaceEdgePairs`, indexing the transformed corners of a
/// `WorldCollisionGridFace`. Entries 0..2 are the edges of a triangle; entries 1..4 are the
/// edges of a quad, so a face with `n` corners walks entries `n - 3` up to
/// `n * 2 - 3`.
typedef struct _GpEdgePair {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
} GpEdgePair;
STATIC_ASSERT_SIZEOF(GpEdgePair, 0x4);

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
