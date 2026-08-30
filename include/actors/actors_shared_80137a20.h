#ifndef ACTORS_SHARED_80137A20_H
#define ACTORS_SHARED_80137A20_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct ActorShared80137a20Coords {
    /* 0x000 */ GsCOORDINATE2 field_0;
    /* 0x050 */ byte          pad_50[0xA0];
    /* 0x0F0 */ GsCOORDINATE2 field_F0;
} ActorShared80137a20Coords;

typedef struct ActorShared80137a20Obj2C {
    /* 0x00 */ byte                       pad_0[8];
    /* 0x08 */ ActorShared80137a20Coords* field_8;
    /* 0x0C */ s16                        field_C;
} ActorShared80137a20Obj2C;

typedef struct ActorShared80137a20Work {
    /* 0x000 */ byte pad_0[0x49A];
    /* 0x49A */ u16  field_49A;
    /* 0x49C */ byte pad_49C[0x218];
    /* 0x6B4 */ s32  field_6B4;
    /* 0x6B8 */ byte pad_6B8[0x20];
    /* 0x6D8 */ s16  field_6D8;
    /* 0x6DA */ s16  field_6DA;
} ActorShared80137a20Work;

typedef struct ActorShared80137a20 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80137a20Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80137a20Obj2C* field_2C;
} ActorShared80137a20;

typedef struct ActorShared80137a20Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} ActorShared80137a20Ctx;

void ActorsShared80137a20(ActorShared80137a20Ctx* arg0, ActorShared80137a20* arg1);

#endif
