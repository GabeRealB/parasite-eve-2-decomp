#ifndef ACTOR_403000_H
#define ACTOR_403000_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// One of the four display nodes the spawn handler `func_actor_403000_801343B8`
/// links in a row from 0xB50 of the work block: the `GpObj` list node `Gp_LinkObj`
/// appends and the task's exit callback `func_actor_403000_8013D4F4` hands back
/// to `Gp_UnlinkObj`, followed by the five-entry `GpRec18` table the node's
/// `field_C` points at (+0x20). `Gp_InitRec18Table(_, 5, 0)` on that table is
/// what fixes the 0x98 stride the four nodes are spaced by.
typedef struct Actor403000Obj {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[5];
} Actor403000Obj;
STATIC_ASSERT_SIZEOF(Actor403000Obj, 0x98);

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
    /* 0x000 */ s16            field_0;
    /* 0x002 */ s16            field_2;
    /* 0x004 */ byte           pad_4[0xAC2];
    /* 0xAC6 */ u16            field_AC6;
    /* 0xAC8 */ byte           pad_AC8[0x88];
    /* 0xB50 */ Actor403000Obj objB50;
    /* 0xBE8 */ Actor403000Obj objBE8;
    /* 0xC80 */ Actor403000Obj objC80;
    /* 0xD18 */ Actor403000Obj objD18;
    /* 0xDB0 */ byte           pad_DB0[0x22C];
} Actor403000Work;
STATIC_ASSERT_SIZEOF(Actor403000Work, 0xFDC);

/// Payload the sender of the animation message passes as `Gp_DispatchMsg`'s
/// `arg2`; only the animation id at 0x4 is read.
typedef struct Actor403000Msg {
    /* 0x0 */ byte pad_0[0x4];
    /* 0x4 */ u16  field_4;
} Actor403000Msg;

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id -- `func_actor_403000_8013ACBC` scales a byte id by 8 into it
/// the same way -- so a record's `vx`/`vy`/`vz` are the vector an actor's
/// handlers copy out of it. Lives in the overlay's trailing data region.
extern SVECTOR D_actor_403000_80158CE0[];

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1);

/// Latch the requested animation and restart the animation state machine.
s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg);

/// `Task::exitCallback` installed by the spawn handler, for the teardown path
/// where the enemy was created: hand the four display nodes back to
/// `Gp_UnlinkObj`, drop the enemy's `field_54` slot, then let `Gp_DestroyEnemy`
/// free the enemy and the task.
void func_actor_403000_8013D4F4(Task* task);

#endif // ACTOR_403000_H
