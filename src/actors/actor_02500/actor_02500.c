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
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/object_fields.h"
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

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

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

static void Actor02500_Fn00078(Enemy* ctx, Task* actor);
static void Actor02500_Fn01AC8(Enemy* ctx, Task* actor);
static void Actor02500_Fn01E60(Enemy* ctx, Task* actor);
static void Actor02500_Fn01F8C(Task* actor);
static void Actor02500_Fn02008(Task* actor);
static void Actor02500_Fn020D0(Task* actor);
static void Actor02500_Fn02178(Task* actor);
static void Actor02500_Fn021F8(Task* actor);
static void Actor02500_Fn02288(Task* actor);
static void Actor02500_Fn02318(Task* actor);
static void Actor02500_Fn023D8(Task* actor);
static void Actor02500_Fn02430(Task* actor);
static void Actor02500_Fn02480(Task* actor);
static void Actor02500_Fn025D0(Enemy* ctx, Task* task);
static void Actor02500_Fn02750(Enemy* ctx, Task* task);
static void Actor02500_Fn02874(Enemy* ctx, Task* task);

/// State handlers of the enemy task `Actor02500_Fn01E04` dispatches, indexed
/// by `Task::state`: spawn, per-frame tick and the dying sequence.
static const EnemyTaskFuncTable3 Actor02500_D00004 = {
    {
        Actor02500_Fn00078,
        Actor02500_Fn01E60,
        Actor02500_Fn01AC8,
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
void                Actor02500_Fn01E04(Task*);
void                Actor02500_Fn02574(Task*);

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
    { { { TASK_BODY_TMD, 96 } }, Actor02500_Fn01E04, { .model = &_gActor02500ScorpionBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor02500_Fn02574, { .value = 0 } },
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

static void Actor02500_Fn00494(Task* actor);
static void Actor02500_Fn00B18(Task* actor);
static void Actor02500_Fn00DD8(Task* actor);
static void Actor02500_Fn01144(Task* actor);
static void Actor02500_Fn012F0(Task* actor);
static void Actor02500_Fn016FC(Task* arg0);
static void Actor02500_Fn0184C(Task* arg0);

static void Actor02500_Fn00078(Enemy* ctx, Task* actor)
{
    _Actor02500Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              i;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor02500Work), 0);
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work         = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    ctx->field_4        = &coord->coord;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->bodyPos.vy               = -0x96;
    ctx->coord                    = coord;
    ctx->node.state.parts.flags   = 0;
    ctx->bodyPos.vx               = 0;
    ctx->bodyPos.vz               = 0;
    ctx->param                    = &Actor02500_D05B38;
    ctx->hp                       = Actor02500_D05B38.hpMax;
    work->hitEffectArg.spawnArgLo = 0x200;
    work->hitEffectArg.coord      = coord;
    work->hitEffectArg.spawnArgHi = 1;
    animationInitContext(&work->rig.anim, Actor02500_D05BA0, obj, work->rig.poses, work->rig.slots);
    work->anim        = ACTOR_02500_ANIM_STAND;
    work->playingAnim = ACTOR_02500_ANIM_STAND;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, work->anim);
    }
    Gp_IncStateF0Ref(0);
    switch (ctx->place->mode) {
        case 0:
            work->action     = ACTOR_02500_ACTION_WANDER;
            work->actionStep = ACTOR_02500_WANDER_STEP_STAND;
            ctx->recs        = work->hitContacts;
            gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->timer      = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            break;
        case 1:
            work->action     = ACTOR_02500_ACTION_AMBUSH;
            work->actionStep = ACTOR_02500_AMBUSH_STEP_WAIT_NEAR;
            ctx->recs        = NULL;
            Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
            break;
        case 2:
            work->action     = ACTOR_02500_ACTION_AMBUSH;
            work->actionStep = ACTOR_02500_AMBUSH_STEP_WAIT_SIGNAL;
            ctx->recs        = NULL;
            Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
            break;
    }
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->idleSoundTimer = ((gRandomLcgState >> 16) & 0x7F) + 0x1E;
    work->home.vx        = coord->coord.t[0];
    work->home.vy        = coord->coord.t[1];
    work->home.vz        = coord->coord.t[2];
    work->targetYaw      = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;

    work->noticeBody.coord            = coord;
    work->noticeBody.context.contacts = work->noticeContacts;
    work->noticeBody.pos.vx           = 0;
    work->noticeBody.pos.vy           = -0x190;
    work->noticeBody.pos.vz           = 0x258;
    work->noticeBody.key              = 0;
    work->noticeBody.radius           = 0x258;
    work->noticeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->noticeBody);
    worldCollisionInitContacts(work->noticeContacts, ARRAY_SIZE(work->noticeContacts), 0);

    work->hitBody.coord            = coord;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = -0x12C;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30019;
    work->hitBody.radius           = 0x12C;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->noticeBody.flags        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);

    if (ctx->place->mode == 0) {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0x12C;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30019;
    work->gridBody.radius           = 0x12C;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->attackBody.coord            = actor->extra.tmd->coords + 4;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = -0x3B6;
    work->attackBody.pos.vz           = 0x1CC;
    work->attackBody.key              = Gp_PackPair(Actor02500_D05B30, 0);
    work->attackBody.radius           = 0x12C;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->state            = 1;
}

