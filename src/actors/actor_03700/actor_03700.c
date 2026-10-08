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

/// Halfword tables `_actor03700Retreat` indexes by a 4-bit LCG draw:
/// the countdowns of `ACTION_RETREAT`, seeded into `retreatTimer` and `timer`.
extern u16 Actor03700_D07F3C[];
extern u16 Actor03700_D07F5C[];

/// Pair `_actor03700AttackHeldPlayer` packs with `damagePackAttackKey` for message 0x3F9.
extern DamageAttack Actor03700_D07F08;

/// Halfword table `_actor03700AttackHeldPlayer` indexes by a 4-bit LCG draw.
extern s16 Actor03700_D07F1C[];

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 Actor03700_D07F98[];

/// Halfword wave table `_actor03700StepSway` indexes by `swayPhase`.
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

/* `D_80067704` selects the model stream the next `effectSpawn` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

/* The two model streams the death effect picks between, in this overlay's data. */
static TmdSource _gActor03700BatBurstWingRight;
static TmdSource _gActor03700BatBurstWingLeft;

/// Task states used by this actor's three-entry enemy dispatcher.
enum {
    ACTOR_03700_TASK_ACTIVE = 1,
    ACTOR_03700_TASK_DYING  = 2
};

/// Flight-wave indexing and fixed-point translation scales.
///
/// Bob selects a 15-entry starting offset, then reads through the inclusive
/// period; row 0's long cycle therefore also uses entries 15 to 21.
/// Sway uses only the first 15 samples of its 16-entry table, scaled by 4096.
enum {
    ACTOR_03700_BOB_ROW_STRIDE       = 15,
    ACTOR_03700_BOB_NORMAL_PERIOD    = 14,
    ACTOR_03700_BOB_LONG_PERIOD      = 21,
    ACTOR_03700_SWAY_CYCLE_TICKS     = 15,
    ACTOR_03700_SWAY_RANDOM_MASK     = 0x3F,
    ACTOR_03700_SWAY_WAVE_RESCALE    = 16,
    ACTOR_03700_SWAY_WAVE_SHIFT      = 16,
    ACTOR_03700_MATRIX_FRACTION_BITS = 12
};

/// Stages shared by the two perch departures; animation ticks include tick zero.
enum {
    ACTOR_03700_PERCH_WAIT_FOR_ALERT = 0,
    ACTOR_03700_PERCH_DELAY_TAKEOFF  = 1,
    ACTOR_03700_PERCH_TAKEOFF        = 2,
    ACTOR_03700_TAKEOFF_LAST_TICK    = 50
};

/// Release stage of the held-player attack action.
enum { ACTOR_03700_ATTACK_START_RELEASE = 3 };

/// Lifetime of the placement-mode-10 wave director.
enum {
    ACTOR_03700_WAVE_DIRECTOR_ACTIVE   = 1,
    ACTOR_03700_WAVE_DIRECTOR_FINISHED = 2
};

/// Player clips in the bat's borrowed animation-set table.
enum {
    ACTOR_03700_PLAYER_GRAB_CLIP    = 1,
    ACTOR_03700_PLAYER_RELEASE_CLIP = 2
};

/// Death visual selected from the attack key, or a damage-free repel entry.
enum {
    ACTOR_03700_DEATH_BURST_WING    = 0,
    ACTOR_03700_DEATH_ADDITIVE_PUFF = 1,
    ACTOR_03700_DEATH_HIT_PUFFS     = 2,
    ACTOR_03700_ATTACK_REPEL_ONLY   = 3
};

/// Starts the player's release clip using a synchronously borrowed request.
///
/// Use as a standalone statement. playerTask is evaluated once; request is
/// evaluated six times and must be a side-effect-free pointer to writable
/// AnimationPlayRequest storage. Uses this actor's borrowed player clip table,
/// which must remain loaded until playback ends. Writes every request field
/// and enables world collision before dispatch; no caller locals are captured.
#define ACTOR_03700_START_PLAYER_RELEASE(playerTask, request)                                          \
    {                                                                                                  \
        (request)->source.sets          = Actor03700_D080FC;                                           \
        (request)->animationId          = ACTOR_03700_PLAYER_RELEASE_CLIP;                             \
        (request)->blend                = ANIMATION_BLEND_RESET;                                       \
        (request)->blendFrames          = 0;                                                           \
        (request)->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                            \
        TASK_MESSAGE_DISPATCH_POINTER((playerTask), ANIMATION_MESSAGE_INSTALL_AND_PLAY, (request), 0); \
    }

static inline void _actor03700ApplyBobStep(Task* task, s32 row, s32 period);
static inline void _actor03700ApplySwayStep(Task* task, s32 baseAmplitude);
static void        _actor03700InitEnemy(Enemy* enemy, Task* task);
static void        _actor03700ProcessContacts(Task* task, TmdObject* unusedModel, s32 unusedEnabled);
static s32         _actor03700DispatchAction(Task* task);
static void        _actor03700Wander(Task* task);
static void        _actor03700DropFromPerch(Task* task);
static void        _actor03700BackOffPerch(Task* task);
static void        _actor03700Chase(Task* task);
static void        _actor03700AttackHeldPlayer(Task* task);
static void        _actor03700Retreat(Task* task);
static void        _actor03700ReleasePlayer(Task* task);
static s32         _actor03700NoticePlayer(Task* task);
static void        _actor03700TurnTowardTarget(Task* task);
static void        _actor03700Die(Enemy* enemy, Task* task);
static void        _actor03700WaitForWave(Task* task);
static void        _actor03700EnterWave(Task* task);
static void        _actor03700EnterScriptedWave(Task* task);
static void        _actor03700UpdateEnemy(Enemy* enemy, Task* task);
static s32         _actor03700TryHoldPlayer(Task* task);
static void        _actor03700MoveForward(Task* task);
static void        _actor03700StepBob(Task* task, s32 row, s32 period);
static void        _actor03700StepSway(Task* task, s32 baseAmplitude);
static void        _actor03700UpdateAnimation(Task* task);
static void        _actor03700RefreshColor(Task* task);
static void        _actor03700AdvanceWave(Task* task);

static void _actor03700Task(Task* task);

static s32 _actor03700ReleaseHoldMsg(Task* task, s32 messageId, s32 unusedArg1, s32 unusedArg2);

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

TaskDesc Actor03700_D07F8C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor03700Task, { .model = &_gActor03700BatBody } };

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
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor03700ReleaseHoldMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline void _actor03700UpdateColor(Task* task);
static inline void _actor03700SpawnRemains(Task* task);

/// Adds the next vertical flight-bob displacement to the model root.
///
/// `period` is the inclusive last phase, so a cycle has period + 1 ticks.
/// Supported pairs are (row 0, period 14 or 21) and (row 1, period 14);
/// their table indices are 0..14, 0..21 and 15..29. The phase advances
/// before sampling and persists between actions. Positive Y is down.
static inline void _actor03700ApplyBobStep(Task* task, s32 row, s32 period)
{
    _Actor03700Work* work;
    GfxCoord*        coord;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (period < ++work->bobPhase) {
        work->bobPhase = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(row * ACTOR_03700_BOB_ROW_STRIDE) + work->bobPhase];
}

/// Adds the next sideways flight-sway displacement along the root's local X axis.
///
/// The 15-tick cycle advances before sampling. On wrapping it chooses an
/// amplitude of baseAmplitude + 0..63 in root-coordinate units, stored as
/// a signed halfword. The table and rotation basis both use 4096 = 1.0.
/// Current callers pass base amplitudes 20, 40 or 80.
static inline void _actor03700ApplySwayStep(Task* task, s32 baseAmplitude)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    s32              displacement;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (++work->swayPhase >= ACTOR_03700_SWAY_CYCLE_TICKS) {
        work->swayPhase     = 0;
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->swayAmplitude = baseAmplitude + ((gRandomLcgState >> 16) & ACTOR_03700_SWAY_RANDOM_MASK);
    }
    displacement       = (work->swayAmplitude * Actor03700_D07FD4[work->swayPhase] * ACTOR_03700_SWAY_WAVE_RESCALE) >> ACTOR_03700_SWAY_WAVE_SHIFT;
    coord->coord.t[0] += (displacement * coord->coord.m[0][0]) >> ACTOR_03700_MATRIX_FRACTION_BITS;
    coord->coord.t[2] += (displacement * coord->coord.m[2][0]) >> ACTOR_03700_MATRIX_FRACTION_BITS;
}

/// The enemy's state handlers, run by `_actor03700Task` for the task's
/// state: spawn, per-frame tick and death.
static const EnemyTaskFuncTable3 Actor03700_D00004 = {
    { _actor03700InitEnemy, _actor03700UpdateEnemy, _actor03700Die },
};

