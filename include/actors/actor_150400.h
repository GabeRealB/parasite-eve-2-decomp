#ifndef ACTOR_150400_H
#define ACTOR_150400_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block the actor hangs off its task's `Task::work` slot, which is not a
/// `TaskIdMap` here. The spawn handler `func_actor_150400_80132014` allocates it
/// and fills it in.
///
/// `light` and `color` are the matrices the actor's model and its sub-model are
/// drawn under; `anim`, `slots` and the buffer at `field_34C` are what
/// `func_800B3F84` fills in.
///
/// `state` drives the step body `func_actor_150400_80132228`: 1 reseeds the
/// animation slots with `animId` and `animArg`, 2 resets them to `animId`
/// without the argument, and both then advance it to 3, which walks off
/// `travel` while clip 4 plays and ticks the slots. Either reseed records the
/// clip it applied in `appliedAnimId`. `yaw` caches the heading the placement
/// and walk-to opcodes last gave the root coordinate.
///
/// `field_4B8` is the sub-model task the spawn handler starts and parents under
/// this one; the visibility opcode drives its model alongside the actor's own.
/// `field_4BC` is the actor's `GpEnemy`.
typedef struct Actor150400Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        state;
    /* 0x47E */ s16        appliedAnimId;
    /* 0x480 */ s16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ u16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ s16        animArg;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ GpEnemy*   field_4BC;
} Actor150400Work;
STATIC_ASSERT_SIZEOF(Actor150400Work, 0x4C0);

/// Argument block of the script opcode `func_actor_150400_801326A4`
/// implements: which animation to play, and how.
typedef struct Actor150400AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor150400AnimArgs;

/// Argument block of the placement opcode: a world translation followed by
/// Euler angles, of which only the yaw (`rot.vy`) is read.
typedef struct Actor150400Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor150400Placement;

/// Argument block of the walk-to opcode: the world position to walk to. Only
/// the horizontal components are read.
typedef struct Actor150400WalkTarget {
    /* 0x00 */ VECTOR pos;
} Actor150400WalkTarget;

void func_actor_150400_80132014(GpEnemy* enemy, Task* task);
void func_actor_150400_80132228(Task* task);
void func_actor_150400_80132434(GpEnemy* enemy, Task* task);
void func_actor_150400_801324B8(Task* task);
void func_actor_150400_801324E0(Task* task);
void func_actor_150400_8013257C(Task* task);
void func_actor_150400_801325C8(Task* task);
void func_actor_150400_80132640(Task* task);

s32 func_actor_150400_801326A4(Task* task, s32 arg1, Actor150400AnimArgs* args);

#endif
