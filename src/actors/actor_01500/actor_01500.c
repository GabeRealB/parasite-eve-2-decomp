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

/// Points `Actor01500_Fn020D8` measures against: the XZ of
/// `Actor01500_D0A090` is where it heads, its Y (`Actor01500_D0A090.vy`) the
/// height it settles below, and `gPlayerStatus.coordMtx` passing the X/Z bounds of
/// `Actor01500_D0A098` ends the state.
extern SVECTOR Actor01500_D0A090;
extern SVECTOR Actor01500_D0A098;

static void Actor01500_Fn00094(Enemy* arg0, Task* arg1);
static void Actor01500_Fn004EC(Task* actor);
static void Actor01500_Fn00AFC(Task* actor, s32 damage);
static void Actor01500_Fn00CA4(Task* actor);
static void Actor01500_Fn00FC4(Task* actor);
static void Actor01500_Fn011B0(Task* actor);
static void Actor01500_Fn015DC(Task* actor);
static void Actor01500_Fn01708(Task* actor);
static void Actor01500_Fn01838(Task* actor);
static void Actor01500_Fn01988(Task* actor);
static void Actor01500_Fn01AB0(Task* arg0);
static void Actor01500_Fn01DF0(Enemy* arg0, Task* arg1);
static void Actor01500_Fn020D8(Task* actor);
static void Actor01500_Fn02428(Task* task);
static void Actor01500_Fn02484(Enemy* enemy, Task* actor);
static void Actor01500_Fn025C8(Task* actor);
static void Actor01500_Fn026D8(Task* actor);
static void Actor01500_Fn027B0(Task* actor);
static void Actor01500_Fn0288C(Task* actor);
static void Actor01500_Fn028B0(Task* actor);
static void Actor01500_Fn02958(Task* actor);
static void Actor01500_Fn02A1C(Task* actor);
static void Actor01500_Fn02B14(Task* actor);
static void Actor01500_Fn02B70(Task* actor);
static void Actor01500_Fn02C34(Task* actor);

