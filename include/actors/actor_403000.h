#ifndef ACTOR_403000_H
#define ACTOR_403000_H

#include "common.h"

#include "actors/actors_shared_80164954.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

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
/// restarted together by the message handlers; `yaw` is the heading
/// `func_actor_403000_8013D364` reads back from the root coordinate (0xC
/// rather than `ActorShared80164af0Work::field_16`); `field_AC6` is the
/// requested animation id, written by the handlers and read back by the
/// handlers that tick the current state.
///
/// `field_AC0` is the playback state `func_actor_403000_80133AF8` switches on
/// (1 is the running state, whose per-frame branch compares `field_AC4`, the
/// animation in progress, against `field_AC6`) and `field_AC2` the frame
/// counter it advances; `field_ACA` is the clip id that tick copies into each
/// display node, so `func_actor_403000_8013D850` starting the actor rewrites
/// `field_AC6` and `field_ACA` together. `field_6` is the per-frame tick that
/// same function bumps, and bit 0x100 of `field_60` is what it watches to tell
/// that the animation it asked for has arrived.
typedef struct Actor403000Work {
    /* 0x000 */ s16            field_0;
    /* 0x002 */ s16            field_2;
    /* 0x004 */ s16            field_4;
    /* 0x006 */ u16            field_6;
    /* 0x008 */ byte           pad_8[0x4];
    /* 0x00C */ s16            yaw;
    /* 0x00E */ byte           pad_E[0x52];
    /* 0x060 */ u16            field_60;
    /* 0x062 */ byte           pad_62[0xA5E];
    /* 0xAC0 */ s16            field_AC0;
    /* 0xAC2 */ s16            field_AC2;
    /* 0xAC4 */ s16            field_AC4;
    /* 0xAC6 */ u16            field_AC6;
    /* 0xAC8 */ byte           pad_AC8[2];
    /* 0xACA */ s16            field_ACA;
    /* 0xACC */ byte           pad_ACC[0x84];
    /* 0xB50 */ Actor403000Obj objB50;
    /* 0xBE8 */ Actor403000Obj objBE8;
    /* 0xC80 */ Actor403000Obj objC80;
    /* 0xD18 */ Actor403000Obj objD18;
    /* 0xDB0 */ byte           pad_DB0[0x21A];
    /* 0xFCA */ s16            field_FCA;
    /* 0xFCC */ byte           pad_FCC[0x6];
    /* 0xFD2 */ s8             field_FD2;
    /* 0xFD3 */ s8             field_FD3;
    /* 0xFD4 */ byte           pad_FD4[0x8];
} Actor403000Work;
STATIC_ASSERT_SIZEOF(Actor403000Work, 0xFDC);

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object.
typedef struct Actor403000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor403000Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor403000;

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

/// Tick the work block's animation playback: state `field_AC0` 1 advances
/// `field_AC2` until it catches up with the requested `field_AC6`, copying
/// `field_ACA` into the four display nodes' clip slot as it goes.
void func_actor_403000_80133AF8(Actor403000* arg0);

/// Copy `placement` onto the actor's root coordinate (Y then X then Z) and
/// cache the resulting heading in `Actor403000Work::yaw`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorShared80164954Placement* placement);

/// Latch the requested animation and restart the animation state machine.
s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg);

/// `Task::exitCallback` installed by the spawn handler, for the teardown path
/// where the enemy was created: hand the four display nodes back to
/// `Gp_UnlinkObj`, drop the enemy's `field_54` slot, then let `Gp_DestroyEnemy`
/// free the enemy and the task.
void func_actor_403000_8013D4F4(Task* task);

/// Copy the `vx`/`vy`/`vz` of record `arg1` of the pose table into `arg0`.
void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1);

void func_actor_403000_8013D5F8(Actor403000* arg0);

/// Per-frame update for the actor once its work block exists: on the frame
/// `field_4` is set, reinstate the display object's buffers and restart the
/// animation state machine on clip 0x10, then tick `field_6` and the playback
/// state, and when bit 0x100 of `field_60` reports the clip has arrived, raise
/// `field_FD2`/`field_FD3` and move the state machine to state 2.
void func_actor_403000_8013D850(Actor403000* arg0);

#endif // ACTOR_403000_H
