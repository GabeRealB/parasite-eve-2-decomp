#ifndef ACTOR_103800_H
#define ACTOR_103800_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor103800Obj2C {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ s16  field_C;
} Actor103800Obj2C;

typedef struct Actor103800Work {
    /* 0x000 */ byte           pad_0[0x21E];
    /* 0x21E */ s16            field_21E;
    /* 0x220 */ byte           pad_220[8];
    /* 0x228 */ s16            field_228;
    /* 0x22A */ byte           pad_22A[0x11A];
    /* 0x344 */ GsCOORDINATE2* field_344;
    /* 0x348 */ s16            field_348;
    /* 0x34A */ byte           pad_34A[8];
    /* 0x352 */ s16            field_352;
    /* 0x354 */ s16            field_354;
    /* 0x356 */ s16            field_356;
    /* 0x358 */ byte           pad_358[4];
    /* 0x35C */ s16            field_35C;
    /* 0x35E */ s16            field_35E;
    /* 0x360 */ s16            field_360;
    /* 0x362 */ s16            field_362;
    /* 0x364 */ s16            field_364;
    /* 0x366 */ byte           pad_366[4];
    /* 0x36A */ s16            field_36A;
    /* 0x36C */ byte           pad_36C[2];
    /* 0x36E */ s16            field_36E;
} Actor103800Work;

typedef struct Actor103800 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor103800Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor103800Obj2C* field_2C;
} Actor103800;

typedef struct Actor103800Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
    /* 0x15 */ byte pad_15[0x37];
    /* 0x4C */ u8   field_4C;
} Actor103800Ctx;

void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1);
void Actor03800_Fn032D8(Actor103800* arg0);

#endif
