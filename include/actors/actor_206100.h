#ifndef ACTOR_206100_H
#define ACTOR_206100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"

/// The `Gp_LinkObj` record `func_actor_206100_8014FBE4` unlinks when it
/// retires the actor, plus the area-record list that handler applies.
extern GpAreaApplyRec D_8018590C;

/// The pair table the beam's collision object carries at `GpObj.field_18`
/// (`Gp_PackPair` kind 1). One word, `field_0` = 0x1A and `field_2` = 5.
extern GpU16Pair D_actor_206100_80155194;

/// Placement records `func_actor_206100_8014EE2C` parks at `GpEnemy::field_3C`
/// -- the same slot `Gp_SpawnArea` fills from a room's own place list, so this
/// is a local six-entry copy of one: `field_0` is 4 on the five live entries
/// and 0xFF on the sixth, the value `Gp_SpawnArea` stops its walk on.  The
/// overlay indexes it with the variant it was spawned for rather than walking
/// it, so the tail entry is reachable.
extern GpAreaPlace D_actor_206100_80155134[];

/// Per-actor state block for the `actor_206100` overlay's enemy.
///
/// `func_actor_206100_8014C274` allocates it with `Mem_Calloc(0x558, 0)` and
/// stores it straight into the `Task::idMap` slot (0x1C), so the size below is
/// the allocation and not a guess: this overlay reuses that pointer field for
/// its own work block and it is *not* a `TaskIdMap` here.  Reach it with
/// `(Actor206100Work*)task->idMap`.  (The overlay's only other allocation,
/// `Mem_Calloc(0x68, 0)` in `func_actor_206100_8014C458`, belongs to the child
/// task that `Task_SpawnFromTable` returns there, so it is a different `Task`
/// and a different block.)
///
/// `field_520` / `field_522` are the state and sub-state indices the handler
/// table walks and `field_51E` is the per-state frame counter -- the same
/// layout the other enemy overlays use.  `field_50C` .. `field_51A` are the
/// animation request the actor hands to its player: `func_actor_206100_8014C274`
/// writes `field_50C` as the request kind and then reads `field_50E` and
/// `field_510` as the clip to play, with `field_51A` the step scale.
///
/// `obj_364` / `obj_414` are the two `Gp_LinkObj` nodes the actor's retirement
/// handler `func_actor_206100_8014FBE4` unlinks, alongside the enemy's own
/// `GpLinkNode`.
typedef struct Actor206100Work {
    /* 0x000 */ byte  pad_0[0x364];
    /* 0x364 */ GpObj obj_364;
    /* 0x384 */ byte  pad_384[0x90];
    /* 0x414 */ GpObj obj_414;
    /* 0x434 */ byte  pad_434[0xD8];
    /* 0x50C */ s16   field_50C; // animation request kind
    /* 0x50E */ byte  pad_50E[0x2];
    /* 0x510 */ s16   field_510; // animation clip id
    /* 0x512 */ byte  pad_512[0x8];
    /* 0x51A */ s16   field_51A; // animation step scale
    /* 0x51C */ byte  pad_51C[0x2];
    /* 0x51E */ u16   field_51E; // per-state frame counter
    /* 0x520 */ s16   field_520; // state index
    /* 0x522 */ u16   field_522; // sub-state index
    /* 0x524 */ s16   field_524;
    /* 0x526 */ u16   field_526;
    /* 0x528 */ byte  pad_528[0xE];
    /* 0x536 */ u16   field_536; // seeded from D_80181A48 when the block is built
    /* 0x538 */ byte  pad_538[0xC];
    /* 0x544 */ s16   field_544; // id handed to func_actor_206100_8014EB48
    /* 0x546 */ byte  pad_546[0xE];
    /* 0x554 */ s8    field_554;
    /* 0x555 */ byte  pad_555[0x1];
    /* 0x556 */ s8    field_556;
    /* 0x557 */ byte  pad_557[0x1];
} Actor206100Work;
STATIC_ASSERT_SIZEOF(Actor206100Work, 0x558);

/// Work block of the beam task `func_actor_206100_8014C458` spawns off
/// `D_actor_206100_80158B0C` when `Actor206100Work::field_555` is set: it
/// `Mem_Calloc(0x68, 0)`s one and parks it in the child's `Task::idMap`, the
/// same reuse `Actor206100Work` makes of the parent's slot.
///
/// `obj` is the kind-1 `GpObj` the spawn state `func_actor_206100_8014EEC0`
/// links into the collision list and `func_actor_206100_8014FBE4` unlinks
/// again on retirement, so the 0x8 before it is not the node's own header and
/// stays zero.  `rec` is the two-entry `GpRec18` table `obj.field_C` points at.
/// `field_58` / `field_5A` / `field_5C` are the view-space deltas the spawner
/// stores from the actor's coordinate, `field_60` the pair index the setup
/// hands to `func_actor_206100_8014A70C`, and `field_64` the scale word it
/// biases by 0x10002000.  The tick handler `func_actor_206100_8014B8B4`
/// advances `field_5A` and adds `field_58` into the coordinate's `t[1]`.
typedef struct Actor206100ChildWork {
    /* 0x00 */ byte    pad_0[0x8];
    /* 0x08 */ GpObj   obj;
    /* 0x28 */ GpRec18 rec[2];
    /* 0x58 */ s16     field_58;
    /* 0x5A */ s16     field_5A;
    /* 0x5C */ s16     field_5C;
    /* 0x5E */ byte    pad_5E[0x2];
    /* 0x60 */ s32     field_60;
    /* 0x64 */ s32     field_64;
} Actor206100ChildWork;
STATIC_ASSERT_SIZEOF(Actor206100ChildWork, 0x68);

/// Builds the child beam's collision state: links its `GpObj` and initializes
/// the coordinate the beam is drawn at. `task` is the child spawned by
/// `func_actor_206100_8014C458`, so its `Task::idMap` is the
/// `Actor206100ChildWork` above.
void func_actor_206100_8014EEC0(Task* task);

/// Moves the beam described by `coord` for one frame. `arg1` is
/// `Actor206100ChildWork::field_60` (the `GpU16Pair` index) and `arg3` the
/// biased `field_64` scale word.
void func_actor_206100_8014A70C(GsCOORDINATE2* coord, u16 arg1, s32 arg2, s32 arg3);

#endif
