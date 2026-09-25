#ifndef ACTOR_451100_H
#define ACTOR_451100_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block of both of this overlay's actors, hung off their task's
/// `Task::work` slot (0x1C), which is not a `TaskIdMap` here.
///
/// The two actors allocate it at different sizes: the one dispatched by
/// `func_actor_451100_801322D4` takes `memCalloc(0x4B8, 0)` and also publishes
/// the block in `D_actor_451100_8014E744`, while the one dispatched by
/// `func_actor_451100_80132BD4` takes `memCalloc(0x4C0, 0)` and reaches the
/// block only through the task. Only the second uses `pairTask` and `enemy`,
/// the fields past 0x4B8; up to there both actors access the same offsets the
/// same way.
///
/// `light` and `color` are the matrices the model renders through (the spawn
/// handlers hand their addresses to `TmdObject::lightMtx` / `colorMtx`), and
/// `anim` / `slots` the animation context and slot array `func_800B3F84`
/// seeds.
///
/// `animId` is the clip to play, `animArg` the extra argument only the
/// `withArg` start path forwards. `state` is what each actor's step routine
/// dispatches on: 1 restarts the clip with a reset argument, 2 restarts it
/// plainly, and both then advance to 3, the per-frame state.
///
/// `travel` counts a walk clip down a step per frame and `yaw` is the heading
/// last applied to the model's root coordinate. `field_47E` is `animId` as it
/// was when the slots were last seeded: every walk over the slot array latches
/// it once it has re-aimed every slot, so it records the set now playing.
typedef struct Actor451100Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x108];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ u16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ u16        animArg;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ Task*      pairTask;
    /* 0x4BC */ GpEnemy*   enemy;
} Actor451100Work;
STATIC_ASSERT_SIZEOF(Actor451100Work, 0x4C0);

/// Work block of the actor `func_actor_451100_801322D4` dispatches, published
/// by its spawn handler so the actor's message handlers and animation helpers
/// reach it without the task.
extern Actor451100Work* D_actor_451100_8014E744;

/// That same actor's task, stored by its spawn handler for the handlers that
/// need the task but are not given it.
extern Task* D_actor_451100_8014E748;

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_451100_8013F700;

/// Picks the distance `func_actor_451100_80131F84` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_451100_8014E74C;

/// Argument block of the "start animation" script opcodes: which animation to
/// play, and how.
typedef struct Actor451100AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor451100AnimArgs;

/// Payload of the message handlers that only read the halfword at 0x2.
typedef struct Actor451100Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor451100Msg;

/// Payload of the placement opcode (message 0x7D4) both actors' message
/// tables carry: the position the model's root coordinate is moved to, and the
/// rotation whose `vy` becomes its yaw.
typedef struct Actor451100Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor451100Placement;

void func_actor_451100_80131E24(GpEnemy* enemy, Task* task);
void func_actor_451100_80132330(GpEnemy* enemy, Task* task);

/// Task exit callback this overlay installs on spawn.
void func_actor_451100_80132CAC(Task* task);

/// Exit callback the `func_actor_451100_801322D4` state-0 handler installs on
/// the model's task: tears down the enemy the task was spawned for.
void func_actor_451100_801323B4(Task* task);

/// Ticks the animation once the runner has moved and turned the model.
void func_actor_451100_801323DC(void);

void func_actor_451100_80132428(void);

/// Restarts the animation with the reset argument in
/// `D_actor_451100_8013F700`; the runner's state 1.
void func_actor_451100_801324B8(void);

/// Per-frame update: states 1 and 2 run their one-shot animation reseed and
/// leave the work block in state 3; state 3 walks the model while `travel`
/// counts down (distance picked by `D_actor_451100_8014E74C`), turns it while
/// `animArg` counts down in animation 3, then ticks the animation.
void func_actor_451100_80131F84(Task* task);

/// Second script opcode taking `Actor451100AnimArgs`: like
/// `func_actor_451100_80132E98`, but it accepts a wider id range and enters the
/// run through `func_actor_451100_80131F84`.
s32 func_actor_451100_80132538(Task* task, s32 arg1, Actor451100AnimArgs* args);

void func_actor_451100_8013280C(Task* task);
void func_actor_451100_801328A8(GpEnemy* enemy, Task* task);
void func_actor_451100_80132A1C(Task* task);
void func_actor_451100_80132C28(GpEnemy* enemy, Task* task);
void func_actor_451100_80132D70(Task* task);
void func_actor_451100_80132DBC(Task* task);
void func_actor_451100_80132E34(Task* task);

s32 func_actor_451100_80132E98(Task* task, s32 arg1, Actor451100AnimArgs* args);

#endif
