#ifndef ACTOR_201200_H
#define ACTOR_201200_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/tmd.h"

/// Work block this overlay parks in `Actor201200::field_1C`. `field_0` is the
/// substate the message handler below switches on; the three bytes at 0x194
/// are the message echo the dispatcher copies in for every 0xB02 message.
typedef struct Actor201200Work {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4;
    /* 0x006 */ s16        field_6; // frame counter within the substate
    /* 0x008 */ s16        field_8;
    /* 0x00A */ byte       pad_A[2];
    /* 0x00C */ GpAnimCtx  anim;
    /* 0x020 */ GpAnimSlot slots[1]; // `func_800B3F84` arg4; later slots overlap the fields below
    /* 0x048 */ byte       pad_48[0x10];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174;
    /* 0x176 */ s16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ s16        field_17A;
    /* 0x17C */ s16        field_17C;
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ byte       pad_180[0x14];
    /* 0x194 */ u8         field_194;
    /* 0x195 */ u8         field_195;
    /* 0x196 */ u8         field_196;
    /* 0x197 */ byte       pad_197[1];
    /* 0x198 */ u16        field_198;
    /* 0x19A */ u16        field_19A;
    /* 0x19C */ byte       pad_19C[0xC];
    /* 0x1A8 */ GpEffArg   eff1A8; // `func_800FDB18`'s argument record
    /* 0x1B0 */ SVECTOR    effOfs; // offset handed to `func_800FDB18`; `pad` picks the coordinate
    /* 0x1B8 */ GpRec18    rec1B8;
    /* 0x1D0 */ byte       pad_1D0[0x60];
    /* 0x230 */ GpObj      obj230;
    /* 0x250 */ GpRec18    rec250;
    /* 0x268 */ byte       pad_268[0x60];
    /* 0x2C8 */ GpObj      obj2C8;
    /* 0x2E8 */ GpRec18    rec2E8;
    /* 0x300 */ GpObj      obj300;
    /* 0x320 */ byte       pad_320[0x18];
    /* 0x338 */ GpObj      obj338;
    /* 0x358 */ SVECTOR    origin;        // model position at spawn
    /* 0x360 */ SVECTOR    patrol[2];     // spawn position plus (0) / minus (1) 1000 units along the facing (XZ)
    /* 0x370 */ byte       pad_370[4];
    /* 0x374 */ MATRIX     lightMtx;      // installed at `TmdObject.field_1C`
    /* 0x394 */ MATRIX     colorMtx;
    /* 0x3B4 */ MATRIX     savedColorMtx; // colorMtx as it was on entering the death state
    /* 0x3D4 */ byte       pad_3D4[0x4];
    /* 0x3D8 */ s8         field_3D8;     // nonzero rebuilds the color matrix each tick
    /* 0x3D9 */ byte       pad_3D9[7];
} Actor201200Work;
STATIC_ASSERT_SIZEOF(Actor201200Work, 0x3E0);

/// Context block at `Actor201200::field_20`; `field_40` is the counter the
/// message handler tests before raising the substate.
typedef struct Actor201200Ctx {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ u16  field_8;   // top nibble selects the sound bank
    /* 0x0A */ byte pad_A[0x6];
    /* 0x10 */ byte node[0x4]; // bound by `func_800DA6E8` on a hit
    /* 0x14 */ s8   field_14;  // cleared by the state handlers
    /* 0x15 */ byte pad_15[0x2B];
    /* 0x40 */ s16  field_40;  // hit points
    /* 0x42 */ byte pad_42[0x9];
    /* 0x4B */ s8   field_4B;  // cleared when the hit points run out
} Actor201200Ctx;

/// 0x18-byte scratch taken from `0x1F8003FC` by the hit check: the first
/// type-2 record's position, its offset from the model origin, the attack id,
/// the computed damage and the hit's yaw relative to the model's facing.
typedef struct Actor201200HitScratch {
    /* 0x00 */ SVECTOR d;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ s32     id;
    /* 0x14 */ u16     dmg;
    /* 0x16 */ s16     angle;
} Actor201200HitScratch;
STATIC_ASSERT_SIZEOF(Actor201200HitScratch, 0x18);

/// 0x34-byte scratch from `G_SCRATCH_HEAD` for the death state's facing
/// rebuild: the rotation, the uniform scale applied to it and the yaw.
typedef struct Actor201200FaceScratch {
    /* 0x00 */ MATRIX m;
    /* 0x20 */ VECTOR scale;
    /* 0x30 */ s16    angle;
} Actor201200FaceScratch;
STATIC_ASSERT_SIZEOF(Actor201200FaceScratch, 0x34);

struct Actor201200;

typedef void (*Actor201200StateFn)(Actor201200Ctx* arg0, struct Actor201200* arg1);

/// The ten substate handlers the tick copies onto its stack before dispatching.
typedef struct Actor201200StateTable {
    /* 0x00 */ Actor201200StateFn fn[10];
} Actor201200StateTable;

typedef struct Actor201200 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor201200Work* field_1C;
    /* 0x20 */ Actor201200Ctx*  field_20;
    /* 0x24 */ void*            field_24;
    /* 0x28 */ byte             pad_28[0x4];
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              state;
    /* 0x34 */ s16              field_34;
    /* 0x36 */ s16              field_36; // 1 starts in state 2, otherwise 7
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
