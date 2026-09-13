#ifndef ACTOR_560800_H
#define ACTOR_560800_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Reach it with
/// `(Actor560800Work*)task->idMap`.
///
/// `func_actor_560800_80135BD8` allocates it with `Mem_Malloc(0x68, 0)`, so the
/// size below is the allocation and not a guess, and fills the first slots with
/// the sub-tasks from `Game_GetPtrSlot(3)` and `ActorsShared80136280Desc`
/// (`field_4` is filled later by `func_actor_560800_801366B0`).
///
/// Above `field_14` the slots are s16 pairs at an 8-byte stride:
/// `func_actor_560800_801367E0` writes 0x28/0x2A, 0x30/0x32, 0x38/0x3A and
/// 0x40/0x42 with the same (value, 0) shape this unit uses for 0x58/0x5A, and
/// `func_actor_560800_80136818` sets 0x64.
///
/// The three pointer slots at 0x1C/0x20/0x24 are `Gp_DispatchMsg` targets, not
/// flags: `func_actor_560800_80133540` sends the message its switch picks to
/// one of them, `func_actor_560800_8013631C` sends 0x7DB to `field_24`, and
/// `func_actor_560800_801362E0` sends 0x7DB to `field_20`.
typedef struct Actor560800Work {
    /* 0x00 */ Task* field_0; // Game_GetPtrSlot(3)
    /* 0x04 */ Task* field_4;
    /* 0x08 */ Task* field_8;
    /* 0x0C */ byte  pad_C[4];
    /* 0x10 */ Task* field_10;
    /* 0x14 */ Task* field_14;
    /* 0x18 */ byte  pad_18[4];
    /* 0x1C */ Task* field_1C;
    /* 0x20 */ Task* field_20;
    /* 0x24 */ Task* field_24;
    /* 0x28 */ s16   field_28;
    /* 0x2A */ s16   field_2A;
    /* 0x2C */ byte  pad_2C[4];
    /* 0x30 */ s16   field_30;
    /* 0x32 */ s16   field_32;
    /* 0x34 */ byte  pad_34[4];
    /* 0x38 */ s16   field_38;
    /* 0x3A */ s16   field_3A;
    /* 0x3C */ byte  pad_3C[4];
    /* 0x40 */ s16   field_40;
    /* 0x42 */ s16   field_42;
    /* 0x44 */ byte  pad_44[0x14];
    /* 0x58 */ s16   field_58;
    /* 0x5A */ s16   field_5A;
    /* 0x5C */ byte  pad_5C[8];
    /* 0x64 */ s16   field_64;
    /* 0x66 */ byte  pad_66[2];
} Actor560800Work;
STATIC_ASSERT_SIZEOF(Actor560800Work, 0x68);

/// Payload `func_actor_560800_8013631C` passes as `Gp_DispatchMsg`'s `arg2` for
/// message 0x7DB: the same 4-byte record the other actors send, whose halfword
/// at 0x2 carries the value the receiver reads.
typedef struct Actor560800Msg {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor560800Msg;
STATIC_ASSERT_SIZEOF(Actor560800Msg, 0x4);

/// Controller task of this overlay, published by `func_actor_560800_80135BD8`
/// and read by the sub-task handlers.
extern Task* D_actor_560800_8017578C;

/// Phase timestamps, one per phase id 1..3: `func_actor_560800_80136930`
/// stamps `Display_State.field_0` (the frame counter) into the slot its argument
/// selects, and `func_actor_560800_80135AEC` reads it back per phase and stores
/// the elapsed frames in the matching slot of `D_actor_560800_80175790`.
extern s32 D_actor_560800_8017579C;
extern s32 D_actor_560800_801757A0;
extern s32 D_actor_560800_801757A4;

#endif
