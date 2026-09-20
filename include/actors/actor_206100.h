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

/// Shared stack storage for posing the actor, spawning its beam, and walking
/// the parent coordinates to determine whether the actor can be locked onto.
typedef struct Actor206100GteView {
    SVECTOR out;
    SVECTOR vec;
    union {
        VECTOR  mac;
        SVECTOR alt;
    } m;
} Actor206100GteView;

typedef union Actor206100VecScratch {
    Actor206100Matrix  matrix;
    Actor206100GteView gte;
} Actor206100VecScratch;

STATIC_ASSERT_SIZEOF(Actor206100VecScratch, 0x20);

extern const TaskFuncTable9 D_actor_206100_80149E70;
extern TaskDesc             D_actor_206100_80158B0C;

/// Status flags `func_actor_206100_8014F970` reads through two widths: bit 0
/// as a halfword, then bits 0x102 as a word.  The same union
/// `ActorsShared8013a0b0Flags` is -- two widths on one address means two views
/// of the field in the original source, and declaring it a single `u16` makes
/// the second test `lhu` too.
typedef union Actor206100Flags {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
    /// The two halfwords `word` spans: the status flags, and the per-frame
    /// counter `func_actor_206100_8014DA28` bumps alongside `field_518`.  The
    /// counter sitting inside the word is why the 0x102 test reaches its own
    /// bit 0 as well.
    struct {
        /* 0x0 */ u16 half;
        /* 0x2 */ u16 field_516;
    } parts;
} Actor206100Flags;
STATIC_ASSERT_SIZEOF(Actor206100Flags, 0x4);

/// The five sub-state handlers `func_actor_206100_8014F524` picks between: it
/// copies the table onto its stack and calls `funcs[(s16)field_522]`, the same
/// local-jump-table shape `func_actor_341700_80168748` uses.  The entries are
/// the handlers themselves, so this is the only writer of the sub-state index
/// that does not live in this overlay's own `_3` unit.
extern TaskFuncTable5 D_actor_206100_80149E94;

/// The three sub-state handlers of the state-0 dispatcher
/// `func_actor_206100_8014D380`, which copies the table onto its stack the same
/// way and calls `funcs[(s16)field_522]`.  Unlike `D_actor_206100_80149E94`
/// above -- and unlike the local array `func_actor_206100_8014CFF4` builds --
/// this one is a table in its own right: the copy is a three-word block move
/// from `.rodata`, which is what the dispatcher's `lui` / three `lw` / three
/// `sw` prologue is.  `func_actor_206100_8014D6F4` has the same body over the
/// sibling table `D_actor_206100_80149EB4`.
extern const TaskFuncTable3 D_actor_206100_80149EA8;

/// The sibling table `func_actor_206100_8014D6F4` dispatches between, one
/// object after `D_actor_206100_80149EA8` in `.rodata`: sub-state 0 is
/// `func_actor_206100_8014F7B4`, 1 is the debris ring
/// `func_actor_206100_8014D8E8`, 2 is `func_actor_206100_8014F878`.
extern const TaskFuncTable3 D_actor_206100_80149EB4;

/// The four handlers `func_actor_206100_8014E7D4` picks between as the effect
/// mode `D_801153F4` changes -- the retirement `func_actor_206100_8014FBE4`,
/// the idle tick `func_actor_206100_8014FCD4`, the teleport tick
/// `func_actor_206100_8014E964` and `func_actor_206100_8014FDE8`.  Its copy
/// onto the stack is the same three-word block move the two tables above get,
/// but its call is the only one in this overlay that passes a second argument,
/// the local table itself; the element type carries that argument even though
/// none of the four reads it.
typedef void (*Actor206100StateFunc)(Task*, void*);

typedef struct {
    Actor206100StateFunc funcs[4];
} Actor206100StateTable4;

/// The state table above, the last object in this unit's `.rodata`: the code
/// it hands off to, `func_actor_206100_80149ED0`, starts where it ends.
extern const Actor206100StateTable4 D_actor_206100_80149EC0;

/// The `Gp_LinkObj` record `func_actor_206100_8014FBE4` unlinks when it
/// retires the actor, plus the area-record list that handler applies.
extern GpAreaApplyRec D_8018590C;

/// The pair table the beam's collision object carries at `GpObj.key`
/// (`Gp_PackPair` kind 1). One word, `field_0` = 0x1A and `field_2` = 5.
extern GpU16Pair D_actor_206100_80155194;

/// Pair source `func_actor_206100_8014AF74` parks in `GpEnemy::param`, whose
/// `pairTable` is `D_actor_206100_80155194` above and whose `hpMax` is the
/// actor's max HP (2000), seeded into `field_40` / `field_42` at spawn.
extern GpPairSrcE D_actor_206100_80155198;

