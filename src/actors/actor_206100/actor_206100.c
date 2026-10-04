#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/waypoints.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/neo_ark_submarine_gallery.h"
#include "../../shared/screen_wave.h"
#include "../../shared/coord_math.h"
#include "../../shared/diver.h"

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
    GfxMatrix          matrix;
    Actor206100GteView gte;
} Actor206100VecScratch;

STATIC_ASSERT_SIZEOF(Actor206100VecScratch, 0x20);

extern TaskDesc D_actor_206100_80158B0C[];

/// The four handlers `func_actor_206100_8014E7D4` picks between as the effect
/// mode `gSceneCombatState.actorControl` changes -- the retirement `func_actor_206100_8014FBE4`,
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

/// The attack the beam's collision object carries at `WorldCollisionBody.key`.
/// `power` is 0x1A and `reaction` is 5.
extern DamageAttack D_actor_206100_80155194;

/// Enemy parameters `func_actor_206100_8014AF74` parks in `Enemy::param`.
/// `attacks` is `D_actor_206100_80155194` above and `hpMax` is the actor's
/// maximum hit points (2000), seeded into `field_40` / `field_42` at spawn.
extern EnemyParams D_actor_206100_80155198;

/// Animation bank handed to `animationInitContext` by `func_actor_206100_8014AF74`.
extern AnimationSet* D_actor_206100_80158B24[];

/// Placement records `func_actor_206100_8014EE2C` parks at `Enemy::place`
/// -- the same slot `Gp_SpawnArea` fills from a room's own place list, so this
/// is a local six-entry copy of one: `field_0` is 4 on the five live entries
/// and 0xFF on the sixth, the value `Gp_SpawnArea` stops its walk on.  The
/// overlay indexes it with the variant it was spawned for rather than walking
/// it, so the tail entry is reachable.
extern AreaPlacement D_actor_206100_80155134[];

/// The eight positions of the ring the actor circles, walked by the index at
/// `Actor206100Work::field_548`.
///
/// Each is a point in the root coordinate's translation units: radius 7600 in
/// the XZ plane, one 45-degree step per entry, at a constant height of 3000.
extern SVECTOR D_actor_206100_80158B68[8];

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
    /* 0x0 */ Enemy* enemy;
    /* 0x4 */ s32    timer;
} Actor206100Slot;
STATIC_ASSERT_SIZEOF(Actor206100Slot, 0x8);

/// The two companion slots, zeroed as an 8-byte-stride pair by the spawn state
/// `func_actor_206100_8014C274`.
extern Actor206100Slot D_actor_206100_80158CBC[2];

/// 0xC-byte scratch `func_actor_206100_8014ED3C` takes off the scratch stack to
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
/// its own work block.  Reach it with
/// `(Actor206100Work*)task->work`.  (The overlay's only other allocation,
/// `memCalloc(0x68, 0)` in `func_actor_206100_8014C458`, belongs to the child
/// task that `Task_SpawnFromTable` returns there, so it is a different `Task`
/// and a different block.)
///
/// `field_520` / `subState` are the state and sub-state indices the handler
/// table walks and `field_51E` is the per-state frame counter -- the same
/// layout the other enemy overlays use.  `animRequest`, `animPlaying`, `animClip`
/// and `animStep` are the animation request the actor hands to its player:
/// `func_actor_206100_8014C274` writes `animRequest` as the request kind and then
/// reads `animPlaying` and `animClip` as the clip to play, with `animStep` the
/// step scale and `field_512` the clip phase the idle handler
/// `func_actor_206100_8014FCD4` ramps -- zeroed when the requested clip is not
/// the one playing, otherwise advanced by `func_actor_206100_8014F3C8` and
/// stepped once per frame in sub-state 3.
/// `animStatus` sits between `animClip` and `animStep` and is
/// status, not part of the request: `func_actor_206100_8014F970` tests its
/// boundary, jump and settled bits to decide whether to advance the actor to
/// state 2.
/// `anim` is the animation context at offset 0 -- the block is handed to
/// `animationResetSlot` as its `AnimationContext` -- with the 0x28-byte animation
/// slots at +0x14, the layout `_Actor400500GrayStalkerWork` uses.
///
/// `obj_364` / `obj_414` are the two `Gp_LinkObj` nodes the actor's retirement
/// handler `func_actor_206100_8014FBE4` unlinks, alongside the enemy's own
/// `WorldTargetNode`.  Both nodes point their `context.contacts` at the same six-entry
/// `WorldCollisionContact` table `func_actor_206100_8014F18C` zeroes in `rec_384`, which is
/// why `Gp_InitRec18Table` is called once for the pair.
typedef struct Actor206100Work {
    /* 0x000 */ AnimationContext anim;
    /* 0x014 */ AnimationSlot    slots[0xF];
    /// `animationInitContext`'s poseBuffer buffer, the 0x90-byte scratch every animation
    /// context carries alongside its slot array.
    /* 0x26C */ byte animAux[0x90];
    /* 0x2FC */ byte pad_2FC[0x60];
    /// Pair `func_actor_206100_8014D574` halves once a frame while
    /// `field_51E` is below 0x1E, ahead of the explosion it triggers on that
    /// frame: both are read back as `u16` (the load is `lhu`), so the halving
    /// is unsigned-promoted, and each is stored with a plain `sh`.
    /* 0x35C */ u16                field_35C;
    /* 0x35E */ u16                field_35E;
    /* 0x360 */ u16                field_360;
    /* 0x362 */ byte               pad_362[0x2];
    /* 0x364 */ WorldCollisionBody obj_364;
    /// The six-entry contact table `func_actor_206100_8014F18C` zeroes and both
    /// objects above point their `context.contacts` at.  `func_actor_206100_8014BAA8`
    /// walks it one record a step: `field_4` carries the packed hit id whose
    /// high half selects kind 2, and the walk stops early once `field_52A` goes
    /// up.  The record's own `field_0` / `field_8..field_14` are the occupancy
    /// flags `Gp_ClearRec18Occupied` walks, so nothing here reads them.
    /* 0x384 */ WorldCollisionContact rec_384[6];
    /* 0x414 */ WorldCollisionBody    obj_414;
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
    /// `Actor503500Work::hitEffect` holds.
    /* 0x4C0 */ EffectSpawnArg eff_4C0;
    /* 0x4C8 */ byte           pad_4C8[0x8];
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
    /* 0x4F4 */ SVECTOR* field_4F4;
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
    /* 0x508 */ s16 field_508;
    /* 0x50A */ s16 field_50A;
    /* 0x50C */ s16 animRequest; // animation request kind
    /* 0x50E */ s16 animPlaying; // clip the request plays, latched from animClip
    /* 0x510 */ s16 animClip;    // animation clip id
    /* 0x512 */ s16 field_512;
    /* 0x514 */ u16 animStatus;  // Slot 1's ANIMATION_SLOT_* results from the latest frame's ticks; the states test this copy to learn their clip ended
    /* 0x516 */ u16 frameCount;  // Frames the actor has run, wrapping; only ever incremented, nothing in this package reads it

                                 /// Second half of the per-frame counter pair the state dispatcher
    /// `func_actor_206100_8014DA28` and the spawn state `func_actor_206100_8014C458`
    /// both bump: the two advance together, ahead of the sub-state handler.
    /* 0x518 */ u16 field_518;
    /* 0x51A */ s16 animStep;  // animation step scale
                               /// Yaw to the walk target, latched with the distance below by
                               /// `func_actor_206100_8014B698` from `ratan2` of the player delta it
                               /// normalises.
    /* 0x51C */ s16 field_51C;
    /* 0x51E */ u16 field_51E; // per-state frame counter
    /* 0x520 */ s16 field_520; // state index
    /* 0x522 */ u16 subState;  // sub-state index
    /* 0x524 */ s16 animBlend;
    /* 0x526 */ u16 field_526;
    /// XZ distance to the walk target the yaw above was taken from, the
    /// shorter of the two `gPlayerActorTasks` distances.
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
    /* 0x536 */ u16  field_536; // seeded from D_neo_ark_submarine_gallery_80181A48 when the block is built
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
    /// View index the actor was in before it teleported, saved by
    /// `func_actor_206100_8014CB68` from `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` right before it
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
    /// Animation step the spawn state leaves at 4 (`diverRestartClip`
    /// copies it into every slot's `rate`) and `func_actor_206100_8014BAA8`
    /// also reads as a part index into the root coordinate array, so the load
    /// there is `lbu` and the field is unsigned.
    /* 0x557 */ u8 field_557;
} Actor206100Work;
STATIC_ASSERT_SIZEOF(Actor206100Work, 0x558);

/// The Diver library's name for this package's work block (see diver.h).
typedef Actor206100Work DiverWork;

/// Work block of the beam task `func_actor_206100_8014C458` spawns off
/// `D_actor_206100_80158B0C` when `Actor206100Work::field_555` is set: it
/// `memCalloc(0x68, 0)`s one and parks it in the child's `Task::work`, the
/// same reuse `Actor206100Work` makes of the parent's slot.
///
/// `obj` is the kind-1 `WorldCollisionBody` the spawn state `func_actor_206100_8014EEC0`
/// links into the collision list and `func_actor_206100_8014FBE4` unlinks
/// again on retirement, so the 0x8 before it is not the node's own header and
/// stays zero.  `rec` is the two-entry `WorldCollisionContact` table `obj.context.contacts` points at.
/// `field_58` / `field_5A` / `field_5C` are the view-space deltas the spawner
/// stores from the actor's coordinate, `field_60` the pair index the setup
/// hands to `diverImpactBurst`, and `field_64` the scale word it
/// biases by 0x10002000.  The tick handler `func_actor_206100_8014B8B4`
/// advances `field_5A` and adds `field_58` into the coordinate's `t[1]`.
typedef struct Actor206100ChildWork {
    /* 0x00 */ byte                  pad_0[0x8];
    /* 0x08 */ WorldCollisionBody    obj;
    /* 0x28 */ WorldCollisionContact rec[2];
    /* 0x58 */ s16                   field_58;
    /* 0x5A */ s16                   field_5A;
    /* 0x5C */ s16                   field_5C;
    /* 0x5E */ byte                  pad_5E[0x2];
    /* 0x60 */ s32                   field_60;
    /* 0x64 */ s32                   field_64;
} Actor206100ChildWork;
STATIC_ASSERT_SIZEOF(Actor206100ChildWork, 0x68);

/// The wave `func_actor_206100_8014CB68` arms: pale cyan modulation with a
/// one-frame ramp.
extern ScreenWaveCtx D_actor_206100_80158CCC;

/// Child task `func_actor_206100_8014CB68` starts with the tint above as its
/// spawn arg.  Its callback is `screenWaveTask`.
extern TaskDesc D_actor_206100_80158AF0[];

/// Spawn-state body: hands the freshly spawned enemy its model, its part
/// coordinate and its state, then starts the animation.
///
/// `task->spawnArg2.pointer` is the `Enemy` `func_actor_206100_8014EC14` spawned, so
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
static void func_actor_206100_8014AF74(Task* task);

/// Builds the enemy's two collision objects.  Each is bound to a part
/// coordinate of the actor's `TmdObject` -- `obj_364` to `coords[1]` with
/// `field_1C` 0x400, `obj_414` to `field_8[4]` with 0x200 -- and both point
/// their `field_C` at the shared `WorldCollisionContact` pair table zeroed at `rec_384`,
/// which is why there is a single `Gp_InitRec18Table` for the pair.  Each
/// block ends by clearing `flags` bit 0x8000 after its `Gp_LinkObj`, the same
/// tail shape `func_actor_403100_80132320` has (`|= 0x8000` there).
static void func_actor_206100_8014F18C(Task* task);

/// Builds the child beam's collision state: links its `WorldCollisionBody` and initializes
/// the coordinate the beam is drawn at. `task` is the child spawned by
/// `func_actor_206100_8014C458`, so its `Task::work` is the
/// `Actor206100ChildWork` above.
static void func_actor_206100_8014EEC0(Task* task);

/// Spawns the beam's impact effect burst at `coord`. `arg1` is
/// `Actor206100ChildWork::field_60` (the `DamageAttack` index): `(arg1 >> 1) % 6`
/// picks the spark frame `diverDrawSpark` plays, bit 0 gates the
/// puff and the low three bits the directional tail. `arg2` selects the burst
/// - 0 a lone spark, 1 the spark plus those two extras, 2 a four-shot ring -
/// and `arg3` is the biased `field_64` scale word, whose low 12 bits are the
/// effect parameter and bits 12..15 a variant index.

/// Steps the actor's model coordinate `arg1` along the heading `arg2`, in the
/// XZ plane, and marks it dirty.
///
/// `task->extra` is the actor's `TmdObject`, so `coords` is the root
/// `GfxCoord` of its part array: `coord.t[0]` gains `rsin(arg2) * arg1`
/// and `coord.t[2]` `rcos(arg2) * arg1`. The `<< 4` on the `rsin` / `rcos`
/// result and the `>> 16` after the multiply are one `>> 12` split in two, the
/// unit circle the rest of the overlay's rotation code uses. Clearing `composeStamp` is
/// what makes the composition pass rebuild the matrix from `coord`, so the caller never
/// writes `workm` itself. The `task->extra` chain is walked again for each of
/// the three statements because `rsin` / `rcos` sit between them.
///
/// Every call site in this overlay takes `arg2` from the actor's heading and
/// `arg1` from a step distance, either a constant (`0x30`, `0x40`) or an
/// `s16` the caller narrows itself.