/// Per-frame collision and damage pass. Carves an `ActorOverlapPushScratch` off
/// the scratchpad stack, lets `func_800E0C10` resolve this frame's movement
/// into it, then walks the three `hitContacts` records: kind 2 is a hit that
/// costs the enemy HP and plays a sound, kinds 1 and 3 push it away from the
/// obstacle, and the strongest push is applied to the coordinate at the end.
static void Actor02500_Fn00494(Task* actor)
{
    u32                      lastId;
    _Actor02500Work*         work;
    Enemy*                   ctx;
    GfxCoord*                coord;
    GfxCoord*                target;
    ActorOverlapPushScratch* head;
    ActorOverlapPushScratch* frame;
    VECTOR*                  normal;
    s32                      i;
    s32                      push;
    s32                      bestPush;
    s32                      damage;
    s32                      param0;
    s32                      cooldown;
    s32                      soundId;

    bestPush = 0;
    lastId   = 0;
    work     = actor->work;
    head     = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    frame = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch) = head - 1;
    coord                                                 = actor->extra.tmd->coords;
    ctx                                                   = actor->spawnArg2.pointer;
    work->blocked                                         = 0;
    switch (func_800E0C10(work->gridContacts, &frame->delta, ARRAY_SIZE(work->gridContacts), NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += frame->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += frame->delta.fixed.vz.halves.integer;
            if (head[-1].delta.fixed.vx.word != 0 || frame->delta.fixed.vz.word != 0) {
                work->blocked = 1;
            }
            break;
        case 2:
            coord->coord.t[0] = work->prevPos.vx;
            coord->coord.t[1] = work->prevPos.vy;
            coord->coord.t[2] = work->prevPos.vz;
            if (head[-1].delta.fixed.vx.word != 0 || frame->delta.fixed.vz.word != 0) {
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
    normal = &frame->normal;
    for (i = 0; i < ARRAY_SIZE(work->hitContacts); i++) {
        switch ((u32)work->hitContacts[i].key.value >> 16) {
            case 2:
                if (work->hitCooldown == 0) {
                    target                 = gPlayerActorTasks[((u32)work->hitContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                    frame->delta.vector.vx = target->coord.t[0] - coord->coord.t[0];
                    frame->delta.vector.vy = target->coord.t[1] - coord->coord.t[1];
                    frame->delta.vector.vz = target->coord.t[2] - coord->coord.t[2];
                    damage                 = Gp_ComputeDamage(work->hitContacts[i].key.value,
                                                              SquareRoot0(frame->delta.vector.vx * frame->delta.vector.vx + frame->delta.vector.vy * frame->delta.vector.vy +
                                                                          frame->delta.vector.vz * frame->delta.vector.vz),
                                                              0, 0);
                    param0                 = Gp_GetIdParam0(work->hitContacts[i].key.value);
                    if ((param0 & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                    }
                    if (Gp_RollEnemyChance(ctx, work->hitContacts[i].key.value, 0) != 0) {
                        damage *= 4;
                        if ((param0 & 0xFFFF) != 5) {
                            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, NULL);
                        }
                    }
                    func_800E2C78(ctx, work->hitContacts[i].key.value, damage, 0);
                    func_800DA6E8(&ctx->node, damage, 0);
                    ctx->hp -= damage;
                    if (ctx->hp <= 0) {
                        work->action            = ACTOR_02500_ACTION_DIE;
                        work->actionStep        = ACTOR_02500_DEATH_STEP_BEGIN;
                        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        soundId                 = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4019000A;
                        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    } else {
                        if (work->flinchGuard == 0) {
                            work->action     = ACTOR_02500_ACTION_FLINCH;
                            work->actionStep = ACTOR_02500_REACTION_STEP_BEGIN;
                        }
                        work->flinchGuard = 0;
                        soundId           = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190009;
                        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    switch (param0 & 0xFFFF) {
                        case 0:
                        case 3:
                        case 5:
                        case 7:
                        case 8:
                        case 9:
                            break;
                        case 1:
                            if (work->inBuildup == 0) {
                                Gp_SetObjFlag1(ctx);
                            }
                            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            break;
                        case 2:
                            Gp_SetObjFlag2(ctx, work->hitContacts[i].key.value, 0);
                            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            break;
                        case 4:
                        case 6:
                            if (ctx->hp <= 0) {
                                work->burstStage = 1;
                            }
                            break;
                    }
                    if (lastId != work->hitContacts[i].key.value) {
                        lastId = work->hitContacts[i].key.value;
                        func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, coord, 0, &work->hitEffectArg);
                    }
                    cooldown = Gp_GetIdParam2(work->hitContacts[i].key.value);
                    if (cooldown > 0) {
                        work->hitCooldown = cooldown;
                    }
                }
                break;
            case 0:
                break;
            /* Kinds 1 and 3 push the enemy back out of the obstacle the same way. */
            case 1:
                frame->delta.vector.vx = coord->workm.t[0] - work->hitContacts[i].point.vx;
                frame->delta.vector.vy = coord->workm.t[1] - work->hitContacts[i].point.vy;
                frame->delta.vector.vz = coord->workm.t[2] - work->hitContacts[i].point.vz;
                push                   = work->hitContacts[i].distance -
                       SquareRoot0(frame->delta.vector.vx * frame->delta.vector.vx + frame->delta.vector.vy * frame->delta.vector.vy +
                                   frame->delta.vector.vz * frame->delta.vector.vz);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal(&frame->delta.vector, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, &frame->pushDirection);
                }
                break;
            case 3:
                frame->delta.vector.vx = coord->workm.t[0] - work->hitContacts[i].point.vx;
                frame->delta.vector.vy = coord->workm.t[1] - work->hitContacts[i].point.vy;
                frame->delta.vector.vz = coord->workm.t[2] - work->hitContacts[i].point.vz;
                push                   = work->hitContacts[i].distance -
                       SquareRoot0(frame->delta.vector.vx * frame->delta.vector.vx + frame->delta.vector.vy * frame->delta.vector.vy +
                                   frame->delta.vector.vz * frame->delta.vector.vz);
                push = (push <= 0) ? 0 : push;
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal(&frame->delta.vector, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, &frame->pushDirection);
                }
                break;
        }
    }
    if (bestPush > 0) {
        coord->coord.t[0] += (bestPush * frame->pushDirection.vx) >> 0xC;
        coord->coord.t[2] += (bestPush * frame->pushDirection.vz) >> 0xC;
    }
    worldCollisionClearContacts(work->hitContacts);
    if (worldCollisionFindContactIndex(work->attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        soundId                 = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190006;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
    worldCollisionClearContacts(work->attackContacts);
    if (Gp_CountRec18Hi(work->noticeContacts, 0x10000) != 0 && work->action == ACTOR_02500_ACTION_WANDER) {
        work->action     = ACTOR_02500_ACTION_CHASE;
        work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
        Gp_ArmStateF0(1);
    }
    worldCollisionClearContacts(work->noticeContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOverlapPushScratch);
}

static void Actor02500_Fn00B18(Task* actor)
{
    _Actor02500Work* work;
    GfxCoord*        coord;
    s16              timer;
    s16              moveTimer;
    s16              step;
    s32              randomAngle;
    s32              randomMoveTime;
    s32              dx;
    s32              randomIdleTime;
    s32              dz;
    s32              idleTime;
    VECTOR*          vector;
    VECTOR*          scratchEnd;

    scratchEnd                 = SCRATCH_STACK_CURSOR(VECTOR);
    vector                     = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(void) = vector;
    work                       = actor->work;
    step                       = work->actionStep;
    coord                      = actor->extra.tmd->coords;
    switch (step) {
        case ACTOR_02500_WANDER_STEP_STAND:
            work->anim  = ACTOR_02500_ANIM_STAND;
            work->speed = 0;
            timer       = (u16)work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                work->headingHome = 0;
                work->anim        = ACTOR_02500_ANIM_WALK;
                work->actionStep  = ACTOR_02500_WANDER_STEP_TURN;
                randomAngle       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState   = randomAngle;
                work->targetYaw   = ((u32)randomAngle >> 0x10) & 0xFFF;
            }
            break;
        case ACTOR_02500_WANDER_STEP_TURN:
            work->speed = 0;
            if (work->yaw == (s16)work->targetYaw) {
                work->actionStep = ACTOR_02500_WANDER_STEP_WALK;
                randomMoveTime   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomMoveTime;
                work->timer      = (((u32)randomMoveTime >> 0x10) & 0x7F) + 0x1E;
            }
            break;
        case ACTOR_02500_WANDER_STEP_WALK:
            work->speed       = (s16)Actor02500_D05B58[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
            scratchEnd[-1].vx = work->home.vx - coord->coord.t[0];
            vector->vy        = 0;
            dz                = work->home.vz - coord->coord.t[2];
            vector->vz        = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) >= 0x7D0) && (work->headingHome == 0)) {
                work->actionStep = ACTOR_02500_WANDER_STEP_HEAD_HOME;
            } else {
                if (work->blocked != 1) {
                    moveTimer   = (u16)work->timer - 1;
                    work->timer = moveTimer;
                    if (moveTimer > 0) {
                        break;
                    }
                }
                work->actionStep = ACTOR_02500_WANDER_STEP_STAND;
                randomIdleTime   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState  = randomIdleTime;
                idleTime         = ((u32)randomIdleTime >> 0x10) & 0x3F;
                work->timer      = idleTime + 0x1E;
            }
            break;
        case ACTOR_02500_WANDER_STEP_HEAD_HOME:
            scratchEnd[-1].vx = work->home.vx - coord->coord.t[0];
            vector->vy        = 0;
            vector->vz        = work->home.vz - coord->coord.t[2];
            work->targetYaw   = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vector->vz) & 0xFFF;
            work->headingHome = 1;
            work->actionStep  = ACTOR_02500_WANDER_STEP_TURN;
            break;
    }
    work->turnRate = (s16)Actor02500_D05B48[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor02500_Fn00DD8(Task* actor)
{
    _Actor02500Work* work;
    GfxCoord*        coord;
    s16              timer;
    s32              step;
    s16              diff;
    s32              frame;
    s32              absDiff;
    s16              angle;
    s32              sound;
    s32              dx;
    s32              dz;
    s32              homeDx;
    s32              homeDz;
    s32              pan;
    VECTOR*          vector;
    VECTOR*          scratchEnd;

    scratchEnd                 = SCRATCH_STACK_CURSOR(VECTOR);
    vector                     = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(void) = vector;
    work                       = actor->work;
    step                       = work->actionStep;
    coord                      = actor->extra.tmd->coords;
    switch (step) {
        case ACTOR_02500_CHASE_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_RUN;
            work->timer      = 0xF0;
            work->speed      = 0;
            work->actionStep = ACTOR_02500_CHASE_STEP_RUN;
            break;
        case ACTOR_02500_CHASE_STEP_RUN:
            work->speed       = (s16)Actor02500_D05B78[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
            scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            vector->vy        = 0;
            vector->vz        = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw   = ratan2((s16)scratchEnd[-1].vx, (s16)vector->vz) & 0xFFF;
            dx                = scratchEnd[-1].vx;
            dz                = vector->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) {
                diff        = work->targetYaw - (u16)work->yaw;
                absDiff     = diff >= 0 ? diff : -diff;
                work->speed = 0;
                if (absDiff < 0x800) {
                    angle = absDiff;
                } else if (diff > 0) {
                    angle = 0x1000 - diff;
                } else {
                    angle = diff + 0x1000;
                }
                if (angle < 0x30) {
                    work->actionStep = ACTOR_02500_CHASE_STEP_STING;
                    work->anim       = ACTOR_02500_ANIM_STING;
                }
            } else {
                timer       = (u16)work->timer - 1;
                work->timer = timer;
                if (timer <= 0) {
                    scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - work->home.vx;
                    vector->vy        = 0;
                    homeDz            = gPlayerStatus.coordMtx->t[2] - work->home.vz;
                    vector->vz        = homeDz;
                    homeDx            = scratchEnd[-1].vx;
                    if (SquareRoot0((homeDx * homeDx) + (homeDz * homeDz)) >= 0x7D1) {
                        work->actionStep = ACTOR_02500_CHASE_STEP_GIVE_UP;
                    }
                }
            }
            break;
        case ACTOR_02500_CHASE_STEP_STING:
            frame             = work->animFrame;
            work->speed       = 0;
            work->flinchGuard = 1;
            if (frame == 41) {
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else if (frame == 42) {
                sound = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190005;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (frame == 44) {
                work->flinchGuard       = 0;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (frame >= 76) {
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
    work->turnRate = (s16)Actor02500_D05B68[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void Actor02500_Fn01144(Task* actor)
{
    _Actor02500Work* work;
    GfxCoord*        coord;
    s32              sound;
    s32              pan;
    s32              pan9;
    s32              pan18;
    u32              random;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    work->idleSoundTimer--;
    if (work->idleSoundTimer <= 0) {
        random               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->idleSoundTimer = ((random >> 16) & 0x7F) + 0x1E;
        gRandomLcgState      = random;
        sound                = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190008;
        pan                  = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if (work->speed != 0) {
        work->stepSoundFrames++;
        if (work->stepSoundFrames == 9) {
            sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190001;
            pan9  = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(sound, pan9, (s8)worldCoordGetOriginAudioDepth(coord));
        } else if (work->stepSoundFrames == 18) {
            sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190002;
            pan18 = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(sound, pan18, (s8)worldCoordGetOriginAudioDepth(coord));
            work->stepSoundFrames = 0;
        }
    } else {
        work->stepSoundFrames = 0;
    }
}

static void Actor02500_Fn012F0(Task* actor)
{
    TmdObject*                obj;
    _Actor02500Work*          work;
    GfxCoord*                 coord;
    s16                       timer2;
    s16                       timer3;
    s16                       timer4;
    s16                       effectTimer;
    s32                       sound;
    s32                       dist;
    s32                       dx;
    s32                       dz;
    s32                       index;
    s32                       i;
    s32                       pan;
    u32                       random;
    ActorFaceScratch*         scratch;
    _Actor02500DustDirection* direction;

    coord   = actor->extra.tmd->coords;
    obj     = actor->extra.tmd;
    work    = actor->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    switch (work->actionStep) {
        case ACTOR_02500_AMBUSH_STEP_WAIT_NEAR:
            obj->flags                                                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            dx                                                         = gPlayerStatus.coordMtx->t[0] - work->home.vx;
            scratch->delta.vy                                          = 0;
            scratch->delta.vx                                          = dx;
            dz                                                         = gPlayerStatus.coordMtx->t[2] - work->home.vz;
            scratch->delta.vz                                          = dz;
            dist                                                       = SquareRoot0((dx * dx) + (dz * dz));
            if (dist < 0x7D0 || gSceneCombatState.actor02500EntranceReady != 0 || gSceneCombatState.expReward != 0) {
                gSceneCombatState.actor02500EntranceReady = 1;
                work->actionStep                          = ACTOR_02500_AMBUSH_STEP_DELAY;
                work->timer                               = ((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 0xA;
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_WAIT_SIGNAL:
            obj->flags                                                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (gSceneCombatState.actor02500EntranceReady != 0 || gSceneCombatState.expReward != 0) {
                work->actionStep = ACTOR_02500_AMBUSH_STEP_DELAY;
                work->timer      = ((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 0xA;
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_DELAY:
            obj->flags                                                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            timer2                                                     = (u16)work->timer - 1;
            work->timer                                                = timer2;
            if (timer2 <= 0) {
                work->actionStep = ACTOR_02500_AMBUSH_STEP_DUST;
                work->timer      = 0xA;
                work->dustTimer  = 0x14;
                sound            = (((u16)((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190003;
                pan              = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_DUST:
            timer3      = (u16)work->timer - 1;
            work->timer = timer3;
            if (timer3 > 0) {
                obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                Gp_SetLightMode(actor->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                obj->flags                               = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
                work->hitBody.flags                     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                ((Enemy*)actor->spawnArg2.pointer)->recs = work->hitContacts;
                work->anim                               = ACTOR_02500_ANIM_EMERGE;
                work->timer                              = 0;
                work->actionStep                         = ACTOR_02500_AMBUSH_STEP_EMERGE;
            }
            break;
        case ACTOR_02500_AMBUSH_STEP_EMERGE:
            timer4      = (u16)work->timer + 1;
            work->timer = timer4;
            if (timer4 < 0x10) {
                obj->flags = (u16)obj->flags | TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer >= 0x1F) {
                work->action     = ACTOR_02500_ACTION_CHASE;
                work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
                Gp_ArmStateF0(1);
            }
            break;
    }
    if (work->dustTimer != 0) {
        effectTimer     = (u16)work->dustTimer - 1;
        work->dustTimer = effectTimer;
        if (!(effectTimer & 3)) {
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            i               = 0;
            dist            = ((random >> 0x10) & 0x3F) + 0x12C;
            gRandomLcgState = random;
            index           = (((u16)work->dustTimer >> 2) ^ 1) & 1;
            for (; i < 4; i++) {
                direction       = &Actor02500_D05BE8[index + i * 2];
                scratch->rot.vx = (direction->x * dist) >> 0xC;
                scratch->rot.vy = 0;
                scratch->rot.vz = (direction->z * dist) >> 0xC;
                Gp_SpawnEff(EFFECT_DUST_PUFF, actor->extra.tmd->coords, 0x80002400, &scratch->rot);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

static void Actor02500_Fn016FC(Task* arg0)
{
    _Actor02500Work*  work;
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
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->yaw = work->targetYaw;
        goto done;
    turn:
        wrapStep = work->turnRate;
        cur      = work->yaw;
        if (diff > 0) {
            work->yaw = cur - wrapStep;
        } else {
            work->yaw = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->yaw;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

static void Actor02500_Fn0184C(Task* arg0)
{
    GameLocationKey  key;
    u32              raw1, raw2, raw3;
    u8               areaByte0;
    TmdObject*       model1;
    TmdObject*       model2;
    TmdObject*       model3;
    u32              index1;
    u32              index2;
    u32              index3;
    EffectWork*      effect1;
    EffectWork*      effect2;
    EffectWork*      effect3;
    AreaPlacement*   entry1;
    AreaPlacement*   entry2;
    AreaPlacement*   entry3;
    GameLocationKey* sessionKey1;
    GameLocationKey* sessionKey2;
    GameLocationKey* sessionKey3;

    D_80067704[0] = &_gActor02500ScorpionBurstHead;
    effect1       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
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
    D_80067704[0] = &_gActor02500ScorpionBurstPincer2;
    effect2       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
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
    D_80067704[0] = &_gActor02500ScorpionBurstPincer1;
    effect3       = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &arg0->extra.tmd->coords[1], 0x100, NULL);
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
}

static void Actor02500_Fn01AC8(Enemy* arg0, Task* arg1)
{
    _Actor02500Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        c;
    VECTOR           vec;
    s32              mode;
    s16              step;
    s16              phase;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    mode  = gSceneCombatState.actorControl;
    coord = obj->coords;
    if (mode == 1) {
        goto case1;
    }
    if (mode < 2) {
        goto common;
    }
    if (mode == 2) {
        goto case2;
    }
    goto common;
case1:
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
common:
    step = work->actionStep;
    if (step == ACTOR_02500_DEATH_STEP_COLLAPSE) {
        goto dying;
    }
    if (step >= ACTOR_02500_DEATH_STEP_DESTROY) {
        goto ge2;
    }
    if (step == ACTOR_02500_DEATH_STEP_BEGIN) {
        goto death;
    }
    return;
ge2:
    if (step == ACTOR_02500_DEATH_STEP_DESTROY) {
        goto destroy;
    }
    if (step == ACTOR_02500_DEATH_STEP_BURST) {
        goto case3;
    }
    return;
death:
    work->anim         = ACTOR_02500_ANIM_DIE;
    work->timer        = 0;
    work->deathScaleY  = ONE;
    work->savedRootMtx = coord->coord;
    arg0->recs         = NULL;
    worldTargetUnlinkNode(&arg0->node);
    worldCollisionUnlinkBody(&work->noticeBody);
    worldCollisionUnlinkBody(&work->hitBody);
    worldCollisionUnlinkBody(&work->gridBody);
    worldCollisionUnlinkBody(&work->attackBody);
    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
    Gp_ReleaseStateF0Add(arg1, 0x19);
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    if (work->burstStage == 0) {
        work->actionStep = ACTOR_02500_DEATH_STEP_COLLAPSE;
        return;
    }
    obj->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->actionStep = ACTOR_02500_DEATH_STEP_BURST;
    return;
dying:
    Actor02500_Fn02480(arg1);
    phase       = work->timer + 1;
    work->timer = phase;
    if (phase == 10) {
        obj->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->timer == 15) {
        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 2, NULL);
        Gp_SpawnEnemyFromTable(Actor02500_D05B88, 1, 0, arg0);
    }
    if (work->timer >= 0x3C) {
        obj->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->actionStep = ACTOR_02500_DEATH_STEP_DESTROY;
    }
    c      = arg1->extra.tmd->coords;
    vec.vx = c->workm.t[0];
    vec.vy = c->workm.t[1];
    vec.vz = c->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &vec, 0, 0);
    return;
destroy:
    enemyDestroy(arg0, arg1);
    return;
case3:
    if (work->burstStage == 0) {
        goto timer;
    }
    if (work->burstStage < 2) {
        goto inc;
    }
    work->burstStage = 0;
    tmdFreePrimitiveBuffer(obj);
    obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    Actor02500_Fn0184C(arg1);
    goto timer;
inc:
    work->burstStage++;
timer:
    phase       = work->timer + 1;
    work->timer = phase;
    if (phase < 0x3C) {
        return;
    }
    work->actionStep = ACTOR_02500_DEATH_STEP_DESTROY;
}

void Actor02500_Fn01E04(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02500_D00004;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor02500_Fn01E60(Enemy* arg0, Task* arg1)
{
    _Actor02500Work* work;
    TmdObject*       temp_a1;
    GfxCoord*        temp_s2;
    s32              state;
    s32              one;

    temp_a1 = arg1->extra.tmd;
    state   = gSceneCombatState.actorControl;
    work    = arg1->work;
    temp_s2 = temp_a1->coords;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    temp_a1->flags               = 0;
    arg0->node.state.parts.flags = 0;
    goto default_body;
case1:
    if (work->action == ACTOR_02500_ACTION_AMBUSH) {
        return;
    }
    Actor02500_Fn023D8(arg1);
    goto tail;
case2:
    temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    if (arg0->reactionFlags != 0) {
        Actor02500_Fn01F8C(arg1);
    }
    Actor02500_Fn00494(arg1);
    Actor02500_Fn02008(arg1);
    if (work->turnRate != 0) {
        Actor02500_Fn016FC(arg1);
    }
    Actor02500_Fn02288(arg1);
    Actor02500_Fn02318(arg1);
    temp_s2->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(temp_s2);
    Actor02500_Fn023D8(arg1);
    if (work->action == ACTOR_02500_ACTION_AMBUSH) {
        return;
    }
tail:
    Actor02500_Fn02430(arg1);
}

static void Actor02500_Fn01F8C(Task* actor)
{
    u8               flags;
    u8               remainingFlags;
    _Actor02500Work* work;
    Enemy*           ctx;

    ctx   = actor->spawnArg2.pointer;
    flags = ctx->reactionFlags;
    work  = actor->work;
    if (flags & ENEMY_REACTION_BUILDUP) {
        ctx->reactionFlags = (u8)(flags & ENEMY_REACTION_BUILDUP_CLEAR);
        work->action       = ACTOR_02500_ACTION_BUILDUP;
        work->actionStep   = ACTOR_02500_REACTION_STEP_BEGIN;
    }
    if (ctx->reactionFlags & ENEMY_REACTION_STAGGER) {
        ctx->reactionFlags = (u8)(ctx->reactionFlags & ENEMY_REACTION_STAGGER_CLEAR);
        if (work->action != ACTOR_02500_ACTION_BUILDUP) {
            work->action     = ACTOR_02500_ACTION_STAGGER;
            work->actionStep = ACTOR_02500_REACTION_STEP_BEGIN;
        }
    }
    remainingFlags = ctx->reactionFlags;
    if (remainingFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        ctx->reactionFlags = (u8)(remainingFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
    }
}

/// State handlers of the helper task `Actor02500_Fn02574` dispatches, indexed
/// by `Task::state`: setup, per-frame tick and the countdown that
/// destroys it.
static const EnemyTaskFuncTable3 Actor02500_D00050 = {
    {
        Actor02500_Fn025D0,
        Actor02500_Fn02750,
        Actor02500_Fn02874,
    },
};

static void Actor02500_Fn02008(Task* arg0)
{
    _Actor02500Work* work;

    work = arg0->work;
    switch (work->action) {
        case ACTOR_02500_ACTION_WANDER:
            Actor02500_Fn00B18(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case ACTOR_02500_ACTION_CHASE:
            Actor02500_Fn00DD8(arg0);
            Actor02500_Fn01144(arg0);
            break;
        case ACTOR_02500_ACTION_FLINCH:
            Actor02500_Fn020D0(arg0);
            break;
        case ACTOR_02500_ACTION_STAGGER:
            Actor02500_Fn02178(arg0);
            break;
        case ACTOR_02500_ACTION_BUILDUP:
            Actor02500_Fn021F8(arg0);
            break;
        case ACTOR_02500_ACTION_AMBUSH:
            Actor02500_Fn012F0(arg0);
            break;
        case ACTOR_02500_ACTION_DIE:
            arg0->state = 2;
            break;
    }
}

static void Actor02500_Fn020D0(Task* arg0)
{
    _Actor02500Work* work;

    work = arg0->work;
    switch (work->actionStep) {
        case ACTOR_02500_REACTION_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_FLINCH;
            work->speed      = 0;
            work->turnRate   = 0;
            work->actionStep = ACTOR_02500_REACTION_STEP_HOLD;
            return;
        case ACTOR_02500_REACTION_STEP_HOLD:
            if (work->animFrame >= 0x20) {
                if (work->inBuildup == 1) {
                    work->action     = ACTOR_02500_ACTION_BUILDUP;
                    work->actionStep = ACTOR_02500_REACTION_STEP_BEGIN;
                    return;
                }
                if (work->inStagger == 1) {
                    work->action     = ACTOR_02500_ACTION_STAGGER;
                    work->actionStep = ACTOR_02500_REACTION_STEP_HOLD;
                    work->timer      = 0x3C;
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

static void Actor02500_Fn02178(Task* arg0)
{
    _Actor02500Work* work;

    work = arg0->work;

    switch (work->actionStep) {
        case ACTOR_02500_REACTION_STEP_BEGIN:
            work->anim       = ACTOR_02500_ANIM_FLINCH;
            work->inStagger  = 1;
            work->speed      = 0;
            work->turnRate   = 0;
            work->timer      = 0x3C;
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

static void Actor02500_Fn021F8(Task* arg0)
{
    _Actor02500Work* work;

    work = arg0->work;

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
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->action     = ACTOR_02500_ACTION_CHASE;
                work->actionStep = ACTOR_02500_CHASE_STEP_BEGIN;
                work->inBuildup  = 0;
            }
            break;
    }
}

static void Actor02500_Fn02288(Task* arg0)
{
    _Actor02500Work* work;
    GfxCoord*        coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->speed) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->speed) >> 0xC;
}

static void Actor02500_Fn02318(Task* arg0)
{
    _Actor02500Work* work;
    s16              anim;
    s32              value;
    s32              i;
    s32              j;

    work = arg0->work;
    anim = work->anim;
    if (anim != work->playingAnim) {
        value             = Actor02500_D05BD0[anim];
        i                 = 1;
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        do {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, value);
            i++;
        } while (i < ARRAY_SIZE(work->rig.slots));
        return;
    }
    j = 1;
    work->animFrame++;
    do {
        animationTickSlot(&work->rig.anim, j);
        j++;
    } while (j < ARRAY_SIZE(work->rig.slots));
}

static void Actor02500_Fn023D8(Task* arg0)
{
    VECTOR    vec;
    GfxCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

static void Actor02500_Fn02430(Task* arg0)
{
    VECTOR3   vec;
    GfxCoord* coord;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    effectDrawGroundShadow(&vec, 0x200, 0x80);
}

static void Actor02500_Fn02480(Task* arg0)
{
    GfxCoord*          coord;
    ActorScaleScratch* head;
    ActorScaleScratch* scratch;
    _Actor02500Work*   work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->deathScaleY >= 0x201) {
        work->deathScaleY = (u16)work->deathScaleY - 0x50;
    }
    scratch->scale.vx                    = ONE;
    scratch->scale.vy                    = work->deathScaleY;
    scratch->scale.vz                    = ONE;
    coord->coord                         = work->savedRootMtx;
    scratch->matrix.rotationWords.m00M01 = ONE;
    scratch->matrix.rotationWords.m02M10 = 0;
    scratch->matrix.rotationWords.m11M12 = ONE;
    scratch->matrix.rotationWords.m20M21 = 0;
    scratch->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&scratch->matrix.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

void Actor02500_Fn02574(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02500_D00050;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static void Actor02500_Fn025D0(Enemy* ctx, Task* task)
{
    _Actor02500CorpsePoisonWork* work;
    GfxCoord*                    coord;
    WorldCollisionContact*       contacts;
    GfxCoord*                    parentCoord;
    EffectWork*                  decal;

    coord       = task->extra.tmd->coords;
    parentCoord = task->parent->extra.tmd->coords;
    work        = memCalloc(sizeof(_Actor02500CorpsePoisonWork), 0);
    if (work == NULL) {
        enemyDestroy(ctx, task);
        return;
    }
    task->work = work;
    // Stand where the corpse lies, under the view rather than the scorpion.
    coord->parent               = &gGfxViewCoord;
    coord->coord                = parentCoord->coord;
    coord->coord.t[0]           = parentCoord->coord.t[0];
    coord->coord.t[1]           = parentCoord->coord.t[1];
    coord->coord.t[2]           = parentCoord->coord.t[2];
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    decal                       = Gp_SpawnEff((EFFECT_GROUND_DECAL | EFFECT_SPAWN_UNLIMITED), coord, 0x10280, NULL);
    work->body.coord            = coord;
    contacts                    = work->contacts;
    work->decal                 = decal;
    work->body.context.contacts = contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = Gp_PackPair(Actor02500_D05B30, 1);
    work->body.radius           = 200;
    work->body.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags = (u16)(work->body.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    taskDetachFromParent(task);
    task->state = 1;
}

static void Actor02500_Fn02750(Enemy* ctx, Task* task)
{
    s32                          sound;
    GfxCoord*                    coord;
    WorldCollisionContact*       contacts;
    s32                          done;
    s32                          pan;
    u16                          timer;
    _Actor02500CorpsePoisonWork* work;

    coord = task->extra.tmd->coords;
    work  = task->work;
    done  = 0;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        contacts = work->contacts;
        // A contact of the player's category: the poison has been delivered.
        if (Gp_CountRec18Hi(contacts, 0x10000) != 0) {
            done  = 1;
            sound = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40190007;
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        worldCollisionClearContacts(contacts);
        timer       = work->timer + 1;
        work->timer = timer;
        if ((s16)timer > 240) {
            done = 1;
        }
        if (gSceneCombatState.battleRefs == 0) {
            done = 1;
        }
        if (done != 0) {
            work->releaseStep = ACTOR_02500_CORPSE_POISON_RELEASE_BEGIN;
            task->state       = 2;
        }
    }
}

static void Actor02500_Fn02874(Enemy* ctx, Task* task)
{
    _Actor02500CorpsePoisonWork* work = task->work;

    switch (work->releaseStep) {
        case ACTOR_02500_CORPSE_POISON_RELEASE_BEGIN:
            worldCollisionUnlinkBody(&work->body);
            if (work->decal != NULL) {
                // The decal's state 3 fades it out and ends its task.
                work->decal->task->state = 3;
            }
            work->timer       = 30;
            work->releaseStep = ACTOR_02500_CORPSE_POISON_RELEASE_LINGER;
            break;
        case ACTOR_02500_CORPSE_POISON_RELEASE_LINGER:
            if (--work->timer > 0) {
                break;
            }
            enemyDestroy(ctx, task);
            break;
    }
}