/// Initializes the placed bat enemy, its animation rig and collision sphere.
///
/// `task` owns a live TMD model and receives a zeroed work block; failure
/// destroys the enemy. `enemy` is the live record in spawnArg2.pointer.
/// Placement modes 0..2 select wander or either perch; other single-digit
/// modes wander. Tens 1 and 2 defer the model buffers for wave entry, with
/// mode 10 directing the wave; tens 3 defer them for scripted entry.
/// The five driven slots start at 16..19 sixteenths of a frame per tick.
/// The task acquires one battle hold and enters its active state.
static void _actor03700InitEnemy(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_03700_SPAWN_MODE_TENS     = 10,
        ACTOR_03700_SPAWN_NORMAL        = 0,
        ACTOR_03700_SPAWN_FIRST_WAVE    = 1,
        ACTOR_03700_SPAWN_LATER_WAVE    = 2,
        ACTOR_03700_SPAWN_SCRIPTED      = 3,
        ACTOR_03700_SPAWN_ACTION_COUNT  = 3,
        ACTOR_03700_SPAWN_DIRECTOR_MODE = 10,
        ACTOR_03700_BODY_ID             = 37,
        ACTOR_03700_HIT_EFFECT_REPEATS  = 1
    };

    TmdObject*       model;
    GfxCoord*        coord;
    _Actor03700Work* work;
    s32              modeOrRateOffset;
    s32              slotIndex;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work          = work;
    model->flags        = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx     = &work->lightMtx;
    model->colorMtx     = &work->colorMtx;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->param                  = &Actor03700_D07F0C;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->recs                   = work->contacts;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = ACTOR_03700_HIT_EFFECT_REPEATS;
    work->yaw                     = enemy->place->yaw;
    modeOrRateOffset              = enemy->place->mode;
    // Placement tens select normal, wave-wait or scripted entrance spawning.
    switch (modeOrRateOffset / ACTOR_03700_SPAWN_MODE_TENS) {
        case ACTOR_03700_SPAWN_NORMAL:
            tmdAllocPrimitiveBuffer(model);
            if (modeOrRateOffset < ACTOR_03700_SPAWN_ACTION_COUNT) {
                work->action = modeOrRateOffset;
            } else {
                work->action = ACTOR_03700_ACTION_WANDER;
            }
            work->anim = modeOrRateOffset < ACTOR_03700_SPAWN_ACTION_COUNT ? modeOrRateOffset + 1 : ACTOR_03700_ANIM_FLY;
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
        case ACTOR_03700_SPAWN_FIRST_WAVE:
        case ACTOR_03700_SPAWN_LATER_WAVE:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->action  = ACTOR_03700_ACTION_WAVE_WAIT;
            work->anim    = ACTOR_03700_ANIM_FLY;
            if (modeOrRateOffset == ACTOR_03700_SPAWN_DIRECTOR_MODE) {
                work->waveDirector = ACTOR_03700_WAVE_DIRECTOR_ACTIVE;
            }
            break;
        case ACTOR_03700_SPAWN_SCRIPTED:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->action  = ACTOR_03700_ACTION_SCRIPTED_ENTRY;
            work->anim    = ACTOR_03700_ANIM_FLY;
            break;
    }
    // All driven parts start at one randomly offset animation rate.
    enemy->hp         = Actor03700_D07F0C.hpMax;
    work->playingAnim = work->anim;
    task->msgTable    = Actor03700_D08108;
    animationInitContext(&work->rig.anim, Actor03700_D080E4, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, work->anim);
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    // The scalar is reused for rate jitter to retain the target register allocation.
    modeOrRateOffset = (gRandomLcgState >> 16) & 3;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate += modeOrRateOffset;
    }
    sceneAcquireBattleRef(0);
    // The task owns the battle hold and collision sphere until death teardown.
    work->homePos.vx            = coord->coord.t[0];
    work->homePos.vy            = coord->coord.t[1];
    work->homePos.vz            = coord->coord.t[2];
    work->body.radius           = 0xC8;
    work->body.coord            = coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_03700_BODY_ID;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state       = ACTOR_03700_TASK_ACTIVE;
}

/// Applies room-grid correction to the root, or restores its saved position.
///
/// GRID_HIT adds the integer halves of the signed 16.16 correction; OPPOSED
/// restores the signed-halfword snapshot taken before forward movement. Other
/// responses leave the root intact. All pointers borrow live, disjoint storage.
static inline void _actor03700ApplyGridResponse(GfxCoord* coord, const _Actor03700Work* work, const ActorContactOverlapPushScratch* scratch, s32 response)
{
    s32 correctedZ;

    switch (response) {
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            correctedZ         = coord->coord.t[2] + scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            coord->coord.t[0] = work->prevPos.vx;
            coord->coord.t[1] = work->prevPos.vy;
            correctedZ        = work->prevPos.vz;
            break;
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
        default:
            return;
    }
    coord->coord.t[2] = correctedZ;
}

/// Resolves room and player overlap, then consumes the bat's four hit contacts.
///
/// Any positive HP damage kills; attachment table value 3 cancels damage and
/// repels. Nonlethal repel reactions interrupt airborne actions, releasing a
/// held player first. Player slot bit 7 selects one of two live attacker tasks;
/// attack rows must satisfy the damage tables (weapon 0..46, attachment 0..54).
/// Overlap uses composed query coordinates and converts its unit direction
/// back to room space; attack distances use the roots' common local frame.
/// The grid and root transforms must be live and squared lengths fit the SDK.
/// Borrows and releases ActorContactOverlapPushScratch. The last two arguments
/// are unused by this implementation but retained by its caller interface.
static void _actor03700ProcessContacts(Task* task, TmdObject* unusedModel, s32 unusedEnabled)
{
    enum {
        ACTOR_03700_ATTACK_ATTACHMENT   = 0x8000,
        ACTOR_03700_ATTACK_ROW_MASK     = 0x7F,
        ACTOR_03700_ATTACK_PLAYER_SHIFT = 7,
        ACTOR_03700_REPEL_REACTION      = 8,
        ACTOR_03700_RETREAT_REPEL_STEP  = 3
    };

    ActorContactOverlapPushScratch* scratch;
    GfxCoord*                       coord;
    GfxCoord*                       attackerCoord;
    _Actor03700Work*                work;
    s32                             deepestOverlap;
    s32                             overlap;
    s32                             contactIndex;
    s32                             clampedOverlap;
    s32                             attackerDeltaX;
    s32                             attackerDeltaY;
    s32                             attackerDeltaZ;
    s32                             repelOrHitEffect;
    u32                             contactKey;
    u32                             hpDamage;

    deepestOverlap   = 0;
    repelOrHitEffect = 0;
    work             = task->work;
    scratch          = SCRATCH_STACK_RESERVE_BLOCK(ActorContactOverlapPushScratch);
    coord            = task->extra.tmd->coords;
    // Resolve grid movement before reusing the delta storage for body contacts.
    _actor03700ApplyGridResponse(coord, work, scratch, worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL));
    contactIndex         = 0;
    work->touchingPlayer = 0;
    do {
        contactKey = work->contacts[contactIndex].key.value;
        switch (contactKey >> 16) {
            case 0:
                break;
            case (WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16):
                work->touchingPlayer     = 1;
                scratch->delta.vector.vx = coord->workm.t[0] - work->contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->contacts[contactIndex].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->contacts[contactIndex].point.vz;
                overlap                  = work->contacts[contactIndex].distance - SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz);
                // Preserve the separate clamp result used before choosing the deepest contact.
                clampedOverlap = overlap;
                if (overlap <= 0) {
                    clampedOverlap = 0;
                }
                overlap = clampedOverlap;
                if (deepestOverlap < overlap) {
                    deepestOverlap = overlap;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->pushDirection);
                }
                break;
            case (WORLD_COLLISION_CONTACT_ATTACK >> 16):
                attackerCoord            = gPlayerActorTasks[(contactKey >> ACTOR_03700_ATTACK_PLAYER_SHIFT) & 1]->extra.tmd->coords;
                attackerDeltaX           = attackerCoord->coord.t[0] - coord->coord.t[0];
                scratch->delta.vector.vx = attackerDeltaX;
                attackerDeltaY           = attackerCoord->coord.t[1] - coord->coord.t[1];
                scratch->delta.vector.vy = attackerDeltaY;
                attackerDeltaZ           = attackerCoord->coord.t[2] - coord->coord.t[2];
                scratch->delta.vector.vz = attackerDeltaZ;
                hpDamage                 = damageComputePlayerAttack(work->contacts[contactIndex].key.value, SquareRoot0(attackerDeltaX * attackerDeltaX + attackerDeltaY * attackerDeltaY + attackerDeltaZ * attackerDeltaZ), 0, 0);
                contactKey               = work->contacts[contactIndex].key.value;
                if (contactKey & ACTOR_03700_ATTACK_ATTACHMENT) {
                    if (Actor03700_D08074[contactKey & ACTOR_03700_ATTACK_ROW_MASK] == ACTOR_03700_ATTACK_REPEL_ONLY) {
                        repelOrHitEffect = 1;
                        hpDamage         = 0;
                    } else {
                        // This temporary holds the hit kind until it becomes the repel flag again.
                        repelOrHitEffect = damageGetPlayerAttackEffectId(contactKey);
                        if ((u32)(repelOrHitEffect - EFFECT_HIT_KIND_APOBIOSIS_SHARD) < 2) {
                            effectSpawnHit(repelOrHitEffect, coord, NULL, &work->hitEffectArg);
                        }
                        work->deathEffect = Actor03700_D08074[work->contacts[contactIndex].key.value & ACTOR_03700_ATTACK_ROW_MASK];
                        repelOrHitEffect  = 0;
                    }
                } else {
                    work->deathEffect = (damageGetPlayerAttackEffectId(contactKey)) == EFFECT_HIT_KIND_SPARK_BURST;
                }
                worldTargetAddReadoutAmount(&((Enemy*)task->spawnArg2.pointer)->node, hpDamage, 0);
                damageAccumulateLifeDrainHp(task->spawnArg2.pointer, work->contacts[contactIndex].key.value, hpDamage, 0);
                if ((s32)hpDamage > 0) {
                    ((Enemy*)task->spawnArg2.pointer)->hp = 0;
                    work->action                          = ACTOR_03700_ACTION_DIE;
                    work->actionStep                      = 0;
                    task->state                           = ACTOR_03700_TASK_DYING;
                } else if (((damageGetPlayerAttackReaction(work->contacts[contactIndex].key.value) & 0xFFFF) == ACTOR_03700_REPEL_REACTION || repelOrHitEffect == 1) &&
                           (work->action != ACTOR_03700_ACTION_PERCH_DROP && work->action != ACTOR_03700_ACTION_PERCH_BACK)) {
                    if (work->holdingPlayer == 0) {
                        work->action     = ACTOR_03700_ACTION_RETREAT;
                        work->actionStep = ACTOR_03700_RETREAT_REPEL_STEP;
                    } else {
                        work->action     = ACTOR_03700_ACTION_RELEASE;
                        work->actionStep = 0;
                    }
                }
                break;
        }
    } while (++contactIndex < ARRAY_SIZE(work->contacts));
    if (deepestOverlap > 0) {
        coord->coord.t[0] += (deepestOverlap * scratch->pushDirection.vx) >> ACTOR_03700_MATRIX_FRACTION_BITS;
        coord->coord.t[2] += (deepestOverlap * scratch->pushDirection.vz) >> ACTOR_03700_MATRIX_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactOverlapPushScratch);
}