/// State-1 body: ticks the actor's per-state frame counter and, once it
/// reaches 0x22, walks the actor out of the scene -- parks its model coordinate
/// at y 0x1B58, clears the counter, hands the area record to the light mode,
/// tells slot 3 (message 0x3E9) to place the player, and advances the sub-state
/// `subState`.  Before that, the counter passing 3 fires the overlay's sound
/// event.  Every other frame ramps `field_526` toward 0x1D4C by a quarter of the
/// remaining distance and steps the actor `0x30` along its heading.
///
/// The second argument of `Gp_SetLightMode` is read through a cast rather than
/// as `task->spawnArg2.pointer` directly: the cast makes the load a *scalar* `MEM`,
/// which is what keeps its dependence on the fixed-address
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` store, so the store is scheduled ahead of it - the same
/// `MEM_IN_STRUCT_P` mechanism `dryfield_water_tank_4.c` and
/// `DECOMPILATION_LEARNINGS.md`, "Scalar memory references", describe.
static void func_actor_206100_8014CD08(Task* task);

/// State handler 4 of `D_actor_206100_80149E94`, and the one that hands the
/// actor to `func_actor_206100_8014CD08` above.  It clears the fixed-address
/// `D_neo_ark_submarine_gallery_801818B8` flag, ticks the per-state counter `field_51E` and seeds
/// `D_actor_206100_80158CCC.state` to `SCREEN_WAVE_RAMP_FINISHED` on the first frame; frame 3 retires the
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
static void func_actor_206100_8014CE60(Task* task);

/// Sub-state 0 of `func_actor_206100_8014CFF4`'s table: clears the per-state
/// frame counter and the actor's animation request, then advances the
/// sub-state.
static void func_actor_206100_8014F6F8(Task* task);

/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks `field_51E`,
/// arms `field_52E` with 0x18 on the first frame, eases `field_35C` toward
/// 0x4000 over frames 0x29..0x4D, fires the overlay's sound events at 0x54
/// (type 6, panned) and 0x77, sets `field_555` on the six cue frames of the
/// counter, and folds the heading to the actor's target --
/// `VectorNormalSS` then `ratan2` -- into `field_43E` in steps of 0xC.
static void func_actor_206100_8014D14C(Task* task);

/// Sub-state 1 of `D_actor_206100_8014D6F4`'s table
/// (`D_actor_206100_80149EB4`, whose first entry `func_actor_206100_8014F7B4`
/// and third `func_actor_206100_8014F878` bracket it).  On the seventh frame of
/// the sub-state it splats 0x20 effect particles around the actor's root
/// coordinate -- the same `Gp_SpawnEff` id 0x01202148 ring
/// `func_actor_206100_8014D574` fires, at a radius of 0x1000 and a constant
/// y of -0x3E8 -- and from frame 0x1F it draws from `gRandomLcgState`: a one-in-four
/// `(state >> 16) & 3 == 0` hands state 2 (the teleport
/// `func_actor_206100_8014CB68`) to the actor at sub-state 0, and every other
/// draw restarts the counter and advances the sub-state.
static void func_actor_206100_8014D8E8(Task* task);

/// Sub-state 1 of the state-2 dispatcher `func_actor_206100_8014DA28`'s
/// two-entry local table, which picks it with `funcs[(s16)field_520]` and is
/// entered from that dispatcher's `gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING` arm -- entry 0 is the ring
/// stepper `func_actor_206100_8014FAE4`.  It maintains the actor's companions.
///
/// `field_51E` is held at 0x1E -- the frame `func_actor_206100_8014D574` fires
/// the explosion on -- by arming the global `gSceneCombatState` flag again through
/// `Gp_ArmStateF0` instead of advancing it, so the sub-state never leaves it;
/// every earlier frame just advances the counter.  The shared companion tick
/// `func_actor_206100_8014DEAC` then runs, and the two slots
/// `D_actor_206100_80158CBC` are walked by index, each handled on its own:
///
/// - an empty slot whose timer has run out spawns a companion with
///   `func_actor_206100_8014EE2C`, stores it, arms its `hp`
///   and advances `field_551` -- but only while `field_551` is still
///   below 5, because that index picks the variant's place record;
/// - an empty slot whose timer is still running counts it down by one;
/// - a filled slot whose enemy has lost its `hp` is emptied and
///   armed with a 0xB4-frame cooldown, and the fifth such release moves the
///   actor to state 2 with the state and sub-state indices cleared.
///
/// The state change reads `task->work` again rather than reusing `work`, the
/// same fresh load `set_state` makes.
static void func_actor_206100_8014DD3C(Task* task);

/// Teardown state of the beam child, run until its countdown kills it.

/// Transforms `pos` from `coord`'s space up the parent chain into the view
/// coordinate's space.  Returns 1 with `pos` rewritten once the walk reaches
/// `gGfxViewCoord`, or 0 with `pos` untouched if the chain ends first.

/// Last of the actor's five top-level states (`D_actor_206100_80149E5C`):
/// hands the task's `Enemy`, parked in `Task::spawnArg2`, back to
/// `enemyDestroy`.
static void func_actor_206100_8014F490(Task* task);

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation
/// alone.
static void func_actor_206100_8014F4B8(MATRIX* src, MATRIX* dst);

static void func_actor_206100_8014B0AC(Task* task, u8 arg1);
static void func_actor_206100_8014E0C0(Task* task);
static void func_actor_206100_8014EB60(Task* task);
static void func_actor_206100_8014EC54(Task* task);
static void func_actor_206100_8014DEAC(Task* task);
static void func_actor_206100_8014FAE4(Task* task);

static Enemy* func_actor_206100_8014EE2C(s32 arg0);

static void func_actor_206100_8014D574(Task* task);
static void func_actor_206100_8014EB48(Task* task, s16 arg1);
static void func_actor_206100_8014ED3C(Task* task, s16 arg1);
static void func_actor_206100_8014F2F0(Task* task);
static s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
static void func_actor_206100_8014F738(Task* task);
static void func_actor_206100_8014F770(Task* task);
static void func_actor_206100_8014F7B4(Task* task);
static void func_actor_206100_8014F878(Task* task);
static void func_actor_206100_8014E964(Task* task, void* unusedTable);
static void func_actor_206100_8014FBE4(Task* task, void* unusedTable);
static void func_actor_206100_8014FCD4(Task* task, void* unusedTable);
static void func_actor_206100_8014FDE8(Task* task, void* unusedTable);
static void func_actor_206100_8014E228(Task* task);

/// Distortion amplitude of the screen wave: `frame * scale / span` of the
/// running spawn argument, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The spawn argument of the running wave task, parked at spawn so the tick
/// reads the ramp through it.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ScreenWaveOscillator gScreenWaveColumns[13];
extern ScreenWaveOscillator gScreenWaveRows[32];

/// Task table `func_actor_206100_8014FDE8` spawns the shockwave from.

static void func_actor_206100_8014DEAC(Task* task);
static void func_actor_206100_8014F2F0(Task* task);
static s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
static void func_actor_206100_8014F970(Task* task);
static void func_actor_206100_8014F9C4(Task* task);
static void func_actor_206100_8014FA08(Task* task);

static void func_actor_206100_8014B8B4(Task* task);

static const TaskFuncTable3 D_actor_206100_80149E24 = {
    {
        func_actor_206100_8014EEC0,
        func_actor_206100_8014B8B4,
        diverStrikeTeardown,
    },
};

static u32     _gActor206100DiverEnergyBallPartVerts[1];
static SVECTOR _gActor206100DiverEnergyBallVerts[24];
static SVECTOR _gActor206100DiverEnergyBallNormals[25];
static TmdBone _gActor206100DiverEnergyBallSkeleton[1];
static u32     _gActor206100DiverEnergyBallStream[270];

static TmdSource _gActor206100DiverBody;
void             func_actor_206100_8014F134(Task*);
void             func_actor_206100_8014F428(Task*);

static TmdBone _gActor206100DiverBodySkeleton[15] = {
#include "assets/diver_body_skeleton.inc"
};

static u32 _gActor206100DiverBodyPartVerts[15] = {
#include "assets/diver_body_partVerts.inc"
};

static SVECTOR _gActor206100DiverBodyVerts[145] = {
#include "assets/diver_body_verts.inc"
};

static SVECTOR _gActor206100DiverBodyNormals[142] = {
#include "assets/diver_body_normals.inc"
};

static u32 _gActor206100DiverBodyStream[2494] = {
#include "assets/diver_body_stream.inc"
};

static TmdSource _gActor206100DiverBody = {
    0,
    11652,
    5104,
    15,
    _gActor206100DiverBodyPartVerts,
    _gActor206100DiverBodyVerts,
    _gActor206100DiverBodyNormals,
    _gActor206100DiverBodySkeleton,
    _gActor206100DiverBodyStream,
};

static TmdBone _gActor206100DiverBurstHeadSkeleton[1] = {
#include "assets/diver_burst_head_skeleton.inc"
};

static u32 _gActor206100DiverBurstHeadPartVerts[1] = {
#include "assets/diver_burst_head_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstHeadVerts[35] = {
#include "assets/diver_burst_head_verts.inc"
};

static SVECTOR _gActor206100DiverBurstHeadNormals[44] = {
#include "assets/diver_burst_head_normals.inc"
};

static u32 _gActor206100DiverBurstHeadStream[360] = {
#include "assets/diver_burst_head_stream.inc"
};

static TmdSource _gActor206100DiverBurstHead = {
    0,
    2388,
    0,
    1,
    _gActor206100DiverBurstHeadPartVerts,
    _gActor206100DiverBurstHeadVerts,
    _gActor206100DiverBurstHeadNormals,
    _gActor206100DiverBurstHeadSkeleton,
    _gActor206100DiverBurstHeadStream,
};

static TmdBone _gActor206100DiverBurstArmRightSkeleton[1] = {
#include "assets/diver_burst_arm_right_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmRightPartVerts[1] = {
#include "assets/diver_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmRightVerts[14] = {
#include "assets/diver_burst_arm_right_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmRightNormals[24] = {
#include "assets/diver_burst_arm_right_normals.inc"
};

static u32 _gActor206100DiverBurstArmRightStream[143] = {
#include "assets/diver_burst_arm_right_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmRight = {
    0,
    904,
    0,
    1,
    _gActor206100DiverBurstArmRightPartVerts,
    _gActor206100DiverBurstArmRightVerts,
    _gActor206100DiverBurstArmRightNormals,
    _gActor206100DiverBurstArmRightSkeleton,
    _gActor206100DiverBurstArmRightStream,
};

static TmdBone _gActor206100DiverBurstArmLeft1Skeleton[1] = {
#include "assets/diver_burst_arm_left_1_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmLeft1PartVerts[1] = {
#include "assets/diver_burst_arm_left_1_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft1Verts[14] = {
#include "assets/diver_burst_arm_left_1_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft1Normals[24] = {
#include "assets/diver_burst_arm_left_1_normals.inc"
};

static u32 _gActor206100DiverBurstArmLeft1Stream[143] = {
#include "assets/diver_burst_arm_left_1_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmLeft1 = {
    0,
    904,
    0,
    1,
    _gActor206100DiverBurstArmLeft1PartVerts,
    _gActor206100DiverBurstArmLeft1Verts,
    _gActor206100DiverBurstArmLeft1Normals,
    _gActor206100DiverBurstArmLeft1Skeleton,
    _gActor206100DiverBurstArmLeft1Stream,
};

static TmdBone _gActor206100DiverBurstLegRightSkeleton[1] = {
#include "assets/diver_burst_leg_right_skeleton.inc"
};

static u32 _gActor206100DiverBurstLegRightPartVerts[1] = {
#include "assets/diver_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstLegRightVerts[19] = {
#include "assets/diver_burst_leg_right_verts.inc"
};

static SVECTOR _gActor206100DiverBurstLegRightNormals[33] = {
#include "assets/diver_burst_leg_right_normals.inc"
};

static u32 _gActor206100DiverBurstLegRightStream[210] = {
#include "assets/diver_burst_leg_right_stream.inc"
};

static TmdSource _gActor206100DiverBurstLegRight = {
    0,
    1360,
    0,
    1,
    _gActor206100DiverBurstLegRightPartVerts,
    _gActor206100DiverBurstLegRightVerts,
    _gActor206100DiverBurstLegRightNormals,
    _gActor206100DiverBurstLegRightSkeleton,
    _gActor206100DiverBurstLegRightStream,
};

static TmdBone _gActor206100DiverBurstArmLeft2Skeleton[1] = {
#include "assets/diver_burst_arm_left_2_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmLeft2PartVerts[1] = {
#include "assets/diver_burst_arm_left_2_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft2Verts[19] = {
#include "assets/diver_burst_arm_left_2_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft2Normals[33] = {
#include "assets/diver_burst_arm_left_2_normals.inc"
};

static u32 _gActor206100DiverBurstArmLeft2Stream[210] = {
#include "assets/diver_burst_arm_left_2_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmLeft2 = {
    0,
    1360,
    0,
    1,
    _gActor206100DiverBurstArmLeft2PartVerts,
    _gActor206100DiverBurstArmLeft2Verts,
    _gActor206100DiverBurstArmLeft2Normals,
    _gActor206100DiverBurstArmLeft2Skeleton,
    _gActor206100DiverBurstArmLeft2Stream,
};

static TmdBone _gActor206100DiverEnergyBallSkeleton[1] = {
#include "assets/diver_energy_ball_skeleton.inc"
};

static u32 _gActor206100DiverEnergyBallPartVerts[1] = {
#include "assets/diver_energy_ball_partVerts.inc"
};

static SVECTOR _gActor206100DiverEnergyBallVerts[24] = {
#include "assets/diver_energy_ball_verts.inc"
};

static SVECTOR _gActor206100DiverEnergyBallNormals[25] = {
#include "assets/diver_energy_ball_normals.inc"
};

static u32 _gActor206100DiverEnergyBallStream[270] = {
#include "assets/diver_energy_ball_stream.inc"
};

static TmdSource _gActor206100DiverEnergyBall = {
    0,
    1760,
    0,
    1,
    _gActor206100DiverEnergyBallPartVerts,
    _gActor206100DiverEnergyBallVerts,
    _gActor206100DiverEnergyBallNormals,
    _gActor206100DiverEnergyBallSkeleton,
    _gActor206100DiverEnergyBallStream,
};

AreaPlacement D_actor_206100_80155134[6] = {
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

DamageAttack D_actor_206100_80155194 = { 26, 5 };

EnemyParams D_actor_206100_80155198 = { &D_actor_206100_80155194, 2000, 400, 1000, 15, 200, 3, 100, 5 };

static AnimationPackedPose _gActor206100Animation0B8F8Bank1[8] = {
#include "assets/actor_206100_animation_0B8F8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0B8F8Bank4[133] = {
#include "assets/actor_206100_animation_0B8F8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0B8F8Records[183] = {
#include "assets/actor_206100_animation_0B8F8_records.inc"
};

static u16 _gActor206100Animation0B8F8Indices[16] = {
#include "assets/actor_206100_animation_0B8F8_indices.inc"
};

static AnimationSet _gActor206100Animation0B8F8 = {
    _gActor206100Animation0B8F8Records,
    _gActor206100Animation0B8F8Indices,
    { NULL, _gActor206100Animation0B8F8Bank1, NULL, NULL, _gActor206100Animation0B8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0BC80Bank1[3] = {
#include "assets/actor_206100_animation_0BC80_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0BC80Bank4[78] = {
#include "assets/actor_206100_animation_0BC80_bank4.inc"
};

static AnimationRecord _gActor206100Animation0BC80Records[121] = {
#include "assets/actor_206100_animation_0BC80_records.inc"
};

static u16 _gActor206100Animation0BC80Indices[16] = {
#include "assets/actor_206100_animation_0BC80_indices.inc"
};

static AnimationSet _gActor206100Animation0BC80 = {
    _gActor206100Animation0BC80Records,
    _gActor206100Animation0BC80Indices,
    { NULL, _gActor206100Animation0BC80Bank1, NULL, NULL, _gActor206100Animation0BC80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0C544Bank1[8] = {
#include "assets/actor_206100_animation_0C544_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0C544Bank4[174] = {
#include "assets/actor_206100_animation_0C544_bank4.inc"
};

static AnimationRecord _gActor206100Animation0C544Records[345] = {
#include "assets/actor_206100_animation_0C544_records.inc"
};

static u16 _gActor206100Animation0C544Indices[16] = {
#include "assets/actor_206100_animation_0C544_indices.inc"
};

static AnimationSet _gActor206100Animation0C544 = {
    _gActor206100Animation0C544Records,
    _gActor206100Animation0C544Indices,
    { NULL, _gActor206100Animation0C544Bank1, NULL, NULL, _gActor206100Animation0C544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0CC4CBank1[10] = {
#include "assets/actor_206100_animation_0CC4C_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0CC4CBank4[167] = {
#include "assets/actor_206100_animation_0CC4C_bank4.inc"
};

static AnimationRecord _gActor206100Animation0CC4CRecords[235] = {
#include "assets/actor_206100_animation_0CC4C_records.inc"
};

static u16 _gActor206100Animation0CC4CIndices[16] = {
#include "assets/actor_206100_animation_0CC4C_indices.inc"
};

static AnimationSet _gActor206100Animation0CC4C = {
    _gActor206100Animation0CC4CRecords,
    _gActor206100Animation0CC4CIndices,
    { NULL, _gActor206100Animation0CC4CBank1, NULL, NULL, _gActor206100Animation0CC4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0D530Bank1[10] = {
#include "assets/actor_206100_animation_0D530_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0D530Bank4[213] = {
#include "assets/actor_206100_animation_0D530_bank4.inc"
};

static AnimationRecord _gActor206100Animation0D530Records[308] = {
#include "assets/actor_206100_animation_0D530_records.inc"
};

static u16 _gActor206100Animation0D530Indices[16] = {
#include "assets/actor_206100_animation_0D530_indices.inc"
};

static AnimationSet _gActor206100Animation0D530 = {
    _gActor206100Animation0D530Records,
    _gActor206100Animation0D530Indices,
    { NULL, _gActor206100Animation0D530Bank1, NULL, NULL, _gActor206100Animation0D530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0D968Bank1[6] = {
#include "assets/actor_206100_animation_0D968_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0D968Bank4[103] = {
#include "assets/actor_206100_animation_0D968_bank4.inc"
};

static AnimationRecord _gActor206100Animation0D968Records[131] = {
#include "assets/actor_206100_animation_0D968_records.inc"
};

static u16 _gActor206100Animation0D968Indices[16] = {
#include "assets/actor_206100_animation_0D968_indices.inc"
};

static AnimationSet _gActor206100Animation0D968 = {
    _gActor206100Animation0D968Records,
    _gActor206100Animation0D968Indices,
    { NULL, _gActor206100Animation0D968Bank1, NULL, NULL, _gActor206100Animation0D968Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0DEA8Bank1[15] = {
#include "assets/actor_206100_animation_0DEA8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0DEA8Bank4[109] = {
#include "assets/actor_206100_animation_0DEA8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0DEA8Records[164] = {
#include "assets/actor_206100_animation_0DEA8_records.inc"
};

static u16 _gActor206100Animation0DEA8Indices[16] = {
#include "assets/actor_206100_animation_0DEA8_indices.inc"
};

static AnimationSet _gActor206100Animation0DEA8 = {
    _gActor206100Animation0DEA8Records,
    _gActor206100Animation0DEA8Indices,
    { NULL, _gActor206100Animation0DEA8Bank1, NULL, NULL, _gActor206100Animation0DEA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0E4A8Bank1[20] = {
#include "assets/actor_206100_animation_0E4A8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0E4A8Bank4[126] = {
#include "assets/actor_206100_animation_0E4A8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0E4A8Records[180] = {
#include "assets/actor_206100_animation_0E4A8_records.inc"
};

static u16 _gActor206100Animation0E4A8Indices[16] = {
#include "assets/actor_206100_animation_0E4A8_indices.inc"
};

static AnimationSet _gActor206100Animation0E4A8 = {
    _gActor206100Animation0E4A8Records,
    _gActor206100Animation0E4A8Indices,
    { NULL, _gActor206100Animation0E4A8Bank1, NULL, NULL, _gActor206100Animation0E4A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0E798Bank1[5] = {
#include "assets/actor_206100_animation_0E798_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0E798Bank4[43] = {
#include "assets/actor_206100_animation_0E798_bank4.inc"
};

static AnimationRecord _gActor206100Animation0E798Records[112] = {
#include "assets/actor_206100_animation_0E798_records.inc"
};

static u16 _gActor206100Animation0E798Indices[16] = {
#include "assets/actor_206100_animation_0E798_indices.inc"
};

static AnimationSet _gActor206100Animation0E798 = {
    _gActor206100Animation0E798Records,
    _gActor206100Animation0E798Indices,
    { NULL, _gActor206100Animation0E798Bank1, NULL, NULL, _gActor206100Animation0E798Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0ECA8Bank1[11] = {
#include "assets/actor_206100_animation_0ECA8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0ECA8Bank4[108] = {
#include "assets/actor_206100_animation_0ECA8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0ECA8Records[165] = {
#include "assets/actor_206100_animation_0ECA8_records.inc"
};

static u16 _gActor206100Animation0ECA8Indices[16] = {
#include "assets/actor_206100_animation_0ECA8_indices.inc"
};

static AnimationSet _gActor206100Animation0ECA8 = {
    _gActor206100Animation0ECA8Records,
    _gActor206100Animation0ECA8Indices,
    { NULL, _gActor206100Animation0ECA8Bank1, NULL, NULL, _gActor206100Animation0ECA8Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_206100_80158AF0[2] = {
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_actor_206100_80158B0C[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_206100_8014F428, { .model = &_gActor206100DiverBody } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_206100_8014F134, { .value = 0 } },
};

AnimationSet* D_actor_206100_80158B24[17] = {
    NULL,
    &_gActor206100Animation0B8F8,
    NULL,
    &_gActor206100Animation0BC80,
    NULL,
    &_gActor206100Animation0C544,
    NULL,
    &_gActor206100Animation0CC4C,
    &_gActor206100Animation0D530,
    NULL,
    &_gActor206100Animation0D968,
    &_gActor206100Animation0DEA8,
    NULL,
    NULL,
    &_gActor206100Animation0E4A8,
    &_gActor206100Animation0E798,
    &_gActor206100Animation0ECA8,
};

SVECTOR D_actor_206100_80158B68[8] = {
    { 0, 3000, 7600, 0 },
    { 5373, 3000, 5373, 0 },
    { 7600, 3000, 0, 0 },
    { 5373, 3000, -5373, 0 },
    { 0, 3000, -7600, 0 },
    { -5373, 3000, -5373, 0 },
    { -7600, 3000, 0, 0 },
    { -5373, 3000, 5373, 0 },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

ScreenWaveOscillator gScreenWaveColumns[13] = { 0 };

ScreenWaveOscillator gScreenWaveRows[32] = { 0 };

Actor206100Slot D_actor_206100_80158CBC[2] = { 0 };

ScreenWaveCtx D_actor_206100_80158CCC = { 0 };

static void func_actor_206100_8014DA28(Task* task);

static void func_actor_206100_8014C458(Task* task);

static void func_actor_206100_8014E7D4(Task* task);

static void func_actor_206100_8014F524(Task* task);

static void func_actor_206100_8014CFF4(Task* task);

static void func_actor_206100_8014D380(Task* task);

static void func_actor_206100_8014D6F4(Task* task);

static void func_actor_206100_8014F59C(Task* task);

static void func_actor_206100_8014F5A4(Task* task);

static void func_actor_206100_8014F5AC(Task* task);

static void func_actor_206100_8014F5B4(Task* task);

static void func_actor_206100_8014F608(Task* task);

static void func_actor_206100_8014F65C(Task* task);

static void func_actor_206100_8014F69C(Task* task);

extern TaskDesc D_80147E48;

static void            func_actor_206100_8014B698(Task* task);
static void            func_actor_206100_8014BAA8(Task* task);
static inline void     _actor206100AnimUpdate(Task* task);
static void            func_actor_206100_8014C274(Task* task);
static __inline__ void Actor206100_UpdateColor(Task* task);
static void            func_actor_206100_8014CB68(Task* task);
static __inline__ void set_state(Task* task, s32 state);
static __inline__ s16  take_request(Task* task);

#include "../../shared/screen_wave.inc.c"

#include "../../shared/diver_impact_burst.inc.c"
#include "../../shared/diver_draw_spark.inc.c"

static void func_actor_206100_8014AF74(Task* task)
{
    Actor206100Work* work;
    TmdObject*       tmd;
    Enemy*           enemy;
    GfxCoord*        coord;
    u16              hp;

    tmd                      = task->extra.tmd;
    work                     = (Actor206100Work*)task->work;
    enemy                    = (Enemy*)task->spawnArg2.pointer;
    tmd->otOffset            = 0xA;
    tmd->lightMtx            = &work->lightMtx;
    tmd->flags               = 0;
    tmd->colorMtx            = &work->colorMtx;
    coord                    = tmd->coords;
    work->eff_4C0.coord      = &task->extra.tmd->coords[1];
    work->eff_4C0.spawnArgLo = 0x580;
    work->eff_4C0.spawnArgHi = 3;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->bodyPos.vx        = 0;
    enemy->bodyPos.vy        = 0;
    enemy->bodyPos.vz        = 0;
    enemy->coord             = &task->extra.tmd->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->rec_384;
    enemy->param                  = &D_actor_206100_80155198;
    hp                            = D_actor_206100_80155198.hpMax;
    enemy->hpMax                  = hp;
    enemy->hp                     = hp;
    coord->parent                 = &gGfxViewCoord;
    animationInitContext(&work->anim, D_actor_206100_80158B24, tmd, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->animAux, work->slots);
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
static void func_actor_206100_8014B0AC(Task* task, u8 arg1)
{
    VECTOR           scale;
    GfxMatrix        rot;
    GfxMatrix        ma;
    GfxMatrix        mb;
    SVECTOR          euler;
    GfxMatrix        mc;
    Actor206100Work* work;
    GfxCoord*        base;
    GfxCoord*        c2;
    GfxCoord*        c3;
    GfxCoord*        c4;
    s32              invScale;

    base = task->extra.tmd->coords;
    work = (Actor206100Work*)task->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];

    switch (work->field_54D) {
        case 1:
            switch (work->field_53A) {
                case 0:
                    base[0].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[1].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(c4);
                    Gp_MtxToEuler(&c2->coord, &work->field_4D8);
                    Gp_MtxToEuler(&c3->coord, &work->field_4E0);
                    work->field_53A = 1;
                    work->field_53C = 0x1000;
                    /* fallthrough */
                case 1: {
                    GfxRotationWords* ir;

                    work->field_4D8.vx       = (u16)work->field_4D8.vx + ((s32) - (work->field_4D8.vx * 0x10) >> 7);
                    work->field_4D8.vy       = (u16)work->field_4D8.vy + ((s32) - (work->field_4D8.vy * 0x10) >> 7);
                    work->field_4D8.vz       = (u16)work->field_4D8.vz + ((s32) - (work->field_4D8.vz * 0x10) >> 7);
                    work->field_4E0.vx       = (u16)work->field_4E0.vx + ((s32) - (work->field_4E0.vx * 0x10) >> 7);
                    work->field_4E0.vy       = (u16)work->field_4E0.vy + ((s32) - (work->field_4E0.vy * 0x10) >> 7);
                    work->field_4E0.vz       = (u16)work->field_4E0.vz + ((s32) - (work->field_4E0.vz * 0x10) >> 7);
                    ir                       = &rot.rotationWords;
                    rot.rotationWords.m00M01 = ONE;
                    rot.rotationWords.m02M10 = 0;
                    ir->m11M12               = ONE;
                    rot.rotationWords.m20M21 = 0;
                    ir->m22                  = ONE;
                    RotMatrix(&work->field_4D8, &rot.mat);
                    func_actor_206100_8014F4B8(&rot.mat, &c2->coord);
                    rot.rotationWords.m00M01 = ONE;
                    rot.rotationWords.m02M10 = 0;
                    ir->m11M12               = ONE;
                    rot.rotationWords.m20M21 = 0;
                    ir->m22                  = ONE;
                    RotMatrix(&work->field_4E0, &rot.mat);
                    func_actor_206100_8014F4B8(&rot.mat, &c3->coord);
                    if ((abs(work->field_4D8.vx) < 0x30) && (abs(work->field_4D8.vy) < 0x30) && (abs(work->field_4D8.vz) < 0x30) &&
                        (abs(work->field_4E0.vx) < 0x30) && (abs(work->field_4E0.vy) < 0x30) && (abs(work->field_4E0.vz) < 0x30)) {
                        work->field_53A = 2;
                    }
                    c2->composeStamp = GRAPHICS_COORD_DIRTY;
                    c3->composeStamp = GRAPHICS_COORD_DIRTY;
                    c4->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(c4);
                    break;
                }
                case 2: {
                    GfxRotationWords* ia;
                    GfxRotationWords* ib;
                    GfxRotationWords* ic;
                    GfxRotationWords* ir;

                    Gp_MtxToEuler(&c4->coord, &euler);
                    work->field_53C         = (u16)work->field_53C + ((0x2AA - work->field_53C) >> 3);
                    ia                      = &ma.rotationWords;
                    ma.rotationWords.m00M01 = ONE;
                    ma.rotationWords.m02M10 = 0;
                    ia->m11M12              = ONE;
                    ma.rotationWords.m20M21 = 0;
                    ia->m22                 = ONE;
                    scale.vx                = 0x1000;
                    scale.vy                = 0x1000;
                    scale.vz                = work->field_53C;
                    ScaleMatrix(&ma.mat, &scale);
                    func_actor_206100_8014F4B8(&ma.mat, &c2->coord);
                    ib                      = &mb.rotationWords;
                    mb.rotationWords.m00M01 = ONE;
                    mb.rotationWords.m02M10 = 0;
                    ib->m11M12              = ONE;
                    mb.rotationWords.m20M21 = 0;
                    ib->m22                 = ONE;
                    scale.vx                = 0x1000;
                    scale.vy                = 0x1000;
                    scale.vz                = 0x1000;
                    ScaleMatrix(&mb.mat, &scale);
                    func_actor_206100_8014F4B8(&mb.mat, &c3->coord);
                    ic                      = &mc.rotationWords;
                    mc.rotationWords.m00M01 = ONE;
                    mc.rotationWords.m02M10 = 0;
                    ic->m11M12              = ONE;
                    mc.rotationWords.m20M21 = 0;
                    ic->m22                 = ONE;
                    scale.vx                = 0x1000;
                    scale.vy                = 0x1000;
                    invScale                = 0x1000000 / work->field_53C;
                    scale.vz                = invScale;
                    ScaleMatrix(&mc.mat, &scale);
                    ir                       = &rot.rotationWords;
                    rot.rotationWords.m00M01 = ONE;
                    rot.rotationWords.m02M10 = 0;
                    ir->m11M12               = ONE;
                    rot.rotationWords.m20M21 = 0;
                    ir->m22                  = ONE;
                    RotMatrix(&euler, &rot.mat);
                    MulMatrix(&mc.mat, &rot.mat);
                    func_actor_206100_8014F4B8(&mc.mat, &c4->coord);
                    base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                    base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(c4);
                    break;
                }
            }
            break;
        case 0:
            base[0].composeStamp = GRAPHICS_COORD_DIRTY;
            base[1].composeStamp = GRAPHICS_COORD_DIRTY;
            base[2].composeStamp = GRAPHICS_COORD_DIRTY;
            base[3].composeStamp = GRAPHICS_COORD_DIRTY;
            base[4].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(c4);
            if (work->field_53C < 0xF80) {
                GfxRotationWords* ia;
                GfxRotationWords* ib;
                GfxRotationWords* ic;
                GfxRotationWords* ir;

                Gp_MtxToEuler(&c4->coord, &euler);
                work->field_53C         = (u16)work->field_53C + ((0x1000 - work->field_53C) >> 2);
                ia                      = &ma.rotationWords;
                ma.rotationWords.m00M01 = ONE;
                ma.rotationWords.m02M10 = 0;
                ia->m11M12              = ONE;
                ma.rotationWords.m20M21 = 0;
                ia->m22                 = ONE;
                scale.vx                = 0x1000;
                scale.vy                = 0x1000;
                scale.vz                = work->field_53C;
                ScaleMatrix(&ma.mat, &scale);
                func_actor_206100_8014F4B8(&ma.mat, &c2->coord);
                ib                      = &mb.rotationWords;
                mb.rotationWords.m00M01 = ONE;
                mb.rotationWords.m02M10 = 0;
                ib->m11M12              = ONE;
                mb.rotationWords.m20M21 = 0;
                ib->m22                 = ONE;
                scale.vx                = 0x1000;
                scale.vy                = 0x1000;
                scale.vz                = 0x1000;
                ScaleMatrix(&mb.mat, &scale);
                func_actor_206100_8014F4B8(&mb.mat, &c3->coord);
                ic                      = &mc.rotationWords;
                mc.rotationWords.m00M01 = ONE;
                mc.rotationWords.m02M10 = 0;
                ic->m11M12              = ONE;
                mc.rotationWords.m20M21 = 0;
                ic->m22                 = ONE;
                scale.vx                = 0x1000;
                scale.vy                = 0x1000;
                invScale                = 0x1000000 / work->field_53C;
                scale.vz                = invScale;
                ScaleMatrix(&mc.mat, &scale);
                ir                       = &rot.rotationWords;
                rot.rotationWords.m00M01 = ONE;
                rot.rotationWords.m02M10 = 0;
                ir->m11M12               = ONE;
                rot.rotationWords.m20M21 = 0;
                ir->m22                  = ONE;
                RotMatrix(&euler, &rot.mat);
                MulMatrix(&mc.mat, &rot.mat);
                func_actor_206100_8014F4B8(&mc.mat, &c4->coord);
                base[2].composeStamp = GRAPHICS_COORD_DIRTY;
                base[3].composeStamp = GRAPHICS_COORD_DIRTY;
                base[4].composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(c4);
                work->field_53A = 0;
            } else {
                func_actor_206100_8014E228(task);
                work->field_53A = 0;
            }
            break;
    }
}
/// Latches the actor's position and picks the nearer of the two `gPlayerActorTasks`
/// actors as its walk target: it stores the standing post in `field_434` /
/// `field_438`, then measures the XZ distance to each slot from the root
/// coordinate, keeping the closer one's position in `field_4D0` / `field_4D4`
/// and its distance in `field_528`.
///
/// Two details are load-bearing.  `gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]` is read *before* the three
/// post stores: written after them the scheduler moves the whole `lui` / `lw`
/// group below the stores, which costs twelve bytes of schedule and shifts
/// every later branch target.  And the player delta `d0` is normalised and fed
/// to `ratan2` even when slot 1 was the closer one, so `field_51C` follows the
/// player's bearing rather than the target's.
static void func_actor_206100_8014B698(Task* task)
{
    Actor206100Work* work;
    GfxCoord*        coord;
    GfxCoord*        c0;
    GfxCoord*        c1;
    Task*            player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist0;
    s32              dist1;

    work            = (Actor206100Work*)task->work;
    coord           = task->extra.tmd->coords;
    player          = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->field_434 = (u16)coord->coord.t[0];
    work->field_436 = (u16)coord->coord.t[1];
    work->field_438 = (u16)coord->coord.t[2];
    if (player != NULL) {
        c0    = player->extra.tmd->coords;
        d0.vx = (u16)c0->coord.t[0] - (u16)coord->coord.t[0];
        d0.vy = (u16)c0->coord.t[1] - (u16)coord->coord.t[1];
        d0.vz = (u16)c0->coord.t[2] - (u16)coord->coord.t[2];
        dist0 = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] == NULL) {
            work->field_4D0 = c0->coord.t[0];
            work->field_4D2 = c0->coord.t[1];
            work->field_4D4 = c0->coord.t[2];
            work->field_528 = dist0;
        } else {
            c1    = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
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
/// `gSceneCombatState.actorControl` is 0 it advances the child's `field_5A` and folds `field_58` /
/// `field_5A` / `field_5C` into the root coordinate, raises `hit` when
/// either collision slot reports one of the three kinds 1/3/5 or when
/// `func_800E0C10`'s push-back says the beam is crowded, and retires the child
/// - clearing the object's draw flags, bumping the task state and switching
/// the effect kind to 2 - once `killCountdown` reaches 0x5B or the flag is up.
/// `field_64` is the scale the setup hands to `diverImpactBurst`
/// biased by 0x10002000; it ramps 0x100 a frame to 0x600 and then holds.
static void func_actor_206100_8014B8B4(Task* task)
{
    Actor206100ChildWork* child;
    GfxCoord*             coord;
    WorldCollisionDelta   delta;
    s32                   mask;
    s32                   hit;
    s32                   mode;
    s32                   i;
    s32                   n;
    s32                   v;

    hit   = 0;
    child = (Actor206100ChildWork*)task->work;
    coord = task->extra.tmd->coords;
    mode  = 1;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        child->field_5A      += 2;
        *&coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[0]    += child->field_58;
        coord->coord.t[1]    += child->field_5A;
        coord->coord.t[2]    += child->field_5C;
        if (Gp_FindRec18(child->rec, 0) != 0) {
            for (i = 0; i < 2; i++) {
                switch (child->rec[i].key.value & 0xFFFF0000) {
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
            child->obj.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            mode                = 2;
            task->state        += 1;
        }
        v = child->field_64;
        if (v < 0x600) {
            child->field_64 = v + 0x100;
        } else {
            child->field_64 = 0x600;
        }
        diverImpactBurst(coord, child->field_60, mode, child->field_64 + 0x10002000);
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
/// enemy's `hp` through `func_800E2C78` and
/// `func_800DA6E8`.  The id's low parameter then picks one of the three flag
/// setters, one of the hit reaction sizes, or clears the hit flag again, and
/// the `0x7F`/`0x8000` pair on an id ending 0x1C forces the light reaction and
/// clears bit 0 of the object's draw flags.  The `else` arm is the same record
/// arriving with the cooldown still up: id parameter 0xD sounds
/// `func_800FDB18` on the root coordinate's second part alone.
///
/// The tail turns `reactionFlags` into requests the same way -- stagger clears
/// and asks for the heavy reaction, buildup asks for the consumer's
/// sound-and-state pair, and the damage-over-time countdown applies
/// its knock-back and asks for the light one -- and every frame ends by
/// releasing the record table and counting the cooldown down, or clamping it to
/// 0 so it never goes negative.
static void func_actor_206100_8014BAA8(Task* task)
{
    Actor206100Work* work;
    Enemy*           enemy;
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
    enemy           = (Enemy*)task->spawnArg2.pointer;
    work->field_52A = 0;
    for (i = 0; i < 6; i++) {
        if ((work->rec_384[i].key.value & 0xFFFF0000) == 0x20000) {
            if (work->field_504 == 0) {
                work->field_52A = hit;
                work->field_54B = hit;
                dmg             = Gp_ComputeDamage(work->rec_384[i].key.value, work->field_528, 0, 0);
                amount          = dmg;
                work->field_504 = Gp_GetIdParam2(work->rec_384[i].key.value);
                if (Gp_RollEnemyChance(enemy, work->rec_384[i].key.value, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->rec_384[i].key.value) & 0xFFFF,
                              &task->extra.tmd->coords[work->field_557], 0, &work->eff_4C0);
                if (amount >= 0xB4) {
                    work->field_52C = heavy;
                } else {
                    work->field_52C = hit;
                }
                switch (Gp_GetIdParam0(work->rec_384[i].key.value) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(enemy);
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->rec_384[i].key.value, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(enemy, work->rec_384[i].key.value, 0);
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
                if ((work->rec_384[i].key.value & 0x7F) == 0x1C && (work->rec_384[i].key.value & 0x8000) == 0) {
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->field_52C       = hit;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->field_557], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->field_557], 2, 0);
                        break;
                }
                func_800E2C78(enemy, work->rec_384[i].key.value, amount, 0);
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if ((s16)enemy->hp < 0) {
                    enemy->hp = 0;
                }
            } else if ((Gp_GetIdParam1(work->rec_384[i].key.value) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &task->extra.tmd->coords[1], 0, &work->eff_4C0);
            }
        }
        if (work->field_52A != 0) {
            break;
        }
    }
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->field_52C       = 2;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_52C       = 3;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
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
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
    Gp_ClearRec18Occupied(work->rec_384);
    if (work->field_504 > 0) {
        work->field_504--;
    } else {
        work->field_504 = 0;
    }
}
#include "../../shared/diver_inlines.inc.c"

#include "../../shared/diver_turn_joint.inc.c"

/// Services the animation request in the work block and steps animation slots
/// 1 through 0xE once.  `animRequest` holds the request kind: kind 1 zeroes the
/// clip phase `field_512` when the clip playing (`animPlaying`) is not the
/// requested one (`animClip`), otherwise hands it to
/// `func_actor_206100_8014F3C8`, and then calls `func_actor_206100_8014F2F0`;
/// kind 2 calls `diverRestartClip` and zeroes the phase.  Both leave
/// kind 3, which advances the phase by one each call.
static inline void _actor206100AnimUpdate(Task* task)
{
    Actor206100Work* work;
    s16              kind;
    s32              i;

    work = (Actor206100Work*)task->work;
    kind = work->animRequest;
    if (kind == 1) {
        if (work->animPlaying != work->animClip) {
            work->field_512 = 0;
        } else {
            work->field_512 = func_actor_206100_8014F3C8(task, work->field_512);
        }
        func_actor_206100_8014F2F0(task);
        work->animRequest = 3;
    } else if (kind == 2) {
        diverRestartClip(task);
        work->animRequest = 3;
        work->field_512   = 0;
    } else if (kind == 3) {
        work->field_512 = work->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        animationTickSlot(&work->anim, i);
    }
}

/// Spawn state of `D_actor_206100_80149E94`: builds the actor's work block --
/// `memCalloc(0x558, 0)` parked straight in `Task::work`, the actor destroyed
/// if that fails -- empties both companion slots of `D_actor_206100_80158CBC`
/// through their index (the walked-pointer form gives the timer field an
/// induction variable of its own) and calls the setup `func_actor_206100_8014AF74`
/// with the block in place.
///
/// It then requests the first clip -- kind 2 in `animRequest`, `animClip` as the
/// clip and `animStep` the step scale -- and services the request at once.
///
/// The tail seeds the walk/HP scales (`field_508`, `field_50A`, `field_526` and
/// `field_53E`), zeroes the root coordinate's translation, takes the state-0
/// reference `Gp_IncStateF0Ref` and re-arms the actor in state 1 with the state
/// and sub-state indices cleared -- the two index pairs written through the two
/// fresh `Task::work` loads, the block-local store shape `func_actor_206100_8014CE60`
/// uses.
static void func_actor_206100_8014C274(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* req;
    Actor206100Work* state;
    Actor206100Work* tail;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              i;

    enemy      = task->spawnArg2.pointer;
    coord      = task->extra.tmd->coords;
    task->work = memCalloc(0x558, 0);
    work       = (Actor206100Work*)task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    D_neo_ark_submarine_gallery_801818B8 = 1;
    work->field_536                      = (u16)D_neo_ark_submarine_gallery_80181A48.height;
    for (i = 0; i < 2; i++) {
        D_actor_206100_80158CBC[i].enemy = NULL;
        D_actor_206100_80158CBC[i].timer = 0;
    }
    func_actor_206100_8014AF74(task);
    req              = (Actor206100Work*)task->work;
    req->animStep    = 0x10;
    req->animClip    = 3;
    req->animRequest = 2;
    _actor206100AnimUpdate(task);
    work->field_508   = 0x1000;
    work->field_50A   = 0x1000;
    work->field_53E   = 0x1EAA;
    work->field_54D   = 1;
    coord->coord.t[0] = 0;
    work->field_526   = 0x2710;
    coord->coord.t[1] = 0x2710;
    coord->coord.t[2] = 0;
    (Gp_IncStateF0Ref)(0);
    state            = (Actor206100Work*)task->work;
    task->state      = 1;
    state->field_520 = 0;
    state->subState  = 0;
    tail             = (Actor206100Work*)task->work;
    tail->field_520  = 0;
    tail->subState   = 0;
}

/// The actor's five top-level states, dispatched on `Task::state` by its task
/// callback `func_actor_206100_8014F428`: `func_actor_206100_8014C274` (which
/// builds the work block), `func_actor_206100_8014DA28`,
/// `func_actor_206100_8014C458`, `func_actor_206100_8014E7D4` and the exit
/// `func_actor_206100_8014F490`.
static const TaskFuncTable5 D_actor_206100_80149E5C = {
    {
        func_actor_206100_8014C274,
        func_actor_206100_8014DA28,
        func_actor_206100_8014C458,
        func_actor_206100_8014E7D4,
        func_actor_206100_8014F490,
    },
};

static const TaskFuncTable9 D_actor_206100_80149E70 = {
    {
        func_actor_206100_8014F524,
        func_actor_206100_8014CFF4,
        func_actor_206100_8014D380,
        func_actor_206100_8014D6F4,
        func_actor_206100_8014F59C,
        func_actor_206100_8014F5A4,
        func_actor_206100_8014F5AC,
        func_actor_206100_8014F5B4,
        func_actor_206100_8014F608,
    },
};

/// Push the model's second coordinate's world position onto the scratch stack
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
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(task->spawnArg2.pointer, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Updates the active enemy, emits its beam, and gates lock-on by its height
/// after transforming the selected model part through the parent chain.
static void func_actor_206100_8014C458(Task* task)
{
    Actor206100Work*       work   = (Actor206100Work*)task->work;
    GfxCoord*              coord  = task->extra.tmd->coords;
    TmdObject*             obj    = task->extra.tmd;
    Enemy*                 enemy  = (Enemy*)task->spawnArg2.pointer;
    TaskFuncTable9         states = D_actor_206100_80149E70;
    Actor206100VecScratch  scratch;
    VECTOR                 scale;
    GfxMatrix              scaling;
    Actor206100Work*       next;
    Actor206100Work*       dying;
    Actor206100Work*       sub;
    Actor206100Work*       pose;
    Actor206100Work*       last;
    Actor206100Work*       anim;
    MATRIX*                dest;
    GfxCoord*              scaled;
    GfxCoord*              destcoord;
    GfxCoord*              child;
    GfxCoord*              walk;
    GfxCoord*              root;
    Enemy*                 end;
    Task*                  spawn;
    Actor206100ChildWork*  beam;
    Actor206100VecScratch* mtx;
    MATRIX*                mtx2;
    s32                    i;
    s32                    sound;
    s32                    pan;
    s32                    flag;
    s16                    state;
    SVECTOR*               launch;
    SVECTOR*               svp;
    SVECTOR*               out;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount = work->frameCount + 1;
            work->field_518  = work->field_518 + 1;
            func_actor_206100_8014B698(task);
            states.funcs[(s16)work->field_520](task);
            sub = (Actor206100Work*)task->work;
            if ((s16)sub->field_52E != 0) {
                if (((u16)sub->field_52E & 7) == 0) {
                    sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4004000B;
                    pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                    SndEvt_EnqueueType6(
                        sound, pan,
                        (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                }
                if ((s16)sub->field_52E == 0x18 || (s16)sub->field_52E == 0x30) {
                    func_800FDB18(7, task->extra.tmd->coords + 1, NULL, &sub->eff_4C0);
                }
                sub->field_52E = (s16)((u16)sub->field_52E - 1);
            }
            next = (Actor206100Work*)task->work;
            if ((s16)next->field_534 != 0) {
                next->field_534 = (s16)((u16)next->field_534 - 1);
            }
            anim  = (Actor206100Work*)task->work;
            state = anim->animRequest;
            if (state == 1) {
                if (anim->animPlaying != anim->animClip) {
                    anim->field_512 = 0;
                } else {
                    anim->field_512 = func_actor_206100_8014F3C8(task, anim->field_512);
                }
                func_actor_206100_8014F2F0(task);
                anim->animRequest = 3;
            } else if (state == 2) {
                diverRestartClip(task);
                anim->animRequest = 3;
                anim->field_512   = 0;
            } else if (state == 3) {
                anim->field_512 = anim->field_512 + 1;
            }
            for (i = 1; i < 0xF; i++) {
                animationTickSlot(&anim->anim, i);
            }
            work->animStatus = work->slots[1].status.fields.flags;
            func_actor_206100_8014B0AC(task, work->field_54D);
            func_actor_206100_8014E0C0(task);
            func_actor_206100_8014EC54(task);
            func_actor_206100_8014EB60(task);
            destcoord                           = task->extra.tmd->coords;
            pose                                = (Actor206100Work*)task->work;
            mtx                                 = &scratch;
            scratch.matrix.rotationWords.m00M01 = ONE;
            scratch.matrix.rotationWords.m02M10 = 0;
            MATRIX_PAIR(&mtx->matrix.mat, 1, 1) = 0x1000;
            scratch.matrix.rotationWords.m20M21 = 0;
            mtx->matrix.mat.m[2][2]             = 0x1000;
            RotMatrixZ(pose->field_440, &mtx->matrix.mat);
            RotMatrixY(pose->field_43E, &mtx->matrix.mat);
            dest                         = &destcoord->coord;
            dest->m[0][0]                = scratch.matrix.mat.m[0][0];
            dest->m[0][1]                = scratch.matrix.mat.m[0][1];
            dest->m[0][2]                = scratch.matrix.mat.m[0][2];
            dest->m[1][0]                = scratch.matrix.mat.m[1][0];
            dest->m[1][1]                = scratch.matrix.mat.m[1][1];
            dest->m[1][2]                = scratch.matrix.mat.m[1][2];
            dest->m[2][0]                = scratch.matrix.mat.m[2][0];
            dest->m[2][1]                = scratch.matrix.mat.m[2][1];
            dest->m[2][2]                = scratch.matrix.mat.m[2][2];
            destcoord->composeStamp      = GRAPHICS_COORD_DIRTY;
            scaled                       = task->extra.tmd->coords;
            scale.vx                     = work->field_53E;
            scale.vy                     = scale.vx;
            scale.vz                     = scale.vx;
            mtx2                         = &scaling.mat;
            scaling.rotationWords.m00M01 = ONE;
            scaling.rotationWords.m02M10 = 0;
            MATRIX_PAIR(mtx2, 1, 1)      = 0x1000;
            scaling.rotationWords.m20M21 = 0;
            mtx2->m[2][2]                = 0x1000;
            ScaleMatrix(&scaling.mat, &scale);
            MulMatrix(&scaled->coord, &scaling.mat);
            func_actor_206100_8014BAA8(task);
            if (work->field_555 != 0) {
                root  = &task->extra.tmd->coords[4];
                spawn = Task_SpawnFromTable(D_actor_206100_80158B0C, 1, 0, 0);
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
                        coordLocalToWorld(root, &scratch.gte.vec);
                        coordLocalToWorld(root, &scratch.gte.m.alt);
                        spawn->work        = beam;
                        child              = spawn->extra.tmd->coords;
                        launch             = &scratch.gte.out;
                        scratch.gte.out.vx = 0;
                        scratch.gte.out.vy = 0;
                        launch->vz         = 0x15E;
                        coordLocalToWorld(root, launch);
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
                dying->subState  = 0;
            }
            coord->coord.t[1] =
                coord->coord.t[1] + (((s16)work->field_526 - coord->coord.t[1]) >> 4);
            enemy->coord = &task->extra.tmd->coords[work->field_557];
        case SCENE_COMBAT_ACTORS_PAUSED:
            Actor206100_UpdateColor(task);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    svp                = &scratch.gte.vec;
    out                = &scratch.gte.out;
    last               = (Actor206100Work*)task->work;
    walk               = &task->extra.tmd->coords[work->field_557];
    end                = (Enemy*)task->spawnArg2.pointer;
    scratch.gte.out.vx = 0;
    scratch.gte.out.vy = 0;
    scratch.gte.out.vz = 0;
    scratch.gte.vec.vx = 0;
    scratch.gte.vec.vy = 0;
    scratch.gte.vec.vz = 0;
    while (1) {
        if (walk->parent == NULL)
            break;
        if (walk != &gGfxViewCoord) {
            gte_SetTransMatrix(&walk->coord);
            gte_SetRotMatrix(&walk->coord);
            gte_ldv0(svp);
            gte_rtv0tr();
            gte_stlvnl(&scratch.gte.m.mac);
            gte_stflg(&flag);
            scratch.gte.vec.vx = scratch.gte.m.mac.vx;
            scratch.gte.vec.vy = scratch.gte.m.mac.vy;
            scratch.gte.vec.vz = scratch.gte.m.mac.vz;
            walk               = walk->parent;
            continue;
        }
        out->vx = scratch.gte.vec.vx;
        out->vy = scratch.gte.vec.vy;
        out->vz = scratch.gte.vec.vz;
        break;
    }
    if ((s16)last->field_536 + 0x190 < scratch.gte.out.vy) {
        end->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else {
        end->node.state.parts.flags = 0;
    }
}

/// Teleport: ramps the white-out `func_actor_206100_8014DEAC` fades with, and
/// once it is fully up, hands slot 3 the actor's new position and re-arms the
/// actor on the far side, spawning the screen tint `D_actor_206100_80158CCC`
/// describes as it goes.
///
/// `modulateTexture` is written between `r` and `g`, not in declaration
/// order, and that is load-bearing: it shares the constant 1 with `span`, and
/// that constant and the `%hi` of the global's own address tie in
/// `local-alloc`'s `QTY_CMP_PRI`
/// (`floor_log2 (n_refs) * n_refs * size / span`). The address only wins
/// that tie while the constant's live range runs the whole store run. Cutting
/// it short is what puts the constant in `$v1` and the `%hi` in `$t0`; with
/// `modulateTexture` written last the two swap and the tail no longer
/// schedules the same way.
static void func_actor_206100_8014CB68(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* work2;
    TmdObject*       tmd;
    GfxCoord*        coord;
    ActorTransform   msg;

    work  = (Actor206100Work*)task->work;
    tmd   = task->extra.tmd;
    coord = tmd->coords;
    func_actor_206100_8014DEAC(task);
    work->field_51E = work->field_51E + 6;
    if ((s16)work->field_51E >= 0x100) {
        work->field_51E = 0xFF;
    }
    Fade_DrawOverlay((u8)work->field_51E, (u8)work->field_51E, (u8)work->field_51E, GPU_BLEND_SUBTRACT);
    if ((s16)work->field_51E == 0xFF) {
        work->obj_364.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj_414.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        msg.pos.vx           = 0x690;
        msg.pos.vy           = 0x1388;
        msg.pos.vz           = 0x898;
        msg.rot.vx           = 0;
        msg.rot.vy           = 0x200;
        msg.rot.vz           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &msg, 0);
        work2                                                      = (Actor206100Work*)task->work;
        work2->animStep                                            = 0x10;
        work2->animClip                                            = 3;
        work2->animRequest                                         = 2;
        coord->coord.t[1]                                          = 0xDAC;
        work->field_526                                            = 0xDAC;
        coord->coord.t[0]                                          = 0x157C;
        coord->coord.t[2]                                          = 0x157C;
        work->field_43C                                            = 0;
        work->field_43E                                            = 0xA00;
        work->field_440                                            = 0;
        work->field_553                                            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 7;
        work->field_51E                                            = 0;
        work->field_54D                                            = 0;
        work->subState                                             = work->subState + 1;
        D_actor_206100_80158CCC.span                               = 1;
        D_actor_206100_80158CCC.scale                              = 0x60;
        D_actor_206100_80158CCC.r                                  = 0x40;
        D_actor_206100_80158CCC.modulateTexture                    = SCREEN_WAVE_MODULATE_TEXTURE;
        D_actor_206100_80158CCC.g                                  = 0x80;
        D_actor_206100_80158CCC.b                                  = 0x80;
        work->field_4F8                                            = Task_SpawnFromTable(D_actor_206100_80158AF0, 0, 0, &D_actor_206100_80158CCC);
    }
}
static void func_actor_206100_8014CD08(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* work2;
    TmdObject*       tmd;
    GfxCoord*        coord;
    ActorTransform   msg;

    work            = (Actor206100Work*)task->work;
    tmd             = task->extra.tmd;
    coord           = tmd->coords;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 3) {
        SndEvt_EnqueueType6(SOUND_NEO_ARK_SUB_GALLERY_DIVER_DEPART, 0, 0);
    }
    if ((s16)work->field_51E == 0x22) {
        coord->coord.t[0]                                          = 0;
        coord->coord.t[2]                                          = 0;
        work->field_526                                            = 0x1388;
        work->field_51E                                            = 0U;
        coord->coord.t[1]                                          = 0x1B58;
        work->field_43E                                            = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
        Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        Gp_MsgPlayer3F3(0);
        msg.pos.vx = 0x690;
        msg.pos.vy = 0x1388;
        msg.pos.vz = 0x898;
        msg.rot.vx = 0;
        msg.rot.vy = 0xA00;
        msg.rot.vz = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &msg, 0);
        work2              = (Actor206100Work*)task->work;
        work2->animStep    = 0x10;
        work2->animClip    = 1;
        work2->animRequest = 2;
        work->subState     = work->subState + 1;
        return;
    }
    work->field_526 = (u16)(work->field_526 + ((s32)(0x1D4C - (s16)work->field_526) >> 2));
    diverStepForward(task, 0x30, work->field_43E);
}
/// State handler 4 of `D_actor_206100_80149E94`: clears the fixed-address
/// `D_neo_ark_submarine_gallery_801818B8` flag, ticks the per-state counter `field_51E` and seeds
/// `D_actor_206100_80158CCC.state` to `SCREEN_WAVE_RAMP_FINISHED` on its first frame.
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
static void func_actor_206100_8014CE60(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GfxCoord*        coord;
    GfxCoord*        ring;
    SVECTOR          vec;
    s32              i;
    s16              y;
    s16              frame;

    work                                 = (Actor206100Work*)task->work;
    D_neo_ark_submarine_gallery_801818B8 = 0;
    coord                                = task->extra.tmd->coords;
    work->field_51E                      = work->field_51E + 1;
    if ((s16)work->field_51E == 1) {
        D_actor_206100_80158CCC.state = SCREEN_WAVE_RAMP_FINISHED;
    }
    frame = (s16)work->field_51E;
    if (frame == 3) {
        if (work->field_4F8 != NULL) {
            taskKill(work->field_4F8);
        }
        if ((s16)work->field_51E == frame) {
            SndEvt_EnqueueType6(SOUND_NEO_ARK_SUB_GALLERY_DIVER_REAPPEAR, 0, 0);
        }
    }
    if ((s16)work->field_51E == 0xC) {
        y    = -0x294;
        i    = 0;
        ring = task->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(gRoomEffectWaterSprayId, ring, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if ((s16)work->field_51E == 0x46) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
        coord->coord.t[0]                                          = 0;
        coord->coord.t[2]                                          = 0;
        next                                                       = (Actor206100Work*)task->work;
        next->field_520                                            = 1;
        next->subState                                             = 0;
    }
}
/// Clears `subState` and hands `field_520` the new state, reloading the work
/// block through the task rather than taking the caller's pointer: the fresh
/// load is what makes `state` a block-local quantity, which is what lets
/// local-alloc hand it `$v0` (see `take_request` below).
static __inline__ void set_state(Task* task, s32 state)
{
    Actor206100Work* next = (Actor206100Work*)task->work;

    next->field_520 = state;
    next->subState  = 0;
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
                SndEvt_EnqueueType7(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, 1);
                work->field_52C = 0;
                set_state(task, 8);
                return 1;
            case 4:
                SndEvt_EnqueueType7(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, 1);
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
static void func_actor_206100_8014CFF4(Task* task)
{
    Actor206100Work* sub                 = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F6F8,
        func_actor_206100_8014D14C,
    };

    if (take_request(task) == 0) {
        states[(s16)sub->subState](task);
    }
}
/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks the per-state
/// counter `field_51E` and arms `field_52E` with 0x18 on the first frame, eases
/// `field_35C` toward 0x4000 by a quarter of the remaining distance over frames
/// 0x29..0x4D, fires the overlay's sound events -- 0x551E0002 panned through
/// `worldCoordGetOriginAudioPan` / `worldCoordGetOriginAudioDepth` at 0x54 and plain at 0x77 -- flags the six
/// cue frames, hands state 2 to the actor at sub-state 0 when its `animStatus`
/// says the clip ended, and folds the heading onto the vector from the root coordinate to
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
static void func_actor_206100_8014D14C(Task* task)
{
    Actor206100Work* sub = (Actor206100Work*)task->work;
    Actor206100Work* work;
    Actor206100Work* next;
    GfxCoord*        coord;
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
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, pan,
                            (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if ((s16)sub->field_51E == 0x77) {
        SndEvt_EnqueueType7(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, 1);
    }
    if ((s16)sub->field_51E == 0x54 || (s16)sub->field_51E == 0x5B || (s16)sub->field_51E == 0x62 ||
        (s16)sub->field_51E == 0x69 || (s16)sub->field_51E == 0x70 || (s16)sub->field_51E == 0x77) {
        sub->field_555 = 1;
    }
    next = (Actor206100Work*)task->work;
    if ((next->animStatus & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (next->animStatus & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (next->animStatus & ANIMATION_SLOT_SETTLED)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond != 0) {
        SndEvt_EnqueueType7(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, 1);
        set_state(task, 2);
    }
    work                = (Actor206100Work*)task->work;
    coord               = task->extra.tmd->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    vec.vx              = sub->field_4D0 - (u16)coord->coord.t[0];
    vec.vy              = 0;
    vec.vz              = sub->field_4D4 - (u16)coord->coord.t[2];
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

/// The five sub-state handlers `func_actor_206100_8014F524` picks between: it
/// copies the table onto its stack and calls `funcs[(s16)subState]`.
static const TaskFuncTable5 D_actor_206100_80149E94 = {
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
static const TaskFuncTable3 D_actor_206100_80149EA8 = {
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
static const TaskFuncTable3 D_actor_206100_80149EB4 = {
    {
        func_actor_206100_8014F7B4,
        func_actor_206100_8014D8E8,
        func_actor_206100_8014F878,
    },
};

/// The last object in this unit's `.rodata`, one object after
/// `D_actor_206100_80149EB4` and flush against the unit's first code address:
/// the four handlers `func_actor_206100_8014E7D4` dispatches between. They
/// ignore the table pointer supplied as their second argument.
static const Actor206100StateTable4 D_actor_206100_80149EC0 = {
    {
        func_actor_206100_8014FBE4,
        func_actor_206100_8014FCD4,
        func_actor_206100_8014E964,
        func_actor_206100_8014FDE8,
    },
};

/// State handler of `D_actor_206100_80149E94`: consumes a pending sub-state
/// request through the inlined `take_request`, and when there was none runs the
/// current sub-state handler from `D_actor_206100_80149EA8` and then steers the
/// actor along its heading.
///
/// The steering half is the same fold `ActorsShared80139c00` makes: clear the
/// root coordinate's `composeStamp`, take the XZ vector from the coordinate to the walk
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
static void func_actor_206100_8014D380(Task* task)
{
    Actor206100Work* sub    = (Actor206100Work*)task->work;
    TaskFuncTable3   states = D_actor_206100_80149EA8;
    Actor206100Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    if (take_request(task) == 0) {
        states.funcs[(s16)sub->subState](task);
        work                = (Actor206100Work*)task->work;
        coord               = task->extra.tmd->coords;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        vec.vx              = sub->field_4D0 - (u16)coord->coord.t[0];
        vec.vy              = 0;
        vec.vz              = sub->field_4D4 - (u16)coord->coord.t[2];
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
static void func_actor_206100_8014D574(Task* task)
{
    Actor206100Work* work;
    GfxCoord*        coord;
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
        coord = task->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(gRoomEffectWaterSprayId, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
        sound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x551E0006;
        pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->field_51E = 0;
        work->field_526 = 0x1E78;
        work->subState  = work->subState + 1;
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
static void func_actor_206100_8014D6F4(Task* task)
{
    Actor206100Work* sub    = (Actor206100Work*)task->work;
    TaskFuncTable3   states = D_actor_206100_80149EB4;
    Actor206100Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    if (take_request(task) == 0) {
        states.funcs[(s16)sub->subState](task);
        work                = (Actor206100Work*)task->work;
        coord               = task->extra.tmd->coords;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        vec.vx              = sub->field_4D0 - (u16)coord->coord.t[0];
        vec.vy              = 0;
        vec.vz              = sub->field_4D4 - (u16)coord->coord.t[2];
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
/// From frame 0x1F on it draws from `gRandomLcgState`: the one-in-four that lands
/// on `(state >> 16) & 3 == 0` hands state 2 to the teleport
/// `func_actor_206100_8014CB68` at sub-state 0 -- so the actor leaves the
/// scene it is exploding in -- and the rest restart the counter and advance
/// to the next sub-state.  The state change goes through the inlined
/// `set_state`, which reloads `task->work` instead of reusing `work`: that
/// fresh load is what keeps the pointer a block-local quantity, exactly as in
/// `take_request`.
static void func_actor_206100_8014D8E8(Task* task)
{
    Actor206100Work* work;
    GfxCoord*        coord;
    SVECTOR          vec;
    s32              i;
    s16              y;

    work            = (Actor206100Work*)task->work;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 7) {
        y     = -0x3E8;
        i     = 0;
        coord = task->extra.tmd->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(gRoomEffectWaterSprayId, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if ((s16)work->field_51E >= 0x1F) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 0x10) & 3) == 0) {
            set_state(task, 2);
            return;
        }
        work->field_51E = 0;
        work->subState  = work->subState + 1;
    }
}
/// State-2 tick: the `gSceneCombatState.actorControl` effect mode 0 arm bumps the actor's two frame
/// counters and runs the handler `funcs[(s16)field_520]` picks out of a
/// two-entry local table, then drives the animation request and re-poses the
/// actor; mode 1 is that tail alone and mode 2 excludes the model from active
/// drawing. The table's entries are the ring stepper
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
/// for word: the same `animRequest` re-arm / ramp / reset chain over a second
/// `task->work` load, and the same `for (i = 1; i < 0xF; i++)` slot tick whose
/// initialiser sits *after* the chain for the reason
/// `func_actor_206100_8014FCD4` documents.  The chain is the only reader of
/// `next`; everything else stays on `work`, which is why the two loads exist.
///
/// Both pose matrices write five words, and which of them land in a register is
/// load-bearing.  `matrix.rotationWords.*` names the union's word view, so those three
/// stores stay frame-relative, while `MATRIX_PAIR(mtx, 1, 1)` and `mtx->m[2][2]`
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
static void func_actor_206100_8014DA28(Task* task)
{
    Actor206100Work* work               = (Actor206100Work*)task->work;
    TmdObject*       obj                = task->extra.tmd;
    void             (*funcs[2])(Task*) = {
        func_actor_206100_8014FAE4,
        func_actor_206100_8014DD3C,
    };
    Actor206100Work* next;
    Actor206100Work* sub;
    GfxCoord*        coord;
    GfxCoord*        scaled;
    MATRIX*          mtx;
    MATRIX*          mtx2;
    MATRIX*          dest;
    GfxMatrix        matrix;
    VECTOR           scale;
    GfxMatrix        scaling;
    s32              i;
    s16              state;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount = work->frameCount + 1;
            work->field_518  = work->field_518 + 1;
            funcs[(s16)work->field_520](task);
            next  = (Actor206100Work*)task->work;
            state = next->animRequest;
            if (state == 1) {
                if (next->animPlaying != next->animClip) {
                    next->field_512 = 0;
                } else {
                    next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
                }
                func_actor_206100_8014F2F0(task);
                next->animRequest = 3;
            } else if (state == 2) {
                diverRestartClip(task);
                next->animRequest = 3;
                next->field_512   = 0;
            } else if (state == 3) {
                next->field_512 = next->field_512 + 1;
            }
            for (i = 1; i < 0xF; i++) {
                animationTickSlot(&next->anim, i);
            }
            work->animStatus = work->slots[1].status.fields.flags;
            func_actor_206100_8014B0AC(task, work->field_54D);
            coord                       = task->extra.tmd->coords;
            sub                         = (Actor206100Work*)task->work;
            mtx                         = &matrix.mat;
            matrix.rotationWords.m00M01 = ONE;
            matrix.rotationWords.m02M10 = 0;
            MATRIX_PAIR(mtx, 1, 1)      = 0x1000;
            matrix.rotationWords.m20M21 = 0;
            mtx->m[2][2]                = 0x1000;
            RotMatrixZ(sub->field_440, &matrix.mat);
            RotMatrixY(sub->field_43E, &matrix.mat);
            dest                         = &coord->coord;
            dest->m[0][0]                = matrix.mat.m[0][0];
            dest->m[0][1]                = matrix.mat.m[0][1];
            dest->m[0][2]                = matrix.mat.m[0][2];
            dest->m[1][0]                = matrix.mat.m[1][0];
            dest->m[1][1]                = matrix.mat.m[1][1];
            dest->m[1][2]                = matrix.mat.m[1][2];
            dest->m[2][0]                = matrix.mat.m[2][0];
            dest->m[2][1]                = matrix.mat.m[2][1];
            dest->m[2][2]                = matrix.mat.m[2][2];
            coord->composeStamp          = GRAPHICS_COORD_DIRTY;
            scaled                       = task->extra.tmd->coords;
            scale.vx                     = work->field_53E;
            scale.vy                     = scale.vx;
            scale.vz                     = scale.vx;
            mtx2                         = &scaling.mat;
            scaling.rotationWords.m00M01 = ONE;
            scaling.rotationWords.m02M10 = 0;
            MATRIX_PAIR(mtx2, 1, 1)      = 0x1000;
            scaling.rotationWords.m20M21 = 0;
            mtx2->m[2][2]                = 0x1000;
            ScaleMatrix(&scaling.mat, &scale);
            MulMatrix(&scaled->coord, &scaling.mat);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            Actor206100_UpdateColor(task);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
static void func_actor_206100_8014DD3C(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    Enemy*           enemy;
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
                next->subState  = 0;
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
/// and hands the actor to `diverStepForward` for a 0x40 step.
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
static void func_actor_206100_8014DEAC(Task* task)
{
    Actor206100Work* sub = (Actor206100Work*)task->work;
    Actor206100Work* work;
    GfxCoord*        coord;
    GfxCoord*        coord2;
    SVECTOR*         ring;
    u32              index;
    SVECTOR          delta;
    SVECTOR          vec;
    u16              roll;
    u8               count;
    s32              angle;
    s32              yaw;
    s32              diff;
    s32              limit;

    coord = task->extra.tmd->coords;
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
    delta.vx       = sub->field_4F4[sub->field_548].vx - (u16)coord->coord.t[0];
    delta.vy       = sub->field_4F4[sub->field_548].vy - (u16)coord->coord.t[1];
    delta.vz       = sub->field_4F4[sub->field_548].vz - (u16)coord->coord.t[2];
    sub->field_526 = sub->field_4F4[sub->field_548].vy;
    if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) < 0x3E8) {
        sub->field_548 = (sub->field_548 + 1) & 7;
        count          = sub->field_54F + 1;
        sub->field_54F = count;
        if (count >= 6) {
            sub->field_54F = 0;
        }
    } else {
        ring                 = sub->field_4F4;
        index                = sub->field_548;
        work                 = (Actor206100Work*)task->work;
        coord2               = task->extra.tmd->coords;
        coord2->composeStamp = GRAPHICS_COORD_DIRTY;
        vec.vx               = ring[index].vx - (u16)coord2->coord.t[0];
        vec.vy               = 0;
        vec.vz               = ring[index].vz - (u16)coord2->coord.t[2];
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
        diverStepForward(task, 0x40, sub->field_43E);
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
static void func_actor_206100_8014E0C0(Task* task)
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
            sound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
            pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
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
static void func_actor_206100_8014E228(Task* task)
{
    SVECTOR           ang;
    SVECTOR*          aim;
    SVECTOR           rot1;
    SVECTOR           rot2;
    MATRIX            t1;
    MATRIX            t2;
    MATRIX            t3;
    GfxMatrix         ma;
    GfxMatrix         mb;
    GfxMatrix         mc;
    MATRIX            view;
    VECTOR            delta;
    VECTOR            local;
    GfxCoord*         c1;
    GfxCoord*         c3;
    GfxCoord*         c4;
    Actor206100Work*  work;
    GfxCoord*         base;
    GfxCoord*         c2;
    GfxRotationWords* ia;
    GfxRotationWords* ib;
    GfxRotationWords* ic;
    MATRIX*           m2;
    MATRIX*           m3;
    MATRIX*           dest;
    s32               hx;
    s32               hy;
    s32               hz;
    s32               total;
    u16               yaw;
    u16               pitch;
    u32               pitchDiff;
    s16               limit;

    base = task->extra.tmd->coords;
    work = (Actor206100Work*)task->work;
    c1   = &base[1];
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &c4->workm, &view);
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
    ia = &ma.rotationWords;
    ib = &mb.rotationWords;
    ic = &mc.rotationWords;

    ma.rotationWords.m00M01 = ONE;
    ma.rotationWords.m02M10 = 0;
    ia->m11M12              = ONE;
    ma.rotationWords.m20M21 = 0;
    ia->m22                 = ONE;
    mb.rotationWords.m00M01 = ONE;
    mb.rotationWords.m02M10 = 0;
    ib->m11M12              = ONE;
    mb.rotationWords.m20M21 = 0;
    ib->m22                 = ONE;
    mc.rotationWords.m00M01 = ONE;
    mc.rotationWords.m02M10 = 0;
    ic->m11M12              = ONE;
    mc.rotationWords.m20M21 = 0;
    ic->m22                 = ONE;

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
    diverTurnJoint(c2, (s16)(u16)work->field_35E / 3);
    diverTurnJoint(c3, (s16)(u16)work->field_35E / 3);

    mc.rotationWords.m00M01 = ONE;
    mc.rotationWords.m02M10 = 0;
    ic->m11M12              = ONE;
    mc.rotationWords.m20M21 = 0;
    ic->m22                 = ONE;

    total = (s16)work->field_35C + work->field_542;
    limit = total;
    if (limit >= 0x300) {
        limit = 0x300;
    } else if (limit < -0x200) {
        limit = -0x200;
    }
    RotMatrixX((s32)limit, &mc.mat);
    RotMatrixY((s16)((s16)(u16)work->field_35E / 3), &mc.mat);
    TransposeMatrix(&c1->coord, &t1);
    TransposeMatrix(&c2->coord, &t2);
    TransposeMatrix(&c3->coord, &t3);
    MulMatrix(&t1, &t2);
    MulMatrix(&t1, &t3);
    MulMatrix(&t1, &mc.mat);
    dest             = &c4->coord;
    dest->m[0][0]    = t1.m[0][0];
    dest->m[0][1]    = t1.m[0][1];
    dest->m[0][2]    = t1.m[0][2];
    dest->m[1][0]    = t1.m[1][0];
    dest->m[1][1]    = t1.m[1][1];
    dest->m[1][2]    = t1.m[1][2];
    dest->m[2][0]    = t1.m[2][0];
    dest->m[2][1]    = t1.m[2][1];
    dest->m[2][2]    = t1.m[2][2];
    c4->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(c4);
}

/// Effect-mode tick of the `field_520` state table `D_actor_206100_80149EC0`,
/// keyed on `gSceneCombatState.actorControl`. Mode 2 only excludes the model from active drawing and
/// leaves; mode 0 runs the handler `field_520` selects, latches the animation
/// slot's flags into `animStatus` and eases the root coordinate -- x and z to a
/// sixteenth of their distance to zero, y the same fraction of the way to the
/// height `field_526` -- before falling into the shared tail; mode 1 is that
/// tail alone.
///
/// The table is copied onto the stack first, the same local jump table
/// `func_actor_206100_8014F524` builds, which is what the prologue's four-word
/// block move out of `.rodata` is.  The tail is `Actor206100_UpdateColor`; see
/// there for why it stays inline.
static void func_actor_206100_8014E7D4(Task* task)
{
    Actor206100Work*       work;
    TmdObject*             obj;
    GfxCoord*              coord;
    Actor206100StateTable4 states;

    work   = (Actor206100Work*)task->work;
    obj    = task->extra.tmd;
    coord  = obj->coords;
    states = D_actor_206100_80149EC0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            states.funcs[(s16)work->field_520](task, &states);
            work->animStatus  = work->slots[1].status.fields.flags;
            coord->coord.t[0] = coord->coord.t[0] + (-coord->coord.t[0] >> 4);
            coord->coord.t[2] = coord->coord.t[2] + (-coord->coord.t[2] >> 4);
            coord->coord.t[1] =
                coord->coord.t[1] + (((s16)work->field_526 - coord->coord.t[1]) >> 4);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
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
/// `coord` is the actor's root coordinate, cleared so the composition pass rebuilds it --
/// the same dereference-store local `func_actor_206100_8014FDE8` binds.
///
/// The loop initialiser sits after the sub-state chain for the reason
/// `func_actor_206100_8014FCD4` documents: ahead of it the store that
/// materialises `i` shares a block with the case-3 increment, post-reload CSE
/// folds that increment's `+ 1` into `+ $s0`, and the phase is written with
/// `addu`.  Here the branch targets the initialiser instead.
static void func_actor_206100_8014E964(Task* task, void* unusedTable)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GfxCoord*        coord;
    s32              i;
    s16              state;

    work            = (Actor206100Work*)task->work;
    coord           = task->extra.tmd->coords;
    work->field_51E = work->field_51E + 1;
    next            = (Actor206100Work*)task->work;
    state           = next->animRequest;
    if (state == 1) {
        if (next->animPlaying != next->animClip) {
            next->field_512 = 0;
        } else {
            next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
        }
        func_actor_206100_8014F2F0(task);
        next->animRequest = 3;
    } else if (state == 2) {
        diverRestartClip(task);
        next->animRequest = 3;
        next->field_512   = 0;
    } else if (state == 3) {
        next->field_512 = next->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        animationTickSlot(&next->anim, i);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((s16)work->field_51E >= 0x32) {
        work->field_51E = 0;
        work->field_520 = work->field_520 + 1;
    }
}
#include "../../shared/diver_step_forward.inc.c"

static void func_actor_206100_8014EB48(Task* task, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->field_556 = 1;
    work->field_554 = 1;
    work->field_544 = arg1;
}

static void func_actor_206100_8014EB60(Task* task)
{
    Actor206100Work* work;
    GfxCoord*        coords;
    SVECTOR          rot;
    GfxMatrix        matrix;
    MATRIX*          dest;
    MATRIX*          mtx;

    work   = (Actor206100Work*)task->work;
    coords = task->extra.tmd->coords;
    dest   = &coords[5].coord;
    mtx    = &matrix.mat;

    matrix.rotationWords.m00M01 = ONE;
    matrix.rotationWords.m02M10 = 0;
    MATRIX_PAIR(mtx, 1, 1)      = 0x1000;
    matrix.rotationWords.m20M21 = 0;
    mtx->m[2][2]                = 0x1000;

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

static void func_actor_206100_8014EC54(Task* task)
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
static void func_actor_206100_8014ED3C(Task* task, s16 arg1)
{
    Actor206100Work*        work;
    GfxCoord*               coord;
    Actor206100DistScratch* head;
    Actor206100DistScratch* scratch;

    head                                         = SCRATCH_STACK_CURSOR(Actor206100DistScratch);
    scratch                                      = head - 1;
    SCRATCH_STACK_CURSOR(Actor206100DistScratch) = scratch;
    work                                         = (Actor206100Work*)task->work;
    coord                                        = task->extra.tmd->coords;
    diverStepForward(task, arg1, work->field_43E);
    scratch->delta.vx = -(u16)coord->coord.t[0];
    scratch->delta.vz = -(u16)coord->coord.t[2];
    scratch->dist     = SquareRoot0(scratch->delta.vx * scratch->delta.vx +
                                    scratch->delta.vz * scratch->delta.vz);
    if (scratch->dist >= 0x191) {
        coord->coord.t[0] = work->field_434;
        coord->coord.t[2] = work->field_438;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor206100DistScratch);
}

static Enemy* func_actor_206100_8014EE2C(s32 arg0)
{
    Enemy*     enemy;
    TmdObject* obj;

    enemy = Gp_SpawnEnemyFromTable(&D_80147E48, 0, 3, NULL);
    if (enemy != NULL) {
        enemy->placeKey        = arg0 << ENEMY_PLACE_INDEX_SHIFT;
        enemy->place           = &D_actor_206100_80155134[(s16)arg0];
        obj                    = enemy->task->extra.tmd;
        obj->texturePageOffset = 0;
        obj->clutRowOffset     = 2;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
        return enemy;
    }
    return NULL;
}

static void func_actor_206100_8014EEC0(Task* task)
{
    Actor206100ChildWork*  child;
    WorldCollisionContact* rec;
    GfxCoord*              coord;

    child                       = (Actor206100ChildWork*)task->work;
    coord                       = task->extra.tmd->coords;
    task->killCountdown         = 0;
    child->field_64             = 0x100;
    child->field_60             = 0;
    coord->parent               = &gGfxViewCoord;
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    child->obj.key              = Gp_PackPair(&D_actor_206100_80155194, 0);
    child->obj.coord            = task->extra.tmd->coords;
    rec                         = child->rec;
    child->obj.context.contacts = rec;
    child->obj.pos.vx           = 0;
    child->obj.pos.vy           = 0;
    child->obj.pos.vz           = 0;
    child->obj.radius           = 0x140;
    child->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &child->obj);
    Gp_InitRec18Table(rec, 2, 0);
    child->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_UpdateCoord(coord);
    diverImpactBurst(coord, (u16)child->field_60, 0, child->field_64 + 0x10002000);
    task->state++;
}

#include "../../shared/diver_strike_teardown.inc.c"

#include "../../shared/coord_math_local_to_world.inc.c"

/// The beam child's callback: runs its current state handler out of
/// `D_actor_206100_80149E24`, copying the table onto the stack first.
void func_actor_206100_8014F134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_206100_80149E24;
    sp.funcs[task->state](task);
}

static void func_actor_206100_8014F18C(Task* task)
{
    Actor206100Work* work;

    work = (Actor206100Work*)task->work;

    work->obj_364.coord            = &task->extra.tmd->coords[1];
    work->obj_364.context.contacts = work->rec_384;
    work->obj_364.pos.vx           = 0;
    work->obj_364.pos.vy           = 0;
    work->obj_364.pos.vz           = 0;
    work->obj_364.key              = 0x3003D;
    work->obj_364.radius           = 0x400;
    work->obj_364.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_364);
    Gp_InitRec18Table(work->rec_384, 6, 0);
    work->obj_364.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->obj_414.coord            = &task->extra.tmd->coords[4];
    work->obj_414.context.contacts = work->rec_384;
    work->obj_414.pos.vx           = 0;
    work->obj_414.pos.vy           = 0;
    work->obj_414.pos.vz           = 0;
    work->obj_414.key              = 0x3003D;
    work->obj_414.radius           = 0x200;
    work->obj_414.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj_414);
    work->obj_414.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

#include "../../shared/diver_restart_clip.inc.c"

/// Re-arms every animation slot for the pending request: writes the request's
/// step scale (`animStep`) into each slot's `rate` and re-seeks the slot to
/// the requested clip with `animationSeekSlotWithBlend`, whose fifth argument is the request's
/// own value at `animBlend`.  When the clip already playing (`animPlaying`) is not
/// the one requested, `animBlend` is cleared as well.  `animPlaying` latches the
/// clip either way, which is what lets the next frame tell the two cases apart.
/// The call sits *inside* the loop and takes a fresh `work` in `$a0` each
/// iteration, the same shape `func_actor_405800_80138294` has.
static void func_actor_206100_8014F2F0(Task* arg0)
{
    Actor206100Work* work;
    s32              i;

    work = (Actor206100Work*)arg0->work;
    if (work->animPlaying == work->animClip) {
        i = 1;
        do {
            work->slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < 0xF);
    } else {
        i = 1;
        do {
            work->slots[i].rate = work->animStep;
            animationSeekSlotWithBlend(&work->anim, i, work->animClip, 0, work->animBlend);
            i++;
        } while (i < 0xF);
        work->animBlend = 0;
    }
    work->animPlaying = work->animClip;
}
static s16 func_actor_206100_8014F3C8(Task* arg0, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)arg0->work;

    if (work->animStep == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->animStep << 12) >> 16;
}

/// The actor's task callback: runs its current top-level state out of
/// `D_actor_206100_80149E5C`, copying the table onto the stack first.
void func_actor_206100_8014F428(Task* task)
{
    TaskFuncTable5 sp;

    sp = D_actor_206100_80149E5C;
    sp.funcs[task->state](task);
}

static void func_actor_206100_8014F490(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

static void func_actor_206100_8014F4B8(MATRIX* src, MATRIX* dst)
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

/// Runs the actor's sub-state handler for the current `subState`, after
/// marking the enemy's list node so the exit path tears the actor down.
static void func_actor_206100_8014F524(Task* task)
{
    Actor206100Work* work;
    Enemy*           enemy;
    TaskFuncTable5   sp;

    work                          = (Actor206100Work*)task->work;
    enemy                         = (Enemy*)task->spawnArg2.pointer;
    sp                            = D_actor_206100_80149E94;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    sp.funcs[(s16)work->subState](task);
}

static void func_actor_206100_8014F59C(Task* task)
{
}

static void func_actor_206100_8014F5A4(Task* task)
{
}

static void func_actor_206100_8014F5AC(Task* task)
{
}

static void func_actor_206100_8014F5B4(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        diverState7Enter,
        func_actor_206100_8014F970,
    };

    states[(s16)work->subState](task);
}

static void func_actor_206100_8014F608(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F9C4,
        func_actor_206100_8014FA08,
    };

    states[(s16)work->subState](task);
}

static void func_actor_206100_8014F65C(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    func_actor_206100_8014DEAC(task);
    Gp_MsgPlayerWeapon(0);
    work->field_51E = 0;
    work->subState  = work->subState + 1;
}

static void func_actor_206100_8014F69C(Task* task)
{
    u16              timer;
    Actor206100Work* work = (Actor206100Work*)task->work;

    func_actor_206100_8014DEAC(task);
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5A) {
        work->field_51E = 0;
        work->subState  = work->subState + 1;
    }
}

static void func_actor_206100_8014F6F8(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* anim;

    work              = (Actor206100Work*)task->work;
    work->field_51E   = 0;
    anim              = (Actor206100Work*)task->work;
    anim->animBlend   = 8;
    anim->animStep    = 8;
    anim->animClip    = 7;
    anim->animRequest = 1;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F738(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->animBlend   = 0xA;
    work->animStep    = 0x10;
    work->animClip    = 1;
    work->animRequest = 1;
    work->field_51E   = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F770(Task* task)
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
        next->subState  = 0;
    }
}

static void func_actor_206100_8014F7B4(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    s32              soundId;
    s32              pan;

    work    = (Actor206100Work*)task->work;
    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x551E0005;
    pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    next              = (Actor206100Work*)task->work;
    next->animBlend   = 0xA;
    next->animStep    = 0x10;
    next->animClip    = 1;
    next->animRequest = 1;
    work->field_526   = 0x1388;
    work->field_51E   = 0;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014F878(Task* task)
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
        next->subState  = 0;
    }
}

#include "../../shared/diver_state7_enter.inc.c"

static void func_actor_206100_8014F970(Task* task)
{
    Actor206100Work* work;
    s32              cond;

    work = (Actor206100Work*)task->work;
    if ((work->animStatus & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->animStatus & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (work->animStatus & ANIMATION_SLOT_SETTLED)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor206100Work*)task->work;
        work->field_520 = 2;
        work->subState  = 0;
    }
}

static void func_actor_206100_8014F9C4(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->animBlend   = 8;
    work->animStep    = 0x10;
    work->animClip    = 0xE;
    work->animRequest = 1;
    work->field_51E   = 0;
    work->field_526   = work->field_536;
    work->subState    = work->subState + 1;
}

static void func_actor_206100_8014FA08(Task* task)
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
    if ((next->animStatus & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (next->animStatus & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (next->animStatus & ANIMATION_SLOT_SETTLED)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond != 0) {
        next              = (Actor206100Work*)task->work;
        next->animBlend   = 8;
        next->animStep    = 8;
        next->animClip    = 0x10;
        next->animRequest = 1;
    }
    if (Gp_TickObjFlag2(task->spawnArg2.pointer) != 0) {
        work->field_557 = 4;
        next            = (Actor206100Work*)task->work;
        next->field_520 = 1;
        next->subState  = 0;
    }
}
/// Ring-spawn state: seeds `field_4F4` and `field_548` from the eight-point ring
/// `D_actor_206100_80158B68`, copies the current vertex into the root part
/// coordinate, advances the index modulo 8, and hands the actor the state-1
/// animation request.  `coord` is the coordinate the effect argument at
/// `eff_4C0` shares, so moving it moves the actor.
///
/// `enemy` is a local rather than the inline
/// `((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;` because the fused form
/// transposes the `spawnArg2` and `task->extra` loads; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store, so give the pointer its own local".
static void func_actor_206100_8014FAE4(Task* task)
{
    GfxCoord*        coord;
    Actor206100Work* work;
    Actor206100Work* next;
    Actor206100Work* last;
    Enemy*           enemy;

    work                          = (Actor206100Work*)task->work;
    enemy                         = (Enemy*)task->spawnArg2.pointer;
    coord                         = task->extra.tmd->coords;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    work->field_54D               = 1;
    work->field_548               = 0;
    work->field_4F4               = D_actor_206100_80158B68;
    next                          = (Actor206100Work*)task->work;
    next->animStep                = 0x10;
    next->animClip                = 3;
    next->animRequest             = 2;
    work->field_43E               = 0x400;
    coord->coord.t[0]             = work->field_4F4[work->field_548].vx;
    coord->coord.t[1]             = work->field_4F4[work->field_548].vy;
    coord->coord.t[2]             = work->field_4F4[work->field_548].vz;
    work->field_548               = (work->field_548 + 1) & 7;
    Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    work->field_51E = 0;
    last            = (Actor206100Work*)task->work;
    last->field_520 = 1;
    last->subState  = 0;
}

static void func_actor_206100_8014FBE4(Task* task, void* unusedTable)
{
    Actor206100Work* work;
    Enemy*           enemy;
    s32              soundId;
    s32              pan;

    work  = (Actor206100Work*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    SndEvt_EnqueueType7(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, 1);
    Gp_ApplyAreaRecs(D_neo_ark_submarine_gallery_8018590C);
    work->field_526 = work->field_536;
    worldTargetUnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add(task, 0);
    GameFlag_SetNibble(GAME_FLAG_0F3, 1);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj_364);
    Gp_UnlinkObj(&work->obj_414);
    work->field_51E = 0;
    work->field_520 = work->field_520 + 1;
    soundId         = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan             = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
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
static void func_actor_206100_8014FCD4(Task* task, void* unusedTable)
{
    Actor206100Work* work;
    Actor206100Work* next;
    s32              i;
    s16              state;

    work              = (Actor206100Work*)task->work;
    work->animBlend   = 4;
    work->animStep    = 0x10;
    work->animClip    = 0xE;
    work->animRequest = 1;
    next              = (Actor206100Work*)task->work;
    state             = next->animRequest;
    if (state == 1) {
        if (next->animPlaying != next->animClip) {
            next->field_512 = 0;
        } else {
            next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
        }
        func_actor_206100_8014F2F0(task);
        next->animRequest = 3;
    } else if (state == 2) {
        diverRestartClip(task);
        next->animRequest = 3;
        next->field_512   = 0;
    } else if (state == 3) {
        next->field_512 = next->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        animationTickSlot(&next->anim, i);
    }
    work->field_520 = work->field_520 + 1;
}
/// Idle-state tick: advances the actor's two frame counters, keeps the root
/// coordinate dirty so the composition pass rebuilds it, spawns the shockwave task once the
/// counter reaches 0x5A and retires the actor four frames later.
///
/// `coord` is a local rather than the inline
/// `task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;` because the fused form loads
/// `task->extra` *after* the two counter stores, and sched1 will not lift a load
/// above an earlier store; its address load stays with the stores and both pick
/// up load-delay nops.  Binding the pointer above the counters frees the two
/// loads to be scheduled first, which is the target's order; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store".
static void func_actor_206100_8014FDE8(Task* task, void* unusedTable)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GfxCoord*        coord;

    coord               = task->extra.tmd->coords;
    work                = (Actor206100Work*)task->work;
    work->field_51E     = work->field_51E + 1;
    work->field_526     = work->field_526 + 0x10;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((s16)work->field_51E == 0x5A) {
        Task_SpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    if ((s16)work->field_51E >= 0x10E) {
        task->state     = 4;
        next            = (Actor206100Work*)task->work;
        next->field_520 = 0;
        next->subState  = 0;
    }
}
