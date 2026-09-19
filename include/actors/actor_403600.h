#ifndef ACTOR_403600_H
#define ACTOR_403600_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

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
    /* 0x75A */ byte                 pad_75A[4];
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
    /* 0x79C */ byte                 pad_79C[2];
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
    /* 0x7B2 */ byte                 pad_7B2[6];
} Actor403600Work;
STATIC_ASSERT_SIZEOF(Actor403600Work, 0x7B8);

typedef struct Actor403600DamageRow {
    /* 0x0 */ s16 threshold;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 pad_6;
} Actor403600DamageRow;
STATIC_ASSERT_SIZEOF(Actor403600DamageRow, 0x8);

typedef struct Actor403600DamageScratch {
    /* 0x00 */ u8             pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ u8             pad_30[0x18];
} Actor403600DamageScratch;
STATIC_ASSERT_SIZEOF(Actor403600DamageScratch, 0x48);

typedef struct Actor403600MotionState {
    /* 0x00 */ s16  field_0[0x40];
    /* 0x80 */ s32  field_80;
    /* 0x84 */ s32  field_84;
    /* 0x88 */ s32  field_88;
    /* 0x8C */ s16  field_8C;
    /* 0x8E */ s16  field_8E;
    /* 0x90 */ byte pad_90[0x50];
    /* 0xE0 */ s32  field_E0;
} Actor403600MotionState;
STATIC_ASSERT_SIZEOF(Actor403600MotionState, 0xE4);

typedef struct Actor403600EffectState {
    /* 0x00 */ s16           field_0[0x20];
    /* 0x40 */ s16           field_40[0x20];
    /* 0x80 */ s32           field_80;
    /* 0x84 */ s32           field_84;
    /* 0x88 */ s32           field_88;
    /* 0x8C */ s16           field_8C;
    /* 0x8E */ s16           field_8E;
    /* 0x90 */ GsCOORDINATE2 field_90;
    /* 0xE0 */ s32           field_E0;
    /* 0xE4 */ s32           field_E4;
} Actor403600EffectState;
STATIC_ASSERT_SIZEOF(Actor403600EffectState, 0xE8);

typedef struct Actor403600 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor403600Work* field_1C;
    /* 0x20 */ struct GpEnemy*  field_20;
    /* 0x24 */ byte             pad_24[6];
    /* 0x2A */ s16              field_2A;
    /* 0x2C */ TmdObject*       field_2C;
    /* 0x30 */ s32              field_30;
} Actor403600;

typedef struct Actor403600Pattern {
    u8 values[9];
} __attribute__((packed)) Actor403600Pattern;
STATIC_ASSERT_SIZEOF(Actor403600Pattern, 9);

typedef struct Actor403600Msg {
    /* 0x0 */ void* field_0;
    /* 0x4 */ s32   field_4;
} Actor403600Msg;
STATIC_ASSERT_SIZEOF(Actor403600Msg, 0x8);

/// Incoming scripted message; field_2 selects the actor's transition.
typedef struct Actor403600MsgArg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor403600MsgArg;
STATIC_ASSERT_SIZEOF(Actor403600MsgArg, 0x4);

/// Position and rotation sent to the player with message 0x3E9.
typedef struct Actor403600MsgPos {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor403600MsgPos;
STATIC_ASSERT_SIZEOF(Actor403600MsgPos, 0x18);

/// Payload sent to the view task with message 0x7DB.
typedef struct Actor403600Msg7DB {
    /* 0x0 */ s8  field_0;
    /* 0x1 */ s8  field_1;
    /* 0x2 */ s16 field_2;
} Actor403600Msg7DB;
STATIC_ASSERT_SIZEOF(Actor403600Msg7DB, 0x4);

/// Coordinate frame with a word view of its rotation matrix.
typedef union Actor403600ViewFrame {
    GsCOORDINATE2 view;
    struct {
        /* 0x00 */ u32        flags;
        /* 0x04 */ GpMtxWords words;
    } matrix;
} Actor403600ViewFrame;
STATIC_ASSERT_SIZEOF(Actor403600ViewFrame, 0x50);

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

/// 0x24-byte scratch block used to hold seven planar distances while selecting
/// the nearest point from D_actor_403600_801605F4.
typedef struct Actor403600DistanceScratch {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 z;
    /* 0x08 */ s32 distances[7];
} Actor403600DistanceScratch;
STATIC_ASSERT_SIZEOF(Actor403600DistanceScratch, 0x24);

typedef struct Actor403600FacingScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR rot;
} Actor403600FacingScratch;
STATIC_ASSERT_SIZEOF(Actor403600FacingScratch, 0x18);

typedef struct Actor403600TurnMatrix {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 pad_12;
    /* 0x14 */ s32 field_14;
    /* 0x18 */ s32 field_18;
    /* 0x1C */ s32 field_1C;
} Actor403600TurnMatrix;
STATIC_ASSERT_SIZEOF(Actor403600TurnMatrix, 0x20);

typedef struct Actor403600TurnScratch {
    /* 0x00 */ u16                   angles[4];
    /* 0x08 */ s16                   vector[4];
    /* 0x10 */ Actor403600TurnMatrix matrix;
} Actor403600TurnScratch;
STATIC_ASSERT_SIZEOF(Actor403600TurnScratch, 0x30);

