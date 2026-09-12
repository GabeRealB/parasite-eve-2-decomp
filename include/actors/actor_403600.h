#ifndef ACTOR_403600_H
#define ACTOR_403600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block of the `actor_403600` task, parked in the task's `Task::idMap`
/// slot (that slot is not a `TaskIdMap` here). The display node at +0x108 is
/// the one the exit callback `func_actor_403600_80138C68` hands back to
/// `Gp_UnlinkObj`.
typedef struct Actor403600Work {
    /* 0x000 */ byte                 pad_0[0x108];
    /* 0x108 */ GpObj                obj;
    /* 0x128 */ byte                 pad_128[0x38C];
    /* 0x4B4 */ struct Actor403600** field_4B4;
    /* 0x4B8 */ GsCOORDINATE2        field_4B8;
    /* 0x508 */ GpObj                field_508;
    /* 0x528 */ byte                 pad_528[0x60];
    /* 0x588 */ GpObj                field_588;
    /* 0x5A8 */ byte                 pad_5A8[0x18];
    /* 0x5C0 */ GpObj                field_5C0;
    /* 0x5E0 */ byte                 pad_5E0[0x18];
    /* 0x5F8 */ GpRec18              field_5F8[4];
    /* 0x658 */ byte                 pad_658[0x58];
    /* 0x6B0 */ VECTOR               field_6B0;
    /* 0x6C0 */ byte                 pad_6C0[0x30];
    /* 0x6F0 */ VECTOR               field_6F0;
    /* 0x700 */ byte                 pad_700[8];
    /* 0x708 */ s16                  field_708;
    /* 0x70A */ byte                 pad_70A[6];
    /* 0x710 */ Task*                field_710;
    /* 0x714 */ byte                 pad_714[0x1C];
    /* 0x730 */ s16                  field_730;
    /* 0x732 */ s16                  field_732;
    /* 0x734 */ s16                  field_734;
    /* 0x736 */ s16                  field_736;
    /* 0x738 */ s16                  field_738;
    /* 0x73A */ s16                  field_73A;
    /* 0x73C */ s16                  field_73C;
    /* 0x73E */ s16                  field_73E;
    /* 0x740 */ byte                 pad_740[2];
    /* 0x742 */ s16                  field_742;
    /* 0x744 */ u16                  field_744;
    /* 0x746 */ s16                  field_746;
    /* 0x748 */ byte                 pad_748[2];
    /* 0x74A */ s16                  field_74A;
    /* 0x74C */ byte                 pad_74C[0xA];
    /* 0x756 */ s16                  field_756;
    /* 0x758 */ byte                 pad_758[6];
    /* 0x75E */ s16                  field_75E;
    /* 0x760 */ byte                 pad_760[4];
    /* 0x764 */ s16                  field_764;
    /* 0x766 */ s16                  field_766;
    /* 0x768 */ byte                 pad_768[6];
    /* 0x76E */ s16                  field_76E;
    /* 0x770 */ byte                 pad_770[4];
    /* 0x774 */ s16                  field_774;
    /* 0x776 */ s16                  field_776;
    /* 0x778 */ s16                  field_778;
    /* 0x77A */ s16                  field_77A;
    /* 0x77C */ s16                  field_77C;
    /* 0x77E */ byte                 pad_77E[6];
    /* 0x784 */ s16                  field_784;
    /* 0x786 */ s16                  field_786;
    /* 0x788 */ byte                 pad_788[2];
    /* 0x78A */ u16                  field_78A;
    /* 0x78C */ s16                  field_78C;
    /* 0x78E */ byte                 pad_78E[4];
    /* 0x792 */ s16                  field_792;
    /* 0x794 */ s16                  field_794;
    /* 0x796 */ byte                 pad_796[8];
    /* 0x79E */ u16                  field_79E;
    /* 0x7A0 */ byte                 pad_7A0[2];
    /* 0x7A2 */ s16                  field_7A2;
    /* 0x7A4 */ s16                  field_7A4;
    /* 0x7A6 */ s16                  field_7A6;
    /* 0x7A8 */ s16                  field_7A8;
    /* 0x7AA */ byte                 pad_7AA[2];
    /* 0x7AC */ s16                  field_7AC;
    /* 0x7AE */ byte                 pad_7AE[0xA];
} Actor403600Work;
STATIC_ASSERT_SIZEOF(Actor403600Work, 0x7B8);

typedef struct Actor403600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor403600Work* field_1C;
    /* 0x20 */ struct _GpEnemy* field_20;
    /* 0x24 */ byte             pad_24[6];
    /* 0x2A */ s16              field_2A;
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              field_30;
} Actor403600;

/// 0x18-byte block temporarily taken from `G_SCRATCH_HEAD` while projecting
/// an actor's origin and calculating its ordering-table depth.
typedef struct Actor403600ProjectScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     sxy;
    /* 0x0C */ s32     dp;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s32     otz;
} Actor403600ProjectScratch;
STATIC_ASSERT_SIZEOF(Actor403600ProjectScratch, 0x18);

typedef struct Actor403600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor403600Ctx;

void ActorsShared80131e24Sub1(Actor403600Ctx* arg0, Actor403600* arg1);
u8*  func_actor_403600_80138DCC(Actor403600* arg0);
s32  func_actor_403600_8013D9A8(Actor403600* arg0);
void func_actor_403600_8013DAF4(Actor403600* arg0, s32 arg1);
void func_actor_403600_8013E470(GsCOORDINATE2* arg0, s32* arg1, s32* arg2);

#endif
