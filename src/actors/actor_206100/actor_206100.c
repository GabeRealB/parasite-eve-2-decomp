#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/inline_c.h"
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actor.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

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
    OverlayMat         matrix;
    Actor206100GteView gte;
} Actor206100VecScratch;

STATIC_ASSERT_SIZEOF(Actor206100VecScratch, 0x20);

extern TaskDesc D_actor_206100_80158B0C;

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

/// The four handlers `func_actor_206100_8014E7D4` picks between as the effect
/// mode `Gp_StateF0.field_4` changes -- the retirement `func_actor_206100_8014FBE4`,
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

/// The wave `func_actor_206100_8014CB68` arms: pale cyan modulation with a
/// one-frame ramp.
extern OverlayWaveCtx D_actor_206100_80158CCC;

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
/// `Mc_SaveData.at4.loc.view` store, so the store is scheduled ahead of it - the same
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
/// entered from that dispatcher's `Gp_StateF0.field_4 == 0` arm -- entry 0 is the ring
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

/// Teardown state of the beam child, run until its countdown kills it.
void func_actor_206100_8014EFC8(Task* task);

/// Transforms `pos` from `coord`'s space up the parent chain into the view
/// coordinate's space.  Returns 1 with `pos` rewritten once the walk reaches
/// `gGfxViewCoord`, or 0 with `pos` untouched if the chain ends first.
s32 func_actor_206100_8014F030(GsCOORDINATE2* coord, SVECTOR* pos);

/// Last of the actor's five top-level states (`D_actor_206100_80149E5C`):
/// hands the task's `GpEnemy`, parked in `Task::spawnArg2`, back to
/// `Gp_DestroyEnemy`.
void func_actor_206100_8014F490(Task* task);

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// alone.
void func_actor_206100_8014F4B8(MATRIX* src, MATRIX* dst);

extern s32 D_80115738;

extern s16 D_801818B8;

extern u16 D_80181A48;

extern s16 D_actor_206100_80158CD0;

void func_actor_206100_8014B0AC(Task* task, u8 arg1);
void func_actor_206100_8014E0C0(Task* task);
void func_actor_206100_8014EB60(Task* task);
void func_actor_206100_8014EC54(Task* task);
void func_actor_206100_8014DEAC(Task* task);
void func_actor_206100_8014FAE4(Task* task);
void func_8004BFF8(s16 angle, MATRIX* matrix);

GpEnemy* func_actor_206100_8014EE2C(s32 arg0);

void func_actor_206100_8014D574(Task* task);
void func_actor_206100_8014EB48(Task* task, s16 arg1);
void func_actor_206100_8014ED3C(Task* task, s16 arg1);
void func_actor_206100_8014F284(Task* task);
void func_actor_206100_8014F2F0(Task* task);
s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
void func_actor_206100_8014F738(Task* task);
void func_actor_206100_8014F770(Task* task);
void func_actor_206100_8014F7B4(Task* task);
void func_actor_206100_8014F878(Task* task);
void func_actor_206100_8014E964(Task* task);
void func_actor_206100_8014FBE4(Task* task);
void func_actor_206100_8014FCD4(Task* task);
void func_actor_206100_8014FDE8(Task* task);
void func_actor_206100_8014AB3C(GsCOORDINATE2* coord, u16 arg1, u16 arg2, s32 arg3);
void func_actor_206100_8014E228(Task* task);

extern s16 D_800691CA;

/// Distortion amplitude of the screen wave: `frame * scale / span` of the
/// running spawn argument, recomputed every frame.
extern s32 D_actor_206100_80158B08;

/// The spawn argument of the running wave task, parked at spawn so the tick
/// reads the ramp through it.
extern OverlayWaveCtx* D_actor_206100_80158BA8;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern OverlayWaveRec6 D_actor_206100_80158BAC[11];
extern OverlayWaveRec6 D_actor_206100_80158BFC[30];

/// Task table `func_actor_206100_8014FDE8` spawns the shockwave from.
extern TaskDesc D_801818BC;

void func_actor_206100_8014DEAC(Task* task);
void func_actor_206100_8014F284(Task* task);
void func_actor_206100_8014F2F0(Task* task);
s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
void func_actor_206100_8014F8BC(Task* task);
void func_actor_206100_8014F970(Task* task);
void func_actor_206100_8014F9C4(Task* task);
void func_actor_206100_8014FA08(Task* task);

void func_actor_206100_8014B8B4(Task* task);

const TaskFuncTable3 D_actor_206100_80149E24 = {
    {
        func_actor_206100_8014EEC0,
        func_actor_206100_8014B8B4,
        func_actor_206100_8014EFC8,
    },
};

