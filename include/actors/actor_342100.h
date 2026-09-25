#ifndef ACTOR_342100_H
#define ACTOR_342100_H

#include "common.h"

#include "gameplay/message.h"
#include "main/task.h"
#include "overlay.h"

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_342100_80164BB8` points at.
///
/// `func_actor_342100_801630A4` allocates it with `Mem_Malloc(0x44, 0)`,
/// `Mem_Set`s the same 0x44 bytes over it and stores it in that task's
/// `Task::work` slot (0x1C), which is not a `TaskIdMap` here, then publishes
/// the task in `D_actor_342100_80164BB8`. Every leaf helper reaches the block
/// that way, `(Actor342100Work*)D_actor_342100_80164BB8->work`.
///
/// `field_2C` is the `gameGetPtrSlot(3)` task the overlay aims its messages
/// at. `field_30` and `field_34` are further message targets, both sent
/// 0x7DB, and `field_38` is a task the overlay spawns itself: with a non-zero
/// argument `func_actor_342100_80163454` writes 1 into its
/// `Task::spawnArg1`. `field_3C` takes `arg0 + 0x2F` from
/// `func_actor_342100_8016334C`'s integer argument, the same value that
/// function forwards as the animation message's second word.
///
/// `wave` is the ramp of the screen-wave task `func_actor_342100_80161E70`:
/// `func_actor_342100_80163408` seeds its span and scale and spawns the task
/// on it, and the fade task `func_actor_342100_80162748`, which reaches this
/// block through `Task::spawnArg2`, ends the wave by setting its ramp state to
/// 2 once the screen has been blanked white.
///
/// shelter_b3_garbage_incinerator carries the same encounter with a smaller
/// block that shares the leading bytes and `wave` but keeps one task pointer
/// fewer, with the child task and the animation fields in other places, so
/// the two are different types.
typedef struct Actor342100Work {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ OverlayWaveCtx wave;
    /* 0x2C */ Task*          field_2C; // gameGetPtrSlot(3)
    /* 0x30 */ Task*          field_30;
    /* 0x34 */ Task*          field_34;
    /* 0x38 */ Task*          field_38;
    /* 0x3C */ s16            field_3C;
    /* 0x3E */ s16            field_3E;
    /* 0x40 */ byte           pad_40[0x4];
} Actor342100Work;
STATIC_ASSERT_SIZEOF(Actor342100Work, 0x44);

/// The overlay's event/controller task, published by
/// `func_actor_342100_801630A4`.
extern Task* D_actor_342100_80164BB8;

/// Single-entry spawn table `func_actor_342100_80163454` starts as entry 3.
extern TaskDesc D_actor_342100_80164B78;

/// Record the fade task `func_actor_342100_80162748` parks in its own
/// `Task::msgTable`: the message id `0x7DB` followed by the handler
/// `func_actor_342100_80163344`. The same shape `src/gameplay/4CC.c` stores
/// there, and all this overlay does with it is take its address.
extern u8 D_actor_342100_801648F8[];

void func_actor_342100_80163344(Task* arg0, s32 arg1, s32 arg2);
void func_actor_342100_8016334C(s32 arg0);
void func_actor_342100_801633D0(s32 arg0);
void func_actor_342100_80163408(void);
void func_actor_342100_80163454(s32 arg0);
void func_actor_342100_80163518(void);

#endif
