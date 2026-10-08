#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "types.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/enemy_params.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Task-table indices entered by spawn and lost-actor retirement.
enum {
    ACTOR_01500_TASK_ACTIVE = 1,
    ACTOR_01500_TASK_DEATH  = 2
};

/// Fractional bits of the root's facing coefficients used for movement.
enum { ACTOR_01500_FACING_FRACTION_BITS = 12 };

/// Values of `_Actor01500Work::action`: the handler the per-frame tick runs.
///
/// `CHASE`, `SCRIPTED` and the task's death state number their stages in
/// `actionStep`.
enum {
    ACTOR_01500_ACTION_PERCH     = 0, // on the perch it spawned on, until the player comes within 2500, a noise or the scene's alert stirs it, or it has lost hit points
    ACTOR_01500_ACTION_ALERT     = 1, // has noticed the player nearby: stays 90 ticks on the perch, then takes off
    ACTOR_01500_ACTION_TAKE_OFF  = 2, // plays the take-off; from tick 30 it backs away from the perch, and at tick 59 it starts the chase
    ACTOR_01500_ACTION_CHASE     = 3, // airborne around the player, stepped by `ACTOR_01500_CHASE_*`
    ACTOR_01500_ACTION_FALL      = 4, // staggered or crippled: sinks until the floor pushes it back up
    ACTOR_01500_ACTION_GROUNDED  = 5, // on the floor, edging away from the player; it dies there when hit points are gone, the player leaves its height band or 1800 ticks pass
    ACTOR_01500_ACTION_WALL_REST = 6, // settled on a wall it flew into, for 30 to 93 ticks, then takes off again
    ACTOR_01500_ACTION_HIT       = 7, // flinching from a hit that left it above 60% of its hit points; resumes by `posture`
    ACTOR_01500_ACTION_DIE       = 8, // the task is in its death state; the tick runs no handler
    ACTOR_01500_ACTION_SCRIPTED  = 9  // the scripted variant: hovers and advances about a fixed point of its room until the player crosses a line, then chases
};

/// Values of `_Actor01500Work::actionStep` during `ACTOR_01500_ACTION_CHASE`.
///
/// `ACTOR_01500_ACTION_SCRIPTED` runs the first three against its fixed point.
enum {
    ACTOR_01500_CHASE_SETTLE  = 0, // climbs or sinks 30 a tick to its hover height while turning to the target; ends within 30 of it or when `timer` runs out
    ACTOR_01500_CHASE_HOVER   = 1, // holds its place, still turning, until `timer` runs out
    ACTOR_01500_CHASE_ADVANCE = 2, // flies forward 200 a tick for `advanceLeft`, then hovers; within 1000 of the player it dives instead, once per advance
    ACTOR_01500_CHASE_DIVE    = 3, // descends on the player with `attackBody` armed, until the attack lands or it is within 1600 of the player's height
    ACTOR_01500_CHASE_CLIMB   = 4  // climbs or sinks 96 a tick toward 1800 above the player, then advances again
};

/// Values of `_Actor01500Work::actionStep` in the task's death state.
enum {
    ACTOR_01500_DEATH_BEGIN   = 0, // saves the root matrix and withdraws the target entry and the three bodies; goes on to `SHRINK`, or to `BURST` when `burstPending`
    ACTOR_01500_DEATH_SHRINK  = 1, // squashes the body for 60 ticks, turning it translucent at tick 10 and spawning the corpse-burn effect at tick 15
    ACTOR_01500_DEATH_DESTROY = 2, // destroys the enemy
    ACTOR_01500_DEATH_BURST   = 3, // body hidden: spawns the four burst parts in its place, then waits out 60 ticks
    ACTOR_01500_DEATH_LOST    = 4  // the actor fell 5000 below the player while alive: withdraws on the first tick and waits 61
};

/// Values of `_Actor01500Work::posture`: what a hit interrupts, which picks
/// the flinch and what follows it.
enum {
    ACTOR_01500_POSTURE_PERCHED  = 0, // on its spawn perch or resting on a wall
    ACTOR_01500_POSTURE_AIRBORNE = 1, // has taken off
    ACTOR_01500_POSTURE_CRIPPLED = 2  // at 60% of its hit points or less: falling or grounded for good
};

/// Values of `_Actor01500Work::perch`: how the actor sits on the perch it
/// spawns on, from bit 0 of the placement's mode.
enum {
    ACTOR_01500_PERCH_WALL    = 0, // room body behind the root; takes off level
    ACTOR_01500_PERCH_CEILING = 1  // room body below the root; also drops as it takes off
};

/// Values of `_Actor01500Work::variant`, the placement's variant.
enum {
    ACTOR_01500_VARIANT_PERCHED  = 0, // spawns on a perch; the placement's mode picks the perch (bit 0) and `noWallPerch` (bit 1)
    ACTOR_01500_VARIANT_SCRIPTED = 1  // spawns airborne in `ACTOR_01500_ACTION_SCRIPTED`
};

/// Values of `_Actor01500Work::anim`: indices into the package's
/// animation-set table.
enum {
    ACTOR_01500_ANIM_PERCH_WALL       = 1,  // waiting on a wall perch
    ACTOR_01500_ANIM_PERCH_CEILING    = 2,  // waiting on a ceiling perch
    ACTOR_01500_ANIM_ALERT_WALL       = 3,  // `ACTION_ALERT` on a wall perch
    ACTOR_01500_ANIM_ALERT_CEILING    = 4,  // `ACTION_ALERT` on a ceiling perch
    ACTOR_01500_ANIM_HOVER            = 5,  // settling and hovering; the only animation the bob is added to
    ACTOR_01500_ANIM_ADVANCE          = 6,  // flying forward, and the climb after a dive
    ACTOR_01500_ANIM_TAKE_OFF_WALL    = 7,  // take-off from a wall
    ACTOR_01500_ANIM_TAKE_OFF_CEILING = 8,  // take-off from a ceiling perch
    ACTOR_01500_ANIM_WALL_REST        = 9,  // `ACTION_WALL_REST`
    ACTOR_01500_ANIM_DIVE             = 10, // the dive at the player
    ACTOR_01500_ANIM_FLINCH_WALL      = 11, // hit while `POSTURE_PERCHED`, wall perch
    ACTOR_01500_ANIM_FLINCH_CEILING   = 12, // hit while `POSTURE_PERCHED`, ceiling perch
    ACTOR_01500_ANIM_FLINCH_AIR       = 13, // hit in any other posture; also the stagger that starts a fall
    ACTOR_01500_ANIM_GROUNDED         = 14  // `ACTION_GROUNDED`
};

/// Values of `_Actor01500Work::loopSound`, and the damage cry: sound requests
/// of the package, queued with the enemy's placement index in bits 8 to 11.
enum {
    ACTOR_01500_SOUND_ALERT        = 0x400F0001, // looped during `ACTION_ALERT`
    ACTOR_01500_SOUND_FLIGHT_START = 0x400F0002, // looped for the first `loopSoundTimer` ticks of a take-off or hover
    ACTOR_01500_SOUND_FLIGHT       = 0x400F0003, // looped in flight once `FLIGHT_START` has run its ticks
    ACTOR_01500_SOUND_DAMAGE       = 0x400F0004  // queued once by each hit that takes hit points
};

/// Ticks `ACTOR_01500_SOUND_FLIGHT_START` loops before the flight sound replaces it.
#define ACTOR_01500_FLIGHT_START_TICKS 15

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, three collision
/// spheres with their contact tables, and the state the per-frame tick moves
/// the actor with.
///
/// Each tick the running action picks `anim`, `targetYaw`, `turnRate`, `speed`
/// and `verticalSpeed`; the tick then turns the model's root toward the
/// target heading, moves it along its facing and vertically, and steps the
/// animation. Angles are 4096ths of a turn about Y, and Y grows downward.
/// Timers count ticks.
typedef struct {
    ActorAnimRig7         rig;               // playback of the model's parts; slots 1 to 6 are driven
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    body;              // sphere of radius 300 on the model's part 2 that takes the hits and meets other enemies' bodies
    WorldCollisionContact contacts[3];       // contacts of `body`; also the enemy's hit records
    WorldCollisionBody    roomBody;          // sphere of radius 300 that meets the room's grid and floor, held 300 off the root: behind it on a wall, below it under a ceiling perch, above it in flight
    WorldCollisionContact roomContacts[5];   // contacts of `roomBody`
    WorldCollisionBody    attackBody;        // sphere of radius 300, 400 ahead of the root, carrying the package's attack; meets other bodies only during `ACTOR_01500_CHASE_DIVE`
    WorldCollisionContact attackContacts[1]; // contact of `attackBody`
    EffectSpawnArg        hitEffectArg;      // argument record of the effect a hit on the actor spawns, hung off the model's root
    VECTOR                prevPos;           // root position before the tick's movement, put back when the room contacts ask for a reset instead of a push; `pad` unused
    MATRIX                deathBaseMtx;      // root matrix as the death state began, the unscaled base of its squash
    s32                   loopSound;         // `ACTOR_01500_SOUND_*` queued on every third tick of the playing animation (0 silent)
    s16                   hitCooldown;       // ticks left in which hits on `body` are ignored, set by the attack of the last one
    s16                   anim;              // `ACTOR_01500_ANIM_*` the actions ask for
    s16                   playingAnim;       // `anim` the slots were last started on; a flinch sets it apart from `anim` to force a restart
    s16                   animFrame;         // ticks since `playingAnim` was started
    s16                   posture;           // `ACTOR_01500_POSTURE_*`
    s16                   action;            // `ACTOR_01500_ACTION_*`
    s16                   actionStep;        // `ACTOR_01500_CHASE_*`, or `ACTOR_01500_DEATH_*` in the death state
    s16                   advanceLeft;       // distance `ACTOR_01500_CHASE_ADVANCE` has yet to fly, 1000 to 1600 at its start
    s16                   speed;             // distance moved along the facing each tick
    s16                   timer;             // countdown or tick count of the running action or step
    s16                   hoverOffset;       // 0 to 511 added to the height above its target the actor settles at; on the spawn perch it counts instead the 1 to 32 ticks between looks at the scene's enemy alert
    s16                   verticalSpeed;     // added to the root's Y each tick; positive sinks
    s16                   deathScaleY;       // Y scale of the death squash, from 4096 down to 512 by 80 a tick
    s16                   attackLanded;      // 1 from `attackBody` touching a body until the dive that armed it ends (0 otherwise)
    s16                   lunged;            // 1 once the current advance has dived at the player; cleared by the next hover
    s16                   perch;             // `ACTOR_01500_PERCH_*`; reset to `WALL` on leaving a wall rest
    s16                   noWallPerch;       // 1 keeps a chasing actor from settling on a wall it flies into: bit 1 of the placement's mode, always 1 on the scripted variant
    u16                   targetYaw;         // heading the actor turns toward, 0 to 4095
    s16                   yaw;               // heading of the model's root, read back from its rotation and stepped toward `targetYaw`
    s16                   turnRate;          // most `yaw` may change in a tick; 0 leaves the rotation alone
    s16                   deathPending;      // 1 once the actor is to die: out of hit points, or done on the floor. Acted on when it is grounded or `burstPending`
    s16                   roused;            // 1 once the actor has started a take-off or its chase; back in `ACTOR_01500_ACTION_PERCH` it then takes off at once
    s16                   bobPhase;          // tick of the 15-tick hover bob, 0 to 14
    s16                   burstPending;      // 0 for the squash death; 1 when the killing hit bursts the body, 2 a tick into the death state, back to 0 as the parts spawn
    s16                   loopSoundTimer;    // ticks left before `ACTOR_01500_SOUND_FLIGHT` replaces `loopSound`; 0 leaves it alone
    s16                   variant;           // `ACTOR_01500_VARIANT_*`
} _Actor01500Work;
STATIC_ASSERT_SIZEOF(_Actor01500Work, 0x384);

