#ifndef ACTOR_342100_H
#define ACTOR_342100_H

#include "common.h"

#include "main/task.h"

typedef struct Actor342100 {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  field_30;
} Actor342100;

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_342100_80164BB8` points at.
///
/// `func_actor_342100_801630A4` allocates it with `Mem_Malloc(0x44, 0)`,
/// `Mem_Set`s the same 0x44 bytes over it and stores it in that task's
/// `Task::work` slot (0x1C), which is not a `TaskIdMap` here, then publishes
/// the task in `D_actor_342100_80164BB8`. Every leaf helper reaches the block
/// that way, `(Actor342100Work*)D_actor_342100_80164BB8->work`.
///
/// `field_2C` is the `Game_GetPtrSlot(3)` task the overlay aims its messages
/// at. `field_30` and `field_34` are further message targets, both sent
/// 0x7DB, and `field_38` is a task the overlay spawns itself: with a non-zero
/// argument `func_actor_342100_80163454` writes 1 into its
/// `Task::spawnArg1`. `field_20` / `field_22` are seeded with 0x258 and 0x100
/// by `func_actor_342100_80163408` just before it hands their address to
/// `Task_SpawnFromTable` as the fourth argument, and `field_3C` takes
/// `arg0 + 0x2F` from `func_actor_342100_8016334C`'s integer argument, the
/// same value that function forwards as the animation message's second word.
///
/// `field_24` is raised to 2 by the fade task `func_actor_342100_80162748`,
/// which reaches this block through `Task::spawnArg2` (that task is spawned
/// with the task owning this block as its fourth `Task_SpawnFromTable`
/// argument), at the moment the screen has been blanked white. Nothing in the
/// decompiled overlay reads it back yet.
typedef struct Actor342100Work {
    /* 0x00 */ byte  pad_0[0x20];
    /* 0x20 */ s16   field_20;
    /* 0x22 */ s16   field_22;
    /* 0x24 */ s16   field_24;
    /* 0x26 */ byte  pad_26[0x6];
    /* 0x2C */ Task* field_2C; // Game_GetPtrSlot(3)
    /* 0x30 */ Task* field_30;
    /* 0x34 */ Task* field_34;
    /* 0x38 */ Task* field_38;
    /* 0x3C */ s16   field_3C;
    /* 0x3E */ s16   field_3E;
    /* 0x40 */ byte  pad_40[0x4];
} Actor342100Work;
STATIC_ASSERT_SIZEOF(Actor342100Work, 0x44);

/// Eight-byte block the overlay's fade task (`func_actor_342100_80162748`)
/// allocates with `Mem_Malloc(8, 0)` and parks in its own `Task::work` slot
/// (0x1C, not a `TaskIdMap` here), so reach it with
/// `(Actor342100FadeWork*)task->work`.
///
/// The three halfwords are the channels `Fade_DrawOverlay` draws, read as
/// bytes at that call and as halfwords by the state machine that ramps them:
/// state 2 steps `field_2` by 0xA until it passes 0x50, state 3 by 1 until it
/// passes 0xFF, and state 4 steps `field_4` / `field_6` by 8 until `field_4`
/// passes 0xFF, at which point the screen is blanked and state 5 draws the
/// white overlay. `pad_0` is never touched.
typedef struct Actor342100FadeWork {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ u16  field_6;
} Actor342100FadeWork;
STATIC_ASSERT_SIZEOF(Actor342100FadeWork, 0x8);

/// Payload `func_actor_342100_80162F54` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x3F7: the null-terminated pointer table at
/// `D_actor_342100_80164900` (three live entries followed by a null word) and
/// the number of live entries the sender counted in it. Same shape as
/// `Actor136100Msg3F7`.
typedef struct Actor342100Msg3F7 {
    /* 0x0 */ s32* table;
    /* 0x4 */ s32  count;
} Actor342100Msg3F7;
STATIC_ASSERT_SIZEOF(Actor342100Msg3F7, 0x8);

/// Payload `func_actor_342100_80163454` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA and then re-sends verbatim as the 0x7DB the slot-4 task
/// forwards. Two zero/action bytes followed by the halfword the receiver
/// reads, the same four bytes as `Actor104000Msg7DA`.
typedef struct Actor342100Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor342100Msg7DA;
STATIC_ASSERT_SIZEOF(Actor342100Msg7DA, 0x4);

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

void func_actor_342100_80163344(Actor342100* arg0, s32 arg1, s32 arg2);
void func_actor_342100_8016334C(s32 arg0);
void func_actor_342100_801633D0(s32 arg0);
void func_actor_342100_80163408(void);
void func_actor_342100_80163454(s32 arg0);
void func_actor_342100_80163518(void);

#endif
