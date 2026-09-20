#ifndef ACTORS_SHARED_80133EB8_H
#define ACTORS_SHARED_80133EB8_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Animation-state view shared by actor_401000 and actor_401800. Each actor
/// owns a larger work block; these are the fields their animation driver uses.
typedef struct ActorsShared80133eb8Work {
    /* 0x000 */ byte      pad_0[0x1C];
    /* 0x01C */ GpAnimCtx anim;
    /* 0x030 */ byte      pad_30[0x428];
    /* 0x458 */ GpAnimCtx blendAnim;
    /* 0x46C */ byte      pad_46C[0x38];
    /* 0x4A4 */ u16       field_4A4;
    /* 0x4A6 */ byte      pad_4A6[0x3F2];
    /* 0x898 */ s16       field_898; // Pending clip change: cross-fade, reset, running
    /* 0x89A */ s16       field_89A; // Nonzero while pose blending is active
    /* 0x89C */ s16       field_89C; // Previous clip
    /* 0x89E */ s16       field_89E; // Requested clip
    /* 0x8A0 */ u16       field_8A0; // Frames since the clip change
    /* 0x8A2 */ s16       field_8A2; // Body slot rate
    /* 0x8A4 */ byte      pad_8A4[2];
    /* 0x8A6 */ s16       field_8A6; // Blend clip change request
    /* 0x8A8 */ s16       field_8A8; // Blend clip
    /* 0x8AA */ u16       field_8AA; // Blend slot rate
    /* 0x8AC */ s16       field_8AC; // Blend weight
    /* 0x8AE */ s16       field_8AE; // Target head yaw
    /* 0x8B0 */ s16       field_8B0; // Current head yaw
    /* 0x8B2 */ byte      pad_8B2[2];
    /* 0x8B4 */ s32       field_8B4; // Last animation event index
} ActorsShared80133eb8Work;
STATIC_ASSERT_SIZEOF(ActorsShared80133eb8Work, 0x8B8);

/// The task slots holding the work block, enemy record and model object.
typedef struct ActorsShared80133eb8Actor {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorsShared80133eb8Work* field_1C;
    /* 0x20 */ GpEnemy*                  field_20;
    /* 0x24 */ byte                      pad_24[8];
    /* 0x2C */ TmdObject*                field_2C;
} ActorsShared80133eb8Actor;
STATIC_ASSERT_SIZEOF(ActorsShared80133eb8Actor, 0x30);

/// Services clip changes, advances pose slots, eases head yaw and emits the
/// current animation's sound event. Local callbacks and transition tables
/// resolve separately in each carrier's symbol map.
void ActorsShared80133eb8(ActorsShared80133eb8Actor* actor);

#endif
