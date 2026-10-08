#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

/// Task-state indices of the scorpion's spawn, active and death handlers.
enum {
    ACTOR_02500_TASK_STATE_SPAWN  = 0,
    ACTOR_02500_TASK_STATE_ACTIVE = 1,
    ACTOR_02500_TASK_STATE_DEATH  = 2
};

/// Placement modes selecting roaming or the two buried ambush roles.
enum {
    ACTOR_02500_PLACE_ROAM            = 0,
    ACTOR_02500_PLACE_AMBUSH_LEADER   = 1,
    ACTOR_02500_PLACE_AMBUSH_FOLLOWER = 2
};

/// Randomized waits in ticks: short waits add 0..63, long waits add 0..127.
enum {
    ACTOR_02500_WAIT_MIN_TICKS  = 30,
    ACTOR_02500_SHORT_WAIT_MASK = 0x3F,
    ACTOR_02500_LONG_WAIT_MASK  = 0x7F,
    ACTOR_02500_STAGGER_TICKS   = 60
};

/// Base sound requests; the placement index supplies the instance byte.
enum {
    ACTOR_02500_SOUND_STEP_FIRST     = 0x40190001,
    ACTOR_02500_SOUND_STEP_SECOND    = 0x40190002,
    ACTOR_02500_SOUND_EMERGE         = 0x40190003,
    ACTOR_02500_SOUND_STING          = 0x40190005,
    ACTOR_02500_SOUND_STING_HIT      = 0x40190006,
    ACTOR_02500_SOUND_POISON_CONTACT = 0x40190007,
    ACTOR_02500_SOUND_IDLE           = 0x40190008,
    ACTOR_02500_SOUND_HIT            = 0x40190009,
    ACTOR_02500_SOUND_DEATH          = 0x4019000A,
    ACTOR_02500_SOUND_INSTANCE_SHIFT = 8
};

/// Values of `_Actor02500Work::action`: the handler the per-frame tick runs.
///
/// A handler numbers its own stages in `actionStep`, from 0 on entry.
enum {
    ACTOR_02500_ACTION_WANDER  = 0, // stands, turns and walks at random around `home`; the player touching `noticeBody` starts the chase
    ACTOR_02500_ACTION_CHASE   = 1, // runs at the player and stings from close range; gives up once the player has long been far from `home`
    ACTOR_02500_ACTION_FLINCH  = 2, // recoils from a hit for 32 ticks, then chases or returns to the stagger or buildup it interrupted
    ACTOR_02500_ACTION_STAGGER = 3, // held still for 60 ticks by a stagger reaction, then chases
    ACTOR_02500_ACTION_BUILDUP = 4, // held still by a buildup reaction until it runs out, then chases
    ACTOR_02500_ACTION_AMBUSH  = 5, // buried and hidden until the player nears or another scorpion gives the signal, then digs out and chases
    ACTOR_02500_ACTION_DIE     = 6  // out of hit points: hands the task to its death state
};

/// Values of `_Actor02500Work::actionStep` in `ACTOR_02500_ACTION_WANDER`.
enum {
    ACTOR_02500_WANDER_STEP_STAND     = 0, // waits out `timer`, then picks a random heading
    ACTOR_02500_WANDER_STEP_TURN      = 1, // turns on the spot until `yaw` reaches `targetYaw`
    ACTOR_02500_WANDER_STEP_WALK      = 2, // walks ahead until `timer` runs out or the room turns it aside
    ACTOR_02500_WANDER_STEP_HEAD_HOME = 3  // 2000 or more from `home`: aims back at it and raises `headingHome`
};

/// Values of `_Actor02500Work::actionStep` in `ACTOR_02500_ACTION_CHASE`.
enum {
    ACTOR_02500_CHASE_STEP_BEGIN   = 0, // starts the run and its 240-tick patience
    ACTOR_02500_CHASE_STEP_RUN     = 1, // runs at the player; within 1000 it stops, turns to face and stings
    ACTOR_02500_CHASE_STEP_STING   = 2, // plays the sting, `attackBody` armed from tick 41 to tick 44, and runs again from tick 76
    ACTOR_02500_CHASE_STEP_GIVE_UP = 3  // returns to `ACTOR_02500_ACTION_WANDER` at its `HEAD_HOME` step
};

/// Values of `_Actor02500Work::actionStep` in the three actions a hit starts:
/// `ACTOR_02500_ACTION_FLINCH`, `_STAGGER` and `_BUILDUP`.
enum {
    ACTOR_02500_REACTION_STEP_BEGIN = 0, // starts the animation and stops the movement
    ACTOR_02500_REACTION_STEP_HOLD  = 1  // waits for the reaction to end
};

/// Values of `_Actor02500Work::actionStep` in `ACTOR_02500_ACTION_AMBUSH`.
///
/// The placement's mode picks the first: 1 starts at `WAIT_NEAR`, 2 at
/// `WAIT_SIGNAL`. Mode 0 is not buried and starts wandering.
enum {
    ACTOR_02500_AMBUSH_STEP_WAIT_NEAR   = 0, // hidden until the player is within 2000 of `home`, then gives the room's scorpions the signal
    ACTOR_02500_AMBUSH_STEP_WAIT_SIGNAL = 1, // hidden until that signal
    ACTOR_02500_AMBUSH_STEP_DELAY       = 2, // hidden for 10 more ticks per placement index, so the scorpions surface in turn
    ACTOR_02500_AMBUSH_STEP_DUST        = 3, // 10 ticks of dust before the body shows
    ACTOR_02500_AMBUSH_STEP_EMERGE      = 4  // plays the emerging animation for 31 ticks, drawn translucent for the first 15, then chases
};

/// Values of `_Actor02500Work::actionStep` in the task's death state.
enum {
    ACTOR_02500_DEATH_STEP_BEGIN    = 0, // unlinks the target node and the four bodies and starts the death animation
    ACTOR_02500_DEATH_STEP_COLLAPSE = 1, // flattens the body for 60 ticks; tick 15 spawns the corpse effect and the package's second task
    ACTOR_02500_DEATH_STEP_DESTROY  = 2, // destroys the enemy
    ACTOR_02500_DEATH_STEP_BURST    = 3  // hidden for 60 ticks instead of collapsing, the fragments spawned on its second tick
};

/// Values of `_Actor02500Work::anim`: indices into the package's animation-set
/// table, named for the action that plays each. The table has no set at 0, 2,
/// 5 and 9.
enum {
    ACTOR_02500_ANIM_STAND   = 1,
    ACTOR_02500_ANIM_WALK    = 3,
    ACTOR_02500_ANIM_RUN     = 4,
    ACTOR_02500_ANIM_STING   = 6,
    ACTOR_02500_ANIM_FLINCH  = 7, // also held through a stagger
    ACTOR_02500_ANIM_DIE     = 8,
    ACTOR_02500_ANIM_EMERGE  = 10,
    ACTOR_02500_ANIM_BUILDUP = 11
};

/// Work block of the package's enemy task, a scorpion.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, four collision
/// spheres with their contact tables, and the state the per-frame tick steers
/// the scorpion with.
///
/// `attackBody` rides the coordinate of the model's part 4; the other three
/// spheres ride the root. Angles are 4096ths of a turn, and `yaw` is the
/// heading about Y. Timers count ticks. Positions are the root coordinate's
/// translation in its parent's space.
typedef struct {
    ActorAnimRig5         rig;               // playback of the model's five parts; slots 1 to 4 are driven
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    noticeBody;        // sphere of radius 600 ahead of the root, with no key of its own: a category-1 contact while wandering starts the chase
    WorldCollisionContact noticeContacts[1]; // contact of `noticeBody`
    WorldCollisionBody    hitBody;           // sphere of radius 300, 300 above the root, that takes the hits and meets other bodies; pair tests are off while buried
    WorldCollisionContact hitContacts[3];    // contacts of `hitBody`; also the enemy's hit records, except while buried
    WorldCollisionBody    gridBody;          // the same sphere for the room grid, which also rests it on the floor
    WorldCollisionContact gridContacts[5];   // contacts of `gridBody`, which push the root out of the room's faces
    WorldCollisionBody    attackBody;        // sphere of radius 300 on the model's part 4, keyed with the package's first attack; pair tests are on only during the sting
    WorldCollisionContact attackContacts[1]; // contact of `attackBody`; any entry means the sting landed and disarms it
    EffectSpawnArg        hitEffectArg;      // argument record of the effect a hit spawns, on the root coordinate
    MATRIX                savedRootMtx;      // root transform at death, the pose the collapse scales
    VECTOR                prevPos;           // root position before the tick's movement, restored when the grid's contacts oppose each other
    SVECTOR               home;              // root position at spawn: the wander stays near it and the ambush measures the player from it; `vy` is recorded, never read
    s16                   anim;              // `ACTOR_02500_ANIM_*` the actions ask for
    s16                   playingAnim;       // `anim` the slots were last started on
    s16                   animFrame;         // ticks since `playingAnim` was started
    s16                   action;            // `ACTOR_02500_ACTION_*`
    s16                   actionStep;        // stage of the running action, 0 on entry; the task's death state counts `ACTOR_02500_DEATH_STEP_*` in it
    s16                   speed;             // distance moved along the facing each tick
    s16                   turnRate;          // most `yaw` may change in a tick on its way to `targetYaw`; 0 leaves the rotation alone
    u16                   targetYaw;         // heading `yaw` is turned toward
    s16                   yaw;               // heading of the model's root, read back from its rotation before each turn
    s16                   timer;             // countdown of the running action's wait; the emerging step and the death state count it up instead
    s16                   dustTimer;         // ticks of dust left around a scorpion digging out, 20 at the start; four puffs every fourth tick
    s16                   deathScaleY;       // Y scale of the collapsing body, 4096 for full height; shrinks by 80 a tick to about an eighth
    s16                   hitCooldown;       // ticks left in which further attack contacts are ignored, set by the attack that landed; 0 when hits count
    s16                   headingHome;       // 1 from the turn back toward `home` until the next stand ends, so the walk home is not turned back again
    s16                   idleSoundTimer;    // ticks to the next idle sound while wandering or chasing, reseeded at random with 30 to 157
    s16                   stepSoundFrames;   // ticks of movement in the current stride: one footstep sound at 9, the other at 18, which restarts it; 0 while standing
    s16                   burstStage;        // 0 unless the killing hit bursts the body; then 1 and 2 over the first death ticks, after which the model gives up its buffers and three fragment effects are spawned
    s16                   inBuildup;         // 1 from a buildup reaction until it runs out; a flinch then returns to `ACTION_BUILDUP`, and a hit asks for no stagger
    s16                   blocked;           // 1 on a tick the grid's contacts pushed the root along X or Z (0 otherwise); ends the wander's walk
    s16                   flinchGuard;       // raised by the sting, lowered on the tick the sting disarms and by the next hit: a hit that finds it raised does not start a flinch
    s16                   inStagger;         // 1 while `ACTION_STAGGER` runs; a flinch then returns to its hold with 60 more ticks
    byte                  pad_346[2];        // never accessed; the allocation's last two bytes
} _Actor02500Work;
STATIC_ASSERT_SIZEOF(_Actor02500Work, 0x348);

