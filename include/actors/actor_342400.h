#ifndef ACTOR_342400_H
#define ACTOR_342400_H

#include "common.h"
#include "actors/actor.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg`. The overlay's 0x7DB handler,
/// `func_actor_342400_801626AC`, reads the halfword at 0x2.
typedef struct Actor342400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor342400Msg;
STATIC_ASSERT_SIZEOF(Actor342400Msg, 0x4);

/// The same four bytes as the overlay builds them for its own 0x7DB send in
/// `func_actor_342400_80163010`: two id bytes followed by the halfword
/// (`Task::spawnArg1`) the receiver reads; see `Actor104000Msg7DA`.
typedef struct Actor342400Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor342400Msg7DB;
STATIC_ASSERT_SIZEOF(Actor342400Msg7DB, 0x4);

/// 8-byte record in the table at `D_actor_342400_8016BF58`, indexed by the
/// halfword at `Task` 0x36 (the high half of `spawnArg1`). A child task that
/// finishes writes 2 into `field_6` before killing itself.
typedef struct Actor342400Slot {
    /* 0x0 */ s16  field_0;
    /* 0x2 */ s16  field_2;
    /* 0x4 */ byte pad_4[0x2];
    /* 0x6 */ s16  field_6;
} Actor342400Slot;
STATIC_ASSERT_SIZEOF(Actor342400Slot, 0x8);

/// Work block of the child task handled by `func_actor_342400_80163178`,
/// stored in its `Task::work` slot; it is killed once `field_A` reaches 3.
/// `func_actor_342400_80162084` allocates it (`memCalloc(0xC, 0)`) and
/// spawns the two enemies it holds.
typedef struct Actor342400ChildWork {
    /* 0x0 */ GpEnemy* enemy0;
    /* 0x4 */ GpEnemy* enemy1;
    /* 0x8 */ s16      field_8;
    /* 0xA */ s16      field_A;
} Actor342400ChildWork;
STATIC_ASSERT_SIZEOF(Actor342400ChildWork, 0xC);

/// Work block of the controller task set up by `func_actor_342400_801628F0`
/// (`memCalloc(6, 0)`, stored in its `Task::work` slot).
/// `func_actor_342400_80162A34` counts `field_0` up to 15 before arming
/// `Gp_StateF0`.
typedef struct Actor342400CtrlWork {
    /* 0x0 */ s16 field_0; // frame counter
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4; // set to 4 by the 0x7DB handler; 4 idles the controller
} Actor342400CtrlWork;
STATIC_ASSERT_SIZEOF(Actor342400CtrlWork, 0x6);

/// Work block of the spawner task set up by `func_actor_342400_80162B60`
/// (`memCalloc(8, 0)`, stored in its `Task::work` slot); holds the enemy it
/// spawned from `D_actor_342400_80173A54`. `func_actor_342400_80162C10`
/// counts `field_4` up past 60 before releasing the enemy.
typedef struct Actor342400SpawnWork {
    /* 0x0 */ GpEnemy* enemy;
    /* 0x4 */ s16      field_4; // frame counter
    /* 0x6 */ byte     pad_6[2];
} Actor342400SpawnWork;
STATIC_ASSERT_SIZEOF(Actor342400SpawnWork, 0x8);

extern TaskDesc D_80151E60; // absolute, spawned twice by func_actor_342400_80162084

#endif