/// Counts one airborne tick and queues the bat's flight cue every sixteenth.
///
/// Reloads the live work and root after the action handler. The placement index
/// supplies the sound instance tag; pan and depth retain signed-byte narrowing.
static inline void _actor03700StepFlightSound(Task* task)
{
    enum {
        ACTOR_03700_FLIGHT_SOUND_TICKS = 16,
        ACTOR_03700_FLIGHT_SOUND       = SOUND_CHARACTER(0x25, 5)
    };
    _Actor03700Work* work;
    GfxCoord*        coord;

    work  = task->work;
    coord = task->extra.tmd->coords;
    if (++work->flightSoundTimer < ACTOR_03700_FLIGHT_SOUND_TICKS) {
        return;
    }
    work->flightSoundTimer = 0;
    {
        u32 soundId;
        s32 pan;
        soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId <<= 8;
        soundId  |= ACTOR_03700_FLIGHT_SOUND;
        pan       = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Runs the current bat action and advances its periodic flight sound.
///
/// Returns 1 for death or hidden wave-wait, skipping movement and animation
/// for this tick; all other actions return 0. Airborne actions count a sound
/// every 16 dispatched ticks. Scripted entry counts only after leaving step 0.
static s32 _actor03700DispatchAction(Task* task)
{
    s16              action;
    s32              skipMovement;
    _Actor03700Work* work;

    work         = task->work;
    action       = work->action;
    skipMovement = 0;
    switch (action) {
        case ACTOR_03700_ACTION_WANDER:
            _actor03700Wander(task);
            _actor03700StepFlightSound(task);
            return skipMovement;
        case ACTOR_03700_ACTION_PERCH_DROP:
            _actor03700DropFromPerch(task);
            return skipMovement;
        case ACTOR_03700_ACTION_PERCH_BACK:
            _actor03700BackOffPerch(task);
            return skipMovement;
        case ACTOR_03700_ACTION_CHASE:
            _actor03700Chase(task);
            _actor03700StepFlightSound(task);
            return skipMovement;
        case ACTOR_03700_ACTION_ATTACK:
            _actor03700AttackHeldPlayer(task);
            _actor03700StepFlightSound(task);
            return skipMovement;
        case ACTOR_03700_ACTION_RETREAT:
            _actor03700Retreat(task);
            _actor03700StepFlightSound(task);
            return skipMovement;
        case ACTOR_03700_ACTION_DIE:
            work->actionStep = 0;
            skipMovement     = 1;
            break;
        case ACTOR_03700_ACTION_WAVE_WAIT:
            worldCollisionClearContacts(work->contacts);
            if (work->waveDirector != 0) {
                _actor03700AdvanceWave(task);
            }
            _actor03700WaitForWave(task);
            skipMovement = 1;
            break;
        case ACTOR_03700_ACTION_ENTER_LOW:
            if (work->waveDirector != 0) {
                _actor03700AdvanceWave(task);
            }
            _actor03700EnterWave(task);
            _actor03700StepFlightSound(task);
            return skipMovement;
        case ACTOR_03700_ACTION_ENTER_HIGH:
            _actor03700EnterWave(task);
            _actor03700StepFlightSound(task);
            return skipMovement;
        case ACTOR_03700_ACTION_SCRIPTED_ENTRY:
            _actor03700EnterScriptedWave(task);
            if (work->actionStep != 0) {
                _actor03700StepFlightSound(task);
            }
            return skipMovement;
        case ACTOR_03700_ACTION_RELEASE:
            _actor03700ReleasePlayer(task);
            break;
    }
    return skipMovement;
}

/// Flies between random waypoints around the spawn position until alerted.
///
/// Selects speed 20..35 and a waypoint within 511 horizontal units of
/// home, with Y offset 0..511. Reaching within 120 horizontal units
/// selects another waypoint. Alert detection switches to chase and
/// latches the group alert; movement and turning run later in the tick.
/// The placement's turn-rate row must be 0..7. Waypoint offsets and the
/// arrival-distance test retain signed-halfword narrowing.
static void _actor03700Wander(Task* task)
{
    enum { ACTOR_03700_WANDER_PICK_TARGET   = 0,
           ACTOR_03700_WANDER_FOLLOW_TARGET = 1 };

    _Actor03700Work* work;
    GfxCoord*        coord;
    SVECTOR*         targetOffset;
    s16              waypointYaw;
    s32              waypointRadius;

    targetOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work         = task->work;
    coord        = task->extra.tmd->coords;

    switch (work->actionStep) {
        // Pick a nearby waypoint around the spawn position.
        case ACTOR_03700_WANDER_PICK_TARGET:
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->speed        = ((gRandomLcgState >> 16) & 0xF) + 20;
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            waypointYaw        = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            waypointRadius     = (gRandomLcgState >> 16) & 0x1FF;
            work->targetPos.vx = work->homePos.vx + ((waypointRadius * rsin(waypointYaw)) >> ACTOR_03700_MATRIX_FRACTION_BITS);
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = work->homePos.vy + ((gRandomLcgState >> 16) & 0x1FF);
            work->targetPos.vz = work->homePos.vz + ((waypointRadius * rcos(waypointYaw)) >> ACTOR_03700_MATRIX_FRACTION_BITS);
            targetOffset->vx   = work->targetPos.vx - coord->coord.t[0];
            targetOffset->vy   = 0;
            targetOffset->vz   = work->targetPos.vz - coord->coord.t[2];
            work->targetYaw    = ratan2(targetOffset->vx, targetOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->actionStep   = ACTOR_03700_WANDER_FOLLOW_TARGET;
            break;
        case ACTOR_03700_WANDER_FOLLOW_TARGET:
            targetOffset->vx = work->targetPos.vx - coord->coord.t[0];
            targetOffset->vz = work->targetPos.vz - coord->coord.t[2];
            if ((s16)SquareRoot0(targetOffset->vx * targetOffset->vx + targetOffset->vz * targetOffset->vz) < 120) {
                work->actionStep = ACTOR_03700_WANDER_PICK_TARGET;
            }
            break;
    }
    _actor03700StepBob(task, 0, ACTOR_03700_BOB_NORMAL_PERIOD);
    _actor03700StepSway(task, 20);
    if (_actor03700NoticePlayer(task) != 0) {
        work->action                       = ACTOR_03700_ACTION_CHASE;
        work->actionStep                   = 0;
        gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Integrates the path segment covering the current takeoff animation tick.
///
/// The first covering segment wins. Divisions truncate per-tick Y and forward
/// movement; table entries have positive frame counts and ordered last ticks.
/// `segments` borrows `segmentCount` elements for this call; it is not retained.
/// The callers pass tables of seven or nine elements and animFrame 0..50;
/// both paths end at tick 50. No covering segment means no movement. Angles
/// use 4096 per turn and translation uses the matrix's 12 fractional bits.
static inline void _actor03700StepTakeoffPath(const _Actor03700Work* work, GfxCoord* coord, const _Actor03700TakeoffSegment* segments, s32 segmentCount)
{
    s32 segmentIndex;
    s32 advancePerTick;

    for (segmentIndex = 0; segmentIndex < segmentCount; segmentIndex++) {
        if (segments[segmentIndex].lastFrame >= work->animFrame) {
            coord->coord.t[1] += segments[segmentIndex].deltaY / segments[segmentIndex].frameCount;
            advancePerTick     = segments[segmentIndex].advance / segments[segmentIndex].frameCount;
            coord->coord.t[0] += (rsin(work->yaw) * advancePerTick) >> ACTOR_03700_MATRIX_FRACTION_BITS;
            coord->coord.t[2] += (rcos(work->yaw) * advancePerTick) >> ACTOR_03700_MATRIX_FRACTION_BITS;
            break;
        }
    }
}

/// Leaves its perch after an alert and a random 0..63-tick delay.
///
/// The takeoff drops and swoops forward along the placement yaw.
/// Each animation tick uses the first covering path segment; integer
/// division truncates its vertical and forward increments. At tick 50
/// the actor requests the flying animation and starts chasing.
static void _actor03700DropFromPerch(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;

    work  = task->work;
    coord = task->extra.tmd->coords;

    switch (work->actionStep) {
        case ACTOR_03700_PERCH_WAIT_FOR_ALERT:
            if (_actor03700NoticePlayer(task) != 0) {
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
                gRandomLcgState                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep                   = ACTOR_03700_PERCH_DELAY_TAKEOFF;
                work->timer                        = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case ACTOR_03700_PERCH_DELAY_TAKEOFF:
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_PERCH_TAKEOFF;
                work->timer      = 0;
                work->anim       = ACTOR_03700_ANIM_TAKEOFF_DROP;
            }
            break;
        // Integrate the first segment that still covers this animation tick.
        case ACTOR_03700_PERCH_TAKEOFF:
            _actor03700StepTakeoffPath(work, coord, Actor03700_D07FF4, ARRAY_SIZE(Actor03700_D07FF4));
            if (work->animFrame >= ACTOR_03700_TAKEOFF_LAST_TICK) {
                work->anim                         = ACTOR_03700_ANIM_FLY;
                work->action                       = ACTOR_03700_ACTION_CHASE;
                work->actionStep                   = 0;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
}

/// Leaves its perch after an alert and a random 0..63-tick delay.
///
/// The takeoff pushes off backwards along the placement yaw.
/// Each animation tick uses the first covering path segment; integer
/// division truncates its vertical and forward increments. At tick 50
/// the actor requests the flying animation and starts chasing.
static void _actor03700BackOffPerch(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;

    work  = task->work;
    coord = task->extra.tmd->coords;

    switch (work->actionStep) {
        case ACTOR_03700_PERCH_WAIT_FOR_ALERT:
            if (_actor03700NoticePlayer(task) != 0) {
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
                gRandomLcgState                    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->actionStep                   = ACTOR_03700_PERCH_DELAY_TAKEOFF;
                work->timer                        = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case ACTOR_03700_PERCH_DELAY_TAKEOFF:
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_PERCH_TAKEOFF;
                work->timer      = 0;
                work->anim       = ACTOR_03700_ANIM_TAKEOFF_BACK;
            }
            break;
        // Integrate the first segment that still covers this animation tick.
        case ACTOR_03700_PERCH_TAKEOFF:
            _actor03700StepTakeoffPath(work, coord, Actor03700_D0802C, ARRAY_SIZE(Actor03700_D0802C));
            if (work->animFrame >= ACTOR_03700_TAKEOFF_LAST_TICK) {
                work->anim                         = ACTOR_03700_ANIM_FLY;
                work->action                       = ACTOR_03700_ACTION_CHASE;
                work->actionStep                   = 0;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
}

/// Chooses an overhead player waypoint, preserving one shared LCG draw.
///
/// The height mask is 0x3FF for these actions: Y is 800..1823 units above
/// the player. All components narrow into signed-halfword target storage.
static inline void _actor03700PickPlayerTarget(_Actor03700Work* work, u32 heightMask)
{
    enum { ACTOR_03700_PLAYER_TARGET_HEIGHT = 800 };
    work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & heightMask) + ACTOR_03700_PLAYER_TARGET_HEIGHT);
    work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
}

/// Turns toward a point above the player and dashes until contact or timeout.
///
/// Targets are 800..1823 units above the player, stored as signed halfwords.
/// Placement turn-rate rows are 0..7; the 16-entry dash table gives nonzero
/// speeds 30..60 units per tick. Dash duration uses a signed-halfword 3D
/// offset and integer distance/speed. Driven animation rates are 17..20
/// sixteenths of a frame. Contact starts the player hold or retreats on refusal;
/// a group release request also retreats. Reserves and releases one SVECTOR.
static void _actor03700Chase(Task* task)
{
    enum {
        ACTOR_03700_CHASE_TURN  = 0,
        ACTOR_03700_CHASE_DASH  = 1,
        ACTOR_03700_CHASE_SOUND = SOUND_CHARACTER(0x25, 2)
    };

    _Actor03700Work* work;
    GfxCoord*        coord;
    SVECTOR*         targetOffset;
    s32              slotIndex;
    s32              soundId;
    s8               animationRate;

    coord        = task->extra.tmd->coords;
    targetOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work         = task->work;

    switch (work->actionStep) {
        // Select the next dash after turning toward an overhead waypoint.
        case ACTOR_03700_CHASE_TURN:
            _actor03700PickPlayerTarget(work, 0x3FF);
            if (work->dashDone == 0) {
                work->speed = 5;
            } else {
                work->speed    = 0;
                work->dashDone = 0;
            }
            work->turnRate = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            _actor03700StepSway(task, 20);
            if (work->yaw == work->targetYaw) {
                work->actionStep = ACTOR_03700_CHASE_DASH;
                work->speed      = Actor03700_D07F1C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                coord            = task->extra.tmd->coords;
                targetOffset->vx = work->targetPos.vx - coord->coord.t[0];
                targetOffset->vy = work->targetPos.vy - coord->coord.t[1];
                targetOffset->vz = work->targetPos.vz - coord->coord.t[2];
                work->timer      = SquareRoot0(targetOffset->vx * targetOffset->vx + targetOffset->vy * targetOffset->vy + targetOffset->vz * targetOffset->vz) / work->speed;
                animationRate    = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 17;
                for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                    work->rig.slots[slotIndex].rate = animationRate;
                }
                coord   = task->extra.tmd->coords;
                soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_03700_CHASE_SOUND;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case ACTOR_03700_CHASE_DASH:
            _actor03700StepSway(task, 40);
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_CHASE_TURN;
                work->timer      = 0;
                work->dashDone   = 1;
            }
            if (work->touchingPlayer != 0) {
                if (_actor03700TryHoldPlayer(task) == 0) {
                    effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, coord, NULL, &work->hitEffectArg);
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
    _actor03700StepBob(task, 0, ACTOR_03700_BOB_NORMAL_PERIOD);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Strikes the held player, backs away, and returns for up to six strikes.
///
/// Each strike deals the package attack, plays its effect and motor ramp, then
/// backs off for 20 ticks. The sixth starts release; completion ends scripted
/// control and returns to retreat. Turn-rate rows are 0..7 and speed draws
/// index the 16-entry table. Release clip data must remain loaded while playing.
/// Reserves 28 scratch bytes per tick for a 20-byte AnimationPlayRequest; the
/// remaining eight bytes are untouched and their role is unproven. All 28
/// bytes are released before returning; dispatch borrows the request only.
static void _actor03700AttackHeldPlayer(Task* task)
{
    enum {
        ACTOR_03700_ATTACK_STRIKE        = 0,
        ACTOR_03700_ATTACK_BACK_AWAY     = 1,
        ACTOR_03700_ATTACK_RETURN        = 2,
        ACTOR_03700_ATTACK_WAIT_RELEASE  = 4,
        ACTOR_03700_ATTACK_STRIKE_LIMIT  = 6,
        ACTOR_03700_ATTACK_BACKOFF_TICKS = 20,
        ACTOR_03700_ATTACK_SOUND         = SOUND_CHARACTER(0x25, 4),
        ACTOR_03700_ATTACK_SCRATCH_BYTES = 0x1C
    };

    _Actor03700Work*      work;
    GfxCoord*             coord;
    Task*                 player;
    AnimationPlayRequest* releaseRequest;
    s32                   soundId;

    work           = task->work;
    coord          = task->extra.tmd->coords;
    player         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    releaseRequest = SCRATCH_STACK_RESERVE_BYTES(ACTOR_03700_ATTACK_SCRATCH_BYTES);

    switch (work->actionStep) {
        case ACTOR_03700_ATTACK_STRIKE:
            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&Actor03700_D07F08, 0), 0);
            effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, coord, NULL, &work->hitEffectArg);
            padScriptSpawnVariableMotorRamp(5, 0xC0, 8);
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_03700_ATTACK_SOUND;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            if (++work->attackCount >= ACTOR_03700_ATTACK_STRIKE_LIMIT) {
                work->attackCount                  = 0;
                work->actionStep                   = ACTOR_03700_ATTACK_START_RELEASE;
                gSceneCombatState.actor03700Flags |= SCENE_COMBAT_ACTOR03700_PLAYER_RELEASE;
            } else {
                work->actionStep = ACTOR_03700_ATTACK_BACK_AWAY;
                work->timer      = ACTOR_03700_ATTACK_BACKOFF_TICKS;
            }
            break;
        case ACTOR_03700_ATTACK_BACK_AWAY:
            work->speed = -20;
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_ATTACK_RETURN;
            }
            break;
        case ACTOR_03700_ATTACK_RETURN:
            _actor03700PickPlayerTarget(work, 0x3FF);
            work->speed    = Actor03700_D07F1C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->turnRate = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            if (work->touchingPlayer != 0) {
                work->actionStep = 0;
            }
            break;
        // Keep scripted control until the release animation has finished.
        case ACTOR_03700_ATTACK_START_RELEASE:
            ACTOR_03700_START_PLAYER_RELEASE(player, releaseRequest);
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_PLAYER_STRUCK;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            work->actionStep = ACTOR_03700_ATTACK_WAIT_RELEASE;
            break;
        case ACTOR_03700_ATTACK_WAIT_RELEASE:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->holdingPlayer                = 0;
                work->action                       = ACTOR_03700_ACTION_RETREAT;
                work->actionStep                   = 0;
                gSceneCombatState.actor03700Flags &= SCENE_COMBAT_ACTOR03700_ALERT;
            }
            break;
    }
    _actor03700StepBob(task, 1, ACTOR_03700_BOB_NORMAL_PERIOD);
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_03700_ATTACK_SCRATCH_BYTES);
}

/// Backs away and returns to chase, or recoils after a repelling hit.
///
/// Normal entry resets the strike count and sets all five animation rates
/// to 10..13, backs away at 50 units per tick, then approaches at 5 units
/// per tick for 91 ticks. Entry at step 3 recoils at 125 units per tick
/// without turning, then hovers until a table-selected 245..310-tick
/// countdown expires; that countdown includes the recoil.
/// Both paths add bob and sway; the recoil path uses a 22-tick bob.
/// The placement's turn-rate row must be 0..7.
static void _actor03700Retreat(Task* task)
{
    enum {
        ACTOR_03700_RETREAT_BEGIN       = 0,
        ACTOR_03700_RETREAT_BACK_AWAY   = 1,
        ACTOR_03700_RETREAT_RETURN      = 2,
        ACTOR_03700_RETREAT_BEGIN_REPEL = 3,
        ACTOR_03700_RETREAT_REPEL_HOVER = 4,
        ACTOR_03700_RETREAT_SOUND       = SOUND_CHARACTER(0x25, 3)
    };

    _Actor03700Work* work;
    GfxCoord*        coord;
    s32              bobPeriod;
    s32              slotIndex;
    s32              animationRate;
    s32              soundId;

    work      = task->work;
    coord     = task->extra.tmd->coords;
    bobPeriod = ACTOR_03700_BOB_NORMAL_PERIOD;

    switch (work->actionStep) {
        case ACTOR_03700_RETREAT_BEGIN:
            work->actionStep   = ACTOR_03700_RETREAT_BACK_AWAY;
            work->timer        = 30;
            work->retreatTimer = Actor03700_D07F3C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->attackCount  = 0;
            animationRate      = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) + 10;
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                work->rig.slots[slotIndex].rate = animationRate;
            }
            break;
        case ACTOR_03700_RETREAT_BACK_AWAY:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x3FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            if (--work->retreatTimer > 0) {
                work->speed = -50;
            } else {
                work->speed = 0;
                _actor03700StepSway(task, 20);
            }
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_RETREAT_RETURN;
                work->timer      = 0;
                work->speed      = 0;
            }
            break;
        case ACTOR_03700_RETREAT_RETURN:
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - (((gRandomLcgState >> 16) & 0x1FF) + 800);
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            work->speed        = 5;
            _actor03700StepSway(task, 20);
            if (++work->timer >= 91) {
                work->timer      = 0;
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
            }
            break;
        // Repelling hits enter here and keep the existing heading.
        case ACTOR_03700_RETREAT_BEGIN_REPEL:
            work->timer        = Actor03700_D07F5C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            work->retreatTimer = 15;
            work->actionStep   = ACTOR_03700_RETREAT_REPEL_HOVER;
            work->turnRate     = 0;
            soundId            = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_03700_RETREAT_SOUND;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case ACTOR_03700_RETREAT_REPEL_HOVER:
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
            _actor03700StepSway(task, 80);
            bobPeriod = ACTOR_03700_BOB_LONG_PERIOD;
            break;
    }
    _actor03700StepBob(task, 0, bobPeriod);
}

