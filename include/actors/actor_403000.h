#ifndef ACTOR_403000_H
#define ACTOR_403000_H

#include "common.h"

#include "main/task.h"

/// Per-actor work block for the `actor_403000` overlay.
///
/// `func_actor_403000_801343B8` allocates it with `Mem_Calloc(0xFDC, 0)` and
/// parks it in the `Task::idMap` slot (0x1C) -- the same slot `Actor00100Work`
/// and `Actor403100Work` use for the same job, so the overlay reaches its state
/// through the task and not through the actor. `field_0` is the animation state
/// the per-frame handler switches on and `field_2` its frame counter, both
/// restarted together by the message handlers; `field_AC6` is the requested
/// animation id, written by the handlers and read back by the handlers that
/// tick the current state.
typedef struct Actor403000Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ s16  field_2;
    /* 0x004 */ byte pad_4[0xAC2];
    /* 0xAC6 */ u16  field_AC6;
    /* 0xAC8 */ byte pad_AC8[0x514];
} Actor403000Work;
STATIC_ASSERT_SIZEOF(Actor403000Work, 0xFDC);

/// Payload the sender of the animation message passes as `Gp_DispatchMsg`'s
/// `arg2`; only the animation id at 0x4 is read.
typedef struct Actor403000Msg {
    /* 0x0 */ byte pad_0[0x4];
    /* 0x4 */ u16  field_4;
} Actor403000Msg;

/// Latch the requested animation and restart the animation state machine.
s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg);

#endif // ACTOR_403000_H
