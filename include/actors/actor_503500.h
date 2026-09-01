#ifndef ACTOR_503500_H
#define ACTOR_503500_H

#include "common.h"

#include "gameplay/3A34.h"

/// Work block allocated by `func_actor_503500_80132430`
/// (`Mem_Calloc(0x48)`) and parked in that task's `Task::idMap` slot.
/// `func_actor_503500_801324EC` republishes the two matrices onto
/// `GameActorExt::field_1C` / `field_20` -- the colour/light matrix pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`
/// -- so the allocation is exactly two `MATRIX`es plus a small tail.
typedef struct Actor503500ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ byte   pad_40[0x4];
    /* 0x44 */ s8     field_44;
    /* 0x45 */ byte   pad_45[0x3];
} Actor503500ColorMtx;
STATIC_ASSERT_SIZEOF(Actor503500ColorMtx, 0x48);

/// Message payload at `D_actor_503500_8017655C`. `func_actor_503500_80132DEC`
/// fills it from the player actor: the three words are the translation of the
/// `GsCOORDINATE2` at `GameActorExt::field_8` (`MATRIX.t`), the three
/// halfwords the rotation triple at +0x50/+0x52/+0x54 of that task's `idMap`
/// block. `func_actor_503500_80132DD4` clears the position;
/// `func_actor_503500_80132E7C` hands the record to `Gp_DispatchMsg` as
/// message 0x3E9 while any position word is non-zero.
///
/// Size is bounded by the next `.bss` symbol in the overlay
/// (`D_actor_503500_80176574`, 0x18 bytes later).
typedef struct Actor503500MsgPos {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  rx;
    /* 0x12 */ s16  ry;
    /* 0x14 */ s16  rz;
    /* 0x16 */ byte pad_16[0x2];
} Actor503500MsgPos;
STATIC_ASSERT_SIZEOF(Actor503500MsgPos, 0x18);

typedef struct Actor503500Work {
    /* 0x000 */ byte pad_0[0xEC];
    /* 0x0EC */ s8   field_EC;
    /* 0x0ED */ s8   field_ED;
    /* 0x0EE */ byte pad_EE[0x2];
    /* 0x0F0 */ s8   field_F0;
    /* 0x0F1 */ byte pad_F1[0x6B];
    /* 0x15C */ s8   field_15C;
    /* 0x15D */ byte pad_15D[0xC4];
    /* 0x221 */ s8   field_221;
    /* 0x222 */ byte pad_222[0x58E];
    /* 0x7B0 */ s16  field_7B0;
    /* 0x7B2 */ u16  field_7B2;
    /* 0x7B4 */ byte pad_7B4[0x16];
    /* 0x7CA */ s16  field_7CA;
    /* 0x7CC */ byte pad_7CC[0x14];
    /* 0x7E0 */ s8   field_7E0;
} Actor503500Work;

typedef struct Actor503500 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor503500Work* field_1C;
    /* 0x20 */ GpObj5D*         field_20;
} Actor503500;

void func_actor_503500_8013F8AC(Actor503500* arg0);
void func_actor_503500_801440F0(Actor503500* arg0);

#endif
