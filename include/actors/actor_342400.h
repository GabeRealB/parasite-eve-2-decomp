#ifndef ACTOR_342400_H
#define ACTOR_342400_H

#include "common.h"
#include "main/task.h"
#include "gameplay/3A34.h"

/// Per-actor state block for the `actor_342400` overlay's main enemy.
///
/// `func_actor_342400_80163C58` and `func_actor_342400_80163E70` both allocate
/// it with `Mem_Calloc(0x454, 0)` and store it in the `Task::idMap` slot
/// (0x1C), so the size below is the allocation, not a guess: this actor reuses
/// that pointer field for its own work block and it is *not* a `TaskIdMap`
/// here. Reach it with `(Actor342400Work*)task->idMap`.
///
/// The layout mirrors the sibling `actor_341700` overlay, whose work block is
/// the same size and drives the same state machine: `field_420` / `field_422`
/// are the state and sub-state indices the handler table walks; `field_412`
/// is the per-state frame counter.
typedef struct Actor342400Work {
    /* 0x000 */ byte    pad_0[0x4];
    /* 0x004 */ u16     field_4; // set to 4 by the 0x7DB handler
    /* 0x006 */ byte    pad_6[0x2A6];
    /* 0x2AC */ GpObj   obj_2AC;
    /* 0x2CC */ GpObj   obj_2CC;
    /* 0x2EC */ GpRec18 rec_2EC[8];
    /* 0x3AC */ GpObj   obj_3AC;
    /* 0x3CC */ byte    pad_3CC[0x46];
    /* 0x412 */ u16     field_412; // per-state frame counter
    /* 0x414 */ byte    pad_414[0xC];
    /* 0x420 */ u16     field_420; // state index
    /* 0x422 */ u16     field_422; // sub-state index
    /* 0x424 */ byte    pad_424[0x30];
} Actor342400Work;
STATIC_ASSERT_SIZEOF(Actor342400Work, 0x454);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor335800Msg`. The overlay's 0x7DB handler,
/// `func_actor_342400_801626AC`, reads the halfword at 0x2.
typedef struct Actor342400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor342400Msg;
STATIC_ASSERT_SIZEOF(Actor342400Msg, 0x4);

/// 8-byte record in the table at `D_actor_342400_8016BF58`, indexed by the
/// halfword at `Task` 0x36 (the high half of `spawnArg1`). A child task that
/// finishes writes 2 into `field_6` before killing itself.
typedef struct Actor342400Slot {
    /* 0x0 */ byte pad_0[0x6];
    /* 0x6 */ s16  field_6;
} Actor342400Slot;
STATIC_ASSERT_SIZEOF(Actor342400Slot, 0x8);

/// Work block of the child task handled by `func_actor_342400_80163178`,
/// stored in its `Task::idMap` slot; it is killed once `field_A` reaches 3.
typedef struct Actor342400ChildWork {
    /* 0x0 */ byte pad_0[0xA];
    /* 0xA */ s16  field_A;
} Actor342400ChildWork;

/// Work block of the controller task set up by `func_actor_342400_801628F0`
/// (`Mem_Calloc(6, 0)`, stored in its `Task::idMap` slot).
/// `func_actor_342400_80162A34` counts `field_0` up to 15 before arming
/// `Gp_StateF0`.
typedef struct Actor342400CtrlWork {
    /* 0x0 */ s16 field_0; // frame counter
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
} Actor342400CtrlWork;
STATIC_ASSERT_SIZEOF(Actor342400CtrlWork, 0x6);

extern Actor342400Slot D_actor_342400_8016BF58[];

void func_actor_342400_801621D8(Task* arg0);
void func_actor_342400_801637DC(Task* arg0);

#endif