/// Releases a held player after a repelling hit, then starts retreating.
///
/// Installs the package's player-release animation once and keeps the hold
/// until playback ends. The request is borrowed only during synchronous
/// dispatch and its scratch storage is released before returning.
static void _actor03700ReleasePlayer(Task* task)
{
    enum {
        ACTOR_03700_RELEASE_START_ANIMATION    = 0,
        ACTOR_03700_RELEASE_WAIT_FOR_ANIMATION = 1,
        ACTOR_03700_PLAYER_RELEASE_ANIMATION   = 2
    };

    _Actor03700Work*      work;
    GfxCoord*             coord;
    Task*                 player;
    AnimationPlayRequest* releaseRequest;
    s32                   soundId;
    s32                   pan;

    work           = task->work;
    coord          = task->extra.tmd->coords;
    player         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    releaseRequest = SCRATCH_STACK_RESERVE_BLOCK(AnimationPlayRequest);

    switch (work->actionStep) {
        case ACTOR_03700_RELEASE_START_ANIMATION:
            releaseRequest->source.sets          = Actor03700_D080FC;
            releaseRequest->animationId          = ACTOR_03700_PLAYER_RELEASE_ANIMATION;
            releaseRequest->blend                = ANIMATION_BLEND_RESET;
            releaseRequest->blendFrames          = 0;
            releaseRequest->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, releaseRequest, 0);
            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_PLAYER_STRUCK;
            pan     = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            work->actionStep = ACTOR_03700_RELEASE_WAIT_FOR_ANIMATION;
            break;
        case ACTOR_03700_RELEASE_WAIT_FOR_ANIMATION:
            if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                work->holdingPlayer = 0;
                work->action        = ACTOR_03700_ACTION_RETREAT;
                work->actionStep    = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(AnimationPlayRequest);
}

