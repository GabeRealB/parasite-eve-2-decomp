#ifndef ACTOR_444000_H
#define ACTOR_444000_H

#include "common.h"

#include "main/task.h"

/// Per-actor work block for the enemy task `D_actor_444000_80161878` points
/// at, reached through the `Task::idMap` slot (0x1C) rather than being a
/// `TaskIdMap` here.
///
/// `func_actor_444000_8013AFF8` allocates it with `Mem_Calloc(0xF24, 0)` and
/// stores the result in that slot, so the size below is the allocation rather
/// than a guess. Only two fields are known so far: the leading state word,
/// which `func_actor_444000_80143D7C` reads with `lhu` and range-checks and
/// the 0x7D9 message handler `func_actor_444000_80143F38` clears, and the
/// byte at 0xEAC written by `func_actor_444000_80143490`. Fill in the padding
/// as the remaining functions are matched.
typedef struct Actor444000Work {
    /* 0x000 */ s16  field_0; // state index
    /* 0x002 */ byte pad_2[0xEAA];
    /* 0xEAC */ s8   field_EAC;
    /* 0xEAD */ byte pad_EAD[0x77];
} Actor444000Work;
STATIC_ASSERT_SIZEOF(Actor444000Work, 0xF24);

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_444000_80161860` points at, which is a different and much smaller
/// block than the enemy's `Actor444000Work` above.
///
/// `func_actor_444000_80132358` allocates it with `Mem_Calloc(0x34, 0)`,
/// `Mem_Set`s 0x34 bytes and parks it in that task's `Task::idMap` slot, so
/// the size is anchored; the same function stores the `Game_GetPtrSlot(3)`
/// task in `field_20` and publishes its owning task in
/// `D_actor_444000_80161860`. `field_20` is the target of every
/// `Gp_DispatchMsg` the leaf helpers send, and they null-check it first
/// (`func_actor_444000_801321FC`). `field_2C` is the action index
/// `func_actor_444000_80132054` switches on, with `field_2E` the sub-state
/// counter reset alongside it.
typedef struct Actor444000EventWork {
    /* 0x00 */ byte  pad_0[0x20];
    /* 0x20 */ Task* field_20; // Game_GetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x24 */ Task* field_24; // subordinate task, killed and cleared by func_actor_444000_80132694
    /* 0x28 */ byte  pad_28[0x4];
    /* 0x2C */ u16   field_2C; // action index, switched on by func_actor_444000_80132054
    /* 0x2E */ s16   field_2E; // cleared whenever field_2C is set
    /* 0x30 */ byte  pad_30[0x4];
} Actor444000EventWork;
STATIC_ASSERT_SIZEOF(Actor444000EventWork, 0x34);

/// Body/collision object the actor task carries at +0x20 (the `Task::spawnArg2`
/// slot). Only the halfword at 0x40 is known so far: it is the remaining HP,
/// tested for `> 0` by `func_actor_444000_80143D68` and topped back up by
/// `func_actor_444000_80143E68`.
typedef struct Actor444000Obj {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ s16  field_40; // remaining HP
} Actor444000Obj;

/// Payload `func_actor_444000_801326DC` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `AcropolisBridgeMsg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor444000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor444000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor444000Msg7DA, 0x4);

/// The overlay's enemy task: the same layout as `Task`, named for the two
/// slots this overlay reaches through it. Not the event task
/// `D_actor_444000_80161860`, whose `idMap` holds an `Actor444000EventWork`.
typedef struct Actor444000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor444000Work* field_1C;
    /* 0x20 */ Actor444000Obj*  field_20;
} Actor444000;

s32 func_actor_444000_80143D68(Actor444000* arg0);
s32 func_actor_444000_80143F38(Actor444000* arg0);

#endif
