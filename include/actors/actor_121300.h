#ifndef ACTOR_121300_H
#define ACTOR_121300_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block for the `actor_121300` overlay's cutscene actor.
///
/// `func_actor_121300_80133BFC` allocates it with `Mem_Malloc(0x4B0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::idMap`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor121300Work*)task->idMap`.  The same function publishes the task
/// itself in `D_actor_121300_8013D418` and stores the
/// `Game_GetPtrSlot(3)` task in `field_488`, which is the target of every
/// `Gp_DispatchMsg` the overlay sends.
///
/// The block opens with the animation prefix `actor_105100` and `actor_136100`
/// also carry: the 0x14-byte `GpAnimCtx` `func_800B3F84` is handed as its
/// `arg0`, the nineteen 0x28-byte `GpAnimSlot`s `Gp_AnimResetSlot` walks, and
/// the pose buffer at 0x30C.  The two `MATRIX`es at 0x43C / 0x45C are the
/// model's light and colour matrices, published through `TmdObject::field_1C`
/// / `field_20`.
typedef struct Actor121300Work {
    /* 0x000 */ GpAnimCtx  anim;             // `func_800B3F84` arg0
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x130]; // pose buffer, `func_800B3F84` arg3
    /* 0x43C */ MATRIX     field_43C;        // light matrix, into TmdObject::field_1C
    /* 0x45C */ MATRIX     field_45C;        // colour matrix, into TmdObject::field_20
    /* 0x47C */ byte       pad_47C[0x4];
    /* 0x480 */ s16        field_480;        // state index driven by func_actor_121300_80133854
    /* 0x482 */ byte       pad_482[0x6];
    /* 0x488 */ Task*      field_488;        // Game_GetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x48C */ byte       pad_48C[0xC];
    /* 0x498 */ s16        field_498;        // set by func_actor_121300_80134250
    /* 0x49A */ s16        field_49A;        // cleared alongside field_498
    /* 0x49C */ byte       pad_49C[0x2];
    /* 0x49E */ s16        field_49E;        // waypoint cursor: index into D_actor_121300_8013CC20
    /* 0x4A0 */ s16        field_4A0;        // animation slot count, set by func_actor_121300_80133BFC
    /* 0x4A2 */ u16        field_4A2;        // state of the waypoint walker func_actor_121300_80133730
    /* 0x4A4 */ u16        field_4A4;        // frames spent on the current waypoint
    /* 0x4A6 */ s16        field_4A6;        // waypoint index handed to func_8017F334 / Task_SpawnFromTable
    /* 0x4A8 */ byte       pad_4A8[0x4];
    /* 0x4AC */ s16        field_4AC;        // GpAreaPlace::field_D, the TmdObject texture page
    /* 0x4AE */ byte       pad_4AE[0x2];
} Actor121300Work;
STATIC_ASSERT_SIZEOF(Actor121300Work, 0x4B0);

/// One record of the cutscene's waypoint table `D_actor_121300_8013CC20`: a
/// position plus a fourth halfword `func_actor_121300_80133730` reads as a
/// liveness flag.  The table is 0xD records long and its last record is
/// `{0, 0, 0, -1}`, so the `!= -1` guard keeps the walker on the 0xC real
/// entries; `func_actor_121300_8013293C` reads the x/y/z of entry
/// `someWork->field_34` off the same table.
typedef struct Actor121300Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 field_6;
} Actor121300Waypoint;
STATIC_ASSERT_SIZEOF(Actor121300Waypoint, 0x8);

extern Actor121300Waypoint D_actor_121300_8013CC20[];

/// 8-byte fade block `func_actor_121300_8013400C` and
/// `func_actor_121300_801326EC` each allocate with `Mem_Malloc(8, 0)` and park
/// in `Task::idMap` -- a second, smaller idMap block in this overlay, distinct
/// from `Actor121300Work`.
///
/// The three halfwords are the RGB channels `Fade_DrawOverlay` draws.  The
/// task seeded by `func_actor_121300_8013400C` seeds all three to 0 and raises
/// them by `Task::spawnArg1` every frame, then once the red channel has reached
/// 0x100 it blanks the display and kills itself; the blue channel is advanced
/// but never read back.  The task seeded by `func_actor_121300_801326EC` is the
/// mirror image: it seeds all three to 0xFF, unblanks the display one state
/// before it starts drawing, and lowers them by `Task::spawnArg1` per frame
/// until the red channel goes negative, reading all three channels back.
typedef struct Actor121300FadeWork {
    /* 0x0 */ u8  pad_0[0x2];
    /* 0x2 */ s16 r;
    /* 0x4 */ s16 g;
    /* 0x6 */ s16 b;
} Actor121300FadeWork;
STATIC_ASSERT_SIZEOF(Actor121300FadeWork, 0x8);

#endif