/// Detects a player stimulus and queues the bat's alert cue.
///
/// Returns 1 when the player is less than 1800 units away in X/Z, makes
/// noise or uses PE, or the group alert is already latched; otherwise 0.
/// Offsets narrow to signed halfwords in the roots' common parent frame.
/// A positive result engages the battle and queues the cue each call;
/// the caller latches the group alert. Borrows one scratch vector.
static s32 _actor03700NoticePlayer(Task* task)
{
    enum { ACTOR_03700_NOTICE_RADIUS = 1800 };

    void**    cursorSlot;
    SVECTOR*  scratchTop;
    SVECTOR*  playerOffset;
    GfxCoord* coord;
    s16       offsetX;
    s16       offsetZ;
    s32       noticed;
    u32       soundId;
    s32       pan;

    cursorSlot                           = SCRATCH_HEAD_ADDR;
    scratchTop                           = SCRATCH_HEAD_AT(cursorSlot, SVECTOR);
    playerOffset                         = scratchTop - 1;
    coord                                = task->extra.tmd->coords;
    playerOffset->vx                     = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    offsetZ                              = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(cursorSlot, SVECTOR) = playerOffset;
    playerOffset->vz                     = offsetZ;
    offsetX                              = (scratchTop - 1)->vx;
    noticed                              = 0;
    if ((SquareRoot0((offsetX * offsetX) + (offsetZ * offsetZ)) < ACTOR_03700_NOTICE_RADIUS) || (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_PE_CAST_MASK)) || (gSceneCombatState.actor03700Flags & SCENE_COMBAT_ACTOR03700_ALERT)) {
        noticed = 1;
        sceneEngageBattle(noticed);
        soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        soundId >>= ENEMY_PLACE_INDEX_SHIFT;
        soundId <<= 8;
        soundId  |= SOUND_CHARACTER(0x25, 0) | noticed;
        pan       = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return noticed;
}

