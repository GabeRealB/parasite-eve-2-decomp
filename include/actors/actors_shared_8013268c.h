#ifndef ACTORS_SHARED_8013268C_H
#define ACTORS_SHARED_8013268C_H

#include "common.h"

#include "main/task.h"

/// The part of the `ActorsShared80131f9c` work block this body touches. Each
/// carrier's block is its own type (see `include/actors/actor_151000.h` for
/// the full layout), but both carriers' setup - `ActorsShared80131f9cSub0` at
/// 0x80131E24 (actor_451100) and 0x80149FB0 (actor_260500) - allocates 0x4B8,
/// so the size below is the allocation and not a guess.
typedef struct ActorsShared8013268cWork {
    /* 0x000 */ byte pad_0[0x4B4];
    /* 0x4B4 */ s16  field_4B4; // countdown the carriers decrement; cleared at spawn
    /* 0x4B6 */ byte pad_4B6[0x2];
} ActorsShared8013268cWork;
STATIC_ASSERT_SIZEOF(ActorsShared8013268cWork, 0x4B8);

extern ActorsShared8013268cWork* ActorsShared80131f9cWork;

/// Payload the message handlers take as `Gp_DispatchMsg`'s `arg2`; the same
/// 4-byte record as `Actor205200Msg7DB`, whose halfword at 0x2 is the only
/// part this handler reads.
typedef struct ActorsShared8013268cMsg {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} ActorsShared8013268cMsg;
STATIC_ASSERT_SIZEOF(ActorsShared8013268cMsg, 0x4);

/// Message 0x7DB handler shared by actor_451100 (0x8013268C) and actor_260500
/// (0x8014A818): a zero payload halfword arms the `field_4B4` countdown at
/// 0x14. Nothing reads the opcode itself, hence `arg1`.
s32 ActorsShared8013268c(Task* task, s32 arg1, ActorsShared8013268cMsg* msg);

#endif