/// The actor's three task states - spawn, per-frame tick and teardown - run
/// by `Actor01500_Fn02428`.
static const EnemyTaskFuncTable3 Actor01500_D00004 = {
    {
        Actor01500_Fn00094,
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

/// Spawn handler: allocates the work block, binds the model's matrices and
/// the three collision bodies, and picks the first action and animation from
/// the placement's variant and mode.
static void Actor01500_Fn00094(Enemy* arg0, Task* arg1)
{
    _Actor01500Work*       work;
    TmdObject*             obj;
    GfxCoord*              coord;
    AreaPlacement*         place;
    WorldCollisionContact* records;
    u32                    draw;
    s32                    i;
    s32                    r;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor01500Work), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->coord                   = &arg1->extra.tmd->coords[2];
    arg0->node.state.parts.flags  = 0;
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vy              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &Actor01500_D09FB8;
    arg0->recs                    = work->contacts;
    arg0->hp                      = Actor01500_D09FB8.hpMax;
    work->hitEffectArg.coord      = coord;
    work->hitEffectArg.spawnArgLo = 0x300;
    work->hitEffectArg.spawnArgHi = 1;
    place                         = arg0->place;
    switch (work->variant = place->variant) {
        case ACTOR_01500_VARIANT_PERCHED:
            work->perch       = arg0->place->mode & 1;
            work->noWallPerch = (arg0->place->mode >> 1) & 1;
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
            draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hoverOffset      = ((draw >> 16) & 0x1F) + 1;
            work->targetYaw        = (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) + 0x800) & 0xFFF;
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
    (sceneAcquireBattleRef)(0);
    animationInitContext(&work->rig.anim, Actor01500_D0A014, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, work->anim);
    }
    if (work->variant == ACTOR_01500_VARIANT_PERCHED) {
        // Restart the perch animation over a random blend, so that neighbours fall out of step.
        draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        r                      = (draw >> 16) & 0x3F;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, r);
        }
    }
    work->body.coord            = &arg1->extra.tmd->coords[2];
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3000F;
    work->body.radius           = 300;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->roomBody.context.contacts = work->roomContacts;
    work->roomBody.coord            = coord;
    work->roomBody.pos.vx           = 0;
    work->body.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    if (work->perch == ACTOR_01500_PERCH_WALL) {
        work->roomBody.pos.vy = 0;
        work->roomBody.pos.vz = -300;
    } else {
        work->roomBody.pos.vy = 300;
        work->roomBody.pos.vz = 0;
    }
    work->roomBody.key    = 0x3000F;
    work->roomBody.radius = 300;
    work->roomBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->roomBody);
    worldCollisionInitContacts(work->roomContacts, ARRAY_SIZE(work->roomContacts), 0);
    records                           = work->attackContacts;
    work->attackBody.coord            = coord;
    work->attackBody.context.contacts = records;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 400;
    work->roomBody.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&Actor01500_D09FB4, 0);
    work->attackBody.radius           = 300;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(records, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->state             = 1;
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
                    Actor01500_Fn00AFC(actor, damage);
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

static void Actor01500_Fn00AFC(Task* actor, s32 damage)
{
    Enemy*           enemy;
    _Actor01500Work* work;
    GfxCoord*        coord;
    s32              id;

    enemy      = actor->spawnArg2.pointer;
    work       = actor->work;
    coord      = actor->extra.tmd->coords;
    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        work->deathPending = 1;
    }
    id = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01500_SOUND_DAMAGE;
    sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    if (enemy->hp <= (Actor01500_D09FB8.hpMax * 60) / 100) {
        work->posture = ACTOR_01500_POSTURE_CRIPPLED;
        if (work->action != ACTOR_01500_ACTION_GROUNDED) {
            work->action          = ACTOR_01500_ACTION_FALL;
            work->roomBody.pos.vy = -300;
            work->roomBody.pos.vz = 0;
        }
    } else {
        // Each flinch names another animation as the playing one, so that it restarts.
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
    sceneSetEnemyAlert(2);
}

/// `ACTOR_01500_ACTION_PERCH`: waits on the spawn perch. The player coming
/// within 2500 units starts `ACTION_ALERT`. Otherwise a noise in the scene
/// (after 1 to 32 ticks), the scene's enemy alert (looked at every 1 to 32
/// ticks) or lost hit points start the take-off, as does a perch the actor
/// has already been `roused` from.
static void Actor01500_Fn00CA4(Task* actor)
{
    _Actor01500Work* work;
    GfxCoord*        coord;
    VECTOR*          frame;
    s32              flag;
    s16              anim;
    s16              takeOffAnim; // carries the roused wait first; a local of its own for that changes the register allocation
    s16              val;

    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    frame = SCRATCH_STACK_CURSOR(VECTOR);
    work  = actor->work;
    coord = actor->extra.tmd->coords;
    flag  = 0;
    if (work->roused != 0) {
        work->action         = ACTOR_01500_ACTION_TAKE_OFF;
        work->anim           = ACTOR_01500_ANIM_TAKE_OFF_WALL;
        work->animFrame      = 0;
        takeOffAnim          = Actor01500_D09FC8[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->hoverOffset    = 0;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->timer          = takeOffAnim;
    }
    work->turnRate = 0;
    frame->vx      = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    frame->vy      = 0;
    frame->vz      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (SquareRoot0(frame->vx * frame->vx + frame->vz * frame->vz) < 2500) {
        work->action = ACTOR_01500_ACTION_ALERT;
        anim         = ACTOR_01500_ANIM_ALERT_WALL;
        if (work->perch != ACTOR_01500_PERCH_WALL) {
            anim = ACTOR_01500_ANIM_ALERT_CEILING;
        }
        work->anim        = anim;
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
                flag = 1;
            }
        }
        // On the perch `hoverOffset` times the looks at the enemy alert.
        work->hoverOffset--;
        if (work->hoverOffset == 0) {
            if (gSceneCombatState.signals.bytes.enemyAlert != 0) {
                flag = 1;
            }
            gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->hoverOffset = ((gRandomLcgState >> 16) & 0x1F) + 1;
        }
        if (((Enemy*)actor->spawnArg2.pointer)->hp != Actor01500_D09FB8.hpMax) {
            flag = 1;
        }
        if (flag != 0) {
            work->action = ACTOR_01500_ACTION_TAKE_OFF;
            takeOffAnim  = ACTOR_01500_ANIM_TAKE_OFF_WALL;
            if (work->perch != ACTOR_01500_PERCH_WALL) {
                takeOffAnim = ACTOR_01500_ANIM_TAKE_OFF_CEILING;
            }
            work->anim           = takeOffAnim;
            val                  = Actor01500_D09FC8[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->hoverOffset    = 0;
            work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
            work->roused         = 1;
            work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
            work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            work->timer          = val;
            sceneEngageBattle(1);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// `ACTOR_01500_ACTION_TAKE_OFF`: once the take-off animation has run 30
/// ticks, reads `yaw` back from the root's facing and moves the actor 40
/// units a tick backwards along it, away from the perch; off a ceiling perch
/// it also drops, fastest at first. At tick 59 it starts the chase with a
/// random settling time and `hoverOffset`.
static void Actor01500_Fn00FC4(Task* actor)
{
    _Actor01500Work* work;
    GfxCoord*        coord;
    s16              angle;
    u32              rnd;
    u32              rnd2;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    if (work->animFrame >= 30) {
        work->yaw = angle = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        switch (work->perch) {
            case ACTOR_01500_PERCH_WALL:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->yaw) * 40) >> 12;
                break;
            case ACTOR_01500_PERCH_CEILING:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->yaw) * 40) >> 12;
                if (work->animFrame < 36) {
                    coord->coord.t[1] += 110;
                } else if (work->animFrame < 46) {
                    coord->coord.t[1] += 35;
                } else {
                    coord->coord.t[1] += 15;
                }
                break;
        }
        if (work->animFrame >= 59) {
            rnd                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->action         = ACTOR_01500_ACTION_CHASE;
            work->anim           = ACTOR_01500_ANIM_HOVER;
            work->actionStep     = ACTOR_01500_CHASE_SETTLE;
            work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
            work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            gRandomLcgState      = rnd;
            work->timer          = ((rnd >> 16) & 0x3F) + 0x3C;
            rnd2                 = rnd * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState      = rnd2;
            work->hoverOffset    = (rnd2 >> 16) & 0x1FF;
        }
    }
}

