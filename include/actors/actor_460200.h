#ifndef ACTOR_460200_H
#define ACTOR_460200_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block of the actors whose spawn routine allocates it with
/// `memCalloc(0x4F8, 0)` and hangs it off their task's `Task::work` slot, which
/// is not a `TaskIdMap` here.
///
/// `state` drives the actor's step body: 1 reseeds the animation slots with
/// `animId` and `animArg`, 2 resets them to `animId` without the argument, and
/// both then advance it to 3, which walks off `travel` while clip 4 plays and
/// ticks the slots. Either reseed records the clip it applied in
/// `appliedAnimId`.
///
/// `slots` is the twenty-element `GpAnimSlot` array those resets walk, and the
/// `0x374` pose buffer the spawn routine hands `func_800B3F84` sits directly
/// past it. `yaw` caches the heading the placement and walk-to opcodes last
/// gave the root coordinate. `enemy` is the `GpEnemy` the spawn routine is
/// passed in its first argument (`Task::spawnArg2`) and parks for the exit
/// path. `field_4F0` is the partner task the visibility opcode drives
/// alongside the actor's own model when `Task::spawnArg1` is set; neither spawn
/// routine stores one.
typedef struct Actor460200Work {
    /* 0x000 */ byte       pad_0[0x40];
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ GpEnemy*   enemy;
} Actor460200Work;
STATIC_ASSERT_SIZEOF(Actor460200Work, 0x4F8);

/// Argument block of the actor's "play animation" script opcodes: which clip
/// to play, and whether to seed the slots with `animArg`.
typedef struct Actor460200AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor460200AnimArgs;

/// Work block of the paired variant whose spawn routine allocates it with
/// `memCalloc(0x4FC, 0)`: `light` / `color` go to the sub-model's
/// `TmdObject::lightMtx` / `field_20`, and `anim`, `slots` and `pose` are what
/// `func_800B3F84` fills in. When `Task::spawnArg1` is set the routine spawns a
/// partner enemy, reparents its own task under the partner's and parks that task
/// in `field_4F4`; `animId` is then 2 rather than 1. `state`, `animId`,
/// `animArg`, `appliedAnimId`, `yaw` and `travel` play the roles they play in
/// `Actor460200Work`, the walk-to opcode here counting steps of 30 rather than
/// 12. `field_4F0` is the weight, 0 to 0x1000, of the per-frame blend toward
/// the `gameGetPtrSlot(3)` task, stepped up while `field_4EE` is 1 and down
/// otherwise. `enemy` is the actor's own `GpEnemy`, handed back to
/// `Gp_DestroyEnemy` on exit.
typedef struct Actor460200PairWork {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pose[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        appliedAnimId;
    /* 0x4B8 */ s16        animId;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        travel;
    /* 0x4EC */ s16        animArg;
    /* 0x4EE */ s16        field_4EE;
    /* 0x4F0 */ u16        field_4F0;
    /* 0x4F2 */ byte       pad_4F2[0x2];
    /* 0x4F4 */ Task*      field_4F4;
    /* 0x4F8 */ GpEnemy*   enemy;
} Actor460200PairWork;
STATIC_ASSERT_SIZEOF(Actor460200PairWork, 0x4FC);

void func_actor_460200_801325FC(Task* task);

s32 func_actor_460200_80132B2C(Task* task, s32 arg1, Actor460200AnimArgs* args);

s32 func_actor_460200_80133C64(Task* task, s32 arg1, Actor460200AnimArgs* args);

s32 func_actor_460200_80133CD0(Task* task, s32 arg1, s32 flags);

/// Payload of the script opcodes that write the work block's `field_4EE`.
typedef struct Actor460200FlagArgs {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  value;
} Actor460200FlagArgs;

/// Payload of the walk-to opcode: the world position to walk to. Only the
/// horizontal components are read.
typedef struct Actor460200WalkTarget {
    /* 0x00 */ VECTOR pos;
} Actor460200WalkTarget;

/// Payload of the placement opcode: a world translation followed by Euler
/// angles, of which only the yaw (`rot.vy`) is used.
typedef struct Actor460200Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor460200Placement;

/// Spawn argument (`Task::spawnArg2`) of the screen-capture task
/// `func_actor_460200_80131E24`: `duration` seeds the task's kill countdown,
/// and `done` is cleared on start, set when the countdown runs out, and ends
/// the task once non-zero.
typedef struct Actor460200CaptureArgs {
    /* 0x0 */ u16 duration;
    /* 0x2 */ s16 done;
} Actor460200CaptureArgs;

#endif
