#ifndef ACTOR_450800_H
#define ACTOR_450800_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"

/// Spawn offset `func_actor_450800_80132108` copies into a local and hands to
/// `Gp_SpawnEff` as the effect's position. Stays in assembly (the symbol's
/// rodata run reaches past it), so it is listed `force_not_migration` in the
/// overlay's symbol map and pulled in by its own `INCLUDE_RODATA`.
extern SVECTOR D_actor_450800_80131E24;

/// Work block this overlay hangs off the task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_450800_80132160` allocates it
/// with `memCalloc(0x504, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor450800Work*)task->work`.
///
/// The three tasks at +0x4F0 .. +0x4F8 are the helper tasks this actor spawns;
/// the exit callback `func_actor_450800_80132868` kills all three on teardown.
///
/// The leading `light` / `color` pair and the `anim` / `slots` / `pad_374` run
/// are the same prefix `Actor450800SpawnWork` carries, and the spawn handler
/// `func_actor_450800_80132160` fills them the same way: `&light` and `&color`
/// go to the model's `TmdObject::lightMtx` / `field_20`, and `func_800B3F84`
/// gets `&anim`, `slots` and `pad_374` as poses.
///
/// `yaw` is the heading last applied to the model's root coordinate: the
/// placement opcode `func_actor_450800_80132C68` caches the placed yaw there,
/// the "face target" handler `func_actor_450800_80132D74` the computed one, and
/// the step state turns it further while clip 3 plays.
///
/// `field_4EA` is the step count the "face target" handler leaves for the walk
/// and `field_4EC` the turn count clip 3 consumes; the spawn handler zeroes
/// both.
///
/// `anim` is the animation context `func_800B4114` walks. `field_4B8` is the
/// current animation id; `func_actor_450800_80132AE0` starts slots 1..0x13 of
/// `anim` from it, forwarding `field_4FC` as the reset argument, and
/// `func_actor_450800_80132A68` resets them to it; both latch the id into
/// `field_4B6` as the applied copy.
///
/// `state` drives `func_actor_450800_80132448`, which dispatches on it: 1 starts
/// the animation through `func_actor_450800_80132AE0` and 2 resets the slots
/// through `func_actor_450800_80132A68`, both then advancing it to 3.
typedef struct Actor450800Work {
    /* 0x000 */ MATRIX     light; // model light matrix (`TmdObject::lightMtx`)
    /* 0x020 */ MATRIX     color; // model colour matrix (`TmdObject::colorMtx`)
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        field_4B6;
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ s16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        field_4EA; // distance to the target over the step count
    /* 0x4EC */ s16        field_4EC;
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ Task*      field_4F4;
    /* 0x4F8 */ Task*      field_4F8;
    /* 0x4FC */ s16        field_4FC;
    /* 0x4FE */ s16        field_4FE; // approach mode the last call selected
    /* 0x500 */ u8         field_500; // 0x7DB mode 1 latches the copied flags here, 2 the 0x84 state
    /* 0x501 */ byte       pad_501[0x3];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

/// The overlay's *second* work block, hung off the `Task::work` slot exactly
/// as `Actor450800Work` is but never on the same task: this one is the enemy's,
/// allocated by the spawn handler `func_actor_450800_80132E9C` with
/// `memCalloc(0x4C0, 0)` and driven by the state `func_actor_450800_801330AC`
/// reads at 0x47C, while `Actor450800Work` (0x504) belongs to the actor's own
/// handler `func_actor_450800_80132160`. The overlay's two `fns` dispatchers
/// (`func_actor_450800_80133264` and `func_actor_450800_80132790`) are what keep
/// them apart, and both sizes are the allocations, not a guess.
///
/// `state` drives `func_actor_450800_801330AC` the way `Actor450800Work::state`
/// drives `func_actor_450800_80132448` for the actor: 1 starts the animation
/// through `func_actor_450800_801334C4` and 2 resets the slots through
/// `func_actor_450800_8013344C`, both then advancing it to 3. `animId` is the
/// clip now playing - the spawn handler sets it to 1 and the state machine
/// tests it against 4 - and both start paths latch it into `appliedAnimId`.
/// `anim` / `slots` are what `func_800B3F84` fills in.
///
/// `yaw` is the heading last applied to the model's root coordinate, cached by
/// the placement opcode `func_actor_450800_801335F8` and the "walk to" opcode
/// `func_actor_450800_80133678`; the latter also leaves in `travel` the
/// distance to cover, in twelfths. State 3 counts `travel` down a step per
/// frame while the walk clip plays and, when it reaches zero, drops back to
/// clip 1 with 0xA in `animArg`, the reset argument the start path forwards.
///
/// The leading matrices are the ones the enemy renders through - the spawn
/// handler hands `&light` and `&color` to the object's `TmdObject::lightMtx` /
/// `colorMtx`, and the sub-model task `func_actor_450800_80133740` lights its
/// own model with the same pair. `field_4B8` is that sub-model's task and
/// `field_4BC` the `GpEnemy` the spawn handler was given.
typedef struct Actor450800SpawnWork {
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
} Actor450800SpawnWork;
STATIC_ASSERT_SIZEOF(Actor450800SpawnWork, 0x4C0);

/// Message payload the overlay's message handlers take as `Gp_DispatchMsg`'s
/// `arg2`: only the halfword at 0x2 is read.
typedef struct Actor450800Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor450800Msg;

/// Script args the "start animation" opcodes receive - the actor's
/// `func_actor_450800_80132B44` and the enemy's `func_actor_450800_80133528`:
/// the clip id, a flag choosing the start path, and the reset argument only
/// that path carries.
typedef struct Actor450800AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor450800AnimArgs;

/// Payload of the placement opcode (message 0x7D4) both of the overlay's
/// message tables carry: the position the model's root coordinate is moved to,
/// and the rotation whose `vy` becomes its yaw.
typedef struct Actor450800Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor450800Placement;

void func_actor_450800_80131F28(s32 arg0);
void func_actor_450800_80132A1C(Task* task);
void func_actor_450800_80132A68(Task* task);
s32  func_actor_450800_80132C68(Task* task, s32 arg1, Actor450800Placement* placement);
void func_actor_450800_801330AC(Task* task);
void func_actor_450800_8013333C(Task* task);
void func_actor_450800_80133400(Task* task);
void func_actor_450800_8013344C(Task* task);
void func_actor_450800_801334C4(Task* task);
s32  func_actor_450800_80133528(Task* task, s32 arg1, Actor450800AnimArgs* args);
s32  func_actor_450800_80133594(Task* task, s32 arg1, s32 flags);
s32  func_actor_450800_801335F8(Task* task, s32 arg1, Actor450800Placement* placement);
s32  func_actor_450800_80133678(Task* task, s32 arg1, VECTOR* target);
void func_actor_450800_80133740(Task* task);

#endif
