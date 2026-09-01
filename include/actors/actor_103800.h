#ifndef ACTOR_103800_H
#define ACTOR_103800_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `Gp_UnlinkNode` list entry at +0x10 of `Actor103800Ctx`. `field_4` is the
/// flag byte `Actor03800_Fn031B8` writes (`sb` at 0x14).
typedef struct Actor103800Node {
    /* 0x0 */ struct Actor103800Node* next;
    /* 0x4 */ u8                      field_4;
    /* 0x5 */ byte                    pad_5[3];
} Actor103800Node;
STATIC_ASSERT_SIZEOF(Actor103800Node, 0x8);

typedef struct Actor103800Obj2C {
    /* 0x00 */ byte           pad_0[0x8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ u16            field_C;
} Actor103800Obj2C;

typedef struct Actor103800Work {
    /* 0x000 */ byte           pad_0[0x1A4];
    /* 0x1A4 */ byte           field_1A4[0x68];
    /* 0x20C */ byte           field_20C[0x12];
    /* 0x21E */ s16            field_21E;
    /* 0x220 */ byte           pad_220[8];
    /* 0x228 */ s16            field_228;
    /* 0x22A */ byte           pad_22A[0x62];
    /* 0x28C */ byte           field_28C[0x40];
    /* 0x2CC */ MATRIX         field_2CC;
    /* 0x2EC */ byte           pad_2EC[0x58];
    /* 0x344 */ GsCOORDINATE2* field_344;
    /* 0x348 */ u16            field_348;
    /* 0x34A */ s16            field_34A;
    /* 0x34C */ u16            field_34C;
    /* 0x34E */ byte           pad_34E[2];
    /* 0x350 */ s16            field_350;
    /* 0x352 */ s16            field_352;
    /* 0x354 */ s16            field_354;
    /* 0x356 */ s16            field_356;
    /* 0x358 */ s16            field_358;
    /* 0x35A */ s16            field_35A;
    /* 0x35C */ s16            field_35C;
    /* 0x35E */ s16            field_35E;
    /* 0x360 */ s16            field_360;
    /* 0x362 */ s16            field_362;
    /* 0x364 */ s16            field_364;
    /* 0x366 */ byte           pad_366[2];
    /* 0x368 */ s16            field_368;
    /* 0x36A */ s16            field_36A;
    /* 0x36C */ s16            field_36C;
    /* 0x36E */ s16            field_36E;
    /* 0x370 */ s16            field_370;
    /* 0x372 */ s16            field_372;
    /* 0x374 */ byte           pad_374[4];
    /* 0x378 */ s16            field_378;
    /* 0x37A */ s16            field_37A;
} Actor103800Work;
STATIC_ASSERT_SIZEOF(Actor103800Work, 0x37C);

typedef struct Actor103800Ctx {
    /* 0x00 */ byte            pad_0[8];
    /* 0x08 */ u16             field_8;
    /* 0x0A */ byte            pad_A[6];
    /* 0x10 */ Actor103800Node node;
    /* 0x18 */ byte            pad_18[0x34];
    /* 0x4C */ u8              field_4C;
    /* 0x4D */ byte            pad_4D[7];
    /* 0x54 */ s32             field_54;
} Actor103800Ctx;
STATIC_ASSERT_SIZEOF(Actor103800Ctx, 0x58);

typedef struct Actor103800 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor103800Work*  field_1C;
    /* 0x20 */ Actor103800Ctx*   field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor103800Obj2C* field_2C;
} Actor103800;
STATIC_ASSERT_SIZEOF(Actor103800, 0x30);

extern s16 Actor03800_D05F90[];

void Actor03800_Fn02998(Actor103800Ctx* arg0, Actor103800* arg1);
void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1);
void Actor03800_Fn032D8(Actor103800* arg0);

#endif
