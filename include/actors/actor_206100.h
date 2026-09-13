#ifndef ACTOR_206100_H
#define ACTOR_206100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries.  The same shape `ActorsShared801639a8MatWords` has.
typedef struct Actor206100MatrixWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor206100MatrixWords;

typedef union Actor206100Matrix {
    MATRIX                 mat;
    Actor206100MatrixWords ident;
} Actor206100Matrix;
STATIC_ASSERT_SIZEOF(Actor206100Matrix, 0x20);

/// The five sub-state handlers `func_actor_206100_8014F524` picks between: it
/// copies the table onto its stack and calls `funcs[(s16)field_522]`, the same
/// local-jump-table shape `func_actor_341700_80168748` uses.  The entries are
/// the handlers themselves, so this is the only writer of the sub-state index
/// that does not live in this overlay's own `_3` unit.
extern TaskFuncTable5 D_actor_206100_80149E94;

/// The `Gp_LinkObj` record `func_actor_206100_8014FBE4` unlinks when it
/// retires the actor, plus the area-record list that handler applies.
extern GpAreaApplyRec D_8018590C;

/// The pair table the beam's collision object carries at `GpObj.field_18`
/// (`Gp_PackPair` kind 1). One word, `field_0` = 0x1A and `field_2` = 5.
extern GpU16Pair D_actor_206100_80155194;

/// Pair source `func_actor_206100_8014AF74` parks in `GpEnemy::field_50`, whose
/// `field_0` is `D_actor_206100_80155194` above and whose `field_4` is the
/// actor's max HP (2000), seeded into `field_40` / `field_42` at spawn.
extern GpPairSrcE D_actor_206100_80155198;

/// Animation bank handed to `func_800B3F84` by `func_actor_206100_8014AF74`.
extern GpAnimSet* D_actor_206100_80158B24[];

/// Placement records `func_actor_206100_8014EE2C` parks at `GpEnemy::field_3C`
/// -- the same slot `Gp_SpawnArea` fills from a room's own place list, so this
/// is a local six-entry copy of one: `field_0` is 4 on the five live entries
/// and 0xFF on the sixth, the value `Gp_SpawnArea` stops its walk on.  The
/// overlay indexes it with the variant it was spawned for rather than walking
/// it, so the tail entry is reachable.
extern GpAreaPlace D_actor_206100_80155134[];

/// 0x28-byte stride overlay of `Actor206100Work` from offset 0.  `anim` is
/// 0x14 bytes, so `stride[i].field_1D` is `slots[i].field_9` -- the same
/// overlap `Actor400500AnimStride` describes.  Walked from index 1 by
/// `func_actor_206100_8014F284`.
typedef struct Actor206100AnimStride {
    /* 0x00 */ byte pad[0x1D];
    /* 0x1D */ u8   field_1D;
    /* 0x1E */ byte pad_1E[0xA];
} Actor206100AnimStride;
STATIC_ASSERT_SIZEOF(Actor206100AnimStride, 0x28);

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
/// `anim` is the animation context at offset 0 -- the block is handed to
/// `Gp_AnimResetSlot` as its `GpAnimCtx` -- with the 0x28-byte animation
/// slots at +0x14, the layout `Actor400500Work` uses.
///
/// `obj_364` / `obj_414` are the two `Gp_LinkObj` nodes the actor's retirement
/// handler `func_actor_206100_8014FBE4` unlinks, alongside the enemy's own
/// `GpLinkNode`.  Both nodes point their `field_C` at the same six-entry
/// `GpRec18` table `func_actor_206100_8014F18C` zeroes in `pad_384`, which is
/// why `Gp_InitRec18Table` is called once for the pair.
typedef struct Actor206100Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0xF];
    /// `func_800B3F84`'s arg3 buffer, the 0x90-byte scratch every animation
    /// context carries alongside its slot array.
    /* 0x26C */ byte  animAux[0x90];
    /* 0x2FC */ byte  pad_2FC[0x68];
    /* 0x364 */ GpObj obj_364;
    /* 0x384 */ byte  pad_384[0x90];
    /* 0x414 */ GpObj obj_414;
    /* 0x434 */ byte  pad_434[0xA];
    /// Yaw `func_actor_206100_8014AF74` reads back out of the root
    /// coordinate's third row at spawn.
    /* 0x43E */ s16  field_43E;
    /* 0x440 */ byte pad_440[0x20];
    /// The light / colour matrix pair the two `TmdObject` slots point at,
    /// the same `field_1C` / `field_20` hand-off `actor_503500` makes.
    /* 0x460 */ MATRIX colorMtx;
    /* 0x480 */ MATRIX lightMtx;
    /* 0x4A0 */ byte   pad_4A0[0x20];
    /// Effect argument: the root coordinate's second part with the overlay's
    /// effect id and part index, the same coordinate / id / 3 trio
    /// `Actor503500Work::field_6E4` holds.
    /* 0x4C0 */ GpEffArg eff_4C0;
    /* 0x4C8 */ byte     pad_4C8[0x44];
    /* 0x50C */ s16      field_50C; // animation request kind
    /* 0x50E */ u16      field_50E; // clip the request plays, latched from field_510
    /* 0x510 */ s16      field_510; // animation clip id
    /* 0x512 */ byte     pad_512[0x8];
    /* 0x51A */ s16      field_51A; // animation step scale
    /* 0x51C */ byte     pad_51C[0x2];
    /* 0x51E */ u16      field_51E; // per-state frame counter
    /* 0x520 */ s16      field_520; // state index
    /* 0x522 */ u16      field_522; // sub-state index
    /* 0x524 */ s16      field_524;
    /* 0x526 */ u16      field_526;
    /* 0x528 */ byte     pad_528[0xE];
    /* 0x536 */ u16      field_536; // seeded from D_80181A48 when the block is built
    /* 0x538 */ byte     pad_538[0x8];
    /// Yaw offset `func_actor_206100_8014EB60` folds into the part-5 rotation
    /// and the walk state `func_actor_206100_8014EC54` ramps to zero.
    /* 0x540 */ s16  field_540;
    /* 0x542 */ byte pad_542[0x2];
    /* 0x544 */ s16  field_544; // id handed to func_actor_206100_8014EB48
    /* 0x546 */ byte pad_546[0xE];
    /* 0x554 */ s8   field_554;
    /* 0x555 */ byte pad_555[0x1];
    /* 0x556 */ s8   field_556;
    /// Animation step the spawn state leaves at 4 (`func_actor_206100_8014F284`
    /// copies it into every slot's `field_1D`).
    /* 0x557 */ s8 field_557;
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

