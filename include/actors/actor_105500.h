#ifndef ACTOR_105500_H
#define ACTOR_105500_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/3A34.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor105500Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor105500Obj2C;

typedef struct Actor105500Work {
    /* 0x000 */ GpObj     obj;
    /* 0x020 */ GpRec18   rec;
    /* 0x038 */ s16       field_38;
    /* 0x03A */ s16       field_3A;
    /* 0x03C */ byte      pad_3C[0x318];
    /* 0x354 */ GpEffArg  field_354;
    /* 0x35C */ VECTOR3   field_35C;
    /* 0x368 */ byte      pad_368[4];
    /* 0x36C */ TaskDesc* field_36C;
    /* 0x370 */ MATRIX    field_370;
    /* 0x390 */ byte      pad_390[2];
    /* 0x392 */ s16       field_392;
    /* 0x394 */ s16       field_394;
    /* 0x396 */ u16       field_396;
    /* 0x398 */ s16       field_398;
    /* 0x39A */ s16       field_39A;
    /* 0x39C */ s16       field_39C;
    /* 0x39E */ s16       field_39E;
    /* 0x3A0 */ s16       field_3A0;
    /* 0x3A2 */ s16       field_3A2;
    /* 0x3A4 */ s16       field_3A4;
    /* 0x3A6 */ s16       field_3A6;
    /* 0x3A8 */ s16       field_3A8;
    /* 0x3AA */ byte      pad_3AA[2];
    /* 0x3AC */ u16       field_3AC;
    /* 0x3AE */ byte      pad_3AE[2];
    /* 0x3B0 */ s16       field_3B0;
    /* 0x3B2 */ s16       field_3B2;
    /* 0x3B4 */ s16       field_3B4;
    /* 0x3B6 */ s16       field_3B6;
    /* 0x3B8 */ byte      pad_3B8[6];
    /* 0x3BE */ s16       field_3BE;
    /* 0x3C0 */ s16       field_3C0;
    /* 0x3C2 */ byte      pad_3C2[6];
    /* 0x3C8 */ s16       field_3C8;
    /* 0x3CA */ byte      pad_3CA[6];
    /* 0x3D0 */ s16       field_3D0;
    /* 0x3D2 */ s16       field_3D2;
} Actor105500Work;

typedef struct Actor105500 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor105500Work*  field_1C;
    /* 0x20 */ void*             field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor105500Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor105500;

typedef struct Actor105500RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor105500RotScratch;
STATIC_ASSERT_SIZEOF(Actor105500RotScratch, 0x18);

typedef struct Actor105500StateFuncTable3 {
    void (*funcs[3])(void*, Actor105500*);
} Actor105500StateFuncTable3;

extern Actor105500StateFuncTable3 Actor05500_D0002C;
extern Actor105500StateFuncTable3 Actor05500_D00038;

typedef struct Actor105500Params {
    /* 0x00 */ byte pad_0[0xF];
    /* 0x0F */ u8   field_F;
} Actor105500Params;

typedef struct Actor105500Ctx {
    /* 0x00 */ byte               pad_0[8];
    /* 0x08 */ u16                field_8;
    /* 0x0A */ byte               pad_A[6];
    /* 0x10 */ s32                field_10;
    /* 0x14 */ u8                 field_14;
    /* 0x15 */ byte               pad_15[0x27];
    /* 0x3C */ Actor105500Params* field_3C;
    /* 0x40 */ s16                field_40;
    /* 0x42 */ byte               pad_42[0xA];
    /* 0x4C */ u8                 field_4C;
} Actor105500Ctx;

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1);

extern u8  D_801153F2[2];
extern u32 Gp_LcgState;
extern u16 Actor05500_D08980[];
extern u16 Actor05500_D08990[];
extern s16 Actor05500_D08A1E;

extern GpU16Pair Actor05500_D08958;
extern s16       Actor05500_D08A18[];
void             Actor05500_Fn03E34(GpEnemy* enemy, Task* task);

#endif