/// `ACTOR_01500_ACTION_CHASE`, stepped by `actionStep`: settle at 1800 plus
/// `hoverOffset` above the player while turning to face them, hover, then
/// advance by a random `Actor01500_D09FE8` distance and hover again. An
/// advance that comes within 1000 units of the player dives at them with
/// `attackBody` armed, until the attack lands or the actor is 1600 above
/// the player, and climbs back before advancing on.
static void Actor01500_Fn011B0(Task* actor)
{
    VECTOR3*         vec;
    _Actor01500Work* work;
    GfxCoord*        coord;
    u32              seed;
    s32              off;
    u16              val;
    u16              val2;
    u16*             tbl;
    u8*              head;
    s32              diff;
    s32              dist;
    s32              y;
    s32              off2;
    s32              diff2;
    s32              dist2;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    vec                      = (VECTOR3*)(head - 0x10);
    work                     = actor->work;
    coord                    = actor->extra.tmd->coords;
    switch (work->actionStep) {
        case ACTOR_01500_CHASE_SETTLE:
            off  = work->hoverOffset + 1800;
            diff = gPlayerStatus.coordMtx->t[1] - off - coord->coord.t[1];
            dist = abs(diff);
            if (dist < 30 || --work->timer <= 0) {
                work->actionStep = ACTOR_01500_CHASE_HOVER;
            } else {
                work->verticalSpeed = diff > 0 ? 30 : -30;
            }
            vec->vx               = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy               = 0;
            vec->vz               = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw       = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
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
                val               = Actor01500_D09FE8[(gRandomLcgState >> 16) & 0xF];
                work->anim        = ACTOR_01500_ANIM_ADVANCE;
                work->actionStep  = ACTOR_01500_CHASE_ADVANCE;
                work->advanceLeft = val;
            }
            vec->vx         = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vec->vy         = 0;
            vec->vz         = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->turnRate  = 100;
            break;
        case ACTOR_01500_CHASE_ADVANCE:
            work->speed        = 200;
            work->advanceLeft -= 200;
            if (work->advanceLeft < 0) {
                tbl                  = Actor01500_D09FC8;
                work->anim           = ACTOR_01500_ANIM_HOVER;
                seed                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                val2                 = tbl[(seed >> 16) & 0xF];
                gRandomLcgState      = seed;
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
                work->actionStep     = ACTOR_01500_CHASE_HOVER;
                work->timer          = val2;
            }
            if (work->lunged == 0) {
                vec->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                if (SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 1000) {
                    work->anim              = ACTOR_01500_ANIM_DIVE;
                    work->actionStep        = ACTOR_01500_CHASE_DIVE;
                    work->loopSound         = 0;
                    work->lunged            = 1;
                    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            break;
        case ACTOR_01500_CHASE_DIVE:
            diff2               = gPlayerStatus.coordMtx->t[1] - 1600;
            work->speed         = 100;
            work->verticalSpeed = 180;
            if (diff2 < coord->coord.t[1] || work->attackLanded != 0) {
                work->actionStep        = ACTOR_01500_CHASE_CLIMB;
                work->attackLanded      = 0;
                work->anim              = ACTOR_01500_ANIM_ADVANCE;
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer             = ((gRandomLcgState >> 16) & 0xF) + 15;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case ACTOR_01500_CHASE_CLIMB:
            off2  = coord->coord.t[1] + 1800;
            diff2 = gPlayerStatus.coordMtx->t[1] - off2;
            dist2 = abs(diff2);
            if (dist2 < 96 || --work->timer <= 0) {
                work->actionStep = ACTOR_01500_CHASE_ADVANCE;
            } else {
                work->verticalSpeed = diff2 > 0 ? 96 : -96;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// `ACTOR_01500_ACTION_GROUNDED`: holds the actor to the floor, edging away
/// from the player, and raises `deathPending` once the actor is more than 500
/// below or 1800 above the player or `timer` counts past 1800 ticks.
static void Actor01500_Fn015DC(Task* actor)
{
    _Actor01500Work* work;
    GfxCoord*        coord;
    VECTOR*          head;
    VECTOR*          blk;

    work                         = actor->work;
    coord                        = actor->extra.tmd->coords;
    work->anim                   = ACTOR_01500_ANIM_GROUNDED;
    work->speed                  = 5;
    work->turnRate               = 5;
    work->loopSound              = 0;
    work->verticalSpeed          = 128;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    blk                          = head - 1;
    head[-1].vx                  = coord->coord.t[0] - gPlayerStatus.coordMtx->t[0];
    blk->vy                      = 0;
    blk->vz                      = coord->coord.t[2] - gPlayerStatus.coordMtx->t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = blk;
    work->targetYaw              = ratan2((s16)head[-1].vx, (s16)blk->vz) & 0xFFF;
    if (coord->coord.t[1] > gPlayerStatus.coordMtx->t[1] + 500 ||
        coord->coord.t[1] < gPlayerStatus.coordMtx->t[1] - 1800 ||
        ++work->timer > 1800) {
        work->deathPending = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// `ACTOR_01500_ACTION_HIT`: ends the flinch once it has run its ticks and
/// resumes by `posture`. A perched actor (20 ticks) goes back to
/// `ACTION_PERCH`, which its lost hit points turn into a take-off; an airborne
/// one (10 ticks) hovers into the chase after a random delay; a crippled one
/// is grounded on the next tick.
static void Actor01500_Fn01708(Task* actor)
{
    _Actor01500Work* work = actor->work;
    s16              anim;
    u32              rnd;
    u16              val;
    u16*             tbl;

    switch (work->posture) {
        case ACTOR_01500_POSTURE_PERCHED:
            anim = ACTOR_01500_ANIM_TAKE_OFF_WALL;
            if (work->animFrame >= 20) {
                work->action = ACTOR_01500_ACTION_PERCH;
                if (work->perch != ACTOR_01500_PERCH_WALL) {
                    anim = ACTOR_01500_ANIM_TAKE_OFF_CEILING;
                }
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->anim           = anim;
                work->actionStep     = 0;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
            }
            break;
        case ACTOR_01500_POSTURE_AIRBORNE:
            if (work->animFrame >= 10) {
                tbl                  = Actor01500_D09FC8;
                work->action         = ACTOR_01500_ACTION_CHASE;
                work->anim           = ACTOR_01500_ANIM_HOVER;
                work->actionStep     = ACTOR_01500_CHASE_SETTLE;
                rnd                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState      = rnd;
                val                  = tbl[(rnd >> 16) & 0xF];
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
                work->timer          = val;
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

/// Turns the actor toward `targetYaw` by at most `turnRate` per call, taking
/// the short way round the 0x1000 circle, then rebuilds its rotation matrix.
static void Actor01500_Fn01838(Task* arg0)
{
    _Actor01500Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->targetYaw;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->yaw = ang;
    if (adiff < 0x800) {
        step = work->turnRate;
        if (step >= adiff) {
            work->yaw = want;
        } else {
            next = work->yaw;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->yaw = next;
        }
    } else {
        step = work->turnRate;
        if (diff > 0 ? step >= 0x1000 - diff : step >= 0x1000 + diff) {
            work->yaw = work->targetYaw;
        } else {
            wrapStep = work->turnRate;
            cur      = work->yaw;
            if (diff > 0) {
                work->yaw = cur - wrapStep;
            } else {
                work->yaw = cur + wrapStep;
            }
        }
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->yaw;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

static void Actor01500_Fn01988(Task* arg0)
{
    _Actor01500Work* work;
    GfxCoord*        coord;
    s16              bob;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    bob   = 0;
    if (work->anim == ACTOR_01500_ANIM_HOVER) {
        work->bobPhase++;
        if (work->bobPhase >= 15) {
            work->bobPhase = 0;
        }
        bob = Actor01500_D0A070[work->bobPhase];
    }
    // Remember the position so that the next contact pass can put it back.
    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> 12;
    coord->coord.t[1] += work->verticalSpeed + bob;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> 12;
    // An actor that has sunk 5000 below the player is removed without a death.
    if (coord->coord.t[1] - gPlayerStatus.coordMtx->t[1] > 5000) {
        arg0->state      = 2;
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
        entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index1);
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
        entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
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
        entry3                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index3);
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
        entry4                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index4);
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

static void Actor01500_Fn020D8(Task* arg0)
{
    u8*              head;
    VECTOR3*         stk;
    VECTOR3*         vec;
    _Actor01500Work* work;
    GfxCoord*        coord;
    s32              dy;
    s32              ady;
    u16              val;
    u16              val2;
    u16*             tbl;
    s32              off;
    s32              delay;

    head                     = SCRATCH_STACK_CURSOR(u8);
    stk                      = (VECTOR3*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)stk;
    vec                      = stk;
    work                     = arg0->work;
    coord                    = arg0->extra.tmd->coords;
    switch (work->actionStep) {
        case ACTOR_01500_CHASE_SETTLE:
            off = work->hoverOffset + 800;
            dy  = Actor01500_D0A090.vy - off - coord->coord.t[1];
            ady = abs(dy);
            if (ady < 30 || --work->timer <= 0) {
                work->actionStep = ACTOR_01500_CHASE_HOVER;
            } else {
                work->verticalSpeed = dy > 0 ? 30 : -30;
            }
            vec->vx               = Actor01500_D0A090.vx - coord->coord.t[0];
            vec->vy               = 0;
            vec->vz               = Actor01500_D0A090.vz - coord->coord.t[2];
            work->targetYaw       = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
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
                val               = Actor01500_D09FE8[(gRandomLcgState >> 16) & 0xF];
                work->anim        = ACTOR_01500_ANIM_ADVANCE;
                work->actionStep  = ACTOR_01500_CHASE_ADVANCE;
                work->advanceLeft = val;
            }
            ((VECTOR3*)(head - 0x10))->vx = Actor01500_D0A090.vx - coord->coord.t[0];
            stk->vy                       = 0;
            stk->vz                       = Actor01500_D0A090.vz - coord->coord.t[2];
            work->targetYaw               = ratan2((s16)((VECTOR3*)(head - 0x10))->vx, (s16)stk->vz) & 0xFFF;
            work->turnRate                = 100;
            break;
        case ACTOR_01500_CHASE_ADVANCE:
            work->speed        = 200;
            work->advanceLeft -= 200;
            if (work->advanceLeft < 0) {
                tbl                  = Actor01500_D09FC8;
                work->anim           = ACTOR_01500_ANIM_HOVER;
                val2                 = tbl[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
                work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
                work->actionStep     = ACTOR_01500_CHASE_HOVER;
                work->timer          = val2;
            }
            break;
    }
    if (gPlayerStatus.coordMtx->t[0] > Actor01500_D0A098.vx && gPlayerStatus.coordMtx->t[2] < Actor01500_D0A098.vz) {
        work->action         = ACTOR_01500_ACTION_CHASE;
        delay                = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x3F) + 60;
        work->anim           = ACTOR_01500_ANIM_HOVER;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->actionStep     = ACTOR_01500_CHASE_SETTLE;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->roused         = 1;
        work->timer          = delay;
        work->hoverOffset    = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1FF;
        sceneEngageBattle(1);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
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
        Actor01500_Fn025C8(arg1);
    }
    Actor01500_Fn004EC(arg1);
    if (work->deathPending != 0) {
        if ((work->action == ACTOR_01500_ACTION_GROUNDED) || (work->burstPending != 0)) {
            work->action     = ACTOR_01500_ACTION_DIE;
            work->actionStep = ACTOR_01500_DEATH_BEGIN;
            arg1->state      = 2;
        }
    }
    Actor01500_Fn026D8(arg1);
    if (work->turnRate != 0) {
        Actor01500_Fn01838(arg1);
    }
    Actor01500_Fn01988(arg1);
    Actor01500_Fn02958(arg1);
    Actor01500_Fn02A1C(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    Actor01500_Fn02B14(arg1);
    Actor01500_Fn02B70(arg1);
}

/// Stagger and buildup both knock the actor into `ACTOR_01500_ANIM_FLINCH_AIR`
/// and, unless it is already crippled, into a fall; damage over time ticks
/// the effect and applies each hit.
static void Actor01500_Fn025C8(Task* actor)
{
    Enemy*           enemy;
    _Actor01500Work* work;
    s32              damage;
    u8               flags;

    enemy = actor->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = actor->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        if (work->posture != ACTOR_01500_POSTURE_CRIPPLED) {
            work->action = ACTOR_01500_ACTION_FALL;
        }
        work->timer     = 0;
        work->anim      = ACTOR_01500_ANIM_FLINCH_AIR;
        work->loopSound = 0;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        if (work->posture != ACTOR_01500_POSTURE_CRIPPLED) {
            work->action = ACTOR_01500_ACTION_FALL;
        }
        work->timer     = 0;
        work->anim      = ACTOR_01500_ANIM_FLINCH_AIR;
        work->loopSound = 0;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = damageTickEnemyDamageOverTime(enemy);
        if (damage != 0) {
            Actor01500_Fn00AFC(actor, damage);
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Runs the handler `action` selects. `ACTOR_01500_ACTION_DIE` has none.
static void Actor01500_Fn026D8(Task* arg0)
{
    _Actor01500Work* work = arg0->work;

    switch (work->action) {
        case ACTOR_01500_ACTION_PERCH:
            Actor01500_Fn00CA4(arg0);
            break;
        case ACTOR_01500_ACTION_ALERT:
            Actor01500_Fn027B0(arg0);
            break;
        case ACTOR_01500_ACTION_TAKE_OFF:
            Actor01500_Fn00FC4(arg0);
            break;
        case ACTOR_01500_ACTION_CHASE:
            Actor01500_Fn011B0(arg0);
            break;
        case ACTOR_01500_ACTION_FALL:
            Actor01500_Fn0288C(arg0);
            break;
        case ACTOR_01500_ACTION_GROUNDED:
            Actor01500_Fn015DC(arg0);
            break;
        case ACTOR_01500_ACTION_WALL_REST:
            Actor01500_Fn028B0(arg0);
            break;
        case ACTOR_01500_ACTION_HIT:
            Actor01500_Fn01708(arg0);
            break;
        case ACTOR_01500_ACTION_SCRIPTED:
            Actor01500_Fn020D8(arg0);
            break;
    }
}

/// `ACTOR_01500_ACTION_ALERT`: counts `timer` up, raising the state-F0 flags
/// at tick 60; at tick 90 it starts the take-off for the actor's perch.
static void Actor01500_Fn027B0(Task* actor)
{
    _Actor01500Work* work = actor->work;
    s16              anim;
    u32              rnd;
    u16              val;

    if (++work->timer == 60) {
        sceneSetEnemyAlert(1);
        sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_NOISE);
    }
    anim = ACTOR_01500_ANIM_TAKE_OFF_WALL;
    if (work->timer >= 90) {
        work->action = ACTOR_01500_ACTION_TAKE_OFF;
        if (work->perch != ACTOR_01500_PERCH_WALL) {
            anim = ACTOR_01500_ANIM_TAKE_OFF_CEILING;
        }
        work->anim           = anim;
        rnd                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState      = rnd;
        val                  = Actor01500_D09FC8[(rnd >> 16) & 0xF];
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->roused         = 1;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->timer          = val;
    }
}

/// `ACTOR_01500_ACTION_FALL`, entered from a stagger or at 60% of the hit
/// points: a forward drift of 20 a tick while sinking 128, with `roomBody`
/// held above the root.
static void Actor01500_Fn0288C(Task* arg0)
{
    _Actor01500Work* work = arg0->work;

    work->speed           = 20;
    work->verticalSpeed   = 128;
    work->roomBody.pos.vy = -300;
    work->roomBody.pos.vz = 0;
}

/// `ACTOR_01500_ACTION_WALL_REST`: holds the actor still on the wall it
/// settled on; when `timer` runs out it takes off again as from a wall perch.
static void Actor01500_Fn028B0(Task* actor)
{
    _Actor01500Work* work = actor->work;
    u32              rnd;
    u16              val;
    u16*             tbl;

    work->anim          = ACTOR_01500_ANIM_WALL_REST;
    work->loopSound     = 0;
    work->speed         = 0;
    work->verticalSpeed = 0;
    if (--work->timer == 0) {
        tbl                  = Actor01500_D09FC8;
        work->posture        = ACTOR_01500_POSTURE_AIRBORNE;
        work->action         = ACTOR_01500_ACTION_TAKE_OFF;
        work->anim           = ACTOR_01500_ANIM_TAKE_OFF_WALL;
        work->actionStep     = 0;
        rnd                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState      = rnd;
        val                  = tbl[(rnd >> 16) & 0xF];
        work->perch          = 0;
        work->loopSound      = ACTOR_01500_SOUND_FLIGHT_START;
        work->loopSoundTimer = ACTOR_01500_FLIGHT_START_TICKS;
        work->timer          = val;
    }
}

/// Animation tick. When `anim` differs from `playingAnim`, every driven slot
/// is started on it over that animation's blend frames and `animFrame` is
/// cleared; while the two agree each slot is ticked and `animFrame` advances
/// by one.
static void Actor01500_Fn02958(Task* arg0)
{
    _Actor01500Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        value             = Actor01500_D0A050[work->anim];
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, value);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
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
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->deathScaleY;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->deathBaseMtx;
    gfxSetRotIdentity(&scratch->matrix.mat);
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
