#ifndef ACTOR_403600_H
#define ACTOR_403600_H

#include "common.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"

#include "main/task.h"

#include "gameplay/3A34.h"

/// Work block of the `actor_403600` task, parked in the task's `Task::work`
/// slot (that slot is not a `TaskIdMap` here). The display node at +0x108 is
/// the one the exit callback `func_actor_403600_80138C68` hands back to
/// `Gp_UnlinkObj`.
typedef struct Actor403600Work {
    /* 0x000 */ byte                 pad_0[0x108];
    /* 0x108 */ GpObj                obj;
    /* 0x128 */ byte                 pad_128[0x34C];
    /* 0x474 */ MATRIX               field_474;
    /* 0x494 */ MATRIX               field_494;
    /* 0x4B4 */ struct Actor403600** field_4B4;
    /* 0x4B8 */ GsCOORDINATE2        field_4B8;
    /* 0x508 */ GpObj                field_508;
    /* 0x528 */ GpRec18              field_528[4];
    /* 0x588 */ GpObj                field_588;
    /* 0x5A8 */ GpRec18              field_5A8[1];
    /* 0x5C0 */ GpObj                field_5C0;
    /* 0x5E0 */ byte                 pad_5E0[0x18];
    /* 0x5F8 */ GpRec18              field_5F8[4];
    /* 0x658 */ GpEffArg             field_658;
    /* 0x660 */ byte                 pad_660[0x50];
    /* 0x6B0 */ VECTOR               field_6B0;
    /* 0x6C0 */ GsCOORDINATE2*       field_6C0;
    /* 0x6C4 */ s16                  field_6C4;
    /* 0x6C6 */ s16                  field_6C6;
    /* 0x6C8 */ byte                 pad_6C8[0x20];
    /* 0x6E8 */ SVECTOR              field_6E8;
    /* 0x6F0 */ VECTOR               field_6F0;
    /* 0x700 */ s16                  field_700;
    /* 0x702 */ byte                 pad_702[6];
    /* 0x708 */ s16                  field_708;
    /* 0x70A */ s16                  field_70A;
    /* 0x70C */ byte                 pad_70C[4];
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
    /* 0x748 */ u16                  field_748;
    /* 0x74A */ s16                  field_74A;
    /* 0x74C */ u16                  field_74C;
    /* 0x74E */ u16                  field_74E;
    /* 0x750 */ u16                  field_750;
    /* 0x752 */ byte                 pad_752[2];
    /* 0x754 */ s16                  field_754;
    /* 0x756 */ s16                  field_756;
    /* 0x758 */ s16                  field_758;
    /* 0x75A */ s16                  field_75A;
    /* 0x75C */ byte                 pad_75C[2];
    /* 0x75E */ s16                  field_75E;
    /* 0x760 */ u16                  field_760;
    /* 0x762 */ s16                  field_762;
    /* 0x764 */ s16                  field_764;
    /* 0x766 */ s16                  field_766;
    /* 0x768 */ s16                  field_768;
    /* 0x76A */ s16                  field_76A;
    /* 0x76C */ s16                  field_76C;
    /* 0x76E */ s16                  field_76E;
    /* 0x770 */ u16                  field_770;
    /* 0x772 */ s16                  field_772;
    /* 0x774 */ s16                  field_774;
    /* 0x776 */ s16                  field_776;
    /* 0x778 */ s16                  field_778;
    /* 0x77A */ s16                  field_77A;
    /* 0x77C */ s16                  field_77C;
    /* 0x77E */ byte                 pad_77E[2];
    /* 0x780 */ s16                  field_780;
    /* 0x782 */ s16                  field_782;
    /* 0x784 */ s16                  field_784;
    /* 0x786 */ s16                  field_786;
    /* 0x788 */ byte                 pad_788[2];
    /* 0x78A */ u16                  field_78A;
    /* 0x78C */ s16                  field_78C;
    /* 0x78E */ byte                 pad_78E[2];
    /* 0x790 */ s16                  field_790;
    /* 0x792 */ s16                  field_792;
    /* 0x794 */ s16                  field_794;
    /* 0x796 */ s16                  field_796;
    /* 0x798 */ s16                  field_798;
    /* 0x79A */ s16                  field_79A;
    /* 0x79C */ s16                  field_79C;
    /* 0x79E */ u16                  field_79E;
    /* 0x7A0 */ u16                  field_7A0;
    /* 0x7A2 */ s16                  field_7A2;
    /* 0x7A4 */ s16                  field_7A4;
    /* 0x7A6 */ s16                  field_7A6;
    /* 0x7A8 */ s16                  field_7A8;
    /* 0x7AA */ byte                 pad_7AA[2];
    /* 0x7AC */ s16                  field_7AC;
    /* 0x7AE */ s16                  field_7AE;
    /* 0x7B0 */ s16                  field_7B0;
    /* 0x7B2 */ u16                  field_7B2;
    /* 0x7B4 */ s16                  field_7B4;
    /* 0x7B6 */ byte                 pad_7B6[2];
} Actor403600Work;
STATIC_ASSERT_SIZEOF(Actor403600Work, 0x7B8);

typedef struct Actor403600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor403600Work* field_1C;
    /* 0x20 */ struct GpEnemy*  field_20;
    /* 0x24 */ byte             pad_24[6];
    /* 0x2A */ s16              field_2A;
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              field_30;
} Actor403600;

void func_actor_403600_801320F8(s32 otz);
void func_actor_403600_80138C68(Task* arg0);

#endif