/// Animation bank handed to `func_800B3F84` by `func_actor_206100_8014AF74`.
extern GpAnimSet* D_actor_206100_80158B24[];

/// Placement records `func_actor_206100_8014EE2C` parks at `GpEnemy::place`
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

/// One vertex of the 8-point ring `func_actor_206100_8014FAE4` steps the actor's
/// root coordinate around: radius 7600 in the XZ plane, one 45-degree step per
/// entry, at a constant 3000 height.  `field_6` is unread.
typedef struct Actor206100RingPos {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor206100RingPos;
STATIC_ASSERT_SIZEOF(Actor206100RingPos, 0x8);

/// Eight `Actor206100RingPos` entries, walked by the index at
/// `Actor206100Work::field_548`.
extern Actor206100RingPos D_actor_206100_80158B68[8];

/// One of the actor's two companion slots: the enemy the spawner
/// `func_actor_206100_8014EE2C` returned for it, and the cooldown that keeps
/// the slot empty for a while after that enemy retires.  The two fill in
/// `Actor206100Work::field_551` order, and the tick
/// `func_actor_206100_8014DD3C` releases a slot when its enemy's health counter
/// `field_40` runs out -- emptying the pointer and arming the timer with 0xB4
/// frames, counted down one a frame while the slot stays empty.
///
/// The tick reaches both fields by *index* (`D_actor_206100_80158CBC[i].enemy`)
/// rather than through a walking pointer, and that is load-bearing: with a
/// pointer the timer's read and write are two identical `DEST_ADDR` givs on the
/// same biv, which `combine_givs` merges into one that passes
/// `strength_reduce`'s "worth while" test -- so the second field gets an
/// induction variable of its own and every later value shifts up a register.
/// See `DECOMPILATION_LEARNINGS.md`, "A walked pointer's second field becomes a
/// second induction variable".
typedef struct Actor206100Slot {
    /* 0x0 */ GpEnemy* enemy;
    /* 0x4 */ s32      timer;
} Actor206100Slot;
STATIC_ASSERT_SIZEOF(Actor206100Slot, 0x8);

/// The two companion slots, zeroed as an 8-byte-stride pair by the spawn state
/// `func_actor_206100_8014C274`.
extern Actor206100Slot D_actor_206100_80158CBC[2];

/// 0xC-byte scratch `func_actor_206100_8014ED3C` takes off `G_SCRATCH_HEAD` to
/// hold the actor's position mirrored through the origin and its distance from
/// it: `delta` is the negated root coordinate (`vy` is left unwritten, the walk
/// is planar) and `dist` the `SquareRoot0` of the two written squares.
///
/// The same `SVECTOR` + length shape `Actor00100RadiusScratch` and
/// `Actor01900RangeScratch` have, and the same walk `Actor00100_OutsideRadius`
/// makes.
typedef struct Actor206100DistScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s32     dist;
} Actor206100DistScratch;
STATIC_ASSERT_SIZEOF(Actor206100DistScratch, 0xC);

