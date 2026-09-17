#ifndef ACTOR_201200_H
#define ACTOR_201200_H

#include "common.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Work block this overlay parks in `Actor201200::field_1C`. `field_0` is the
/// substate the message handler below switches on; the three bytes at 0x194
/// are the message echo the dispatcher copies in for every 0xB02 message.
typedef struct Actor201200Work {
    /* 0x000 */ s16   field_0;
    /* 0x002 */ s16   field_2;
    /* 0x004 */ s16   field_4;
    /* 0x006 */ byte  pad_6[0x52];
    /* 0x058 */ u16   field_58;
    /* 0x05A */ byte  pad_5A[0x116];
    /* 0x170 */ s16   field_170;
    /* 0x172 */ s16   field_172;
    /* 0x174 */ s16   field_174;
    /* 0x176 */ s16   field_176;
    /* 0x178 */ s16   field_178;
    /* 0x17A */ s16   field_17A;
    /* 0x17C */ s16   field_17C;
    /* 0x17E */ byte  pad_17E[0x16];
    /* 0x194 */ u8    field_194;
    /* 0x195 */ u8    field_195;
    /* 0x196 */ u8    field_196;
    /* 0x197 */ byte  pad_197[0x99];
    /* 0x230 */ GpObj obj230;
    /* 0x250 */ byte  pad_250[0x78];
    /* 0x2C8 */ GpObj obj2C8;
    /* 0x2E8 */ byte  pad_2E8[0x18];
    /* 0x300 */ GpObj obj300;
    /* 0x320 */ byte  pad_320[0x18];
    /* 0x338 */ GpObj obj338;
} Actor201200Work;

/// Context block at `Actor201200::field_20`; `field_40` is the counter the
/// message handler tests before raising the substate.
typedef struct Actor201200Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s8   field_14; // cleared by the state handlers
    /* 0x15 */ byte pad_15[0x2B];
    /* 0x40 */ s16  field_40;
} Actor201200Ctx;

typedef struct Actor201200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor201200Work* field_1C;
    /* 0x20 */ Actor201200Ctx*  field_20;
    /* 0x24 */ byte             pad_24[0x8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor201200;

/// Message handed to the handler: a type word that selects the actor and a
/// command word, over the same four bytes the handler also copies out one at
/// a time.
typedef union Actor201200Msg {
    struct {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
    } bytes;
    struct {
        u16 type;
        u16 cmd;
    } words;
} Actor201200Msg;

/// 0xC-byte scratch taken from `0x1F8003FC` for the player-in-radius test:
/// the X/Z offset to the camera target and the radius, squared in place.
typedef struct Actor201200RangeScratch {
    /* 0x0 */ s32 dx;
    /* 0x4 */ s32 dz;
    /* 0x8 */ s32 r;
} Actor201200RangeScratch;

s32 func_actor_201200_8014D8DC(Actor201200* arg0, s32 arg1, Actor201200Msg* arg2);

#endif