/// Spawn-state body: hands the freshly spawned enemy its model, its part
/// coordinate and its state, then starts the animation.
///
/// `task->spawnArg2` is the `GpEnemy` `func_actor_206100_8014EC14` spawned, so
/// this is the writer of nearly every field that spawn leaves unset.  The
/// `TmdObject` at `task->extra` gets the two `MATRIX` slots the overlay's
/// light / colour hand-off uses (`field_1C` the 0x480 `lightMtx`, `field_20`
/// the 0x460 `colorMtx`) and `field_E` 0xA -- the same "render state" byte
/// `Tmd_ProcessStream` reads back.  `coord` is the model's root coordinate,
/// which the effect argument at `eff_4C0` reuses for part 1 (`field_8[1]`),
/// so `enemy->field_4` and the effect share one coordinate.
///
/// `hp` is read once into a local because `D_actor_206100_80155198.field_4` is
/// the pair's max HP and both `field_40` and `field_42` take it -- reading the
/// global twice instead costs a register and shifts the whole function's
/// allocation (see `DECOMPILATION_LEARNINGS.md`, "A repeated global load ...").
/// The two `task->extra` walks after `Gp_LinkNode` are separate reloads in the
/// original, which is why `tmd` is not reused for `field_8[4]`.
void func_actor_206100_8014AF74(Task* task);

/// Builds the enemy's two collision objects.  Each is bound to a part
/// coordinate of the actor's `TmdObject` -- `obj_364` to `field_8[1]` with
/// `field_1C` 0x400, `obj_414` to `field_8[4]` with 0x200 -- and both point
/// their `field_C` at the shared `GpRec18` pair table zeroed at `pad_384`,
/// which is why there is a single `Gp_InitRec18Table` for the pair.  Each
/// block ends by clearing `flags` bit 0x8000 after its `Gp_LinkObj`, the same
/// tail shape `func_actor_403100_80132320` has (`|= 0x8000` there).
void func_actor_206100_8014F18C(Task* task);

/// Builds the child beam's collision state: links its `GpObj` and initializes
/// the coordinate the beam is drawn at. `task` is the child spawned by
/// `func_actor_206100_8014C458`, so its `Task::idMap` is the
/// `Actor206100ChildWork` above.
void func_actor_206100_8014EEC0(Task* task);

/// Moves the beam described by `coord` for one frame. `arg1` is
/// `Actor206100ChildWork::field_60` (the `GpU16Pair` index) and `arg3` the
/// biased `field_64` scale word.
void func_actor_206100_8014A70C(GsCOORDINATE2* coord, u16 arg1, s32 arg2, s32 arg3);

/// Steps the actor's model coordinate `arg1` along the heading `arg2`, in the
/// XZ plane, and marks it dirty.
///
/// `task->extra` is the actor's `TmdObject`, so `field_8` is the root
/// `GsCOORDINATE2` of its part array: `coord.t[0]` gains `rsin(arg2) * arg1`
/// and `coord.t[2]` `rcos(arg2) * arg1`. The `<< 4` on the `rsin` / `rcos`
/// result and the `>> 16` after the multiply are one `>> 12` split in two, the
/// unit circle the rest of the overlay's rotation code uses. Clearing `flg` is
/// what makes `GsGetLw` rebuild the matrix from `coord`, so the caller never
/// writes `workm` itself. The `task->extra` chain is walked again for each of
/// the three statements because `rsin` / `rcos` sit between them.
///
/// Every call site in this overlay takes `arg2` from the actor's heading and
/// `arg1` from a step distance, either a constant (`0x30`, `0x40`) or an
/// `s16` the caller narrows itself.
void func_actor_206100_8014EA8C(Task* task, s16 arg1, s16 arg2);

#endif
