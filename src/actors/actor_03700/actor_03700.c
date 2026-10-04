#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
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

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Values of `_Actor03700Work::action`: the handler the per-frame tick runs.
///
/// A handler numbers its own stages in `actionStep`, from 0 on entry. The
/// spawn picks the first action from the placement's mode: 0 to 2 are the
/// action itself, 10 to 29 `WAVE_WAIT` and 30 to 39 `SCRIPTED_ENTRY`.
enum {
    ACTOR_03700_ACTION_WANDER         = 0,  // flies between random points around `homePos` until it notices the player, then chases
    ACTOR_03700_ACTION_PERCH_DROP     = 1,  // perched until it notices the player; after a random wait it takes off by dropping and swooping forward, then chases
    ACTOR_03700_ACTION_PERCH_BACK     = 2,  // the same wait on the other perch, left by pushing off backwards
    ACTOR_03700_ACTION_CHASE          = 3,  // turns to a point above the player, then dashes at it; on touching the player it asks for the hold and attacks, or retreats when refused
    ACTOR_03700_ACTION_ATTACK         = 4,  // while it holds the player: strikes, backs off for 20 ticks and returns for the next; the sixth strike releases the player
    ACTOR_03700_ACTION_RETREAT        = 5,  // backs away from the player, hovers, then drifts back and chases; entered at step 3 by a repelling hit, which throws it back without turning and leaves it hovering longer
    ACTOR_03700_ACTION_DIE            = 6,  // out of hit points: the task is in its death state
    ACTOR_03700_ACTION_WAVE_WAIT      = 7,  // hidden with its body off until the scene's wave counter reaches the placement's mode less 9, then enters
    ACTOR_03700_ACTION_ENTER_LOW      = 8,  // rises to the point `WAVE_WAIT` picked 850 or more above it, waits 30 ticks and chases
    ACTOR_03700_ACTION_ENTER_HIGH     = 9,  // the same entrance to a point 1850 or more above
    ACTOR_03700_ACTION_SCRIPTED_ENTRY = 10, // hidden until the wave counter is 0; flies to a point below and beside it, sets the counter to 2, follows the player for 30 ticks and chases
    ACTOR_03700_ACTION_RELEASE        = 11  // repelled while holding the player: plays the player's release animation, ends the hold and retreats
};