/// Per-actor state block for the `actor_206100` overlay's enemy.
///
/// `func_actor_206100_8014C274` allocates it with `memCalloc(0x558, 0)` and
/// stores it straight into the `Task::work` slot (0x1C), so the size below is
/// the allocation and not a guess: this overlay reuses that pointer field for
/// its own work block and it is *not* a `TaskIdMap` here.  Reach it with
/// `(Actor206100Work*)task->work`.  (The overlay's only other allocation,
/// `memCalloc(0x68, 0)` in `func_actor_206100_8014C458`, belongs to the child
/// task that `Task_SpawnFromTable` returns there, so it is a different `Task`
/// and a different block.)
///
/// `field_520` / `field_522` are the state and sub-state indices the handler
/// table walks and `field_51E` is the per-state frame counter -- the same
/// layout the other enemy overlays use.  `field_50C`, `field_50E`, `field_510`
/// and `field_51A` are the animation request the actor hands to its player:
/// `func_actor_206100_8014C274` writes `field_50C` as the request kind and then
/// reads `field_50E` and `field_510` as the clip to play, with `field_51A` the
/// step scale and `field_512` the clip phase the idle handler
/// `func_actor_206100_8014FCD4` ramps -- zeroed when the requested clip is not
/// the one playing, otherwise advanced by `func_actor_206100_8014F3C8` and
/// stepped once per frame in sub-state 3.
/// `flags_514` sits between `field_510` and `field_51A` and is
/// status, not part of the request: `func_actor_206100_8014F970` tests bit 0 of
/// its halfword or bits 0x102 of its word to decide whether to advance the
/// actor to state 2.
/// `anim` is the animation context at offset 0 -- the block is handed to
/// `Gp_AnimResetSlot` as its `GpAnimCtx` -- with the 0x28-byte animation
/// slots at +0x14, the layout `Actor400500Work` uses.
///
/// `obj_364` / `obj_414` are the two `Gp_LinkObj` nodes the actor's retirement
/// handler `func_actor_206100_8014FBE4` unlinks, alongside the enemy's own
/// `GpLinkNode`.  Both nodes point their `ctx.recs` at the same six-entry
/// `GpRec18` table `func_actor_206100_8014F18C` zeroes in `rec_384`, which is
/// why `Gp_InitRec18Table` is called once for the pair.
typedef struct Actor206100Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0xF];
    /// `func_800B3F84`'s arg3 buffer, the 0x90-byte scratch every animation
    /// context carries alongside its slot array.
    /* 0x26C */ byte animAux[0x90];
    /* 0x2FC */ byte pad_2FC[0x60];
    /// Pair `func_actor_206100_8014D574` halves once a frame while
    /// `field_51E` is below 0x1E, ahead of the explosion it triggers on that
    /// frame: both are read back as `u16` (the load is `lhu`), so the halving
    /// is unsigned-promoted, and each is stored with a plain `sh`.
    /* 0x35C */ u16   field_35C;
    /* 0x35E */ u16   field_35E;
    /* 0x360 */ u16   field_360;
    /* 0x362 */ byte  pad_362[0x2];
    /* 0x364 */ GpObj obj_364;
    /// The six-entry contact table `func_actor_206100_8014F18C` zeroes and both
    /// objects above point their `ctx.recs` at.  `func_actor_206100_8014BAA8`
    /// walks it one record a step: `field_4` carries the packed hit id whose
    /// high half selects kind 2, and the walk stops early once `field_52A` goes
    /// up.  The record's own `field_0` / `field_8..field_14` are the occupancy
    /// flags `Gp_ClearRec18Occupied` walks, so nothing here reads them.
    /* 0x384 */ GpRec18 rec_384[6];
    /* 0x414 */ GpObj   obj_414;
    /// Post the actor walks out from: `func_actor_206100_8014B698` latches the
    /// root coordinate's three halves here, and `func_actor_206100_8014ED3C`
    /// snaps `t[0]` / `t[2]` back to `field_434` / `field_438` once the walk has
    /// carried the actor more than 0x191 away from it.
    /* 0x434 */ s16  field_434;
    /* 0x436 */ s16  field_436; // written with the pair, never read
    /* 0x438 */ s16  field_438;
    /* 0x43A */ byte pad_43A[0x2];
    /// Euler triple `func_actor_206100_8014CB68` resets to (0, 0xA00, 0) when
    /// the actor teleports on `field_51E` reaching 0xFF.  `field_43E` is the
    /// yaw `func_actor_206100_8014AF74` reads back out of the root coordinate's
    /// third row at spawn, so this is the same rotation the actor spawns with.
    /* 0x43C */ s16  field_43C; // pitch
    /* 0x43E */ s16  field_43E; // yaw
    /* 0x440 */ s16  field_440; // roll
    /* 0x442 */ byte pad_442[0x1E];
    /// The light / colour matrix pair the two `TmdObject` slots point at,
    /// the same `field_1C` / `field_20` hand-off `actor_503500` makes.
    /* 0x460 */ MATRIX colorMtx;
    /* 0x480 */ MATRIX lightMtx;
    /* 0x4A0 */ byte   pad_4A0[0x20];
    /// Effect argument: the root coordinate's second part with the overlay's
    /// effect id and part index, the same coordinate / id / 3 trio
    /// `Actor503500Work::field_6E4` holds.
    /* 0x4C0 */ GpEffArg eff_4C0;
    /* 0x4C8 */ byte     pad_4C8[0x8];
    /// Walk target the two state dispatchers `func_actor_206100_8014D380` /
    /// `func_actor_206100_8014D6F4` steer the actor toward: after the sub-state
    /// handler they fold the root coordinate's heading onto the vector from the
    /// coordinate to this XZ pair and hand the actor to
    /// `func_actor_206100_8014ED3C`, which walks it along that heading.  Both
    /// halves are read as `u16` (the loads are `lhu`).
    /* 0x4D0 */ u16  field_4D0;
    /* 0x4D2 */ u16  field_4D2;
    /* 0x4D4 */ u16  field_4D4;
    /* 0x4D6 */ byte pad_4D6[0x2];
    /// Euler-angle mirrors of the two inner parts `func_actor_206100_8014B0AC`
    /// rotates: each is latched from the matching coordinate's matrix with
    /// `Gp_MtxToEuler`, ramped toward zero, and turned back into the matrix by
    /// `RotMatrix`.  The pair sits at the two parts it belongs to -- `field_4D8`
    /// is the second part `field_4D0` steers, `field_4E0` the third.
    /* 0x4D8 */ SVECTOR field_4D8;
    /* 0x4E0 */ SVECTOR field_4E0;
    /* 0x4E8 */ byte    pad_4E8[0xC];
    /// Position ring `func_actor_206100_8014FAE4` seeds from
    /// `D_actor_206100_80158B68` and then walks one entry per frame with
    /// `field_548`.
    /* 0x4F4 */ Actor206100RingPos* field_4F4;
    /// Child task `func_actor_206100_8014CB68` spawns off
    /// `D_actor_206100_80158AF0` with `D_actor_206100_80158CCC` as its spawn arg
    /// when the actor teleports: the screen tint that fades the flash in over
    /// the transition.  The handle is kept so the parent can reach it, but
    /// nothing in this overlay reads it back.
    /* 0x4F8 */ Task* field_4F8;
    /* 0x4FC */ byte  pad_4FC[0x8];
    /// Damage cooldown `func_actor_206100_8014BAA8` reloads with
    /// `Gp_GetIdParam2` of whatever hit it and decrements once a frame: while it
    /// reads 0 the record's hit is applied, and the tail always clamps it back
    /// to 0 rather than letting it go negative.
    /* 0x504 */ s16  field_504;
    /* 0x506 */ byte pad_506[0x2];
    /// Pair of halfwords the spawn state `func_actor_206100_8014C274` seeds
    /// with 0x1000 (1.0 in the 4.12 fixed point the overlay's scales use) on
    /// the same frame it builds the block.  Nothing in this overlay reads
    /// either one back.
    /* 0x508 */ s16              field_508;
    /* 0x50A */ s16              field_50A;
    /* 0x50C */ s16              field_50C; // animation request kind
    /* 0x50E */ s16              field_50E; // clip the request plays, latched from field_510
    /* 0x510 */ s16              field_510; // animation clip id
    /* 0x512 */ s16              field_512;
    /* 0x514 */ Actor206100Flags flags_514;
    /// Second half of the per-frame counter pair the state dispatcher
    /// `func_actor_206100_8014DA28` and the spawn state `func_actor_206100_8014C458`
    /// both bump: the two advance together, ahead of the sub-state handler.
    /* 0x518 */ u16 field_518;
    /* 0x51A */ s16 field_51A; // animation step scale
                               /// Yaw to the walk target, latched with the distance below by
                               /// `func_actor_206100_8014B698` from `ratan2` of the player delta it
                               /// normalises.
    /* 0x51C */ s16 field_51C;
    /* 0x51E */ u16 field_51E; // per-state frame counter
    /* 0x520 */ s16 field_520; // state index
    /* 0x522 */ u16 field_522; // sub-state index
    /* 0x524 */ s16 field_524;
    /* 0x526 */ u16 field_526;
    /// XZ distance to the walk target the yaw above was taken from, the
    /// shorter of the two `Gp_ActorSlots` distances.
    /* 0x528 */ s16 field_528;
    /// Hit flag `func_actor_206100_8014BAA8` raises on the frame it applies a
    /// record's damage, which also stops its walk, and clears again for the
    /// record that says not to react.  `func_actor_206100_8014CFF4` gates its
    /// whole body on it: the sub-state request is only honoured while this
    /// reads 1.
    /* 0x52A */ s16 field_52A;
    /// The pending sub-state request `func_actor_206100_8014CFF4` consumes and
    /// clears.  1 and 2 only run the 0x135 flinch and the 0x3A0 recovery
    /// through `func_actor_206100_8014EB48`; 3 and 4 also sound
    /// `SndEvt_EnqueueType7(0x551E0002, 1)` and move the actor to state 8 and 7
    /// at sub-state 0.  `func_actor_206100_8014BAA8` is the writer: it picks 1
    /// or 2 from the damage it just applied, and 2, 4, 0 or 1 from the record's
    /// own id parameter.
    /* 0x52C */ s16 field_52C;
    /// Halfword the sub-state handler `func_actor_206100_8014D14C` arms with
    /// 0x18 on the first frame of its sub-state.  Nothing in this overlay reads
    /// it back -- the field appears as a bare `sh` -- so its type is free.
    /* 0x52E */ s16  field_52E;
    /* 0x530 */ byte pad_530[0x4];
    /// Countdown decremented by the running state each frame while nonzero.
    /* 0x534 */ s16  field_534;
    /* 0x536 */ u16  field_536; // seeded from D_80181A48 when the block is built
    /* 0x538 */ byte pad_538[0x2];
    /// Retract ramp `func_actor_206100_8014B0AC` runs while the effect is
    /// retiring: 0 is the resting sub-state, 1 folds the two `field_4D8` /
    /// `field_4E0` angles back toward zero and 2 blows the parts out toward the
    /// restored size.  The sub-state clears it back to 0 when it finishes.
    /* 0x53A */ s16 field_53A;
    /// 4.12 fixed-point scale the same retract ramps: 0x1000 (1.0) while the
    /// actor is whole, stepped toward the 0x2AA the retired parts shrink to and
    /// back up toward 0xF80, which is where the effect hands the actor to
    /// `func_actor_206100_8014E228`.  `field_4D8` / `field_4E0` converge on
    /// zero exactly when this lands on 0x2AA.
    /* 0x53C */ s16 field_53C;
    /// Halfword the spawn state `func_actor_206100_8014C274` seeds with 0x1EAA
    /// and both `func_actor_206100_8014C458` and `func_actor_206100_8014DA28`
    /// read back with a signed load.
    /* 0x53E */ s16 field_53E;
    /// Yaw offset `func_actor_206100_8014EB60` folds into the part-5 rotation
    /// and the walk state `func_actor_206100_8014EC54` ramps to zero.
    /* 0x540 */ s16 field_540;
    /// Clip phase the retarget tick `func_actor_206100_8014E0C0` decays toward
    /// `field_544` while it ramps into the requested clip.
    /* 0x542 */ s16  field_542;
    /* 0x544 */ s16  field_544; // id handed to func_actor_206100_8014EB48
    /* 0x546 */ byte pad_546[0x2];
    /// Ring index `func_actor_206100_8014FAE4` resets to 0 and then advances
    /// modulo 8 each time it consumes an entry.
    /* 0x548 */ u8   field_548;
    /* 0x549 */ byte pad_549[0x2];
    /// Hit flag `func_actor_206100_8014BAA8` raises with `field_52A` on the same
    /// frame it applies a record's damage.  Nothing in this overlay reads it
    /// back, so its role is still open.
    /* 0x54B */ u8   field_54B;
    /* 0x54C */ byte pad_54C[0x1];
    /// Companion flag `func_actor_206100_8014FAE4` sets alongside the ring
    /// reset: 1 while the actor is whole, 0 once it starts retiring.  The state
    /// dispatcher `func_actor_206100_8014B0AC` switches on it, and both there
    /// and at its other two read sites the load is `lbu`, so it is a `u8`.
    /* 0x54D */ u8   field_54D;
    /* 0x54E */ byte pad_54E[0x1];
    /// Ring-step counter the shared companion tick `func_actor_206100_8014DEAC`
    /// advances each time the actor reaches the ring vertex `field_548` picks,
    /// and clears back to 0 once it passes 5 -- so the ring is consumed six
    /// vertices at a time.  It is read back *unsigned* (the loads are `lbu`),
    /// so it is a `u8`.
    /* 0x54F */ u8 field_54F;
    /// Roll-in-progress flag the same tick arms while `field_54F` reads 0 and
    /// clears once the roll `field_440` it ramps by 0x20 a frame lands on a
    /// 0x1000 boundary, one full turn of the actor's spin.
    /* 0x550 */ u8 field_550;
    /// Companion index the tick `func_actor_206100_8014DD3C` hands to the
    /// spawner `func_actor_206100_8014EE2C` and advances only when that spawn
    /// succeeds, so it counts the slots filled so far: the tick stops filling at
    /// 5, which is the six-entry `D_actor_206100_80155134` place list's live
    /// variants.  It is read back *unsigned* -- `lbu` for the test and for the
    /// increment -- so it is a `u8`, not the `s8` the neighbouring counters are.
    /// The increment reloads rather than reusing the test's load because the
    /// spawn call sits between the two.
    /* 0x551 */ u8 field_551;
    /// Companion count of the slots that retired, advanced the same way and
    /// compared against the same 5: the fifth release moves the actor to
    /// state 2.
    /* 0x552 */ u8 field_552;
    /// Stage id the actor was in before it teleported, saved by
    /// `func_actor_206100_8014CB68` from `Mc_SaveData::field_4` right before it
    /// writes 7 there.
    /* 0x553 */ u8 field_553;
    /// Clip-retarget state `func_actor_206100_8014EB48` arms to 1 and the
    /// retarget tick `func_actor_206100_8014E0C0` walks to 2 and clears.  It is
    /// read back *unsigned* -- the dispatch load and the increment are both
    /// `lbu` -- so this is a `u8`, not the `s8` the neighbours are.
    /* 0x554 */ u8 field_554;
    /// Flag the sub-state handler `func_actor_206100_8014D14C` sets -- a plain
    /// `sb` of 1 -- when the per-state counter `field_51E` reaches one of the
    /// six cue frames it shares with its heading fold: 0x54, 0x5B, 0x62, 0x69,
    /// 0x70 and 0x77.
    /* 0x555 */ u8 field_555;
    /// Phase of the `field_540` ramp the walk state `func_actor_206100_8014EC54`
    /// steps, the only reader of it: 0 decays the offset toward its zero fixed
    /// point, `func_actor_206100_8014EB48` arms 1 alongside the clip retarget it
    /// stores in `field_544`, 2 runs the offset down the `(-0x3000 - 16 *
    /// field_540) >> 6` convergence to the -0x300 it targets and hands over to
    /// 3 once the offset passes -0x2DF, and 3 walks it back up 0x1C a frame
    /// until it is non-negative, which clears the phase.  It is read back
    /// *unsigned* -- the dispatch load and the 2's increment are both `lbu` --
    /// so this is a `u8`, not the `s8` the neighbours are.
    /* 0x556 */ u8 field_556;
    /// Animation step the spawn state leaves at 4 (`func_actor_206100_8014F284`
    /// copies it into every slot's `field_1D`) and `func_actor_206100_8014BAA8`
    /// also reads as a part index into the root coordinate array, so the load
    /// there is `lbu` and the field is unsigned.
    /* 0x557 */ u8 field_557;
} Actor206100Work;
STATIC_ASSERT_SIZEOF(Actor206100Work, 0x558);

