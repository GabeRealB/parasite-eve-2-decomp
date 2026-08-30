#ifndef ACTOR_205200_H
#define ACTOR_205200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor205200Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor205200Obj2C;

typedef struct Actor205200Work {
    /* 0x000 */ byte pad_0[0x594];
    /* 0x594 */ s16  field_594;
} Actor205200Work;

typedef struct Actor205200 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor205200Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor205200Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor205200;

/// Owning context; this overlay's update entry point does not touch it.
typedef struct Actor205200Ctx Actor205200Ctx;

void func_actor_205200_8014C59C(Actor205200Ctx* arg0, Actor205200* arg1);

#endif
