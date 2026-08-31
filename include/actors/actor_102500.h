#ifndef ACTOR_102500_H
#define ACTOR_102500_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor02500Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor02500Obj2C;

typedef struct Actor02500Work {
    /* 0x000 */ byte   pad_0[0x16C];
    /* 0x16C */ byte   field_16C[0x38];
    /* 0x1A4 */ byte   field_1A4[0x68];
    /* 0x20C */ byte   field_20C[0x98];
    /* 0x2A4 */ byte   field_2A4[0x40];
    /* 0x2E4 */ MATRIX field_2E4;
    /* 0x304 */ byte   pad_304[0x18];
    /* 0x31C */ s16    field_31C;
    /* 0x31E */ byte   pad_31E[4];
    /* 0x322 */ s16    field_322;
    /* 0x324 */ s16    field_324;
    /* 0x326 */ byte   pad_326[2];
    /* 0x328 */ s16    field_328;
    /* 0x32A */ u16    field_32A;
    /* 0x32C */ s16    field_32C;
    /* 0x32E */ s16    field_32E;
    /* 0x330 */ byte   pad_330[2];
    /* 0x332 */ s16    field_332;
    /* 0x334 */ byte   pad_334[8];
    /* 0x33C */ s16    field_33C;
} Actor02500Work;

typedef struct Actor02500 {
    /* 0x00 */ byte                  pad_0[0x1C];
    /* 0x1C */ Actor02500Work*       field_1C;
    /* 0x20 */ struct Actor02500Ctx* field_20;
    /* 0x24 */ byte                  pad_24[8];
    /* 0x2C */ Actor02500Obj2C*      field_2C;
    /* 0x30 */ s32                   field_30;
} Actor02500;

/// `Gp_UnlinkNode` list entry at +0x10 of `Actor02500Ctx`. `field_4` is the
/// flag byte `Actor02500_Fn01E60` writes (`sb` at 0x14).
typedef struct Actor02500Node {
    /* 0x0 */ struct Actor02500Node* next;
    /* 0x4 */ u8                     field_4;
    /* 0x5 */ byte                   pad_5[3];
} Actor02500Node;
STATIC_ASSERT_SIZEOF(Actor02500Node, 0x8);

typedef struct Actor02500Ctx {
    /* 0x00 */ byte           pad_0[0x10];
    /* 0x10 */ Actor02500Node node;
    /* 0x18 */ byte           pad_18[0x34];
    /* 0x4C */ u8             field_4C;
    /* 0x4D */ byte           pad_4D[7];
    /* 0x54 */ s32            field_54;
} Actor02500Ctx;
STATIC_ASSERT_SIZEOF(Actor02500Ctx, 0x58);

/// 0x18-byte frame this overlay allocates on the scratchpad stack; only the
/// `SVECTOR` at +0x10 is used by `Actor02500_Fn016FC`.
typedef struct Actor02500RotScratch {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR rot;
} Actor02500RotScratch;
STATIC_ASSERT_SIZEOF(Actor02500RotScratch, 0x18);

extern s16 Actor02500_D05B88[];

void Actor02500_Fn01AC8(Actor02500Ctx* arg0, Actor02500* arg1);
void Actor02500_Fn01E60(Actor02500Ctx* arg0, Actor02500* arg1);

#endif
