#ifndef ACTOR_110300_H
#define ACTOR_110300_H

#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

/// Per-actor work block for the `actor_110300` overlay, reached through the
/// global `func_actor_110300_80131F9C` publishes.
///
/// `anim` sits at offset 0, so `&D_actor_110300_8013A0A0->anim` compiles to the
/// bare pointer load. `slots` follows it directly (0x28 apart, as
/// `Gp_AnimResetSlot` is handed `work + i * 0x28`), and the animation-id pair
/// sits at 0x476/0x478.
typedef struct Actor110300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14]; // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       aux[0x140];  // `GpAnimCtx.poses`, one 0x10-byte record per slot
    /* 0x474 */ s16        field_474;   // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x476 */ s16        field_476;   // copy of `animId`, kept for change detection
    /* 0x478 */ u16        animId;      // animation id the slots are seeded with
    /* 0x47A */ u16        field_47A;   // incremented by the step-0 handler, cleared by the animation-start handler
    /* 0x47C */ byte       pad_47C[0xE0];
} Actor110300Work;
STATIC_ASSERT_SIZEOF(Actor110300Work, 0x55C);

/// Argument block of the 0x7D3 message: the animation to start, after a
/// 4-byte field the handler does not read.
typedef struct Actor110300AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor110300AnimArgs;

/// The block above, published by `func_actor_110300_80131F9C` from the task's
/// `Task::work`.
extern Actor110300Work* D_actor_110300_8013A0A0;

/// The actor's own task, stored by the step-0 handler. The message handlers
/// drive the animation step driver and the model through it, and the helper
/// task's entry `func_actor_110300_80131FF8` parents its coordinate to one of
/// its model's nodes.
extern GpActorWork* D_actor_110300_8013A0A4;

/// The helper task `Task_SpawnFromTable` returns in the step-0 handler; the
/// visibility handler drives its model alongside the actor's, and the exit
/// callback kills it.
extern Task* D_actor_110300_8013A0A8;

/// Spawn descriptor table the step-0 handler spawns the helper task from:
/// index 0 is the actor's own dispatcher, index 1 the helper.
extern TaskDesc D_actor_110300_8013A06C[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110300_8013A084[];

/// Message table published as `Task::msgTable`: the 0x7D3 and 0x7D5 handlers
/// and a terminator.
extern GpMsgEntry D_actor_110300_8013A054[];

void func_actor_110300_80131E24(GpEnemy* enemy, Task* task);
void func_actor_110300_80132020(GpEnemy* enemy, Task* task);
void func_actor_110300_80132088(Task* task);
void func_actor_110300_801320C4(GpActorWork* arg0);
void func_actor_110300_80132138(void);
void func_actor_110300_80132180(void);
void func_actor_110300_80132208(void);

#endif
