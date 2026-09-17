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

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_450800_80132160` allocates it
/// with `Mem_Calloc(0x504, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor450800Work*)task->idMap`.
///
/// The three tasks at +0x4F0 .. +0x4F8 are the helper tasks this actor spawns;
/// the exit callback `func_actor_450800_80132868` kills all three on teardown.
///
/// `yaw` and `travel` are the cache the "walk to" placement opcode writes:
/// the heading it applied to the root coordinate and the distance left to
/// cover, in twelfths. That opcode is the shared body `ActorsShared80133678`,
/// which reaches the same two fields through `ActorsShared80133678Work`.
///
/// `anim` is the animation context `func_800B4114` walks. `field_4B8` is the
/// current animation id; `func_actor_450800_80132AE0` starts slots 1..0x13 of
/// `anim` from it, forwarding `field_4FC` as the reset argument, and latches the
/// id into `field_4B6` as the copy kept for change detection.
///
/// `state` drives `func_actor_450800_80132448`, which dispatches on it: 1 starts
/// the animation through `func_actor_450800_80132AE0` and 2 reseeds the slots
/// through `ActorsShared80132514`, the same pair `func_actor_460200_80132B2C`
/// selects between.
typedef struct Actor450800Work {
    /* 0x000 */ byte      pad_0[0x40];
    /* 0x040 */ GpAnimCtx anim;
    /* 0x054 */ byte      pad_54[0x45A];
    /* 0x4AE */ u16       yaw;
    /* 0x4B0 */ byte      pad_4B0[0x2];
    /* 0x4B2 */ s16       travel;
    /* 0x4B4 */ s16       state;
    /* 0x4B6 */ s16       field_4B6;
    /* 0x4B8 */ s16       field_4B8;
    /* 0x4BA */ s16       field_4BA;
    /* 0x4BC */ byte      pad_4BC[0x2A];
    /* 0x4E6 */ s16       field_4E6; // yaw seeding the root coordinate
    /* 0x4E8 */ byte      pad_4E8[0x2];
    /* 0x4EA */ s16       field_4EA; // distance to the target over the step count
    /* 0x4EC */ byte      pad_4EC[0x4];
    /* 0x4F0 */ Task*     field_4F0;
    /* 0x4F4 */ Task*     field_4F4;
    /* 0x4F8 */ Task*     field_4F8;
    /* 0x4FC */ s16       field_4FC;
    /* 0x4FE */ s16       field_4FE; // approach mode the last call selected
    /* 0x500 */ u8        field_500; // 0x7DB mode 1 latches the copied flags here, 2 the 0x84 state
    /* 0x501 */ byte      pad_501[0x3];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

/// The overlay's *second* work block, hung off the `Task::idMap` slot exactly
/// as `Actor450800Work` is but never on the same task: this one is the enemy's,
/// allocated by the spawn handler `func_actor_450800_80132E9C` with
/// `Mem_Calloc(0x4C0, 0)` and driven by the state `func_actor_450800_801330AC`
/// reads at 0x47C, while `Actor450800Work` (0x504) belongs to the actor's own
/// handler `func_actor_450800_80132160`. The overlay's two `fns` dispatchers
/// (`func_actor_450800_80133264` and `func_actor_450800_80132790`) are what keep
/// them apart, and both sizes are the allocations, not a guess.
///
/// `state` drives `func_actor_450800_801330AC` the way `field_4B4` drives
/// `func_actor_450800_80132448` for the actor: 1 starts the animation through
/// `ActorsShared80132640` and 2 reseeds the slots through `ActorsShared801325c8`,
/// both then advancing it to 3. `animId` is the clip now playing - the spawn
/// handler sets it to 1 and the state machine tests it against 4. `anim` /
/// `slots` are what `func_800B3F84` fills in.
///
/// The leading matrices are the ones the enemy renders through - the spawn
/// handler hands `&light` and `&color` to the object's `field_1C` / `field_20` -
/// and `field_4B8` / `field_4BC` are the task and the `GpEnemy` of the model
/// that handler spawns, the same two roles `Actor150400Work` gives them.
typedef struct Actor450800SpawnWork {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        state;
    /* 0x47E */ byte       pad_47E[0x2];
    /* 0x480 */ u16        animId;
    /* 0x482 */ byte       pad_482[0x36];
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ GpEnemy*   field_4BC;
} Actor450800SpawnWork;
STATIC_ASSERT_SIZEOF(Actor450800SpawnWork, 0x4C0);

/// Message payload the overlay's message handlers take as `Gp_DispatchMsg`'s
/// `arg2`, the shape `Actor461800Msg` and `Actor560800Msg` share: only the
/// halfword at 0x2 is read.
typedef struct Actor450800Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor450800Msg;

/// Script args the "start animation" opcode `func_actor_450800_80132B44`
/// receives: the clip id, a flag choosing the start path, and the reset
/// argument only that path carries. Same shape as the `Actor150400AnimArgs` and
/// `Actor460200AnimArgs` that opcode's twins take.
typedef struct Actor450800AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor450800AnimArgs;

#endif
