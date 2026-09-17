#ifndef ACTORS_SHARED_80133240_H
#define ACTORS_SHARED_80133240_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Per-actor work block behind `ActorsShared80133240Actor::field_1C`.
typedef struct ActorsShared80133240Work {
    /* 0x000 */ byte pad_0[0x128];
    /* 0x128 */ s16  field_128;
    /* 0x12A */ s16  field_12A;
    /* 0x12C */ s16  field_12C;
    /* 0x12E */ byte pad_12E[0xE];
    /* 0x13C */ s16  field_13C;
    /* 0x13E */ s16  field_13E;
    /* 0x140 */ s16  field_140;
} ActorsShared80133240Work;

typedef struct ActorsShared80133240Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
} ActorsShared80133240Obj2C;

typedef struct ActorsShared80133240Actor {
    /* 0x00 */ byte                       pad_0[0x1C];
    /* 0x1C */ ActorsShared80133240Work*  field_1C;
    /* 0x20 */ byte                       pad_20[0xC];
    /* 0x2C */ ActorsShared80133240Obj2C* field_2C;
} ActorsShared80133240Actor;

/// Declared as an array: a scalar extern lets sched1 hoist the load above
/// the work-field store (`fixed_scalar_and_varying_struct_p`).
extern u8  D_801153F2[2];
extern s8  D_8011540B;
extern u32 Gp_LcgState;

void ActorsShared80133240(ActorsShared80133240Actor* arg0);

#endif
