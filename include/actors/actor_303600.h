#ifndef ACTOR_303600_H
#define ACTOR_303600_H

#include "common.h"

#include "main/gfx.h"
#include "main/task.h"

/// Work block for the `actor_303600` overlay's cutscene controller.
///
/// `func_actor_303600_8016216C` allocates it with `Mem_Malloc(0x10, 0)`, zeroes
/// it with `Mem_Set` and parks the pointer in the task's `Task::work` slot
/// (0x1C) -- that slot is not a `TaskIdMap` here, so reach the block with
/// `(Actor303600Work*)task->work`.  The same function publishes the task
/// itself in `D_actor_303600_8016E4C0` and stores the `Game_GetPtrSlot(3)` task
/// in `field_0`.
///
/// `command` is the request the overlay's state machine dispatches on:
/// `func_actor_303600_80161F40` switches on it through
/// `jtbl_actor_303600_80161E24` (values 0..8) and clears it again on the way
/// out.  `field_C` records the message the dispatcher last sent and `field_E`
/// is the "a message is outstanding" flag that
/// `func_actor_303600_801624B0` / `func_actor_303600_8016253C` test before
/// sending another.
typedef struct Actor303600Work {
    /* 0x0 */ Task* field_0; // Game_GetPtrSlot(3) task
    /* 0x4 */ u16   command; // state-machine request, see jtbl_actor_303600_80161E24
    /* 0x6 */ s16   field_6; // cleared alongside command
    /* 0x8 */ byte  pad_8[0x4];
    /* 0xC */ s16   field_C; // message id last dispatched
    /* 0xE */ u16   field_E; // set to 1 while a dispatched message is outstanding
} Actor303600Work;
STATIC_ASSERT_SIZEOF(Actor303600Work, 0x10);

/// Fade block `func_actor_303600_801623CC` allocates with `Mem_Malloc(8, 0)` and
/// parks in its own task's `Task::work` slot (0x1C, again not a `TaskIdMap`),
/// so reach it with `(Actor303600FadeWork*)task->work`.  The allocation size is
/// the struct size, and not a guess.  The three halfwords are the RGB channels
/// `Fade_DrawOverlay` draws: the task steps them by `Task::spawnArg1` -- the
/// fade rate, not a colour -- and hands `r` and `g` to that call, so only the
/// green channel reads as a colour and the blue one is stepped without ever
/// being shown.  The leading halfword is part of the allocation and is never
/// touched.  `func_actor_303600_801622E8` walks this same block the other way,
/// subtracting where this one adds.
typedef struct Actor303600FadeWork {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  r;
    /* 0x4 */ u16  g;
    /* 0x6 */ u16  b;
} Actor303600FadeWork;
STATIC_ASSERT_SIZEOF(Actor303600FadeWork, 0x8);

/// Payload `func_actor_303600_801624B0` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers:
/// the session's two id bytes followed by the halfword the receiver switches on,
/// here the selector 9 that the sender latches into `Actor303600Work::field_C`.
/// `Gp_SendMsgType9` forwards that same payload under id 0x7DB to the slot-4
/// task's type-9 children, which is where `func_actor_303600_80162870` reads it;
/// the senders of the 0 and 1 selectors it arms on are other overlays.
/// The same four bytes as `Actor444000Msg7DA`.
typedef struct Actor303600Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor303600Msg7DA;
STATIC_ASSERT_SIZEOF(Actor303600Msg7DA, 0x4);

/// Light / colour matrix pair the overlay's actor hands to its model: the pair
/// `func_actor_303600_80162950` allocates with `Mem_Calloc(0x44, 0)` and parks
/// in its own task's `Task::work` slot (0x1C, again not a `TaskIdMap`), so
/// reach it with `(Actor303600LightMats*)task->work`.  The four bytes after
/// the two matrices are part of the allocation and are never read here.
typedef struct Actor303600LightMats {
    /* 0x00 */ MATRIX lightMtx;
    /* 0x20 */ MATRIX colorMtx;
    /* 0x40 */ byte   pad_40[0x4];
} Actor303600LightMats;
STATIC_ASSERT_SIZEOF(Actor303600LightMats, 0x44);

/// The rig's 16.16 angle accumulator, `Actor303600RigWork::field_18`: the rig
/// code ramps it and folds the whole word back into +/-4000, while the
/// coordinate's Y takes its integer half.  Same union shape as
/// `Actor100400SpawnArg`.
typedef union Actor303600RigAngle {
    /* 0x0 */ s32 w;
    struct {
        /* 0x0 */ u16 lo;
        /* 0x2 */ s16 hi;
    } half;
} Actor303600RigAngle;

/// Work block of the task `func_actor_303600_80162A7C` dispatches through
/// `D_actor_303600_80161E48`: `func_actor_303600_801626C0` allocates it with
/// `Mem_Calloc(0x3C, 0)`, parks it in `Task::work` (0x1C, again not a
/// `TaskIdMap`), fills `children` with the five model tasks it spawns -- one
/// `Task_SpawnFromTable` of `D_actor_303600_8016E468` entry 1 each, spread
/// 8000 units apart in y and spliced under this task's own coordinate, so
/// `children[i]` owns the light matrices -- and installs the 0x7DB handler
/// table in `Task::field_24`.  `func_actor_303600_801627B8` then moves the rig
/// each frame: `field_28` (a 16.16 speed) ramps toward `field_38` at `field_34`
/// a frame and stops once it passes it, and `field_18` accumulates `field_28`
/// and is folded back into +/-4000 before its integer half becomes the task
/// coordinate's `t[1]` (the `lh` from 0x1A).  The words this block does not yet
/// name are the same shape, so `field_28`/`field_34`/`field_38` are the three
/// the 0x7DB handler below arms.
typedef struct Actor303600RigWork {
    /* 0x00 */ Task*               children[5];
    /* 0x14 */ s32                 field_14;
    /* 0x18 */ Actor303600RigAngle field_18;
    /* 0x1C */ s32                 field_1C;
    /* 0x20 */ s32                 field_20;
    /* 0x24 */ s32                 field_24;
    /* 0x28 */ s32                 field_28;
    /* 0x2C */ s32                 field_2C;
    /* 0x30 */ s32                 field_30;
    /* 0x34 */ s32                 field_34;
    /* 0x38 */ s32                 field_38;
} Actor303600RigWork;
STATIC_ASSERT_SIZEOF(Actor303600RigWork, 0x3C);

/// The overlay's three flat lights, loaded into the model by
/// `func_actor_303600_80162A0C`; one `GsF_LIGHT` (0x10 bytes) each.
extern GsF_LIGHT D_actor_303600_8016E490[3];

#endif // ACTOR_303600_H