/// Screen-wave child `func_actor_206100_8014CB68` starts off
/// `D_actor_206100_80158AF0`.  State 0 seeds the column and row phases, parks
/// the spawn argument and clears its ramp; state 1 ramps `frame` up to `span`
/// (ramp state 0) or back down to zero (ramp state 1, then 2, which kills the
/// task and restores the display field), and redraws the frame buffer as a
/// 10 by 30 mesh of textured quads displaced by sine waves of that amplitude,
/// modulated by the tint when `blend` is set.
///
/// `Task::state` is read as a scalar through a cast: that keeps the load
/// behind the `D_800691CA` store, which a member read lets GCC hoist above it.
void func_actor_206100_80149ED0(Task* task)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    /* `Task::state` read as a scalar through a cast: that keeps the load
       behind the `D_800691CA` store, which a member read lets GCC hoist
       above it. */
    switch (*(s32*)((u8*)task + OFFSET_OF(Task, state))) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_actor_206100_80158BAC[i].phase  = 0;
                D_actor_206100_80158BAC[i].offset = (u32)rand() >> 3;
                D_actor_206100_80158BAC[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_actor_206100_80158BFC[i].phase  = 0;
                D_actor_206100_80158BFC[i].offset = (u32)rand() >> 3;
                D_actor_206100_80158BFC[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_actor_206100_80158B08        = 0;
            D_actor_206100_80158BA8        = task->spawnArg2;
            D_actor_206100_80158BA8->frame = 0;
            D_actor_206100_80158BA8->state = 0;
            Display_ClampField126(-8);
            task->state++;
            break;
        case 1:
            ctx = D_actor_206100_80158BA8;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(task);
                    Display_ClampField126(0);
                    break;
            }
            D_actor_206100_80158B08 = D_actor_206100_80158BA8->frame * D_actor_206100_80158BA8->scale / D_actor_206100_80158BA8->span;
            for (i = 0; i < 11; i++) {
                D_actor_206100_80158BAC[i].phase += D_actor_206100_80158BAC[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_actor_206100_80158BFC[i].phase += D_actor_206100_80158BFC[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_actor_206100_80158BA8->blend == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_actor_206100_80158BA8->r;
                        p->g0 = D_actor_206100_80158BA8->g;
                        p->b0 = D_actor_206100_80158BA8->b;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_actor_206100_80158B08 * (rsin((j << 9) + D_actor_206100_80158BAC[k].phase + D_actor_206100_80158BAC[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_actor_206100_80158B08 * (rsin((k << 10) + D_actor_206100_80158BFC[j].phase + D_actor_206100_80158BFC[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_actor_206100_80158B08 * (rsin((j << 9) + D_actor_206100_80158BAC[k + 1].phase + D_actor_206100_80158BAC[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_actor_206100_80158B08 * (rsin(((k + 1) << 10) + D_actor_206100_80158BFC[j].phase + D_actor_206100_80158BFC[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_actor_206100_80158B08 * (rsin(((j + 1) << 9) + D_actor_206100_80158BAC[k].phase + D_actor_206100_80158BAC[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_actor_206100_80158B08 * (rsin((k << 10) + D_actor_206100_80158BFC[j + 1].phase + D_actor_206100_80158BFC[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_actor_206100_80158B08 * (rsin(((j + 1) << 9) + D_actor_206100_80158BAC[k + 1].phase + D_actor_206100_80158BAC[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_actor_206100_80158B08 * (rsin(((k + 1) << 10) + D_actor_206100_80158BFC[j + 1].phase + D_actor_206100_80158BFC[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

void func_actor_206100_8014A70C(GsCOORDINATE2* coord, u16 arg1, u16 arg2, u32 arg3)
{
    SVECTOR vec;
    s32     i;
    u16     variant;
    u16     param;

    if (Gp_State1C->eventState != 0) {
        func_actor_206100_8014AB3C(coord, ((u32)arg1 >> 1) % 6, 0x400, 0);
        if (Gp_State1C->eventState >= 4) {
            return;
        }
    }

    variant = (arg3 >> 12) & 0xF;
    param   = arg3 & 0xFFF;

    switch (arg2) {
        case 0:
            Gp_SpawnEff(D_80115738, coord, 0x14001000 + param + variant, NULL);
            break;

        case 1:
            func_actor_206100_8014AB3C(coord, ((u32)arg1 >> 1) % 6, param, 0);
            if (!(arg1 & 1)) {
                Gp_SpawnEff(D_80115738, coord, 0x01000000 + param + variant, NULL);
            }
            if (!(arg1 & 7)) {
                SVECTOR* dir;

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;

        case 2:
            func_actor_206100_8014AB3C(coord, ((u32)arg1 >> 1) % 6, param, 0);
            Gp_SpawnEff(D_80115738, coord, 0x10001000 + param + variant, NULL);
            for (i = 0; i < 4; i++) {
                SVECTOR* dir;

                Gp_SpawnEff(D_80115738, coord, 0x02001000 + param + variant, NULL);

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;
    }
}
/// Links one frame of the rotating impact-spark billboard at `arg0`'s world
/// position, projected through `GsWSMATRIX` by a single `RTPS`; a negative
/// projection flag drops the quad.  `arg1` picks one of six 0x27-square frames
/// along row 0x38 of tpage 0x2A, `arg2` sizes the quad and `arg3` spins it: the
/// corners sit `arg2 * 0x27 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the spark shrinks with depth.
void func_actor_206100_8014AB3C(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    GpEffFlareScratch* blk;
    GpEffFlareScratch* copy;
    POLY_FT4*          prim;
    s32                ang;
    u16                frame;
    s32                u;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    blk                            = (GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch));
    copy                           = blk;
    blk->vec.vx                    = *(u16*)&arg0->workm.t[0];
    blk->vec.vy                    = *(u16*)&arg0->workm.t[1];
    blk->vec.vz                    = *(u16*)&arg0->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = blk;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->vec);
    gte_rtps();
    gte_stsxy(&((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->sx);
    gte_stflg(&((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(copy);
        ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x2A;
        prim->clut  = 0x4293;
        frame       = arg1 % 6;
        u           = frame * 0x28;
        setUV4(prim, u, 0x38, u + 0x27, 0x38, u, 0x5F, u + 0x27, 0x5F);
        ang      = (s16)arg3;
        blk->dx  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rcos(ang)) >> 12;
        prim->x0 = *(u16*)&blk->sx + *(u16*)&blk->dx;
        prim->x3 = *(u16*)&blk->sx - *(u16*)&blk->dx;
        prim->y0 = *(u16*)&blk->sy - *(u16*)&blk->dy;
        prim->y3 = *(u16*)&blk->sy + *(u16*)&blk->dy;
        ang      = ang + 0x400;
        blk->dx  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rsin(ang)) >> 12;
        blk->dy  = (((arg2 * 0x27) / ((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz) * rcos(ang)) >> 12;
        prim->x1 = *(u16*)&blk->sx + *(u16*)&blk->dx;
        prim->x2 = *(u16*)&blk->sx - *(u16*)&blk->dx;
        prim->y1 = *(u16*)&blk->sy - *(u16*)&blk->dy;
        prim->y2 = *(u16*)&blk->sy + *(u16*)&blk->dy;
        addPrim((u_long*)(((((u32)((GpEffFlareScratch*)(head - sizeof(GpEffFlareScratch)))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(GpEffFlareScratch));
}

void func_actor_206100_8014AF74(Task* task)
{
    Actor206100Work* work;
    TmdObject*       tmd;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              hp;

    tmd                      = task->extra;
    work                     = (Actor206100Work*)task->work;
    enemy                    = (GpEnemy*)task->spawnArg2;
    tmd->otOffset            = 0xA;
    tmd->lightMtx            = &work->lightMtx;
    tmd->flags               = 0;
    tmd->colorMtx            = &work->colorMtx;
    coord                    = tmd->coords;
    work->eff_4C0.coord      = &((TmdObject*)task->extra)->coords[1];
    work->eff_4C0.spawnArgLo = 0x580;
    work->eff_4C0.spawnArgHi = 3;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->bodyPos.vx        = 0;
    enemy->bodyPos.vy        = 0;
    enemy->bodyPos.vz        = 0;
    enemy->coord             = &((TmdObject*)task->extra)->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 1;
    enemy->recs               = work->rec_384;
    enemy->param              = &D_actor_206100_80155198;
    hp                        = D_actor_206100_80155198.hpMax;
    enemy->hpMax              = hp;
    enemy->hp                 = hp;
    coord->sub                = &gGfxViewCoord;
    func_800B3F84(&work->anim, D_actor_206100_80158B24, tmd, work->animAux, work->slots);
    func_actor_206100_8014F18C(task);
    work->field_43E = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->field_557 = 4;
}

/// Drives the retract effect the actor plays when `field_54D` clears: the three
/// parts `func_actor_206100_8014AF74` parents to the root coordinate are
/// rebuilt from scratch each frame, the second and third from an Euler angle
/// pair and the fourth from an angle plus a scale.
///
/// `field_54D` picks the direction of travel.  While it reads 1 the actor is
/// retiring: sub-state `field_53A` 0 latches the two angle pairs out of the
/// parts' matrices and falls straight into 1, which folds all six components
/// toward zero by a sixteenth and arms 2 once every one of them is within
/// 0x30; 2 then ramps `field_53C` down toward the 0x2AA the retired parts
/// shrink to.  Once `field_54D` reads 0 the else arm ramps `field_53C` back up
/// toward 0x1000, and while it is still under 0xF80 it rebuilds the parts the
/// same way; at 0xF80 the actor is whole again and the effect hands it to
/// `func_actor_206100_8014E228`.
///
/// The fourth part is the one that blows up rather than shrinks: `field_53C`
/// scales parts 2 and 3 down, so `mc` gets its reciprocal, `0x1000000 /
/// field_53C`.  Note also that the two arms clear `field_53A` rather than the
/// join after them: written once after the `if`, the else arm's tail becomes
/// instruction-for-instruction the case-2 tail and the jump optimiser merges
/// the two, costing the case-2 copy of the last fifty instructions.
void func_actor_206100_8014B0AC(Task* task, u8 arg1)
{
    VECTOR           scale;
    OverlayMat       rot;
    OverlayMat       ma;
    OverlayMat       mb;
    SVECTOR          euler;
    OverlayMat       mc;
    Actor206100Work* work;
    GsCOORDINATE2*   base;
    GsCOORDINATE2*   c2;
    GsCOORDINATE2*   c3;
    GsCOORDINATE2*   c4;
    s32              invScale;

    base = ((TmdObject*)task->extra)->coords;
    work = (Actor206100Work*)task->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];

    switch (work->field_54D) {
        case 1:
            switch (work->field_53A) {
                case 0:
                    base[0].flg = 0;
                    base[1].flg = 0;
                    base[2].flg = 0;
                    base[3].flg = 0;
                    base[4].flg = 0;
                    Gp_UpdateCoord(c4);
                    Gp_MtxToEuler(&c2->coord, &work->field_4D8);
                    Gp_MtxToEuler(&c3->coord, &work->field_4E0);
                    work->field_53A = 1;
                    work->field_53C = 0x1000;
                    /* fallthrough */
                case 1: {
                    GpMtxWords* ir;

                    work->field_4D8.vx = (u16)work->field_4D8.vx + ((s32) - (work->field_4D8.vx * 0x10) >> 7);
                    work->field_4D8.vy = (u16)work->field_4D8.vy + ((s32) - (work->field_4D8.vy * 0x10) >> 7);
                    work->field_4D8.vz = (u16)work->field_4D8.vz + ((s32) - (work->field_4D8.vz * 0x10) >> 7);
                    work->field_4E0.vx = (u16)work->field_4E0.vx + ((s32) - (work->field_4E0.vx * 0x10) >> 7);
                    work->field_4E0.vy = (u16)work->field_4E0.vy + ((s32) - (work->field_4E0.vy * 0x10) >> 7);
                    work->field_4E0.vz = (u16)work->field_4E0.vz + ((s32) - (work->field_4E0.vz * 0x10) >> 7);
                    ir                 = &rot.ident;
                    rot.ident.m00_m01  = 0x1000;
                    rot.ident.m02_m10  = 0;
                    ir->m11_m12        = 0x1000;
                    rot.ident.m20_m21  = 0;
                    ir->m22            = 0x1000;
                    RotMatrix(&work->field_4D8, &rot.mat);
                    func_actor_206100_8014F4B8(&rot.mat, &c2->coord);
                    rot.ident.m00_m01 = 0x1000;
                    rot.ident.m02_m10 = 0;
                    ir->m11_m12       = 0x1000;
                    rot.ident.m20_m21 = 0;
                    ir->m22           = 0x1000;
                    RotMatrix(&work->field_4E0, &rot.mat);
                    func_actor_206100_8014F4B8(&rot.mat, &c3->coord);
                    if ((abs(work->field_4D8.vx) < 0x30) && (abs(work->field_4D8.vy) < 0x30) && (abs(work->field_4D8.vz) < 0x30) &&
                        (abs(work->field_4E0.vx) < 0x30) && (abs(work->field_4E0.vy) < 0x30) && (abs(work->field_4E0.vz) < 0x30)) {
                        work->field_53A = 2;
                    }
                    c2->flg = 0;
                    c3->flg = 0;
                    c4->flg = 0;
                    Gp_UpdateCoord(c4);
                    break;
                }
                case 2: {
                    GpMtxWords* ia;
                    GpMtxWords* ib;
                    GpMtxWords* ic;
                    GpMtxWords* ir;

                    Gp_MtxToEuler(&c4->coord, &euler);
                    work->field_53C  = (u16)work->field_53C + ((0x2AA - work->field_53C) >> 3);
                    ia               = &ma.ident;
                    ma.ident.m00_m01 = 0x1000;
                    ma.ident.m02_m10 = 0;
                    ia->m11_m12      = 0x1000;
                    ma.ident.m20_m21 = 0;
                    ia->m22          = 0x1000;
                    scale.vx         = 0x1000;
                    scale.vy         = 0x1000;
                    scale.vz         = work->field_53C;
                    ScaleMatrix(&ma.mat, &scale);
                    func_actor_206100_8014F4B8(&ma.mat, &c2->coord);
                    ib               = &mb.ident;
                    mb.ident.m00_m01 = 0x1000;
                    mb.ident.m02_m10 = 0;
                    ib->m11_m12      = 0x1000;
                    mb.ident.m20_m21 = 0;
                    ib->m22          = 0x1000;
                    scale.vx         = 0x1000;
                    scale.vy         = 0x1000;
                    scale.vz         = 0x1000;
                    ScaleMatrix(&mb.mat, &scale);
                    func_actor_206100_8014F4B8(&mb.mat, &c3->coord);
                    ic               = &mc.ident;
                    mc.ident.m00_m01 = 0x1000;
                    mc.ident.m02_m10 = 0;
                    ic->m11_m12      = 0x1000;
                    mc.ident.m20_m21 = 0;
                    ic->m22          = 0x1000;
                    scale.vx         = 0x1000;
                    scale.vy         = 0x1000;
                    invScale         = 0x1000000 / work->field_53C;
                    scale.vz         = invScale;
                    ScaleMatrix(&mc.mat, &scale);
                    ir                = &rot.ident;
                    rot.ident.m00_m01 = 0x1000;
                    rot.ident.m02_m10 = 0;
                    ir->m11_m12       = 0x1000;
                    rot.ident.m20_m21 = 0;
                    ir->m22           = 0x1000;
                    RotMatrix(&euler, &rot.mat);
                    MulMatrix(&mc.mat, &rot.mat);
                    func_actor_206100_8014F4B8(&mc.mat, &c4->coord);
                    base[2].flg = 0;
                    base[3].flg = 0;
                    base[4].flg = 0;
                    Gp_UpdateCoord(c4);
                    break;
                }
            }
            break;
        case 0:
            base[0].flg = 0;
            base[1].flg = 0;
            base[2].flg = 0;
            base[3].flg = 0;
            base[4].flg = 0;
            Gp_UpdateCoord(c4);
            if (work->field_53C < 0xF80) {
                GpMtxWords* ia;
                GpMtxWords* ib;
                GpMtxWords* ic;
                GpMtxWords* ir;

                Gp_MtxToEuler(&c4->coord, &euler);
                work->field_53C  = (u16)work->field_53C + ((0x1000 - work->field_53C) >> 2);
                ia               = &ma.ident;
                ma.ident.m00_m01 = 0x1000;
                ma.ident.m02_m10 = 0;
                ia->m11_m12      = 0x1000;
                ma.ident.m20_m21 = 0;
                ia->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                scale.vz         = work->field_53C;
                ScaleMatrix(&ma.mat, &scale);
                func_actor_206100_8014F4B8(&ma.mat, &c2->coord);
                ib               = &mb.ident;
                mb.ident.m00_m01 = 0x1000;
                mb.ident.m02_m10 = 0;
                ib->m11_m12      = 0x1000;
                mb.ident.m20_m21 = 0;
                ib->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                scale.vz         = 0x1000;
                ScaleMatrix(&mb.mat, &scale);
                func_actor_206100_8014F4B8(&mb.mat, &c3->coord);
                ic               = &mc.ident;
                mc.ident.m00_m01 = 0x1000;
                mc.ident.m02_m10 = 0;
                ic->m11_m12      = 0x1000;
                mc.ident.m20_m21 = 0;
                ic->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                invScale         = 0x1000000 / work->field_53C;
                scale.vz         = invScale;
                ScaleMatrix(&mc.mat, &scale);
                ir                = &rot.ident;
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                ir->m11_m12       = 0x1000;
                rot.ident.m20_m21 = 0;
                ir->m22           = 0x1000;
                RotMatrix(&euler, &rot.mat);
                MulMatrix(&mc.mat, &rot.mat);
                func_actor_206100_8014F4B8(&mc.mat, &c4->coord);
                base[2].flg = 0;
                base[3].flg = 0;
                base[4].flg = 0;
                Gp_UpdateCoord(c4);
                work->field_53A = 0;
            } else {
                func_actor_206100_8014E228(task);
                work->field_53A = 0;
            }
            break;
    }
}
/// Latches the actor's position and picks the nearer of the two `Gp_ActorSlots`
/// actors as its walk target: it stores the standing post in `field_434` /
/// `field_438`, then measures the XZ distance to each slot from the root
/// coordinate, keeping the closer one's position in `field_4D0` / `field_4D4`
/// and its distance in `field_528`.
///
/// Two details are load-bearing.  `Gp_ActorSlots[0]` is read *before* the three
/// post stores: written after them the scheduler moves the whole `lui` / `lw`
/// group below the stores, which costs twelve bytes of schedule and shifts
/// every later branch target.  And the player delta `d0` is normalised and fed
/// to `ratan2` even when slot 1 was the closer one, so `field_51C` follows the
/// player's bearing rather than the target's.
void func_actor_206100_8014B698(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c0;
    GsCOORDINATE2*   c1;
    Task*            player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist0;
    s32              dist1;

    work            = (Actor206100Work*)task->work;
    coord           = ((TmdObject*)task->extra)->coords;
    player          = Gp_ActorSlots[0];
    work->field_434 = (u16)coord->coord.t[0];
    work->field_436 = (u16)coord->coord.t[1];
    work->field_438 = (u16)coord->coord.t[2];
    if (player != NULL) {
        c0    = ((TmdObject*)player->extra)->coords;
        d0.vx = (u16)c0->coord.t[0] - (u16)coord->coord.t[0];
        d0.vy = (u16)c0->coord.t[1] - (u16)coord->coord.t[1];
        d0.vz = (u16)c0->coord.t[2] - (u16)coord->coord.t[2];
        dist0 = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] == NULL) {
            work->field_4D0 = c0->coord.t[0];
            work->field_4D2 = c0->coord.t[1];
            work->field_4D4 = c0->coord.t[2];
            work->field_528 = dist0;
        } else {
            c1    = ((TmdObject*)Gp_ActorSlots[1]->extra)->coords;
            d1.vx = (u16)c1->coord.t[0] - (u16)coord->coord.t[0];
            d1.vy = (u16)c1->coord.t[1] - (u16)coord->coord.t[1];
            d1.vz = (u16)c1->coord.t[2] - (u16)coord->coord.t[2];
            dist1 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist1 < dist0) {
                work->field_4D0 = c1->coord.t[0];
                work->field_4D2 = c1->coord.t[1];
                work->field_4D4 = c1->coord.t[2];
                dist0           = dist1;
            } else {
                work->field_4D0 = c0->coord.t[0];
                work->field_4D2 = c0->coord.t[1];
                work->field_4D4 = c0->coord.t[2];
            }
            work->field_528 = dist0;
        }
        VectorNormalSS(&d0, &d0);
        work->field_51C = (ratan2(d0.vx, d0.vz) - (u16)work->field_43E) & 0xFFF;
    }
}
/// Tick handler of the beam child `func_actor_206100_8014EEC0` starts, the
/// same shape the marker `Actor00400_Fn02D48` has: while the effect mode
/// `Gp_StateF0.field_4` is 0 it advances the child's `field_5A` and folds `field_58` /
/// `field_5A` / `field_5C` into the root coordinate, raises `hit` when
/// either collision slot reports one of the three kinds 1/3/5 or when
/// `func_800E0C10`'s push-back says the beam is crowded, and retires the child
/// - clearing the object's draw flags, bumping the task state and switching
/// the effect kind to 2 - once `killCountdown` reaches 0x5B or the flag is up.
/// `field_64` is the scale the setup hands to `func_actor_206100_8014A70C`
/// biased by 0x10002000; it ramps 0x100 a frame to 0x600 and then holds.
void func_actor_206100_8014B8B4(Task* task)
{
    Actor206100ChildWork* child;
    GsCOORDINATE2*        coord;
    GpDeltaScratch        delta;
    s32                   mask;
    s32                   hit;
    s32                   mode;
    s32                   i;
    s32                   n;
    s32                   v;

    hit   = 0;
    child = (Actor206100ChildWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    mode  = 1;
    if (Gp_StateF0.field_4 == 0) {
        child->field_5A   += 2;
        *(u32*)&coord->flg = 0;
        coord->coord.t[0] += child->field_58;
        coord->coord.t[1] += child->field_5A;
        coord->coord.t[2] += child->field_5C;
        if (Gp_FindRec18(child->rec, 0) != 0) {
            for (i = 0; i < 2; i++) {
                switch (child->rec[i].key & 0xFFFF0000) {
                    case 0x10000:
                    case 0x30000:
                    case 0x50000:
                        hit = 1;
                        break;
                }
            }
        }
        n = func_800E0C10(child->rec, &delta, 2, &mask);
        if (n < 3) {
            if (n > 0) {
                if ((mask & 8) == 0) {
                    hit = 1;
                }
            }
        }
        Gp_ClearRec18Occupied(child->rec);
        if ((++task->killCountdown >= 0x5B) || (hit != 0)) {
            task->killCountdown = 0;
            child->obj.flags   &= 0x3FFF;
            mode                = 2;
            task->state        += 1;
        }
        v = child->field_64;
        if (v < 0x600) {
            child->field_64 = v + 0x100;
        } else {
            child->field_64 = 0x600;
        }
        func_actor_206100_8014A70C(coord, child->field_60, mode, child->field_64 + 0x10002000);
    }
}
/// Damage / knock-back tick: walks the six contact records of the actor's
/// `rec_384` table and turns the first occupied one into a hit.  `field_504` is
/// the cooldown that gates it -- `Gp_GetIdParam2` of the record arms it, and
/// `hit` / `heavy` are the two sub-state requests it leaves in `field_52C`, the
/// light flinch (1) and the heavy recovery (2) that the consumer `take_request`
/// maps to the 0x135 and 0x3A0 clips.  Both are initialised before the loop
/// rather than written as literals at each site: the arms that write
/// `field_52C` land in different basic blocks, and a literal in each of them is
/// reloaded per block, so only a value that is live from the entry block keeps
/// one register for all of them.  `hit` doubles as the value of the two hit
/// flags `field_52A` / `field_54B`, which the actor raises on the frame it
/// takes the hit and which stop the walk.
///
/// While the cooldown reads 0 the record's packed id is rolled through
/// `Gp_ComputeDamage` and `Gp_RollEnemyChance` -- a successful roll scales the
/// damage and selects the effect kind -- and the result is applied to the
/// enemy's `field_40` health counter through `func_800E2C78` and
/// `func_800DA6E8`.  The id's low parameter then picks one of the three flag
/// setters, one of the hit reaction sizes, or clears the hit flag again, and
/// the `0x7F`/`0x8000` pair on an id ending 0x1C forces the light reaction and
/// clears bit 0 of the object's draw flags.  The `else` arm is the same record
/// arriving with the cooldown still up: id parameter 0xD sounds
/// `func_800FDB18` on the root coordinate's second part alone.
///
/// The tail turns the object's own flags into requests the same way -- bit 0 of
/// `field_4C` clears and asks for the heavy reaction, bit 1 asks for the
/// consumer's sound-and-state pair, and the `Gp_TickObjFlag4` countdown applies
/// its knock-back and asks for the light one -- and every frame ends by
/// releasing the record table and counting the cooldown down, or clamping it to
/// 0 so it never goes negative.
void func_actor_206100_8014BAA8(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    s32              kind;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s32              tick;
    s32              i;
    s32              hit;
    s32              heavy;

    kind            = 0;
    hit             = 1;
    heavy           = 2;
    work            = (Actor206100Work*)task->work;
    enemy           = (GpEnemy*)task->spawnArg2;
    work->field_52A = 0;
    for (i = 0; i < 6; i++) {
        if ((work->rec_384[i].key & 0xFFFF0000) == 0x20000) {
            if (work->field_504 == 0) {
                work->field_52A = hit;
                work->field_54B = hit;
                dmg             = Gp_ComputeDamage(work->rec_384[i].key, work->field_528, 0, 0);
                amount          = dmg;
                work->field_504 = Gp_GetIdParam2(work->rec_384[i].key);
                if (Gp_RollEnemyChance(enemy, work->rec_384[i].key, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->rec_384[i].key) & 0xFFFF,
                              &((TmdObject*)task->extra)->coords[work->field_557], 0, &work->eff_4C0);
                if (amount >= 0xB4) {
                    work->field_52C = heavy;
                } else {
                    work->field_52C = hit;
                }
                switch (Gp_GetIdParam0(work->rec_384[i].key) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(enemy);
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->rec_384[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(enemy, work->rec_384[i].key, 0);
                        break;
                    case 4:
                        work->field_52C = 4;
                        break;
                    case 5:
                    case 6:
                        work->field_52C = heavy;
                        break;
                    case 7:
                        kind            = 2;
                        work->field_52C = 2;
                        amount         += amount;
                        break;
                    case 8:
                        work->field_52C = 0;
                        work->field_52A = 0;
                        break;
                    case 9:
                        work->field_52C = hit;
                        break;
                }
                if ((work->rec_384[i].key & 0x7F) == 0x1C && (work->rec_384[i].key & 0x8000) == 0) {
                    enemy->reactionFlags &= 0xFE;
                    work->field_52C       = hit;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->coords[work->field_557], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->coords[work->field_557], 2, 0);
                        break;
                }
                func_800E2C78(enemy, work->rec_384[i].key, amount, 0);
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if ((s16)enemy->hp < 0) {
                    enemy->hp = 0;
                }
            } else if ((Gp_GetIdParam1(work->rec_384[i].key) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &((TmdObject*)task->extra)->coords[1], 0, &work->eff_4C0);
            }
        }
        if (work->field_52A != 0) {
            break;
        }
    }
    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= 0xFE;
        work->field_52C       = 2;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        work->field_52C       = 3;
    }
    if (enemy->reactionFlags & 0xC) {
        tmp  = Gp_TickObjFlag4(enemy);
        tick = (s16)tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if ((s16)enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_52A = 1;
            work->field_52C = 1;
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }
    Gp_ClearRec18Occupied(work->rec_384);
    if (work->field_504 > 0) {
        work->field_504--;
    } else {
        work->field_504 = 0;
    }
}
/// Accumulate `arg0`'s parent chain into `arg1`: seed it with the node's own
/// rotation, then pre-multiply by each (renormalised) ancestor up to but not
/// including `arg2`, renormalising after every step. Returns whether the walk
/// stopped on `arg2` rather than running off the end of the chain.
static __inline__ s32 Actor206100_AccumulateRotation(GsCOORDINATE2* arg0, MATRIX* arg1, GsCOORDINATE2* arg2)
{
    MATRIX         normal;
    MATRIX         matrix;
    GsCOORDINATE2* coord;

    coord = arg0->sub;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        matrix = coord->coord;
        MatrixNormal(&matrix, &matrix);
        gte_SetRotMatrix(&matrix);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &normal);
        *arg1 = normal;
        coord = coord->sub;
    }
}

/// Undo the parent chain again, turning the world-space rotation in `arg1`
/// back into one relative to `arg0`'s parent: accumulate the chain *above* the
/// parent, transpose it (the 3x3 inverse of a rotation) and pre-multiply.
/// Nothing to do when the parent is already the view coordinate.
///
/// Returns `arg0` so the caller stores through the returned pointer; the copy
/// GCC emits where the exits merge is what gives the store base its own
/// pseudo. Three details here are matching requirements rather than style:
/// the early `return arg0;` on the end-of-chain exit (it is what lifts `arg0`
/// past the scratch pointers in global-alloc's priority order, so it keeps
/// `$s3`), and the `mp` / `lp` pointer variables, whose declarations must
/// precede `view` so their pseudos out-rank it when the two tie.
static __inline__ GsCOORDINATE2* Actor206100_LocalizeRotation(GsCOORDINATE2* arg0, MATRIX* arg1)
{
    MATRIX         matrix;
    MATRIX         local;
    MATRIX         normal;
    MATRIX         transposed;
    MATRIX*        mp;
    MATRIX*        lp;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* view;

    coord = arg0->sub;
    if (coord != &gGfxViewCoord) {
        mp     = &matrix;
        view   = &gGfxViewCoord;
        lp     = &local;
        matrix = coord->coord;
        while (1) {
            coord = coord->sub;
            if (coord == NULL) {
                return arg0;
            }
            if (coord == view) {
                __asm__ volatile(
                    "lhu $12, 0(%0);"
                    "lhu $13, 6(%0);"
                    "lhu $14, 12(%0);"
                    "sh $12, 0(%1);"
                    "sh $13, 2(%1);"
                    "sh $14, 4(%1);"
                    "lhu $12, 2(%0);"
                    "lhu $13, 8(%0);"
                    "lhu $14, 14(%0);"
                    "sh $12, 6(%1);"
                    "sh $13, 8(%1);"
                    "sh $14, 10(%1);"
                    "lhu $12, 4(%0);"
                    "lhu $13, 10(%0);"
                    "lhu $14, 16(%0);"
                    "sh $12, 12(%1);"
                    "sh $13, 14(%1);"
                    "sh $14, 16(%1);"
                    : : "r"(mp), "r"(&transposed) : "$12", "$13", "$14", "memory");
                gte_SetRotMatrix(&transposed);
                MulRotMatrix(arg1);
                break;
            }
            local = coord->coord;
            MatrixNormal(&local, &local);
            gte_SetRotMatrix(lp);
            MulRotMatrix(&matrix);
            MatrixNormal(&matrix, &normal);
            matrix = normal;
        }
    }
    return arg0;
}

/// Turns the joint `coord` by `yaw` about the world Y axis: accumulates its
/// rotation up to the view coordinate, turns it, expresses the result back in
/// the parent's frame and writes the 3x3 into the joint.  The working matrix
/// is one `MATRIX` taken off the scratchpad head for the duration.
void func_actor_206100_8014BEC4(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    Actor206100_AccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor206100_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Spawn state of `D_actor_206100_80149E94`: builds the actor's work block --
/// `memCalloc(0x558, 0)` parked straight in `Task::work`, the actor destroyed
/// if that fails -- empties both companion slots of `D_actor_206100_80158CBC`
/// through their index (the walked-pointer form gives the timer field an
/// induction variable of its own) and calls the setup `func_actor_206100_8014AF74`
/// with the block in place.
///
/// It then asks for the first clip: `field_50C` is written 2 -- the request kind
/// the animation player picks up -- with `field_510` as the clip and `field_51A`
/// the step scale, and the kind read back decides what to do with the clip phase
/// `field_512`.  Kind 1 either zeroes it, when the clip now playing is not the
/// requested one, or hands it to `func_actor_206100_8014F3C8` and re-requests
/// kind 3; kind 2 goes through `func_actor_206100_8014F284` and zeroes it; kind
/// 3 advances it by one.  Every path lands on slot 1 and steps animation slots
/// 1 through 0xE once.
///
/// The tail seeds the walk/HP scales (`field_508`, `field_50A`, `field_526` and
/// `field_53E`), zeroes the root coordinate's translation, takes the state-0
/// reference `Gp_IncStateF0Ref` and re-arms the actor in state 1 with the state
/// and sub-state indices cleared -- the two index pairs written through the two
/// fresh `Task::work` loads, the block-local store shape `func_actor_206100_8014CE60`
/// uses.
void func_actor_206100_8014C274(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* req;
    Actor206100Work* anim;
    Actor206100Work* state;
    Actor206100Work* tail;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              kind;
    s32              slot;
    s32              i;

    enemy      = task->spawnArg2;
    coord      = ((TmdObject*)task->extra)->coords;
    task->work = memCalloc(0x558, 0);
    work       = (Actor206100Work*)task->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    D_801818B8      = 1;
    work->field_536 = D_80181A48;
    for (i = 0; i < 2; i++) {
        D_actor_206100_80158CBC[i].enemy = NULL;
        D_actor_206100_80158CBC[i].timer = 0;
    }
    func_actor_206100_8014AF74(task);
    req            = (Actor206100Work*)task->work;
    req->field_51A = 0x10;
    req->field_510 = 3;
    req->field_50C = 2;
    anim           = (Actor206100Work*)task->work;
    kind           = anim->field_50C;
    if (kind == 1) {
        if (anim->field_50E != anim->field_510) {
            anim->field_512 = 0;
        } else {
            anim->field_512 = func_actor_206100_8014F3C8(task, anim->field_512);
        }
        func_actor_206100_8014F2F0(task);
        anim->field_50C = 3;
        goto block_13;
    }
    if (kind == 2) {
        func_actor_206100_8014F284(task);
        anim->field_50C = 3;
        anim->field_512 = 0;
        goto block_13;
    }
    slot = 1;
    if (kind == 3) {
        TOUCH_REG(slot);
        anim->field_512 = (s16)((u16)anim->field_512 + 1);
    block_13:
        slot = 1;
    }
    do {
        Gp_AnimTickIndex((GpAnimCtx*)anim, slot);
        slot = slot + 1;
    } while (slot < 0xF);
    work->field_508   = 0x1000;
    work->field_50A   = 0x1000;
    work->field_53E   = 0x1EAA;
    work->field_54D   = 1;
    coord->coord.t[0] = 0;
    work->field_526   = 0x2710;
    coord->coord.t[1] = 0x2710;
    coord->coord.t[2] = 0;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    state            = (Actor206100Work*)task->work;
    task->state      = 1;
    state->field_520 = 0;
    state->field_522 = 0;
    tail             = (Actor206100Work*)task->work;
    tail->field_520  = 0;
    tail->field_522  = 0;
}
void func_actor_206100_8014DA28(Task* task);
void func_actor_206100_8014C458(Task* task);
void func_actor_206100_8014E7D4(Task* task);

/// The actor's five top-level states, dispatched on `Task::state` by its task
/// callback `func_actor_206100_8014F428`: `func_actor_206100_8014C274` (which
/// builds the work block), `func_actor_206100_8014DA28`,
/// `func_actor_206100_8014C458`, `func_actor_206100_8014E7D4` and the exit
/// `func_actor_206100_8014F490`.
const TaskFuncTable5 D_actor_206100_80149E5C = {
    {
        func_actor_206100_8014C274,
        func_actor_206100_8014DA28,
        func_actor_206100_8014C458,
        func_actor_206100_8014E7D4,
        func_actor_206100_8014F490,
    },
};

void func_actor_206100_8014F524(Task* task);
void func_actor_206100_8014CFF4(Task* task);
void func_actor_206100_8014D380(Task* task);
void func_actor_206100_8014D6F4(Task* task);
void func_actor_206100_8014F59C(void);
void func_actor_206100_8014F5A4(void);
void func_actor_206100_8014F5AC(void);
void func_actor_206100_8014F5B4(Task* task);
void func_actor_206100_8014F608(Task* task);

const TaskFuncTable9 D_actor_206100_80149E70 = {
    {
        func_actor_206100_8014F524,
        func_actor_206100_8014CFF4,
        func_actor_206100_8014D380,
        func_actor_206100_8014D6F4,
        (TaskFunc)func_actor_206100_8014F59C,
        (TaskFunc)func_actor_206100_8014F5A4,
        (TaskFunc)func_actor_206100_8014F5AC,
        func_actor_206100_8014F5B4,
        func_actor_206100_8014F608,
    },
};

/// Push the model's second coordinate's world position onto `G_SCRATCH_HEAD`
/// and hand it to `Gp_UpdateActorColor`.  The body is `ActorsShared8013a2c0`'s,
/// inlined the way `actorUpdateModelColor` and `actorUpdateModelColor`
/// inline it -- and it has to stay an inlined copy.  Only while expanding an
/// inline body does cc1 keep the scratch head's absolute address folded into
/// the memory operand (`lw $a1,0x1F8003FC` / `sw $a1,0x1F8003FC`, which the
/// assembler expands to the `lui` + `%lo` pair); written out at the call site
/// the same statements materialise the address in a register instead, and the
/// three instructions that costs are the whole difference.
static __inline__ void Actor206100_UpdateColor(Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord                          = &((TmdObject*)task->extra)->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(task->spawnArg2, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Keep the model load and its long-lived saved copy as separate values.
static __inline__ TmdObject* Actor206100_ModelCopy(TmdObject* input)
{
    TmdObject* result;
    result = input;
    SOFT_TOUCH_REG_USE(result, input);
    return result;
}

/// Updates the active enemy, emits its beam, and gates lock-on by its height
/// after transforming the selected model part through the parent chain.
void func_actor_206100_8014C458(Task* task)
{
    Actor206100Work*          work   = (Actor206100Work*)task->work;
    TmdObject*                obj    = Actor206100_ModelCopy(*(TmdObject* volatile*)&task->extra);
    GpEnemy*                  enemy  = (GpEnemy*)task->spawnArg2;
    GsCOORDINATE2*            coord  = obj->coords;
    TaskFuncTable9            states = D_actor_206100_80149E70;
    Actor206100VecScratch     scratch;
    VECTOR                    scale;
    OverlayMat                scaling;
    Actor206100Work*          next;
    Actor206100Work*          dying;
    Actor206100Work*          sub;
    Actor206100Work*          pose;
    register Actor206100Work* last asm("s0");
    Actor206100Work*          anim;
    MATRIX*                   dest;
    GsCOORDINATE2*            scaled;
    GsCOORDINATE2*            destcoord;
    GsCOORDINATE2*            child;
    GsCOORDINATE2*            walk;
    GsCOORDINATE2*            root;
    register GpEnemy*         end asm("t2");
    Task*                     spawn;
    Actor206100ChildWork*     beam;
    Actor206100VecScratch*    mtx;
    MATRIX*                   mtx2;
    s32                       i;
    s32                       sound;
    s32                       pan;
    s32                       flag;
    s16                       state;
    SVECTOR*                  launch;
    register SVECTOR*         svp asm("t1");
    SVECTOR*                  out;
    VECTOR*                   vecp;
    register GsCOORDINATE2*   view asm("t7");
    s32*                      flagp;
    u32                       viewhi;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            break;
        case 0:
            work->flags_514.parts.field_516 = work->flags_514.parts.field_516 + 1;
            work->field_518                 = work->field_518 + 1;
            func_actor_206100_8014B698(task);
            states.funcs[(s16)work->field_520](task);
            sub = (Actor206100Work*)task->work;
            if ((s16)sub->field_52E != 0) {
                if (((u16)sub->field_52E & 7) == 0) {
                    sound = ((((GpEnemy*)task->spawnArg2)->placeKey >> 0xC) << 8) | 0x4004000B;
                    pan   = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)task->extra)->coords);
                    SndEvt_EnqueueType6(
                        sound, pan,
                        (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)task->extra)->coords));
                }
                if ((s16)sub->field_52E == 0x18 || (s16)sub->field_52E == 0x30) {
                    func_800FDB18(7, ((TmdObject*)task->extra)->coords + 1, NULL, &sub->eff_4C0);
                }
                sub->field_52E = (s16)((u16)sub->field_52E - 1);
            }
            next = (Actor206100Work*)task->work;
            if ((s16)next->field_534 != 0) {
                next->field_534 = (s16)((u16)next->field_534 - 1);
            }
            anim  = (Actor206100Work*)task->work;
            state = anim->field_50C;
            if (state == 1) {
                if (anim->field_50E != anim->field_510) {
                    anim->field_512 = 0;
                } else {
                    anim->field_512 = func_actor_206100_8014F3C8(task, anim->field_512);
                }
                func_actor_206100_8014F2F0(task);
                anim->field_50C = 3;
            } else if (state == 2) {
                func_actor_206100_8014F284(task);
                anim->field_50C = 3;
                anim->field_512 = 0;
            } else if (state == 3) {
                anim->field_512 = anim->field_512 + 1;
            }
            for (i = 1; i < 0xF; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)anim, i);
            }
            work->flags_514.parts.half = work->slots[1].flags;
            func_actor_206100_8014B0AC(task, work->field_54D);
            func_actor_206100_8014E0C0(task);
            func_actor_206100_8014EC54(task);
            func_actor_206100_8014EB60(task);
            destcoord                       = ((TmdObject*)task->extra)->coords;
            pose                            = (Actor206100Work*)task->work;
            mtx                             = &scratch;
            scratch.matrix.ident.m00_m01    = 0x1000;
            scratch.matrix.ident.m02_m10    = 0;
            *(s32*)&mtx->matrix.mat.m[1][1] = 0x1000;
            scratch.matrix.ident.m20_m21    = 0;
            mtx->matrix.mat.m[2][2]         = 0x1000;
            RotMatrixZ(pose->field_440, &mtx->matrix.mat);
            func_8004BFF8(pose->field_43E, &mtx->matrix.mat);
            dest                  = &destcoord->coord;
            dest->m[0][0]         = scratch.matrix.mat.m[0][0];
            dest->m[0][1]         = scratch.matrix.mat.m[0][1];
            dest->m[0][2]         = scratch.matrix.mat.m[0][2];
            dest->m[1][0]         = scratch.matrix.mat.m[1][0];
            dest->m[1][1]         = scratch.matrix.mat.m[1][1];
            dest->m[1][2]         = scratch.matrix.mat.m[1][2];
            dest->m[2][0]         = scratch.matrix.mat.m[2][0];
            dest->m[2][1]         = scratch.matrix.mat.m[2][1];
            dest->m[2][2]         = scratch.matrix.mat.m[2][2];
            destcoord->flg        = 0;
            scaled                = ((TmdObject*)task->extra)->coords;
            scale.vx              = work->field_53E;
            scale.vy              = scale.vx;
            scale.vz              = scale.vx;
            mtx2                  = &scaling.mat;
            scaling.ident.m00_m01 = 0x1000;
            scaling.ident.m02_m10 = 0;
            *(s32*)&mtx2->m[1][1] = 0x1000;
            scaling.ident.m20_m21 = 0;
            mtx2->m[2][2]         = 0x1000;
            ScaleMatrix(&scaling.mat, &scale);
            MulMatrix(&scaled->coord, &scaling.mat);
            func_actor_206100_8014BAA8(task);
            if (work->field_555 != 0) {
                root  = &((TmdObject*)task->extra)->coords[4];
                spawn = Task_SpawnFromTable(&D_actor_206100_80158B0C, 1, 0, 0);
                if (spawn != NULL) {
                    beam = memCalloc(0x68, 0);
                    if (beam == NULL) {
                        taskKill(spawn);
                    } else {
                        scratch.gte.vec.vx   = 0;
                        scratch.gte.vec.vy   = 0;
                        scratch.gte.vec.vz   = 0;
                        scratch.gte.m.alt.vx = 0;
                        scratch.gte.m.alt.vy = 0;
                        mtx->gte.m.alt.vz    = 0x5A;
                        func_actor_206100_8014F030(root, &scratch.gte.vec);
                        func_actor_206100_8014F030(root, &scratch.gte.m.alt);
                        spawn->work        = beam;
                        child              = ((TmdObject*)spawn->extra)->coords;
                        launch             = &scratch.gte.out;
                        scratch.gte.out.vx = 0;
                        scratch.gte.out.vy = 0;
                        launch->vz         = 0x15E;
                        func_actor_206100_8014F030(root, launch);
                        child->coord.t[0] = scratch.gte.out.vx;
                        child->coord.t[1] = scratch.gte.out.vy;
                        child->coord.t[2] = scratch.gte.out.vz;
                        beam->field_58    = scratch.gte.m.alt.vx - scratch.gte.vec.vx;
                        beam->field_5A    = scratch.gte.m.alt.vy - scratch.gte.vec.vy;
                        beam->field_5C    = scratch.gte.m.alt.vz - scratch.gte.vec.vz;
                    }
                }
                work->field_555 = 0;
            }
            if (enemy->hp <= 0) {
                dying            = (Actor206100Work*)task->work;
                task->state      = 3;
                dying->field_520 = 0;
                dying->field_522 = 0;
            }
            coord->coord.t[1] =
                coord->coord.t[1] + (((s16)work->field_526 - coord->coord.t[1]) >> 4);
            enemy->coord = &((TmdObject*)task->extra)->coords[work->field_557];
        case 1:
            Actor206100_UpdateColor(task);
            obj->flags &= 0xFF7F;
            break;
    }
    svp = &scratch.gte.vec;
    out = &scratch.gte.out;
    __asm__("lui %0, %%hi(gGfxViewCoord); addiu %1, %0, %%lo(gGfxViewCoord)"
            : "=&r"(viewhi), "=r"(view));
    vecp               = &scratch.gte.m.mac;
    flagp              = &flag;
    last               = (Actor206100Work*)task->work;
    walk               = &((TmdObject*)task->extra)->coords[work->field_557];
    end                = (GpEnemy*)task->spawnArg2;
    scratch.gte.out.vx = 0;
    scratch.gte.out.vy = 0;
    scratch.gte.out.vz = 0;
    scratch.gte.vec.vx = 0;
    scratch.gte.vec.vy = 0;
    scratch.gte.vec.vz = 0;
loop:
    if (walk->sub != NULL) {
        if (walk != view) {
            gte_SetTransMatrix(&walk->coord);
            gte_SetRotMatrix(&walk->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            scratch.gte.vec.vx = (u16)scratch.gte.m.mac.vx;
            scratch.gte.vec.vy = (u16)scratch.gte.m.mac.vy;
            scratch.gte.vec.vz = (u16)scratch.gte.m.mac.vz;
            walk               = walk->sub;
            goto loop;
        }
        out->vx = scratch.gte.vec.vx;
        out->vy = scratch.gte.vec.vy;
        out->vz = scratch.gte.vec.vz;
    }
    if ((s16)last->field_536 + 0x190 < scratch.gte.out.vy) {
        end->node.state.b.flags = 1;
    } else {
        end->node.state.b.flags = 0;
    }
}

/// Teleport: ramps the white-out `func_actor_206100_8014DEAC` fades with, and
/// once it is fully up, hands slot 3 the actor's new position and re-arms the
/// actor on the far side, spawning the screen tint `D_actor_206100_80158CCC`
/// describes as it goes.
///
/// The tint's `blend` is written between `r` and `g`, not in declaration
/// order, and that is load-bearing: the constant 1 and the `%hi` of the
/// global's own address tie in `local-alloc`'s `QTY_CMP_PRI`
/// (`floor_log2 (n_refs) * n_refs * size / span`), and the address only wins
/// that tie while the constant's live range runs the whole store run. Cutting
/// it short is what puts the constant in `$v1` and the `%hi` in `$t0`; with
/// `blend` written last the two swap and the tail no longer schedules the same
/// way.
void func_actor_206100_8014CB68(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* work2;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpXformArg       msg;

    work  = (Actor206100Work*)task->work;
    tmd   = task->extra;
    coord = tmd->coords;
    func_actor_206100_8014DEAC(task);
    work->field_51E = work->field_51E + 6;
    if ((s16)work->field_51E >= 0x100) {
        work->field_51E = 0xFF;
    }
    Fade_DrawOverlay((u8)work->field_51E, (u8)work->field_51E, (u8)work->field_51E, 2);
    if ((s16)work->field_51E == 0xFF) {
        work->obj_364.flags |= 0x8000;
        work->obj_414.flags |= 0x8000;
        msg.pos.vx           = 0x690;
        msg.pos.vy           = 0x1388;
        msg.pos.vz           = 0x898;
        msg.rot.vx           = 0;
        msg.rot.vy           = 0x200;
        msg.rot.vz           = 0;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&msg, 0);
        work2                         = (Actor206100Work*)task->work;
        work2->field_51A              = 0x10;
        work2->field_510              = 3;
        work2->field_50C              = 2;
        coord->coord.t[1]             = 0xDAC;
        work->field_526               = 0xDAC;
        coord->coord.t[0]             = 0x157C;
        coord->coord.t[2]             = 0x157C;
        work->field_43C               = 0;
        work->field_43E               = 0xA00;
        work->field_440               = 0;
        work->field_553               = Mc_SaveData.at4.loc.view;
        Mc_SaveData.at4.loc.view      = 7;
        work->field_51E               = 0;
        work->field_54D               = 0;
        work->field_522               = work->field_522 + 1;
        D_actor_206100_80158CCC.span  = 1;
        D_actor_206100_80158CCC.scale = 0x60;
        D_actor_206100_80158CCC.r     = 0x40;
        D_actor_206100_80158CCC.blend = 1;
        D_actor_206100_80158CCC.g     = 0x80;
        D_actor_206100_80158CCC.b     = 0x80;
        work->field_4F8               = Task_SpawnFromTable(&D_actor_206100_80158AF0, 0, 0, (s32)&D_actor_206100_80158CCC);
    }
}
void func_actor_206100_8014CD08(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* work2;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpXformArg       msg;

    work            = (Actor206100Work*)task->work;
    tmd             = task->extra;
    coord           = tmd->coords;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 3) {
        SndEvt_EnqueueType6(0x551E0004, 0, 0);
    }
    if ((s16)work->field_51E == 0x22) {
        coord->coord.t[0]        = 0;
        coord->coord.t[2]        = 0;
        work->field_526          = 0x1388;
        work->field_51E          = 0U;
        coord->coord.t[1]        = 0x1B58;
        work->field_43E          = 0;
        Mc_SaveData.at4.loc.view = 6;
        Gp_SetLightMode(task->spawnArg2, 0);
        Gp_MsgPlayer3F3(0);
        msg.pos.vx = 0x690;
        msg.pos.vy = 0x1388;
        msg.pos.vz = 0x898;
        msg.rot.vx = 0;
        msg.rot.vy = 0xA00;
        msg.rot.vz = 0;
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&msg, 0);
        work2            = (Actor206100Work*)task->work;
        work2->field_51A = 0x10;
        work2->field_510 = 1;
        work2->field_50C = 2;
        work->field_522  = work->field_522 + 1;
        return;
    }
    work->field_526 = (u16)(work->field_526 + ((s32)(0x1D4C - (s16)work->field_526) >> 2));
    func_actor_206100_8014EA8C(task, 0x30, work->field_43E);
}
/// State handler 4 of `D_actor_206100_80149E94`: clears the fixed-address
/// `D_801818B8` flag, ticks the per-state counter `field_51E` and seeds
/// `D_actor_206100_80158CD0` with 2 on its first frame.
///
/// Frame 3 retires the child task `func_actor_206100_8014CB68` spawned into
/// `field_4F8` and, if the kill left the counter where it was, fires the
/// overlay's sound event 0x551E0003.  Frame 0xC splats the 0x01202148 particle
/// ring -- the same one `func_actor_206100_8014D574` fires, at a radius of
/// 0x1000 and a constant y of -0x294 -- and frame 0x46 hands the actor to state
/// 1 (`func_actor_206100_8014CD08`): it clears the model coordinate's x and z,
/// plays the weapon and 0x3F3 messages under light mode 2, and zeroes the
/// sub-state index along with the new state.
///
/// The frame-0x46 block reads `task->work` again rather than reusing `work`,
/// the fresh load that keeps the pair of stores a block-local quantity -- the
/// same reload `set_state` below makes.
void func_actor_206100_8014CE60(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   ring;
    SVECTOR          vec;
    s32              i;
    s16              y;
    s16              frame;

    work            = (Actor206100Work*)task->work;
    D_801818B8      = 0;
    coord           = ((TmdObject*)task->extra)->coords;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 1) {
        D_actor_206100_80158CD0 = 2;
    }
    frame = (s16)work->field_51E;
    if (frame == 3) {
        if (work->field_4F8 != NULL) {
            taskKill(work->field_4F8);
        }
        if ((s16)work->field_51E == frame) {
            SndEvt_EnqueueType6(0x551E0003, 0, 0);
        }
    }
    if ((s16)work->field_51E == 0xC) {
        y    = -0x294;
        i    = 0;
        ring = ((TmdObject*)task->extra)->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(D_80115738, ring, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if ((s16)work->field_51E == 0x46) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Mc_SaveData.at4.loc.view = 2;
        coord->coord.t[0]        = 0;
        coord->coord.t[2]        = 0;
        next                     = (Actor206100Work*)task->work;
        next->field_520          = 1;
        next->field_522          = 0;
    }
}
/// Clears `field_522` and hands `field_520` the new state, reloading the work
/// block through the task rather than taking the caller's pointer: the fresh
/// load is what makes `state` a block-local quantity, which is what lets
/// local-alloc hand it `$v0` (see `take_request` below).
static __inline__ void set_state(Task* task, s32 state)
{
    Actor206100Work* next = (Actor206100Work*)task->work;

    next->field_520 = state;
    next->field_522 = 0;
}

/// Consumes the pending sub-state request in `field_52C`, which is only
/// honoured while `field_52A` reads 1, and returns 1 when it did, so the caller
/// skips this frame's sub-state handler.  Requests 1 and 2 run an animation
/// through `func_actor_206100_8014EB48` (0x135, then the 0x3A0 recovery);
/// 3 and 4 sound `SndEvt_EnqueueType7` and move the actor to state 8 and 7 at
/// sub-state 0.  Every arm clears `field_52C`.
///
/// The arms that only run an animation `break` to the single `return 0` after
/// the switch: that leaves a `return 0` block between the last arm and the
/// join, which dbr later steals into the branch delay slots, while the arms
/// that set a state `return 1` straight past it.  The `s16` return is what
/// makes the flag a halfword value: promoting it in the caller is the
/// `addu $v0,$a0,$zero` at the join, where an `s32` return leaves the flag in
/// `$a0` and tests it there, one instruction short of the target.
static __inline__ s16 take_request(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    if (work->field_52A == 1) {
        switch (work->field_52C) {
            case 1:
                work->field_52C = 0;
                func_actor_206100_8014EB48(task, 0x135);
                break;
            case 2:
                work->field_52C = 0;
                func_actor_206100_8014EB48(task, 0x3A0);
                break;
            case 3:
                SndEvt_EnqueueType7(0x551E0002, 1);
                work->field_52C = 0;
                set_state(task, 8);
                return 1;
            case 4:
                SndEvt_EnqueueType7(0x551E0002, 1);
                work->field_52C = 0;
                func_actor_206100_8014EB48(task, 0x3A0);
                set_state(task, 7);
                return 1;
            default:
                work->field_52C = 0;
                break;
        }
    }
    return 0;
}

/// State handler: consumes a pending sub-state request and, when there was
/// none, runs the current sub-state handler.  The request is handled through
/// the inlined `take_request`, so a request that moved the actor to another
/// state skips this frame's handler entirely.
void func_actor_206100_8014CFF4(Task* task)
{
    Actor206100Work* sub                 = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F6F8,
        func_actor_206100_8014D14C,
    };

    if (take_request(task) == 0) {
        states[(s16)sub->field_522](task);
    }
}
/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks the per-state
/// counter `field_51E` and arms `field_52E` with 0x18 on the first frame, eases
/// `field_35C` toward 0x4000 by a quarter of the remaining distance over frames
/// 0x29..0x4D, fires the overlay's sound events -- 0x551E0002 panned through
/// `Gp_GetObjPan` / `gpGetObjDepth` at 0x54 and plain at 0x77 -- flags the six
/// cue frames, hands state 2 to the actor at sub-state 0 when its `flags_514`
/// say so, and folds the heading onto the vector from the root coordinate to
/// the walk target `field_4D0` / `field_4D4`, exactly as
/// `func_actor_206100_8014D380` does but with a step of 0xC and a deadband of
/// 0x18, before handing the actor to `func_actor_206100_8014ED3C` with step
/// 0x10.  The heading fold is that function's, instruction for instruction
/// once the constants are substituted -- including the `yaw` load after the
/// `jal` and the deadband as a variable rather than a literal.
///
/// Three details are load-bearing:
///
/// - the six cue frames are an `||` chain, not a `switch`: GCC 2.8.1 expands a
///   six-case switch over the 0x24-wide range 0x54..0x77 into a jump table
///   (`sltiu` + `jr` off a `.rodata` table, the shape the m2c seed produced),
///   where the chain stays the five `beq` the target has.  The 0x54 and 0x77
///   constants of the two sound blocks above are `CSE`'d into registers the
///   chain's own comparisons then reuse, which is why they live in callee-saved
///   `$s3` / `$s0` across the calls in between.
/// - the three reads of `task->work` are three separate variables.  A single
///   variable assigned in all three places is one pseudo with three
///   definitions, and `global_alloc` homes the whole of it in one callee-saved
///   register -- `$s0` for the flags test and the state change as well as the
///   tail, which is the register only the tail's load crosses calls for.
/// - the state change goes through the inlined `set_state`, the same reloading
///   helper `func_actor_206100_8014D8E8` calls.
void func_actor_206100_8014D14C(Task* task)
{
    Actor206100Work* sub = (Actor206100Work*)task->work;
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    u16              ease;
    s32              pan;
    s32              cond;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    sub->field_51E = sub->field_51E + 1;
    if ((s16)sub->field_51E == 1) {
        sub->field_52E = 0x18;
    }
    if ((u32)(sub->field_51E - 0x29) < 0x25U) {
        ease           = sub->field_35C;
        sub->field_35C = ease + ((s32)((0x4000 - (ease * 0x10)) << 0x10) >> 0x16);
    }
    if ((s16)sub->field_51E == 0x54) {
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(0x551E0002, pan,
                            (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    }
    if ((s16)sub->field_51E == 0x77) {
        SndEvt_EnqueueType7(0x551E0002, 1);
    }
    if ((s16)sub->field_51E == 0x54 || (s16)sub->field_51E == 0x5B || (s16)sub->field_51E == 0x62 ||
        (s16)sub->field_51E == 0x69 || (s16)sub->field_51E == 0x70 || (s16)sub->field_51E == 0x77) {
        sub->field_555 = 1;
    }
    next = (Actor206100Work*)task->work;
    if ((next->flags_514.half & 1) || (next->flags_514.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond != 0) {
        SndEvt_EnqueueType7(0x551E0002, 1);
        set_state(task, 2);
    }
    work       = (Actor206100Work*)task->work;
    coord      = ((TmdObject*)task->extra)->coords;
    coord->flg = 0;
    vec.vx     = sub->field_4D0 - (u16)coord->coord.t[0];
    vec.vy     = 0;
    vec.vz     = sub->field_4D4 - (u16)coord->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    limit = 0x18;
    angle = (u16)work->field_43E;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > limit) {
        work->field_43E = angle - 0xC;
    } else if (diff < -0x18) {
        work->field_43E = angle + 0xC;
    }
    func_actor_206100_8014ED3C(task, 0x10);
}
void func_actor_206100_8014F65C(Task* task);
void func_actor_206100_8014F69C(Task* task);

/// The five sub-state handlers `func_actor_206100_8014F524` picks between: it
/// copies the table onto its stack and calls `funcs[(s16)field_522]`.
const TaskFuncTable5 D_actor_206100_80149E94 = {
    {
        func_actor_206100_8014F65C,
        func_actor_206100_8014F69C,
        func_actor_206100_8014CB68,
        func_actor_206100_8014CD08,
        func_actor_206100_8014CE60,
    },
};

/// The state-0 dispatcher's sub-state table, a table in its own right rather
/// than the local array `func_actor_206100_8014CFF4` builds -- the dispatcher
/// copies it whole, which is why the copy is a three-word block move out of
/// `.rodata`.  `func_actor_206100_8014D6F4` has the same body over the sibling
/// table `D_actor_206100_80149EB4`.
const TaskFuncTable3 D_actor_206100_80149EA8 = {
    {
        func_actor_206100_8014F738,
        func_actor_206100_8014D574,
        func_actor_206100_8014F770,
    },
};

/// The sibling table, immediately after `D_actor_206100_80149EA8` in
/// `.rodata`: the three sub-state handlers `func_actor_206100_8014D6F4`
/// dispatches between, the first `func_actor_206100_8014F7B4` and the third
/// `func_actor_206100_8014F878` bracketing the ring of debris
/// `func_actor_206100_8014D8E8` throws.
const TaskFuncTable3 D_actor_206100_80149EB4 = {
    {
        func_actor_206100_8014F7B4,
        func_actor_206100_8014D8E8,
        func_actor_206100_8014F878,
    },
};

/// The last object in this unit's `.rodata`, one object after
/// `D_actor_206100_80149EB4` and flush against the unit's first code address:
/// the four handlers `func_actor_206100_8014E7D4` dispatches between.  The cast
/// is the second argument its call passes, which the four one-argument handlers
/// ignore -- see `Actor206100StateTable4`.
const Actor206100StateTable4 D_actor_206100_80149EC0 = {
    {
        (Actor206100StateFunc)func_actor_206100_8014FBE4,
        (Actor206100StateFunc)func_actor_206100_8014FCD4,
        (Actor206100StateFunc)func_actor_206100_8014E964,
        (Actor206100StateFunc)func_actor_206100_8014FDE8,
    },
};

/// State handler of `D_actor_206100_80149E94`: consumes a pending sub-state
/// request through the inlined `take_request`, and when there was none runs the
/// current sub-state handler from `D_actor_206100_80149EA8` and then steers the
/// actor along its heading.
///
/// The steering half is the same fold `ActorsShared80139c00` makes: clear the
/// root coordinate's `flg`, take the XZ vector from the coordinate to the walk
/// target `field_4D0` / `field_4D4`, normalise it, and turn `field_43E` toward
/// `ratan2` of it by 0x18 a frame while the heading is more than a deadband off.
/// Three details are load-bearing:
///
/// - `yaw` is read from the call before `angle` is loaded, so the angle load
///   sits after the `jal` and never conflicts with the call-clobbered `$a0`;
///   with the load written first it is pushed into a callee-saved register and
///   every later value moves up one.
/// - the deadband is a variable, not the literal 0x28 -- which is what makes
///   `diff`'s comparison a register-register `slt` against a `li`'d `$v1`, the
///   same `li` + `slt` shape `Actor00400_TurnToward`'s range parameter forces.
///   Written as a literal the test becomes `slti $a0,0x29` + `bnez` instead:
///   the literal is folded away into `!(diff < 0x29)`, where a register operand
///   reaches `gen_int_relational`'s `reverse_regs` arm and its constant is
///   `force_reg`'d.  Only the first test is affected; the second one, written
///   against the same literal, stays an `slti`.
/// - `field_43E` is read through the `u16` view, so the load is `lhu` and its
///   zero-extension folds into the load; the field's own `s16` view is the `lh`
///   `func_actor_206100_8014ED3C` makes.
///
/// It ends by handing the actor to `func_actor_206100_8014ED3C` with step 0x14,
/// the walk that retires the actor back to `field_434` / `field_438` once it has
/// travelled far enough.
void func_actor_206100_8014D380(Task* task)
{
    Actor206100Work* sub    = (Actor206100Work*)task->work;
    TaskFuncTable3   states = D_actor_206100_80149EA8;
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    if (take_request(task) == 0) {
        states.funcs[(s16)sub->field_522](task);
        work       = (Actor206100Work*)task->work;
        coord      = ((TmdObject*)task->extra)->coords;
        coord->flg = 0;
        vec.vx     = sub->field_4D0 - (u16)coord->coord.t[0];
        vec.vy     = 0;
        vec.vz     = sub->field_4D4 - (u16)coord->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x28;
        angle = (u16)work->field_43E;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->field_43E = angle - 0x18;
        } else if (diff < -0x28) {
            work->field_43E = angle + 0x18;
        }
        func_actor_206100_8014ED3C(task, 0x14);
    }
}
void func_actor_206100_8014D574(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              i;
    s16              y;

    work            = (Actor206100Work*)task->work;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E < 0x1E) {
        work->field_35C = work->field_35C + ((s32) - (work->field_35C << 0x14) >> 0x15);
        work->field_35E = work->field_35E + ((s32) - (work->field_35E << 0x14) >> 0x15);
    }
    y = -0x64;
    if ((s16)work->field_51E == 0x1E) {
        i     = 0;
        coord = ((TmdObject*)task->extra)->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
        sound = (((u16)((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x551E0006;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
        work->field_51E = 0;
        work->field_526 = 0x1E78;
        work->field_522 = work->field_522 + 1;
    }
}
/// State handler of the second table, `D_actor_206100_80149EB4`: consumes a
/// pending sub-state request through the inlined `take_request`, and when there
/// was none runs the current sub-state handler from that table and then steers
/// the actor along its heading, exactly as `func_actor_206100_8014D380` does
/// over `D_actor_206100_80149EA8`.
///
/// The body is that function's instruction for instruction -- the two are the
/// same source shape over different tables, so the prologue's `lui` / `addiu`
/// pair is the only thing that differs between them, and an edit to one belongs
/// in the other.  See `func_actor_206100_8014D380` for what the steering fold
/// does and for why the `yaw` load sits after the `jal` and the deadband is a
/// variable.
void func_actor_206100_8014D6F4(Task* task)
{
    Actor206100Work* sub    = (Actor206100Work*)task->work;
    TaskFuncTable3   states = D_actor_206100_80149EB4;
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    if (take_request(task) == 0) {
        states.funcs[(s16)sub->field_522](task);
        work       = (Actor206100Work*)task->work;
        coord      = ((TmdObject*)task->extra)->coords;
        coord->flg = 0;
        vec.vx     = sub->field_4D0 - (u16)coord->coord.t[0];
        vec.vy     = 0;
        vec.vz     = sub->field_4D4 - (u16)coord->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x28;
        angle = (u16)work->field_43E;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->field_43E = angle - 0x18;
        } else if (diff < -0x28) {
            work->field_43E = angle + 0x18;
        }
        func_actor_206100_8014ED3C(task, 0x14);
    }
}
/// Sub-state 1 of `func_actor_206100_8014D6F4`'s table: the ring of debris the
/// death throes throw off, and the draw that decides whether the actor
/// teleports out of them.
///
/// On the seventh frame of the sub-state it splats 0x20 effect particles
/// (`Gp_SpawnEff` id 0x01202148, the same pair `func_actor_206100_8014D574`
/// fires at frame 0x1E) around the actor's root coordinate -- `rsin` / `rcos`
/// of `i << 7` shifted down by 3, so a ring of radius 0x1000 in 0x20 steps,
/// held at a constant y of -0x3E8.  `y` is a local rather than a literal in
/// the store because the whole ring shares the height, the same local
/// `func_actor_206100_8014D574` hoists.
///
/// From frame 0x1F on it draws from `Gp_LcgState`: the one-in-four that lands
/// on `(state >> 16) & 3 == 0` hands state 2 to the teleport
/// `func_actor_206100_8014CB68` at sub-state 0 -- so the actor leaves the
/// scene it is exploding in -- and the rest restart the counter and advance
/// to the next sub-state.  The state change goes through the inlined
/// `set_state`, which reloads `task->work` instead of reusing `work`: that
/// fresh load is what keeps the pointer a block-local quantity, exactly as in
/// `take_request`.
void func_actor_206100_8014D8E8(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              i;
    s16              y;

    work            = (Actor206100Work*)task->work;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 7) {
        y     = -0x3E8;
        i     = 0;
        coord = ((TmdObject*)task->extra)->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if ((s16)work->field_51E >= 0x1F) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 0x10) & 3) == 0) {
            set_state(task, 2);
            return;
        }
        work->field_51E = 0;
        work->field_522 = work->field_522 + 1;
    }
}
/// State-2 tick: the `Gp_StateF0.field_4` effect mode 0 arm bumps the actor's two frame
/// counters and runs the handler `funcs[(s16)field_520]` picks out of a
/// two-entry local table, then drives the animation request and re-poses the
/// actor; mode 1 is that tail alone and mode 2 is the deferred-kill bit of the
/// model.  The table's entries are the ring stepper
/// `func_actor_206100_8014FAE4` and the companion tick
/// `func_actor_206100_8014DD3C`, which is the `field_520` index the spawn state
/// `func_actor_206100_8014C274` leaves at 0.
///
/// The table is two addresses materialised into `$v0`, not a block move out of
/// `.rodata` -- the head of the function's four `lui` / `addiu` / `sw` pairs
/// are the declaration initialiser.  They follow the two `Task` walks because
/// both of those are locals with initialisers as well, and a declaration's
/// initialiser is emitted where the declaration is.
///
/// From the request kind down the body is `func_actor_206100_8014E964`'s word
/// for word: the same `field_50C` re-arm / ramp / reset chain over a second
/// `task->work` load, and the same `for (i = 1; i < 0xF; i++)` slot tick whose
/// initialiser sits *after* the chain for the reason
/// `func_actor_206100_8014FCD4` documents.  The chain is the only reader of
/// `next`; everything else stays on `work`, which is why the two loads exist.
///
/// Both pose matrices write five words, and which of them land in a register is
/// load-bearing.  `matrix.ident.*` names the union's word view, so those three
/// stores stay frame-relative, while `*(s32*)&mtx->m[1][1]` and `mtx->m[2][2]`
/// reach the same words through the `mtx` pointer and so are `8($s0)` and
/// `0x10($s0)` off the local matrix's own address.  Written without `mtx` all
/// five are frame-relative and that address is never materialised -- the same
/// `rotation` / `mtx` split `func_actor_403100_801339EC` makes.
///
/// The nine stores onto the coordinate are the same split one level down:
/// through `dest` their address is a register plus a displacement, so the copy
/// is `4($s3)` followed by eight off `$v1`; naming `coord->coord.m[i][j]`
/// instead gives nine distinct sums and no register at all, and every store
/// comes out frame-relative.  `scale` is declared between the two matrices
/// because the frame slots are handed out in declaration order -- matrix /
/// scale / scaling is what puts them at 0x18, 0x38 and 0x48.
void func_actor_206100_8014DA28(Task* task)
{
    Actor206100Work* work               = (Actor206100Work*)task->work;
    TmdObject*       obj                = (TmdObject*)task->extra;
    void             (*funcs[2])(Task*) = {
        func_actor_206100_8014FAE4,
        func_actor_206100_8014DD3C,
    };
    Actor206100Work* next;
    Actor206100Work* sub;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   scaled;
    MATRIX*          mtx;
    MATRIX*          mtx2;
    MATRIX*          dest;
    OverlayMat       matrix;
    VECTOR           scale;
    OverlayMat       scaling;
    s32              i;
    s16              state;

    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->flags_514.parts.field_516 = work->flags_514.parts.field_516 + 1;
            work->field_518                 = work->field_518 + 1;
            funcs[(s16)work->field_520](task);
            next  = (Actor206100Work*)task->work;
            state = next->field_50C;
            if (state == 1) {
                if (next->field_50E != next->field_510) {
                    next->field_512 = 0;
                } else {
                    next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
                }
                func_actor_206100_8014F2F0(task);
                next->field_50C = 3;
            } else if (state == 2) {
                func_actor_206100_8014F284(task);
                next->field_50C = 3;
                next->field_512 = 0;
            } else if (state == 3) {
                next->field_512 = next->field_512 + 1;
            }
            for (i = 1; i < 0xF; i++) {
                Gp_AnimTickIndex(&next->anim, i);
            }
            work->flags_514.parts.half = work->slots[1].flags;
            func_actor_206100_8014B0AC(task, work->field_54D);
            coord                = ((TmdObject*)task->extra)->coords;
            sub                  = (Actor206100Work*)task->work;
            mtx                  = &matrix.mat;
            matrix.ident.m00_m01 = 0x1000;
            matrix.ident.m02_m10 = 0;
            *(s32*)&mtx->m[1][1] = 0x1000;
            matrix.ident.m20_m21 = 0;
            mtx->m[2][2]         = 0x1000;
            RotMatrixZ(sub->field_440, &matrix.mat);
            func_8004BFF8(sub->field_43E, &matrix.mat);
            dest                  = &coord->coord;
            dest->m[0][0]         = matrix.mat.m[0][0];
            dest->m[0][1]         = matrix.mat.m[0][1];
            dest->m[0][2]         = matrix.mat.m[0][2];
            dest->m[1][0]         = matrix.mat.m[1][0];
            dest->m[1][1]         = matrix.mat.m[1][1];
            dest->m[1][2]         = matrix.mat.m[1][2];
            dest->m[2][0]         = matrix.mat.m[2][0];
            dest->m[2][1]         = matrix.mat.m[2][1];
            dest->m[2][2]         = matrix.mat.m[2][2];
            coord->flg            = 0;
            scaled                = ((TmdObject*)task->extra)->coords;
            scale.vx              = work->field_53E;
            scale.vy              = scale.vx;
            scale.vz              = scale.vx;
            mtx2                  = &scaling.mat;
            scaling.ident.m00_m01 = 0x1000;
            scaling.ident.m02_m10 = 0;
            *(s32*)&mtx2->m[1][1] = 0x1000;
            scaling.ident.m20_m21 = 0;
            mtx2->m[2][2]         = 0x1000;
            ScaleMatrix(&scaling.mat, &scale);
            MulMatrix(&scaled->coord, &scaling.mat);
            /* fallthrough */
        case 1:
            Actor206100_UpdateColor(task);
            obj->flags &= 0xFF7F;
            return;
    }
}
/// Companion tick: holds the per-state counter at the explosion frame and then
/// fills and retires the actor's two companion slots (see the header for the
/// full walk -- each slot is handled on its own, and the fifth release moves
/// the actor to state 2).
///
/// Both slots are reached by *index* rather than through a walking pointer, and
/// that is what the target's register file depends on: a walked pointer makes
/// `timer`'s read and write two identical `DEST_ADDR` givs on the same biv,
/// which `combine_givs` merges into one that survives `strength_reduce`'s
/// "worth while" test -- so the second field is given an induction variable of
/// its own, `$s1` goes to it instead of to `work`, and the frame grows by a
/// slot.  Indexed, the two `timer` accesses are displacements off the address
/// register strength reduction builds for `enemy`, and neither is reduced.
/// See `DECOMPILATION_LEARNINGS.md`, "A walked pointer's second field becomes a
/// second induction variable".
///
/// The two loops carry their own counters for the same reason: one variable
/// used by both is a single pseudo whose live range spans both loops, so
/// local-alloc has to home it in a callee-saved register for the whole
/// function, where the target's second loop counts in `$a0`.
void func_actor_206100_8014DD3C(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GpEnemy*         enemy;
    s32              i;
    s32              j;
    u8               count;

    work = (Actor206100Work*)task->work;
    if ((s16)work->field_51E == 0x1E) {
        Gp_ArmStateF0(1);
    } else {
        work->field_51E = work->field_51E + 1;
    }
    func_actor_206100_8014DEAC(task);
    i = 0;
    do {
        if (work->field_551 < 5 && D_actor_206100_80158CBC[i].enemy == NULL) {
            if (D_actor_206100_80158CBC[i].timer == 0) {
                enemy = func_actor_206100_8014EE2C(work->field_551);
                if (enemy != NULL) {
                    D_actor_206100_80158CBC[i].enemy = enemy;
                    enemy->hp                        = 1;
                    work->field_551                  = work->field_551 + 1;
                }
            } else {
                D_actor_206100_80158CBC[i].timer = D_actor_206100_80158CBC[i].timer - 1;
            }
        }
        i++;
    } while (i < 2);
    j = 0;
    do {
        if (D_actor_206100_80158CBC[j].enemy != NULL &&
            D_actor_206100_80158CBC[j].enemy->hp <= 0) {
            D_actor_206100_80158CBC[j].enemy = NULL;
            D_actor_206100_80158CBC[j].timer = 0xB4;
            count                            = work->field_552 + 1;
            work->field_552                  = count;
            if (count >= 5) {
                next            = (Actor206100Work*)task->work;
                task->state     = 2;
                next->field_520 = 0;
                next->field_522 = 0;
            }
        }
        j++;
    } while (j < 2);
}
/// Companion tick every state runs: while the flag `field_550` is up it ramps
/// the roll `field_440` by 0x20 a frame and clears the flag once the ramp lands
/// on a 0x1000 boundary, and it walks the actor along the eight-vertex ring
/// `field_4F4` the index `field_548` points into.  Reaching the vertex (the
/// planar distance below 0x3E8) advances `field_548` modulo 8 and the ring-step
/// counter `field_54F`, which resets after its sixth step and re-arms the roll;
/// otherwise it steers the yaw `field_43E` toward the vertex by 0x2C a frame
/// and hands the actor to `func_actor_206100_8014EA8C` for a 0x40 step.
///
/// The three diffs are written into the `delta` `SVECTOR` although only `vx`
/// and `vz` are read back -- the distance is planar, so `vy` is dead.  That
/// dead member is load-bearing: as scalar locals the three stay in registers
/// and their stores disappear, and it is the store boundaries of the *stack*
/// form that the target's four recomputed ring addresses hang off.
///
/// The else arm reads the ring through the `ring` / `index` pair instead of a
/// pointer to the entry.  With both operands in registers the address is a
/// register `plus` that survives the two `vec` stores, so the ring index and
/// base are loaded once; a pointer to the entry leaves `lbu field_548` /
/// `lw field_4F4` inside the address expression, which any store invalidates,
/// and it also puts the base in the `addu`'s first operand.  See
/// `DECOMPILATION_LEARNINGS.md`, "Array index vs intermediate pointer for
/// `addu` operand order".
void func_actor_206100_8014DEAC(Task* task)
{
    Actor206100Work*    sub = (Actor206100Work*)task->work;
    Actor206100Work*    work;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      coord2;
    Actor206100RingPos* ring;
    u32                 index;
    SVECTOR             delta;
    SVECTOR             vec;
    u16                 roll;
    u8                  count;
    s32                 angle;
    s32                 yaw;
    s32                 diff;
    s32                 limit;

    coord = ((TmdObject*)task->extra)->coords;
    if (sub->field_54F == 0) {
        sub->field_550 = 1;
    }
    if (sub->field_550 == 1) {
        roll           = sub->field_440 + 0x20;
        sub->field_440 = roll;
        if ((roll & 0xFFF) == 0) {
            sub->field_550 = 0;
        }
    }
    delta.vx       = (u16)sub->field_4F4[sub->field_548].field_0 - (u16)coord->coord.t[0];
    delta.vy       = (u16)sub->field_4F4[sub->field_548].field_2 - (u16)coord->coord.t[1];
    delta.vz       = (u16)sub->field_4F4[sub->field_548].field_4 - (u16)coord->coord.t[2];
    sub->field_526 = (u16)sub->field_4F4[sub->field_548].field_2;
    if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) < 0x3E8) {
        sub->field_548 = (sub->field_548 + 1) & 7;
        count          = sub->field_54F + 1;
        sub->field_54F = count;
        if (count >= 6) {
            sub->field_54F = 0;
        }
    } else {
        ring        = sub->field_4F4;
        index       = sub->field_548;
        work        = (Actor206100Work*)task->work;
        coord2      = ((TmdObject*)task->extra)->coords;
        coord2->flg = 0;
        vec.vx      = (u16)ring[index].field_0 - (u16)coord2->coord.t[0];
        vec.vy      = 0;
        vec.vz      = (u16)ring[index].field_4 - (u16)coord2->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x100;
        angle = (u16)work->field_43E;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->field_43E = angle - 0x2C;
        } else if (diff < -0x100) {
            work->field_43E = angle + 0x2C;
        }
        func_actor_206100_8014EA8C(task, 0x40, sub->field_43E);
    }
}
/// Retarget tick: `func_actor_206100_8014EB48` arms the state to 1 with the
/// clip it wants in `field_544`, and this walks the three phases it takes to
/// get there -- fire the switch sound, ramp the phase `field_542` half the
/// remaining distance each frame and advance once it is within 0x20 of the
/// clip, then step the phase down by 0x10 a frame until it is back at zero,
/// where the phase snaps to 0 and the state to the idle below.
///
/// State 0 is that idle, and what a freshly spawned (zeroed) block sits in: the
/// leftover phase decays by an eighth of itself each frame.
///
/// The decay reads `field_542` twice and lands as an `lh` and an `lhu` pair,
/// with nothing in the C saying so: the multiplicand's sign is needed (it feeds
/// the shift) while the addend's high bits are dead, the sum going straight back
/// through `sh` into the same halfword.  The ramp in case 2, whose operands do
/// need full width, is where the `(u16)` casts are load-bearing.
void func_actor_206100_8014E0C0(Task* task)
{
    Actor206100Work* work;
    s32              sound;
    s32              pan;
    s16              phase;

    work = (Actor206100Work*)task->work;
    switch (work->field_554) {
        case 0:
            work->field_542 = work->field_542 + ((s32) - (work->field_542 * 0x10) >> 7);
            break;
        case 1:
            sound = (((u16)((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40040006;
            pan   = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
            work->field_554 = work->field_554 + 1;
            break;
        case 2:
            phase           = (u16)work->field_542 + ((s32)(((u16)work->field_544 - (u16)work->field_542) << 0x14) >> 0x15);
            work->field_542 = phase;
            if (phase < work->field_544 - 0x20) {
                break;
            }
            work->field_554 = work->field_554 + 1;
            break;
        case 3:
            phase           = (u16)work->field_542 - 0x10;
            work->field_542 = phase;
            if ((phase << 0x10) <= 0) {
                work->field_554 = 0;
                work->field_542 = 0;
            }
            break;
    }
}
void func_actor_206100_8014E228(Task* task)
{
    SVECTOR          ang;
    SVECTOR*         aim;
    SVECTOR          rot1;
    SVECTOR          rot2;
    MATRIX           t1;
    MATRIX           t2;
    MATRIX           t3;
    OverlayMat       ma;
    OverlayMat       mb;
    OverlayMat       mc;
    MATRIX           view;
    VECTOR           delta;
    VECTOR           local;
    GsCOORDINATE2*   c1;
    GsCOORDINATE2*   c3;
    GsCOORDINATE2*   c4;
    Actor206100Work* work;
    GsCOORDINATE2*   base;
    GsCOORDINATE2*   c2;
    GpMtxWords*      ia;
    GpMtxWords*      ib;
    GpMtxWords*      ic;
    MATRIX*          m2;
    MATRIX*          m3;
    MATRIX*          dest;
    s32              hx;
    s32              hy;
    s32              hz;
    s32              total;
    u16              yaw;
    u16              pitch;
    u32              pitchDiff;
    s16              limit;

    base = ((TmdObject*)task->extra)->coords;
    work = (Actor206100Work*)task->work;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &c4->workm, &view);
    delta.vx = (s16)work->field_4D0 - view.t[0];
    delta.vy = (s16)work->field_4D2 - (view.t[1] + 0x100);
    delta.vz = (s16)work->field_4D4 - view.t[2];
    ApplyTransposeMatrixLV(&base->coord, &delta, &local);
    aim             = &ang;
    aim->vx         = (ratan2(-local.vy, local.vz) << 20) >> 20;
    aim->vy         = (ratan2(local.vx, local.vz) << 20) >> 20;
    aim->vz         = 0;
    hx              = (s16)work->field_35C;
    hy              = (s16)work->field_35E;
    hz              = (s16)work->field_360;
    work->field_35C = hx;
    work->field_35E = hy;
    work->field_360 = hz;
    if ((u16)(ang.vy + 0x3FF) < 0x7FF) {
        if ((u32)(((s16)ang.vy - (s16)work->field_35E) + 0x20) >= 0x41) {
            work->field_35E = ((s16)work->field_35E < ang.vy) ? work->field_35E + 0x18
                                                              : work->field_35E - 0x18;
        }
        yaw = ang.vx;
        if ((u16)(yaw + 0x3FF) < 0x7FF) {
            pitchDiff = ((s16)yaw - (s16)work->field_35C) + 0x20;
            pitch     = work->field_35C;
            if (pitchDiff >= 0x41) {
                work->field_35C = pitch + ((s32)(((u16)yaw - pitch) << 20) >> 23);
            }
        }
    } else {
        work->field_35E = work->field_35E + ((s32) - (s16)(work->field_35E * 0x10) >> 8);
        work->field_35C = work->field_35C + ((s32) - (s16)(work->field_35C * 0x10) >> 8);
    }
    m2 = &c2->coord;
    ia = &ma.ident;
    ib = &mb.ident;
    ic = &mc.ident;

    ma.ident.m00_m01 = 0x1000;
    ma.ident.m02_m10 = 0;
    ia->m11_m12      = 0x1000;
    ma.ident.m20_m21 = 0;
    ia->m22          = 0x1000;
    mb.ident.m00_m01 = 0x1000;
    mb.ident.m02_m10 = 0;
    ib->m11_m12      = 0x1000;
    mb.ident.m20_m21 = 0;
    ib->m22          = 0x1000;
    mc.ident.m00_m01 = 0x1000;
    mc.ident.m02_m10 = 0;
    ic->m11_m12      = 0x1000;
    mc.ident.m20_m21 = 0;
    ic->m22          = 0x1000;

    Gp_MtxToEuler(m2, &rot1);
    m3 = &c3->coord;
    Gp_MtxToEuler(&c3->coord, &rot2);
    RotMatrixX(rot1.vx + (s16)((s16)(u16)work->field_542 / 3), &ma.mat);
    RotMatrixX(rot2.vx + (s16)((s16)(u16)work->field_542 / 3), &mb.mat);
    c2->coord.m[0][0] = ma.mat.m[0][0];
    m2->m[0][1]       = ma.mat.m[0][1];
    m2->m[0][2]       = ma.mat.m[0][2];
    m2->m[1][0]       = ma.mat.m[1][0];
    m2->m[1][1]       = ma.mat.m[1][1];
    m2->m[1][2]       = ma.mat.m[1][2];
    m2->m[2][0]       = ma.mat.m[2][0];
    m2->m[2][1]       = ma.mat.m[2][1];
    m2->m[2][2]       = ma.mat.m[2][2];
    c3->coord.m[0][0] = mb.mat.m[0][0];
    m3->m[0][1]       = mb.mat.m[0][1];
    m3->m[0][2]       = mb.mat.m[0][2];
    m3->m[1][0]       = mb.mat.m[1][0];
    m3->m[1][1]       = mb.mat.m[1][1];
    m3->m[1][2]       = mb.mat.m[1][2];
    m3->m[2][0]       = mb.mat.m[2][0];
    m3->m[2][1]       = mb.mat.m[2][1];
    m3->m[2][2]       = mb.mat.m[2][2];
    Gp_UpdateCoord(c1);
    Gp_UpdateCoord(c2);
    Gp_UpdateCoord(c3);
    func_actor_206100_8014BEC4(c2, (s16)(u16)work->field_35E / 3);
    func_actor_206100_8014BEC4(c3, (s16)(u16)work->field_35E / 3);

    mc.ident.m00_m01 = 0x1000;
    mc.ident.m02_m10 = 0;
    ic->m11_m12      = 0x1000;
    mc.ident.m20_m21 = 0;
    ic->m22          = 0x1000;

    total = (s16)work->field_35C + work->field_542;
    limit = total;
    if (limit >= 0x300) {
        limit = 0x300;
    } else if (limit < -0x200) {
        limit = -0x200;
    }
    RotMatrixX((s32)limit, &mc.mat);
    func_8004BFF8((s16)(u16)work->field_35E / 3, &mc.mat);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(&c2->coord, &t2);
    TransposeMatrix(&c3->coord, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc.mat);
    dest          = &c4->coord;
    dest->m[0][0] = t1.m[0][0];
    dest->m[0][1] = t1.m[0][1];
    dest->m[0][2] = t1.m[0][2];
    dest->m[1][0] = t1.m[1][0];
    dest->m[1][1] = t1.m[1][1];
    dest->m[1][2] = t1.m[1][2];
    dest->m[2][0] = t1.m[2][0];
    dest->m[2][1] = t1.m[2][1];
    dest->m[2][2] = t1.m[2][2];
    c4->flg       = 0;
    Gp_UpdateCoord(c4);
}

/// Effect-mode tick of the `field_520` state table `D_actor_206100_80149EC0`,
/// keyed on `Gp_StateF0.field_4`.  Mode 2 only sets the model's deferred-kill bit and
/// leaves; mode 0 runs the handler `field_520` selects, latches the animation
/// slot's flags into `flags_514` and eases the root coordinate -- x and z to a
/// sixteenth of their distance to zero, y the same fraction of the way to the
/// height `field_526` -- before falling into the shared tail; mode 1 is that
/// tail alone.
///
/// The table is copied onto the stack first, the same local jump table
/// `func_actor_206100_8014F524` builds, which is what the prologue's four-word
/// block move out of `.rodata` is.  The tail is `Actor206100_UpdateColor`; see
/// there for why it stays inline.
void func_actor_206100_8014E7D4(Task* task)
{
    Actor206100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor206100StateTable4 states;

    work   = (Actor206100Work*)task->work;
    obj    = (TmdObject*)task->extra;
    coord  = obj->coords;
    states = D_actor_206100_80149EC0;
    switch (Gp_StateF0.field_4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            states.funcs[(s16)work->field_520](task, &states);
            work->flags_514.half = work->slots[1].flags;
            coord->coord.t[0]    = coord->coord.t[0] + (-coord->coord.t[0] >> 4);
            coord->coord.t[2]    = coord->coord.t[2] + (-coord->coord.t[2] >> 4);
            coord->coord.t[1] =
                coord->coord.t[1] + (((s16)work->field_526 - coord->coord.t[1]) >> 4);
            /* fallthrough */
        case 1:
            Actor206100_UpdateColor(task);
            return;
    }
}
/// Teleport-state tick: the same animation request chain
/// `func_actor_206100_8014FCD4` runs -- re-arm, ramp or reset the clip phase
/// `field_512` and tick every slot -- but on its own frame counter, and with the
/// tail this actor needs instead of that one's: `field_51E` reaching 0x32
/// rewinds it and steps the state index `field_520`, the frame the sub-state
/// table walks to pick the next handler.
///
/// `coord` is the actor's root coordinate, cleared so `GsGetLw` rebuilds it --
/// the same dereference-store local `func_actor_206100_8014FDE8` binds.
///
/// The loop initialiser sits after the sub-state chain for the reason
/// `func_actor_206100_8014FCD4` documents: ahead of it the store that
/// materialises `i` shares a block with the case-3 increment, post-reload CSE
/// folds that increment's `+ 1` into `+ $s0`, and the phase is written with
/// `addu`.  Here the branch targets the initialiser instead.
void func_actor_206100_8014E964(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;
    s32              i;
    s16              state;

    work            = (Actor206100Work*)task->work;
    coord           = ((TmdObject*)task->extra)->coords;
    work->field_51E = work->field_51E + 1;
    next            = (Actor206100Work*)task->work;
    state           = next->field_50C;
    if (state == 1) {
        if (next->field_50E != next->field_510) {
            next->field_512 = 0;
        } else {
            next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
        }
        func_actor_206100_8014F2F0(task);
        next->field_50C = 3;
    } else if (state == 2) {
        func_actor_206100_8014F284(task);
        next->field_50C = 3;
        next->field_512 = 0;
    } else if (state == 3) {
        next->field_512 = next->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        Gp_AnimTickIndex(&next->anim, i);
    }
    coord->flg = 0;
    if ((s16)work->field_51E >= 0x32) {
        work->field_51E = 0;
        work->field_520 = work->field_520 + 1;
    }
}
void func_actor_206100_8014EA8C(Task* task, s16 arg1, s16 arg2)
{
    ((TmdObject*)task->extra)->coords->coord.t[0] += ((rsin(arg2) << 4) * arg1) >> 16;
    ((TmdObject*)task->extra)->coords->coord.t[2] += ((rcos(arg2) << 4) * arg1) >> 16;
    ((TmdObject*)task->extra)->coords->flg         = 0;
}

void func_actor_206100_8014EB48(Task* task, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->field_556 = 1;
    work->field_554 = 1;
    work->field_544 = arg1;
}

void func_actor_206100_8014EB60(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coords;
    SVECTOR          rot;
    OverlayMat       matrix;
    MATRIX*          dest;
    MATRIX*          mtx;

    work   = (Actor206100Work*)task->work;
    coords = ((TmdObject*)task->extra)->coords;
    dest   = &coords[5].coord;
    mtx    = &matrix.mat;

    matrix.ident.m00_m01 = 0x1000;
    matrix.ident.m02_m10 = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    matrix.ident.m20_m21 = 0;
    mtx->m[2][2]         = 0x1000;

    Gp_MtxToEuler(dest, &rot);
    rot.vx += work->field_540;
    RotMatrix(&rot, &matrix.mat);
    dest->m[0][0] = matrix.mat.m[0][0];
    dest->m[0][1] = matrix.mat.m[0][1];
    dest->m[0][2] = matrix.mat.m[0][2];
    dest->m[1][0] = matrix.mat.m[1][0];
    dest->m[1][1] = matrix.mat.m[1][1];
    dest->m[1][2] = matrix.mat.m[1][2];
    dest->m[2][0] = matrix.mat.m[2][0];
    dest->m[2][1] = matrix.mat.m[2][1];
    dest->m[2][2] = matrix.mat.m[2][2];
}

void func_actor_206100_8014EC54(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;
    s16              value;
    s16              angle;

    switch (work->field_556) {
        case 0:
            work->field_540 = (u16)work->field_540 + ((-(work->field_540 * 0x10)) >> 7);
            return;
        case 1:
            work->field_556 = 2;
            return;
        case 2:
            value           = (u16)work->field_540 + ((-0x3000 - work->field_540 * 0x10) >> 6);
            work->field_540 = value;
            if (value < -0x2DF) {
                work->field_556++;
                return;
            }
            return;
        case 3:
            angle           = (u16)work->field_540 + 0x1C;
            work->field_540 = angle;
            if (angle >= 0) {
                work->field_556 = 0;
            }
            break;
    }
}
void func_actor_206100_8014ED3C(Task* task, s16 arg1)
{
    Actor206100Work*        work;
    GsCOORDINATE2*          coord;
    Actor206100DistScratch* head;
    Actor206100DistScratch* scratch;

    head                                 = SCRATCH_HEAD(Actor206100DistScratch);
    scratch                              = head - 1;
    SCRATCH_HEAD(Actor206100DistScratch) = scratch;
    work                                 = (Actor206100Work*)task->work;
    coord                                = ((TmdObject*)task->extra)->coords;
    func_actor_206100_8014EA8C(task, arg1, work->field_43E);
    scratch->delta.vx = -(u16)coord->coord.t[0];
    scratch->delta.vz = -(u16)coord->coord.t[2];
    scratch->dist     = SquareRoot0(scratch->delta.vx * scratch->delta.vx +
                                    scratch->delta.vz * scratch->delta.vz);
    if (scratch->dist >= 0x191) {
        coord->coord.t[0] = work->field_434;
        coord->coord.t[2] = work->field_438;
    }
    SCRATCH_POP(Actor206100DistScratch);
}

extern TaskDesc D_80147E48;

GpEnemy* func_actor_206100_8014EE2C(s32 arg0)
{
    GpEnemy*   enemy;
    TmdObject* obj;

    enemy = Gp_SpawnEnemyFromTable(&D_80147E48, 0, 3, NULL);
    if (enemy != NULL) {
        enemy->placeKey = arg0 << 12;
        enemy->place    = &D_actor_206100_80155134[(s16)arg0];
        obj             = (TmdObject*)enemy->task->extra;
        obj->tpage      = 0;
        obj->clut       = 2;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
        return enemy;
    }
    return NULL;
}

void func_actor_206100_8014EEC0(Task* task)
{
    Actor206100ChildWork* child;
    GpRec18*              rec;
    GsCOORDINATE2*        coord;

    child               = (Actor206100ChildWork*)task->work;
    coord               = ((TmdObject*)task->extra)->coords;
    task->killCountdown = 0;
    child->field_64     = 0x100;
    child->field_60     = 0;
    coord->sub          = &gGfxViewCoord;
    coord->flg          = 0;
    child->obj.key      = Gp_PackPair(&D_actor_206100_80155194, 0);
    child->obj.coord    = ((TmdObject*)task->extra)->coords;
    rec                 = child->rec;
    child->obj.ctx.recs = rec;
    child->obj.pos.vx   = 0;
    child->obj.pos.vy   = 0;
    child->obj.pos.vz   = 0;
    child->obj.radius   = 0x140;
    child->obj.flags    = 1;
    Gp_LinkObj(3, &child->obj);
    Gp_InitRec18Table(rec, 2, 0);
    child->obj.flags |= 0xC000;
    Gp_UpdateCoord(coord);
    func_actor_206100_8014A70C(coord, (u16)child->field_60, 0, child->field_64 + 0x10002000);
    task->state++;
}

/// Teardown state of the beam child: each frame it clears the root
/// coordinate's `flg` and counts `killCountdown` up, and on the twelfth frame
/// unlinks the beam's collision object and kills the task.
void func_actor_206100_8014EFC8(Task* task)
{
    Actor206100ChildWork* child;
    TmdObject*            tmd;
    u16                   countdown;

    child               = (Actor206100ChildWork*)task->work;
    tmd                 = (TmdObject*)task->extra;
    tmd->coords->flg    = 0;
    countdown           = task->killCountdown + 1;
    task->killCountdown = countdown;
    if ((s16)countdown >= 0xC) {
        Gp_UnlinkObj(&child->obj);
        taskKill(task);
    }
}

s32 func_actor_206100_8014F030(GsCOORDINATE2* coord, SVECTOR* pos)
{
    SVECTOR        local;
    VECTOR         result;
    s32            flag;
    GsCOORDINATE2* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

/// The beam child's callback: runs its current state handler out of
/// `D_actor_206100_80149E24`, copying the table onto the stack first.
void func_actor_206100_8014F134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_206100_80149E24;
    sp.funcs[task->state](task);
}

void func_actor_206100_8014F18C(Task* task)
{
    Actor206100Work* work;

    work = (Actor206100Work*)task->work;

    work->obj_364.coord    = &((TmdObject*)task->extra)->coords[1];
    work->obj_364.ctx.recs = work->rec_384;
    work->obj_364.pos.vx   = 0;
    work->obj_364.pos.vy   = 0;
    work->obj_364.pos.vz   = 0;
    work->obj_364.key      = 0x3003D;
    work->obj_364.radius   = 0x400;
    work->obj_364.flags    = 1;
    Gp_LinkObj(2, &work->obj_364);
    Gp_InitRec18Table(work->rec_384, 6, 0);
    work->obj_364.flags &= 0x7FFF;

    work->obj_414.coord    = &((TmdObject*)task->extra)->coords[4];
    work->obj_414.ctx.recs = work->rec_384;
    work->obj_414.pos.vx   = 0;
    work->obj_414.pos.vy   = 0;
    work->obj_414.pos.vz   = 0;
    work->obj_414.key      = 0x3003D;
    work->obj_414.radius   = 0x200;
    work->obj_414.flags    = 1;
    Gp_LinkObj(2, &work->obj_414);
    work->obj_414.flags &= 0x7FFF;
}

void func_actor_206100_8014F284(Task* task)
{
    Actor206100Work*       work;
    Actor206100AnimStride* stride;
    s32                    i;

    work   = (Actor206100Work*)task->work;
    i      = 1;
    stride = (Actor206100AnimStride*)work + 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_510);
        i++;
        stride->field_1D = (u8)work->field_51A;
        stride++;
    } while (i < 0xF);
    work->field_50E = (u16)work->field_510;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Re-arms every animation slot for the pending request: writes the request's
/// step scale (`field_51A`) into each slot's `field_1D` and re-seeks the slot to
/// the requested clip with `func_800B4114`, whose fifth argument is the request's
/// own value at `field_524`.  When the clip already playing (`field_50E`) is not
/// the one requested, `field_524` is cleared as well.  `field_50E` latches the
/// clip either way, which is what lets the next frame tell the two cases apart.
/// The call sits *inside* the loop and takes a fresh `work` in `$a0` each
/// iteration, the same shape `func_actor_405800_80138294` has.
void func_actor_206100_8014F2F0(Task* arg0)
{
    Actor206100Work* work;
    s32              i;

    work = (Actor206100Work*)arg0->work;
    if (work->field_50E == work->field_510) {
        i = 1;
        do {
            ((Actor206100AnimStride*)work)[i].field_1D = (u8)work->field_51A;
            func_800B4114(&work->anim, i, work->field_510, 0, work->field_524);
            i++;
        } while (i < 0xF);
    } else {
        i = 1;
        do {
            ((Actor206100AnimStride*)work)[i].field_1D = (u8)work->field_51A;
            func_800B4114(&work->anim, i, work->field_510, 0, work->field_524);
            i++;
        } while (i < 0xF);
        work->field_524 = 0;
    }
    work->field_50E = work->field_510;
}
s16 func_actor_206100_8014F3C8(Task* arg0, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)arg0->work;

    if (work->field_51A == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_51A << 12) >> 16;
}

/// The actor's task callback: runs its current top-level state out of
/// `D_actor_206100_80149E5C`, copying the table onto the stack first.
void func_actor_206100_8014F428(Task* task)
{
    TaskFuncTable5 sp;

    sp = D_actor_206100_80149E5C;
    sp.funcs[task->state](task);
}

void func_actor_206100_8014F490(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

void func_actor_206100_8014F4B8(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

/// Runs the actor's sub-state handler for the current `field_522`, after
/// marking the enemy's list node so the exit path tears the actor down.
void func_actor_206100_8014F524(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    TaskFuncTable5   sp;

    work                      = (Actor206100Work*)task->work;
    enemy                     = (GpEnemy*)task->spawnArg2;
    sp                        = D_actor_206100_80149E94;
    enemy->node.state.b.flags = 1;
    sp.funcs[(s16)work->field_522](task);
}

void func_actor_206100_8014F59C(void)
{
}

void func_actor_206100_8014F5A4(void)
{
}

void func_actor_206100_8014F5AC(void)
{
}

void func_actor_206100_8014F5B4(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F8BC,
        func_actor_206100_8014F970,
    };

    states[(s16)work->field_522](task);
}

void func_actor_206100_8014F608(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F9C4,
        func_actor_206100_8014FA08,
    };

    states[(s16)work->field_522](task);
}

void func_actor_206100_8014F65C(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    func_actor_206100_8014DEAC(task);
    Gp_MsgPlayerWeapon(0);
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F69C(Task* task)
{
    u16              timer;
    Actor206100Work* work = (Actor206100Work*)task->work;

    func_actor_206100_8014DEAC(task);
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5A) {
        work->field_51E = 0;
        work->field_522 = work->field_522 + 1;
    }
}

void func_actor_206100_8014F6F8(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* anim;

    work            = (Actor206100Work*)task->work;
    work->field_51E = 0;
    anim            = (Actor206100Work*)task->work;
    anim->field_524 = 8;
    anim->field_51A = 8;
    anim->field_510 = 7;
    anim->field_50C = 1;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F738(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->field_524 = 0xA;
    work->field_51A = 0x10;
    work->field_510 = 1;
    work->field_50C = 1;
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F770(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;

    work            = (Actor206100Work*)task->work;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5B) {
        next            = (Actor206100Work*)task->work;
        next->field_520 = 3;
        next->field_522 = 0;
    }
}

void func_actor_206100_8014F7B4(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    s32              soundId;
    s32              pan;

    work    = (Actor206100Work*)task->work;
    soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 0xC) << 8) | 0x551E0005;
    pan     = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    next            = (Actor206100Work*)task->work;
    next->field_524 = 0xA;
    next->field_51A = 0x10;
    next->field_510 = 1;
    next->field_50C = 1;
    work->field_526 = 0x1388;
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F878(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;

    work            = (Actor206100Work*)task->work;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x3D) {
        next            = (Actor206100Work*)task->work;
        next->field_520 = 1;
        next->field_522 = 0;
    }
}

void func_actor_206100_8014F8BC(Task* task)
{
    Actor206100Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor206100Work*)task->work;
    work->field_524 = 6;
    work->field_51A = 0x10;
    work->field_510 = 0xA;
    work->field_50C = 1;
    soundId         = ((((GpEnemy*)task->spawnArg2)->placeKey >> 0xC) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F970(Task* task)
{
    Actor206100Work* work;
    s32              cond;

    work = (Actor206100Work*)task->work;
    if ((work->flags_514.half & 1) || (work->flags_514.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor206100Work*)task->work;
        work->field_520 = 2;
        work->field_522 = 0;
    }
}

void func_actor_206100_8014F9C4(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->field_524 = 8;
    work->field_51A = 0x10;
    work->field_510 = 0xE;
    work->field_50C = 1;
    work->field_51E = 0;
    work->field_526 = work->field_536;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014FA08(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;
    s32              cond;

    work            = (Actor206100Work*)task->work;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x1F) {
        work->field_557 = 1;
    }
    next = (Actor206100Work*)task->work;
    if ((next->flags_514.half & 1) || (next->flags_514.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond != 0) {
        next            = (Actor206100Work*)task->work;
        next->field_524 = 8;
        next->field_51A = 8;
        next->field_510 = 0x10;
        next->field_50C = 1;
    }
    if (Gp_TickObjFlag2(task->spawnArg2) != 0) {
        work->field_557 = 4;
        next            = (Actor206100Work*)task->work;
        next->field_520 = 1;
        next->field_522 = 0;
    }
}
/// Ring-spawn state: seeds `field_4F4` and `field_548` from the eight-point ring
/// `D_actor_206100_80158B68`, copies the current vertex into the root part
/// coordinate, advances the index modulo 8, and hands the actor the state-1
/// animation request.  `coord` is the coordinate the effect argument at
/// `eff_4C0` shares, so moving it moves the actor.
///
/// `enemy` is a local rather than the inline
/// `((GpEnemy*)task->spawnArg2)->node.state.b.flags = 1;` because the fused form
/// transposes the `spawnArg2` and `task->extra` loads; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store, so give the pointer its own local".
void func_actor_206100_8014FAE4(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor206100Work* work;
    Actor206100Work* next;
    Actor206100Work* last;
    GpEnemy*         enemy;

    work                      = (Actor206100Work*)task->work;
    enemy                     = (GpEnemy*)task->spawnArg2;
    coord                     = ((TmdObject*)task->extra)->coords;
    enemy->node.state.b.flags = 1;
    work->field_54D           = 1;
    work->field_548           = 0;
    work->field_4F4           = D_actor_206100_80158B68;
    next                      = (Actor206100Work*)task->work;
    next->field_51A           = 0x10;
    next->field_510           = 3;
    next->field_50C           = 2;
    work->field_43E           = 0x400;
    coord->coord.t[0]         = work->field_4F4[work->field_548].field_0;
    coord->coord.t[1]         = work->field_4F4[work->field_548].field_2;
    coord->coord.t[2]         = work->field_4F4[work->field_548].field_4;
    work->field_548           = (work->field_548 + 1) & 7;
    Gp_SetLightMode(task->spawnArg2, 2);
    work->field_51E = 0;
    last            = (Actor206100Work*)task->work;
    last->field_520 = 1;
    last->field_522 = 0;
}

void func_actor_206100_8014FBE4(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work  = (Actor206100Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    SndEvt_EnqueueType7(0x551E0002, 1);
    Gp_ApplyAreaRecs(&D_8018590C);
    work->field_526 = work->field_536;
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(task, 0);
    GameFlag_SetNibble(0xF3, 1);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj_364);
    Gp_UnlinkObj(&work->obj_414);
    work->field_51E = 0;
    work->field_520 = work->field_520 + 1;
    soundId         = ((((GpEnemy*)task->spawnArg2)->placeKey >> 0xC) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
}

/// Idle-state tick: re-arms the animation request, then advances the clip
/// phase `field_512` -- reset when the requested clip is not the one playing,
/// otherwise ramped by `func_actor_206100_8014F3C8` -- ticks every animation
/// slot and bumps the frame counter.
///
/// `next` is the same block as `work` loaded a second time: the first four
/// stores reach it through one local and everything after the request-kind
/// read through the other, which is what the two loads of `Task::work` are.
///
/// The loop is written `for (i = 1; i < 0xF; i++)` rather than as the
/// `do`/`while` its test-at-the-bottom shape suggests, and its initialiser sits
/// *after* the sub-state chain rather than before it.  Placed before the chain,
/// the store that materialises `i` is the one the case-3 branch jumps over, and
/// post-reload CSE then rewrites the phase's `+ 1` into `+ $s0`; after the chain
/// that store is the branch's own target, reorg copies it into the delay slot
/// and threads the branch past it.  See `DECOMPILATION_LEARNINGS.md`, "A
/// constant store in a delay slot decides whether post-reload CSE folds it into
/// a later increment".
void func_actor_206100_8014FCD4(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    s32              i;
    s16              state;

    work            = (Actor206100Work*)task->work;
    work->field_524 = 4;
    work->field_51A = 0x10;
    work->field_510 = 0xE;
    work->field_50C = 1;
    next            = (Actor206100Work*)task->work;
    state           = next->field_50C;
    if (state == 1) {
        if (next->field_50E != next->field_510) {
            next->field_512 = 0;
        } else {
            next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
        }
        func_actor_206100_8014F2F0(task);
        next->field_50C = 3;
    } else if (state == 2) {
        func_actor_206100_8014F284(task);
        next->field_50C = 3;
        next->field_512 = 0;
    } else if (state == 3) {
        next->field_512 = next->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        Gp_AnimTickIndex(&next->anim, i);
    }
    work->field_520 = work->field_520 + 1;
}
/// Idle-state tick: advances the actor's two frame counters, keeps the root
/// coordinate dirty so `GsGetLw` rebuilds it, spawns the shockwave task once the
/// counter reaches 0x5A and retires the actor four frames later.
///
/// `coord` is a local rather than the inline
/// `((TmdObject*)task->extra)->field_8->flg = 0;` because the fused form loads
/// `task->extra` *after* the two counter stores, and sched1 will not lift a load
/// above an earlier store; its address load stays with the stores and both pick
/// up load-delay nops.  Binding the pointer above the counters frees the two
/// loads to be scheduled first, which is the target's order; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store".
void func_actor_206100_8014FDE8(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;

    coord           = ((TmdObject*)task->extra)->coords;
    work            = (Actor206100Work*)task->work;
    work->field_51E = work->field_51E + 1;
    work->field_526 = work->field_526 + 0x10;
    coord->flg      = 0;
    if ((s16)work->field_51E == 0x5A) {
        Task_SpawnFromTable(&D_801818BC, 0, 0, 0);
    }
    if ((s16)work->field_51E >= 0x10E) {
        task->state     = 4;
        next            = (Actor206100Work*)task->work;
        next->field_520 = 0;
        next->field_522 = 0;
    }
}
