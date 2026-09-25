#ifndef ACTOR_146300_H
#define ACTOR_146300_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/task.h"

/// Per-actor work block for the `actor_146300` overlay.
///
/// The spawn routine, `func_actor_146300_801324AC`, allocates it with
/// `memCalloc(0x4EC, 0)` and stores the pointer both in
/// `D_actor_146300_80142828` and in the task's `Task::work` slot, so the size
/// below is the allocation and not a guess. Every other function in the
/// overlay reaches the block through the global.
///
/// `light` and `color` are the matrices the spawn routine hands the model;
/// `anim` is the animation context the tick and reseed loops walk, and `slots`
/// and `pad_374` are what `func_800B3F84` fills in beside it.
typedef struct Actor146300Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // animation reset mode `func_actor_146300_8013299C` latches (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by `func_actor_146300_8013299C` before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[0x4];
} Actor146300Work;
STATIC_ASSERT_SIZEOF(Actor146300Work, 0x4EC);

/// The work block above, published by the task handler
/// `func_actor_146300_801326CC` and by the spawn routine.
extern Actor146300Work* D_actor_146300_80142828;

/// Animation preset the overlay's play-animation message handler applies to
/// the work block: `field_4` is the animation id, `field_8` picks the reset
/// path -- non-zero for the reseed through `func_800B4114` with a reset
/// argument, zero for a plain one -- and `field_C` becomes that reset argument.
/// `func_actor_146300_8013299C` accepts the first 0x11 ids.
typedef struct Actor146300AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor146300AnimPreset;
STATIC_ASSERT_SIZEOF(Actor146300AnimPreset, 0x10);

/// The task the reseed is handed: `func_actor_146300_801327CC` steps it, and
/// `func_actor_146300_80132B1C` reaches the overlay's models through its
/// `extra`.
extern Task* D_actor_146300_8014282C;

/// Reset argument the reseed forwards, read back signed by
/// `func_actor_146300_8013291C`: the play-animation handler latches the preset's
/// `field_C` here.
extern s16 D_actor_146300_8014279C;

/// The companion task the spawn routine starts from
/// `D_actor_146300_801427C8`; its `extra` is the model whose texture page and
/// CLUT row come out of the area record, and the actor's own task is reparented
/// under it.
extern Task* D_actor_146300_80142830;

/// Spawn table the state-0 handler starts the companion task from, index 1.
extern TaskDesc D_actor_146300_801427C8[];

/// Animation stream the state-0 handler binds into the work block's animation
/// context with `func_800B3F84`.
extern u8 D_actor_146300_801427E0[];

/// Message handler table the state-0 handler publishes as `Task::msgTable`.
extern GpMsgEntry D_actor_146300_801427A0[];

void func_actor_146300_801324AC(GpEnemy* enemy, Task* task);
void func_actor_146300_80132728(GpEnemy* enemy, Task* task);
void func_actor_146300_801327A4(Task* task);
void func_actor_146300_801327CC(Task* task);
void func_actor_146300_80132840(void);
void func_actor_146300_8013288C(void);

s32 func_actor_146300_8013299C(Task* task, s32 arg1, Actor146300AnimPreset* preset);

#endif
