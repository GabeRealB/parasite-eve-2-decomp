#ifndef ACTORS_SHARED_80168D3C_H
#define ACTORS_SHARED_80168D3C_H

#include "common.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Status flags at `ActorsShared80168d3cWork` + 0xEC, read through two
/// widths: guards test bit 0 as a halfword and then bits 0x102 as a word
/// (`ActorsShared8016974c` is the out-of-line copy of the test).
typedef union ActorsShared80168d3cFlags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
} ActorsShared80168d3cFlags;
STATIC_ASSERT_SIZEOF(ActorsShared80168d3cFlags, 0x4);

/// 0x454-byte work block `actor_341700` and `actor_342400` allocate with
/// `Mem_Calloc` and park in `Task::idMap` (that slot is not a `TaskIdMap`
/// here). `field_92` is the low half of the model root `coord.t[1]`,
/// `obj_2AC` / `obj_2CC` / `obj_3AC` the `Gp_LinkObj` nodes the death handler
/// unlinks, `field_412` the per-state frame counter, `field_422` the sub-state
/// index the handler table walks, and `field_414` .. `field_426` the animation
/// request the actor hands to its player. The size below is the allocation,
/// not a guess.
typedef struct ActorsShared80168d3cWork {
    /* 0x000 */ MATRIX                    mat_0;    // copied into the model root coord by ActorsShared8016bd98
    /* 0x020 */ byte                      pad_20[0x40];
    /* 0x060 */ VECTOR                    field_60; // own position, copied from the model root coord.t
    /* 0x070 */ byte                      pad_70[0x8];
    /* 0x078 */ s16                       field_78;
    /* 0x07A */ s16                       field_7A; // heading
    /* 0x07C */ s16                       field_7C;
    /* 0x07E */ byte                      pad_7E[0xA];
    /* 0x088 */ s16                       field_88; // x of the vector turned towards
    /* 0x08A */ s16                       field_8A; // y of the vector turned towards
    /* 0x08C */ s16                       field_8C; // z of the vector turned towards
    /* 0x08E */ byte                      pad_8E[0x4];
    /* 0x092 */ u16                       field_92; // low half of root coord.t[1]
    /* 0x094 */ byte                      pad_94[0x58];
    /* 0x0EC */ ActorsShared80168d3cFlags flags_EC;
    /* 0x0F0 */ byte                      pad_F0[0x1BC];
    /* 0x2AC */ GpObj                     obj_2AC;
    /* 0x2CC */ GpObj                     obj_2CC;
    /* 0x2EC */ GpRec18                   rec_2EC[8];
    /* 0x3AC */ GpObj                     obj_3AC;
    /* 0x3CC */ byte                      pad_3CC[0x40];
    /* 0x40C */ s16                       field_40C; // heading of the fall push
    /* 0x40E */ byte                      pad_40E[0x4];
    /* 0x412 */ u16                       field_412; // per-state frame counter
    /* 0x414 */ s16                       field_414; // animation request kind
    /* 0x416 */ byte                      pad_416[0x2];
    /* 0x418 */ s16                       field_418; // animation id
    /* 0x41A */ byte                      pad_41A[0x2];
    /* 0x41C */ s16                       field_41C; // animation speed / step scale
    /* 0x41E */ s16                       field_41E;
    /* 0x420 */ u16                       field_420; // state index
    /* 0x422 */ u16                       field_422; // sub-state index
    /* 0x424 */ s16                       field_424; // yaw added to model parts 3..5, a third each
    /* 0x426 */ s16                       field_426;
    /* 0x428 */ s16                       field_428;
    /* 0x42A */ s16                       field_42A;
    /* 0x42C */ byte                      pad_42C[0x4];
    /* 0x430 */ u16                       field_430; // Y scale, shrunk by 0x40 a frame
    /* 0x432 */ s16                       field_432;
    /* 0x434 */ s16                       field_434; // pitch latched by ActorsShared8016a538
    /* 0x436 */ byte                      pad_436[0x2];
    /* 0x438 */ s16                       field_438;
    /* 0x43A */ s16                       field_43A; // distance to the nearer player actor
    /* 0x43C */ byte                      pad_43C[0x4];
    /* 0x440 */ s16                       field_440;
    /* 0x442 */ s16                       field_442; // phase of the pitch sway
    /* 0x444 */ s16                       field_444; // heading to the nearer player actor, relative to field_7A
    /* 0x446 */ byte                      pad_446[0x2];
    /* 0x448 */ s16                       field_448;
    /* 0x44A */ s16                       field_44A;
    /* 0x44C */ byte                      pad_44C[0x5];
    /* 0x451 */ u8                        field_451;
    /* 0x452 */ byte                      pad_452[0x2];
} ActorsShared80168d3cWork;
STATIC_ASSERT_SIZEOF(ActorsShared80168d3cWork, 0x454);

/// Seed the next sub-state from the model root Y, request animation 8 at
/// speed 0x10, clear the frame counter and two motion halfwords, and advance
/// `field_422`. `Actor04400_Fn06EEC` is the same body inside the slot-1/slot-3
/// `actor_104400_text` object, which cannot join this unit: that overlay's
/// whole `.text` is already one shared span.
void ActorsShared80168d3c(Task* arg0);

#endif