/// Values of `_Actor02500CorpsePoisonWork::releaseStep`.
enum {
    ACTOR_02500_CORPSE_POISON_RELEASE_BEGIN  = 0, // unlinks the sphere, dismisses the decal and starts the 30-tick wait
    ACTOR_02500_CORPSE_POISON_RELEASE_LINGER = 1  // waits out `timer`, then destroys the task
};

/// Work block of the poison a dead scorpion leaves where it fell.
///
/// The collapse spawns the package's second task for it, which allocates the
/// block zeroed and keeps it at `Task::work`. The task takes a copy of the
/// corpse's root transform and detaches from the scorpion, so the poison
/// outlives it: a decal on the ground and a sphere of radius 200 keyed with
/// the package's second attack, whose reaction is poison.
///
/// It stands for 240 ticks, or until the sphere holds a contact with the
/// player's body or the battle has no holds left, and takes 30 more to go.
typedef struct {
    WorldCollisionBody    body;        // sphere of radius 200 at the task's coordinate, keyed with the poison attack; unlinked on release
    WorldCollisionContact contacts[1]; // contact of `body`; an entry of the player's category ends the poison
    EffectWork*           decal;       // the ground decal, sent to its fade-out on release; NULL when the effect could not be spawned
    s16                   timer;       // ticks the poison has stood, to 240; on release, the 30 ticks left before the task is destroyed
    s16                   releaseStep; // `ACTOR_02500_CORPSE_POISON_RELEASE_*`, once the task's state is its release
} _Actor02500CorpsePoisonWork;
STATIC_ASSERT_SIZEOF(_Actor02500CorpsePoisonWork, 0x40);

/// One horizontal direction of the ring of dust around a scorpion digging out.
///
/// A unit vector in the root's XZ plane, 4096 for one unit. Scaled by the
/// ring's radius it is the offset of one puff from the root.
typedef struct {
    s16 x;
    s16 z;
} _Actor02500DustDirection;

extern EnemyParams              Actor02500_D05B38;
extern DamageAttack             Actor02500_D05B30[];
extern s16                      Actor02500_D05B48[];
extern s16                      Actor02500_D05B58[];
extern s16                      Actor02500_D05B68[];
extern s16                      Actor02500_D05B78[];
extern TaskDesc                 Actor02500_D05B88[];
extern AnimationSet*            Actor02500_D05BA0[12];
extern s16                      Actor02500_D05BD0[];
extern _Actor02500DustDirection Actor02500_D05BE8[];
static TmdSource                _gActor02500ScorpionBurstHead;
static TmdSource                _gActor02500ScorpionBurstPincer2;
static TmdSource                _gActor02500ScorpionBurstPincer1;
extern void*                    D_80067704[1];

static void _actor02500Spawn(Enemy* enemy, Task* actor);
static void _actor02500Die(Enemy* enemy, Task* actor);
static void _actor02500Tick(Enemy* enemy, Task* actor);
static void _actor02500ConsumeReactions(Task* actor);
static void _actor02500DispatchAction(Task* actor);
static void _actor02500Flinch(Task* actor);
static void _actor02500Stagger(Task* actor);
static void _actor02500Buildup(Task* actor);
static void _actor02500Move(Task* actor);
static void _actor02500TickAnimation(Task* actor);
static void _actor02500UpdateColor(Task* actor);
static void _actor02500DrawShadow(Task* actor);
static void _actor02500SquashCorpse(Task* actor);
static void _actor02500SpawnCorpsePoison(Enemy* enemy, Task* task);
static void _actor02500TickCorpsePoison(Enemy* enemy, Task* task);
static void _actor02500ReleaseCorpsePoison(Enemy* enemy, Task* task);

/// State handlers of the enemy task `_actor02500Task` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
static const EnemyTaskFuncTable3 Actor02500_D00004 = {
    {
        _actor02500Spawn,
        _actor02500Tick,
        _actor02500Die,
    },
};

static AnimationSet _gActor02500Actor102500Animation04C64;
static AnimationSet _gActor02500Actor102500Animation04E3C;
static AnimationSet _gActor02500Actor102500Animation04FF0;
static AnimationSet _gActor02500Actor102500Animation0534C;
static AnimationSet _gActor02500Actor102500Animation05520;
static AnimationSet _gActor02500Actor102500Animation057D4;
static AnimationSet _gActor02500Actor102500Animation05998;
static AnimationSet _gActor02500Actor102500Animation05B08;
static TmdSource    _gActor02500ScorpionBody;
static void         _actor02500Task(Task* actor);
static void         _actor02500CorpsePoisonTask(Task* task);

static TmdBone _gActor02500ScorpionBodySkeleton[5] = {
#include "assets/scorpion_body_skeleton.inc"
};

static u32 _gActor02500ScorpionBodyPartVerts[5] = {
#include "assets/scorpion_body_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBodyVerts[93] = {
#include "assets/scorpion_body_verts.inc"
};

static SVECTOR _gActor02500ScorpionBodyNormals[93] = {
#include "assets/scorpion_body_normals.inc"
};

static u32 _gActor02500ScorpionBodyStream[989] = {
#include "assets/scorpion_body_stream.inc"
};

static TmdSource _gActor02500ScorpionBody = {
    0,
    5684,
    1152,
    5,
    _gActor02500ScorpionBodyPartVerts,
    _gActor02500ScorpionBodyVerts,
    _gActor02500ScorpionBodyNormals,
    _gActor02500ScorpionBodySkeleton,
    _gActor02500ScorpionBodyStream,
};

static TmdBone _gActor02500ScorpionBurstHeadSkeleton[1] = {
#include "assets/scorpion_burst_head_skeleton.inc"
};

static u32 _gActor02500ScorpionBurstHeadPartVerts[1] = {
#include "assets/scorpion_burst_head_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBurstHeadVerts[22] = {
#include "assets/scorpion_burst_head_verts.inc"
};

static SVECTOR _gActor02500ScorpionBurstHeadNormals[22] = {
#include "assets/scorpion_burst_head_normals.inc"
};

static u32 _gActor02500ScorpionBurstHeadStream[223] = {
#include "assets/scorpion_burst_head_stream.inc"
};

static TmdSource _gActor02500ScorpionBurstHead = {
    0,
    1292,
    0,
    1,
    _gActor02500ScorpionBurstHeadPartVerts,
    _gActor02500ScorpionBurstHeadVerts,
    _gActor02500ScorpionBurstHeadNormals,
    _gActor02500ScorpionBurstHeadSkeleton,
    _gActor02500ScorpionBurstHeadStream,
};

static TmdBone _gActor02500ScorpionBurstPincer2Skeleton[1] = {
#include "assets/scorpion_burst_pincer_2_skeleton.inc"
};

static u32 _gActor02500ScorpionBurstPincer2PartVerts[1] = {
#include "assets/scorpion_burst_pincer_2_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer2Verts[15] = {
#include "assets/scorpion_burst_pincer_2_verts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer2Normals[15] = {
#include "assets/scorpion_burst_pincer_2_normals.inc"
};

static u32 _gActor02500ScorpionBurstPincer2Stream[130] = {
#include "assets/scorpion_burst_pincer_2_stream.inc"
};

static TmdSource _gActor02500ScorpionBurstPincer2 = {
    0,
    844,
    0,
    1,
    _gActor02500ScorpionBurstPincer2PartVerts,
    _gActor02500ScorpionBurstPincer2Verts,
    _gActor02500ScorpionBurstPincer2Normals,
    _gActor02500ScorpionBurstPincer2Skeleton,
    _gActor02500ScorpionBurstPincer2Stream,
};

static TmdBone _gActor02500ScorpionBurstPincer1Skeleton[1] = {
#include "assets/scorpion_burst_pincer_1_skeleton.inc"
};

static u32 _gActor02500ScorpionBurstPincer1PartVerts[1] = {
#include "assets/scorpion_burst_pincer_1_partVerts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer1Verts[15] = {
#include "assets/scorpion_burst_pincer_1_verts.inc"
};

static SVECTOR _gActor02500ScorpionBurstPincer1Normals[15] = {
#include "assets/scorpion_burst_pincer_1_normals.inc"
};

static u32 _gActor02500ScorpionBurstPincer1Stream[130] = {
#include "assets/scorpion_burst_pincer_1_stream.inc"
};

static TmdSource _gActor02500ScorpionBurstPincer1 = {
    0,
    844,
    0,
    1,
    _gActor02500ScorpionBurstPincer1PartVerts,
    _gActor02500ScorpionBurstPincer1Verts,
    _gActor02500ScorpionBurstPincer1Normals,
    _gActor02500ScorpionBurstPincer1Skeleton,
    _gActor02500ScorpionBurstPincer1Stream,
};

static AnimationPackedPose _gActor02500Actor102500Animation04C64Bank1[8] = {
#include "assets/actor_102500_animation_04C64_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation04C64Bank4[21] = {
#include "assets/actor_102500_animation_04C64_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation04C64Records[44] = {
#include "assets/actor_102500_animation_04C64_records.inc"
};

