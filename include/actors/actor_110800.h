#ifndef ACTOR_110800_H
#define ACTOR_110800_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/task.h"

/// Per-actor work block for the `actor_110800` overlay, reached through the
/// global `func_actor_110800_801322A0` publishes.
///
/// `anim` sits at offset 0, so `&D_actor_110800_80139F10->anim` compiles to the
/// bare pointer load. `slots` follows it directly (0x28 apart, as
/// `Gp_AnimResetSlot` is handed `work + i * 0x28`), and the animation-id pair
/// sits at 0x476/0x478.
typedef struct Actor110800Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14]; // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       aux[0x140];  // `GpAnimCtx.poses`, one 0x10-byte record per slot
    /* 0x474 */ s16        field_474;   // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x476 */ s16        field_476;   // copy of `animId`, kept for change detection
    /* 0x478 */ u16        animId;      // animation id the slots are seeded with
    /* 0x47A */ u16        field_47A;   // bumped once per entry into step 0
    /* 0x47C */ s16        field_47C;   // frame slot 19 or 16 `func_actor_110800_80131F9C` last cued a sound for, kept for change detection
} Actor110800Work;
STATIC_ASSERT_SIZEOF(Actor110800Work, 0x480);

/// Argument block of the 0x7D3 message: the animation to start, after a
/// 4-byte field the handler does not read.
typedef struct Actor110800AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor110800AnimArgs;

/// The block above, published by `func_actor_110800_801322A0` from the task's
/// `Task::work`.
extern Actor110800Work* D_actor_110800_80139F10;

/// The task this actor was paired with when it spawned, stored by the step-0
/// handler and read back by `func_actor_110800_801322FC` to reach the model's
/// own coordinate.
extern GpActorWork* D_actor_110800_80139F14;

/// The self-kill handler's second victim: the helper task `Task_SpawnFromTable`
/// returns here, killed alongside the enemy.
extern Task* D_actor_110800_80139F18;

/// Spawn descriptor table the step-0 handler binds the actor to (the task from
/// index 1 is `D_actor_110800_80139F18` above).
extern TaskDesc D_actor_110800_80139EDC[];

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_110800_80139EF4[];

/// Message table published as `Task::msgTable`, 0x18 bytes below
/// `D_actor_110800_80139EDC` in the same trailing data blob - three
/// `GpMsgEntry` records.
extern GpMsgEntry D_actor_110800_80139EC4[];

/// Self-kill exit callback the step-0 handler installs; defined in
/// `actor_110800_2.c`.
void func_actor_110800_8013232C(Task* task);

/// Animation step driver: dispatches on the work block's `field_474` to one of
/// the three slot routines below. Its argument is never read, so it is
/// declared as the task the handlers pass.
void func_actor_110800_80132368(Task* task);

/// The two state handlers `func_actor_110800_801322A0` dispatches to.
void func_actor_110800_80131E24(GpEnemy* enemy, Task* task);
void func_actor_110800_80131F9C(GpEnemy* enemy, Task* task);

/// Animation slot routines `func_actor_110800_80132368` selects.
void func_actor_110800_801323DC(void);
void func_actor_110800_80132424(void);
void func_actor_110800_801324AC(void);

#endif