/// Turns the model root toward its target by at most the requested turn rate.
///
/// Angles use 4096 units per turn. The X/Z target offset narrows to signed
/// halfwords before measuring its bearing. The heading is read from the
/// root matrix and replaced with a pure Y rotation; translation remains.
/// The shortest turn is chosen, with the wrapped branch used at exactly
/// half a turn. Callers supply a nonnegative turn rate (0 keeps heading).
static void _actor03700TurnTowardTarget(Task* task)
{
    _Actor03700Work* work;
    GfxCoord*        coord;
    SVECTOR*         targetOffsetAndRotation;
    u16              targetYaw;
    s16              currentYaw;
    s16              yawDelta;
    s32              angleDistance;
    s32              turnLimit;
    s32              storedYaw;
    s32              nextYaw;
    s32              wrappedTurnLimit;

    coord                       = task->extra.tmd->coords;
    work                        = task->work;
    targetOffsetAndRotation     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    targetOffsetAndRotation->vx = work->targetPos.vx - coord->coord.t[0];
    targetOffsetAndRotation->vy = 0;
    targetOffsetAndRotation->vz = work->targetPos.vz - coord->coord.t[2];
    work->targetYaw             = ratan2(targetOffsetAndRotation->vx, targetOffsetAndRotation->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    currentYaw                  = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    targetYaw                   = work->targetYaw;
    currentYaw                 &= ACTOR_TRANSFORM_ANGLE_MASK;
    yawDelta                    = targetYaw - currentYaw;
    angleDistance               = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    // Retain the opposite half-turn choice when the headings differ by exactly 2048.
    if (angleDistance < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnLimit = work->turnRate;
        if (turnLimit >= angleDistance) {
            work->yaw = targetYaw;
        } else {
            nextYaw = currentYaw;
            if (yawDelta <= 0) {
                nextYaw -= turnLimit;
            } else {
                nextYaw += turnLimit;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnLimit = work->turnRate;
        if (yawDelta > 0 ? turnLimit >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : turnLimit >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnLimit = work->turnRate;
            storedYaw        = work->yaw;
            if (yawDelta > 0) {
                nextYaw = storedYaw - wrappedTurnLimit;
            } else {
                nextYaw = storedYaw + wrappedTurnLimit;
            }
            work->yaw = nextYaw;
        }
    }
    targetOffsetAndRotation->vx = 0;
    targetOffsetAndRotation->vy = work->yaw;
    targetOffsetAndRotation->vz = 0;
    RotMatrix(targetOffsetAndRotation, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Refreshes the bat's world lighting at its composed root position.
///
/// The live Enemy receives the color update; the three written VECTOR
/// components are world game units. The color API consumes that position
/// synchronously and ignores the remaining VECTOR word.
static inline void _actor03700UpdateColor(Task* task)
{
    GfxCoord* coord;
    VECTOR    worldPosition;

    coord            = task->extra.tmd->coords;
    worldPosition.vx = coord->workm.t[0];
    worldPosition.vy = coord->workm.t[1];
    worldPosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
}

/// Applies a detached wing's placement textures to both primitive-buffer halves.
///
/// Both objects are live; placement storage is borrowed only for this call.
/// With no buffer, the offsets are retained for subsequent model allocation.
static inline void _actor03700SetRemainsTextures(TmdObject* model, const AreaPlacement* placement)
{
    model->texturePageOffset = placement->texturePageOffset;
    model->clutRowOffset     = placement->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Spawns a randomly selected detached bat wing with its placement textures.
///
/// Chooses either wing with one LCG draw and spawns bank-4 body-part effect
/// at coordinate 4. Failure leaves the selected descriptor source installed.
/// The current location variant and the placement index encoded in placeKey
/// must be valid. Copies its texture page and CLUT offsets and rebuilds both
/// primitive-buffer halves when allocated. The effect owns its spawned model;
/// wing geometry stays borrowed from this loaded actor overlay. The spawn's
/// size 128 selects its emitted puffs and excludes the later corpse-burn effect.
static inline void _actor03700SpawnRemains(Task* task)
{
    enum { ACTOR_03700_REMAINS_PUFF_SIZE = 128 };

    GameLocationKey  location;
    GameLocationKey* sessionLocation;
    u8               currentView;
    AreaVariant*     variant;
    EffectWork*      remains;
    TmdObject*       remainsModel;
    s32              placementIndex;
    u32              placeKey;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        D_80067704[0] = &_gActor03700BatBurstWingRight;
    } else {
        D_80067704[0] = &_gActor03700BatBurstWingLeft;
    }
    remains = effectSpawn(EFFECT_BURST_BODY_PART_BANK4, &task->extra.tmd->coords[4], ACTOR_03700_REMAINS_PUFF_SIZE, NULL);
    if (remains == NULL) {
        return;
    }
    sessionLocation = &gGameSession->location.loc;
    placeKey        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
    remainsModel    = remains->task->extra.tmd;
    location.stage  = sessionLocation->stage;
    location.area   = sessionLocation->area;
    location.room   = sessionLocation->room;
    currentView     = sessionLocation->view;
    placementIndex  = placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view   = currentView;
    areaSyncLocationVariant(&location);
    variant = areaGetVariant(&location);
    _actor03700SetRemainsTextures(remainsModel, &variant->placements[placementIndex]);
}

/// Unlinks the dying bat, releases its player hold, and delays final teardown.
///
/// Pause only updates lighting; hidden control only disables drawing. Running
/// control unlinks collision and targeting, releases the battle reward hold,
/// and delays the selected death visual by placement index % 6 + 2 ticks.
/// The release request is consumed synchronously; its clip table stays loaded.
/// After release it counts 60 down through zero before entering destruction.
/// A wave director remains alive until its wave state reports completion.
static void _actor03700Die(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_03700_DEATH_BEGIN        = 0,
        ACTOR_03700_DEATH_WAIT_EFFECT  = 1,
        ACTOR_03700_DEATH_WAIT_PLAYER  = 2,
        ACTOR_03700_DEATH_LINGER       = 3,
        ACTOR_03700_DEATH_DESTROY      = 4,
        ACTOR_03700_DEATH_LINGER_TICKS = 60,
        ACTOR_03700_DEATH_SOUND        = SOUND_CHARACTER(0x25, 3),
        // Size 640, with randomized rise; size is a projection numerator.
        ACTOR_03700_DEATH_RISING_PUFF_ARG = (1 << 16) | 640,
        // Alternate palette, additive blend, three ticks per cell, size 896.
        ACTOR_03700_DEATH_SLOW_HIT_PUFF_ARG = (1 << 28) | (1 << 16) | (3 << 12) | 896,
        // Alternate palette, randomized velocity, additive blend, size 768.
        ACTOR_03700_DEATH_MOVING_HIT_PUFF_ARG = (1 << 28) | (1 << 20) | (1 << 16) | (1 << 12) | 768
    };

    TmdObject*           model;
    GfxCoord*            coord;
    _Actor03700Work*     work;
    Task*                player;
    AnimationPlayRequest releaseRequest;
    s32                  deathSoundId;
    s32                  releaseSoundId;

    work   = task->work;
    coord  = task->extra.tmd->coords;
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
                _actor03700AdvanceWave(task);
            }
            switch (work->actionStep) {
                // Stop receiving contacts and release the encounter hold before the visual delay.
                case ACTOR_03700_DEATH_BEGIN:
                    enemy->recs = 0;
                    worldCollisionUnlinkBody(&work->body);
                    worldTargetUnlinkNode(&enemy->node);
                    sceneReleaseBattleRefWithRewards(task, 0x25); // The second argument is ignored.
                    model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    deathSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_03700_DEATH_SOUND;
                    sndEvtRequestScriptStart(deathSoundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    if (work->holdingPlayer != 0) {
                        ACTOR_03700_START_PLAYER_RELEASE(player, &releaseRequest);
                        releaseSoundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_PLAYER_STRUCK;
                        sndEvtRequestScriptStart(releaseSoundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                    work->actionStep   = ACTOR_03700_DEATH_WAIT_EFFECT;
                    work->retreatTimer = (((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 6 + 2;
                    break;
                case ACTOR_03700_DEATH_WAIT_EFFECT:
                    if (--work->retreatTimer > 0) {
                        break;
                    }
                    switch (work->deathEffect) {
                        case ACTOR_03700_DEATH_BURST_WING:
                            tmdFreePrimitiveBuffer(model);
                            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            _actor03700SpawnRemains(task);
                            break;
                        case ACTOR_03700_DEATH_ADDITIVE_PUFF:
                            effectSpawn(EFFECT_ADDITIVE_PUFF, coord, ACTOR_03700_DEATH_RISING_PUFF_ARG, NULL);
                            break;
                        case ACTOR_03700_DEATH_HIT_PUFFS:
                            effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_03700_DEATH_SLOW_HIT_PUFF_ARG, NULL);
                            effectSpawn(EFFECT_HIT_PUFF, coord, ACTOR_03700_DEATH_MOVING_HIT_PUFF_ARG, NULL);
                            break;
                    }
                    work->actionStep = ACTOR_03700_DEATH_WAIT_PLAYER;
                    break;
                // The player can outlive the death visual while its release clip finishes.
                case ACTOR_03700_DEATH_WAIT_PLAYER:
                    if (work->holdingPlayer != 0) {
                        if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                            work->actionStep    = ACTOR_03700_DEATH_LINGER;
                            work->timer         = ACTOR_03700_DEATH_LINGER_TICKS;
                            work->holdingPlayer = 0;
                        }
                    } else {
                        work->actionStep = ACTOR_03700_DEATH_LINGER;
                        work->timer      = ACTOR_03700_DEATH_LINGER_TICKS;
                    }
                    break;
                case ACTOR_03700_DEATH_LINGER:
                    if (--work->timer < 0) {
                        work->actionStep = ACTOR_03700_DEATH_DESTROY;
                    }
                    break;
                case ACTOR_03700_DEATH_DESTROY:
                    if (work->waveDirector == 0 || work->waveDirector == ACTOR_03700_WAVE_DIRECTOR_FINISHED) {
                        enemyDestroy(enemy, task);
                    }
                    break;
            }
            break;
    }
}

#undef ACTOR_03700_START_PLAYER_RELEASE

/// Keeps a wave enemy hidden until its placement's activation counter is reached.
///
/// The threshold is placement mode - 9. After five ticks it enables the
/// collision sphere, allocates model buffers and selects a randomized
/// entrance target: 850..1361 units above for thresholds below 10, or
/// 1850..2361 above otherwise, with X/Z offsets 0..255. The entrance
/// receives a fresh 0..31-tick delay. Y is down; packed target components
/// retain signed-halfword truncation. Drawing resumes on the next tick.
static void _actor03700WaitForWave(Task* task)
{
    enum { ACTOR_03700_WAVE_WAIT_FOR_COUNTER = 0,
           ACTOR_03700_WAVE_DELAY_ENTRY      = 1,
           ACTOR_03700_WAVE_MODE_OFFSET      = 9,
           ACTOR_03700_WAVE_HIGH_THRESHOLD   = 10,
           ACTOR_03700_WAVE_START_DELAY      = 5,
           ACTOR_03700_WAVE_LOW_RISE         = 850,
           ACTOR_03700_WAVE_HIGH_RISE        = 1850 };

    TmdObject*       model;
    TmdObject*       taskModel;
    _Actor03700Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              activationWave;

    taskModel = task->extra.tmd;
    work      = task->work;
    coord     = taskModel->coords;
    enemy     = (Enemy*)task->spawnArg2.pointer;
    // Both pointers refer to one model; separate uses retain instruction scheduling.
    model                         = taskModel;
    work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    model->flags                 |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;

    switch (work->actionStep) {
        case ACTOR_03700_WAVE_WAIT_FOR_COUNTER:
            activationWave = enemy->place->mode - ACTOR_03700_WAVE_MODE_OFFSET;
            if (gSceneCombatState.actor03700Wave >= activationWave) {
                work->actionStep = ACTOR_03700_WAVE_DELAY_ENTRY;
                work->timer      = ACTOR_03700_WAVE_START_DELAY;
            }
            break;
        case ACTOR_03700_WAVE_DELAY_ENTRY:
            activationWave = enemy->place->mode - ACTOR_03700_WAVE_MODE_OFFSET;
            if (--work->timer <= 0) {
                // Choose an arrival point above the root before enabling contact and buffers.
                work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (activationWave < ACTOR_03700_WAVE_HIGH_THRESHOLD) {
                    work->action       = ACTOR_03700_ACTION_ENTER_LOW;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vx = (u16)coord->coord.t[0] + ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vy = (u16)coord->coord.t[1] - (((gRandomLcgState >> 16) & 0x1FF) + ACTOR_03700_WAVE_LOW_RISE);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vz = (u16)coord->coord.t[2] + ((gRandomLcgState >> 16) & 0xFF);
                } else {
                    work->action       = ACTOR_03700_ACTION_ENTER_HIGH;
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vx = (u16)coord->coord.t[0] + ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vy = (u16)coord->coord.t[1] - (((gRandomLcgState >> 16) & 0x1FF) + ACTOR_03700_WAVE_HIGH_RISE);
                    gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->targetPos.vz = (u16)coord->coord.t[2] + ((gRandomLcgState >> 16) & 0xFF);
                }
                work->actionStep = 0;
                gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer      = (gRandomLcgState >> 16) & 0x1F;
                tmdAllocPrimitiveBuffer(model);
                model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
    }
}

/// Flies a wave enemy to its arrival height and starts chasing after a hover.
///
/// After the entry delay it takes approximately 150 root-coordinate units
/// per tick along a normalized 3D target offset. Arrival tests only Y,
/// within 150 units; it then waits 30 ticks, starts chase and engages
/// the battle. The scratch block is released on every return.
static void _actor03700EnterWave(Task* task)
{
    enum {
        ACTOR_03700_ENTRY_DELAY            = 0,
        ACTOR_03700_ENTRY_FLY              = 1,
        ACTOR_03700_ENTRY_HOVER            = 2,
        ACTOR_03700_ENTRY_DIRECTION_FACTOR = 75,
        ACTOR_03700_ENTRY_DIRECTION_SHIFT  = 11,
        ACTOR_03700_ENTRY_HEIGHT_TOLERANCE = 150,
        ACTOR_03700_ENTRY_HOVER_TICKS      = 30
    };

    _Actor03700EntryScratch* scratch;
    _Actor03700Work*         work;
    GfxCoord*                coord;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor03700EntryScratch);
    work    = task->work;
    coord   = task->extra.tmd->coords;
    switch (work->actionStep) {
        case ACTOR_03700_ENTRY_DELAY:
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_ENTRY_FLY;
            }
            break;
        // Normalized components use 4096 = 1.0, giving a 150-unit step.
        case ACTOR_03700_ENTRY_FLY:
            scratch->toTarget.vx = work->targetPos.vx - coord->coord.t[0];
            scratch->toTarget.vy = work->targetPos.vy - coord->coord.t[1];
            scratch->toTarget.vz = work->targetPos.vz - coord->coord.t[2];
            VectorNormalS(&scratch->toTarget, &scratch->direction);
            coord->coord.t[0] += (scratch->direction.vx * ACTOR_03700_ENTRY_DIRECTION_FACTOR) >> ACTOR_03700_ENTRY_DIRECTION_SHIFT;
            coord->coord.t[1] += (scratch->direction.vy * ACTOR_03700_ENTRY_DIRECTION_FACTOR) >> ACTOR_03700_ENTRY_DIRECTION_SHIFT;
            coord->coord.t[2] += (scratch->direction.vz * ACTOR_03700_ENTRY_DIRECTION_FACTOR) >> ACTOR_03700_ENTRY_DIRECTION_SHIFT;
            if (abs(work->targetPos.vy - coord->coord.t[1]) < ACTOR_03700_ENTRY_HEIGHT_TOLERANCE) {
                work->actionStep = ACTOR_03700_ENTRY_HOVER;
                work->timer      = ACTOR_03700_ENTRY_HOVER_TICKS;
            }
            break;
        case ACTOR_03700_ENTRY_HOVER:
            if (--work->timer <= 0) {
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
                work->timer      = 0;
                sceneEngageBattle(1);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor03700EntryScratch);
}

/// Flies a hidden bat into the encounter and starts the first enemy wave.
///
/// Wave counter zero enables collision and buffers after a 0..31-tick delay.
/// The target is 500..1011 units toward negative X, 3500..4011 down, and
/// 2000..2511 toward positive Z, narrowed to signed halfwords. Flight takes
/// 40 units per tick along a 4096-scale normalized direction, with bob and
/// sway. Within 40 vertical units it engages battle, faces the player for
/// 30 ticks, sets the wave counter to 2, then chases. Turn-rate rows are 0..7.
/// Each call leaves its 24-byte scratch reservation active. The task dispatcher
/// and resident task runner do not restore it; the main loop resets the shared
/// cursor on its next pass. Subsequent tasks share the remaining capacity.
static void _actor03700EnterScriptedWave(Task* task)
{
    enum {
        ACTOR_03700_SCRIPTED_WAIT            = 0,
        ACTOR_03700_SCRIPTED_DELAY           = 1,
        ACTOR_03700_SCRIPTED_FLY             = 2,
        ACTOR_03700_SCRIPTED_FACE_PLAYER     = 3,
        ACTOR_03700_SCRIPTED_FOLLOW_TICKS    = 30,
        ACTOR_03700_SCRIPTED_DIRECTION_SHIFT = 9,
        ACTOR_03700_SCRIPTED_DIRECTION_SCALE = 5,
        ACTOR_03700_SCRIPTED_FIRST_WAVE      = 2
    };

    _Actor03700Work*         work;
    TmdObject*               model;
    GfxCoord*                coord;
    _Actor03700EntryScratch* scratch;
    Enemy*                   enemy;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor03700EntryScratch);
    model   = task->extra.tmd;
    coord   = model->coords;
    work    = task->work;
    enemy   = (Enemy*)task->spawnArg2.pointer;

    switch (work->actionStep) {
        case ACTOR_03700_SCRIPTED_WAIT:
            work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            model->flags                 |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            if (gSceneCombatState.actor03700Wave == 0) {
                work->actionStep   = ACTOR_03700_SCRIPTED_DELAY;
                work->body.flags  |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->targetPos.vx = coord->coord.t[0] - (((gRandomLcgState >> 16) & 0x1FF) + 500);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->targetPos.vy = coord->coord.t[1] + (((gRandomLcgState >> 16) & 0x1FF) + 3500);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->targetPos.vz = coord->coord.t[2] + (((gRandomLcgState >> 16) & 0x1FF) + 2000);
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer        = (gRandomLcgState >> 16) & 0x1F;
                tmdAllocPrimitiveBuffer(model);
                model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            break;
        case ACTOR_03700_SCRIPTED_DELAY:
            if (--work->timer <= 0) {
                work->actionStep = ACTOR_03700_SCRIPTED_FLY;
            }
            break;
        // Enter in a straight line, then face the player before activating the wave.
        case ACTOR_03700_SCRIPTED_FLY:
            scratch->toTarget.vx = work->targetPos.vx - coord->coord.t[0];
            scratch->toTarget.vy = work->targetPos.vy - coord->coord.t[1];
            scratch->toTarget.vz = work->targetPos.vz - coord->coord.t[2];
            VectorNormalS(&scratch->toTarget, &scratch->direction);
            coord->coord.t[0] += (scratch->direction.vx * ACTOR_03700_SCRIPTED_DIRECTION_SCALE) >> ACTOR_03700_SCRIPTED_DIRECTION_SHIFT;
            coord->coord.t[1] += (scratch->direction.vy * ACTOR_03700_SCRIPTED_DIRECTION_SCALE) >> ACTOR_03700_SCRIPTED_DIRECTION_SHIFT;
            coord->coord.t[2] += (scratch->direction.vz * ACTOR_03700_SCRIPTED_DIRECTION_SCALE) >> ACTOR_03700_SCRIPTED_DIRECTION_SHIFT;
            work->turnRate     = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            _actor03700TurnTowardTarget(task);

            _actor03700ApplyBobStep(task, 0, ACTOR_03700_BOB_LONG_PERIOD);
            _actor03700ApplySwayStep(task, 80);

            if (abs(work->targetPos.vy - coord->coord.t[1]) < 40) {
                work->actionStep = ACTOR_03700_SCRIPTED_FACE_PLAYER;
                work->timer      = ACTOR_03700_SCRIPTED_FOLLOW_TICKS;
                sceneEngageBattle(1);
            }
            break;
        case ACTOR_03700_SCRIPTED_FACE_PLAYER:
            _actor03700PickPlayerTarget(work, 0x3FF);
            work->turnRate = Actor03700_D07F7C[((Enemy*)task->spawnArg2.pointer)->place->rowIndex];
            _actor03700TurnTowardTarget(task);

            _actor03700ApplyBobStep(task, 0, ACTOR_03700_BOB_LONG_PERIOD);
            _actor03700ApplySwayStep(task, 80);

            if (work->timer == ACTOR_03700_SCRIPTED_FOLLOW_TICKS) {
                gSceneCombatState.actor03700Wave = ACTOR_03700_SCRIPTED_FIRST_WAVE;
            }
            if (--work->timer <= 0) {
                work->action     = ACTOR_03700_ACTION_CHASE;
                work->actionStep = 0;
                work->timer      = 0;
            }
            break;
    }
}

/// Dispatches the bat task's spawn, active or death state.
///
/// The descriptor supplies a live Enemy in spawnArg2.pointer. Task state must
/// index the three-entry table (0 spawn, 1 active, 2 dying). The table is copied
/// by value before dispatch. The callback does not restore the scratch cursor.
static void _actor03700Task(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = Actor03700_D00004;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Updates the active bat's contacts, action, movement, animation and lighting.
///
/// Pause refreshes lighting only; hidden control hides and disables targeting.
/// Running control restores drawing and targeting before action dispatch.
/// Actions below wave-wait consume contacts. Death and wave-wait skip the
/// rest of the tick; otherwise it turns, moves at nonzero speed, advances the
/// wave director, animates, composes the root and refreshes world lighting.
/// Task, enemy, model, work and borrowed clip tables must remain live.
static void _actor03700UpdateEnemy(Enemy* enemy, Task* task)
{
    GfxCoord*        coord;
    TmdObject*       model;
    _Actor03700Work* work;
    s32              actorControl;
    s32              notLockable;

    model        = task->extra.tmd;
    actorControl = gSceneCombatState.actorControl;
    work         = task->work;
    coord        = model->coords;
    notLockable  = WORLD_TARGET_NOT_LOCKABLE;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor03700RefreshColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = notLockable;
            return;
    }
    // The previous collision pass can change the action or start death this tick.
    if (work->action < ACTOR_03700_ACTION_WAVE_WAIT) {
        _actor03700ProcessContacts(task, model, notLockable);
    }
    if (_actor03700DispatchAction(task) != 0) {
        return;
    }
    if (work->turnRate != 0) {
        _actor03700TurnTowardTarget(task);
    }
    if (work->speed != 0) {
        _actor03700MoveForward(task);
    }
    if (work->waveDirector != 0) {
        _actor03700AdvanceWave(task);
    }
    // Lighting samples the composed root after movement and animation.
    _actor03700UpdateAnimation(task);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _actor03700RefreshColor(task);
}

/// Asks the player to escape a grab by pressing buttons and starts its grab clip.
///
/// Returns 1 on acceptance and latches holdingPlayer, otherwise 0. A player
/// already in scripted mode is refused. The request sets eight escape presses;
/// only that field is read by the hold handler. Accepted playback borrows the
/// bat's player clip table until release. Both synchronous payloads occupy one
/// 44-byte ActorPlayerHoldScratch reservation, released before returning.
static s32 _actor03700TryHoldPlayer(Task* task)
{
    enum {
        ACTOR_03700_HOLD_ESCAPE_PRESSES = 8
    };

    _Actor03700Work*        work;
    Task*                   player;
    ActorPlayerHoldScratch* scratch;
    s32                     accepted;

    work    = task->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorPlayerHoldScratch);

    accepted = 0;
    if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        scratch->buttonPressHold.pressCount = ACTOR_03700_HOLD_ESCAPE_PRESSES;
        if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) == 0) {
            scratch->playerAnim.source.sets          = Actor03700_D080FC;
            scratch->playerAnim.animationId          = ACTOR_03700_PLAYER_GRAB_CLIP;
            scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
            scratch->playerAnim.blendFrames          = 0;
            scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            work->holdingPlayer = 1;
            accepted            = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerHoldScratch);
    return accepted;
}

/// Moves along the root's facing and steps its height toward the current target.
///
/// Saves a signed-halfword position for a later grid reset. X/Z use the
/// 4096-scale forward matrix column and signed speed in units per tick. Y
/// steps 30 units, including upward when exactly at target height; it can
/// overshoot. The caller invokes this only for nonzero speed.
static void _actor03700MoveForward(Task* task)
{
    GfxCoord*        coord;
    _Actor03700Work* work;
    s32              currentY;

    coord = task->extra.tmd->coords;
    work  = task->work;

    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> ACTOR_03700_MATRIX_FRACTION_BITS;
    currentY           = coord->coord.t[1];
    coord->coord.t[1]  = (work->targetPos.vy - currentY > 0) ? currentY + 30 : currentY - 30;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> ACTOR_03700_MATRIX_FRACTION_BITS;
}

/// Adds a vertical flight-bob step to the model root.
///
/// `row` and `period` have the same inclusive-phase contract as that helper.
static void _actor03700StepBob(Task* task, s32 row, s32 period)
{
    _actor03700ApplyBobStep(task, row, period);
}

/// Adds a sideways flight-sway step along the model root's local X axis.
///
/// `baseAmplitude` is in root-coordinate units and receives a 0..63 jitter.
static void _actor03700StepSway(Task* task, s32 baseAmplitude)
{
    _actor03700ApplySwayStep(task, baseAmplitude);
}

/// Restarts or advances the bat's five driven animation slots.
///
/// A changed request resets animFrame and seeks slots 1..5 with a four-frame
/// blend; otherwise the signed-halfword tick count advances and each slot
/// ticks at its own rate. Requested clips are indices 1..5 of the live set
/// table, and slot zero remains undriven.
static void _actor03700UpdateAnimation(Task* task)
{
    enum {
        ACTOR_03700_ANIMATION_BLEND_FRAMES = 4
    };

    _Actor03700Work* work;
    s32              slotIndex;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, ACTOR_03700_ANIMATION_BLEND_FRAMES);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Refreshes the bat's world lighting at its composed root position.
static void _actor03700RefreshColor(Task* task)
{
    _actor03700UpdateColor(task);
}

/// Requests release of an active bat's player hold, or retreat when it holds none.
///
/// Handles `ACTOR_MESSAGE_RELEASE_HOLD` without payload and always returns
/// 0. Active actions below wave-wait respond only in the task's active
/// state. A held player is released by the attack action's animation
/// stage; an unheld actor starts normal retreat with the flying animation.
static s32 _actor03700ReleaseHoldMsg(Task* task, s32 messageId, s32 unusedArg1, s32 unusedArg2)
{
    _Actor03700Work* work;

    work = task->work;
    if (work->action >= ACTOR_03700_ACTION_WAVE_WAIT) {
        return 0;
    }
    if (task->state != ACTOR_03700_TASK_ACTIVE) {
        return 0;
    }
    if (work->holdingPlayer != 0) {
        work->actionStep = ACTOR_03700_ATTACK_START_RELEASE;
    } else {
        work->action     = ACTOR_03700_ACTION_RETREAT;
        work->anim       = ACTOR_03700_ANIM_FLY;
        work->actionStep = 0;
    }
    return 0;
}

/// Advances the bat encounter's wave counter as scene battle holds are released.
///
/// The placement-mode-10 director changes counter 1 to 2, then advances
/// to 3, 4 and 5 below 17, 14 and 10 outstanding battle references.
/// Each call advances at most once. At counter 5, a dying director with
/// no battle references marks itself finished so death teardown may end it.
static void _actor03700AdvanceWave(Task* task)
{
    enum {
        ACTOR_03700_WAVE_DORMANT          = 0,
        ACTOR_03700_WAVE_ACTIVATED        = 1,
        ACTOR_03700_WAVE_FIRST            = 2,
        ACTOR_03700_WAVE_SECOND           = 3,
        ACTOR_03700_WAVE_THIRD            = 4,
        ACTOR_03700_WAVE_FINAL            = 5,
        ACTOR_03700_SECOND_WAVE_REF_LIMIT = 17,
        ACTOR_03700_THIRD_WAVE_REF_LIMIT  = 14,
        ACTOR_03700_FINAL_WAVE_REF_LIMIT  = 10
    };

    _Actor03700Work* work = task->work;

    switch (gSceneCombatState.actor03700Wave) {
        case ACTOR_03700_WAVE_DORMANT:
            break;
        case ACTOR_03700_WAVE_ACTIVATED:
            gSceneCombatState.actor03700Wave = ACTOR_03700_WAVE_FIRST;
            return;
        case ACTOR_03700_WAVE_FIRST:
            if (gSceneCombatState.battleRefs < ACTOR_03700_SECOND_WAVE_REF_LIMIT) {
                gSceneCombatState.actor03700Wave = ACTOR_03700_WAVE_SECOND;
                return;
            }
            break;
        case ACTOR_03700_WAVE_SECOND:
            if (gSceneCombatState.battleRefs < ACTOR_03700_THIRD_WAVE_REF_LIMIT) {
                gSceneCombatState.actor03700Wave = ACTOR_03700_WAVE_THIRD;
                return;
            }
            break;
        case ACTOR_03700_WAVE_THIRD:
            if (gSceneCombatState.battleRefs < ACTOR_03700_FINAL_WAVE_REF_LIMIT) {
                gSceneCombatState.actor03700Wave = ACTOR_03700_WAVE_FINAL;
                return;
            }
            break;
        case ACTOR_03700_WAVE_FINAL:
            if (task->state == ACTOR_03700_TASK_DYING && gSceneCombatState.battleRefs == 0) {
                work->waveDirector = ACTOR_03700_WAVE_DIRECTOR_FINISHED;
            }
            break;
    }
}