/// Blend frames handed to `animationSeekSlotWithBlend` when an animation starts, indexed by `anim`.
extern s16 Actor01500_D0A050[];

/// Fifteen vertical bob offsets cycled by `bobPhase` while `anim` is `ACTOR_01500_ANIM_HOVER`.
extern s16 Actor01500_D0A070[];

/// Sixteen tick counts the hovering states reload `timer` from, picked
/// by a `gRandomLcgState` draw.
extern u16 Actor01500_D09FC8[];

/// Sixteen distances `advanceLeft` is reloaded from when the actor starts to
/// advance, picked by a `gRandomLcgState` draw.
extern u16 Actor01500_D09FE8[];

/* `D_80067704` selects the model stream the next `effectSpawn` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Pair packed into `attackBody`'s `key` at spawn.
extern DamageAttack Actor01500_D09FB4;
/// The enemy's parameter record; `hpMax` seeds the hit points.
extern EnemyParams Actor01500_D09FB8;
/// Animation bank handed to `animationInitContext`.
extern AnimationSet* Actor01500_D0A014[15];

/// The four model streams `Actor01500_Fn01AB0` spawns effects from.
static TmdSource _gActor01500MindSucklerBurstHead;
static TmdSource _gActor01500MindSucklerBurstWing;
static TmdSource _gActor01500MindSucklerBurstStinger;
static TmdSource _gActor01500MindSucklerBurstTail;

/// Points `_actor01500ScriptedAction` measures against: the XZ of
/// `Actor01500_D0A090` is where it heads, its Y (`Actor01500_D0A090.vy`) the
/// height it settles below, and `gPlayerStatus.coordMtx` passing the X/Z bounds of
/// `Actor01500_D0A098` ends the state.
extern SVECTOR Actor01500_D0A090;
extern SVECTOR Actor01500_D0A098;

static void _actor01500Spawn(Enemy* enemy, Task* task);
static void Actor01500_Fn004EC(Task* actor);
static void _actor01500ApplyDamage(Task* actor, s32 damage);
static void _actor01500PerchAction(Task* actor);
static void _actor01500TakeOffAction(Task* actor);
static void _actor01500ChaseAction(Task* actor);
static void _actor01500GroundedAction(Task* actor);
static void _actor01500HitAction(Task* actor);
static void _actor01500TurnTowardTarget(Task* actor);
static void _actor01500Move(Task* actor);
static void Actor01500_Fn01AB0(Task* arg0);
static void Actor01500_Fn01DF0(Enemy* arg0, Task* arg1);
static void _actor01500ScriptedAction(Task* actor);
static void Actor01500_Fn02428(Task* task);
static void Actor01500_Fn02484(Enemy* enemy, Task* actor);
static void _actor01500ApplyReactions(Task* actor);
static void _actor01500UpdateAction(Task* actor);
static void _actor01500AlertAction(Task* actor);
static void _actor01500FallAction(Task* actor);
static void _actor01500WallRestAction(Task* actor);
static void _actor01500UpdateAnimation(Task* actor);
static void Actor01500_Fn02A1C(Task* actor);
static void Actor01500_Fn02B14(Task* actor);
static void Actor01500_Fn02B70(Task* actor);
static void Actor01500_Fn02C34(Task* actor);

/// The actor's three task states - spawn, per-frame tick and teardown - run
/// by `Actor01500_Fn02428`.
static const EnemyTaskFuncTable3 Actor01500_D00004 = {
    {
        _actor01500Spawn,
        Actor01500_Fn02484,
        Actor01500_Fn01DF0,
    },
};

static void Actor01500_Fn02428(Task*);

static TmdBone _gActor01500MindSucklerBodySkeleton[7] = {
#include "assets/mind_suckler_body_skeleton.inc"
};

static u32 _gActor01500MindSucklerBodyPartVerts[7] = {
#include "assets/mind_suckler_body_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBodyVerts[90] = {
#include "assets/mind_suckler_body_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBodyNormals[96] = {
#include "assets/mind_suckler_body_normals.inc"
};

static u32 _gActor01500MindSucklerBodyStream[927] = {
#include "assets/mind_suckler_body_stream.inc"
};

static TmdSource _gActor01500MindSucklerBody = {
    0,
    5292,
    1000,
    7,
    _gActor01500MindSucklerBodyPartVerts,
    _gActor01500MindSucklerBodyVerts,
    _gActor01500MindSucklerBodyNormals,
    _gActor01500MindSucklerBodySkeleton,
    _gActor01500MindSucklerBodyStream,
};

static TmdBone _gActor01500MindSucklerBurstHeadSkeleton[1] = {
#include "assets/mind_suckler_burst_head_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstHeadPartVerts[1] = {
#include "assets/mind_suckler_burst_head_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstHeadVerts[13] = {
#include "assets/mind_suckler_burst_head_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstHeadNormals[13] = {
#include "assets/mind_suckler_burst_head_normals.inc"
};

static u32 _gActor01500MindSucklerBurstHeadStream[121] = {
#include "assets/mind_suckler_burst_head_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstHead = {
    0,
    768,
    0,
    1,
    _gActor01500MindSucklerBurstHeadPartVerts,
    _gActor01500MindSucklerBurstHeadVerts,
    _gActor01500MindSucklerBurstHeadNormals,
    _gActor01500MindSucklerBurstHeadSkeleton,
    _gActor01500MindSucklerBurstHeadStream,
};

static TmdBone _gActor01500MindSucklerBurstWingSkeleton[1] = {
#include "assets/mind_suckler_burst_wing_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstWingPartVerts[1] = {
#include "assets/mind_suckler_burst_wing_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstWingVerts[5] = {
#include "assets/mind_suckler_burst_wing_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstWingNormals[5] = {
#include "assets/mind_suckler_burst_wing_normals.inc"
};

static u32 _gActor01500MindSucklerBurstWingStream[42] = {
#include "assets/mind_suckler_burst_wing_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstWing = {
    0,
    240,
    0,
    1,
    _gActor01500MindSucklerBurstWingPartVerts,
    _gActor01500MindSucklerBurstWingVerts,
    _gActor01500MindSucklerBurstWingNormals,
    _gActor01500MindSucklerBurstWingSkeleton,
    _gActor01500MindSucklerBurstWingStream,
};

static TmdBone _gActor01500MindSucklerBurstStingerSkeleton[1] = {
#include "assets/mind_suckler_burst_stinger_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstStingerPartVerts[1] = {
#include "assets/mind_suckler_burst_stinger_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstStingerVerts[9] = {
#include "assets/mind_suckler_burst_stinger_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstStingerNormals[9] = {
#include "assets/mind_suckler_burst_stinger_normals.inc"
};

static u32 _gActor01500MindSucklerBurstStingerStream[68] = {
#include "assets/mind_suckler_burst_stinger_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstStinger = {
    0,
    420,
    0,
    1,
    _gActor01500MindSucklerBurstStingerPartVerts,
    _gActor01500MindSucklerBurstStingerVerts,
    _gActor01500MindSucklerBurstStingerNormals,
    _gActor01500MindSucklerBurstStingerSkeleton,
    _gActor01500MindSucklerBurstStingerStream,
};

static TmdBone _gActor01500MindSucklerBurstTailSkeleton[1] = {
#include "assets/mind_suckler_burst_tail_skeleton.inc"
};

static u32 _gActor01500MindSucklerBurstTailPartVerts[1] = {
#include "assets/mind_suckler_burst_tail_partVerts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstTailVerts[9] = {
#include "assets/mind_suckler_burst_tail_verts.inc"
};

static SVECTOR _gActor01500MindSucklerBurstTailNormals[9] = {
#include "assets/mind_suckler_burst_tail_normals.inc"
};

static u32 _gActor01500MindSucklerBurstTailStream[63] = {
#include "assets/mind_suckler_burst_tail_stream.inc"
};

static TmdSource _gActor01500MindSucklerBurstTail = {
    0,
    392,
    0,
    1,
    _gActor01500MindSucklerBurstTailPartVerts,
    _gActor01500MindSucklerBurstTailVerts,
    _gActor01500MindSucklerBurstTailNormals,
    _gActor01500MindSucklerBurstTailSkeleton,
    _gActor01500MindSucklerBurstTailStream,
};

static AnimationPackedPose _gActor01500Actor101500Animation04DE0Bank1[13] = {
#include "assets/actor_101500_animation_04DE0_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation04DE0Bank4[65] = {
#include "assets/actor_101500_animation_04DE0_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation04DE0Records[94] = {
#include "assets/actor_101500_animation_04DE0_records.inc"
};

static u16 _gActor01500Actor101500Animation04DE0Indices[8] = {
#include "assets/actor_101500_animation_04DE0_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation04DE0 = {
    _gActor01500Actor101500Animation04DE0Records,
    _gActor01500Actor101500Animation04DE0Indices,
    { NULL, _gActor01500Actor101500Animation04DE0Bank1, NULL, NULL, _gActor01500Actor101500Animation04DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation05130Bank1[13] = {
#include "assets/actor_101500_animation_05130_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation05130Bank4[65] = {
#include "assets/actor_101500_animation_05130_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation05130Records[94] = {
#include "assets/actor_101500_animation_05130_records.inc"
};

static u16 _gActor01500Actor101500Animation05130Indices[8] = {
#include "assets/actor_101500_animation_05130_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation05130 = {
    _gActor01500Actor101500Animation05130Records,
    _gActor01500Actor101500Animation05130Indices,
    { NULL, _gActor01500Actor101500Animation05130Bank1, NULL, NULL, _gActor01500Actor101500Animation05130Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation06660Bank1[88] = {
#include "assets/actor_101500_animation_06660_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation06660Bank4[480] = {
#include "assets/actor_101500_animation_06660_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation06660Records[598] = {
#include "assets/actor_101500_animation_06660_records.inc"
};

static u16 _gActor01500Actor101500Animation06660Indices[8] = {
#include "assets/actor_101500_animation_06660_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation06660 = {
    _gActor01500Actor101500Animation06660Records,
    _gActor01500Actor101500Animation06660Indices,
    { NULL, _gActor01500Actor101500Animation06660Bank1, NULL, NULL, _gActor01500Actor101500Animation06660Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation07B90Bank1[88] = {
#include "assets/actor_101500_animation_07B90_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation07B90Bank4[480] = {
#include "assets/actor_101500_animation_07B90_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation07B90Records[598] = {
#include "assets/actor_101500_animation_07B90_records.inc"
};

static u16 _gActor01500Actor101500Animation07B90Indices[8] = {
#include "assets/actor_101500_animation_07B90_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation07B90 = {
    _gActor01500Actor101500Animation07B90Records,
    _gActor01500Actor101500Animation07B90Indices,
    { NULL, _gActor01500Actor101500Animation07B90Bank1, NULL, NULL, _gActor01500Actor101500Animation07B90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation07C7CBank1[2] = {
#include "assets/actor_101500_animation_07C7C_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation07C7CBank4[9] = {
#include "assets/actor_101500_animation_07C7C_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation07C7CRecords[30] = {
#include "assets/actor_101500_animation_07C7C_records.inc"
};

static u16 _gActor01500Actor101500Animation07C7CIndices[8] = {
#include "assets/actor_101500_animation_07C7C_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation07C7C = {
    _gActor01500Actor101500Animation07C7CRecords,
    _gActor01500Actor101500Animation07C7CIndices,
    { NULL, _gActor01500Actor101500Animation07C7CBank1, NULL, NULL, _gActor01500Actor101500Animation07C7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation07D68Bank1[2] = {
#include "assets/actor_101500_animation_07D68_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation07D68Bank4[9] = {
#include "assets/actor_101500_animation_07D68_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation07D68Records[30] = {
#include "assets/actor_101500_animation_07D68_records.inc"
};

static u16 _gActor01500Actor101500Animation07D68Indices[8] = {
#include "assets/actor_101500_animation_07D68_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation07D68 = {
    _gActor01500Actor101500Animation07D68Records,
    _gActor01500Actor101500Animation07D68Indices,
    { NULL, _gActor01500Actor101500Animation07D68Bank1, NULL, NULL, _gActor01500Actor101500Animation07D68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation08844Bank1[71] = {
#include "assets/actor_101500_animation_08844_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation08844Bank4[176] = {
#include "assets/actor_101500_animation_08844_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation08844Records[292] = {
#include "assets/actor_101500_animation_08844_records.inc"
};

static u16 _gActor01500Actor101500Animation08844Indices[8] = {
#include "assets/actor_101500_animation_08844_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation08844 = {
    _gActor01500Actor101500Animation08844Records,
    _gActor01500Actor101500Animation08844Indices,
    { NULL, _gActor01500Actor101500Animation08844Bank1, NULL, NULL, _gActor01500Actor101500Animation08844Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation092F4Bank1[67] = {
#include "assets/actor_101500_animation_092F4_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation092F4Bank4[176] = {
#include "assets/actor_101500_animation_092F4_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation092F4Records[293] = {
#include "assets/actor_101500_animation_092F4_records.inc"
};

static u16 _gActor01500Actor101500Animation092F4Indices[8] = {
#include "assets/actor_101500_animation_092F4_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation092F4 = {
    _gActor01500Actor101500Animation092F4Records,
    _gActor01500Actor101500Animation092F4Indices,
    { NULL, _gActor01500Actor101500Animation092F4Bank1, NULL, NULL, _gActor01500Actor101500Animation092F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09650Bank1[14] = {
#include "assets/actor_101500_animation_09650_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09650Bank4[65] = {
#include "assets/actor_101500_animation_09650_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09650Records[94] = {
#include "assets/actor_101500_animation_09650_records.inc"
};

static u16 _gActor01500Actor101500Animation09650Indices[8] = {
#include "assets/actor_101500_animation_09650_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09650 = {
    _gActor01500Actor101500Animation09650Records,
    _gActor01500Actor101500Animation09650Indices,
    { NULL, _gActor01500Actor101500Animation09650Bank1, NULL, NULL, _gActor01500Actor101500Animation09650Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09708Bank1[2] = {
#include "assets/actor_101500_animation_09708_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09708Bank4[5] = {
#include "assets/actor_101500_animation_09708_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09708Records[21] = {
#include "assets/actor_101500_animation_09708_records.inc"
};

static u16 _gActor01500Actor101500Animation09708Indices[8] = {
#include "assets/actor_101500_animation_09708_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09708 = {
    _gActor01500Actor101500Animation09708Records,
    _gActor01500Actor101500Animation09708Indices,
    { NULL, _gActor01500Actor101500Animation09708Bank1, NULL, NULL, _gActor01500Actor101500Animation09708Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation098C0Bank1[7] = {
#include "assets/actor_101500_animation_098C0_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation098C0Bank4[30] = {
#include "assets/actor_101500_animation_098C0_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation098C0Records[45] = {
#include "assets/actor_101500_animation_098C0_records.inc"
};

static u16 _gActor01500Actor101500Animation098C0Indices[8] = {
#include "assets/actor_101500_animation_098C0_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation098C0 = {
    _gActor01500Actor101500Animation098C0Records,
    _gActor01500Actor101500Animation098C0Indices,
    { NULL, _gActor01500Actor101500Animation098C0Bank1, NULL, NULL, _gActor01500Actor101500Animation098C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09A78Bank1[7] = {
#include "assets/actor_101500_animation_09A78_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09A78Bank4[30] = {
#include "assets/actor_101500_animation_09A78_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09A78Records[45] = {
#include "assets/actor_101500_animation_09A78_records.inc"
};

static u16 _gActor01500Actor101500Animation09A78Indices[8] = {
#include "assets/actor_101500_animation_09A78_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09A78 = {
    _gActor01500Actor101500Animation09A78Records,
    _gActor01500Actor101500Animation09A78Indices,
    { NULL, _gActor01500Actor101500Animation09A78Bank1, NULL, NULL, _gActor01500Actor101500Animation09A78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09BC0Bank1[6] = {
#include "assets/actor_101500_animation_09BC0_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09BC0Bank4[17] = {
#include "assets/actor_101500_animation_09BC0_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09BC0Records[33] = {
#include "assets/actor_101500_animation_09BC0_records.inc"
};

static u16 _gActor01500Actor101500Animation09BC0Indices[8] = {
#include "assets/actor_101500_animation_09BC0_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09BC0 = {
    _gActor01500Actor101500Animation09BC0Records,
    _gActor01500Actor101500Animation09BC0Indices,
    { NULL, _gActor01500Actor101500Animation09BC0Bank1, NULL, NULL, _gActor01500Actor101500Animation09BC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01500Actor101500Animation09F8CBank1[16] = {
#include "assets/actor_101500_animation_09F8C_bank1.inc"
};

static AnimationPackedRotation _gActor01500Actor101500Animation09F8CBank4[75] = {
#include "assets/actor_101500_animation_09F8C_bank4.inc"
};

static AnimationRecord _gActor01500Actor101500Animation09F8CRecords[106] = {
#include "assets/actor_101500_animation_09F8C_records.inc"
};

static u16 _gActor01500Actor101500Animation09F8CIndices[8] = {
#include "assets/actor_101500_animation_09F8C_indices.inc"
};

static AnimationSet _gActor01500Actor101500Animation09F8C = {
    _gActor01500Actor101500Animation09F8CRecords,
    _gActor01500Actor101500Animation09F8CIndices,
    { NULL, _gActor01500Actor101500Animation09F8CBank1, NULL, NULL, _gActor01500Actor101500Animation09F8CBank4, NULL, NULL, NULL },
};

DamageAttack Actor01500_D09FB4 = { 8, 7 };

EnemyParams Actor01500_D09FB8 = { &Actor01500_D09FB4, 50, 12, 36, 2, 100, 1, 100, 0 };

u16 Actor01500_D09FC8[16] = {
    30,
    34,
    36,
    38,
    40,
    42,
    44,
    46,
    48,
    50,
    52,
    54,
    56,
    58,
    60,
    65,
};

u16 Actor01500_D09FE8[16] = {
    1000,
    1050,
    1100,
    1150,
    1200,
    1200,
    1250,
    1250,
    1300,
    1300,
    1350,
    1400,
    1450,
    1500,
    1550,
    1600,
};

TaskDesc Actor01500_D0A008 = { { { TASK_BODY_TMD, 96 } }, Actor01500_Fn02428, { .model = &_gActor01500MindSucklerBody } };

AnimationSet* Actor01500_D0A014[15] = {
    NULL,
    &_gActor01500Actor101500Animation04DE0,
    &_gActor01500Actor101500Animation05130,
    &_gActor01500Actor101500Animation06660,
    &_gActor01500Actor101500Animation07B90,
    &_gActor01500Actor101500Animation07C7C,
    &_gActor01500Actor101500Animation07D68,
    &_gActor01500Actor101500Animation08844,
    &_gActor01500Actor101500Animation092F4,
    &_gActor01500Actor101500Animation09650,
    &_gActor01500Actor101500Animation09708,
    &_gActor01500Actor101500Animation098C0,
    &_gActor01500Actor101500Animation09A78,
    &_gActor01500Actor101500Animation09BC0,
    &_gActor01500Actor101500Animation09F8C,
};

s16 Actor01500_D0A050[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    4,
    4,
    4,
    4,
    0,
};

s16 Actor01500_D0A070[16] = {
    0,
    10,
    19,
    24,
    25,
    22,
    15,
    5,
    -5,
    -15,
    -22,
    -25,
    -24,
    -19,
    -10,
    0,
};

SVECTOR Actor01500_D0A090 = { 1110, -0x2EE0, -2500, 0 };

SVECTOR Actor01500_D0A098 = { 1900, -0x2EE0, 1000, 0 };

/// Links the part-2 hurt sphere and initializes the enemy's three contact slots.
static __inline__ void _actor01500InitHurtBody(_Actor01500Work* work, GfxCoord* coord, s32 key)
{
    work->body.coord            = coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = key;
    work->body.radius           = 300;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
}

/// Aims chase flight at the player using the horizontal parent-frame offset.
///
/// XYZ scratch storage is borrowed for this call; X/Z narrow to signed
/// halfwords for the bearing, and the selected turn limit is 100 angle units.
static __inline__ void _actor01500AimChaseAtPlayer(_Actor01500Work* work, GfxCoord* rootCoord, VECTOR* playerOffset)
{
    playerOffset->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerOffset->vy = 0;
    playerOffset->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    work->targetYaw  = ratan2((s16)playerOffset->vx, (s16)playerOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    work->turnRate   = 100;
}

/// Selects a reaction flinch, preserving a crippled actor's current action.
static __inline__ void _actor01500StartReactionFall(_Actor01500Work* work)
{
    if (work->posture != ACTOR_01500_POSTURE_CRIPPLED) {
        work->action = ACTOR_01500_ACTION_FALL;
    }
    work->timer     = 0;
    work->anim      = ACTOR_01500_ANIM_FLINCH_AIR;
    work->loopSound = 0;
}

/// Creates the actor's work, playback rig, target entry and three collision spheres.
///
/// `enemy` is owned by `task`, which already has the seven-coordinate model
/// and a placement with variant 0 (perched) or 1 (scripted) loaded. The task
/// owns the zeroed work allocation and keeps the model and clip data live.
/// Allocation failure destroys the enemy; success acquires one battle hold
/// and advances to the active task state.
static void _actor01500Spawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_01500_BODY_KEY = WORLD_COLLISION_CONTACT_ENEMY_BODY | 15 };

    _Actor01500Work*       work;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    AreaPlacement*         placement;
    WorldCollisionContact* attackContacts;
    u32                    randomDraw;
    s32                    slotIndex;
    s32                    blendFrames;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Actor01500Work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[2];
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &Actor01500_D09FB8;
    enemy->recs                   = work->contacts;
    enemy->hp                     = Actor01500_D09FB8.hpMax;
    work->hitEffectArg.coord      = rootCoord;
    work->hitEffectArg.spawnArgLo = 0x300;
    work->hitEffectArg.spawnArgHi = 1;
    placement                     = enemy->place;
    // The placement selects attachment orientation or an already-airborne start.
    switch (work->variant = placement->variant) {
        case ACTOR_01500_VARIANT_PERCHED:
            work->perch       = enemy->place->mode & 1;
            work->noWallPerch = (enemy->place->mode >> 1) & 1;
            switch (work->perch) {
                case ACTOR_01500_PERCH_WALL:
                    work->anim        = ACTOR_01500_ANIM_PERCH_WALL;
                    work->playingAnim = ACTOR_01500_ANIM_PERCH_WALL;
                    work->loopSound   = 0;
                    break;
                case ACTOR_01500_PERCH_CEILING:
                    work->anim        = ACTOR_01500_ANIM_PERCH_CEILING;
                    work->playingAnim = ACTOR_01500_ANIM_PERCH_CEILING;
                    work->loopSound   = 0;
                    break;
            }
            work->timer = 0;
            randomDraw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hoverOffset            = ((randomDraw >> 16) & 0x1F) + 1;
            work->targetYaw              = (ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
            break;
        case ACTOR_01500_VARIANT_SCRIPTED:
            work->noWallPerch    = 1;
            work->anim           = ACTOR_01500_ANIM_HOVER;
            work->playingAnim    = ACTOR_01500_ANIM_HOVER;
            work->action         = ACTOR_01500_ACTION_SCRIPTED;
            work->actionStep     = ACTOR_01500_CHASE_SETTLE;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer          = ((gRandomLcgState >> 16) & 0x3F) + 0x3C;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hoverOffset    = (gRandomLcgState >> 16) & 0x1FF;
            work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
            work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            break;
    }
    sceneAcquireBattleRef(0);
    animationInitContext(&work->rig.anim, Actor01500_D0A014, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, work->anim);
    }
    if (work->variant == ACTOR_01500_VARIANT_PERCHED) {
        // Blend from each slot's ticked pose for a random number of whole frames.
        randomDraw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        blendFrames                  = (randomDraw >> 16) & 0x3F;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
        }
    }
    // Keep hurt, room-query and attack contacts separate; a later dive arms attacks.
    _actor01500InitHurtBody(work, &task->extra.tmd->coords[2], ACTOR_01500_BODY_KEY);
    work->roomBody.context.contacts = work->roomContacts;
    work->roomBody.coord            = rootCoord;
    work->roomBody.pos.vx           = 0;
    work->body.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    if (work->perch == ACTOR_01500_PERCH_WALL) {
        work->roomBody.pos.vy = 0;
        work->roomBody.pos.vz = -300;
    } else {
        work->roomBody.pos.vy = 300;
        work->roomBody.pos.vz = 0;
    }
    work->roomBody.key    = ACTOR_01500_BODY_KEY;
    work->roomBody.radius = 300;
    work->roomBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->roomBody);
    worldCollisionInitContacts(work->roomContacts, ARRAY_SIZE(work->roomContacts), 0);
    attackContacts                    = work->attackContacts;
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 400;
    work->roomBody.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&Actor01500_D09FB4, 0);
    work->attackBody.radius           = 300;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state             = ACTOR_01500_TASK_ACTIVE;
}

/// Per-frame contact pass. Applies the room's push-back from `roomContacts`,
/// which also settles a chasing actor on a wall it flew into and grounds a
/// falling one. Then walks `contacts`: a player's attack deals its damage,
/// reaction and hit effect unless `hitCooldown` is running, and another
/// enemy's body pushes the actor out along the deepest overlap. Last, a
/// contact of `attackBody` disarms it and sets `attackLanded`.
static void Actor01500_Fn004EC(Task* actor)
{
    _Actor01500Work*                work;
    ActorContactOverlapPushScratch* frame;
    s32                             push;
    VECTOR*                         normal;
    GfxCoord*                       coord;
    GfxCoord*                       sourceCoord;
    WorldCollisionContact*          effectRec;

    s32 result;
    s32 i;
    s32 depth;
    s32 boundedDepth;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 wallDx;
    s32 wallDy;
    s32 wallDz;
    u32 lastId;
    u32 id;
    u32 hitId;
    u32 damage;

    push   = 0;
    lastId = 0;
    work   = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorContactOverlapPushScratch);
    frame  = SCRATCH_STACK_CURSOR(ActorContactOverlapPushScratch);
    coord  = actor->extra.tmd->coords;
    result = worldCollisionResolvePushback(work->roomContacts, &frame->delta, ARRAY_SIZE(work->roomContacts), NULL);
    if (result != 0) {
        // A push with no vertical part is a wall: rest on it, facing into the first grid contact.
        if (work->noWallPerch == 0 && work->action == ACTOR_01500_ACTION_CHASE && frame->delta.fixed.vy.word == 0) {
            work->action          = ACTOR_01500_ACTION_WALL_REST;
            work->posture         = ACTOR_01500_POSTURE_PERCHED;
            work->roomBody.pos.vy = 0;
            work->roomBody.pos.vz = -300;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer           = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            for (i = 0; i < ARRAY_SIZE(work->roomContacts); i++) {
                if ((work->roomContacts[i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                    frame->gridNormalX = work->roomContacts[i].response.direction.vx;
                    frame->gridNormalZ = work->roomContacts[i].response.direction.vz;
                    work->targetYaw    = (ratan2(frame->gridNormalX, frame->gridNormalZ) + 0x800) & 0xFFF;
                    break;
                }
            }
        }
        // A fall ends once the floor pushes up this hard.
        if (work->action == ACTOR_01500_ACTION_FALL && frame->delta.fixed.vy.word < -0xDDA) {
            work->action = ACTOR_01500_ACTION_GROUNDED;
            work->timer  = 0;
        }
        switch (result) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += frame->delta.fixed.vx.halves.integer;
                coord->coord.t[1] += frame->delta.fixed.vy.halves.integer;
                coord->coord.t[2] += frame->delta.fixed.vz.halves.integer;
                break;
            case 2:
                if (work->action != ACTOR_01500_ACTION_FALL) {
                    coord->coord.t[0] = work->prevPos.vx;
                    coord->coord.t[1] = work->prevPos.vy;
                    coord->coord.t[2] = work->prevPos.vz;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->roomContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    normal = &frame->normal;
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        id = work->contacts[i].key.value;
        switch (id >> 0x10) {
            case 0:
            case 1:
                break;
            case 2: // a player's attack
                if (work->hitCooldown == 0) {
                    sourceCoord            = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
                    dx                     = sourceCoord->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vx = dx;
                    dy                     = sourceCoord->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vy = dy;
                    dz                     = sourceCoord->coord.t[2] - coord->coord.t[2];
                    frame->delta.vector.vz = dz;
                    damage                 = damageComputePlayerAttack(work->contacts[i].key.value, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (damageRollCriticalHit(actor->spawnArg2.pointer, work->contacts[i].key.value, 0) != 0) {
                        damage *= 4;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords, 0, NULL);
                    }
                    worldTargetAddReadoutAmount(&((Enemy*)actor->spawnArg2.pointer)->node, damage, 0);
                    damageAccumulateLifeDrainHp(actor->spawnArg2.pointer, work->contacts[i].key.value, damage, 0);
                    _actor01500ApplyDamage(actor, damage);
                    switch (damageGetPlayerAttackReaction(work->contacts[i].key.value) & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        case 5:
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            damageStartEnemyStagger(actor->spawnArg2.pointer);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(actor->spawnArg2.pointer, work->contacts[i].key.value, 0);
                            break;
                        case 4:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                                work->burstPending = 1;
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                        case 8:
                        case 9:
                            damageStartEnemyBuildup(actor->spawnArg2.pointer, work->contacts[i].key.value, 0);
                            break;
                    }
                    hitId = work->contacts[i].key.value;
                    if (lastId != hitId) {
                        lastId = hitId;
                        effectSpawnHit(damageGetPlayerAttackEffectId(hitId), coord, NULL, &work->hitEffectArg);
                    }
                    damage = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
                    if ((s32)damage > 0) {
                        work->hitCooldown = damage;
                    }
                }
                break;
            case 3: // another enemy's body
                wallDx                 = coord->workm.t[0] - work->contacts[i].point.vx;
                frame->delta.vector.vx = wallDx;
                wallDy                 = coord->workm.t[1] - work->contacts[i].point.vy;
                frame->delta.vector.vy = wallDy;
                wallDz                 = coord->workm.t[2] - work->contacts[i].point.vz;
                frame->delta.vector.vz = wallDz;
                depth                  = work->contacts[i].distance - SquareRoot0((wallDx * wallDx) + (wallDy * wallDy) + (wallDz * wallDz));
                boundedDepth           = depth;
                if (depth <= 0) {
                    boundedDepth = 0;
                }
                depth = boundedDepth;
                if (push < depth) {
                    push = depth;
                    VectorNormal(&frame->delta.vector, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, &frame->pushDirection);
                }
                break;
        }
    }
    if (push > 0) {
        coord->coord.t[0] += (s32)(push * frame->pushDirection.vx) >> 0xC;
        coord->coord.t[2] += (s32)(push * frame->pushDirection.vz) >> 0xC;
    }
    worldCollisionClearContacts(work->contacts);
    effectRec = work->attackContacts;
    if (worldCollisionFindContactIndex(effectRec, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(effectRec);
        work->attackLanded = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactOverlapPushScratch);
}

/// Subtracts damage and selects a flinch or a permanent fall at 60% HP or less.
///
/// `damage` is a signed HP amount; subtraction narrows into the enemy's signed
/// halfword without clamping. Nonpositive HP requests later teardown, which
/// waits for grounding unless a burst is pending. Every call queues the damage
/// cry and publishes this actor's damage alert class, even for zero damage.
static void _actor01500ApplyDamage(Task* actor, s32 damage)
{
    // This actor publishes class 2 on damage; other kinds interpret it separately.
    enum {
        ACTOR_01500_DAMAGE_ALERT_CLASS  = 2,
        ACTOR_01500_CRIPPLED_HP_PERCENT = 60
    };

    Enemy*           enemy;
    Enemy*           soundEnemy;
    _Actor01500Work* work;
    GfxCoord*        rootCoord;
    s32              soundRequest;

    enemy      = actor->spawnArg2.pointer;
    work       = actor->work;
    rootCoord  = actor->extra.tmd->coords;
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        work->deathPending = 1;
    }
    soundEnemy   = actor->spawnArg2.pointer;
    soundRequest = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01500_SOUND_DAMAGE;
    sndEvtRequestScriptStart(soundRequest, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
    if (enemy->hp <= (Actor01500_D09FB8.hpMax * ACTOR_01500_CRIPPLED_HP_PERCENT) / 100) {
        work->posture = ACTOR_01500_POSTURE_CRIPPLED;
        if (work->action != ACTOR_01500_ACTION_GROUNDED) {
            work->action          = ACTOR_01500_ACTION_FALL;
            work->roomBody.pos.vy = -300;
            work->roomBody.pos.vz = 0;
        }
    } else {
        // Keep the playing clip different to force the selected flinch to restart.
        work->action = ACTOR_01500_ACTION_HIT;
        switch (work->posture) {
            case ACTOR_01500_POSTURE_PERCHED:
                if (work->perch == ACTOR_01500_PERCH_WALL) {
                    work->anim        = ACTOR_01500_ANIM_FLINCH_WALL;
                    work->playingAnim = ACTOR_01500_ANIM_PERCH_WALL;
                } else {
                    work->anim        = ACTOR_01500_ANIM_FLINCH_CEILING;
                    work->playingAnim = ACTOR_01500_ANIM_PERCH_CEILING;
                }
                break;
            case ACTOR_01500_POSTURE_AIRBORNE:
                work->anim        = ACTOR_01500_ANIM_FLINCH_AIR;
                work->playingAnim = ACTOR_01500_ANIM_ADVANCE;
                break;
            case ACTOR_01500_POSTURE_CRIPPLED:
                work->anim        = ACTOR_01500_ANIM_FLINCH_AIR;
                work->playingAnim = ACTOR_01500_ANIM_GROUNDED;
                break;
        }
        work->loopSound = 0;
        work->animFrame = 0;
    }
    sceneSetEnemyAlert(ACTOR_01500_DAMAGE_ALERT_CLASS);
}

/// Watches for the player, scene stimuli or damage while on the spawn perch.
///
/// The horizontal proximity test selects alert within 2500 game units;
/// otherwise a delayed noise, a polled enemy alert or lost HP selects take-off.
/// A previously roused actor primes wall take-off before the proximity test.
/// Countdown values measure calls, and the horizontal offset ignores Y.
static void _actor01500PerchAction(Task* actor)
{
    enum { ACTOR_01500_PERCH_ALERT_DISTANCE = 2500 };

    _Actor01500Work* work;
    Enemy*           enemy;
    GfxCoord*        rootCoord;
    VECTOR*          playerOffset;
    s32              takeOffPending;
    s16              alertAnim;
    s16              takeOffAnim; // Hover ticks in the roused path, then the take-off clip
    s16              hoverTicks;

    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    playerOffset   = SCRATCH_STACK_CURSOR(VECTOR);
    work           = actor->work;
    rootCoord      = actor->extra.tmd->coords;
    takeOffPending = 0;
    // Priming a resumed take-off still allows the proximity test to choose alert.
    if (work->roused != 0) {
        work->action         = ACTOR_01500_ACTION_TAKE_OFF;
        work->anim           = ACTOR_01500_ANIM_TAKE_OFF_WALL;
        work->animFrame      = 0;
        takeOffAnim          = Actor01500_D09FC8[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->hoverOffset    = 0;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->timer          = takeOffAnim;
    }
    work->turnRate   = 0;
    playerOffset->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerOffset->vy = 0;
    playerOffset->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    if (SquareRoot0(playerOffset->vx * playerOffset->vx + playerOffset->vz * playerOffset->vz) < ACTOR_01500_PERCH_ALERT_DISTANCE) {
        work->action = ACTOR_01500_ACTION_ALERT;
        alertAnim    = ACTOR_01500_ANIM_ALERT_WALL;
        if (work->perch != ACTOR_01500_PERCH_WALL) {
            alertAnim = ACTOR_01500_ANIM_ALERT_CEILING;
        }
        work->anim        = alertAnim;
        work->loopSound   = ACTOR_01500_SOUND_ALERT;
        work->timer       = 0;
        work->hoverOffset = 0;
        sceneEngageBattle(1);
    } else {
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {
            if (work->timer == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = ((gRandomLcgState >> 16) & 0x1F) + 1;
            }
        }
        if (work->timer != 0) {
            work->timer--;
            if (work->timer <= 0) {
                takeOffPending = 1;
            }
        }
        // Reuse the hover-height field as a staggered alert-poll countdown on the perch.
        work->hoverOffset--;
        if (work->hoverOffset == 0) {
            if (gSceneCombatState.signals.bytes.enemyAlert != 0) {
                takeOffPending = 1;
            }
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hoverOffset = ((gRandomLcgState >> 16) & 0x1F) + 1;
        }
        enemy = actor->spawnArg2.pointer;
        if (enemy->hp != Actor01500_D09FB8.hpMax) {
            takeOffPending = 1;
        }
        if (takeOffPending != 0) {
            work->action = ACTOR_01500_ACTION_TAKE_OFF;
            takeOffAnim  = ACTOR_01500_ANIM_TAKE_OFF_WALL;
            if (work->perch != ACTOR_01500_PERCH_WALL) {
                takeOffAnim = ACTOR_01500_ANIM_TAKE_OFF_CEILING;
            }
            work->anim           = takeOffAnim;
            hoverTicks           = Actor01500_D09FC8[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
            work->hoverOffset    = 0;
            work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
            work->roused         = 1;
            work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
            work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            work->timer          = hoverTicks;
            sceneEngageBattle(1);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Backs away from the perch during take-off, then starts the airborne chase.
///
/// Direct root movement begins at animation tick 30 and chase begins at tick
/// 59. Ceiling take-off also descends, with decreasing speed. Coordinates are
/// in the root's parent frame, with positive Y downward and 4096 angle units
/// per turn; the chase starts with a random settling delay and height offset.
static void _actor01500TakeOffAction(Task* actor)
{
    _Actor01500Work* work;
    GfxCoord*        rootCoord;
    s16              yaw;
    u32              settleDraw;
    u32              heightDraw;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    if (work->animFrame >= 30) {
        work->yaw = yaw = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
        switch (work->perch) {
            case ACTOR_01500_PERCH_WALL:
                rootCoord->coord.t[0] += -(rsin(yaw) * 40) >> ACTOR_01500_FACING_FRACTION_BITS;
                rootCoord->coord.t[2] += -(rcos(work->yaw) * 40) >> ACTOR_01500_FACING_FRACTION_BITS;
                break;
            case ACTOR_01500_PERCH_CEILING:
                rootCoord->coord.t[0] += -(rsin(yaw) * 40) >> ACTOR_01500_FACING_FRACTION_BITS;
                rootCoord->coord.t[2] += -(rcos(work->yaw) * 40) >> ACTOR_01500_FACING_FRACTION_BITS;
                if (work->animFrame < 36) {
                    rootCoord->coord.t[1] += 110;
                } else if (work->animFrame < 46) {
                    rootCoord->coord.t[1] += 35;
                } else {
                    rootCoord->coord.t[1] += 15;
                }
                break;
        }
        if (work->animFrame >= 59) {
            settleDraw           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->action         = ACTOR_01500_ACTION_CHASE;
            work->anim           = ACTOR_01500_ANIM_HOVER;
            work->actionStep     = ACTOR_01500_CHASE_SETTLE;
            work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
            work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            gRandomLcgState      = settleDraw;
            work->timer          = ((settleDraw >> 16) & 0x3F) + 0x3C;
            heightDraw           = settleDraw * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState      = heightDraw;
            work->hoverOffset    = (heightDraw >> 16) & 0x1FF;
        }
    }
}

/// Selects heading and velocity through the chase's hover, advance and dive cycle.
///
/// Settling aims 1800 plus `hoverOffset` game units above the player. An
/// advance may dive once when the horizontal distance is below 1000; only
/// the dive arms the attack sphere. It ends on contact or at the player's
/// height minus 1600, then climbs toward height minus 1800. Timers count calls;
/// the subsequent movement pass consumes the selected velocities.
static void _actor01500ChaseAction(Task* actor)
{
    VECTOR*          playerOffset;
    _Actor01500Work* work;
    GfxCoord*        rootCoord;
    u32              hoverDraw;
    s32              settleHeight;
    u16              advanceDistance;
    u16              hoverTicks;
    u16*             hoverWaits;
    s32              settleDeltaY;
    s32              settleDistanceY;
    s32              climbHeightReference;
    s32              heightY; // Dive stop height, then the climb displacement
    s32              climbDistanceY;

    playerOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work         = actor->work;
    rootCoord    = actor->extra.tmd->coords;
    // Transitions retain this tick's velocities until the next step updates them.
    switch (work->actionStep) {
        case ACTOR_01500_CHASE_SETTLE:
            settleHeight    = work->hoverOffset + 1800;
            settleDeltaY    = gPlayerStatus.coordMtx->t[1] - settleHeight - rootCoord->coord.t[1];
            settleDistanceY = abs(settleDeltaY);
            if (settleDistanceY < 30 || --work->timer <= 0) {
                work->actionStep = ACTOR_01500_CHASE_HOVER;
            } else {
                work->verticalSpeed = settleDeltaY > 0 ? 30 : -30;
            }
            _actor01500AimChaseAtPlayer(work, rootCoord, playerOffset);
            work->roomBody.pos.vy = -300;
            work->roomBody.pos.vz = 0;
            break;
        case ACTOR_01500_CHASE_HOVER:
            work->verticalSpeed = 0;
            work->speed         = 0;
            work->lunged        = 0;
            if (--work->timer < 0) {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                advanceDistance   = Actor01500_D09FE8[(gRandomLcgState >> 16) & (ARRAY_SIZE(Actor01500_D09FE8) - 1)];
                work->anim        = ACTOR_01500_ANIM_ADVANCE;
                work->actionStep  = ACTOR_01500_CHASE_ADVANCE;
                work->advanceLeft = advanceDistance;
            }
            _actor01500AimChaseAtPlayer(work, rootCoord, playerOffset);
            break;
        case ACTOR_01500_CHASE_ADVANCE:
            work->speed        = 200;
            work->advanceLeft -= 200;
            if (work->advanceLeft < 0) {
                hoverWaits           = Actor01500_D09FC8;
                work->anim           = ACTOR_01500_ANIM_HOVER;
                hoverDraw            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                hoverTicks           = hoverWaits[(hoverDraw >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
                gRandomLcgState      = hoverDraw;
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
                work->actionStep     = ACTOR_01500_CHASE_HOVER;
                work->timer          = hoverTicks;
            }
            // Each advance arms at most one dive; the dive exit disarms it.
            if (work->lunged == 0) {
                playerOffset->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                playerOffset->vy = 0;
                playerOffset->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                if (SquareRoot0(playerOffset->vx * playerOffset->vx + playerOffset->vz * playerOffset->vz) < 1000) {
                    work->anim              = ACTOR_01500_ANIM_DIVE;
                    work->actionStep        = ACTOR_01500_CHASE_DIVE;
                    work->loopSound         = 0;
                    work->lunged            = 1;
                    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            break;
        case ACTOR_01500_CHASE_DIVE:
            heightY             = gPlayerStatus.coordMtx->t[1] - 1600;
            work->speed         = 100;
            work->verticalSpeed = 180;
            if (heightY < rootCoord->coord.t[1] || work->attackLanded != 0) {
                work->actionStep        = ACTOR_01500_CHASE_CLIMB;
                work->attackLanded      = 0;
                work->anim              = ACTOR_01500_ANIM_ADVANCE;
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer             = ((gRandomLcgState >> 16) & 0xF) + 15;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case ACTOR_01500_CHASE_CLIMB:
            climbHeightReference = rootCoord->coord.t[1] + 1800;
            heightY              = gPlayerStatus.coordMtx->t[1] - climbHeightReference;
            climbDistanceY       = abs(heightY);
            if (climbDistanceY < 96 || --work->timer <= 0) {
                work->actionStep = ACTOR_01500_CHASE_ADVANCE;
            } else {
                work->verticalSpeed = heightY > 0 ? 96 : -96;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Edges along the floor away from the player and expires outside its height band.
///
/// Positive Y is downward. A root more than 500 game units below or 1800 above
/// the player, or more than 1800 calls in this action, requests death. The
/// downward velocity keeps the room sphere in contact with the floor.
static void _actor01500GroundedAction(Task* actor)
{
    _Actor01500Work* work;
    GfxCoord*        rootCoord;
    VECTOR*          awayFromPlayer;

    work                = actor->work;
    rootCoord           = actor->extra.tmd->coords;
    work->anim          = ACTOR_01500_ANIM_GROUNDED;
    work->speed         = 5;
    work->turnRate      = 5;
    work->loopSound     = 0;
    work->verticalSpeed = 128;
    awayFromPlayer      = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    awayFromPlayer->vx  = rootCoord->coord.t[0] - gPlayerStatus.coordMtx->t[0];
    awayFromPlayer->vy  = 0;
    awayFromPlayer->vz  = rootCoord->coord.t[2] - gPlayerStatus.coordMtx->t[2];
    work->targetYaw     = ratan2((s16)awayFromPlayer->vx, (s16)awayFromPlayer->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    if (rootCoord->coord.t[1] > gPlayerStatus.coordMtx->t[1] + 500 ||
        rootCoord->coord.t[1] < gPlayerStatus.coordMtx->t[1] - 1800 ||
        ++work->timer > 1800) {
        work->deathPending = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Ends a hit flinch and resumes the action selected by the interrupted posture.
///
/// A perched flinch lasts 20 animation ticks, an airborne one 10; the crippled
/// path grounds the actor after the first tick. The perch path returns to the
/// perch handler with a take-off clip selected, while the airborne path starts
/// chase settling with a random wait.
static void _actor01500HitAction(Task* actor)
{
    _Actor01500Work* work = actor->work;
    s16              takeOffAnim;
    u32              hoverDraw;
    u16              hoverTicks;
    u16*             hoverWaits;

    switch (work->posture) {
        case ACTOR_01500_POSTURE_PERCHED:
            takeOffAnim = ACTOR_01500_ANIM_TAKE_OFF_WALL;
            if (work->animFrame >= 20) {
                work->action = ACTOR_01500_ACTION_PERCH;
                if (work->perch != ACTOR_01500_PERCH_WALL) {
                    takeOffAnim = ACTOR_01500_ANIM_TAKE_OFF_CEILING;
                }
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->anim           = takeOffAnim;
                work->actionStep     = 0;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            }
            break;
        case ACTOR_01500_POSTURE_AIRBORNE:
            if (work->animFrame >= 10) {
                hoverWaits           = Actor01500_D09FC8;
                work->action         = ACTOR_01500_ACTION_CHASE;
                work->anim           = ACTOR_01500_ANIM_HOVER;
                work->actionStep     = ACTOR_01500_CHASE_SETTLE;
                hoverDraw            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState      = hoverDraw;
                hoverTicks           = hoverWaits[(hoverDraw >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
                work->timer          = hoverTicks;
            }
            break;
        case ACTOR_01500_POSTURE_CRIPPLED:
            if (work->animFrame > 0) {
                work->action    = ACTOR_01500_ACTION_GROUNDED;
                work->timer     = 0;
                work->anim      = ACTOR_01500_ANIM_GROUNDED;
                work->loopSound = 0;
            }
            break;
    }
}

/// Turns the root toward its target heading and rebuilds its yaw-only rotation.
///
/// Headings use 4096 units per turn and the change is limited by `turnRate`.
/// The current heading is read from the matrix on every call. Half-turn ties
/// use the wrapped branch; the stored result may lie outside 0..4095 and is
/// passed directly to `RotMatrix`. Rebuilding replaces pitch, roll and scale;
/// translation is retained.
static void _actor01500TurnTowardTarget(Task* actor)
{
    _Actor01500Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               yawDelta;
    s32               yawDistance;
    s32               maxTurn;
    s32               wrappedYaw;
    s32               nextYaw;
    s32               wrappedMaxTurn;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord   = actor->extra.tmd->coords;
    work        = actor->work;
    currentYaw  = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw   = work->targetYaw;
    yawDelta    = targetYaw - currentYaw;
    yawDistance = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (yawDistance < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        maxTurn = work->turnRate;
        if (maxTurn >= yawDistance) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDelta <= 0) {
                nextYaw -= maxTurn;
            } else {
                nextYaw += maxTurn;
            }
            work->yaw = nextYaw;
        }
    } else {
        maxTurn = work->turnRate;
        if (yawDelta > 0 ? maxTurn >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : maxTurn >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrappedMaxTurn = work->turnRate;
            wrappedYaw     = work->yaw;
            if (yawDelta > 0) {
                work->yaw = wrappedYaw - wrappedMaxTurn;
            } else {
                work->yaw = wrappedYaw + wrappedMaxTurn;
            }
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Saves the root position, applies flight velocity and hover bob, and checks retirement.
///
/// Forward motion uses the root's 12-fractional-bit facing basis; vertical
/// speed and the bob are parent-frame game units per call, positive downward.
/// Only the hover clip advances the 15-tick bob cycle. A root more than 5000
/// units below the player enters the lost-actor teardown phase.
static void _actor01500Move(Task* actor)
{
    // The final element of the 16-entry table is outside the active bob cycle.
    enum {
        ACTOR_01500_HOVER_BOB_TICKS = 15,
        ACTOR_01500_RETIRE_DROP     = 5000
    };

    _Actor01500Work* work;
    GfxCoord*        rootCoord;
    s16              bobOffset;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    bobOffset = 0;
    if (work->anim == ACTOR_01500_ANIM_HOVER) {
        work->bobPhase++;
        if (work->bobPhase >= ACTOR_01500_HOVER_BOB_TICKS) {
            work->bobPhase = 0;
        }
        bobOffset = Actor01500_D0A070[work->bobPhase];
    }
    // Save the complete root position for the next contact pass's reset response.
    work->prevPos.vx       = rootCoord->coord.t[0];
    work->prevPos.vy       = rootCoord->coord.t[1];
    work->prevPos.vz       = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->speed) >> ACTOR_01500_FACING_FRACTION_BITS;
    rootCoord->coord.t[1] += work->verticalSpeed + bobOffset;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->speed) >> ACTOR_01500_FACING_FRACTION_BITS;
    // Retire a lost actor through the delayed teardown path.
    if (rootCoord->coord.t[1] - gPlayerStatus.coordMtx->t[1] > ACTOR_01500_RETIRE_DROP) {
        actor->state     = 2;
        work->action     = ACTOR_01500_ACTION_DIE;
        work->actionStep = ACTOR_01500_DEATH_LOST;
        work->timer      = 0;
    }
}

static void Actor01500_Fn01AB0(Task* arg0)
{
    GameLocationKey  key;
    u8               areaByte0;
    u32              raw1, index1;
    EffectWork*      effect1;
    TmdObject*       model1;
    AreaPlacement*   entry1;
    GameLocationKey* sessionKey1;
    u32              raw2, index2;
    EffectWork*      effect2;
    TmdObject*       model2;
    AreaPlacement*   entry2;
    GameLocationKey* sessionKey2;
    u32              raw3, index3;
    EffectWork*      effect3;
    TmdObject*       model3;
    AreaPlacement*   entry3;
    GameLocationKey* sessionKey3;
    u32              raw4, index4;
    EffectWork*      effect4;
    TmdObject*       model4;
    AreaPlacement*   entry4;
    GameLocationKey* sessionKey4;

    D_80067704[0] = &_gActor01500MindSucklerBurstHead;
    effect1       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect1 != NULL) {
        sessionKey1 = &gGameSession->location.loc;
        raw1        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model1      = effect1->task->extra.tmd;
        key.stage   = sessionKey1->stage;
        key.area    = sessionKey1->area;
        key.room    = sessionKey1->room;
        areaByte0   = gGameSession->location.loc.view;
        index1      = raw1 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry1                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index1);
        model1->texturePageOffset = entry1->texturePageOffset;
        model1->clutRowOffset     = entry1->clutRowOffset;
        if (model1->buffer != NULL) {
            tmdBuildBufferHalf(model1);
            tmdBuildBufferHalf(model1);
        }
    }

    D_80067704[0] = &_gActor01500MindSucklerBurstWing;
    effect2       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect2 != NULL) {
        sessionKey2 = &gGameSession->location.loc;
        raw2        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model2      = effect2->task->extra.tmd;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->location.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry2                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index2);
        model2->texturePageOffset = entry2->texturePageOffset;
        model2->clutRowOffset     = entry2->clutRowOffset;
        if (model2->buffer != NULL) {
            tmdBuildBufferHalf(model2);
            tmdBuildBufferHalf(model2);
        }
    }

    D_80067704[0] = &_gActor01500MindSucklerBurstStinger;
    effect3       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect3 != NULL) {
        sessionKey3 = &gGameSession->location.loc;
        raw3        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model3      = effect3->task->extra.tmd;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->location.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry3                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index3);
        model3->texturePageOffset = entry3->texturePageOffset;
        model3->clutRowOffset     = entry3->clutRowOffset;
        if (model3->buffer != NULL) {
            tmdBuildBufferHalf(model3);
            tmdBuildBufferHalf(model3);
        }
    }

    D_80067704[0] = &_gActor01500MindSucklerBurstTail;
    effect4       = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
    if (effect4 != NULL) {
        sessionKey4 = &gGameSession->location.loc;
        raw4        = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        model4      = effect4->task->extra.tmd;
        key.stage   = sessionKey4->stage;
        key.area    = sessionKey4->area;
        key.room    = sessionKey4->room;
        areaByte0   = gGameSession->location.loc.view;
        index4      = raw4 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry4                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index4);
        model4->texturePageOffset = entry4->texturePageOffset;
        model4->clutRowOffset     = entry4->clutRowOffset;
        if (model4->buffer != NULL) {
            tmdBuildBufferHalf(model4);
            tmdBuildBufferHalf(model4);
        }
    }
}

/// Per-frame handler for the death sequence. Scene mode 1 only refreshes the
/// actor colour and mode 2 hides the model. Otherwise `actionStep` steps
/// through `ACTOR_01500_DEATH_*`: `BEGIN` saves the root matrix and unlinks
/// the actor, `SHRINK` runs `Actor01500_Fn02C34` and spawns an effect at tick
/// 15, `BURST` frees the model's buffers and spawns the burst parts once
/// `burstPending` reaches 2, and `LOST` unlinks on its first tick; those three
/// move to `DESTROY` once `timer` has counted their ticks, which destroys the
/// enemy.
static void Actor01500_Fn01DF0(Enemy* arg0, Task* arg1)
{
    VECTOR           pos;
    _Actor01500Work* work;
    TmdObject*       model;
    GfxCoord*        coord;
    GfxCoord*        sub;

    model = arg1->extra.tmd;
    work  = arg1->work;
    coord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            sub    = &coord[1];
            pos.vx = sub->workm.t[0];
            pos.vy = sub->workm.t[1];
            pos.vz = sub->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    switch (work->actionStep) {
        case ACTOR_01500_DEATH_BEGIN:
            work->deathScaleY  = ONE;
            work->deathBaseMtx = coord->coord;
            arg0->recs         = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->body);
            worldCollisionUnlinkBody(&work->roomBody);
            worldCollisionUnlinkBody(&work->attackBody);
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(arg1, 0xF);
            work->timer      = 0;
            work->actionStep = ACTOR_01500_DEATH_SHRINK;
            if (work->burstPending != 0) {
                model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->actionStep = ACTOR_01500_DEATH_BURST;
            }
            break;
        case ACTOR_01500_DEATH_SHRINK:
            Actor01500_Fn02C34(arg1);
            work->timer++;
            if (work->timer == 10) {
                model->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer == 15) {
                effectSpawn(EFFECT_CORPSE_BURN, coord, 2, NULL);
            }
            if (work->timer >= 60) {
                work->actionStep = ACTOR_01500_DEATH_DESTROY;
            }
            break;
        case ACTOR_01500_DEATH_DESTROY:
            enemyDestroy(arg0, arg1);
            return;
        case ACTOR_01500_DEATH_BURST:
            if (work->burstPending != 0) {
                if (work->burstPending >= 2) {
                    work->burstPending = 0;
                    tmdFreePrimitiveBuffer(model);
                    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    Actor01500_Fn01AB0(arg1);
                } else {
                    work->burstPending++;
                }
            }
            work->timer++;
            if (work->timer >= 60) {
                work->actionStep = ACTOR_01500_DEATH_DESTROY;
            }
            break;
        case ACTOR_01500_DEATH_LOST:
            if (work->timer == 0) {
                worldTargetUnlinkNode(&arg0->node);
                worldCollisionUnlinkBody(&work->body);
                worldCollisionUnlinkBody(&work->roomBody);
                worldCollisionUnlinkBody(&work->attackBody);
                sceneReleaseBattleRefWithRewards(arg1, 0xF);
            }
            work->timer++;
            if (work->timer >= 61) {
                work->actionStep = ACTOR_01500_DEATH_DESTROY;
            }
            break;
    }
    sub    = arg1->extra.tmd->coords;
    sub    = &sub[1];
    pos.vx = sub->workm.t[0];
    pos.vy = sub->workm.t[1];
    pos.vz = sub->workm.t[2];
    worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
}

/// Hovers and advances around a fixed room point until the player enters the trigger region.
///
/// Settling aims 800 plus `hoverOffset` game units above `Actor01500_D0A090`.
/// The trigger requires player X above `Actor01500_D0A098.vx` and Z below
/// `Actor01500_D0A098.vz`; Y is ignored. It then engages battle and starts
/// ordinary chase settling. Timers count calls and heading uses 4096 units
/// per turn in the root's parent frame.
static void _actor01500ScriptedAction(Task* actor)
{
    VECTOR*          targetOffset;
    _Actor01500Work* work;
    GfxCoord*        rootCoord;
    s32              settleDeltaY;
    s32              settleDistanceY;
    u16              advanceDistance;
    u16              hoverTicks;
    u16*             hoverWaits;
    s32              settleHeight;
    s32              settleTicks;

    targetOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work         = actor->work;
    rootCoord    = actor->extra.tmd->coords;
    switch (work->actionStep) {
        case ACTOR_01500_CHASE_SETTLE:
            settleHeight    = work->hoverOffset + 800;
            settleDeltaY    = Actor01500_D0A090.vy - settleHeight - rootCoord->coord.t[1];
            settleDistanceY = abs(settleDeltaY);
            if (settleDistanceY < 30 || --work->timer <= 0) {
                work->actionStep = ACTOR_01500_CHASE_HOVER;
            } else {
                work->verticalSpeed = settleDeltaY > 0 ? 30 : -30;
            }
            targetOffset->vx      = Actor01500_D0A090.vx - rootCoord->coord.t[0];
            targetOffset->vy      = 0;
            targetOffset->vz      = Actor01500_D0A090.vz - rootCoord->coord.t[2];
            work->targetYaw       = ratan2((s16)targetOffset->vx, (s16)targetOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->turnRate        = 100;
            work->roomBody.pos.vy = -300;
            work->roomBody.pos.vz = 0;
            break;
        case ACTOR_01500_CHASE_HOVER:
            work->verticalSpeed = 0;
            work->speed         = 0;
            work->lunged        = 0;
            if (--work->timer < 0) {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                advanceDistance   = Actor01500_D09FE8[(gRandomLcgState >> 16) & (ARRAY_SIZE(Actor01500_D09FE8) - 1)];
                work->anim        = ACTOR_01500_ANIM_ADVANCE;
                work->actionStep  = ACTOR_01500_CHASE_ADVANCE;
                work->advanceLeft = advanceDistance;
            }
            targetOffset->vx = Actor01500_D0A090.vx - rootCoord->coord.t[0];
            targetOffset->vy = 0;
            targetOffset->vz = Actor01500_D0A090.vz - rootCoord->coord.t[2];
            work->targetYaw  = ratan2((s16)targetOffset->vx, (s16)targetOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->turnRate   = 100;
            break;
        case ACTOR_01500_CHASE_ADVANCE:
            work->speed        = 200;
            work->advanceLeft -= 200;
            if (work->advanceLeft < 0) {
                hoverWaits           = Actor01500_D09FC8;
                work->anim           = ACTOR_01500_ANIM_HOVER;
                hoverTicks           = hoverWaits[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
                work->actionStep     = ACTOR_01500_CHASE_HOVER;
                work->timer          = hoverTicks;
            }
            break;
    }
    // Crossing both bounds switches from the fixed room point to the player.
    if (gPlayerStatus.coordMtx->t[0] > Actor01500_D0A098.vx && gPlayerStatus.coordMtx->t[2] < Actor01500_D0A098.vz) {
        work->action         = ACTOR_01500_ACTION_CHASE;
        settleTicks          = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x3F) + 60;
        work->anim           = ACTOR_01500_ANIM_HOVER;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->actionStep     = ACTOR_01500_CHASE_SETTLE;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->roused         = 1;
        work->timer          = settleTicks;
        work->hoverOffset    = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1FF;
        sceneEngageBattle(1);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Runs the task's current state handler from `Actor01500_D00004`, copying
/// the table onto the stack before the call.
static void Actor01500_Fn02428(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor01500_D00004;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame handler. Scene mode 1 only recolours and shadows the actor and
/// mode 2 hides it; otherwise it applies pending hit reactions and contacts,
/// hands off to the death state once `deathPending` is raised and the actor
/// is grounded or about to burst, runs the action, turns, moves, animates
/// and voices the actor and rebuilds its root coordinate.
static void Actor01500_Fn02484(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    _Actor01500Work* work;
    s32              state;

    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    switch (state) {
        case 0:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case 1:
            Actor01500_Fn02B14(arg1);
            Actor01500_Fn02B70(arg1);
            return;
        case 2:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = 1;
            return;
    }
    if (arg0->reactionFlags != 0) {
        _actor01500ApplyReactions(arg1);
    }
    Actor01500_Fn004EC(arg1);
    if (work->deathPending != 0) {
        if ((work->action == ACTOR_01500_ACTION_GROUNDED) || (work->burstPending != 0)) {
            work->action     = ACTOR_01500_ACTION_DIE;
            work->actionStep = ACTOR_01500_DEATH_BEGIN;
            arg1->state      = 2;
        }
    }
    _actor01500UpdateAction(arg1);
    if (work->turnRate != 0) {
        _actor01500TurnTowardTarget(arg1);
    }
    _actor01500Move(arg1);
    _actor01500UpdateAnimation(arg1);
    Actor01500_Fn02A1C(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    Actor01500_Fn02B14(arg1);
    Actor01500_Fn02B70(arg1);
}

/// Consumes stagger and buildup requests and applies active damage-over-time pulses.
///
/// Stagger and buildup select the airborne flinch, stop its loop sound and
/// start a fall unless the actor is already crippled. Each nonzero pulse goes
/// through the ordinary damage handler and adds its HP readout; the stored
/// damage-over-time bits remain set until the shared expiry test succeeds.
static void _actor01500ApplyReactions(Task* actor)
{
    Enemy*           enemy;
    _Actor01500Work* work;
    s32              pulseDamage;
    u8               pendingReactions;

    enemy            = actor->spawnArg2.pointer;
    pendingReactions = enemy->reactionFlags;
    work             = actor->work;
    if (pendingReactions & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = pendingReactions & ENEMY_REACTION_STAGGER_CLEAR;
        _actor01500StartReactionFall(work);
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        _actor01500StartReactionFall(work);
    }
    // Pulse damage can replace the reaction selected above with a hit or crippled fall.
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        pulseDamage = damageTickEnemyDamageOverTime(enemy);
        if (pulseDamage != 0) {
            _actor01500ApplyDamage(actor, pulseDamage);
            worldTargetAddReadoutAmount(&enemy->node, pulseDamage, 0);
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Runs the current behavior action; the death action has no active handler.
static void _actor01500UpdateAction(Task* actor)
{
    _Actor01500Work* work = actor->work;

    switch (work->action) {
        case ACTOR_01500_ACTION_PERCH:
            _actor01500PerchAction(actor);
            break;
        case ACTOR_01500_ACTION_ALERT:
            _actor01500AlertAction(actor);
            break;
        case ACTOR_01500_ACTION_TAKE_OFF:
            _actor01500TakeOffAction(actor);
            break;
        case ACTOR_01500_ACTION_CHASE:
            _actor01500ChaseAction(actor);
            break;
        case ACTOR_01500_ACTION_FALL:
            _actor01500FallAction(actor);
            break;
        case ACTOR_01500_ACTION_GROUNDED:
            _actor01500GroundedAction(actor);
            break;
        case ACTOR_01500_ACTION_WALL_REST:
            _actor01500WallRestAction(actor);
            break;
        case ACTOR_01500_ACTION_HIT:
            _actor01500HitAction(actor);
            break;
        case ACTOR_01500_ACTION_SCRIPTED:
            _actor01500ScriptedAction(actor);
            break;
    }
}

/// Signals the player's presence from the perch, then begins take-off.
///
/// At call 60 it publishes this actor's proximity alert class and latches the
/// scene noise signal. At call 90 it selects wall or ceiling take-off, marks
/// the actor airborne and roused, and loads a random hover wait.
static void _actor01500AlertAction(Task* actor)
{
    enum {
        ACTOR_01500_PROXIMITY_ALERT_CLASS = 1,
        ACTOR_01500_ALERT_SIGNAL_TICKS    = 60,
        ACTOR_01500_ALERT_TAKE_OFF_TICKS  = 90
    };

    _Actor01500Work* work = actor->work;
    s16              takeOffAnim;
    u32              hoverDraw;
    u16              hoverTicks;

    if (++work->timer == ACTOR_01500_ALERT_SIGNAL_TICKS) {
        sceneSetEnemyAlert(ACTOR_01500_PROXIMITY_ALERT_CLASS);
        sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_NOISE);
    }
    takeOffAnim = ACTOR_01500_ANIM_TAKE_OFF_WALL;
    if (work->timer >= ACTOR_01500_ALERT_TAKE_OFF_TICKS) {
        work->action = ACTOR_01500_ACTION_TAKE_OFF;
        if (work->perch != ACTOR_01500_PERCH_WALL) {
            takeOffAnim = ACTOR_01500_ANIM_TAKE_OFF_CEILING;
        }
        work->anim           = takeOffAnim;
        hoverDraw            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState      = hoverDraw;
        hoverTicks           = Actor01500_D09FC8[(hoverDraw >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->roused         = 1;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->timer          = hoverTicks;
    }
}

/// Sets a forward drift and downward fall with the room sphere above the root.
///
/// Velocities are 20 forward and 128 downward game units per call. The contact
/// pass handles the transition to grounded when the floor pushes upward.
static void _actor01500FallAction(Task* actor)
{
    _Actor01500Work* work = actor->work;

    work->speed           = 20;
    work->verticalSpeed   = 128;
    work->roomBody.pos.vy = -300;
    work->roomBody.pos.vz = 0;
}

/// Holds still against a wall until the rest timer expires, then begins wall take-off.
///
/// The countdown measures calls and transitions only on reaching zero. Leaving
/// rest clears the action step, selects wall perch and reloads the hover wait;
/// it leaves animation restart to the animation update.
static void _actor01500WallRestAction(Task* actor)
{
    _Actor01500Work* work = actor->work;
    u32              hoverDraw;
    u16              hoverTicks;
    u16*             hoverWaits;

    work->anim          = ACTOR_01500_ANIM_WALL_REST;
    work->loopSound     = 0;
    work->speed         = 0;
    work->verticalSpeed = 0;
    if (--work->timer == 0) {
        hoverWaits           = Actor01500_D09FC8;
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->action         = ACTOR_01500_ACTION_TAKE_OFF;
        work->anim           = ACTOR_01500_ANIM_TAKE_OFF_WALL;
        work->actionStep     = 0;
        hoverDraw            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState      = hoverDraw;
        hoverTicks           = hoverWaits[(hoverDraw >> 16) & (ARRAY_SIZE(Actor01500_D09FC8) - 1)];
        work->perch          = ACTOR_01500_PERCH_WALL;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->timer          = hoverTicks;
    }
}

/// Restarts a changed clip with its blend duration or advances the current clip.
///
/// The action supplies a clip index in 1..14. Slots 1..6 drive the six parts
/// below the root; slot zero is untouched. `Actor01500_D0A050` entries count
/// whole normal-rate blend frames. A restart resets `animFrame` and blends
/// from each slot's ticked pose; otherwise the counter advances once per call.
static void _actor01500UpdateAnimation(Task* actor)
{
    _Actor01500Work* work;
    s32              slotIndex;
    s32              blendFrames;

    work = actor->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        blendFrames       = Actor01500_D0A050[work->anim];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Sound tick: while `loopSound` holds a request, queues it on every third
/// tick of the animation, tagged with the placement index, and once
/// `loopSoundTimer` runs out switches to `ACTOR_01500_SOUND_FLIGHT`.
static void Actor01500_Fn02A1C(Task* arg0)
{
    s32              soundId;
    s32              objectSoundId;
    s32              pan;
    GfxCoord*        object;
    _Actor01500Work* work;

    work          = arg0->work;
    objectSoundId = work->loopSound;
    object        = arg0->extra.tmd->coords;
    if (objectSoundId != 0) {
        if ((s16)(work->animFrame % 3) == 1) {
            soundId = objectSoundId | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan     = (s8)worldCoordGetOriginAudioPan(object);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
        }
        if (work->loopSoundTimer > 0) {
            if (--work->loopSoundTimer <= 0) {
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT;
                work->loopSoundTimer = 0;
            }
        }
    }
}

/// Hands `worldCoordUpdateActorColor` the world position of the model's second
/// coordinate, with zero for the unused arguments.
static void Actor01500_Fn02B14(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = &arg0->extra.tmd->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Ground shadow for the actor: carves a `VECTOR3` off the scratchpad and fills
/// it from the root coordinate's world translation - straight out of `workm.t`
/// while grounded, otherwise from the hit point `worldCollisionProjectGroundPoint` finds casting a
/// ray down. The shade passed to `effectDrawGroundShadow` is `0x80` while
/// grounded, otherwise `effectGetGroundShadowShade`'s reading of the ray's drop.
static void Actor01500_Fn02B70(Task* arg0)
{
    _Actor01500Work* work;
    GfxCoord*        coord;
    VECTOR3*         vec;
    VECTOR*          head;
    s16              hit;

    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    work                         = arg0->work;
    coord                        = arg0->extra.tmd->coords;
    SCRATCH_STACK_CURSOR(VECTOR) = head - 1;
    vec                          = (VECTOR3*)(head - 1);
    if (work->action != ACTOR_01500_ACTION_GROUNDED) {
        hit = worldCollisionProjectGroundPoint(MATRIX_TRANS(&coord->workm), vec);
        if (hit != 0) {
            effectDrawGroundShadow(vec, 0x200, effectGetGroundShadowShade(0x200, 0x80, hit));
        }
    } else {
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        effectDrawGroundShadow(vec, 0x200, 0x80);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Death shrink: restores the root coordinate from `deathBaseMtx`, saved
/// when the death sequence began, and squashes it along Y by `deathScaleY`,
/// which winds down by 80 a frame until it reaches 512. The
/// scale is applied through an identity rotation carved off the scratchpad,
/// `ScaleMatrix` and `MulMatrix`, and `composeStamp` is cleared so the coordinate's work
/// matrix is rebuilt.
static void Actor01500_Fn02C34(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    _Actor01500Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->deathScaleY > 512) {
        work->deathScaleY -= 80;
    }
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->deathScaleY;
    scratch->scale.vz = ONE;
    coord->coord      = work->deathBaseMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
