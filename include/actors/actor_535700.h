#ifndef ACTOR_535700_H
#define ACTOR_535700_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Work block of the overlay's first enemy, allocated by its spawn handler
/// `func_actor_535700_80131FA0` with `memCalloc(0x4C0, 0)`, so the size is the
/// allocation. The handler publishes it in `D_actor_535700_80146844` as well
/// as the task's `Task::work`, and most of the enemy's code reaches it through
/// the global.
///
/// `light` and `color` become the model's light and colour matrices; `anim`,
/// `field_34C` and `slots` are handed to `func_800B3F84` to start the
/// animation. `state` is what the runner `func_actor_535700_80132108`
/// dispatches on: 1 restarts the clip in `animId` through
/// `func_actor_535700_80132730` and 2 through `func_actor_535700_80132694`,
/// both then leaving it at 3, and both latch the clip into `field_47E`.
/// `field_4AE` is the model's yaw, `field_4B2` the frames of walking left and
/// `field_4B4` the frames of turning left. `field_4B8` is the last animation
/// record the footstep check saw, and `field_4BC` a flag the message handler
/// sets that makes the runner play footsteps.
typedef struct Actor535700Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ s16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        field_4AE;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        field_4B2;
    /* 0x4B4 */ s16        field_4B4;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ GpAnimRec* field_4B8;
    /* 0x4BC */ u8         field_4BC;
    /* 0x4BD */ byte       pad_4BD[0x3];
} Actor535700Work;
STATIC_ASSERT_SIZEOF(Actor535700Work, 0x4C0);

/// The first enemy's work block, published by its spawn handler.
extern Actor535700Work* D_actor_535700_80146844;

/// The first enemy's task, published by its spawn handler so the message
/// handlers can reach its model.
extern Task* D_actor_535700_80146848;

/// Work block of the overlay's second enemy, allocated by its spawn handler
/// `func_actor_535700_80132B58` with `memCalloc(0x4C0, 0)` and hung off the
/// task's `Task::work`.
///
/// `light` and `color` become the model's light and colour matrices, and the
/// sub-model task `func_actor_535700_801333FC` lights its own model with the
/// same pair; `anim`, `field_34C` and `slots` are handed to `func_800B3F84` to
/// start the animation. `state` drives `func_actor_535700_80132D68`: 1 starts
/// the clip in `animId` through `func_actor_535700_80133180` and 2 resets the
/// slots through `func_actor_535700_80133108`, both then advancing to 3 and
/// latching the clip into `appliedAnimId`. `yaw` is the heading last applied
/// to the model's root coordinate, `travel` the distance still to walk in
/// twelfths and `animArg` the reset argument the start path forwards.
/// `field_4B8` keeps the sub-model task and `field_4BC` the enemy that owns
/// the block.
typedef struct Actor535700SpawnWork {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        appliedAnimId;
    /* 0x480 */ u16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ u16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ s16        animArg;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ GpEnemy*   field_4BC;
} Actor535700SpawnWork;
STATIC_ASSERT_SIZEOF(Actor535700SpawnWork, 0x4C0);

/// Message payload the message handlers take as `Gp_DispatchMsg`'s `arg2`:
/// only the halfword at 0x2 is read.
typedef struct Actor535700Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor535700Msg;

/// Payload of the placement opcode: the position the model's root coordinate
/// is moved to, and the rotation whose `vy` becomes its yaw.
typedef struct Actor535700Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor535700Placement;

/// Script args of the second enemy's "start animation" opcode
/// `func_actor_535700_801331E4`: the clip id, a flag choosing the start path,
/// and the reset argument only that path carries.
typedef struct Actor535700AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor535700AnimArgs;

/// Fade countdown at 0x80146840, the word just below the work pointer.
/// `func_actor_535700_80131EF0` seeds it from its argument and spawns the fade
/// task from `D_actor_535700_8013346C`; that task
/// (`func_actor_535700_80131E24`) draws a full-screen black `TILE` into
/// ordering table slot 0xA while the count is non-zero, kills itself once it
/// reaches zero, and decrements the count every frame.
extern s32 D_actor_535700_80146840;

/// Reset argument the first enemy's "play animation" opcode leaves behind:
/// `func_actor_535700_80132730` forwards it to every reseeded slot, and the
/// runner sets it to 10 when a walk ends.
extern s16 D_actor_535700_8013DAA8;

/// Payload of the first enemy's "play animation" script opcode: `field_4` is
/// the animation id, `field_8` picks the state the runner is left in --
/// non-zero for 1, which runs the clip through `func_actor_535700_80132730`
/// using `field_C` as the reset argument, zero for 2, the plain reset
/// `func_actor_535700_80132694` -- and `field_0` is not read here.
typedef struct Actor535700AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor535700AnimPreset;

/// Picks the distance `func_actor_535700_80132108` walks the model each frame:
/// 0 steps 0x3C forward, 1 steps 0xF back, 2 steps 0x19 forward.
extern s16 D_actor_535700_8014684C;

void func_actor_535700_80131FA0(GpEnemy* enemy, Task* task);
void func_actor_535700_801324D4(GpEnemy* enemy, Task* task);

/// Exit callback the first enemy's spawn handler installs on its task: tears
/// down the enemy the task was spawned for.
void func_actor_535700_80132558(Task* task);

/// Plays the first enemy's footstep sounds.
void func_actor_535700_80132580(Task* task);

/// Ticks the first enemy's animation slots.
void func_actor_535700_80132648(void);

/// Resets the first enemy's animation slots to `animId`; the runner's state 2.
void func_actor_535700_80132694(void);

/// Restarts the first enemy's animation with the reset argument in
/// `D_actor_535700_8013DAA8`; the runner's state 1.
void func_actor_535700_80132730(void);

/// Drives the first enemy for one frame off the state its work block holds.
void func_actor_535700_80132108(Task* task);

/// Draws the first enemy's ground shadow quad.
void func_actor_535700_80132ABC(Task* task);

/// Script opcode 0x7D3: applies the animation preset to the work block --
/// the id is copied in, the reset mode is picked by the preset's flag and the
/// reset argument is either taken from the preset or left at 2 -- then
/// restarts the animation through `func_actor_535700_80132108`. Only ids up to
/// 0x22 are accepted; anything else leaves the work block untouched and
/// reports the failure.
s32 func_actor_535700_801327BC(Task* task, s32 arg1, Actor535700AnimPreset* preset, s32 arg3);

void func_actor_535700_80132B58(GpEnemy* enemy, Task* task);

/// The second enemy's animation state machine.
void func_actor_535700_80132D68(Task* task);

void func_actor_535700_80132F74(GpEnemy* enemy, Task* task);

/// Exit callback of the second enemy's task: tears down the enemy the task
/// was spawned for.
void func_actor_535700_80132FF8(Task* task);

/// Ticks, resets and starts the second enemy's animation slots.
void func_actor_535700_801330BC(Task* task);
void func_actor_535700_80133108(Task* task);
void func_actor_535700_80133180(Task* task);

#endif
