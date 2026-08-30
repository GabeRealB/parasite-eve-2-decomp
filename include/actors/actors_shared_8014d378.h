#ifndef ACTORS_SHARED_8014D378_H
#define ACTORS_SHARED_8014D378_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared8014d378Obj2C {
    /* 0x0 */ byte           pad_0[0x8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ s16            field_C;
} ActorShared8014d378Obj2C;

typedef struct ActorShared8014d378Work {
    /* 0x000 */ byte pad_0[0x3A6];
    /* 0x3A6 */ s16  field_3A6;
    /* 0x3A8 */ byte pad_3A8[0x8];
    /* 0x3B0 */ s16  field_3B0;
} ActorShared8014d378Work;

typedef struct ActorShared8014d378 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared8014d378Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared8014d378Obj2C* field_2C;
} ActorShared8014d378;

typedef struct ActorShared8014d378Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} ActorShared8014d378Ctx;

void ActorsShared8014d378(ActorShared8014d378Ctx* arg0, ActorShared8014d378* arg1);

#endif
