#ifndef ACTOR_121300_H
#define ACTOR_121300_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block for the `actor_121300` overlay's cutscene actor.
///
/// `func_actor_121300_80133BFC` allocates it with `Mem_Malloc(0x4B0, 0)`,
/// zeroes it with `Mem_Set` and parks the pointer in the task's `Task::work`
/// slot (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor121300Work*)task->work`.  The same function publishes the task
/// itself in `D_actor_121300_8013D418` and stores the
/// `Game_GetPtrSlot(3)` task in `field_488`, which is the target of every
/// `Gp_DispatchMsg` the overlay sends.
///
/// The block opens with the animation prefix `actor_105100` and `actor_136100`
/// also carry: the 0x14-byte `GpAnimCtx` `func_800B3F84` is handed as its
/// `arg0`, the nineteen 0x28-byte `GpAnimSlot`s `Gp_AnimResetSlot` walks, and
/// the pose buffer at 0x30C.  The two `MATRIX`es at 0x43C / 0x45C are the
/// model's light and colour matrices, published through `TmdObject::lightMtx`
/// / `field_20`.
typedef struct Actor121300Work {
    /* 0x000 */ GpAnimCtx  anim;             // `func_800B3F84` arg0
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       field_30C[0x130]; // pose buffer, `func_800B3F84` arg3
    /* 0x43C */ MATRIX     field_43C;        // light matrix, into TmdObject::lightMtx
    /* 0x45C */ MATRIX     field_45C;        // colour matrix, into TmdObject::colorMtx
    /* 0x47C */ byte       pad_47C[0x4];
    /* 0x480 */ s16        field_480;        // state index driven by func_actor_121300_80133854
    /* 0x482 */ byte       pad_482[0x6];
    /* 0x488 */ Task*      field_488;        // Game_GetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x48C */ byte       pad_48C[0xC];
    /* 0x498 */ s16        field_498;        // set by func_actor_121300_80134250
    /* 0x49A */ s16        field_49A;        // cleared alongside field_498
    /* 0x49C */ byte       pad_49C[0x2];
    /* 0x49E */ s16        field_49E;        // waypoint cursor: index into D_actor_121300_8013CC20
    /* 0x4A0 */ u16        field_4A0;        // animation slot count, set by func_actor_121300_80133BFC
    /* 0x4A2 */ u16        field_4A2;        // state of the waypoint walker func_actor_121300_80133730
    /* 0x4A4 */ u16        field_4A4;        // frames spent on the current waypoint
    /* 0x4A6 */ s16        field_4A6;        // waypoint index handed to func_8017F334 / Task_SpawnFromTable
    /* 0x4A8 */ s16        field_4A8;        // effect-count reduction, bumped by func_actor_121300_80133580
    /* 0x4AA */ s16        field_4AA;        // frame counter for field_4A8 (wraps at 20)
    /* 0x4AC */ s16        field_4AC;        // GpAreaPlace::field_D, the TmdObject texture page
    /* 0x4AE */ byte       pad_4AE[0x2];
} Actor121300Work;
STATIC_ASSERT_SIZEOF(Actor121300Work, 0x4B0);

/// Animation-id table `func_actor_121300_80132818` indexes by
/// `Actor121300Work::field_4A0`, whose `>= 0` guard is what gates the slot
/// re-arm; the entry it holds is then written back over `field_4A0`.  All four
/// of its entries are -1, so the re-arm never runs in practice.
extern s16 D_actor_121300_8013CC18[];

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

/// Scratch `func_actor_121300_80133D98` stages the three states that build a
/// payload in.  Their live ranges do not overlap -- the state-0 message 0x3E8
/// record is dead once the state advances, and state 3 kills the task without
/// reaching the tail -- so the three share one stack slot and the frame stays
/// 0x38 bytes.
typedef union Actor121300Scratch {
    /* 0x0 */ GpAnimArg msg;  // state 0: slot-3 weapon record, message 0x3E8
    /* 0x0 */ RECT      rect; // state 3: the area ClearImage blanks
    /* 0x0 */ VECTOR    vec;  // tail: model part-1 translation for func_800D7A9C
} Actor121300Scratch;

/// 8-byte fade block `func_actor_121300_8013400C` and
/// `func_actor_121300_801326EC` each allocate with `Mem_Malloc(8, 0)` and park
/// in `Task::work` -- a second, smaller work block in this overlay, distinct
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

/// Frame counter `func_actor_121300_80133D98` bumps once a frame and the
/// effect spawners gate on: `func_actor_121300_8013343C` only runs on every
/// fourth frame (`& 3`), `func_actor_121300_80133580` too.
extern s32 D_actor_121300_8013CC00;

/// The gameplay LCG the effect spawners draw their jitter from,
/// `state = state * 5 + 0x71357911`; the draws are logical shifts of the high
/// half (`srl`), which a signed declaration would turn into arithmetic ones.
extern u32 Gp_LcgState;

/// The two position tables `func_actor_121300_8013343C` walks, each an array
/// of `SVECTOR`s ending on a zeroed one -- the walker's guard is `vx != 0`, so
/// the sentinel is read with the position.  Both trace the same ring around
/// the arena (`vx` 2500..6500 at `vz` 4700, then back at 1500) and differ only
/// in height: `8013CCB8` sits at ground level, `8013CD48` at `vy` -0xC8.
extern SVECTOR D_actor_121300_8013CCB8[];
extern SVECTOR D_actor_121300_8013CD48[];
/// Spawn points of `func_actor_121300_80133580`, of which the first
/// `6 - Actor121300Work::field_4A8` are used.
extern SVECTOR D_actor_121300_8013CDC8[];

/// 0x5C work block of the debris task `func_actor_121300_8013293C`, allocated
/// into `Task::work`.  The two matrices are published as the model's light
/// and colour matrices (`TmdObject::lightMtx` / `field_20`); the rest is a
/// per-frame spin and velocity, all rolled from `Gp_LcgState` on spawn, and a
/// short random delay before the model's buffers are allocated.
typedef struct Actor121300DebrisWork {
    /* 0x00 */ MATRIX lightMtx; // TmdObject::lightMtx
    /* 0x20 */ MATRIX colorMtx; // TmdObject::colorMtx
    /* 0x40 */ s16    rotX;
    /* 0x42 */ s16    rotY;
    /* 0x44 */ s16    rotZ;
    /* 0x46 */ s16    pad_46;
    /* 0x48 */ s16    spinX;
    /* 0x4A */ s16    spinY;
    /* 0x4C */ s16    spinZ;
    /* 0x4E */ s16    pad_4E;
    /* 0x50 */ s16    velX;
    /* 0x52 */ s16    velY;
    /* 0x54 */ s16    velZ;
    /* 0x56 */ s16    pad_56;
    /* 0x58 */ s16    delay;
    /* 0x5A */ s16    pad_5A;
} Actor121300DebrisWork;
STATIC_ASSERT_SIZEOF(Actor121300DebrisWork, 0x5C);

#endif