/// Scratch vector, rotation matrix and yaw used by func_actor_403600_8013C864.
typedef struct Actor403600TargetScratch {
    /* 0x00 */ SVECTOR               vector;
    /* 0x08 */ Actor403600TurnMatrix matrix;
    /* 0x28 */ s32                   angle;
} Actor403600TargetScratch;
STATIC_ASSERT_SIZEOF(Actor403600TargetScratch, 0x2C);

typedef struct Actor403600MatrixRef {
    /* 0x00 */ MATRIX* matrix;
} Actor403600MatrixRef;
STATIC_ASSERT_SIZEOF(Actor403600MatrixRef, 0x4);

/// 0x1C-byte scratch block used while building the screen transition grid.
typedef struct Actor403600ScreenScratch {
    /* 0x00 */ u8      pad_0[0x10];
    /* 0x10 */ s32     otz;
    /* 0x14 */ SVECTOR offset;
} Actor403600ScreenScratch;
STATIC_ASSERT_SIZEOF(Actor403600ScreenScratch, 0x1C);

/// 0x78-byte scratch block used to project the radial effect grid.
typedef struct Actor403600EffectScratch {
    /* 0x00 */ s32     dp;
    /* 0x04 */ s32     flag;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     nclip;
    /* 0x10 */ s32     sxy;
    /* 0x14 */ SVECTOR projected;
    /* 0x1C */ SVECTOR vec;
    /* 0x24 */ s32     maxOtz;
    /* 0x28 */ MATRIX  matrix;
    /* 0x48 */ SVECTOR points[3];
    /* 0x60 */ s32     pad_60[2];
    /* 0x68 */ s32     sxy3[3];
    /* 0x74 */ s32     pad_74;
} Actor403600EffectScratch;
STATIC_ASSERT_SIZEOF(Actor403600EffectScratch, 0x78);

typedef struct Actor403600Point {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 pad_2;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad_6;
} Actor403600Point;
STATIC_ASSERT_SIZEOF(Actor403600Point, 0x8);

/// The two `s16` halves of `D_actor_403600_801606B8`, zeroed together when the
/// actor is spawned. The indexed view records the two most recent action choices.
typedef union {
    struct {
        /* 0x0 */ s16 field_0;
        /* 0x2 */ s16 field_2;
    } fields;
    u16 values[2];
} Actor403600Pair;
STATIC_ASSERT_SIZEOF(Actor403600Pair, 0x4);

typedef struct Actor403600TargetPair {
    /* 0x00 */ s16 x0;
    /* 0x02 */ s16 y0;
    /* 0x04 */ s16 z0;
    /* 0x06 */ s16 pad_6;
    /* 0x08 */ s16 x1;
    /* 0x0A */ s16 y1;
    /* 0x0C */ s16 z1;
    /* 0x0E */ s16 pad_E;
} Actor403600TargetPair;
STATIC_ASSERT_SIZEOF(Actor403600TargetPair, 0x10);

extern Actor403600TargetPair D_actor_403600_8016063C;
extern Actor403600TargetPair D_actor_403600_8016064C;
extern u8                    D_actor_403600_80160694;
extern u8                    D_actor_403600_80160695;

typedef struct Actor403600Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ u8   field_14;
} Actor403600Ctx;

extern SVECTOR           D_actor_403600_801605D4;
extern SVECTOR           D_actor_403600_801605E4;
extern SVECTOR           D_actor_403600_801605EC;
extern s16               D_actor_403600_801606F2;
extern TaskDesc          D_actor_403600_80160514;
extern Task*             D_actor_403600_801606B0;
extern Actor403600MsgPos D_actor_403600_801606E0;
extern GpU16Pair         D_8016A408[];
extern u16               D_8016AEF8[];
extern u16               D_8016E450;
extern SVECTOR           D_actor_403600_8016065C;
extern s32               D_actor_403600_80160700[9];

void ActorsShared80131e24Sub1(Actor403600Ctx* arg0, Actor403600* arg1);
void func_8004BFF8(s32 angle, MATRIX* matrix);
void func_actor_403600_8013C864(Actor403600* arg0);
void func_actor_403600_80138C9C(Actor403600MotionState* arg0);
u8*  func_actor_403600_80138DCC(Actor403600* arg0);
void func_actor_403600_8013CCEC(Actor403600* arg0, s32 arg1);
s32  func_actor_403600_8013D9A8(Actor403600* arg0);
void func_actor_403600_8013DAF4(Actor403600* arg0, s32 arg1);
s32  func_actor_403600_8013DDF4(Actor403600* arg0, s16 arg1);
s32  func_actor_403600_8013DFE0(Actor403600* arg0);
void func_actor_403600_8013E470(GsCOORDINATE2* arg0, s32* arg1, s32* arg2);
s32  func_actor_403600_8013E7D4(s32 arg0, s32 arg1);
s32  func_actor_403600_801406A4(Actor403600* arg0, s32 arg1, Actor403600MsgArg* arg2);
void func_actor_403600_80140B4C(struct GpEnemy* arg0, Actor403600* arg1);

void func_actor_403600_80141F58(GsCOORDINATE2* arg0, s32 arg1);

#endif