/// Values of `_Actor03700Work::anim`: indices into the package's
/// animation-set table.
enum {
    ACTOR_03700_ANIM_FLY          = 1, // every airborne action
    ACTOR_03700_ANIM_PERCH_DROP   = 2, // waiting in `ACTION_PERCH_DROP`
    ACTOR_03700_ANIM_PERCH_BACK   = 3, // waiting in `ACTION_PERCH_BACK`
    ACTOR_03700_ANIM_TAKEOFF_DROP = 4, // 50-tick take-off of `ACTION_PERCH_DROP`
    ACTOR_03700_ANIM_TAKEOFF_BACK = 5  // 50-tick take-off of `ACTION_PERCH_BACK`
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, the collision
/// sphere with its contact table, and the state the per-frame tick flies the
/// actor with.
///
/// Each tick the running action picks `targetPos`, `turnRate` and `speed`;
/// the tick then turns the model's root toward the target, moves it along its
/// facing and 30 units toward the target's height, and adds the bob and sway.
/// Angles are 4096ths of a turn about Y. Timers count ticks.
typedef struct {
    ActorAnimRig6         rig;              // playback of the model's parts; slots 1 to 5 are driven
    MATRIX                colorMtx;         // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;         // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    body;             // sphere of radius 200 on the model's root that takes the hits and meets the room and other bodies; off while the actor is hidden
    WorldCollisionContact contacts[4];      // contacts of `body`; also the enemy's hit records
    EffectSpawnArg        hitEffectArg;     // argument record of the hit effects, hung off the model's part 1: spawned when a hit lands on the actor and when it strikes the player
    SVECTOR               prevPos;          // root position before the tick's forward movement, put back when the contact response asks for a reset instead of a push
    SVECTOR               homePos;          // root position at spawn, the centre `ACTION_WANDER` picks its targets around
    SVECTOR               targetPos;        // point the actor turns and climbs toward; `pad` unused
    s16                   targetYaw;        // heading from the root to `targetPos`, 0 to 4095
    s16                   yaw;              // heading of the model's root: the placement's at spawn, afterwards read back from the root's rotation and stepped toward `targetYaw`
    s16                   anim;             // `ACTOR_03700_ANIM_*` the actions ask for
    s16                   playingAnim;      // `anim` the slots were last started on
    s16                   animFrame;        // ticks since `playingAnim` was started
    s16                   action;           // `ACTOR_03700_ACTION_*`
    s16                   actionStep;       // stage of the running action, 0 on entry; the death state of the task counts its own stages in it (0 begin, 1 delay and effect, 2 player released, 3 linger, 4 destroy)
    s16                   speed;            // distance moved along the facing each tick; negative backs away, 0 also leaves the height alone
    s16                   turnRate;         // most `yaw` may change in a tick on its way to `targetYaw`; 0 leaves the rotation alone
    s16                   timer;            // countdown of the running action's wait or dash; the last stage of `ACTION_RETREAT`'s return counts it up to 91 instead, and the death state counts its final 60 ticks down in it
    s16                   retreatTimer;     // ticks `ACTION_RETREAT` still moves backwards; the death state counts down in it the 2 to 7 ticks before its effect
    s16                   swayAmplitude;    // peak sideways step of the sway, drawn anew at the start of each cycle: the action's base plus 0 to 63
    s16                   swayPhase;        // tick of the 15-tick sway cycle, 0 to 14
    s16                   bobPhase;         // tick of the bob cycle, 0 to the action's period of 14 or 21
    s16                   flightSoundTimer; // ticks since the flight sound was queued; the airborne actions replay it on every sixteenth
    s16                   holdingPlayer;    // 1 from the player accepting the hold until its release animation has ended (0 otherwise)
    s16                   touchingPlayer;   // 1 on a tick `contacts` held a player character's body (0 otherwise)
    s16                   waveDirector;     // 0 for most actors; 1 on the one that advances the scene's wave counter as enemies fall; 2 once that one has died and seen the last enemy go, which lets its task end
    s16                   deathEffect;      // effect the death state spawns, chosen by the weapon of the last hit (0 bursts the body, leaving a wing; 1 an additive puff; 2 two hit puffs)
    s16                   dashDone;         // 1 when a dash of `ACTION_CHASE` has run its length; the first tick of the next turn then stands still, and clears it
    s16                   attackCount;      // strikes made on the held player; cleared by the sixth, which ends the hold, and at the start of `ACTION_RETREAT`
} _Actor03700Work;
STATIC_ASSERT_SIZEOF(_Actor03700Work, 0x270);

extern u16 Actor03700_D07F7C[];

/// Halfword tables `Actor03700_Fn018C8` indexes by a 4-bit LCG draw:
/// the countdowns of `ACTION_RETREAT`, seeded into `retreatTimer` and `timer`.
extern u16 Actor03700_D07F3C[];
extern u16 Actor03700_D07F5C[];

/// Pair `Actor03700_Fn01550` packs with `Gp_PackPair` for message 0x3F9.
extern DamageAttack Actor03700_D07F08;

/// Halfword table `Actor03700_Fn01550` indexes by a 4-bit LCG draw.
extern s16 Actor03700_D07F1C[];

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 Actor03700_D07F98[];

/// Halfword wave table `Actor03700_Fn03320` indexes by `swayPhase`.
extern s16 Actor03700_D07FD4[];

/// One segment of the path a perched actor leaves its perch along.
///
/// A take-off table lists its segments in frame order, the last one ending on
/// the take-off animation's final tick. Each tick of the take-off, the first
/// segment whose `lastFrame` is not below `_Actor03700Work::animFrame` moves
/// the model's root: `deltaY / frameCount` in height and `advance / frameCount`
/// along `_Actor03700Work::yaw`. Both quotients truncate, so a segment can
/// fall a few units short of its totals.
typedef struct {
    s16 lastFrame;  // last tick of the take-off animation the segment covers
    s16 frameCount; // ticks the segment lasts: `lastFrame` less the previous segment's, or `lastFrame` + 1 for the first
    s16 deltaY;     // height the root changes by over the whole segment; positive is down
    s16 advance;    // distance the root covers along its facing over the whole segment; negative moves it backwards
} _Actor03700TakeoffSegment;
STATIC_ASSERT_SIZEOF(_Actor03700TakeoffSegment, 0x8);

extern _Actor03700TakeoffSegment Actor03700_D07FF4[];
extern _Actor03700TakeoffSegment Actor03700_D0802C[];

/// The enemy's parameters. The spawn stores them in `Enemy::param` and
/// seeds hit points from `hpMax`, which retail addresses as its own label.
extern EnemyParams Actor03700_D07F0C;

/// Animation-set table bound by `animationInitContext`, and the task's `field_24` table.
extern AnimationSet*    Actor03700_D080E4[6];
extern TaskMessageEntry Actor03700_D08108[2];

/// Animation-set table handed to the player as the 0x3FF payload's `source.sets`.
extern AnimationSet* Actor03700_D080FC[];

/// Halfword table indexed by the low 7 bits of a hit id; 3 cancels the damage.
extern s16 Actor03700_D08074[];

/// Scratch-stack block of the entrance actions, which fly the model's root in
/// a straight line to `_Actor03700Work::targetPos`.
///
/// `ACTION_ENTER_LOW`, `ACTION_ENTER_HIGH` and `ACTION_SCRIPTED_ENTRY` reserve
/// it on every tick and fill it only on the ticks of the flight. Nothing in
/// it carries over to the next tick. `ACTION_ENTER_LOW` and `ACTION_ENTER_HIGH`
/// release it before returning; `ACTION_SCRIPTED_ENTRY` leaves it reserved.
typedef struct {
    VECTOR  toTarget;  // `targetPos` minus the root's position, world units; `pad` is never written
    SVECTOR direction; // `toTarget` normalised, 4096 = 1.0; the root moves a fixed fraction of it each tick
} _Actor03700EntryScratch;
STATIC_ASSERT_SIZEOF(_Actor03700EntryScratch, 0x18);

/* `D_80067704` selects the model stream the next `Gp_SpawnEff` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

/* The two model streams the death effect picks between, in this overlay's data. */
static TmdSource _gActor03700BatBurstWingRight;
static TmdSource _gActor03700BatBurstWingLeft;

static void Actor03700_Fn000A4(Enemy* arg0, Task* task);
static void Actor03700_Fn0042C(Task* task, TmdObject* arg1, s32 arg2);
static s32  Actor03700_Fn008D0(Task* task);
static void Actor03700_Fn00ABC(Task* task);
static void Actor03700_Fn00D5C(Task* task);
static void Actor03700_Fn00F88(Task* task);
static void Actor03700_Fn011B4(Task* task);
static void Actor03700_Fn01550(Task* task);
static void Actor03700_Fn018C8(Task* task);
static void Actor03700_Fn01C94(Task* task);
static s32  Actor03700_Fn01DFC(Task* task);
static void Actor03700_Fn01F48(Task* task);
static void Actor03700_Fn020D4(Enemy* enemy, Task* task);
static void Actor03700_Fn025C8(Task* task);
static void Actor03700_Fn027DC(Task* task);
static void Actor03700_Fn029C0(Task* task);
static void Actor03700_Fn03004(Enemy* enemy, Task* task);
static s32  Actor03700_Fn03130(Task* task);
static void Actor03700_Fn0321C(Task* task);
static void Actor03700_Fn032BC(Task* task, s32 arg1, s32 arg2);
static void Actor03700_Fn03320(Task* task, s32 arg1);
static void Actor03700_Fn033F0(Task* task);
static void Actor03700_Fn034A0(Task* task);
static void Actor03700_Fn0355C(Task* task);

static void Actor03700_Fn02FA8(Task*);

s32 Actor03700_Fn034F8(Task*, s32, s32, s32);

static TmdBone _gActor03700BatBodySkeleton[6] = {
#include "assets/bat_body_skeleton.inc"
};

static u32 _gActor03700BatBodyPartVerts[6] = {
#include "assets/bat_body_partVerts.inc"
};

static SVECTOR _gActor03700BatBodyVerts[74] = {
#include "assets/bat_body_verts.inc"
};

static SVECTOR _gActor03700BatBodyNormals[61] = {
#include "assets/bat_body_normals.inc"
};

static u32 _gActor03700BatBodyStream[424] = {
#include "assets/bat_body_stream.inc"
};

static TmdSource _gActor03700BatBody = {
    0,
    2184,
    480,
    6,
    _gActor03700BatBodyPartVerts,
    _gActor03700BatBodyVerts,
    _gActor03700BatBodyNormals,
    _gActor03700BatBodySkeleton,
    _gActor03700BatBodyStream,
};

static TmdBone _gActor03700BatBurstWingRightSkeleton[1] = {
#include "assets/bat_burst_wing_right_skeleton.inc"
};

static u32 _gActor03700BatBurstWingRightPartVerts[1] = {
#include "assets/bat_burst_wing_right_partVerts.inc"
};

static SVECTOR _gActor03700BatBurstWingRightVerts[16] = {
#include "assets/bat_burst_wing_right_verts.inc"
};

static SVECTOR _gActor03700BatBurstWingRightNormals[11] = {
#include "assets/bat_burst_wing_right_normals.inc"
};

static u32 _gActor03700BatBurstWingRightStream[54] = {
#include "assets/bat_burst_wing_right_stream.inc"
};

static TmdSource _gActor03700BatBurstWingRight = {
    0,
    320,
    0,
    1,
    _gActor03700BatBurstWingRightPartVerts,
    _gActor03700BatBurstWingRightVerts,
    _gActor03700BatBurstWingRightNormals,
    _gActor03700BatBurstWingRightSkeleton,
    _gActor03700BatBurstWingRightStream,
};

static TmdBone _gActor03700BatBurstWingLeftSkeleton[1] = {
#include "assets/bat_burst_wing_left_skeleton.inc"
};

static u32 _gActor03700BatBurstWingLeftPartVerts[1] = {
#include "assets/bat_burst_wing_left_partVerts.inc"
};

static SVECTOR _gActor03700BatBurstWingLeftVerts[16] = {
#include "assets/bat_burst_wing_left_verts.inc"
};

static SVECTOR _gActor03700BatBurstWingLeftNormals[12] = {
#include "assets/bat_burst_wing_left_normals.inc"
};

static u32 _gActor03700BatBurstWingLeftStream[54] = {
#include "assets/bat_burst_wing_left_stream.inc"
};

static TmdSource _gActor03700BatBurstWingLeft = {
    0,
    320,
    0,
    1,
    _gActor03700BatBurstWingLeftPartVerts,
    _gActor03700BatBurstWingLeftVerts,
    _gActor03700BatBurstWingLeftNormals,
    _gActor03700BatBurstWingLeftSkeleton,
    _gActor03700BatBurstWingLeftStream,
};

static AnimationPackedPose _gActor03700Actor103700Animation047C4Bank1[8] = {
#include "assets/actor_103700_animation_047C4_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation047C4Bank4[28] = {
#include "assets/actor_103700_animation_047C4_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation047C4Records[49] = {
#include "assets/actor_103700_animation_047C4_records.inc"
};

static u16 _gActor03700Actor103700Animation047C4Indices[6] = {
#include "assets/actor_103700_animation_047C4_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation047C4 = {
    _gActor03700Actor103700Animation047C4Records,
    _gActor03700Actor103700Animation047C4Indices,
    { NULL, _gActor03700Actor103700Animation047C4Bank1, NULL, NULL, _gActor03700Actor103700Animation047C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation04AC0Bank1[19] = {
#include "assets/actor_103700_animation_04AC0_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation04AC0Bank4[18] = {
#include "assets/actor_103700_animation_04AC0_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation04AC0Records[103] = {
#include "assets/actor_103700_animation_04AC0_records.inc"
};

static u16 _gActor03700Actor103700Animation04AC0Indices[6] = {
#include "assets/actor_103700_animation_04AC0_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation04AC0 = {
    _gActor03700Actor103700Animation04AC0Records,
    _gActor03700Actor103700Animation04AC0Indices,
    { NULL, _gActor03700Actor103700Animation04AC0Bank1, NULL, NULL, _gActor03700Actor103700Animation04AC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation04C58Bank1[7] = {
#include "assets/actor_103700_animation_04C58_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation04C58Bank4[24] = {
#include "assets/actor_103700_animation_04C58_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation04C58Records[44] = {
#include "assets/actor_103700_animation_04C58_records.inc"
};

static u16 _gActor03700Actor103700Animation04C58Indices[6] = {
#include "assets/actor_103700_animation_04C58_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation04C58 = {
    _gActor03700Actor103700Animation04C58Records,
    _gActor03700Actor103700Animation04C58Indices,
    { NULL, _gActor03700Actor103700Animation04C58Bank1, NULL, NULL, _gActor03700Actor103700Animation04C58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation05448Bank1[49] = {
#include "assets/actor_103700_animation_05448_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation05448Bank4[126] = {
#include "assets/actor_103700_animation_05448_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation05448Records[222] = {
#include "assets/actor_103700_animation_05448_records.inc"
};

static u16 _gActor03700Actor103700Animation05448Indices[6] = {
#include "assets/actor_103700_animation_05448_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation05448 = {
    _gActor03700Actor103700Animation05448Records,
    _gActor03700Actor103700Animation05448Indices,
    { NULL, _gActor03700Actor103700Animation05448Bank1, NULL, NULL, _gActor03700Actor103700Animation05448Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation05CE8Bank1[61] = {
#include "assets/actor_103700_animation_05CE8_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation05CE8Bank4[132] = {
#include "assets/actor_103700_animation_05CE8_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation05CE8Records[224] = {
#include "assets/actor_103700_animation_05CE8_records.inc"
};

static u16 _gActor03700Actor103700Animation05CE8Indices[6] = {
#include "assets/actor_103700_animation_05CE8_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation05CE8 = {
    _gActor03700Actor103700Animation05CE8Records,
    _gActor03700Actor103700Animation05CE8Indices,
    { NULL, _gActor03700Actor103700Animation05CE8Bank1, NULL, NULL, _gActor03700Actor103700Animation05CE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation06B4CBank1[31] = {
#include "assets/actor_103700_animation_06B4C_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation06B4CBank4[344] = {
#include "assets/actor_103700_animation_06B4C_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation06B4CRecords[464] = {
#include "assets/actor_103700_animation_06B4C_records.inc"
};

static u16 _gActor03700Actor103700Animation06B4CIndices[20] = {
#include "assets/actor_103700_animation_06B4C_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation06B4C = {
    _gActor03700Actor103700Animation06B4CRecords,
    _gActor03700Actor103700Animation06B4CIndices,
    { NULL, _gActor03700Actor103700Animation06B4CBank1, NULL, NULL, _gActor03700Actor103700Animation06B4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor03700Actor103700Animation07EE0Bank1[36] = {
#include "assets/actor_103700_animation_07EE0_bank1.inc"
};

static AnimationPackedRotation _gActor03700Actor103700Animation07EE0Bank4[522] = {
#include "assets/actor_103700_animation_07EE0_bank4.inc"
};

static AnimationRecord _gActor03700Actor103700Animation07EE0Records[603] = {
#include "assets/actor_103700_animation_07EE0_records.inc"
};

static u16 _gActor03700Actor103700Animation07EE0Indices[20] = {
#include "assets/actor_103700_animation_07EE0_indices.inc"
};

static AnimationSet _gActor03700Actor103700Animation07EE0 = {
    _gActor03700Actor103700Animation07EE0Records,
    _gActor03700Actor103700Animation07EE0Indices,
    { NULL, _gActor03700Actor103700Animation07EE0Bank1, NULL, NULL, _gActor03700Actor103700Animation07EE0Bank4, NULL, NULL, NULL },
};

DamageAttack Actor03700_D07F08 = { 3, 7 };

EnemyParams Actor03700_D07F0C = { &Actor03700_D07F08, 1, 5, 18, 1, 100, 0, 100, 99 };

s16 Actor03700_D07F1C[16] = {
    30,
    32,
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
};

u16 Actor03700_D07F3C[16] = {
    10,
    12,
    14,
    16,
    18,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
};

u16 Actor03700_D07F5C[16] = {
    245,
    250,
    255,
    260,
    265,
    270,
    275,
    280,
    285,
    290,
    295,
    300,
    295,
    300,
    305,
    310,
};

u16 Actor03700_D07F7C[8] = {
    16,
    20,
    24,
    28,
    36,
    44,
    48,
    48,
};

TaskDesc Actor03700_D07F8C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor03700_Fn02FA8, { .model = &_gActor03700BatBody } };

s16 Actor03700_D07F98[30] = {
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
    20,
    38,
    48,
    50,
    44,
    30,
    10,
    -10,
    -30,
    -44,
    -50,
    -48,
    -38,
    -20,
};

s16 Actor03700_D07FD4[16] = {
    4096,
    3742,
    2741,
    1267,
    -426,
    -2046,
    -3312,
    -4006,
    -4007,
    -3316,
    -2052,
    -433,
    1261,
    2737,
    3739,
    0,
};

_Actor03700TakeoffSegment Actor03700_D07FF4[7] = {
    { 20, 21, 0, 0 },
    { 23, 3, 45, 5 },
    { 26, 3, 170, 47 },
    { 30, 4, 105, 189 },
    { 35, 5, -60, 70 },
    { 40, 5, -10, 50 },
    { 50, 10, 0, 0 },
};

_Actor03700TakeoffSegment Actor03700_D0802C[9] = {
    { 10, 11, 0, 0 },
    { 17, 7, 0, -20 },
    { 22, 5, 4, -276 },
    { 27, 5, 88, -97 },
    { 32, 5, -48, -47 },
    { 37, 5, 16, -26 },
    { 40, 3, -42, -7 },
    { 45, 5, -19, -5 },
    { 50, 5, 0, 0 },
};

s16 Actor03700_D08074[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
};

AnimationSet* Actor03700_D080E4[6] = {
    NULL,
    &_gActor03700Actor103700Animation047C4,
    &_gActor03700Actor103700Animation04AC0,
    &_gActor03700Actor103700Animation04C58,
    &_gActor03700Actor103700Animation05448,
    &_gActor03700Actor103700Animation05CE8,
};

AnimationSet* Actor03700_D080FC[3] = {
    NULL,
    &_gActor03700Actor103700Animation06B4C,
    &_gActor03700Actor103700Animation07EE0,
};

TaskMessageEntry Actor03700_D08108[2] = {
    { 2014, Actor03700_Fn034F8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline void Actor03700_BobInline(Task* task, s32 arg1, s32 arg2);
static inline void Actor03700_SwayInline(Task* task, s32 arg1);
static inline void _actor03700UpdateColor(Task* task);
static inline void _actor03700SpawnRemains(Task* task);

/// The bob step of `Actor03700_Fn032BC`, which `Actor03700_Fn029C0` carries
/// expanded in place rather than as a call.
static inline void Actor03700_BobInline(Task* task, s32 arg1, s32 arg2)
{
    _Actor03700Work* work;
    GfxCoord*        coord;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (arg2 < ++work->bobPhase) {
        work->bobPhase = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(arg1 * 15) + work->bobPhase];
}

/// The sway step of `Actor03700_Fn03320`, expanded in place the same way.
static inline void Actor03700_SwayInline(Task* task, s32 arg1)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    s32              amp;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (++work->swayPhase >= 15) {
        work->swayPhase     = 0;
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->swayAmplitude = arg1 + ((gRandomLcgState >> 16) & 0x3F);
    }
    amp                = (work->swayAmplitude * Actor03700_D07FD4[work->swayPhase] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

/// The enemy's state handlers, run by `Actor03700_Fn02FA8` for the task's
/// state: spawn, per-frame tick and death.
static const EnemyTaskFuncTable3 Actor03700_D00004 = {
    { Actor03700_Fn000A4, Actor03700_Fn03004, Actor03700_Fn020D4 },
};

/// Spawn handler. Allocates the work block onto the task, points the
/// model at its light/colour matrices and links the enemy node. The placement's
/// mode (`AreaPlacement::mode`) picks the first action: tens digit 0 allocates
/// the model buffers and takes the units digit (0..2) as the action, nudging the
/// root coordinate for the two perches; 1 and 2 set `TMD_OBJECT_SKIP_AUTO_BUFFER`
/// and `ACTION_WAVE_WAIT`, 3 the same flag and `ACTION_SCRIPTED_ENTRY`. Mode 10
/// also makes the actor the wave director.
/// The animation slots then get a shared random rate offset, and the collision body
/// is linked with its four contacts before the task moves to state 1.
static void Actor03700_Fn000A4(Enemy* arg0, Task* task)
{
    TmdObject*       obj;
    GfxCoord*        coord;
    _Actor03700Work* work;
    s32              kind;
    s32              i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor03700Work), 0);
    if (work == NULL) {
        enemyDestroy(arg0, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->param                   = &Actor03700_D07F0C;
    arg0->coord                   = coord;
    arg0->node.state.parts.flags  = 0;
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vy              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->recs                    = work->contacts;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = 1;
    work->yaw                     = arg0->place->yaw;
    kind                          = arg0->place->mode;
    switch (kind / 10) {
        case 0:
            Tmd_AllocBuffers(obj);
            if (kind < 3) {
                work->action = kind;
            } else {
                work->action = ACTOR_03700_ACTION_WANDER;
            }
            work->anim = kind < 3 ? kind + 1 : ACTOR_03700_ANIM_FLY;
            switch (work->action) {
                case ACTOR_03700_ACTION_PERCH_DROP:
                    work->anim         = ACTOR_03700_ANIM_PERCH_DROP;
                    coord->coord.t[1] += 0x50;
                    break;
                case ACTOR_03700_ACTION_PERCH_BACK:
                    work->anim         = ACTOR_03700_ANIM_PERCH_BACK;
                    coord->coord.t[2] -= 0x55;
                    break;
                case ACTOR_03700_ACTION_WANDER:
                    work->anim = ACTOR_03700_ANIM_FLY;
                    break;
            }
            break;
        case 1:
        case 2:
            obj->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->action = ACTOR_03700_ACTION_WAVE_WAIT;
            work->anim   = ACTOR_03700_ANIM_FLY;
            if (kind == 10) {
                work->waveDirector = 1;
            }
            break;
        case 3:
            obj->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->action = ACTOR_03700_ACTION_SCRIPTED_ENTRY;
            work->anim   = ACTOR_03700_ANIM_FLY;
            break;
    }
    arg0->hp          = Actor03700_D07F0C.hpMax;
    work->playingAnim = work->anim;
    task->msgTable    = Actor03700_D08108;
    animationInitContext(&work->rig.anim, Actor03700_D080E4, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, work->anim);
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    kind            = (gRandomLcgState >> 16) & 3;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        work->rig.slots[i].rate += kind;
    }
    (Gp_IncStateF0Ref)(0);
    work->homePos.vx            = coord->coord.t[0];
    work->homePos.vy            = coord->coord.t[1];
    work->homePos.vz            = coord->coord.t[2];
    work->body.radius           = 0xC8;
    work->body.coord            = coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = 0x30025;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state       = 1;
}

/// Collision step. Applies the `func_800E0C10` push-back to the root coordinate
/// (a delta for 1, an absolute reset to `prevPos` for 2), then walks the four
/// contacts: kind 1, a player's body, sets `touchingPlayer` and pushes the actor
/// out along the deepest overlap; kind 2 is a hit from a player slot, which
/// deals its damage and spawns the hit effect. Any damage kills, recording
/// `deathEffect`; a hit that only repels sends an actor off its perch into
/// `ACTION_RETREAT`, or into `ACTION_RELEASE` while it holds the player.
static void Actor03700_Fn0042C(Task* task, TmdObject* arg1, s32 arg2)
{
    ActorContactOverlapPushScratch* scratch;
    GfxCoord*                       coord;
    GfxCoord*                       src;
    _Actor03700Work*                work;
    s32                             push;
    s32                             reach;
    s32                             res;
    s32                             i;
    s32                             z;
    s32                             val;
    s32                             ex;
    s32                             ey;
    s32                             ez;
    s32                             broke;
    u32                             id;
    u32                             damage;

    push    = 0;
    broke   = 0;
    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactOverlapPushScratch);
    coord   = task->extra.tmd->coords;
    res     = func_800E0C10(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
    coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
    z                  = coord->coord.t[2] + scratch->delta.fixed.vz.halves.integer;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->prevPos.vx;
    coord->coord.t[1] = work->prevPos.vy;
    z                 = work->prevPos.vz;
move_z:
    coord->coord.t[2] = z;
move_done:
    i                    = 0;
    work->touchingPlayer = 0;
    do {
        id = work->contacts[i].key.value;
        switch (id >> 16) {
            case 0:
                break;
            case 1:
                work->touchingPlayer     = 1;
                scratch->delta.vector.vx = coord->workm.t[0] - work->contacts[i].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->contacts[i].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->contacts[i].point.vz;
                reach                    = work->contacts[i].distance - SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz);
                val                      = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->pushDirection);
                }
                break;
            case 2:
                src                      = gPlayerActorTasks[(id >> 7) & 1]->extra.tmd->coords;
                ex                       = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vector.vx = ex;
                ey                       = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vector.vy = ey;
                ez                       = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vector.vz = ez;
                damage                   = Gp_ComputeDamage(work->contacts[i].key.value, SquareRoot0(ex * ex + ey * ey + ez * ez), 0, 0);
                id                       = work->contacts[i].key.value;
                if (id & 0x8000) {
                    if (Actor03700_D08074[id & 0x7F] == 3) {
                        broke  = 1;
                        damage = 0;
                    } else {
                        broke = Gp_GetIdParam1(id) & 0xFFFF;
                        if ((u32)(broke - 0xC) < 2) {
                            func_800FDB18(broke, coord, NULL, &work->hitEffectArg);
                        }
                        work->deathEffect = Actor03700_D08074[work->contacts[i].key.value & 0x7F];
                        broke             = 0;
                    }
                } else {
                    work->deathEffect = (Gp_GetIdParam1(id) & 0xFFFF) == 7;
                }
                func_800DA6E8(&((Enemy*)task->spawnArg2.pointer)->node, damage, 0);
                func_800E2C78(task->spawnArg2.pointer, work->contacts[i].key.value, damage, 0);
                if ((s32)damage > 0) {
                    ((Enemy*)task->spawnArg2.pointer)->hp = 0;
                    work->action                          = ACTOR_03700_ACTION_DIE;
                    work->actionStep                      = 0;
                    task->state                           = 2;
                } else if (((Gp_GetIdParam0(work->contacts[i].key.value) & 0xFFFF) == 8 || broke == 1) &&
                           (u16)(work->action - ACTOR_03700_ACTION_PERCH_DROP) >= 2) { // not on either perch
                    if (work->holdingPlayer == 0) {
                        work->action     = ACTOR_03700_ACTION_RETREAT;
                        work->actionStep = 3;
                    } else {
                        work->action     = ACTOR_03700_ACTION_RELEASE;
                        work->actionStep = 0;
                    }
                }
                break;
        }
    } while (++i < ARRAY_SIZE(work->contacts));
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->pushDirection.vx) >> 12;
        coord->coord.t[2] += (push * scratch->pushDirection.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactOverlapPushScratch);
}

/// The tick's action dispatcher: runs the handler for the work block's
/// `action`. The airborne actions - `WANDER`, `CHASE`, `ATTACK`, `RETREAT`, the
/// two entrances, and `SCRIPTED_ENTRY` once `actionStep` is set - also count
/// `flightSoundTimer` up and replay the flight sound from the placement's
/// sound bank every 16 frames. `DIE` clears `actionStep`, `WAVE_WAIT` releases the
/// contacts and runs the wait in `Actor03700_Fn025C8`; both report 1,
/// which tells the caller to skip this frame's movement.
static s32 Actor03700_Fn008D0(Task* task)
{
    s16              state;
    GfxCoord*        object;
    s32              ret;
    _Actor03700Work* soundWork;
    _Actor03700Work* work;

    work  = task->work;
    state = work->action;
    ret   = 0;
    /* Each sound block needs separate locals to preserve the call scheduling. */
    switch (state) {
        case ACTOR_03700_ACTION_WANDER:
            Actor03700_Fn00ABC(task);
            soundWork = task->work;
            object    = task->extra.tmd->coords;
            if (++soundWork->flightSoundTimer < 0x10) {
                return ret;
            }
            soundWork->flightSoundTimer = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case ACTOR_03700_ACTION_PERCH_DROP:
            Actor03700_Fn00D5C(task);
            return ret;
        case ACTOR_03700_ACTION_PERCH_BACK:
            Actor03700_Fn00F88(task);
            return ret;
        case ACTOR_03700_ACTION_CHASE:
            Actor03700_Fn011B4(task);
            soundWork = task->work;
            object    = task->extra.tmd->coords;
            if (++soundWork->flightSoundTimer < 0x10) {
                return ret;
            }
            soundWork->flightSoundTimer = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case ACTOR_03700_ACTION_ATTACK:
            Actor03700_Fn01550(task);
            soundWork = task->work;
            object    = task->extra.tmd->coords;
            if (++soundWork->flightSoundTimer < 0x10) {
                return ret;
            }
            soundWork->flightSoundTimer = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case ACTOR_03700_ACTION_RETREAT:
            Actor03700_Fn018C8(task);
            soundWork = task->work;
            object    = task->extra.tmd->coords;
            if (++soundWork->flightSoundTimer < 0x10) {
                return ret;
            }
            soundWork->flightSoundTimer = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case ACTOR_03700_ACTION_DIE:
            work->actionStep = 0;
            ret              = 1;
            break;
        case ACTOR_03700_ACTION_WAVE_WAIT:
            Gp_ClearRec18Occupied(work->contacts);
            if (work->waveDirector != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn025C8(task);
            ret = 1;
            break;
        case ACTOR_03700_ACTION_ENTER_LOW:
            if (work->waveDirector != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn027DC(task);
            soundWork = task->work;
            object    = task->extra.tmd->coords;
            if (++soundWork->flightSoundTimer < 0x10) {
                return ret;
            }
            soundWork->flightSoundTimer = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case ACTOR_03700_ACTION_ENTER_HIGH:
            Actor03700_Fn027DC(task);
            soundWork = task->work;
            object    = task->extra.tmd->coords;
            if (++soundWork->flightSoundTimer < 0x10) {
                return ret;
            }
            soundWork->flightSoundTimer = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)worldCoordGetOriginAudioPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
            }
            return ret;
        case ACTOR_03700_ACTION_SCRIPTED_ENTRY:
            Actor03700_Fn029C0(task);
            if (work->actionStep != 0) {
                soundWork = task->work;
                object    = task->extra.tmd->coords;
                if (++soundWork->flightSoundTimer < 0x10) {
                    return ret;
                }
                soundWork->flightSoundTimer = 0;
                {
                    u32 soundId;
                    s32 pan;
                    soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
                    soundId >>= 0xC;
                    soundId <<= 8;
                    soundId  |= 0x40250005;
                    pan       = (s8)worldCoordGetOriginAudioPan(object);
                    SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(object));
                }
            }
            return ret;
        case ACTOR_03700_ACTION_RELEASE:
            Actor03700_Fn01C94(task);
            break;
    }
    return ret;
}

static void Actor03700_Fn00ABC(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    s16              angle;
    s32              dist;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    work                          = task->work;
    coord                         = task->extra.tmd->coords;

    switch (work->actionStep) {
        case 0:
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->speed        = ((gRandomLcgState >> 16) & 0xF) + 20;
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            angle              = (gRandomLcgState >> 16) & 0xFFF;
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            dist               = (gRandomLcgState >> 16) & 0x1FF;
            work->targetPos.vx = work->homePos.vx + ((dist * rsin(angle)) >> 12);
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = work->homePos.vy + ((gRandomLcgState >> 16) & 0x1FF);
            work->targetPos.vz = work->homePos.vz + ((dist * rcos(angle)) >> 12);
            vec->vx            = work->targetPos.vx - coord->coord.t[0];
            vec->vy            = 0;
            vec->vz            = work->targetPos.vz - coord->coord.t[2];
            work->targetYaw    = ratan2(vec->vx, vec->vz) & 0xFFF;
            work->actionStep   = 1;
            break;
        case 1:
            vec->vx = work->targetPos.vx - coord->coord.t[0];
            vec->vz = work->targetPos.vz - coord->coord.t[2];
            if ((s16)SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 120) {
                work->actionStep = 0;
            }
            break;
    }
    Actor03700_Fn032BC(task, 0, 14);
    Actor03700_Fn03320(task, 20);
    if (Actor03700_Fn01DFC(task) != 0) {
        work->action                       = ACTOR_03700_ACTION_CHASE;
        work->actionStep                   = 0;
        gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static void Actor03700_Fn00D5C(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    s32              dist;
    s32              i;

    work  = task->work;
    coord = task->extra.tmd->coords;

    switch (work->actionStep) {
        case 0:
            if (Actor03700_Fn01DFC(task) != 0) {
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
                gRandomLcgState                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep                   = 1;
                work->timer                        = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if (--work->timer <= 0) {
                work->actionStep = 2;
                work->timer      = 0;
                work->anim       = ACTOR_03700_ANIM_TAKEOFF_DROP;
            }
            break;
        case 2:
            for (i = 0; i < 7; i++) {
                if (Actor03700_D07FF4[i].lastFrame >= work->animFrame) {
                    coord->coord.t[1] += Actor03700_D07FF4[i].deltaY / Actor03700_D07FF4[i].frameCount;
                    dist               = Actor03700_D07FF4[i].advance / Actor03700_D07FF4[i].frameCount;
                    coord->coord.t[0] += (rsin(work->yaw) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->yaw) * dist) >> 12;
                    break;
                }
            }
            if (work->animFrame >= 50) {
                work->anim                         = ACTOR_03700_ANIM_FLY;
                work->action                       = ACTOR_03700_ACTION_CHASE;
                work->actionStep                   = 0;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
}

static void Actor03700_Fn00F88(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    s32              dist;
    s32              i;

    work  = task->work;
    coord = task->extra.tmd->coords;

    switch (work->actionStep) {
        case 0:
            if (Actor03700_Fn01DFC(task) != 0) {
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
                gRandomLcgState                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep                   = 1;
                work->timer                        = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if (--work->timer <= 0) {
                work->actionStep = 2;
                work->timer      = 0;
                work->anim       = ACTOR_03700_ANIM_TAKEOFF_BACK;
            }
            break;
        case 2:
            for (i = 0; i < 9; i++) {
                if (Actor03700_D0802C[i].lastFrame >= work->animFrame) {
                    coord->coord.t[1] += Actor03700_D0802C[i].deltaY / Actor03700_D0802C[i].frameCount;
                    dist               = Actor03700_D0802C[i].advance / Actor03700_D0802C[i].frameCount;
                    coord->coord.t[0] += (rsin(work->yaw) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->yaw) * dist) >> 12;
                    break;
                }
            }
            if (work->animFrame >= 50) {
                work->anim                         = ACTOR_03700_ANIM_FLY;
                work->action                       = ACTOR_03700_ACTION_CHASE;
                work->actionStep                   = 0;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
}

static void Actor03700_Fn011B4(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    void*            head;
    SVECTOR*         vec;
    s32              i;
    s32              sound;
    s8               slot;

    coord                      = task->extra.tmd->coords;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(SVECTOR);
    work                       = task->work;
    vec                        = SCRATCH_STACK_CURSOR(void);

    switch (work->actionStep) {
        case 0:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            if (work->dashDone == 0) {
                work->speed = 5;
            } else {
                work->speed    = 0;
                work->dashDone = 0;
            }
            work->turnRate = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            Actor03700_Fn03320(task, 20);
            if (work->yaw == work->targetYaw) {
                work->actionStep = 1;
                work->speed      = Actor03700_D07F1C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                coord            = task->extra.tmd->coords;
                vec->vx          = work->targetPos.vx - coord->coord.t[0];
                vec->vy          = work->targetPos.vy - coord->coord.t[1];
                vec->vz          = work->targetPos.vz - coord->coord.t[2];
                work->timer      = SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz) / work->speed;
                slot             = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 17;
                for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                    work->rig.slots[i].rate = slot;
                }
                coord = task->extra.tmd->coords;
                sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250002;
                SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 1:
            Actor03700_Fn03320(task, 40);
            if (--work->timer <= 0) {
                work->actionStep = 0;
                work->timer      = 0;
                work->dashDone   = 1;
            }
            if (work->touchingPlayer != 0) {
                if (Actor03700_Fn03130(task) == 0) {
                    func_800FDB18(1, coord, NULL, &work->hitEffectArg);
                    work->action = ACTOR_03700_ACTION_RETREAT;
                } else {
                    work->action = ACTOR_03700_ACTION_ATTACK;
                }
                work->actionStep = 0;
            }
            if (gSceneCombatState.actor03700Flags & SCENE_COMBAT_ACTOR03700_PLAYER_RELEASE) {
                work->action     = ACTOR_03700_ACTION_RETREAT;
                work->anim       = ACTOR_03700_ANIM_FLY;
                work->actionStep = 0;
            }
            break;
    }
    Actor03700_Fn032BC(task, 0, 14);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

static void Actor03700_Fn01550(Task* task)
{
    _Actor03700Work*      work;
    GfxCoord*             obj;
    Task*                 player;
    void*                 head;
    AnimationPlayRequest* arg;
    s32                   sound;

    work                       = task->work;
    obj                        = task->extra.tmd->coords;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - 0x1C;
    arg                        = SCRATCH_STACK_CURSOR(AnimationPlayRequest);

    switch (work->actionStep) {
        case 0:
            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(&Actor03700_D07F08, 0), 0);
            func_800FDB18(1, obj, NULL, &work->hitEffectArg);
            Gp_SpawnPadLerp(5, 0xC0, 8);
            sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250004;
            SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            if (++work->attackCount >= 6) {
                work->attackCount                  = 0;
                work->actionStep                   = 3;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_PLAYER_RELEASE;
            } else {
                work->actionStep = 1;
                work->timer      = 20;
            }
            break;
        case 1:
            work->speed = -20;
            if (--work->timer <= 0) {
                work->actionStep = 2;
            }
            break;
        case 2:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            work->speed        = Actor03700_D07F1C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            if (work->touchingPlayer != 0) {
                work->actionStep = 0;
            }
            break;
        case 3:
            arg->source.sets          = Actor03700_D080FC;
            arg->animationId          = 2;
            arg->blend                = ANIMATION_BLEND_RESET;
            arg->blendFrames          = 0;
            arg->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, arg, 0);
            sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
            SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            work->actionStep = 4;
            break;
        case 4:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->holdingPlayer                = 0;
                work->action                       = ACTOR_03700_ACTION_RETREAT;
                work->actionStep                   = 0;
                gSceneCombatState.actor03700Flags &= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
    Actor03700_Fn032BC(task, 1, 14);
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

static void Actor03700_Fn018C8(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        obj;
    s32              period;
    s32              i;
    s32              slot;
    s32              sound;

    work   = task->work;
    obj    = task->extra.tmd->coords;
    period = 14;

    switch (work->actionStep) {
        case 0:
            work->actionStep   = 1;
            work->timer        = 30;
            work->retreatTimer = Actor03700_D07F3C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->attackCount  = 0;
            slot               = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 10;
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                work->rig.slots[i].rate = slot;
            }
            break;
        case 1:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            if (--work->retreatTimer > 0) {
                work->speed = -50;
            } else {
                work->speed = 0;
                Actor03700_Fn03320(task, 20);
            }
            if (--work->timer <= 0) {
                work->actionStep = 2;
                work->timer      = 0;
                work->speed      = 0;
            }
            break;
        case 2:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            work->speed        = 5;
            Actor03700_Fn03320(task, 20);
            if (++work->timer >= 91) {
                work->timer      = 0;
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
            }
            break;
        case 3:
            work->timer        = Actor03700_D07F5C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->retreatTimer = 15;
            work->actionStep   = 4;
            work->turnRate     = 0;
            sound              = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250003;
            SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
            break;
        case 4:
            if (--work->retreatTimer > 0) {
                work->speed = -125;
            } else {
                work->speed = 0;
            }
            if (--work->timer <= 0) {
                work->timer      = 0;
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
            }
            Actor03700_Fn03320(task, 80);
            period = 21;
            break;
    }
    Actor03700_Fn032BC(task, 0, period);
}

static void Actor03700_Fn01C94(Task* task)
{
    _Actor03700Work*      work;
    GfxCoord*             obj;
    Task*                 player;
    void*                 head;
    AnimationPlayRequest* arg;
    s32                   sound;
    s32                   pan;

    work                       = task->work;
    obj                        = task->extra.tmd->coords;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(AnimationPlayRequest);
    arg                        = SCRATCH_STACK_CURSOR(AnimationPlayRequest);

    switch (work->actionStep) {
        case 0:
            arg->source.sets          = Actor03700_D080FC;
            arg->animationId          = 2;
            arg->blend                = ANIMATION_BLEND_RESET;
            arg->blendFrames          = 0;
            arg->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, arg, 0);
            sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
            pan   = (s8)worldCoordGetOriginAudioPan(obj);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            work->actionStep = 1;
            break;
        case 1:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->holdingPlayer = 0;
                work->action        = ACTOR_03700_ACTION_RETREAT;
                work->actionStep    = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(AnimationPlayRequest));
}

/// Tests whether the actor has noticed the player: true when the player is
/// under 0x708 units away on the XZ plane (the offset is staged on the
/// scratchpad stack), mid-action (`gSceneCombatState.signals.bytes.actionFlags` low nibble) or holding
/// the aim button (`field_19` bit 0). On noticing, it arms `gSceneCombatState` and
/// plays the alert cue from the placement's sound bank. Returns 1 when noticed.
static s32 Actor03700_Fn01DFC(Task* task)
{
    void**    scratch;
    u8*       head;
    SVECTOR*  vec;
    GfxCoord* coord;
    s16       dx;
    s16       dz;
    s32       ret;
    u32       soundId;
    s32       pan;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    vec                            = (SVECTOR*)(head - 8);
    coord                          = task->extra.tmd->coords;
    vec->vx                        = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    dz                             = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(scratch, void) = vec;
    vec->vz                        = dz;
    dx                             = ((SVECTOR*)(head - 8))->vx;
    ret                            = 0;
    if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x708) || (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_PE_CAST_MASK)) || (gSceneCombatState.actor03700Flags & SCENE_COMBAT_ACTOR03700_ALERT)) {
        ret = 1;
        Gp_ArmStateF0(ret);
        soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x40250000 | ret;
        pan       = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
    return ret;
}

/// Turns the root coordinate towards the target position `targetPos`:
/// `targetYaw` becomes the heading to the target, and `yaw` steps from
/// the matrix's current heading towards it by at most `turnRate`, taking the
/// short way round. The result is written back as a Y rotation.
static void Actor03700_Fn01F48(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    SVECTOR*         rot;
    u16              want;
    s16              ang;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    coord           = task->extra.tmd->coords;
    work            = task->work;
    rot             = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    rot->vx         = work->targetPos.vx - coord->coord.t[0];
    rot->vy         = 0;
    rot->vz         = work->targetPos.vz - coord->coord.t[2];
    work->targetYaw = ratan2(rot->vx, rot->vz) & 0xFFF;
    ang             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    want            = work->targetYaw;
    ang            &= 0xFFF;
    diff            = want - ang;
    adiff           = diff >= 0 ? diff : -diff;

    work->yaw = ang;
    if (adiff < 0x800) {
        step = work->turnRate;
        if (step >= adiff) {
            work->yaw = want;
        } else {
            next = ang;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            goto store;
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
            next = cur - wrapStep;
        } else {
            next = cur + wrapStep;
        }
    store:
        work->yaw = next;
    }
done:
    rot->vx = 0;
    rot->vy = work->yaw;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Relights the actor for the world position of its root coordinate.
static inline void _actor03700UpdateColor(Task* task)
{
    GfxCoord* coord;
    VECTOR    color;

    coord    = task->extra.tmd->coords;
    color.vx = coord->workm.t[0];
    color.vy = coord->workm.t[1];
    color.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &color, 0, 0);
}

/// Spawns effect 0x40007 at the model's fifth coordinate with one of two model
/// streams picked at random, and gives the spawned model the texture page and
/// CLUT of the actor's placement in the current area.
static inline void _actor03700SpawnRemains(Task* task)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    u8               view;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    EffectWork*      eff;
    TmdObject*       model;
    s32              idx;
    u32              raw;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_80067704[0] = &_gActor03700BatBurstWingRight;
    } else {
        D_80067704[0] = &_gActor03700BatBurstWingLeft;
    }
    eff = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[4], 0x80, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey = &gGameSession->location.loc;
    raw        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
    model      = eff->task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    view       = sessionKey->view;
    idx        = raw >> 12;
    key.view   = view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    entry                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

/// Death handler. Mode 1 of `gSceneCombatState.actorControl` only refreshes the actor colour and
/// mode 2 hides the model; otherwise it steps `actionStep`: unlink the enemy and
/// play the death cue (starting the player's release if `holdingPlayer` is set), wait
/// out the delay in `retreatTimer`, spawn the effect `deathEffect` selects, let the
/// player go, count 60 frames in `timer` and destroy the enemy - which a wave
/// director puts off until the last enemy is gone.
static void Actor03700_Fn020D4(Enemy* enemy, Task* task)
{
    TmdObject*           model;
    GfxCoord*            obj;
    _Actor03700Work*     work;
    Task*                player;
    AnimationPlayRequest arg;
    s32                  sound;
    s32                  sound2;

    work   = task->work;
    obj    = task->extra.tmd->coords;
    model  = task->extra.tmd;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor03700UpdateColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->waveDirector != 0) {
                Actor03700_Fn0355C(task);
            }
            switch (work->actionStep) {
                case 0:
                    enemy->recs = 0;
                    Gp_UnlinkObj(&work->body);
                    worldTargetUnlinkNode(&enemy->node);
                    Gp_ReleaseStateF0Add(task, 0x25);
                    model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    sound        = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40250003;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
                    if (work->holdingPlayer != 0) {
                        arg.source.sets          = Actor03700_D080FC;
                        arg.animationId          = 2;
                        arg.blend                = ANIMATION_BLEND_RESET;
                        arg.blendFrames          = 0;
                        arg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &arg, 0);
                        sound2 = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                        SndEvt_EnqueueType6(sound2, (s8)worldCoordGetOriginAudioPan(obj), (s8)worldCoordGetOriginAudioDepth(obj));
                    }
                    work->actionStep   = 1;
                    work->retreatTimer = (((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 6 + 2;
                    break;
                case 1:
                    if (--work->retreatTimer > 0) {
                        break;
                    }
                    switch (work->deathEffect) {
                        case 0:
                            Tmd_FreeBuffers(model);
                            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            _actor03700SpawnRemains(task);
                            break;
                        case 1:
                            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, obj, 0x10280, NULL);
                            break;
                        case 2:
                            Gp_SpawnEff(EFFECT_HIT_PUFF, obj, 0x10013380, NULL);
                            Gp_SpawnEff(EFFECT_HIT_PUFF, obj, 0x10111300, NULL);
                            break;
                    }
                    work->actionStep = 2;
                    break;
                case 2:
                    if (work->holdingPlayer != 0) {
                        if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                            work->actionStep    = 3;
                            work->timer         = 60;
                            work->holdingPlayer = 0;
                        }
                    } else {
                        work->actionStep = 3;
                        work->timer      = 60;
                    }
                    break;
                case 3:
                    if (--work->timer < 0) {
                        work->actionStep = 4;
                    }
                    break;
                case 4:
                    if (work->waveDirector == 0 || work->waveDirector == 2) {
                        enemyDestroy(enemy, task);
                    }
                    break;
            }
            break;
    }
}

/// `ACTION_WAVE_WAIT`: keeps the actor hidden, unlockable and out of the
/// contact passes until `gSceneCombatState.actor03700Wave` reaches the placement's `mode - 9`, then
/// counts `timer` down from 5 and picks a target position `targetPos` above
/// the root coordinate from `gRandomLcgState` - a lower one and `ACTION_ENTER_LOW`
/// where `mode - 9` is below 10, a higher one and `ACTION_ENTER_HIGH` otherwise -
/// before re-enabling the body, rearming a random countdown and allocating the
/// model buffers.
static void Actor03700_Fn025C8(Task* task)
{
    TmdObject*       obj;
    TmdObject*       ext;
    _Actor03700Work* work;
    Enemy*           spawn;
    GfxCoord*        coord;
    s32              diff;

    ext                           = task->extra.tmd;
    work                          = task->work;
    coord                         = ext->coords;
    spawn                         = (Enemy*)task->spawnArg2.pointer;
    obj                           = ext;
    work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    obj->flags                   |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    spawn->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;

    switch (work->actionStep) {
        case 0:
            diff = spawn->place->mode - 9;
            if (gSceneCombatState.actor03700Wave >= diff) {
                work->actionStep = 1;
                work->timer      = 5;
            }
            break;
        case 1:
            diff = spawn->place->mode - 9;
            if (--work->timer <= 0) {
                work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (diff < 10) {
                    work->action       = ACTOR_03700_ACTION_ENTER_LOW;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vx = (u16)coord->coord.t[0] + ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vy = (u16)coord->coord.t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 0x352);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vz = (u16)coord->coord.t[2] + ((gRandomLcgState >> 16) & 0xFF);
                } else {
                    work->action       = ACTOR_03700_ACTION_ENTER_HIGH;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vx = (u16)coord->coord.t[0] + ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vy = (u16)coord->coord.t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 0x73A);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vz = (u16)coord->coord.t[2] + ((gRandomLcgState >> 16) & 0xFF);
                }
                work->actionStep = 0;
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer      = (gRandomLcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
    }
}

/// `ACTION_ENTER_LOW` and `ACTION_ENTER_HIGH`, the entrance after the wave wait:
/// counts `timer` down, then moves the root coordinate towards the target position
/// `targetPos` by 75/2048 of the unit direction per frame until it is within 150
/// on Y, then counts 30 frames and hands over to `ACTION_CHASE`, arming
/// `gSceneCombatState`.
static void Actor03700_Fn027DC(Task* task)
{
    _Actor03700EntryScratch* scratch;
    _Actor03700Work*         work;
    GfxCoord*                coord;
    s32                      d;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor03700EntryScratch);
    work    = task->work;
    coord   = task->extra.tmd->coords;
    switch (work->actionStep) {
        case 0:
            if (--work->timer <= 0) {
                work->actionStep = 1;
            }
            break;
        case 1:
            scratch->toTarget.vx = work->targetPos.vx - coord->coord.t[0];
            scratch->toTarget.vy = work->targetPos.vy - coord->coord.t[1];
            scratch->toTarget.vz = work->targetPos.vz - coord->coord.t[2];
            VectorNormalS(&scratch->toTarget, &scratch->direction);
            coord->coord.t[0] += (scratch->direction.vx * 75) >> 11;
            coord->coord.t[1] += (scratch->direction.vy * 75) >> 11;
            coord->coord.t[2] += (scratch->direction.vz * 75) >> 11;
            d                  = (s32)work->targetPos.vy - coord->coord.t[1];
            if ((d < 0 ? -d : d) < 150) {
                work->actionStep = 2;
                work->timer      = 30;
            }
            break;
        case 2:
            if (--work->timer <= 0) {
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
                work->timer      = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor03700EntryScratch);
}

static void Actor03700_Fn029C0(Task* task)
{
    _Actor03700Work*         work;
    TmdObject*               obj;
    GfxCoord*                coord;
    _Actor03700EntryScratch* scratch;
    Enemy*                   ctx;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor03700EntryScratch);
    obj     = task->extra.tmd;
    coord   = obj->coords;
    work    = task->work;
    ctx     = (Enemy*)task->spawnArg2.pointer;

    switch (work->actionStep) {
        case 0:
            work->body.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            obj->flags                 |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (gSceneCombatState.actor03700Wave == 0) {
                work->actionStep   = 1;
                work->body.flags  |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->targetPos.vx = coord->coord.t[0] - (((gRandomLcgState >> 16) & 0x1FF) + 500);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->targetPos.vy = coord->coord.t[1] + (((gRandomLcgState >> 16) & 0x1FF) + 3500);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->targetPos.vz = coord->coord.t[2] + (((gRandomLcgState >> 16) & 0x1FF) + 2000);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer        = (gRandomLcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
        case 1:
            if (--work->timer <= 0) {
                work->actionStep = 2;
            }
            break;
        case 2:
            scratch->toTarget.vx = work->targetPos.vx - coord->coord.t[0];
            scratch->toTarget.vy = work->targetPos.vy - coord->coord.t[1];
            scratch->toTarget.vz = work->targetPos.vz - coord->coord.t[2];
            VectorNormalS(&scratch->toTarget, &scratch->direction);
            coord->coord.t[0] += (scratch->direction.vx * 5) >> 9;
            coord->coord.t[1] += (scratch->direction.vy * 5) >> 9;
            coord->coord.t[2] += (scratch->direction.vz * 5) >> 9;
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            Actor03700_Fn01F48(task);

            Actor03700_BobInline(task, 0, 21);
            Actor03700_SwayInline(task, 80);

            if (abs(work->targetPos.vy - coord->coord.t[1]) < 40) {
                work->actionStep = 3;
                work->timer      = 30;
                Gp_ArmStateF0(1);
            }
            break;
        case 3:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            Actor03700_Fn01F48(task);

            Actor03700_BobInline(task, 0, 21);
            Actor03700_SwayInline(task, 80);

            if (work->timer == 30) {
                gSceneCombatState.actor03700Wave = 2;
            }
            if (--work->timer <= 0) {
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
                work->timer      = 0;
            }
            break;
    }
}

/// The actor's per-frame task callback: runs the handler for the task's state
/// from `Actor03700_D00004` (spawn, tick, death), passing the enemy record the
/// task was spawned with. The table is copied onto the stack before the call.
static void Actor03700_Fn02FA8(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor03700_D00004;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

static void Actor03700_Fn03004(Enemy* enemy, Task* task)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    _Actor03700Work* work;
    s32              state;
    s32              one;

    obj   = task->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = task->work;
    coord = obj->coords;
    one   = 1;
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
    obj->flags                    = 0;
    enemy->node.state.parts.flags = 0;
    goto default_body;
case2:
    obj->flags                   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    enemy->node.state.parts.flags = one;
    return;
default_body:
    if (work->action < ACTOR_03700_ACTION_WAVE_WAIT) {
        Actor03700_Fn0042C(task, obj, one);
    }
    if (Actor03700_Fn008D0(task) != 0) {
        return;
    }
    if (work->turnRate != 0) {
        Actor03700_Fn01F48(task);
    }
    if (work->speed != 0) {
        Actor03700_Fn0321C(task);
    }
    if (work->waveDirector != 0) {
        Actor03700_Fn0355C(task);
    }
    Actor03700_Fn033F0(task);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
case1:
    Actor03700_Fn034A0(task);
}

/// Asks the player for the melee hold (message 0x3F8, range 8) and, once it is
/// accepted, starts the grab on the actor's animation slot (message 0x3FF) and
/// sets `_Actor03700Work::holdingPlayer`. The task's own unit is held for as long
/// as `GameActor::mode` stays out of mode 2; the two message buffers come
/// from one 0x2C-byte scratch stack push.
static s32 Actor03700_Fn03130(Task* task)
{
    _Actor03700Work*        work;
    Task*                   player;
    void*                   head;
    ActorPlayerHoldScratch* scratch;
    s32                     ret;

    work                       = task->work;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(ActorPlayerHoldScratch);
    scratch                    = SCRATCH_STACK_CURSOR(ActorPlayerHoldScratch);

    ret = 0;
    if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        scratch->buttonPressHold.pressCount = 8;
        if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) == 0) {
            scratch->playerAnim.source.sets          = Actor03700_D080FC;
            scratch->playerAnim.animationId          = 1;
            scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
            scratch->playerAnim.blendFrames          = 0;
            scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            work->holdingPlayer = 1;
            ret                 = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorPlayerHoldScratch));
    return ret;
}

/// Moves the actor forward: remembers the root coordinate's position in
/// `prevPos` (where a collision reset returns it), steps it along the
/// matrix's third column scaled by `speed`, and moves its height 30 units
/// toward the target's `targetPos.vy`.
static void Actor03700_Fn0321C(Task* task)
{
    GfxCoord*        coord;
    _Actor03700Work* work;
    s32              y;

    coord = task->extra.tmd->coords;
    work  = task->work;

    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> 12;
    y                  = coord->coord.t[1];
    coord->coord.t[1]  = (work->targetPos.vy - y > 0) ? y + 30 : y - 30;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> 12;
}

static void Actor03700_Fn032BC(Task* task, s32 arg1, s32 arg2)
{
    _Actor03700Work* work;
    GfxCoord*        coord;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (arg2 < ++work->bobPhase) {
        work->bobPhase = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(arg1 * 15) + work->bobPhase];
}

static void Actor03700_Fn03320(Task* task, s32 arg1)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    s32              amp;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (++work->swayPhase >= 15) {
        work->swayPhase     = 0;
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->swayAmplitude = arg1 + ((gRandomLcgState >> 16) & 0x3F);
    }
    amp                = (work->swayAmplitude * Actor03700_D07FD4[work->swayPhase] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

/// Drives the five animation slots from the requested animation `anim`.
/// When the request differs from the one playing (`playingAnim`), it is
/// latched, the tick counter `animFrame` restarts and every slot is pointed
/// at it with a blend of 4; otherwise the counter ticks and each slot advances.
static void Actor03700_Fn033F0(Task* task)
{
    _Actor03700Work* work;
    s32              i;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, 4);
        }
    } else {
        work->animFrame++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Refreshes the actor's colour from the world position of its root
/// coordinate, with no blend parameters.
static void Actor03700_Fn034A0(Task* task)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2.pointer, &vec, 0, 0);
}

/// Handler for message 0x7DE. Ignored unless the actor is in one of its
/// active actions (below `ACTION_WAVE_WAIT`) and the task is in its tick state.
/// While the actor holds the player (`holdingPlayer`) it moves the hold to its
/// release stage (`actionStep` = 3); otherwise it drops to `ACTION_RETREAT` with
/// the flying animation requested. Always answers 0.
s32 Actor03700_Fn034F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    _Actor03700Work* work;

    work = task->work;
    if (work->action >= ACTOR_03700_ACTION_WAVE_WAIT) {
        return 0;
    }
    if (task->state != 1) {
        return 0;
    }
    if (work->holdingPlayer != 0) {
        work->actionStep = 3;
    } else {
        work->action     = ACTOR_03700_ACTION_RETREAT;
        work->anim       = ACTOR_03700_ANIM_FLY;
        work->actionStep = 0;
    }
    return 0;
}

static void Actor03700_Fn0355C(Task* task)
{
    _Actor03700Work* work = task->work;

    switch (gSceneCombatState.actor03700Wave) {
        case 0:
            break;
        case 1:
            gSceneCombatState.actor03700Wave = 2;
            return;
        case 2:
            if (gSceneCombatState.battleRefs < 0x11) {
                gSceneCombatState.actor03700Wave = 3;
                return;
            }
            break;
        case 3:
            if (gSceneCombatState.battleRefs < 0xE) {
                gSceneCombatState.actor03700Wave = 4;
                return;
            }
            break;
        case 4:
            if (gSceneCombatState.battleRefs < 0xA) {
                gSceneCombatState.actor03700Wave = 5;
                return;
            }
            break;
        case 5:
            if (task->state == 2 && gSceneCombatState.battleRefs == 0) {
                work->waveDirector = 2;
            }
            break;
    }
}