/// Work block of the beam task `func_actor_206100_8014C458` spawns off
/// `D_actor_206100_80158B0C` when `Actor206100Work::field_555` is set: it
/// `memCalloc(0x68, 0)`s one and parks it in the child's `Task::work`, the
/// same reuse `Actor206100Work` makes of the parent's slot.
///
/// `obj` is the kind-1 `GpObj` the spawn state `func_actor_206100_8014EEC0`
/// links into the collision list and `func_actor_206100_8014FBE4` unlinks
/// again on retirement, so the 0x8 before it is not the node's own header and
/// stays zero.  `rec` is the two-entry `GpRec18` table `obj.ctx.recs` points at.
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

/// Position and Euler rotation payload `func_actor_206100_8014CD08` sends to
/// slot 3 as message 0x3E9 when the actor's `field_51E` counter reaches 0x22,
/// teleporting it to (0x690, 0x1388, 0x898) with a 0xA00 yaw.  The same
/// `VECTOR` + `SVECTOR` pair `Actor403100MsgPos` and `Actor341900MsgPos` carry.
typedef struct Actor206100Msg3E9 {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor206100Msg3E9;
STATIC_ASSERT_SIZEOF(Actor206100Msg3E9, 0x18);

/// Spawn arg `func_actor_206100_8014CB68` hands the child task it starts off
/// `D_actor_206100_80158AF0` -- written to `Task::spawnArg2` by
/// `Task_SpawnFromDesc`, and parked by that child in
/// `D_actor_206100_80158BA8`.
///
/// It is a screen tint the child draws as a full-screen `TILE` packet, so the
/// child owns the ramp: `state` selects between fading in (0, counting `frame`
/// up to `span`) and fading out (1, counting down, then handing `field_4` the
/// next state and retiring).  The colour the packet carries is
/// `frame * scale / span` per channel, which is why this actor sets `span` to 1
/// -- its flash reaches full brightness on the second frame.  `blend` picks the
/// semi-transparent packet code when it is 0 and the opaque one otherwise.
///
/// `func_actor_206100_8014CB68` writes the two `1`s with `blend` between `r`
/// and `g`, which is not the declaration order and is load-bearing: the
/// constant and the `%hi` of the global's own address tie in `local-alloc`'s
/// `QTY_CMP_PRI` (`floor_log2 (n_refs) * n_refs * size / span`), and the
/// address only wins that tie while the constant's live range runs the whole
/// store run.  Cutting it short is what puts the constant in `$v1` and the
/// `%hi` in `$t0`; with `blend` written last the two swap and the tail no longer
/// schedules the same way.
typedef struct Actor206100FlashArg {
    /* 0x00 */ s16 span;  // frame count the ramp runs over
    /* 0x02 */ s16 scale; // channel value at the top of the ramp
    /* 0x04 */ s16 state; // ramp state the child walks
    /* 0x06 */ s16 frame; // ramp position, counted by the child
    /* 0x08 */ u8  blend; // 0 draws the semi-transparent packet
    /* 0x09 */ u8  r;
    /* 0x0A */ u8  g;
    /* 0x0B */ u8  b;
} Actor206100FlashArg;
STATIC_ASSERT_SIZEOF(Actor206100FlashArg, 0xC);

/// The tint `func_actor_206100_8014CB68` arms: pale cyan at half scale, opaque,
/// over a single frame.
extern Actor206100FlashArg D_actor_206100_80158CCC;

/// Child task `func_actor_206100_8014CB68` starts with the tint above as its
/// spawn arg.  Its callback is `func_actor_206100_80149ED0`.
extern TaskDesc D_actor_206100_80158AF0;

/// Spawn-state body: hands the freshly spawned enemy its model, its part
/// coordinate and its state, then starts the animation.
///
/// `task->spawnArg2` is the `GpEnemy` `func_actor_206100_8014EC14` spawned, so
/// this is the writer of nearly every field that spawn leaves unset.  The
/// `TmdObject` at `task->extra` gets the two `MATRIX` slots the overlay's
/// light / colour hand-off uses (`lightMtx` the 0x480 `work->lightMtx`,
/// `colorMtx` the 0x460 `work->colorMtx`) and `otOffset` 0xA -- the
/// ordering-table offset the draw pass links the model's primitives at.
/// `coord` is the model's root coordinate,
/// which the effect argument at `eff_4C0` reuses for part 1 (`field_8[1]`),
/// so `enemy->field_4` and the effect share one coordinate.
///
/// `hp` is read once into a local because `D_actor_206100_80155198.hpMax` is
/// the pair's max HP and both `field_40` and `field_42` take it -- reading the
/// global twice instead costs a register and shifts the whole function's
/// allocation (see `DECOMPILATION_LEARNINGS.md`, "A repeated global load ...").
/// The two `task->extra` walks after `Gp_LinkNode` are separate reloads in the
/// original, which is why `tmd` is not reused for `field_8[4]`.
void func_actor_206100_8014AF74(Task* task);

/// Builds the enemy's two collision objects.  Each is bound to a part
/// coordinate of the actor's `TmdObject` -- `obj_364` to `field_8[1]` with
/// `field_1C` 0x400, `obj_414` to `field_8[4]` with 0x200 -- and both point
/// their `field_C` at the shared `GpRec18` pair table zeroed at `rec_384`,
/// which is why there is a single `Gp_InitRec18Table` for the pair.  Each
/// block ends by clearing `flags` bit 0x8000 after its `Gp_LinkObj`, the same
/// tail shape `func_actor_403100_80132320` has (`|= 0x8000` there).
void func_actor_206100_8014F18C(Task* task);

/// Builds the child beam's collision state: links its `GpObj` and initializes
/// the coordinate the beam is drawn at. `task` is the child spawned by
/// `func_actor_206100_8014C458`, so its `Task::work` is the
/// `Actor206100ChildWork` above.
void func_actor_206100_8014EEC0(Task* task);

/// Spawns the beam's impact effect burst at `coord`. `arg1` is
/// `Actor206100ChildWork::field_60` (the `GpU16Pair` index): `(arg1 >> 1) % 6`
/// picks the spark frame `func_actor_206100_8014AB3C` plays, bit 0 gates the
/// puff and the low three bits the directional tail. `arg2` selects the burst
/// - 0 a lone spark, 1 the spark plus those two extras, 2 a four-shot ring -
/// and `arg3` is the biased `field_64` scale word, whose low 12 bits are the
/// effect parameter and bits 12..15 a variant index.
void func_actor_206100_8014A70C(GsCOORDINATE2* coord, u16 arg1, u16 arg2, u32 arg3);

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

/// State-1 body: ticks the actor's per-state frame counter and, once it
/// reaches 0x22, walks the actor out of the scene -- parks its model coordinate
/// at y 0x1B58, clears the counter, hands the area record to the light mode,
/// tells slot 3 (message 0x3E9) to place the player, and advances the sub-state
/// `field_522`.  Before that, the counter passing 3 fires the overlay's sound
/// event.  Every other frame ramps `field_526` toward 0x1D4C by a quarter of the
/// remaining distance and steps the actor `0x30` along its heading.
///
/// The second argument of `Gp_SetLightMode` is read through a cast rather than
/// as `task->spawnArg2` directly: the cast makes the load a *scalar* `MEM`,
/// which is what keeps its dependence on the fixed-address
/// `D_8007216C` store, so the store is scheduled ahead of it - the same
/// `MEM_IN_STRUCT_P` mechanism `dryfield_water_tank_4.c` and
/// `DECOMPILATION_LEARNINGS.md`, "Scalar memory references", describe.
void func_actor_206100_8014CD08(Task* task);

/// State handler 4 of `D_actor_206100_80149E94`, and the one that hands the
/// actor to `func_actor_206100_8014CD08` above.  It clears the fixed-address
/// `D_801818B8` flag, ticks the per-state counter `field_51E` and seeds
/// `D_actor_206100_80158CD0` with 2 on the first frame; frame 3 retires the
/// child task `func_actor_206100_8014CB68` spawned into `field_4F8`, and if the
/// kill left the counter alone, fires sound event 0x551E0003; frame 0xC splats
/// the 0x01202148 particle ring `func_actor_206100_8014D574` fires, at a radius
/// of 0x1000 and a constant y of -0x294; and frame 0x46 clears the model
/// coordinate's x and z, plays the weapon and 0x3F3 messages under light mode
/// 2, and moves the actor to state 1 at sub-state 0.
///
/// That last block reads `task->work` again instead of reusing the `work`
/// pointer, the same fresh load `set_state` makes, so the two stores stay a
/// block-local quantity.
void func_actor_206100_8014CE60(Task* task);

/// Sub-state 0 of `func_actor_206100_8014CFF4`'s table: clears the per-state
/// frame counter and the actor's animation request, then advances the
/// sub-state.
void func_actor_206100_8014F6F8(Task* task);

/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks `field_51E`,
/// arms `field_52E` with 0x18 on the first frame, eases `field_35C` toward
/// 0x4000 over frames 0x29..0x4D, fires the overlay's sound events at 0x54
/// (type 6, panned) and 0x77, sets `field_555` on the six cue frames of the
/// counter, and folds the heading to the actor's target --
/// `VectorNormalSS` then `ratan2` -- into `field_43E` in steps of 0xC.
void func_actor_206100_8014D14C(Task* task);

/// Sub-state 1 of `D_actor_206100_8014D6F4`'s table
/// (`D_actor_206100_80149EB4`, whose first entry `func_actor_206100_8014F7B4`
/// and third `func_actor_206100_8014F878` bracket it).  On the seventh frame of
/// the sub-state it splats 0x20 effect particles around the actor's root
/// coordinate -- the same `Gp_SpawnEff` id 0x01202148 ring
/// `func_actor_206100_8014D574` fires, at a radius of 0x1000 and a constant
/// y of -0x3E8 -- and from frame 0x1F it draws from `Gp_LcgState`: a one-in-four
/// `(state >> 16) & 3 == 0` hands state 2 (the teleport
/// `func_actor_206100_8014CB68`) to the actor at sub-state 0, and every other
/// draw restarts the counter and advances the sub-state.
void func_actor_206100_8014D8E8(Task* task);

/// Sub-state 1 of the state-2 dispatcher `func_actor_206100_8014DA28`'s
/// two-entry local table, which picks it with `funcs[(s16)field_520]` and is
/// entered from that dispatcher's `D_801153F4 == 0` arm -- entry 0 is the ring
/// stepper `func_actor_206100_8014FAE4`.  It maintains the actor's companions.
///
/// `field_51E` is held at 0x1E -- the frame `func_actor_206100_8014D574` fires
/// the explosion on -- by arming the global `Gp_StateF0` flag again through
/// `Gp_ArmStateF0` instead of advancing it, so the sub-state never leaves it;
/// every earlier frame just advances the counter.  The shared companion tick
/// `func_actor_206100_8014DEAC` then runs, and the two slots
/// `D_actor_206100_80158CBC` are walked by index, each handled on its own:
///
/// - an empty slot whose timer has run out spawns a companion with
///   `func_actor_206100_8014EE2C`, stores it, arms its health counter
///   `field_40` and advances `field_551` -- but only while `field_551` is still
///   below 5, because that index picks the variant's place record;
/// - an empty slot whose timer is still running counts it down by one;
/// - a filled slot whose enemy has lost its `field_40` counter is emptied and
///   armed with a 0xB4-frame cooldown, and the fifth such release moves the
///   actor to state 2 with the state and sub-state indices cleared.
///
/// The state change reads `task->work` again rather than reusing `work`, the
/// same fresh load `set_state` makes.
void func_actor_206100_8014DD3C(Task* task);
#endif