static u16 _gActor02500Actor102500Animation04C64Indices[6] = {
#include "assets/actor_102500_animation_04C64_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation04C64 = {
    _gActor02500Actor102500Animation04C64Records,
    _gActor02500Actor102500Animation04C64Indices,
    { NULL, _gActor02500Actor102500Animation04C64Bank1, NULL, NULL, _gActor02500Actor102500Animation04C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation04E3CBank1[10] = {
#include "assets/actor_102500_animation_04E3C_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation04E3CBank4[27] = {
#include "assets/actor_102500_animation_04E3C_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation04E3CRecords[48] = {
#include "assets/actor_102500_animation_04E3C_records.inc"
};

static u16 _gActor02500Actor102500Animation04E3CIndices[6] = {
#include "assets/actor_102500_animation_04E3C_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation04E3C = {
    _gActor02500Actor102500Animation04E3CRecords,
    _gActor02500Actor102500Animation04E3CIndices,
    { NULL, _gActor02500Actor102500Animation04E3CBank1, NULL, NULL, _gActor02500Actor102500Animation04E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation04FF0Bank1[7] = {
#include "assets/actor_102500_animation_04FF0_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation04FF0Bank4[27] = {
#include "assets/actor_102500_animation_04FF0_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation04FF0Records[48] = {
#include "assets/actor_102500_animation_04FF0_records.inc"
};

static u16 _gActor02500Actor102500Animation04FF0Indices[6] = {
#include "assets/actor_102500_animation_04FF0_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation04FF0 = {
    _gActor02500Actor102500Animation04FF0Records,
    _gActor02500Actor102500Animation04FF0Indices,
    { NULL, _gActor02500Actor102500Animation04FF0Bank1, NULL, NULL, _gActor02500Actor102500Animation04FF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation0534CBank1[20] = {
#include "assets/actor_102500_animation_0534C_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation0534CBank4[55] = {
#include "assets/actor_102500_animation_0534C_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation0534CRecords[87] = {
#include "assets/actor_102500_animation_0534C_records.inc"
};

static u16 _gActor02500Actor102500Animation0534CIndices[6] = {
#include "assets/actor_102500_animation_0534C_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation0534C = {
    _gActor02500Actor102500Animation0534CRecords,
    _gActor02500Actor102500Animation0534CIndices,
    { NULL, _gActor02500Actor102500Animation0534CBank1, NULL, NULL, _gActor02500Actor102500Animation0534CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation05520Bank1[10] = {
#include "assets/actor_102500_animation_05520_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation05520Bank4[27] = {
#include "assets/actor_102500_animation_05520_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation05520Records[47] = {
#include "assets/actor_102500_animation_05520_records.inc"
};

static u16 _gActor02500Actor102500Animation05520Indices[6] = {
#include "assets/actor_102500_animation_05520_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation05520 = {
    _gActor02500Actor102500Animation05520Records,
    _gActor02500Actor102500Animation05520Indices,
    { NULL, _gActor02500Actor102500Animation05520Bank1, NULL, NULL, _gActor02500Actor102500Animation05520Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation057D4Bank1[16] = {
#include "assets/actor_102500_animation_057D4_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation057D4Bank4[45] = {
#include "assets/actor_102500_animation_057D4_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation057D4Records[67] = {
#include "assets/actor_102500_animation_057D4_records.inc"
};

static u16 _gActor02500Actor102500Animation057D4Indices[6] = {
#include "assets/actor_102500_animation_057D4_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation057D4 = {
    _gActor02500Actor102500Animation057D4Records,
    _gActor02500Actor102500Animation057D4Indices,
    { NULL, _gActor02500Actor102500Animation057D4Bank1, NULL, NULL, _gActor02500Actor102500Animation057D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation05998Bank1[10] = {
#include "assets/actor_102500_animation_05998_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation05998Bank4[27] = {
#include "assets/actor_102500_animation_05998_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation05998Records[43] = {
#include "assets/actor_102500_animation_05998_records.inc"
};

static u16 _gActor02500Actor102500Animation05998Indices[6] = {
#include "assets/actor_102500_animation_05998_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation05998 = {
    _gActor02500Actor102500Animation05998Records,
    _gActor02500Actor102500Animation05998Indices,
    { NULL, _gActor02500Actor102500Animation05998Bank1, NULL, NULL, _gActor02500Actor102500Animation05998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02500Actor102500Animation05B08Bank1[7] = {
#include "assets/actor_102500_animation_05B08_bank1.inc"
};

static AnimationPackedRotation _gActor02500Actor102500Animation05B08Bank4[18] = {
#include "assets/actor_102500_animation_05B08_bank4.inc"
};

static AnimationRecord _gActor02500Actor102500Animation05B08Records[40] = {
#include "assets/actor_102500_animation_05B08_records.inc"
};

static u16 _gActor02500Actor102500Animation05B08Indices[6] = {
#include "assets/actor_102500_animation_05B08_indices.inc"
};

static AnimationSet _gActor02500Actor102500Animation05B08 = {
    _gActor02500Actor102500Animation05B08Records,
    _gActor02500Actor102500Animation05B08Indices,
    { NULL, _gActor02500Actor102500Animation05B08Bank1, NULL, NULL, _gActor02500Actor102500Animation05B08Bank4, NULL, NULL, NULL },
};

DamageAttack Actor02500_D05B30[2] = {
    { 10, 3 },
    { 1, 3 },
};

EnemyParams Actor02500_D05B38 = { Actor02500_D05B30, 68, 20, 8, 1, 0, 20, 0, 0 };

s16 Actor02500_D05B48[8] = {
    16,
    18,
    20,
    22,
    22,
    22,
    22,
    22,
};

s16 Actor02500_D05B58[8] = {
    24,
    26,
    28,
    30,
    30,
    30,
    30,
    30,
};

s16 Actor02500_D05B68[8] = {
    18,
    22,
    26,
    30,
    30,
    30,
    30,
    30,
};

s16 Actor02500_D05B78[8] = {
    30,
    34,
    34,
    38,
    38,
    38,
    38,
    38,
};

TaskDesc Actor02500_D05B88[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor02500Task, { .model = &_gActor02500ScorpionBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor02500CorpsePoisonTask, { .value = 0 } },
};

AnimationSet* Actor02500_D05BA0[12] = {
    NULL,
    &_gActor02500Actor102500Animation04C64,
    NULL,
    &_gActor02500Actor102500Animation04E3C,
    &_gActor02500Actor102500Animation04FF0,
    NULL,
    &_gActor02500Actor102500Animation0534C,
    &_gActor02500Actor102500Animation05520,
    &_gActor02500Actor102500Animation057D4,
    NULL,
    &_gActor02500Actor102500Animation05998,
    &_gActor02500Actor102500Animation05B08,
};

s16 Actor02500_D05BD0[12] = {
    0,
    8,
    8,
    8,
    8,
    8,
    8,
    0,
    8,
    8,
    0,
    4,
};

_Actor02500DustDirection Actor02500_D05BE8[8] = {
    { 0, 4096 },
    { 2896, 2896 },
    { 4096, 0 },
    { 2896, -2896 },
    { 0, -4096 },
    { -2896, -2896 },
    { -4096, 0 },
    { -2896, 2896 },
};

static void _actor02500ResolveContacts(Task* actor);
static void _actor02500Wander(Task* actor);
static void _actor02500Chase(Task* actor);
static void _actor02500TickSounds(Task* actor);
static void _actor02500Ambush(Task* actor);
static void _actor02500TurnTowardTargetYaw(Task* actor);
static void _actor02500SpawnBurstFragments(Task* actor);

/// Initializes the four scorpion spheres and their owned contact tables.
///
/// Requires live work, enemy placement and five model coordinates. The root
/// carries notice, hit and grid spheres; coordinate 4 carries the sting.
/// Registers each body before initializing its contacts, then enables only
/// the passes allowed by the placement and attack state. Work outlives all links.
static __inline__ void _actor02500InitCollisionBodies(const Enemy* enemy, const Task* actor, _Actor02500Work* work, GfxCoord* rootCoord)
{
    enum { ACTOR_02500_BODY_ID            = 25,
           ACTOR_02500_NOTICE_RADIUS      = 600,
           ACTOR_02500_BODY_RADIUS        = 300,
           ACTOR_02500_NOTICE_Y           = -400,
           ACTOR_02500_STING_Y            = -950,
           ACTOR_02500_STING_Z            = 460,
           ACTOR_02500_STING_ATTACK_INDEX = 0,
           ACTOR_02500_TAIL_COORD         = 4 };

    // Root spheres sense the player, take hits and meet the grid; the tail stings.
    work->noticeBody.coord            = rootCoord;
    work->noticeBody.context.contacts = work->noticeContacts;
    work->noticeBody.pos.vx           = 0;
    work->noticeBody.pos.vy           = ACTOR_02500_NOTICE_Y;
    work->noticeBody.pos.vz           = ACTOR_02500_NOTICE_RADIUS;
    work->noticeBody.key              = 0;
    work->noticeBody.radius           = ACTOR_02500_NOTICE_RADIUS;
    work->noticeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->noticeBody);
    worldCollisionInitContacts(work->noticeContacts, ARRAY_SIZE(work->noticeContacts), 0);

    work->hitBody.coord            = rootCoord;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = -ACTOR_02500_BODY_RADIUS;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02500_BODY_ID;
    work->hitBody.radius           = ACTOR_02500_BODY_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->noticeBody.flags        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);

    if (enemy->place->mode == ACTOR_02500_PLACE_ROAM) {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    work->gridBody.coord            = rootCoord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -ACTOR_02500_BODY_RADIUS;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02500_BODY_ID;
    work->gridBody.radius           = ACTOR_02500_BODY_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->attackBody.coord            = actor->extra.tmd->coords + ACTOR_02500_TAIL_COORD;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = ACTOR_02500_STING_Y;
    work->attackBody.pos.vz           = ACTOR_02500_STING_Z;
    work->attackBody.key              = damagePackAttackKey(Actor02500_D05B30, ACTOR_02500_STING_ATTACK_INDEX);
    work->attackBody.radius           = ACTOR_02500_BODY_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Allocates the scorpion's work, animation rig and four collision spheres.
///
/// Requires a live enemy, its placement and a five-coordinate model. Placement
/// modes 0/1/2 select roaming, proximity-triggered ambush and signalled ambush.
/// The task owns the zeroed work; allocation failure destroys both arguments.
/// Success links the target and bodies, acquires a battle reference and enters
/// the active task state. Buried instances cannot receive hits until emerging.
static void _actor02500Spawn(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_02500_HIT_EFFECT_ARG_LO = 0x200
    };

    _Actor02500Work* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              slotIndex;

    model     = actor->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Actor02500Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work             = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->bodyPos.vy             = -0x96;
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &Actor02500_D05B38;
    enemy->hp                     = Actor02500_D05B38.hpMax;
    work->hitEffectArg.spawnArgLo = ACTOR_02500_HIT_EFFECT_ARG_LO;
    work->hitEffectArg.coord      = rootCoord;
    work->hitEffectArg.spawnArgHi = 1;
    animationInitContext(&work->rig.anim, Actor02500_D05BA0, model, work->rig.poses, work->rig.slots);
    work->anim        = ACTOR_02500_ANIM_STAND;
    work->playingAnim = ACTOR_02500_ANIM_STAND;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, work->anim);
    }
    sceneAcquireBattleRef(0);
    switch (enemy->place->mode) {
        case ACTOR_02500_PLACE_ROAM:
            work->action     = ACTOR_02500_ACTION_WANDER;
            work->actionStep = ACTOR_02500_WANDER_STEP_STAND;
            enemy->recs      = work->hitContacts;
            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer      = ((gRandomLcgState >> 16) & ACTOR_02500_SHORT_WAIT_MASK) + ACTOR_02500_WAIT_MIN_TICKS;
            break;
        case ACTOR_02500_PLACE_AMBUSH_LEADER:
            work->action     = ACTOR_02500_ACTION_AMBUSH;
            work->actionStep = ACTOR_02500_AMBUSH_STEP_WAIT_NEAR;
            enemy->recs      = NULL;
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            break;
        case ACTOR_02500_PLACE_AMBUSH_FOLLOWER:
            work->action     = ACTOR_02500_ACTION_AMBUSH;
            work->actionStep = ACTOR_02500_AMBUSH_STEP_WAIT_SIGNAL;
            enemy->recs      = NULL;
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            break;
    }
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->idleSoundTimer = ((gRandomLcgState >> 16) & ACTOR_02500_LONG_WAIT_MASK) + ACTOR_02500_WAIT_MIN_TICKS;
    work->home.vx        = rootCoord->coord.t[0];
    work->home.vy        = rootCoord->coord.t[1];
    work->home.vz        = rootCoord->coord.t[2];
    work->targetYaw      = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;

    _actor02500InitCollisionBodies(enemy, actor, work, rootCoord);
    actor->state = ACTOR_02500_TASK_STATE_ACTIVE;
}

/// Queues a scorpion sound at an already composed coordinate's origin.
///
/// soundId includes its placement instance byte. The coordinate is borrowed
/// for the two spatial queries and is not retained by the queued event.
static __inline__ void _actor02500RequestSound(const GfxCoord* coord, s32 soundId)
{
    s32 panOffset;

    panOffset = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(coord));
}

/// Resolves grid correction, body overlap and attack damage for the scorpion.
///
/// Requires initialized work, a live Enemy and composed root/player coordinates.
/// Opposed grid contacts restore the pre-movement position. Attack keys select
/// player or companion roots; damage is applied before reaction and hit cooldown.
/// Body contacts retain only the strongest horizontal push, in Q12 room axes.
/// Clears all four contact tables and borrows one ActorOverlapPushScratch block.
static void _actor02500ResolveContacts(Task* actor)
{
    enum {
        ACTOR_02500_HIT_REACTION_BURST         = 4,
        ACTOR_02500_HIT_REACTION_DOUBLE_DAMAGE = 5,
        ACTOR_02500_CRITICAL_EFFECT_STYLE      = 0,
        ACTOR_02500_DOUBLE_DAMAGE_EFFECT_STYLE = 2,
        ACTOR_02500_BURST_REQUESTED            = 1,
        ACTOR_02500_ATTACKER_SELECT_SHIFT      = 7,
        ACTOR_02500_CONTACT_KIND_SHIFT         = 16,
        ACTOR_02500_PUSH_FRACTION_BITS         = 12
    };

    u32                      lastHitEffectKey;
    _Actor02500Work*         work;
    Enemy*                   enemy;
    GfxCoord*                rootCoord;
    GfxCoord*                attackerCoord;
    ActorOverlapPushScratch* scratchHead;
    ActorOverlapPushScratch* scratch;
    VECTOR*                  separationNormal;
    s32                      contactIndex;
    s32                      overlap;
    s32                      maxOverlap;
    s32                      damage;
    s32                      reaction;
    s32                      hitCooldown;
    s32                      soundId;

/// Keeps the strongest body overlap and its room-axis direction.
///
/// Captures rootCoord, work, scratch, separationNormal, overlap and maxOverlap.
/// contactIndex is evaluated repeatedly and must be a side-effect-free index
/// in work->hitContacts. Expands to statements; use as a standalone switch arm.
#define ACTOR_02500_RETAIN_BODY_OVERLAP(contactIndex)                                                                                 \
    scratch->delta.vector.vx = rootCoord->workm.t[0] - work->hitContacts[contactIndex].point.vx;                                      \
    scratch->delta.vector.vy = rootCoord->workm.t[1] - work->hitContacts[contactIndex].point.vy;                                      \
    scratch->delta.vector.vz = rootCoord->workm.t[2] - work->hitContacts[contactIndex].point.vz;                                      \
    overlap                  = work->hitContacts[contactIndex].distance -                                                             \
              SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + \
                          scratch->delta.vector.vz * scratch->delta.vector.vz);                                                       \
    overlap = (overlap <= 0) ? 0 : overlap;                                                                                           \
    if (maxOverlap < overlap) {                                                                                                       \
        maxOverlap = overlap;                                                                                                         \
        VectorNormal(&scratch->delta.vector, separationNormal);                                                                       \
        ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, separationNormal, &scratch->pushDirection);                          \
    }

    maxOverlap       = 0;
    lastHitEffectKey = 0;
    work             = actor->work;
    scratchHead      = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    scratch = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch) = scratchHead - 1;
    rootCoord                                               = actor->extra.tmd->coords;
    enemy                                                   = actor->spawnArg2.pointer;
    work->blocked                                           = 0;
    // Apply grid correction, or roll back movement when the contacts oppose it.
    switch (worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratchHead[-1].delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            if (scratchHead[-1].delta.fixed.vx.word != 0 || scratch->delta.fixed.vz.word != 0) {
                work->blocked = 1;
            }
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevPos.vx;
            rootCoord->coord.t[1] = work->prevPos.vy;
            rootCoord->coord.t[2] = work->prevPos.vz;
            if (scratchHead[-1].delta.fixed.vx.word != 0 || scratch->delta.fixed.vz.word != 0) {
                work->blocked = 1;
            }
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    separationNormal = &scratch->normal;
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        switch ((u32)work->hitContacts[contactIndex].key.value >> ACTOR_02500_CONTACT_KIND_SHIFT) {
            case WORLD_COLLISION_CONTACT_ATTACK >> ACTOR_02500_CONTACT_KIND_SHIFT:
                if (work->hitCooldown == 0) {
                    attackerCoord            = gPlayerActorTasks[((u32)work->hitContacts[contactIndex].key.value >> ACTOR_02500_ATTACKER_SELECT_SHIFT) & 1]->extra.tmd->coords;
                    scratch->delta.vector.vx = attackerCoord->coord.t[0] - rootCoord->coord.t[0];
                    scratch->delta.vector.vy = attackerCoord->coord.t[1] - rootCoord->coord.t[1];
                    scratch->delta.vector.vz = attackerCoord->coord.t[2] - rootCoord->coord.t[2];
                    damage                   = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value,
                                                                         SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                                                     scratch->delta.vector.vz * scratch->delta.vector.vz),
                                                                         0, 0);
                    reaction                 = damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value);
                    if ((reaction & 0xFFFF) == ACTOR_02500_HIT_REACTION_DOUBLE_DAMAGE) {
                        damage *= 2;
                        effectSpawn(EFFECT_CRITICAL_HIT, rootCoord, ACTOR_02500_DOUBLE_DAMAGE_EFFECT_STYLE, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->hitContacts[contactIndex].key.value, 0) != 0) {
                        damage *= 4;
                        if ((reaction & 0xFFFF) != ACTOR_02500_HIT_REACTION_DOUBLE_DAMAGE) {
                            effectSpawn(EFFECT_CRITICAL_HIT, rootCoord, ACTOR_02500_CRITICAL_EFFECT_STYLE, NULL);
                        }
                    }
                    damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, damage, 0);
                    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                    enemy->hp -= damage;
                    if (enemy->hp <= 0) {
                        work->action            = ACTOR_02500_ACTION_DIE;
                        work->actionStep        = ACTOR_02500_DEATH_STEP_BEGIN;
                        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        soundId                 = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_DEATH;
                        _actor02500RequestSound(rootCoord, soundId);
                    } else {
                        if (work->flinchGuard == 0) {
                            work->action     = ACTOR_02500_ACTION_FLINCH;
                            work->actionStep = ACTOR_02500_REACTION_STEP_BEGIN;
                        }
                        work->flinchGuard = 0;
                        soundId           = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_HIT;
                        _actor02500RequestSound(rootCoord, soundId);
                    }
                    switch (reaction & 0xFFFF) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        case DAMAGE_PLAYER_REACTION_POISON:
                        case ACTOR_02500_HIT_REACTION_DOUBLE_DAMAGE:
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        case 8:
                        case 9:
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                            if (work->inBuildup == 0) {
                                damageStartEnemyStagger(enemy);
                            }
                            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(enemy, work->hitContacts[contactIndex].key.value, 0);
                            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            break;
                        case ACTOR_02500_HIT_REACTION_BURST:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (enemy->hp <= 0) {
                                work->burstStage = ACTOR_02500_BURST_REQUESTED;
                            }
                            break;
                    }
                    if (lastHitEffectKey != work->hitContacts[contactIndex].key.value) {
                        lastHitEffectKey = work->hitContacts[contactIndex].key.value;
                        effectSpawnHit(damageGetPlayerAttackEffectId(lastHitEffectKey), rootCoord, 0, &work->hitEffectArg);
                    }
                    hitCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                    if (hitCooldown > 0) {
                        work->hitCooldown = hitCooldown;
                    }
                }
                break;
            case 0:
                break;
            // Player and enemy bodies use the same separation calculation.
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> ACTOR_02500_CONTACT_KIND_SHIFT:
                ACTOR_02500_RETAIN_BODY_OVERLAP(contactIndex);
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> ACTOR_02500_CONTACT_KIND_SHIFT:
                ACTOR_02500_RETAIN_BODY_OVERLAP(contactIndex);
                break;
        }
    }
    if (maxOverlap > 0) {
        rootCoord->coord.t[0] += (maxOverlap * scratch->pushDirection.vx) >> ACTOR_02500_PUSH_FRACTION_BITS;
        rootCoord->coord.t[2] += (maxOverlap * scratch->pushDirection.vz) >> ACTOR_02500_PUSH_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->hitContacts);
    if (worldCollisionFindContactIndex(work->attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        soundId                 = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_STING_HIT;
        _actor02500RequestSound(rootCoord, soundId);
    }
    worldCollisionClearContacts(work->attackContacts);
    if (worldCollisionCountContactsByKind(work->noticeContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0 && work->action == ACTOR_02500_ACTION_WANDER) {
        work->action     = ACTOR_02500_ACTION_CHASE;
        work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
        sceneEngageBattle(1);
    }
    worldCollisionClearContacts(work->noticeContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOverlapPushScratch);

#undef ACTOR_02500_RETAIN_BODY_OVERLAP
}

/// Alternates standing, turning and walking within 2000 coordinate units of home.
///
/// Requires initialized work and a placement row in 0..7. The row selects
/// speed and turn rate; timers count active ticks. Returning home overrides
/// the radius test until the next stand ends. Borrows one VECTOR scratch block;
/// bearing inputs retain signed-halfword narrowing, while distances use words.
static void _actor02500Wander(Task* actor)
{
    enum { ACTOR_02500_HOME_RADIUS = 2000 };

    _Actor02500Work* work;
    GfxCoord*        rootCoord;
    s16              standTicks;
    s16              walkTicks;
    s16              wanderStep;
    u32              randomYawState;
    u32              randomWalkState;
    s32              homeDx;
    u32              randomStandState;
    s32              homeDz;
    s32              randomStandTicks;
    VECTOR*          homeOffset;
    VECTOR*          scratchHead;

    scratchHead                = SCRATCH_STACK_CURSOR(VECTOR);
    homeOffset                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(void) = homeOffset;
    work                       = actor->work;
    wanderStep                 = work->actionStep;
    rootCoord                  = actor->extra.tmd->coords;
    switch (wanderStep) {
        case ACTOR_02500_WANDER_STEP_STAND:
            work->anim  = ACTOR_02500_ANIM_STAND;
            work->speed = 0;
            standTicks  = (u16)work->timer - 1;
            work->timer = standTicks;
            if (standTicks <= 0) {
                work->headingHome = 0;
                work->anim        = ACTOR_02500_ANIM_WALK;
                work->actionStep  = ACTOR_02500_WANDER_STEP_TURN;
                randomYawState    = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = randomYawState;
                work->targetYaw   = (randomYawState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            }
            break;
        case ACTOR_02500_WANDER_STEP_TURN:
            work->speed = 0;
            if (work->yaw == (s16)work->targetYaw) {
                work->actionStep = ACTOR_02500_WANDER_STEP_WALK;
                randomWalkState  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomWalkState;
                work->timer      = ((randomWalkState >> 16) & ACTOR_02500_LONG_WAIT_MASK) + ACTOR_02500_WAIT_MIN_TICKS;
            }
            break;
        case ACTOR_02500_WANDER_STEP_WALK:
            work->speed    = Actor02500_D05B58[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
            homeOffset->vx = work->home.vx - rootCoord->coord.t[0];
            homeOffset->vy = 0;
            homeDz         = work->home.vz - rootCoord->coord.t[2];
            homeOffset->vz = homeDz;
            homeDx         = homeOffset->vx;
            if ((SquareRoot0((homeDx * homeDx) + (homeDz * homeDz)) >= ACTOR_02500_HOME_RADIUS) && (work->headingHome == 0)) {
                work->actionStep = ACTOR_02500_WANDER_STEP_HEAD_HOME;
            } else {
                if (work->blocked != 1) {
                    walkTicks   = (u16)work->timer - 1;
                    work->timer = walkTicks;
                    if (walkTicks > 0) {
                        break;
                    }
                }
                work->actionStep = ACTOR_02500_WANDER_STEP_STAND;
                randomStandState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomStandState;
                randomStandTicks = (randomStandState >> 16) & ACTOR_02500_SHORT_WAIT_MASK;
                work->timer      = randomStandTicks + ACTOR_02500_WAIT_MIN_TICKS;
            }
            break;
        // Aim home in the root parent frame; the turn phase precedes walking.
        case ACTOR_02500_WANDER_STEP_HEAD_HOME:
            homeOffset->vx    = work->home.vx - rootCoord->coord.t[0];
            homeOffset->vy    = 0;
            homeOffset->vz    = work->home.vz - rootCoord->coord.t[2];
            work->targetYaw   = ratan2((s16)homeOffset->vx, (s16)homeOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->headingHome = 1;
            work->actionStep  = ACTOR_02500_WANDER_STEP_TURN;
            break;
    }
    work->turnRate = Actor02500_D05B48[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Chases the player, faces it at close range and gates the timed sting attack.
///
/// Requires initialized work, placement row 0..7 and a live player root in the
/// same parent frame. Distances use coordinate units and yaw uses 4096 per turn.
/// Far ticks spend the chase timer; after expiry, a player at least 2001 from
/// home sends the scorpion home. Borrows and releases one VECTOR scratch block.
static void _actor02500Chase(Task* actor)
{
    enum {
        ACTOR_02500_CHASE_PATIENCE_TICKS = 240,
        ACTOR_02500_STING_RADIUS         = 1000,
        ACTOR_02500_GIVE_UP_RADIUS       = 2001,
        ACTOR_02500_STING_YAW_TOLERANCE  = 48,
        ACTOR_02500_STING_ARM_TICK       = 41,
        ACTOR_02500_STING_SOUND_TICK     = 42,
        ACTOR_02500_STING_DISARM_TICK    = 44,
        ACTOR_02500_STING_END_TICK       = 76
    };

    _Actor02500Work* work;
    GfxCoord*        rootCoord;
    s16              chaseTicks;
    s32              chaseStep;
    s16              yawDelta;
    s32              stingFrame;
    s32              absYawDelta;
    s16              facingError;
    s32              soundId;
    s32              playerDx;
    s32              playerDz;
    s32              homeDx;
    s32              homeDz;
    VECTOR*          targetOffset;
    VECTOR*          scratchHead;

    scratchHead                = SCRATCH_STACK_CURSOR(VECTOR);
    targetOffset               = scratchHead - 1;
    SCRATCH_STACK_CURSOR(void) = targetOffset;
    work                       = actor->work;
    chaseStep                  = work->actionStep;
    rootCoord                  = actor->extra.tmd->coords;
    switch (chaseStep) {
        case ACTOR_02500_CHASE_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_RUN;
            work->timer      = ACTOR_02500_CHASE_PATIENCE_TICKS;
            work->speed      = 0;
            work->actionStep = ACTOR_02500_CHASE_STEP_RUN;
            break;
        case ACTOR_02500_CHASE_STEP_RUN:
            work->speed      = Actor02500_D05B78[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
            targetOffset->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            targetOffset->vy = 0;
            targetOffset->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            work->targetYaw  = ratan2((s16)targetOffset->vx, (s16)targetOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            playerDx         = targetOffset->vx;
            playerDz         = targetOffset->vz;
            if (SquareRoot0((playerDx * playerDx) + (playerDz * playerDz)) < ACTOR_02500_STING_RADIUS) {
                yawDelta    = work->targetYaw - (u16)work->yaw;
                absYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;
                work->speed = 0;
                if (absYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                    facingError = absYawDelta;
                } else if (yawDelta > 0) {
                    facingError = ACTOR_TRANSFORM_ANGLE_TURN - yawDelta;
                } else {
                    facingError = yawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
                }
                if (facingError < ACTOR_02500_STING_YAW_TOLERANCE) {
                    work->actionStep = ACTOR_02500_CHASE_STEP_STING;
                    work->anim       = ACTOR_02500_ANIM_STING;
                }
            } else {
                chaseTicks  = (u16)work->timer - 1;
                work->timer = chaseTicks;
                if (chaseTicks <= 0) {
                    targetOffset->vx = gPlayerStatus.coordMtx->t[0] - work->home.vx;
                    targetOffset->vy = 0;
                    homeDz           = gPlayerStatus.coordMtx->t[2] - work->home.vz;
                    targetOffset->vz = homeDz;
                    homeDx           = targetOffset->vx;
                    if (SquareRoot0((homeDx * homeDx) + (homeDz * homeDz)) >= ACTOR_02500_GIVE_UP_RADIUS) {
                        work->actionStep = ACTOR_02500_CHASE_STEP_GIVE_UP;
                    }
                }
            }
            break;
        // The hit pass may disarm a landed sting before its scheduled closing tick.
        case ACTOR_02500_CHASE_STEP_STING:
            stingFrame        = work->animFrame;
            work->speed       = 0;
            work->flinchGuard = 1;
            if (stingFrame == ACTOR_02500_STING_ARM_TICK) {
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else if (stingFrame == ACTOR_02500_STING_SOUND_TICK) {
                soundId = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_STING;
                _actor02500RequestSound(rootCoord, soundId);
            } else if (stingFrame == ACTOR_02500_STING_DISARM_TICK) {
                work->flinchGuard       = 0;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (stingFrame >= ACTOR_02500_STING_END_TICK) {
                work->actionStep = ACTOR_02500_CHASE_STEP_RUN;
                work->anim       = ACTOR_02500_ANIM_RUN;
            }
            break;
        case ACTOR_02500_CHASE_STEP_GIVE_UP:
            work->action     = ACTOR_02500_ACTION_WANDER;
            work->actionStep = ACTOR_02500_WANDER_STEP_HEAD_HOME;
            work->anim       = ACTOR_02500_ANIM_WALK;
            break;
    }
    work->turnRate = Actor02500_D05B68[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Advances idle and moving-stride sound timers while wandering or chasing.
///
/// Requires live work, enemy and a composed root for audio positioning. Idle
/// requests repeat after 30..157 calls; movement sounds occur at stride ticks
/// 9 and 18, and stopping resets the stride. Placement index selects the sound
/// instance. Requests borrow the cached coordinate and retain no pointer to it.
static void _actor02500TickSounds(Task* actor)
{
    enum { ACTOR_02500_STEP_FIRST_TICK  = 9,
           ACTOR_02500_STEP_SECOND_TICK = 18 };

    _Actor02500Work* work;
    GfxCoord*        rootCoord;
    s32              soundId;
    u32              randomState;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    work->idleSoundTimer--;
    if (work->idleSoundTimer <= 0) {
        randomState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->idleSoundTimer = ((randomState >> 16) & ACTOR_02500_LONG_WAIT_MASK) + ACTOR_02500_WAIT_MIN_TICKS;
        gRandomLcgState      = randomState;
        soundId              = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_IDLE;
        _actor02500RequestSound(rootCoord, soundId);
    }
    if (work->speed != 0) {
        work->stepSoundFrames++;
        if (work->stepSoundFrames == ACTOR_02500_STEP_FIRST_TICK) {
            soundId = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_STEP_FIRST;
            _actor02500RequestSound(rootCoord, soundId);
        } else if (work->stepSoundFrames == ACTOR_02500_STEP_SECOND_TICK) {
            soundId = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_STEP_SECOND;
            _actor02500RequestSound(rootCoord, soundId);
            work->stepSoundFrames = 0;
        }
    } else {
        work->stepSoundFrames = 0;
    }
}

/// Keeps a buried scorpion hidden, then staggers its emergence and starts chasing.
///
/// Requires live work/model/enemy storage and the player in the root's parent
/// frame. A leader signals at distance <2000, or a battle reward forces the
/// signal; followers wait for it. Placement index 0..15 delays emergence by
/// ten ticks each. Five dust bursts alternate four axis and diagonal offsets
/// at radii 300..363. Borrows ActorFaceScratch; effect offsets are copied at spawn.
static void _actor02500Ambush(Task* actor)
{
    enum {
        ACTOR_02500_AMBUSH_NOTICE_RADIUS           = 2000,
        ACTOR_02500_AMBUSH_DELAY_PER_INDEX_TICKS   = 10,
        ACTOR_02500_AMBUSH_HIDDEN_DUST_TICKS       = 10,
        ACTOR_02500_AMBUSH_DUST_TICKS              = 20,
        ACTOR_02500_AMBUSH_OPAQUE_TICK             = 16,
        ACTOR_02500_AMBUSH_CHASE_TICK              = 31,
        ACTOR_02500_AMBUSH_DUST_RADIUS_MIN         = 300,
        ACTOR_02500_AMBUSH_DUST_RADIUS_MASK        = 0x3F,
        ACTOR_02500_AMBUSH_PUFFS_PER_BURST         = 4,
        ACTOR_02500_AMBUSH_DUST_PERIOD_SHIFT       = 2,
        ACTOR_02500_AMBUSH_DUST_PERIOD_MASK        = 3,
        ACTOR_02500_AMBUSH_DIRECTION_FRACTION_BITS = 12,
        ACTOR_02500_AMBUSH_PUFF_SIZE               = 1024,
        ACTOR_02500_AMBUSH_PUFF_PERIOD_TICKS       = 2,
        ACTOR_02500_AMBUSH_PUFF_PERIOD_SHIFT       = 12,
        ACTOR_02500_AMBUSH_PUFF_SECONDARY_FLAG     = 0x80000000
    };

    TmdObject*                model;
    _Actor02500Work*          work;
    GfxCoord*                 rootCoord;
    s16                       actionTicks;
    s16                       dustTicks;
    s32                       soundId;
    s32                       distance;
    s32                       playerDx;
    s32                       playerDz;
    s32                       directionParity;
    s32                       puffIndex;
    u32                       randomState;
    ActorFaceScratch*         scratch;
    _Actor02500DustDirection* direction;

    rootCoord = actor->extra.tmd->coords;
    model     = actor->extra.tmd;
    work      = actor->work;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    switch (work->actionStep) {
        case ACTOR_02500_AMBUSH_STEP_WAIT_NEAR:
            model->flags                                               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            playerDx                                                   = gPlayerStatus.coordMtx->t[0] - work->home.vx;
            scratch->delta.vy                                          = 0;
            scratch->delta.vx                                          = playerDx;
            playerDz                                                   = gPlayerStatus.coordMtx->t[2] - work->home.vz;
            scratch->delta.vz                                          = playerDz;
            distance                                                   = SquareRoot0((playerDx * playerDx) + (playerDz * playerDz));
            if (distance < ACTOR_02500_AMBUSH_NOTICE_RADIUS || gSceneCombatState.actor02500EntranceReady != 0 || gSceneCombatState.expReward != 0) {
                gSceneCombatState.actor02500EntranceReady = 1;
                work->actionStep                          = ACTOR_02500_AMBUSH_STEP_DELAY;
                work->timer                               = ((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * ACTOR_02500_AMBUSH_DELAY_PER_INDEX_TICKS;
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_WAIT_SIGNAL:
            model->flags                                               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (gSceneCombatState.actor02500EntranceReady != 0 || gSceneCombatState.expReward != 0) {
                work->actionStep = ACTOR_02500_AMBUSH_STEP_DELAY;
                work->timer      = ((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * ACTOR_02500_AMBUSH_DELAY_PER_INDEX_TICKS;
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_DELAY:
            model->flags                                               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            actionTicks                                                = (u16)work->timer - 1;
            work->timer                                                = actionTicks;
            if (actionTicks <= 0) {
                work->actionStep = ACTOR_02500_AMBUSH_STEP_DUST;
                work->timer      = ACTOR_02500_AMBUSH_HIDDEN_DUST_TICKS;
                work->dustTimer  = ACTOR_02500_AMBUSH_DUST_TICKS;
                soundId          = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_EMERGE;
                _actor02500RequestSound(rootCoord, soundId);
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_DUST:
            actionTicks = (u16)work->timer - 1;
            work->timer = actionTicks;
            if (actionTicks > 0) {
                model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                // Expose the model and hit sphere together after the hidden dust wait.
                worldCoordSetActorColorMode(actor->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                model->flags                             = model->flags | TMD_OBJECT_SEMI_TRANS;
                work->hitBody.flags                     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                ((Enemy*)actor->spawnArg2.pointer)->recs = work->hitContacts;
                work->anim                               = ACTOR_02500_ANIM_EMERGE;
                work->timer                              = 0;
                work->actionStep                         = ACTOR_02500_AMBUSH_STEP_EMERGE;
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_EMERGE:
            actionTicks = (u16)work->timer + 1;
            work->timer = actionTicks;
            if (actionTicks < ACTOR_02500_AMBUSH_OPAQUE_TICK) {
                model->flags = model->flags | TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer >= ACTOR_02500_AMBUSH_CHASE_TICK) {
                work->action     = ACTOR_02500_ACTION_CHASE;
                work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
                sceneEngageBattle(1);
            }
            break;
    }
    if (work->dustTimer != 0) {
        dustTicks       = (u16)work->dustTimer - 1;
        work->dustTimer = dustTicks;
        // One random radius per burst; the stepped table index stays in 0..7.
        if (!(dustTicks & ACTOR_02500_AMBUSH_DUST_PERIOD_MASK)) {
            randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            puffIndex       = 0;
            distance        = ((randomState >> 16) & ACTOR_02500_AMBUSH_DUST_RADIUS_MASK) + ACTOR_02500_AMBUSH_DUST_RADIUS_MIN;
            gRandomLcgState = randomState;
            directionParity = (((u16)work->dustTimer >> ACTOR_02500_AMBUSH_DUST_PERIOD_SHIFT) ^ 1) & 1;
            for (; puffIndex < ACTOR_02500_AMBUSH_PUFFS_PER_BURST; puffIndex++) {
                direction       = &Actor02500_D05BE8[directionParity + puffIndex * 2];
                scratch->rot.vx = (direction->x * distance) >> ACTOR_02500_AMBUSH_DIRECTION_FRACTION_BITS;
                scratch->rot.vy = 0;
                scratch->rot.vz = (direction->z * distance) >> ACTOR_02500_AMBUSH_DIRECTION_FRACTION_BITS;
                effectSpawn(EFFECT_DUST_PUFF, actor->extra.tmd->coords, ACTOR_02500_AMBUSH_PUFF_SECONDARY_FLAG | (ACTOR_02500_AMBUSH_PUFF_PERIOD_TICKS << ACTOR_02500_AMBUSH_PUFF_PERIOD_SHIFT) | ACTOR_02500_AMBUSH_PUFF_SIZE, &scratch->rot);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Turns the root toward targetYaw by at most turnRate units along the shorter arc.
///
/// Requires live work and model storage. targetYaw is 0..4095, with 4096 per
/// turn, and turnRate is nonnegative. Reads yaw from the basis and rebuilds a
/// pure yaw rotation, preserving translation; the caller invalidates composition.
/// At a half-turn tie the wrapped branch selects the turn direction. Borrows
/// and releases one ActorFaceScratch block; stored yaw retains halfword wrapping.
static void _actor02500TurnTowardTargetYaw(Task* actor)
{
    _Actor02500Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               yawDelta;
    s32               absYawDelta;
    s32               turnRate;
    s32               wrappedYaw;
    s32               nextYaw;
    s32               wrapTurnRate;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord   = actor->extra.tmd->coords;
    work        = actor->work;
    currentYaw  = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw   = work->targetYaw;
    yawDelta    = targetYaw - currentYaw;
    absYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (absYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= absYawDelta) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDelta <= 0) {
                nextYaw -= turnRate;
            } else {
                nextYaw += turnRate;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnRate = work->turnRate;
        if (yawDelta > 0 ? turnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : turnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrapTurnRate = work->turnRate;
            wrappedYaw   = work->yaw;
            if (yawDelta > 0) {
                work->yaw = wrappedYaw - wrapTurnRate;
            } else {
                work->yaw = wrappedYaw + wrapTurnRate;
            }
        }
    }
    // Replace the basis with pure yaw; translation stays in the existing matrix.
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Spawns the detached head and two pincers at model coordinate 1.
///
/// Requires a live five-part model and its Enemy placement key, plus the
/// current area's placement table. Each effect snapshots its selected model
/// during spawn and receives the placement's texture-page and CLUT-row offsets;
/// both packet halves are rebuilt when present. A failed spawn skips that part.
static void _actor02500SpawnBurstFragments(Task* actor)
{
    enum { ACTOR_02500_BURST_ANCHOR_COORD    = 1,
           ACTOR_02500_BURST_EFFECT_ARGUMENT = 0x100 };

    GameLocationKey location;
    u8              viewId;
    EffectWork*     headEffect;
    EffectWork*     pincer2Effect;
    EffectWork*     pincer1Effect;

/// Copies a spawned fragment's placement texture offsets and refreshes both halves.
///
/// Captures actor, location and viewId. fragmentEffect is evaluated repeatedly
/// and must be a side-effect-free pointer to live effect work or NULL.
/// A NULL effect skips the lookup. Expands to statements; invoke standalone.
#define ACTOR_02500_APPLY_FRAGMENT_TEXTURE(fragmentEffect)                                                       \
    if ((fragmentEffect) != NULL) {                                                                              \
        const GameLocationKey* sessionLocation;                                                                  \
        u32                    placementKey;                                                                     \
        TmdObject*             fragmentModel;                                                                    \
        u32                    placementIndex;                                                                   \
        const AreaPlacement*   placement;                                                                        \
        sessionLocation = &gGameSession->location.loc;                                                           \
        placementKey    = ((Enemy*)actor->spawnArg2.pointer)->placeKey;                                          \
        fragmentModel   = (fragmentEffect)->task->extra.tmd;                                                     \
        location.stage  = sessionLocation->stage;                                                                \
        location.area   = sessionLocation->area;                                                                 \
        location.room   = sessionLocation->room;                                                                 \
        viewId          = gGameSession->location.loc.view;                                                       \
        placementIndex  = placementKey >> ENEMY_PLACE_INDEX_SHIFT;                                               \
        location.view   = viewId;                                                                                \
        areaSyncLocationVariant(&location);                                                                      \
        placement                        = gpAreaPlaceAt(areaGetVariant(&location)->placements, placementIndex); \
        fragmentModel->texturePageOffset = placement->texturePageOffset;                                         \
        fragmentModel->clutRowOffset     = placement->clutRowOffset;                                             \
        if (fragmentModel->buffer != NULL) {                                                                     \
            tmdBuildBufferHalf(fragmentModel);                                                                   \
            tmdBuildBufferHalf(fragmentModel);                                                                   \
        }                                                                                                        \
    }

    // Each descriptor payload is consumed by the following model allocation.
    D_80067704[0] = &_gActor02500ScorpionBurstHead;
    headEffect    = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &actor->extra.tmd->coords[ACTOR_02500_BURST_ANCHOR_COORD], ACTOR_02500_BURST_EFFECT_ARGUMENT, NULL);
    ACTOR_02500_APPLY_FRAGMENT_TEXTURE(headEffect);
    D_80067704[0] = &_gActor02500ScorpionBurstPincer2;
    pincer2Effect = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &actor->extra.tmd->coords[ACTOR_02500_BURST_ANCHOR_COORD], ACTOR_02500_BURST_EFFECT_ARGUMENT, NULL);
    ACTOR_02500_APPLY_FRAGMENT_TEXTURE(pincer2Effect);
    D_80067704[0] = &_gActor02500ScorpionBurstPincer1;
    pincer1Effect = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &actor->extra.tmd->coords[ACTOR_02500_BURST_ANCHOR_COORD], ACTOR_02500_BURST_EFFECT_ARGUMENT, NULL);
    ACTOR_02500_APPLY_FRAGMENT_TEXTURE(pincer1Effect);

#undef ACTOR_02500_APPLY_FRAGMENT_TEXTURE
}

/// Releases all scorpion collision links before its work can be destroyed.
///
/// work owns the four live bodies and their contact tables; unlinking disables
/// their passes without freeing either the bodies or work.
static __inline__ void _actor02500UnlinkCollisionBodies(_Actor02500Work* work)
{
    worldCollisionUnlinkBody(&work->noticeBody);
    worldCollisionUnlinkBody(&work->hitBody);
    worldCollisionUnlinkBody(&work->gridBody);
    worldCollisionUnlinkBody(&work->attackBody);
}

/// Unlinks a dead scorpion and runs its collapse or fragment-burst teardown.
///
/// Requires initialized work/model storage and the task's live Enemy. Paused
/// combat refreshes color only; hidden combat suppresses drawing. Death entry
/// releases targeting, collision and the battle reference once. Collapse leaves
/// independently owned corpse poison at tick 15; bursting releases the primitive
/// buffer before spawning parts. Both wait 60 active calls before destruction.
static void _actor02500Die(Enemy* enemy, Task* actor)
{
    enum { ACTOR_02500_DEATH_TRANSLUCENT_TICK   = 10,
           ACTOR_02500_CORPSE_POISON_SPAWN_TICK = 15,
           ACTOR_02500_DEATH_END_TICK           = 60,
           ACTOR_02500_CORPSE_POISON_TASK_INDEX = 1,
           ACTOR_02500_BURST_SPAWN_TICK         = 2,
           ACTOR_02500_CORPSE_FLAME_BATCH_COUNT = 2 };

    _Actor02500Work* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    GfxCoord*        colorCoord;
    VECTOR           viewPosition;
    s32              actorControl;
    s16              deathTicks;

    model        = actor->extra.tmd;
    work         = actor->work;
    actorControl = gSceneCombatState.actorControl;
    rootCoord    = model->coords;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            viewPosition.vx = rootCoord->workm.t[0];
            viewPosition.vy = rootCoord->workm.t[1];
            viewPosition.vz = rootCoord->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &viewPosition, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (work->actionStep) {
        case ACTOR_02500_DEATH_STEP_BEGIN:
            // Release battle ownership before either corpse sequence outlives it.
            work->anim         = ACTOR_02500_ANIM_DIE;
            work->timer        = 0;
            work->deathScaleY  = ONE;
            work->savedRootMtx = rootCoord->coord;
            enemy->recs        = NULL;
            worldTargetUnlinkNode(&enemy->node);
            _actor02500UnlinkCollisionBodies(work);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(actor, 25);
            colorCoord      = actor->extra.tmd->coords;
            viewPosition.vx = colorCoord->workm.t[0];
            viewPosition.vy = colorCoord->workm.t[1];
            viewPosition.vz = colorCoord->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &viewPosition, 0, 0);
            if (work->burstStage == 0) {
                work->actionStep = ACTOR_02500_DEATH_STEP_COLLAPSE;
                return;
            }
            model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->actionStep = ACTOR_02500_DEATH_STEP_BURST;
            return;
        case ACTOR_02500_DEATH_STEP_COLLAPSE:
            _actor02500SquashCorpse(actor);
            deathTicks  = work->timer + 1;
            work->timer = deathTicks;
            if (deathTicks == ACTOR_02500_DEATH_TRANSLUCENT_TICK) {
                model->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer == ACTOR_02500_CORPSE_POISON_SPAWN_TICK) {
                effectSpawn(EFFECT_CORPSE_BURN, rootCoord, ACTOR_02500_CORPSE_FLAME_BATCH_COUNT, NULL);
                enemySpawnFromTable(Actor02500_D05B88, ACTOR_02500_CORPSE_POISON_TASK_INDEX, 0, enemy);
            }
            if (work->timer >= ACTOR_02500_DEATH_END_TICK) {
                model->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->actionStep = ACTOR_02500_DEATH_STEP_DESTROY;
            }
            colorCoord      = actor->extra.tmd->coords;
            viewPosition.vx = colorCoord->workm.t[0];
            viewPosition.vy = colorCoord->workm.t[1];
            viewPosition.vz = colorCoord->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &viewPosition, 0, 0);
            return;
        case ACTOR_02500_DEATH_STEP_DESTROY:
            enemyDestroy(enemy, actor);
            return;
        case ACTOR_02500_DEATH_STEP_BURST:
            if (work->burstStage != 0) {
                if (work->burstStage >= ACTOR_02500_BURST_SPAWN_TICK) {
                    work->burstStage = 0;
                    tmdFreePrimitiveBuffer(model);
                    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    _actor02500SpawnBurstFragments(actor);
                } else {
                    work->burstStage++;
                }
            }
            deathTicks  = work->timer + 1;
            work->timer = deathTicks;
            if (deathTicks >= ACTOR_02500_DEATH_END_TICK) {
                work->actionStep = ACTOR_02500_DEATH_STEP_DESTROY;
            }
            return;
    }
}

/// Dispatches the scorpion's spawn, active or death handler for this task.
///
/// The descriptor supplies a five-coordinate TMD model and spawnArg2 points
/// to its live Enemy. Task state must be 0..2; the selected handler may destroy
/// both objects, so nothing is accessed after dispatch. The callback is private
/// to this package family and reached through its exported task descriptor.
static void _actor02500Task(Task* actor)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor02500_D00004;
    handlers.funcs[actor->state](actor->spawnArg2.pointer, actor);
}

/// Advances scorpion contacts, behavior, motion, animation and presentation.
///
/// Requires initialized work/model storage and the task's Enemy. Running combat
/// consumes reactions before damage and action dispatch, then moves and composes
/// the root before color/shadow sampling. Paused ambushes do nothing; other paused
/// actors refresh presentation only. Hidden actors cannot be target-locked.
static void _actor02500Tick(Enemy* enemy, Task* actor)
{
    _Actor02500Work* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              actorControl;

    model        = actor->extra.tmd;
    actorControl = gSceneCombatState.actorControl;
    work         = actor->work;
    rootCoord    = model->coords;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->action == ACTOR_02500_ACTION_AMBUSH) {
                return;
            }
            _actor02500UpdateColor(actor);
            _actor02500DrawShadow(actor);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (enemy->reactionFlags != 0) {
        _actor02500ConsumeReactions(actor);
    }
    _actor02500ResolveContacts(actor);
    _actor02500DispatchAction(actor);
    if (work->turnRate != 0) {
        _actor02500TurnTowardTargetYaw(actor);
    }
    _actor02500Move(actor);
    _actor02500TickAnimation(actor);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _actor02500UpdateColor(actor);
    if (work->action != ACTOR_02500_ACTION_AMBUSH) {
        _actor02500DrawShadow(actor);
    }
}

/// Consumes enemy reaction requests, giving buildup priority over stagger.
///
/// Requires live enemy and work storage. Consumed bits are cleared in the
/// enemy byte without changing unrelated flags. Damage-over-time bits are
/// acknowledged without starting a movement action; this pass does no HP work.
static void _actor02500ConsumeReactions(Task* actor)
{
    u8               reactionFlags;
    u8               remainingFlags;
    _Actor02500Work* work;
    Enemy*           enemy;

    enemy         = actor->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = actor->work;
    if (reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
        work->action         = ACTOR_02500_ACTION_BUILDUP;
        work->actionStep     = ACTOR_02500_REACTION_STEP_BEGIN;
    }
    // A simultaneous stagger is acknowledged even when buildup takes precedence.
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
        if (work->action != ACTOR_02500_ACTION_BUILDUP) {
            work->action     = ACTOR_02500_ACTION_STAGGER;
            work->actionStep = ACTOR_02500_REACTION_STEP_BEGIN;
        }
    }
    remainingFlags = enemy->reactionFlags;
    if (remainingFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = remainingFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

/// State handlers of the helper task `_actor02500CorpsePoisonTask` dispatches, indexed
/// by `Task::state`: setup, per-frame tick and the countdown that
/// destroys it.
static const EnemyTaskFuncTable3 Actor02500_D00050 = {
    {
        _actor02500SpawnCorpsePoison,
        _actor02500TickCorpsePoison,
        _actor02500ReleaseCorpsePoison,
    },
};

/// Runs the scorpion's current action or hands its task to the death state.
///
/// Requires initialized work and an ACTOR_02500_ACTION_* value. Wander and chase
/// also advance sound timers; a death request takes effect on the next dispatch
/// of the task. Movement and animation remain the frame handler's responsibility.
static void _actor02500DispatchAction(Task* actor)
{
    _Actor02500Work* work;

    work = actor->work;
    switch (work->action) {
        case ACTOR_02500_ACTION_WANDER:
            _actor02500Wander(actor);
            _actor02500TickSounds(actor);
            break;
        case ACTOR_02500_ACTION_CHASE:
            _actor02500Chase(actor);
            _actor02500TickSounds(actor);
            break;
        case ACTOR_02500_ACTION_FLINCH:
            _actor02500Flinch(actor);
            break;
        case ACTOR_02500_ACTION_STAGGER:
            _actor02500Stagger(actor);
            break;
        case ACTOR_02500_ACTION_BUILDUP:
            _actor02500Buildup(actor);
            break;
        case ACTOR_02500_ACTION_AMBUSH:
            _actor02500Ambush(actor);
            break;
        case ACTOR_02500_ACTION_DIE:
            actor->state = ACTOR_02500_TASK_STATE_DEATH;
            break;
    }
}

/// Holds the hit recoil until animation tick 32, then restores combat behavior.
///
/// Requires initialized work and a reaction step. Buildup resumes before
/// stagger; a resumed stagger gets 60 fresh ticks. Otherwise chasing restarts.
static void _actor02500Flinch(Task* actor)
{
    enum { ACTOR_02500_FLINCH_END_TICK = 32 };

    _Actor02500Work* work;

    work = actor->work;
    switch (work->actionStep) {
        case ACTOR_02500_REACTION_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_FLINCH;
            work->speed      = 0;
            work->turnRate   = 0;
            work->actionStep = ACTOR_02500_REACTION_STEP_HOLD;
            return;
        case ACTOR_02500_REACTION_STEP_HOLD:
            if (work->animFrame >= ACTOR_02500_FLINCH_END_TICK) {
                if (work->inBuildup == 1) {
                    work->action     = ACTOR_02500_ACTION_BUILDUP;
                    work->actionStep = ACTOR_02500_REACTION_STEP_BEGIN;
                    return;
                }
                if (work->inStagger == 1) {
                    work->action     = ACTOR_02500_ACTION_STAGGER;
                    work->actionStep = ACTOR_02500_REACTION_STEP_HOLD;
                    work->timer      = ACTOR_02500_STAGGER_TICKS;
                    return;
                }
                work->action     = ACTOR_02500_ACTION_CHASE;
                work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
            } else {
                return;
            }
            break;
    }
}

/// Stops the scorpion for a 60-tick stagger, then restarts its chase.
///
/// Requires initialized work and a reaction step. The entry holds the flinch
/// animation and sets inStagger; the hold decrements a signed-halfword timer.
static void _actor02500Stagger(Task* actor)
{
    _Actor02500Work* work;

    work = actor->work;

    switch (work->actionStep) {
        case ACTOR_02500_REACTION_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_FLINCH;
            work->inStagger  = 1;
            work->speed      = 0;
            work->turnRate   = 0;
            work->timer      = ACTOR_02500_STAGGER_TICKS;
            work->actionStep = ACTOR_02500_REACTION_STEP_HOLD;
            break;
        case ACTOR_02500_REACTION_STEP_HOLD:
            if (--work->timer <= 0) {
                work->action     = ACTOR_02500_ACTION_CHASE;
                work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
                work->inStagger  = 0;
            }
            break;
    }
}

/// Stops the scorpion until its enemy buildup reaction has finished.
///
/// Requires initialized work and a started buildup reaction with valid enemy
/// parameters. Entry cancels the stagger latch; each hold advances buildup by
/// one call. Completion clears inBuildup and starts a fresh chase.
static void _actor02500Buildup(Task* actor)
{
    _Actor02500Work* work;

    work = actor->work;

    switch (work->actionStep) {
        case ACTOR_02500_REACTION_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_BUILDUP;
            work->inBuildup  = 1;
            work->inStagger  = 0;
            work->speed      = 0;
            work->turnRate   = 0;
            work->actionStep = ACTOR_02500_REACTION_STEP_HOLD;
            break;
        case ACTOR_02500_REACTION_STEP_HOLD:
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                work->action     = ACTOR_02500_ACTION_CHASE;
                work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
                work->inBuildup  = 0;
            }
            break;
    }
}

/// Saves the pre-movement root position, steps forward and adds the ground drop.
///
/// Requires live root and work storage. The Q12 forward X/Z basis times the
/// signed speed gives parent-coordinate displacement per tick; Y adds 128.
/// prevPos retains all three full-width components for opposed-grid rollback.
/// This pass leaves composition invalidation to the frame handler.
static void _actor02500Move(Task* actor)
{
    enum { ACTOR_02500_BASIS_FRACTION_BITS = 12,
           ACTOR_02500_GROUND_STEP         = 128 };

    _Actor02500Work* work;
    GfxCoord*        rootCoord;

    rootCoord              = actor->extra.tmd->coords;
    work                   = actor->work;
    work->prevPos.vx       = rootCoord->coord.t[0];
    work->prevPos.vy       = rootCoord->coord.t[1];
    work->prevPos.vz       = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->speed) >> ACTOR_02500_BASIS_FRACTION_BITS;
    rootCoord->coord.t[1] += ACTOR_02500_GROUND_STEP;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->speed) >> ACTOR_02500_BASIS_FRACTION_BITS;
}

/// Starts a changed animation request or advances model-part slots 1 through 4.
///
/// Requires an initialized five-slot rig and a valid package animation id
/// (1, 3, 4, 6, 7, 8, 10 or 11). A new request uses that id's blend duration
/// in frames and resets animFrame without ticking; unchanged requests advance
/// it as a signed halfword and tick each part. Root slot 0 is not driven.
static void _actor02500TickAnimation(Task* actor)
{
    _Actor02500Work* work;
    s16              requestedAnim;
    s32              blendFrames;
    s32              seekSlot;
    s32              tickSlot;

    work          = actor->work;
    requestedAnim = work->anim;
    // A changed request reseeds every part before any subsequent playback tick.
    if (requestedAnim != work->playingAnim) {
        blendFrames       = Actor02500_D05BD0[requestedAnim];
        seekSlot          = 1;
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        do {
            animationSeekSlotWithBlend(&work->rig.anim, seekSlot, work->anim, 0, blendFrames);
            seekSlot++;
        } while (seekSlot < ARRAY_SIZE(work->rig.slots));
        return;
    }
    tickSlot = 1;
    work->animFrame++;
    do {
        animationTickSlot(&work->rig.anim, tickSlot);
        tickSlot++;
    } while (tickSlot < ARRAY_SIZE(work->rig.slots));
}

/// Updates scorpion lighting and color at the root's cached view-space origin.
///
/// Requires a live Enemy and an already composed model root. Borrows a stack
/// position for the query and does not recompute the coordinate transform.
static void _actor02500UpdateColor(Task* actor)
{
    VECTOR    viewPosition;
    GfxCoord* rootCoord;

    rootCoord       = actor->extra.tmd->coords;
    viewPosition.vx = rootCoord->workm.t[0];
    viewPosition.vy = rootCoord->workm.t[1];
    viewPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &viewPosition, 0, 0);
}

/// Draws the scorpion's ground shadow at the root's cached translation.
///
/// Requires a live root whose cached transform maps into view space.
/// The square has a 512-coordinate-unit half-side and shade 128. Its temporary
/// XYZ record is borrowed only during drawing; this pass does not compose it.
static void _actor02500DrawShadow(Task* actor)
{
    enum { ACTOR_02500_SHADOW_HALF_SIZE = 512,
           ACTOR_02500_SHADOW_SHADE     = 128 };

    VECTOR3   viewPosition;
    GfxCoord* rootCoord;

    rootCoord       = actor->extra.tmd->coords;
    viewPosition.vx = rootCoord->workm.t[0];
    viewPosition.vy = rootCoord->workm.t[1];
    viewPosition.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&viewPosition, ACTOR_02500_SHADOW_HALF_SIZE, ACTOR_02500_SHADOW_SHADE);
}

/// Restores the saved death transform and applies its Q12 local Y scale.
///
/// The root, work and scratch records must be live and disjoint. The caller
/// owns scratch reservation and release. Restoring the entire saved matrix
/// preserves translation and avoids compounded scaling; composition is dirtied.
static __inline__ void _actor02500ApplyCorpseScale(GfxCoord* rootCoord, const _Actor02500Work* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->deathScaleY;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Squashes the corpse along local Y from its saved death transform.
///
/// Requires live work/model storage and savedRootMtx captured on death entry.
/// The Q12 scale decreases by 80 while above 512; a start of ONE ends at 496,
/// retaining the undershoot. X/Z stay at ONE. Restoring the saved matrix avoids
/// compounded scaling and preserves translation. Marks composition dirty and
/// releases its temporary ActorScaleScratch block before returning.
static void _actor02500SquashCorpse(Task* actor)
{
    enum { ACTOR_02500_CORPSE_SCALE_CUTOFF_Q12 = 512,
           ACTOR_02500_CORPSE_SCALE_STEP_Q12   = 80 };

    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    _Actor02500Work*   work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = actor->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = actor->extra.tmd->coords;
    if (work->deathScaleY > ACTOR_02500_CORPSE_SCALE_CUTOFF_Q12) {
        work->deathScaleY -= ACTOR_02500_CORPSE_SCALE_STEP_Q12;
    }
    _actor02500ApplyCorpseScale(rootCoord, work, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Dispatches corpse-poison setup, active contact/lifetime checks or release.
///
/// The descriptor supplies a coordinate body, with a separately allocated Enemy
/// at spawnArg2 and task state 0..2. Setup needs its parent scorpion model;
/// success detaches the task, so it survives that parent. A handler may destroy
/// the task and Enemy; nothing is accessed after the call.
static void _actor02500CorpsePoisonTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor02500_D00050;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Copies the corpse transform into an independent poison sphere and ground decal.
///
/// Requires a newly spawned coordinate task, its own Enemy and its parent
/// scorpion's live model. Allocates zeroed task-owned work; failure destroys
/// the task and Enemy. Success links a radius-200 poison attack sphere and
/// detaches the task after copying the full parent-space matrix. The optional
/// decal belongs to its effect task; no parent transform pointer is retained.
static void _actor02500SpawnCorpsePoison(Enemy* enemy, Task* task)
{
    enum { ACTOR_02500_CORPSE_POISON_RADIUS           = 200,
           ACTOR_02500_CORPSE_POISON_ATTACK_INDEX     = 1,
           ACTOR_02500_CORPSE_POISON_STATE_ACTIVE     = 1,
           ACTOR_02500_CORPSE_POISON_DECAL_HALF_SIZE  = 640,
           ACTOR_02500_CORPSE_POISON_DECAL_CLUT       = 1,
           ACTOR_02500_CORPSE_POISON_DECAL_CLUT_SHIFT = 16 };

    _Actor02500CorpsePoisonWork* work;
    GfxCoord*                    poisonCoord;
    WorldCollisionContact*       contacts;
    GfxCoord*                    corpseCoord;
    EffectWork*                  decal;

    poisonCoord = task->extra.coordBody->coord;
    corpseCoord = task->parent->extra.tmd->coords;
    work        = memCalloc(sizeof(_Actor02500CorpsePoisonWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work = work;
    // Stand where the corpse lies, under the view rather than the scorpion.
    poisonCoord->parent = &gGfxViewCoord;
    poisonCoord->coord  = corpseCoord->coord;
    // Keep the explicit translation stores before invalidating the copied pose.
    poisonCoord->coord.t[0]     = corpseCoord->coord.t[0];
    poisonCoord->coord.t[1]     = corpseCoord->coord.t[1];
    poisonCoord->coord.t[2]     = corpseCoord->coord.t[2];
    poisonCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    decal                       = effectSpawn((EFFECT_GROUND_DECAL | EFFECT_SPAWN_UNLIMITED), poisonCoord,
                                              (ACTOR_02500_CORPSE_POISON_DECAL_CLUT << ACTOR_02500_CORPSE_POISON_DECAL_CLUT_SHIFT) | ACTOR_02500_CORPSE_POISON_DECAL_HALF_SIZE, NULL);
    work->body.coord            = poisonCoord;
    contacts                    = work->contacts;
    work->decal                 = decal;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = damagePackAttackKey(Actor02500_D05B30, ACTOR_02500_CORPSE_POISON_ATTACK_INDEX);
    work->body.radius           = ACTOR_02500_CORPSE_POISON_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags = work->body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    taskDetachFromParent(task);
    task->state = ACTOR_02500_CORPSE_POISON_STATE_ACTIVE;
}

/// Watches corpse-poison contacts and its active lifetime, then requests release.
///
/// Requires the detached coordinate-body task, its owned Enemy and initialized work.
/// Only running combat ticks clear contacts and advance the signed-halfword
/// timer. A player-body contact, timer greater than 240 or zero battle references
/// selects release state 2. A contact also queues the poison-contact sound;
/// the collision body and decal remain live until the release handler runs.
static void _actor02500TickCorpsePoison(Enemy* enemy, Task* task)
{
    enum { ACTOR_02500_CORPSE_POISON_TIMEOUT_TICKS = 240,
           ACTOR_02500_CORPSE_POISON_STATE_RELEASE = 2 };

    s32                          soundId;
    GfxCoord*                    coord;
    WorldCollisionContact*       contacts;
    s32                          releaseRequested;
    u16                          elapsedTicks;
    _Actor02500CorpsePoisonWork* work;

    coord            = task->extra.coordBody->coord;
    work             = task->work;
    releaseRequested = 0;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        contacts = work->contacts;
        // A contact of the player's category: the poison has been delivered.
        if (worldCollisionCountContactsByKind(contacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
            releaseRequested = 1;
            soundId          = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_02500_SOUND_INSTANCE_SHIFT) | ACTOR_02500_SOUND_POISON_CONTACT;
            _actor02500RequestSound(coord, soundId);
        }
        worldCollisionClearContacts(contacts);
        elapsedTicks = work->timer + 1;
        work->timer  = elapsedTicks;
        if ((s16)elapsedTicks > ACTOR_02500_CORPSE_POISON_TIMEOUT_TICKS) {
            releaseRequested = 1;
        }
        if (gSceneCombatState.battleRefs == 0) {
            releaseRequested = 1;
        }
        if (releaseRequested != 0) {
            work->releaseStep = ACTOR_02500_CORPSE_POISON_RELEASE_BEGIN;
            task->state       = ACTOR_02500_CORPSE_POISON_STATE_RELEASE;
        }
    }
}

/// Unlinks corpse poison, asks its decal to fade and destroys it after 30 calls.
///
/// Requires the poison task, its live Enemy and initialized work. The optional
/// decal belongs to its effect task; this handler only changes its state. The
/// linger counts every invocation, including paused combat. Destruction releases
/// the task and enemy, and neither may be accessed afterwards.
static void _actor02500ReleaseCorpsePoison(Enemy* enemy, Task* task)
{
    enum { ACTOR_02500_CORPSE_POISON_LINGER_TICKS = 30 };

    _Actor02500CorpsePoisonWork* work = task->work;

    switch (work->releaseStep) {
        case ACTOR_02500_CORPSE_POISON_RELEASE_BEGIN:
            worldCollisionUnlinkBody(&work->body);
            if (work->decal != NULL) {
                work->decal->task->state = EFFECT_GROUND_DECAL_STATE_FADE;
            }
            work->timer       = ACTOR_02500_CORPSE_POISON_LINGER_TICKS;
            work->releaseStep = ACTOR_02500_CORPSE_POISON_RELEASE_LINGER;
            break;
        case ACTOR_02500_CORPSE_POISON_RELEASE_LINGER:
            if (--work->timer > 0) {
                break;
            }
            enemyDestroy(enemy, task);
            break;
    }
}
