#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>
#include <psyq/limits.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_contacts.h"

/// Phases of the directional flinch pose mixed over the current body animation.
enum {
    ACTOR_01100_FLINCH_OFF      = 0,
    ACTOR_01100_FLINCH_START    = 1,
    ACTOR_01100_FLINCH_FADE_IN  = 2,
    ACTOR_01100_FLINCH_FADE_OUT = 3
};

/// Lifetimes count active ticks; the cloud smoke word is the effect's packed spawn argument.
enum {
    ACTOR_01100_SPIT_LIFETIME_TICKS       = 90,
    ACTOR_01100_SPIT_ATTACK_INDEX         = 5,
    ACTOR_01100_SPIT_CLOUD_SMOKE_ARGUMENT = 0xC0031FFF
};

/// Work block of the two kinds of task the spit state launches: the globs the
/// Mossback throws three at a time, and the clouds its larger variant puts out
/// on either side of its head.
///
/// The task's first state allocates it zeroed and keeps it at `Task::work`;
/// the exit callback unlinks `body` before the task is killed. `body` rides on
/// the task's own coordinate and reports into `contacts`.
///
/// A glob is a capsule: each tick it moves by `velocity` and covers the
/// stretch it has just crossed, until a contact ends the flight. A cloud stays
/// where it was put and is a sphere, so `velocity` and `capsule` stay zero.
typedef struct {
    SVECTOR               velocity;    // glob only: movement per tick in world units, thrown along the launch yaw with a random climb and speed; each tick adds 10 to its Y
    WorldCollisionBody    body;        // attack body on the task's coordinate; taken out of the grid and pair tests once it has struck, and for a cloud's last 0x14 ticks
    WorldCollisionCapsule capsule;     // glob only: shape of `body`, radius 0x96 at both ends, with end 1 trailing by the tick's movement
    WorldCollisionContact contacts[1]; // contact table of `body`; a glob's is emptied every tick
} _Actor01100SpitWork;
STATIC_ASSERT_SIZEOF(_Actor01100SpitWork, 0x58);

/// Indices into `_Actor01100Work::bodies` and `_Actor01100Work::contacts`.
enum {
    ACTOR_01100_BODY_ROOT       = 0, // sphere on the model's root, tested against the world
    ACTOR_01100_BODY_LEFT_HAND  = 1, // attack sphere on part 12
    ACTOR_01100_BODY_RIGHT_HAND = 2, // attack sphere on part 8
    ACTOR_01100_BODY_CHEST      = 3, // sphere on part 3 that takes the hits
    ACTOR_01100_BODY_COUNT
};

/// Model parts a state handler can name in `_Actor01100Scratch::splashPart`,
/// as indices into the model's part coordinates.
enum {
    ACTOR_01100_PART_CHEST      = 3,
    ACTOR_01100_PART_RIGHT_HAND = 8,
    ACTOR_01100_PART_LEFT_HAND  = 12
};

/// Model coordinates used by setup and attack handlers; the player also uses part 1 for its body.
enum {
    ACTOR_01100_PART_BODY            = 1,
    ACTOR_01100_PART_HEAD            = 4,
    ACTOR_01100_PART_RIGHT_UPPER_ARM = 6,
    ACTOR_01100_PART_LEFT_UPPER_ARM  = 10
};

/// Animation-set indices requested by these state handlers, independent of state indices.
enum {
    ACTOR_01100_MOTION_IDLE             = 1,
    ACTOR_01100_MOTION_IDLE_SWING_LEFT  = 2,
    ACTOR_01100_MOTION_IDLE_SWING_RIGHT = 3,
    ACTOR_01100_MOTION_TURN             = 4,
    ACTOR_01100_MOTION_REST             = 5,
    ACTOR_01100_MOTION_PUNCH_LEFT       = 6,
    ACTOR_01100_MOTION_PUNCH_RIGHT      = 7,
    ACTOR_01100_MOTION_ADVANCE          = 9,
    ACTOR_01100_MOTION_FLINCH_FRONT     = 11,
    ACTOR_01100_MOTION_FLINCH_BEHIND    = 14
};

/// Attack-row indices in the selected enemy parameter table.
enum {
    ACTOR_01100_ATTACK_IDLE_LEFT   = 1,
    ACTOR_01100_ATTACK_IDLE_RIGHT  = 2,
    ACTOR_01100_ATTACK_PUNCH_LEFT  = 3,
    ACTOR_01100_ATTACK_PUNCH_RIGHT = 4
};

/// Placement variants and strict squared-distance limits, in room coordinate units squared.
enum {
    ACTOR_01100_ENTRY_STANDARD            = 0xB,
    ACTOR_01100_ENTRY_LARGE               = 0x31,
    ACTOR_01100_PUNCH_RANGE_SQUARED       = 3300 * 3300,
    ACTOR_01100_SPIT_RANGE_SQUARED        = 3000 * 3000,
    ACTOR_01100_LARGE_SPIT_RANGE_SQUARED  = 1500 * 1500,
    ACTOR_01100_ADVANCE_MIN_RANGE_SQUARED = 1400 * 1400,
    ACTOR_01100_BODY_TURN_STEP            = 16,
    ACTOR_01100_FACING_TOLERANCE          = 128
};

/// Sound-script base ids; a request adds the water variant and placement index.
enum {
    ACTOR_01100_SOUND_STRIDE_END          = 0x400B0001,
    ACTOR_01100_SOUND_STRIDE_BEGIN        = 0x400B0002,
    ACTOR_01100_SOUND_REST                = 0x400B0004,
    ACTOR_01100_SOUND_PUNCH               = 0x400B0008,
    ACTOR_01100_SOUND_WATER_VARIANT_SHIFT = 22,
    ACTOR_01100_SOUND_PLACEMENT_SHIFT     = 8
};

/// Values of `_Actor01100Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// 5 to 9 and 0x10 to 0x13 run the `IDLE` handler and are never selected.
enum {
    ACTOR_01100_STATE_IDLE             = 0x00, // stands for a random time, then picks one of the next four
    ACTOR_01100_STATE_IDLE_SWING_LEFT  = 0x01, // swings the left arm, its hand sphere live for the middle of the motion
    ACTOR_01100_STATE_IDLE_SWING_RIGHT = 0x02, // the same with the right arm
    ACTOR_01100_STATE_IDLE_TURN        = 0x03, // turns on the spot by a random angle
    ACTOR_01100_STATE_IDLE_REST        = 0x04, // plays motion 5 through with a sound cue; a hit taken here counts double
    ACTOR_01100_STATE_NOTICE           = 0x0A, // looks round at the player, pauses, turns to face and picks an attack
    ACTOR_01100_STATE_SPIT             = 0x0B, // swells both shoulders and launches projectiles from beside the head
    ACTOR_01100_STATE_PUNCH_LEFT       = 0x0C, // stretches the left arm out at the player
    ACTOR_01100_STATE_PUNCH_RIGHT      = 0x0D, // stretches the right arm out at the player
    ACTOR_01100_STATE_ADVANCE          = 0x0E, // walks at the player, then punches, spits or turns
    ACTOR_01100_STATE_FACE_PLAYER      = 0x0F, // turns to face the player, then punches; spits when the turn outlasts its wait and the player is in range
    ACTOR_01100_STATE_STUNNED          = 0x14, // lies where it fell until the status buildup runs out
    ACTOR_01100_STATE_FLINCH           = 0x15, // recoils from a hit
    ACTOR_01100_STATE_FALL             = 0x16, // falls over
    ACTOR_01100_STATE_RISE             = 0x17, // gets up
    ACTOR_01100_STATE_DEATH            = 0x18, // falls or bursts apart, then burns and fades
    ACTOR_01100_STATE_FALL_AGAIN       = 0x19  // thrown back down by a hit taken while rising
};

/// Values of `_Actor01100Work::mode`: which follow-up the per-frame tick runs
/// after the state handler.
enum {
    ACTOR_01100_MODE_UNAWARE  = 0x00, // idling; the player coming within range, or a hit, engages it
    ACTOR_01100_MODE_ENGAGED  = 0x01, // fighting; takes hits, and goes back to idling when a motion ends with the player far off
    ACTOR_01100_MODE_REACTING = 0x02, // thrown by a hit; takes hits while the arm stretches and shoulder swells die away
    ACTOR_01100_MODE_DYING    = 0x03, // takes no more hits
    ACTOR_01100_MODE_FINISHED = 0x10  // the death has played out; the tick exits the task
};

/// Values of `_Actor01100Work::reaction`: what the last hit made of the enemy.
///
/// `TWITCH`, `FLINCH` and `FALL` are in order of strength: a hit draws the
/// stronger of what its own damage and `recentDamage` call for.
enum {
    ACTOR_01100_REACTION_NONE         = 0x00, // nothing: a hit taken while falling, or none since the last recovery
    ACTOR_01100_REACTION_TWITCH       = 0x01, // the flinch motion mixed over the current one, which carries on
    ACTOR_01100_REACTION_FLINCH       = 0x02, // `ACTOR_01100_STATE_FLINCH`
    ACTOR_01100_REACTION_FALL         = 0x03, // `ACTOR_01100_STATE_FALL`
    ACTOR_01100_REACTION_FALL_AGAIN   = 0x04, // `ACTOR_01100_STATE_FALL_AGAIN`: a hit of flinch strength or more while rising
    ACTOR_01100_REACTION_STUNNED      = 0x05, // held in `ACTOR_01100_STATE_STUNNED`; further hits change nothing until it ends
    ACTOR_01100_REACTION_RISING_LIGHT = 0x06, // a weaker hit while rising: its cue and effect only
    ACTOR_01100_REACTION_RISING       = 0x10, // written as the rise begins
    ACTOR_01100_REACTION_DYING        = 0x20  // written as the death begins
};

/// Values of `_Actor01100Work::downState`.
enum {
    ACTOR_01100_DOWN_STANDING = 0, // on its feet
    ACTOR_01100_DOWN_FALLING  = 1, // from the start of a fall; hits draw no reaction
    ACTOR_01100_DOWN_RISING   = 2  // from the end of a fall until partway through the rise; hits draw `FALL_AGAIN` or `RISING_LIGHT`
};

/// Work block of this package's enemy task, the Mossback its model record
/// names.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`; every
/// state handler and the model-draw message handler work on it. It holds the
/// coordinate that scales the model, the body animation and the flinch
/// animation mixed over it, the four collision bodies with a contact table
/// each, storage for the model's matrices, and the values of the state
/// machine.
///
/// Placement entry 0x31 is the larger variant: its own parameter set, a model
/// scaled by 1.25, a shorter wait before it spits and a different spit.
///
/// Angles are 4096ths of a turn. The stretch and swell values are 12-bit
/// fractions added to a scale of 1.0. Motions index the package's animation
/// table.
typedef struct {
    GfxCoord              scaleCoord;                          // spliced between the model's root and part 1: identity, scaled by 1.25 for placement entry 0x31, and flattened as the corpse fades
    ActorAnimRig21        rig;                                 // playback of the model's parts 1 to 20; slot 1's boundary and jump flags and pose indices time the states
    ActorAnimRig21        flinchRig;                           // second playback of the same model, started on the flinch motion and mixed over `rig` by `flinchWeight`
    WorldCollisionBody    bodies[ACTOR_01100_BODY_COUNT];      // `ACTOR_01100_BODY_*`; the hand spheres are enabled only for an attack's active frames
    WorldCollisionContact contacts[ACTOR_01100_BODY_COUNT][3]; // contact table of the body at the same index: the root's pushes the model out of the world, a hand's reports a landed attack, the chest's is scanned for the hits taken
    MATRIX                lightMtx;                            // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;                            // storage for the model's `TmdObject::colorMtx`
    u32                   placeIndex;                          // placement index from `Enemy::placeKey`; its low byte goes into bits 8 to 15 of the sound cue ids
    s16                   stateCounter;                        // counter private to the state: frames elapsed or left in most, the turn still to make in `IDLE_TURN`
    s16                   lookYaw;                             // twist of the upper body toward the player, spread over the head (part 4), chest and waist; within +-0x600
    s16                   playerBearing;                       // bearing of the player in the root's frame as last measured, -0x800 to 0x800
    s16                   hp;                                  // hit points left, mirrored into `Enemy::hp`
    s16                   leftArmStretch;                      // lengthening of the left upper arm (part 10) along its axis; a quarter of it thickens the arm
    s16                   rightArmStretch;                     // the same for the right upper arm (part 6)
    s16                   leftShoulderSwell;                   // enlargement of the left shoulder (part 9); part 10 is scaled by the reciprocal, so the arm below keeps its size
    s16                   rightShoulderSwell;                  // the same for the right shoulder (part 5) and part 6
    s16                   recentDamage;                        // damage taken lately: each hit adds its damage and each tick takes 1 off; 0x3D raises a hit's reaction to a flinch and 0x65 to a fall
    u16                   stretchGoal;                         // value a punch ramps its arm stretch to, from the distance to the player as the punch began: 0 within 900 units, 0x2000 beyond 2700
    s8                    hidden;                              // 1 while a model-draw message has the model hidden and its bodies disabled; the tick does nothing meanwhile
    u8                    savedTargetFlags;                    // the enemy's target-node flags from before it was hidden, restored when it is shown
    s8                    flinchWeight;                        // share of `flinchRig`'s pose in the mix, of 0x80; fades in steps of 4 up to 0x40 and back
    s8                    flinchPhase;                         // overlay of the flinch on the body animation (0 off, 1 start `flinchRig`, 2 fade in until its motion ends, 3 fade out)
    s8                    motion;                              // motion the state wants on `rig`
    s8                    startedMotion;                       // `motion` as last started; a difference restarts the slots, so a state repeats a motion by writing 1 here
    u8                    mode;                                // `ACTOR_01100_MODE_*`
    s8                    state;                               // `ACTOR_01100_STATE_*`
    s8                    stateStep;                           // step within `state`; 0 makes the handler set itself up
    s8                    motionEnded;                         // 1 on a tick where `rig`'s slot 1 reached a clip boundary
    u8                    field_BAA;                           // cleared while unaware, stepped as a punch that missed or a spit ends; never read, role unproven
    u8                    reaction;                            // `ACTOR_01100_REACTION_*`
    u8                    blockedFrames;                       // consecutive ticks on which the world contacts pushed the root sideways; 11 keeps `ADVANCE` from punching
    s8                    strideFrame;                         // tick within `ADVANCE`'s walk cycle, -1 as it begins and whenever the clip wraps; the root moves forward on 1 to 0x2E
    u8                    hitFromBehind;                       // 1 when the last hit that drew a twitch, flinch or fall came from behind; picks between the two variants of the flinch, fall, rise and death motions. Spawn states 1 and 2 start it at 0 and 1
    u8                    downState;                           // `ACTOR_01100_DOWN_*`; also keeps `lookYaw` from being eased back to 0 while standing in `ACTOR_01100_MODE_REACTING`
    EffectSpawnArg        hitEffectArg;                        // argument of the hit effect, which is placed on the head (part 4)
    u8                    waterRoom;                           // 1 when spawned in stage 3 area 32: picks the other sound bank and cue variant (bit 22 of the ids) and turns on the ripples and spray
    u8                    sprayFrames;                         // ticks of spray left; 5 from each crossing of the water surface
    u8                    splashPart;                          // model part watched for crossing the water surface, as a state last named it (0 none)
    s8                    entryId;                             // entry id of the placement spawned from; 0x31 is the larger variant
    s16                   splashHeight;                        // water level minus `splashPart`'s view-space height on the last tick; a change of sign is a crossing
    s16                   hitEffectFrames;                     // ticks the hit effect goes on being repeated, every eighth one
    s32                   hitEffectKind;                       // effect of the last hit, from the hit id's first parameter
    s32                   hitCooldown;                         // ticks before another hit counts, from the hit id's second parameter
    u8                    roomNotified;                        // 1 once the death has been reported to the room task, which only stage 5 area 24 is
    u8                    prevMode;                            // `mode` as the tick began, before the state handler ran
} _Actor01100Work;
STATIC_ASSERT_SIZEOF(_Actor01100Work, 0xBCC);

/// Working storage the enemy task borrows from the scratchpad stack around
/// each call of its state handler, and gives back when the call returns.
///
/// The handler is handed the block and passes it on to everything it runs, so
/// nothing in it outlives the call. Besides the vectors, matrix and pose
/// buffers any of them may work in, it carries three values between the
/// per-frame tick and the `_Actor01100Work::state` handler the tick runs: where
/// the enemy is heard from, which the tick measures for the sound cues the
/// handlers queue, and the model part a handler asks the tick to watch for
/// crossing the water surface.
typedef struct {
    VECTOR        vector;      // long vector: the per-axis 4.12 factors a shoulder's matrix columns are scaled by, then the root's world position with 0x320 taken off its Y, where the enemy's colour is sampled
    SVECTOR       shortVector; // short vector: the contact point of a hit, a matrix column while it is scaled, the offset from an arm to the player, the walk's step, the offset an effect is spawned at
    byte          field_18[8]; // never accessed; role unproven
    MATRIX        viewInverse; // transpose of the view coordinate's world matrix, built each tick in the water room; its rotation takes a world offset into the view's frame
    AnimationPose poses[2];    // one part's pose from `_Actor01100Work::rig` and from `flinchRig`, mixed by `flinchWeight` into the part's coordinate
    s16           pan;         // audio pan of the origin of model part 1, measured before the state handler runs; the cues take its low byte
    s16           depth;       // audio depth of the same point, likewise
    u8            splashPart;  // model part the state handler names for the water-surface check: 0 for none or an `ACTOR_01100_PART_*`; cleared before each call
} _Actor01100Scratch;
STATIC_ASSERT_SIZEOF(_Actor01100Scratch, 0x68);

/// A handler of the enemy task: one of the three task states, or one of the
/// `ACTOR_01100_STATE_*` handlers the per-frame task state runs.
///
/// Wider than a `TaskFunc`: the task's callback looks up the enemy record, the
/// work block at `Task::work` and a scratch block borrowed for the call, and
/// hands all four on; the per-frame state passes the same four to the handler
/// of `_Actor01100Work::state`.
typedef void (*_Actor01100StateFunc)(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);

/// Handlers of the enemy task's three states, indexed by `Task::state`: set-up,
/// arming the collision bodies once the CD command queue is idle, and the
/// per-frame tick. Wrapped in a struct so the task's callback can copy the
/// whole table onto its stack by assignment.
typedef struct {
    _Actor01100StateFunc funcs[3];
} _Actor01100TaskStateTable;

/// Handlers of the per-frame tick, indexed by `_Actor01100Work::state`
/// (`ACTOR_01100_STATE_*`). Wrapped in a struct for the same copy by
/// assignment.
typedef struct {
    _Actor01100StateFunc funcs[ACTOR_01100_STATE_FALL_AGAIN + 1];
} _Actor01100StateTable;

/* The loop that arms the two hand bodies walks a scalar byte offset to each
   body's contact table rather than indexing `contacts`: the ROM adds the base
   to the offset on every pass, which loop.c produces for a scalar offset but
   strength-reduces away for an array index. */

extern EnemyParams   Actor01100_D074E8;
extern EnemyParams   Actor01100_D07510;
extern AnimationSet* Actor01100_D15604[23];

/// Actor id the set-up `_actor01100Init` stores for the secondary tasks,
/// which shift it into bits 8-15 of their sound ids.
extern u8 Actor01100_D15670;

/// Pair table the spawn state packs into the collision body's `WorldCollisionBody.key`.
extern DamageAttack Actor01100_D074F8[6];

// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01100_D15660[2];
extern DamageAttack     Actor01100_D074D0[];
extern TaskDesc         Actor01100_D155E0[];
static TmdSource        _gActor01100BruteMossbackBurstArm;
static TmdSource        _gActor01100BruteMossbackBurstHead;

/// Scales column `col` of `m` by `fac` through the GTE's GPF, using `sv` as
/// the working vector.
#define SCALE_COL(m, sv, col, fac)    \
    gte_ReadMatrixColumn(m, col, sv); \
    gte_lddp(fac);                    \
    gte_ldsv(sv);                     \
    gte_gpf12();                      \
    gte_stsv(sv);                     \
    gte_WriteMatrixColumn(sv, m, col)

static void _actor01100Init(Enemy* enemy, Task* task, _Actor01100Work* unusedWork, _Actor01100Scratch* unusedScratch);
static void _actor01100ArmCollisionBodies(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static s32  _actor01100ProcessHits(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100Tick(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100IdleSwingLeft(Enemy* enemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100IdleSwingRight(Enemy* enemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100IdleTurn(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static void _actor01100TrackPlayerLookYaw(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static void _actor01100NoticePlayer(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static void _actor01100FacePlayer(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100PunchLeft(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100PunchRight(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn04DB4(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100Advance(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn05678(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn05CFC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100SpitGlobInit(Task* task);
static void _actor01100SpitGlobFly(Task* task);
static void _actor01100SpitCloudInit(Task* task);
static void _actor01100Exit(Task* task);
static void _actor01100ScaleMatrixColumns(MATRIX* matrix, VECTOR* scale);
static s32  _actor01100BearingToPlayerSlot(const GfxCoord* self, s32 playerSlot);
static s32  _actor01100MeasurePlayerDistanceSquared(const GfxCoord* self);
static void _actor01100Idle(Enemy* unusedEnemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static void _actor01100IdleRest(Enemy* unusedEnemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn07014(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100Flinch(Enemy* unusedEnemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static void Actor01100_Fn07148(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100Rise(Enemy* enemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void _actor01100SpitGlobCountdown(Task* task);
static void _actor01100SpitExit(Task* task);
static void _actor01100SpitCloudTick(Task* task);

/// The enemy task's three states - set-up, the per-frame dispatcher and
/// teardown - which `_actor01100Task` runs by `Task::state`.
static const _Actor01100TaskStateTable Actor01100_D00004 = { {
    _actor01100Init,
    _actor01100ArmCollisionBodies,
    _actor01100Tick,
} };

/// Scale copied onto the stack and passed to `_actor01100ScaleMatrixColumns` when the
/// placement `entryId` is 0x31: 0x1400 on each axis.
static const VECTOR Actor01100_D00010 = { 0x1400, 0x1400, 0x1400, 0 };

extern DamageAttack Actor01100_D074F8[6];
static s32          _actor01100SetModelDraw(Task* task, s32 unusedMessageId, s32 drawEnabled, s32 unusedSecondArg);
static void         _actor01100Task(Task* task);
static void         _actor01100SpitGlobTask(Task* task);
static void         _actor01100SpitCloudTask(Task* task);

DamageAttack Actor01100_D074D0[6] = {
    { 0, 0 },
    { 8, 0 },
    { 8, 0 },
    { 20, 7 },
    { 20, 7 },
    { 10, 3 },
};

EnemyParams Actor01100_D074E8 = { Actor01100_D074D0, 280, 152, 102, 5, 200, 10, 100, 20 };

DamageAttack Actor01100_D074F8[6] = { { 0, 0 }, { 10, 0 }, { 10, 0 }, { 30, 7 }, { 30, 7 }, { 15, 1 } };

EnemyParams Actor01100_D07510 = { Actor01100_D074F8, 450, 204, 152, 6, 200, 5, 100, 10 };

static TmdBone _gActor01100BruteMossbackBodySkeleton[21] = {
#include "assets/brute_mossback_body_skeleton.inc"
};

static u32 _gActor01100BruteMossbackBodyPartVerts[21] = {
#include "assets/brute_mossback_body_partVerts.inc"
};

static SVECTOR _gActor01100BruteMossbackBodyVerts[303] = {
#include "assets/brute_mossback_body_verts.inc"
};

static SVECTOR _gActor01100BruteMossbackBodyNormals[303] = {
#include "assets/brute_mossback_body_normals.inc"
};

static u32 _gActor01100BruteMossbackBodyStream[4084] = {
#include "assets/brute_mossback_body_stream.inc"
};

static TmdSource _gActor01100BruteMossbackBody = {
    0,
    19516,
    9080,
    21,
    _gActor01100BruteMossbackBodyPartVerts,
    _gActor01100BruteMossbackBodyVerts,
    _gActor01100BruteMossbackBodyNormals,
    _gActor01100BruteMossbackBodySkeleton,
    _gActor01100BruteMossbackBodyStream,
};

static TmdBone _gActor01100BruteMossbackBurstLegSkeleton[1] = {
#include "assets/brute_mossback_burst_leg_skeleton.inc"
};

static u32 _gActor01100BruteMossbackBurstLegPartVerts[1] = {
#include "assets/brute_mossback_burst_leg_partVerts.inc"
};

static SVECTOR _gActor01100BruteMossbackBurstLegVerts[24] = {
#include "assets/brute_mossback_burst_leg_verts.inc"
};

static SVECTOR _gActor01100BruteMossbackBurstLegNormals[25] = {
#include "assets/brute_mossback_burst_leg_normals.inc"
};

static u32 _gActor01100BruteMossbackBurstLegStream[212] = {
#include "assets/brute_mossback_burst_leg_stream.inc"
};

static TmdSource _gActor01100BruteMossbackBurstLeg = {
    0,
    1392,
    0,
    1,
    _gActor01100BruteMossbackBurstLegPartVerts,
    _gActor01100BruteMossbackBurstLegVerts,
    _gActor01100BruteMossbackBurstLegNormals,
    _gActor01100BruteMossbackBurstLegSkeleton,
    _gActor01100BruteMossbackBurstLegStream,
};

static TmdBone _gActor01100BruteMossbackBurstArmSkeleton[1] = {
#include "assets/brute_mossback_burst_arm_skeleton.inc"
};

static u32 _gActor01100BruteMossbackBurstArmPartVerts[1] = {
#include "assets/brute_mossback_burst_arm_partVerts.inc"
};

static SVECTOR _gActor01100BruteMossbackBurstArmVerts[38] = {
#include "assets/brute_mossback_burst_arm_verts.inc"
};

static SVECTOR _gActor01100BruteMossbackBurstArmNormals[49] = {
#include "assets/brute_mossback_burst_arm_normals.inc"
};

static u32 _gActor01100BruteMossbackBurstArmStream[361] = {
#include "assets/brute_mossback_burst_arm_stream.inc"
};

static TmdSource _gActor01100BruteMossbackBurstArm = {
    0,
    2432,
    0,
    1,
    _gActor01100BruteMossbackBurstArmPartVerts,
    _gActor01100BruteMossbackBurstArmVerts,
    _gActor01100BruteMossbackBurstArmNormals,
    _gActor01100BruteMossbackBurstArmSkeleton,
    _gActor01100BruteMossbackBurstArmStream,
};

static TmdBone _gActor01100BruteMossbackBurstHeadSkeleton[1] = {
#include "assets/brute_mossback_burst_head_skeleton.inc"
};

static u32 _gActor01100BruteMossbackBurstHeadPartVerts[1] = {
#include "assets/brute_mossback_burst_head_partVerts.inc"
};

static SVECTOR _gActor01100BruteMossbackBurstHeadVerts[59] = {
#include "assets/brute_mossback_burst_head_verts.inc"
};

static SVECTOR _gActor01100BruteMossbackBurstHeadNormals[66] = {
#include "assets/brute_mossback_burst_head_normals.inc"
};

static u32 _gActor01100BruteMossbackBurstHeadStream[493] = {
#include "assets/brute_mossback_burst_head_stream.inc"
};

static TmdSource _gActor01100BruteMossbackBurstHead = {
    0,
    3396,
    0,
    1,
    _gActor01100BruteMossbackBurstHeadPartVerts,
    _gActor01100BruteMossbackBurstHeadVerts,
    _gActor01100BruteMossbackBurstHeadNormals,
    _gActor01100BruteMossbackBurstHeadSkeleton,
    _gActor01100BruteMossbackBurstHeadStream,
};

static AnimationPackedPose _gActor01100Actor101100Animation0E8CCBank1[4] = {
#include "assets/actor_101100_animation_0E8CC_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation0E8CCBank4[66] = {
#include "assets/actor_101100_animation_0E8CC_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation0E8CCRecords[154] = {
#include "assets/actor_101100_animation_0E8CC_records.inc"
};

static u16 _gActor01100Actor101100Animation0E8CCIndices[22] = {
#include "assets/actor_101100_animation_0E8CC_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation0E8CC = {
    _gActor01100Actor101100Animation0E8CCRecords,
    _gActor01100Actor101100Animation0E8CCIndices,
    { NULL, _gActor01100Actor101100Animation0E8CCBank1, NULL, NULL, _gActor01100Actor101100Animation0E8CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation0EE30Bank1[4] = {
#include "assets/actor_101100_animation_0EE30_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation0EE30Bank4[127] = {
#include "assets/actor_101100_animation_0EE30_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation0EE30Records[185] = {
#include "assets/actor_101100_animation_0EE30_records.inc"
};

static u16 _gActor01100Actor101100Animation0EE30Indices[22] = {
#include "assets/actor_101100_animation_0EE30_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation0EE30 = {
    _gActor01100Actor101100Animation0EE30Records,
    _gActor01100Actor101100Animation0EE30Indices,
    { NULL, _gActor01100Actor101100Animation0EE30Bank1, NULL, NULL, _gActor01100Actor101100Animation0EE30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation0F300Bank1[3] = {
#include "assets/actor_101100_animation_0F300_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation0F300Bank4[107] = {
#include "assets/actor_101100_animation_0F300_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation0F300Records[171] = {
#include "assets/actor_101100_animation_0F300_records.inc"
};

static u16 _gActor01100Actor101100Animation0F300Indices[22] = {
#include "assets/actor_101100_animation_0F300_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation0F300 = {
    _gActor01100Actor101100Animation0F300Records,
    _gActor01100Actor101100Animation0F300Indices,
    { NULL, _gActor01100Actor101100Animation0F300Bank1, NULL, NULL, _gActor01100Actor101100Animation0F300Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation0F5A8Bank1[4] = {
#include "assets/actor_101100_animation_0F5A8_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation0F5A8Bank4[34] = {
#include "assets/actor_101100_animation_0F5A8_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation0F5A8Records[103] = {
#include "assets/actor_101100_animation_0F5A8_records.inc"
};

static u16 _gActor01100Actor101100Animation0F5A8Indices[22] = {
#include "assets/actor_101100_animation_0F5A8_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation0F5A8 = {
    _gActor01100Actor101100Animation0F5A8Records,
    _gActor01100Actor101100Animation0F5A8Indices,
    { NULL, _gActor01100Actor101100Animation0F5A8Bank1, NULL, NULL, _gActor01100Actor101100Animation0F5A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation0FDD8Bank1[5] = {
#include "assets/actor_101100_animation_0FDD8_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation0FDD8Bank4[207] = {
#include "assets/actor_101100_animation_0FDD8_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation0FDD8Records[281] = {
#include "assets/actor_101100_animation_0FDD8_records.inc"
};

static u16 _gActor01100Actor101100Animation0FDD8Indices[22] = {
#include "assets/actor_101100_animation_0FDD8_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation0FDD8 = {
    _gActor01100Actor101100Animation0FDD8Records,
    _gActor01100Actor101100Animation0FDD8Indices,
    { NULL, _gActor01100Actor101100Animation0FDD8Bank1, NULL, NULL, _gActor01100Actor101100Animation0FDD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation1050CBank1[10] = {
#include "assets/actor_101100_animation_1050C_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation1050CBank4[173] = {
#include "assets/actor_101100_animation_1050C_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation1050CRecords[237] = {
#include "assets/actor_101100_animation_1050C_records.inc"
};

static u16 _gActor01100Actor101100Animation1050CIndices[22] = {
#include "assets/actor_101100_animation_1050C_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation1050C = {
    _gActor01100Actor101100Animation1050CRecords,
    _gActor01100Actor101100Animation1050CIndices,
    { NULL, _gActor01100Actor101100Animation1050CBank1, NULL, NULL, _gActor01100Actor101100Animation1050CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation10B0CBank1[5] = {
#include "assets/actor_101100_animation_10B0C_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation10B0CBank4[144] = {
#include "assets/actor_101100_animation_10B0C_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation10B0CRecords[204] = {
#include "assets/actor_101100_animation_10B0C_records.inc"
};

static u16 _gActor01100Actor101100Animation10B0CIndices[22] = {
#include "assets/actor_101100_animation_10B0C_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation10B0C = {
    _gActor01100Actor101100Animation10B0CRecords,
    _gActor01100Actor101100Animation10B0CIndices,
    { NULL, _gActor01100Actor101100Animation10B0CBank1, NULL, NULL, _gActor01100Actor101100Animation10B0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation10FE0Bank1[2] = {
#include "assets/actor_101100_animation_10FE0_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation10FE0Bank4[112] = {
#include "assets/actor_101100_animation_10FE0_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation10FE0Records[170] = {
#include "assets/actor_101100_animation_10FE0_records.inc"
};

static u16 _gActor01100Actor101100Animation10FE0Indices[22] = {
#include "assets/actor_101100_animation_10FE0_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation10FE0 = {
    _gActor01100Actor101100Animation10FE0Records,
    _gActor01100Actor101100Animation10FE0Indices,
    { NULL, _gActor01100Actor101100Animation10FE0Bank1, NULL, NULL, _gActor01100Actor101100Animation10FE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation116DCBank1[18] = {
#include "assets/actor_101100_animation_116DC_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation116DCBank4[137] = {
#include "assets/actor_101100_animation_116DC_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation116DCRecords[235] = {
#include "assets/actor_101100_animation_116DC_records.inc"
};

static u16 _gActor01100Actor101100Animation116DCIndices[22] = {
#include "assets/actor_101100_animation_116DC_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation116DC = {
    _gActor01100Actor101100Animation116DCRecords,
    _gActor01100Actor101100Animation116DCIndices,
    { NULL, _gActor01100Actor101100Animation116DCBank1, NULL, NULL, _gActor01100Actor101100Animation116DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation11C90Bank1[15] = {
#include "assets/actor_101100_animation_11C90_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation11C90Bank4[122] = {
#include "assets/actor_101100_animation_11C90_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation11C90Records[177] = {
#include "assets/actor_101100_animation_11C90_records.inc"
};

static u16 _gActor01100Actor101100Animation11C90Indices[22] = {
#include "assets/actor_101100_animation_11C90_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation11C90 = {
    _gActor01100Actor101100Animation11C90Records,
    _gActor01100Actor101100Animation11C90Indices,
    { NULL, _gActor01100Actor101100Animation11C90Bank1, NULL, NULL, _gActor01100Actor101100Animation11C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation12300Bank1[10] = {
#include "assets/actor_101100_animation_12300_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation12300Bank4[149] = {
#include "assets/actor_101100_animation_12300_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation12300Records[212] = {
#include "assets/actor_101100_animation_12300_records.inc"
};

static u16 _gActor01100Actor101100Animation12300Indices[22] = {
#include "assets/actor_101100_animation_12300_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation12300 = {
    _gActor01100Actor101100Animation12300Records,
    _gActor01100Actor101100Animation12300Indices,
    { NULL, _gActor01100Actor101100Animation12300Bank1, NULL, NULL, _gActor01100Actor101100Animation12300Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation126F4Bank1[2] = {
#include "assets/actor_101100_animation_126F4_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation126F4Bank4[86] = {
#include "assets/actor_101100_animation_126F4_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation126F4Records[140] = {
#include "assets/actor_101100_animation_126F4_records.inc"
};

static u16 _gActor01100Actor101100Animation126F4Indices[22] = {
#include "assets/actor_101100_animation_126F4_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation126F4 = {
    _gActor01100Actor101100Animation126F4Records,
    _gActor01100Actor101100Animation126F4Indices,
    { NULL, _gActor01100Actor101100Animation126F4Bank1, NULL, NULL, _gActor01100Actor101100Animation126F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation12DA8Bank1[20] = {
#include "assets/actor_101100_animation_12DA8_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation12DA8Bank4[136] = {
#include "assets/actor_101100_animation_12DA8_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation12DA8Records[212] = {
#include "assets/actor_101100_animation_12DA8_records.inc"
};

static u16 _gActor01100Actor101100Animation12DA8Indices[22] = {
#include "assets/actor_101100_animation_12DA8_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation12DA8 = {
    _gActor01100Actor101100Animation12DA8Records,
    _gActor01100Actor101100Animation12DA8Indices,
    { NULL, _gActor01100Actor101100Animation12DA8Bank1, NULL, NULL, _gActor01100Actor101100Animation12DA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation13354Bank1[11] = {
#include "assets/actor_101100_animation_13354_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation13354Bank4[122] = {
#include "assets/actor_101100_animation_13354_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation13354Records[187] = {
#include "assets/actor_101100_animation_13354_records.inc"
};

static u16 _gActor01100Actor101100Animation13354Indices[22] = {
#include "assets/actor_101100_animation_13354_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation13354 = {
    _gActor01100Actor101100Animation13354Records,
    _gActor01100Actor101100Animation13354Indices,
    { NULL, _gActor01100Actor101100Animation13354Bank1, NULL, NULL, _gActor01100Actor101100Animation13354Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation13A9CBank1[16] = {
#include "assets/actor_101100_animation_13A9C_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation13A9CBank4[170] = {
#include "assets/actor_101100_animation_13A9C_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation13A9CRecords[227] = {
#include "assets/actor_101100_animation_13A9C_records.inc"
};

static u16 _gActor01100Actor101100Animation13A9CIndices[22] = {
#include "assets/actor_101100_animation_13A9C_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation13A9C = {
    _gActor01100Actor101100Animation13A9CRecords,
    _gActor01100Actor101100Animation13A9CIndices,
    { NULL, _gActor01100Actor101100Animation13A9CBank1, NULL, NULL, _gActor01100Actor101100Animation13A9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation143D0Bank1[14] = {
#include "assets/actor_101100_animation_143D0_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation143D0Bank4[227] = {
#include "assets/actor_101100_animation_143D0_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation143D0Records[299] = {
#include "assets/actor_101100_animation_143D0_records.inc"
};

static u16 _gActor01100Actor101100Animation143D0Indices[22] = {
#include "assets/actor_101100_animation_143D0_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation143D0 = {
    _gActor01100Actor101100Animation143D0Records,
    _gActor01100Actor101100Animation143D0Indices,
    { NULL, _gActor01100Actor101100Animation143D0Bank1, NULL, NULL, _gActor01100Actor101100Animation143D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation14580Bank1[2] = {
#include "assets/actor_101100_animation_14580_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation14580Bank4[18] = {
#include "assets/actor_101100_animation_14580_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation14580Records[63] = {
#include "assets/actor_101100_animation_14580_records.inc"
};

static u16 _gActor01100Actor101100Animation14580Indices[22] = {
#include "assets/actor_101100_animation_14580_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation14580 = {
    _gActor01100Actor101100Animation14580Records,
    _gActor01100Actor101100Animation14580Indices,
    { NULL, _gActor01100Actor101100Animation14580Bank1, NULL, NULL, _gActor01100Actor101100Animation14580Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation14730Bank1[2] = {
#include "assets/actor_101100_animation_14730_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation14730Bank4[18] = {
#include "assets/actor_101100_animation_14730_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation14730Records[63] = {
#include "assets/actor_101100_animation_14730_records.inc"
};

static u16 _gActor01100Actor101100Animation14730Indices[22] = {
#include "assets/actor_101100_animation_14730_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation14730 = {
    _gActor01100Actor101100Animation14730Records,
    _gActor01100Actor101100Animation14730Indices,
    { NULL, _gActor01100Actor101100Animation14730Bank1, NULL, NULL, _gActor01100Actor101100Animation14730Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation14AF8Bank1[5] = {
#include "assets/actor_101100_animation_14AF8_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation14AF8Bank4[86] = {
#include "assets/actor_101100_animation_14AF8_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation14AF8Records[120] = {
#include "assets/actor_101100_animation_14AF8_records.inc"
};

static u16 _gActor01100Actor101100Animation14AF8Indices[22] = {
#include "assets/actor_101100_animation_14AF8_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation14AF8 = {
    _gActor01100Actor101100Animation14AF8Records,
    _gActor01100Actor101100Animation14AF8Indices,
    { NULL, _gActor01100Actor101100Animation14AF8Bank1, NULL, NULL, _gActor01100Actor101100Animation14AF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation14F0CBank1[5] = {
#include "assets/actor_101100_animation_14F0C_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation14F0CBank4[95] = {
#include "assets/actor_101100_animation_14F0C_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation14F0CRecords[130] = {
#include "assets/actor_101100_animation_14F0C_records.inc"
};

static u16 _gActor01100Actor101100Animation14F0CIndices[22] = {
#include "assets/actor_101100_animation_14F0C_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation14F0C = {
    _gActor01100Actor101100Animation14F0CRecords,
    _gActor01100Actor101100Animation14F0CIndices,
    { NULL, _gActor01100Actor101100Animation14F0CBank1, NULL, NULL, _gActor01100Actor101100Animation14F0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation15244Bank1[4] = {
#include "assets/actor_101100_animation_15244_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation15244Bank4[51] = {
#include "assets/actor_101100_animation_15244_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation15244Records[122] = {
#include "assets/actor_101100_animation_15244_records.inc"
};

static u16 _gActor01100Actor101100Animation15244Indices[22] = {
#include "assets/actor_101100_animation_15244_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation15244 = {
    _gActor01100Actor101100Animation15244Records,
    _gActor01100Actor101100Animation15244Indices,
    { NULL, _gActor01100Actor101100Animation15244Bank1, NULL, NULL, _gActor01100Actor101100Animation15244Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01100Actor101100Animation155B8Bank1[5] = {
#include "assets/actor_101100_animation_155B8_bank1.inc"
};

static AnimationPackedRotation _gActor01100Actor101100Animation155B8Bank4[54] = {
#include "assets/actor_101100_animation_155B8_bank4.inc"
};

static AnimationRecord _gActor01100Actor101100Animation155B8Records[131] = {
#include "assets/actor_101100_animation_155B8_records.inc"
};

static u16 _gActor01100Actor101100Animation155B8Indices[22] = {
#include "assets/actor_101100_animation_155B8_indices.inc"
};

static AnimationSet _gActor01100Actor101100Animation155B8 = {
    _gActor01100Actor101100Animation155B8Records,
    _gActor01100Actor101100Animation155B8Indices,
    { NULL, _gActor01100Actor101100Animation155B8Bank1, NULL, NULL, _gActor01100Actor101100Animation155B8Bank4, NULL, NULL, NULL },
};

TaskDesc Actor01100_D155E0[3] = {
    { { { TASK_BODY_TMD, 96 } }, _actor01100Task, { .model = &_gActor01100BruteMossbackBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor01100SpitGlobTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, _actor01100SpitCloudTask, { .value = 0 } },
};

AnimationSet* Actor01100_D15604[23] = {
    NULL,
    &_gActor01100Actor101100Animation0E8CC,
    &_gActor01100Actor101100Animation0EE30,
    &_gActor01100Actor101100Animation0F300,
    &_gActor01100Actor101100Animation0F5A8,
    &_gActor01100Actor101100Animation0FDD8,
    &_gActor01100Actor101100Animation1050C,
    &_gActor01100Actor101100Animation10B0C,
    &_gActor01100Actor101100Animation10FE0,
    &_gActor01100Actor101100Animation116DC,
    &_gActor01100Actor101100Animation11C90,
    &_gActor01100Actor101100Animation12300,
    &_gActor01100Actor101100Animation126F4,
    &_gActor01100Actor101100Animation12DA8,
    &_gActor01100Actor101100Animation13354,
    &_gActor01100Actor101100Animation13A9C,
    &_gActor01100Actor101100Animation143D0,
    &_gActor01100Actor101100Animation14580,
    &_gActor01100Actor101100Animation14730,
    &_gActor01100Actor101100Animation14AF8,
    &_gActor01100Actor101100Animation14F0C,
    &_gActor01100Actor101100Animation15244,
    &_gActor01100Actor101100Animation155B8,
};

TaskMessageEntry Actor01100_D15660[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor01100SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8 Actor01100_D15670;

static __inline__ void _actor01100SetSlotRates(_Actor01100Work* work, s8 rate);
static __inline__ s32  _actor01100FindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts);
static __inline__ s32  _actor01100PushOut(GfxCoord* coord, const WorldCollisionContact* contacts);
static __inline__ void _actor01100DisableHandBodies(_Actor01100Work* work);
static void            _actor01100UpdateAwareness(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void            _actor01100ApplyPoseAdjustments(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static __inline__ s32  _actor01100BearingToPlayer(const GfxCoord* self);
static __inline__ s32  _actor01100DistSqToPlayer(const GfxCoord* self);
static __inline__ u8*  _actor01100ReadScratchCursor(void);
static __inline__ void _actor01100WriteScratchCursor(u8* cursor);
static __inline__ void _actor01100ScaleForwardAxis(const MATRIX* matrix, SVECTOR* scaledForward, s32 distance);
static __inline__ void _actor01100SpawnBurstPart(Task* task, TmdSource* model);
static void            _actor01100StepForward(GfxCoord* coord, _Actor01100Scratch* scratch, s32 distance);
static void            Actor01100_Fn06C0C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void            Actor01100_Fn06D3C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);

#include "../../shared/actor_contacts_find_push.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

/// Sets both animation rigs' part-slot rates, leaving their root slots untouched.
///
/// `rate` is a signed step in sixteenths of a frame per tick
/// (`ANIMATION_RATE_ONE` advances one frame); all 20 part slots are updated.
static __inline__ void _actor01100SetSlotRates(_Actor01100Work* work, s8 rate)
{
    AnimationSlot* slot;
    s32            partIndex;

    for (partIndex = 1; partIndex < ARRAY_SIZE(work->rig.slots); partIndex++) {
        slot       = &work->rig.slots[partIndex];
        slot->rate = rate;
        slot       = &work->flinchRig.slots[partIndex];
        slot->rate = rate;
    }
}

/// Allocates and initializes the Mossback's model, animation rigs and enemy work.
///
/// Queues the normal or water sound bank once per scene. The work belongs to the
/// task; allocation failure exits immediately. The large placement variant gets
/// its own parameters and a 1.25 model scale. Dead spawn variants enter the death
/// state with accelerated animation. The supplied work and scratch are unused.
static void _actor01100Init(Enemy* enemy, Task* task, _Actor01100Work* unusedWork, _Actor01100Scratch* unusedScratch)
{
    // Retain the high-half spawn behavior; the low half is not used by this actor.
    enum { ACTOR_01100_SPAWN_BEHAVIOR_MASK = 0xFFFF0000 };
    enum {
        ACTOR_01100_SOUND_FILE_GROUP        = 10,
        ACTOR_01100_SOUND_FILE_HUNDREDS     = 11,
        ACTOR_01100_SOUND_FILE_DRY          = 1,
        ACTOR_01100_SOUND_FILE_WATER        = 2,
        ACTOR_01100_SPAWN_DEAD_FRONT        = 1,
        ACTOR_01100_SPAWN_DEAD_BEHIND       = 2,
        ACTOR_01100_HIT_EFFECT_SERIES_COUNT = 3
    };
    _Actor01100Work*  work;
    TmdObject*        model;
    GfxCoord*         root;
    SceneCombatState* combatState;
    u8                soundFileKey[4];
    u8                soundLoadArgs[4];
    VECTOR            scale;
    s8                entryId;
    u32               placeIndex;
    u32               locationWord;
    u16               hp;
    s32               partIndex;
    GfxCoord*         modelCoords;

    model                  = task->extra.tmd;
    root                   = model->coords;
    task->spawnArg1.value &= ACTOR_01100_SPAWN_BEHAVIOR_MASK;
    work                   = memCalloc(sizeof(_Actor01100Work), 0);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }

    // Select the scene sound file; key byte 1 is ignored by the CD queue.
    locationWord     = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
    locationWord    &= GAME_LOCATION_STAGE_AREA_MASK;
    soundFileKey[2]  = ACTOR_01100_SOUND_FILE_GROUP;
    soundLoadArgs[0] = ACTOR_01100_SOUND_FILE_HUNDREDS;
    soundFileKey[3]  = 0;
    soundLoadArgs[3] = 0;
    soundLoadArgs[2] = 0;
    soundLoadArgs[1] = CD_COMMAND_LOAD_DEFAULT;
    if (locationWord == GAME_LOCATION_KEY(3, 32, 0, 0)) {
        soundFileKey[0] = ACTOR_01100_SOUND_FILE_WATER;
        work->waterRoom = 1;
    } else {
        work->waterRoom = 0;
        soundFileKey[0] = ACTOR_01100_SOUND_FILE_DRY;
    }

    combatState = &gSceneCombatState;
    if (combatState->enemySoundBankQueued == 0) {
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, soundFileKey, soundLoadArgs);
        combatState->enemySoundBankQueued = 1;
    }

    task->work    = work;
    entryId       = enemy->place->entryId;
    work->entryId = entryId;
    if (entryId == ACTOR_01100_ENTRY_LARGE) {
        enemy->param = &Actor01100_D07510;
    } else {
        enemy->param = &Actor01100_D074E8;
    }

    placeIndex                = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    work->placeIndex          = placeIndex;
    *(s32*)&Actor01100_D15670 = placeIndex;
    model->lightMtx           = &work->lightMtx;
    model->colorMtx           = &work->colorMtx;
    animationInitContext(&work->rig.anim, Actor01100_D15604, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->flinchRig.anim, Actor01100_D15604, model, work->flinchRig.poses, work->flinchRig.slots);
    work->startedMotion = ACTOR_01100_MOTION_IDLE;
    work->motion        = ACTOR_01100_MOTION_IDLE;

    // Insert the scale coordinate above all animated model parts.
    gfxSetRotIdentity(&work->scaleCoord.coord);
    if (work->entryId == ACTOR_01100_ENTRY_LARGE) {
        scale = Actor01100_D00010;
        _actor01100ScaleMatrixColumns(&work->scaleCoord.coord, &scale);
    }
    work->scaleCoord.coord.t[0]   = 0;
    work->scaleCoord.coord.t[1]   = 0;
    work->scaleCoord.coord.t[2]   = 0;
    work->scaleCoord.parent       = root;
    work->scaleCoord.composeStamp = GRAPHICS_COORD_DIRTY;

    hp                                                          = enemy->param->hpMax;
    work->hp                                                    = hp;
    enemy->hp                                                   = hp;
    task->extra.tmd->coords[ACTOR_01100_PART_BODY].parent       = &work->scaleCoord;
    task->extra.tmd->coords[ACTOR_01100_PART_BODY].composeStamp = GRAPHICS_COORD_DIRTY;

    partIndex = ACTOR_01100_PART_BODY;
    do {
        animationResetSlot(&work->rig.anim, partIndex, work->motion);
        animationResetSlot(&work->flinchRig.anim, partIndex, work->motion);
        partIndex++;
    } while (partIndex < ARRAY_SIZE(work->rig.slots));

    switch (enemy->spawnState) {
        case ACTOR_01100_SPAWN_DEAD_FRONT:
            work->hitFromBehind = 0;
            work->state         = ACTOR_01100_STATE_DEATH;
            _actor01100SetSlotRates(work, SCHAR_MAX);
            break;
        case ACTOR_01100_SPAWN_DEAD_BEHIND:
            work->hitFromBehind = 1;
            work->state         = ACTOR_01100_STATE_DEATH;
            _actor01100SetSlotRates(work, SCHAR_MAX);
            break;
    }

    sceneAcquireBattleRef(0);
    modelCoords                   = task->extra.tmd->coords;
    work->hitEffectArg.spawnArgLo = 0x400;
    work->hitEffectArg.spawnArgHi = ACTOR_01100_HIT_EFFECT_SERIES_COUNT;
    work->hitEffectArg.coord      = modelCoords + ACTOR_01100_PART_HEAD;
    task->state++;
}

/// Links the Mossback's root, chest and two hand spheres after its sound load finishes.
///
/// Each sphere borrows its corresponding three-contact row from the task work.
/// The root tests the world, the chest receives attacks and the hands start with
/// their tests disabled. Installs message and exit callbacks before advancing
/// the task to its per-frame state. Requires the model and initialized work.
static void _actor01100ArmCollisionBodies(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    enum { ACTOR_01100_CHEST_BODY_ID = 11 };
    WorldCollisionBody* body;
    s32                 handRadius;
    s32                 contactRowByteOffset;
    s32                 handIndex;
    s32                 handPart;

    if ((u16)cdCmdIsIdle()) {
        worldTargetLinkNode(&enemy->node);
        body                   = &work->bodies[ACTOR_01100_BODY_ROOT];
        body->coord            = task->extra.tmd->coords;
        body->context.contacts = &work->contacts[ACTOR_01100_BODY_ROOT][0];
        body->pos.vx           = 0;
        body->pos.vy           = -0x1D8;
        body->pos.vz           = 0;
        body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
        body->radius           = 0x258;
        body->flags            = WORLD_COLLISION_BODY_SPHERE;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
        body->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts[0]), 0);

        body                   = &work->bodies[ACTOR_01100_BODY_CHEST];
        body->coord            = &task->extra.tmd->coords[ACTOR_01100_PART_CHEST];
        body->context.contacts = &work->contacts[ACTOR_01100_BODY_CHEST][0];
        body->pos.vx           = 0;
        body->pos.vy           = 0;
        body->pos.vz           = 0;
        body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_01100_CHEST_BODY_ID;
        body->radius           = 0x1C2;
        body->flags            = WORLD_COLLISION_BODY_SPHERE;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
        body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts[0]), 0);
        body->pos.vx = 0;
        body->pos.vy = -0xC8;
        body->pos.vz = 0xC8;

        // Hand spheres stay linked but their tests are armed only during attacks.
        handIndex            = 0;
        handRadius           = 0x12C;
        contactRowByteOffset = OFFSET_OF(_Actor01100Work, contacts[ACTOR_01100_BODY_LEFT_HAND]);
        body                 = &work->bodies[ACTOR_01100_BODY_LEFT_HAND];
        do {
            handPart = ACTOR_01100_PART_RIGHT_HAND;
            if (handIndex == 0) {
                handPart = ACTOR_01100_PART_LEFT_HAND;
            }
            body->coord            = &task->extra.tmd->coords[handPart];
            body->context.contacts = (WorldCollisionContact*)((u8*)work + contactRowByteOffset);
            do {
                if (handIndex == 0) {
                    body->pos.vx = -0x12C;
                } else {
                    body->pos.vx = handRadius;
                }
                body->pos.vy = 0;
                body->pos.vz = 0;
                body->radius = handRadius;
                body->key    = damagePackEnemyAttackKey(enemy, ACTOR_01100_ATTACK_IDLE_LEFT);
                body->flags  = WORLD_COLLISION_BODY_SPHERE;
                worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
                body->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts[0]), 0);
                contactRowByteOffset += sizeof(work->contacts[0]);
                handIndex++;
                body = &work->bodies[handIndex + 1];
            } while (0);
        } while (handIndex < ACTOR_01100_BODY_RIGHT_HAND - ACTOR_01100_BODY_LEFT_HAND + 1);

        enemy->recs            = &work->contacts[ACTOR_01100_BODY_CHEST][0];
        task->exitCallback     = _actor01100Exit;
        task->extra.tmd->flags = (u16)(task->extra.tmd->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW);
        task->msgTable         = Actor01100_D15660;
        task->state++;
        enemy->reactionFlags = 0;
    }
}

/// Returns the first attack key in a three-record chest contact table.
///
/// Stops at the first zero key even if a later record is occupied. On success,
/// copies the contact's XYZ into `hitPoint`, leaving its fourth halfword intact;
/// otherwise returns zero without writing the point. Both buffers must be live.
static __inline__ s32 _actor01100FindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts)
{
    enum { ACTOR_01100_ATTACK_CONTACT_COUNT = 3 };
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ACTOR_01100_ATTACK_CONTACT_COUNT; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPoint->vx = contacts[contactIndex].point.vx;
            hitPoint->vy = contacts[contactIndex].point.vy;
            hitPoint->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Adds a 16.16 correction's sign to a translation when its low half is nonzero.
///
/// The caller has already added the signed integer half. Negative fractions
/// therefore subtract an extra unit from that arithmetic floor, as retained by
/// the grid pushback behavior. `translation` is one writable MATRIX translation
/// component; the addition must fit its signed word. No pointer is retained.
static __inline__ void _actor01100ApplyFractionalPush(s32 correctionWord, long* translation)
{
    enum { ACTOR_01100_PUSH_FRACTION_MASK = 0xFFFF };
    if (correctionWord & ACTOR_01100_PUSH_FRACTION_MASK) {
        if (correctionWord > 0) {
            *translation += 1;
        } else {
            *translation -= 1;
        }
    }
}

/// Applies a three-record root contact table's grid correction to a coordinate.
///
/// Adds the signed 16.16 integer halves to XYZ and a further sign unit to X/Z
/// when a fraction remains; negative fractions step below the integer floor.
/// Then adds a 128-unit Y bias, including with no grid hit. Both callers save
/// and restore Y, keeping only X/Z, and invalidate the cache on horizontal correction.
/// Returns 1 for nonzero resolved X/Z, otherwise 0. Does nothing only when
/// the saved movement-freeze byte equals 1. Requires live inputs in the room
/// frame, signed-word translation sums and initialized scratch space (72-byte
/// peak). Retains no pointers; preserves rotation and the composition stamp.
static __inline__ s32 _actor01100PushOut(GfxCoord* coord, const WorldCollisionContact* contacts)
{
    enum { ACTOR_01100_ROOT_CONTACT_COUNT    = 3,
           ACTOR_01100_ROOT_PUSH_HEIGHT_BIAS = 128 };
    ActorContactPushScratch* previousCursor;
    ActorContactPushScratch* scratch;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    // Both cursor views address the newly reserved correction block.
    previousCursor = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
    scratch        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
    scratch->moved = 0;
    // Apply integer corrections, then the retained fractional X/Z sign step.
    if (worldCollisionResolvePushback(contacts, &scratch->delta, ACTOR_01100_ROOT_CONTACT_COUNT, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        coord->coord.t[0] += previousCursor[-1].delta.fixed.vx.halves.integer;
        coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
        coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
        _actor01100ApplyFractionalPush(previousCursor[-1].delta.fixed.vx.word, &coord->coord.t[0]);
        _actor01100ApplyFractionalPush(scratch->delta.fixed.vz.word, &coord->coord.t[2]);
    }
    coord->coord.t[1] += ACTOR_01100_ROOT_PUSH_HEIGHT_BIAS;
    if ((scratch->delta.fixed.vx.word != 0) || (scratch->delta.fixed.vz.word != 0)) {
        scratch->moved = 1;
    }
    // No reservation or call intervenes before reading the released result.
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    return scratch->moved;
}

/// Disables grid and pair tests on the two hand attack bodies.
///
/// Their list links, existing contacts and other flags remain intact.
static __inline__ void _actor01100DisableHandBodies(_Actor01100Work* work)
{
    s32 handOffset;

    for (handOffset = 0; handOffset <= ACTOR_01100_BODY_RIGHT_HAND - ACTOR_01100_BODY_LEFT_HAND; handOffset++) {
        WorldCollisionBody* body = &work->bodies[handOffset + ACTOR_01100_BODY_LEFT_HAND];
        body->flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
}

/// Applies incoming attacks and status damage, stages reactions, and consumes body contacts.
///
/// Returns 1 after positive HP damage passes the cooldown, otherwise 0. The first
/// attack contact supplies damage attributes; the first nonzero chest contact
/// independently supplies the source-selector and attachment bits. Distance is
/// measured to the registered player even when the source selects the companion.
/// Resting amplifies damage. Hit direction, status and down-state select recoil,
/// fall, renewed fall or death, while HP is mirrored into the enemy record.
///
/// Every call also resolves root pushback, retains only its X/Z correction,
/// clears all four contact tables and advances buildup. Requires initialized enemy
/// work, composed caches in one frame and live per-call scratch storage.
static s32 _actor01100ProcessHits(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{

/// Requests the hit cue for living work at the sampled pan/depth.
///
/// Used only in this handler. Captures its s32 soundId local and hit-sound enum;
/// workPtr and scratchPtr occur repeatedly and must have no side effects.
/// Invoke as a standalone statement in a braced block; retains no pointers.
#define ACTOR_01100_REQUEST_HIT_SOUND(workPtr, scratchPtr)                                                      \
    {                                                                                                           \
        if ((workPtr)->hp > 0) {                                                                                \
            soundId  = ((workPtr)->waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | ACTOR_01100_SOUND_HIT; \
            soundId |= (u8)(workPtr)->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT;                          \
            sndEvtRequestScriptStart(soundId, (s8)(scratchPtr)->pan, (s8)(scratchPtr)->depth);                  \
        }                                                                                                       \
    }

    enum {
        ACTOR_01100_UNUSED_REACTION_SCALE_Q8  = 16 * 256,
        ACTOR_01100_HIT_TWITCH_DAMAGE_LIMIT   = 29,
        ACTOR_01100_HIT_FLINCH_ACCUMULATED    = 61,
        ACTOR_01100_HIT_FALL_ACCUMULATED      = 101,
        ACTOR_01100_HIT_BURST_ATTRIBUTE       = 4,
        ACTOR_01100_HIT_AMPLIFIED_ATTRIBUTE   = 5,
        ACTOR_01100_HIT_ZERO_READOUT_FIRST    = 8,
        ACTOR_01100_HIT_ZERO_READOUT_LAST     = 9,
        ACTOR_01100_HIT_REPEAT_TICKS          = 30,
        ACTOR_01100_HIT_BURN_TICKS            = 90,
        ACTOR_01100_HIT_EFFECT_PERIOD         = 8,
        ACTOR_01100_CRITICAL_EFFECT_BASE      = 0,
        ACTOR_01100_CRITICAL_EFFECT_AMPLIFIED = 2,
        ACTOR_01100_CRITICAL_EFFECT_REST      = 3,
        ACTOR_01100_HIT_EFFECT_NONE           = -1,
        ACTOR_01100_HIT_BEHIND_MIN_ANGLE      = 1025,
        ACTOR_01100_HIT_COMPANION_SHIFT       = 7,
        ACTOR_01100_HIT_ATTACHMENT_BIT        = 0x8000,
        ACTOR_01100_DEATH_BURST_SPAWN_STATE   = 3,
        ACTOR_01100_DEATH_BURN_SPAWN_STATE    = 0x10,
        ACTOR_01100_MOTION_FALL_AGAIN_FRONT   = 19,
        ACTOR_01100_MOTION_FALL_AGAIN_BEHIND  = 20,
        ACTOR_01100_SOUND_DEATH               = 0x400B0006,
        ACTOR_01100_SOUND_HIT                 = 0x400B0007
    };
    s32                    damageAccepted;
    s32                    hitFromBehind;
    s32                    damageOverTime;
    s32                    restVulnerability;
    s32                    criticalChanceMultiplier;
    s32                    incendiaryHit;
    s32                    burstHit;
    s32                    killed;
    s32                    soundId;
    s32                    playbackRate;
    AnimationSlot*         animationSlot;
    WorldCollisionContact* rootContacts;
    GfxCoord*              root;
    s32                    savedRootY;
    s32                    pushedHorizontally;
    s16                    hitEffectFrames;
    s16                    remainingHp;
    s32                    playerDistance;
    s32                    contactKey;
    s32                    cooldownFrames;
    s32                    suppressedAttackAttribute;
    s32                    contactIndex;
    s32                    slotIndex;
    s32                    hitReaction;
    s32                    criticalEffectVariant;
    s32                    sourceKey;
    s32                    sourceBearingMagnitude;
    s32                    accumulatedDamage;
    s32                    damageReactionLevel;
    u32                    attackAttribute;
    u32                    contactDamage;
    u32                    damage;
    u32                    attackKey;
    u8                     reactionFlags;
    u8                     downState;
    u8                     stagedReaction;

    // Age prior-hit feedback before accepting a fresh chest attack.
    damage                   = 0;
    criticalEffectVariant    = ACTOR_01100_HIT_EFFECT_NONE;
    damageAccepted           = 0;
    hitFromBehind            = 0;
    damageOverTime           = 0;
    restVulnerability        = 0;
    criticalChanceMultiplier = 1;
    incendiaryHit            = 0;
    burstHit                 = 0;
    sourceKey                = 0;
    if (work->recentDamage > 0) {
        work->recentDamage = (u16)work->recentDamage - 1;
    }
    if (work->hitEffectFrames > 0) {
        hitEffectFrames       = (u16)work->hitEffectFrames - 1;
        work->hitEffectFrames = hitEffectFrames;
        if (!(hitEffectFrames & (ACTOR_01100_HIT_EFFECT_PERIOD - 1))) {
            effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
            effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
        }
    }
    attackKey = _actor01100FindAttackContact(&scratch->shortVector, work->contacts[ACTOR_01100_BODY_CHEST]);
    if (attackKey != 0) {
        playerDistance = _actor01100MeasurePlayerDistanceSquared(task->extra.tmd->coords);
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts[ACTOR_01100_BODY_CHEST]); contactIndex++) {
            contactKey = work->contacts[ACTOR_01100_BODY_CHEST][contactIndex].key.value;
            if (contactKey != 0) {
                sourceKey = contactKey;
                break;
            }
        }
        // Convert squared cached distance to the units required by damage scaling.
        playerDistance         = SquareRoot0(playerDistance);
        sourceBearingMagnitude = _actor01100BearingToPlayerSlot(task->extra.tmd->coords, (sourceKey >> ACTOR_01100_HIT_COMPANION_SHIFT) & 1);
        if (sourceBearingMagnitude < 0) {
            sourceBearingMagnitude = -sourceBearingMagnitude;
        }
        hitFromBehind = sourceBearingMagnitude >= ACTOR_01100_HIT_BEHIND_MIN_ANGLE;
        if ((work->state == ACTOR_01100_STATE_IDLE_REST) && !(sourceKey & ACTOR_01100_HIT_ATTACHMENT_BIT)) {
            restVulnerability = 1;
            if (criticalEffectVariant < ACTOR_01100_CRITICAL_EFFECT_REST) {
                criticalEffectVariant = ACTOR_01100_CRITICAL_EFFECT_REST;
            }
            criticalChanceMultiplier = 5;
        }
        contactDamage = damageComputePlayerAttack(attackKey, (u32)playerDistance, 0, ACTOR_01100_UNUSED_REACTION_SCALE_Q8);
        if (damageRollCriticalHit(enemy, attackKey, criticalChanceMultiplier) != 0) {
            if (criticalEffectVariant < 0) {
                criticalEffectVariant = ACTOR_01100_CRITICAL_EFFECT_BASE;
            }
            contactDamage *= 4;
        }
        damage += contactDamage;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damageOverTime = damageTickEnemyDamageOverTime(enemy);
        if (damageOverTime > 0) {
            effectSpawn(EFFECT_HIT_PUFF, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], 0x11112400, 0);
            damage += damageOverTime;
        }
    }
    if (restVulnerability != 0) {
        damage *= 2;
    }
    // Cooldown suppresses both contact and status HP loss for this tick.
    cooldownFrames = work->hitCooldown;
    if (cooldownFrames > 0) {
        work->hitCooldown = cooldownFrames - 1;
        damage            = 0;
    } else if (attackKey != 0) {
        work->hitCooldown = damageGetPlayerAttackHitCooldown((s32)attackKey);
    }
    if (damage == 0) {
        suppressedAttackAttribute = damageGetPlayerAttackReaction((s32)attackKey) & 0xFFFF;
        if (suppressedAttackAttribute < ACTOR_01100_HIT_ZERO_READOUT_LAST + 1) {
            if (suppressedAttackAttribute >= ACTOR_01100_HIT_ZERO_READOUT_FIRST) {
                worldTargetAddReadoutAmount(&enemy->node, 0, 0);
            }
        }
    } else if ((s32)damage > 0) {
        killed                = 0;
        work->hitEffectKind   = damageGetPlayerAttackEffectId((s32)attackKey);
        hitReaction           = ACTOR_01100_REACTION_FLINCH;
        work->hitEffectFrames = 0;
        accumulatedDamage     = (u16)work->recentDamage + damage;
        work->recentDamage    = accumulatedDamage;
        if ((s32)damage < ACTOR_01100_HIT_TWITCH_DAMAGE_LIMIT) {
            hitReaction = ACTOR_01100_REACTION_TWITCH;
        }
        // Signed-halfword accumulated damage supplies the minimum reaction tier.
        if ((s16)accumulatedDamage < ACTOR_01100_HIT_FLINCH_ACCUMULATED) {
            damageReactionLevel = ACTOR_01100_REACTION_TWITCH;
        } else if ((s16)accumulatedDamage < ACTOR_01100_HIT_FALL_ACCUMULATED) {
            damageReactionLevel = ACTOR_01100_REACTION_FLINCH;
        } else {
            damageReactionLevel = ACTOR_01100_REACTION_FALL;
        }
        damageAccepted = 1;
        if (hitReaction < damageReactionLevel) {
            hitReaction = damageReactionLevel;
        }
        attackAttribute = damageGetPlayerAttackReaction((s32)attackKey) & 0xFFFF;
        switch (attackAttribute) {
            case DAMAGE_PLAYER_REACTION_NONE:
                break;
            case DAMAGE_PLAYER_REACTION_STAGGER:
                damageStartEnemyStagger(enemy);
                break;
            case DAMAGE_PLAYER_REACTION_BUILDUP:
                if (!(enemy->reactionFlags & ENEMY_REACTION_BUILDUP)) {
                    damageStartEnemyBuildup(enemy, sourceKey, 0);
                    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->reaction != ACTOR_01100_REACTION_STUNNED)) {
                        hitReaction = ACTOR_01100_REACTION_FALL;
                    }
                } else {
                    damageStartEnemyBuildup(enemy, sourceKey, 0);
                }
                break;
            case DAMAGE_PLAYER_REACTION_POISON:
                effectSpawn(EFFECT_HIT_PUFF, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], 0x11112400, 0);
                damageTryStartEnemyDamageOverTime(enemy, sourceKey, 0);
                break;
            case ACTOR_01100_HIT_AMPLIFIED_ATTRIBUTE:
                if (restVulnerability == 0) {
                    damage *= 2;
                    if (criticalEffectVariant < ACTOR_01100_CRITICAL_EFFECT_AMPLIFIED) {
                        criticalEffectVariant = ACTOR_01100_CRITICAL_EFFECT_AMPLIFIED;
                    }
                } else {
                    damage += (s32)damage / 2;
                }
                work->hitEffectFrames = ACTOR_01100_HIT_REPEAT_TICKS;
                break;
            case ACTOR_01100_HIT_BURST_ATTRIBUTE:
            case DAMAGE_PLAYER_REACTION_EXPLOSION:
                burstHit = 1;
                break;
            case DAMAGE_PLAYER_REACTION_INCENDIARY:
                if (restVulnerability == 0) {
                    damage *= 2;
                    if (criticalEffectVariant < ACTOR_01100_CRITICAL_EFFECT_AMPLIFIED) {
                        criticalEffectVariant = ACTOR_01100_CRITICAL_EFFECT_AMPLIFIED;
                    }
                } else {
                    damage += (s32)damage / 2;
                }
                effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], 0x80023300, 0);
                incendiaryHit = 1;
                effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], 0x80023300, 0);
                break;
            case ACTOR_01100_HIT_ZERO_READOUT_FIRST:
            case ACTOR_01100_HIT_ZERO_READOUT_LAST:
                break;
        }
        if (criticalEffectVariant >= 0) {
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], criticalEffectVariant, 0);
        }
        if ((hitReaction == ACTOR_01100_REACTION_TWITCH) && (work->mode == ACTOR_01100_MODE_UNAWARE)) {
            hitReaction = ACTOR_01100_REACTION_FLINCH;
        }
        reactionFlags = enemy->reactionFlags;
        if (reactionFlags & ENEMY_REACTION_STAGGER) {
            hitReaction          = ACTOR_01100_REACTION_FALL;
            enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if ((enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) && (damageIsEnemyDamageOverTimeExpired(enemy) != 0)) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
        damageAccumulateLifeDrainHp(enemy, (s32)attackKey, (s32)damage, 0);
        worldTargetAddReadoutAmount(&enemy->node, (s32)damage, 0);
        if (work->hp > 0) {
            remainingHp = (u16)work->hp - damage;
            work->hp    = remainingHp;
            enemy->hp   = remainingHp;
            if (work->hp <= 0) {
                if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 24, 0, 0)) {
                    work->roomNotified = 0;
                } else {
                    sceneReleaseBattleRefWithRewards(task, work->entryId);
                }
                killed   = 1;
                soundId  = (work->waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | ACTOR_01100_SOUND_DEATH;
                soundId |= (u8)work->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT;
                sndEvtRequestScriptStart(soundId, (s8)scratch->pan, (s8)scratch->depth);
                hitReaction = ACTOR_01100_REACTION_FALL;
                if (work->downState == ACTOR_01100_DOWN_RISING) {
                    hitReaction = ACTOR_01100_REACTION_FALL_AGAIN;
                }
                if (burstHit != 0) {
                    enemy->spawnState = ACTOR_01100_DEATH_BURST_SPAWN_STATE;
                } else if (incendiaryHit != 0) {
                    work->hitEffectFrames = ACTOR_01100_HIT_BURN_TICKS;
                    enemy->spawnState     = ACTOR_01100_DEATH_BURN_SPAWN_STATE;
                }
            }
        } else {
            hitReaction = ACTOR_01100_REACTION_NONE;
        }
        if (killed == 0) {
            downState = work->downState;
            if (downState == ACTOR_01100_DOWN_FALLING) {
                hitReaction = ACTOR_01100_REACTION_NONE;
            } else if (downState == ACTOR_01100_DOWN_RISING) {
                if (hitReaction >= ACTOR_01100_REACTION_FLINCH) {
                    hitReaction = ACTOR_01100_REACTION_FALL_AGAIN;
                } else {
                    hitReaction = ACTOR_01100_REACTION_RISING_LIGHT;
                }
            }
        }
        if ((hitReaction == ACTOR_01100_REACTION_TWITCH) && (damageOverTime != 0)) {
            hitReaction = ACTOR_01100_REACTION_FLINCH;
        }
        if (work->reaction == ACTOR_01100_REACTION_STUNNED) {
            hitReaction = ACTOR_01100_REACTION_STUNNED;
        }
        work->reaction = hitReaction;
        if (hitReaction != ACTOR_01100_REACTION_NONE) {
            playbackRate = ANIMATION_RATE_ONE;
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSlot       = &work->rig.slots[slotIndex];
                animationSlot->rate = playbackRate;
                animationSlot       = &work->flinchRig.slots[slotIndex];
                animationSlot->rate = playbackRate;
            }
        }
        // Stage state changes only after HP, status and down-state select the reaction.
        stagedReaction = work->reaction;
        switch (stagedReaction) {
            case ACTOR_01100_REACTION_TWITCH:
                work->flinchPhase = ACTOR_01100_FLINCH_START;
                ACTOR_01100_REQUEST_HIT_SOUND(work, scratch);
                work->hitFromBehind = hitFromBehind;
                effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_FLINCH:
                ACTOR_01100_REQUEST_HIT_SOUND(work, scratch);
                work->state = ACTOR_01100_STATE_FLINCH;
                _actor01100DisableHandBodies(work);
                work->mode          = ACTOR_01100_MODE_REACTING;
                work->stateStep     = 0;
                work->hitFromBehind = hitFromBehind;
                effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_FALL:
                ACTOR_01100_REQUEST_HIT_SOUND(work, scratch);
                work->flinchPhase = ACTOR_01100_FLINCH_OFF;
                work->state       = ACTOR_01100_STATE_FALL;
                if (enemy->spawnState == ACTOR_01100_DEATH_BURST_SPAWN_STATE) {
                    work->state = ACTOR_01100_STATE_DEATH;
                }
                _actor01100DisableHandBodies(work);
                work->mode          = ACTOR_01100_MODE_REACTING;
                work->stateStep     = 0;
                work->hitFromBehind = hitFromBehind;
                effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_FALL_AGAIN:
                ACTOR_01100_REQUEST_HIT_SOUND(work, scratch);
                work->flinchPhase = ACTOR_01100_FLINCH_OFF;
                work->state       = ACTOR_01100_STATE_FALL_AGAIN;
                if (enemy->spawnState == ACTOR_01100_DEATH_BURST_SPAWN_STATE) {
                    if (work->hitFromBehind != 0) {
                        work->motion = ACTOR_01100_MOTION_FALL_AGAIN_BEHIND;
                    } else {
                        work->motion = ACTOR_01100_MOTION_FALL_AGAIN_FRONT;
                    }
                    work->state = ACTOR_01100_STATE_DEATH;
                }
                _actor01100DisableHandBodies(work);
                work->mode      = ACTOR_01100_MODE_REACTING;
                work->stateStep = 0;
                effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_STUNNED:
                ACTOR_01100_REQUEST_HIT_SOUND(work, scratch);
                effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_RISING_LIGHT:
                ACTOR_01100_REQUEST_HIT_SOUND(work, scratch);
                effectSpawnHit((u16)work->hitEffectKind, &task->extra.tmd->coords[ACTOR_01100_PART_HEAD], NULL, &work->hitEffectArg);
                break;
        }
    }
    // Consume every contact table after retaining horizontal world correction.
    rootContacts       = work->contacts[ACTOR_01100_BODY_ROOT];
    root               = task->extra.tmd->coords;
    savedRootY         = root->coord.t[1];
    pushedHorizontally = _actor01100PushOut(root, rootContacts);
    if (pushedHorizontally != 0) {
        root->composeStamp   = GRAPHICS_COORD_DIRTY;
        work->blockedFrames += 1;
    } else {
        work->blockedFrames = 0;
    }
    root->coord.t[1] = savedRootY;
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_ROOT]);
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_LEFT_HAND]);
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_RIGHT_HAND]);
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_CHEST]);
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (damageTickEnemyBuildup(enemy) != 0)) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
    }
    return damageAccepted;
}

#undef ACTOR_01100_REQUEST_HIT_SOUND

/// Updates an unaware enemy and enters combat after a nearby player action or a hit.
///
/// Spawn argument 0 detects distances below 2500 units. Argument 0x20000 detects
/// an action byte equal to 1, or running within 2000 units; other nonzero values
/// detect running within 4000 units or any movement within 1000 units. Distances
/// use all three cached axes. The player must exist for either nonzero selector.
/// The hit handler still runs when no player qualifies. Only a living enemy still
/// in unaware mode enters the notice state; hand attacks are disabled on engagement.
static void _actor01100UpdateAwareness(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_AWARENESS_DEFAULT_RANGE_SQUARED    = 2500 * 2500,
        ACTOR_01100_AWARENESS_ACTION_SELECTOR          = 0x20000,
        ACTOR_01100_AWARENESS_ACTION_RUN_RANGE_SQUARED = 2000 * 2000,
        ACTOR_01100_AWARENESS_RUN_RANGE_SQUARED        = 4000 * 4000,
        ACTOR_01100_AWARENESS_WALK_RANGE_SQUARED       = 1000 * 1000,
        ACTOR_01100_PLAYER_MOVEMENT_RUN                = 3
    };

    WorldTargetNode* targetNode;
    s32              engage;
    u32              distanceSquared;
    s16              lookYaw;

    engage          = 0;
    distanceSquared = _actor01100MeasurePlayerDistanceSquared(task->extra.tmd->coords);
    lookYaw         = work->lookYaw;
    if (lookYaw >= ACTOR_01100_BODY_TURN_STEP + 1) {
        work->lookYaw = (s16)((u16)work->lookYaw - ACTOR_01100_BODY_TURN_STEP);
    } else if (lookYaw < -ACTOR_01100_BODY_TURN_STEP) {
        work->lookYaw = (s16)((u16)work->lookYaw + ACTOR_01100_BODY_TURN_STEP);
    } else {
        work->lookYaw = 0;
    }
    work->field_BAA = 0;

    if (task->spawnArg1.value == 0) {
        if (distanceSquared <= ACTOR_01100_AWARENESS_DEFAULT_RANGE_SQUARED - 1) {
            engage = 1;
        }
    } else if (task->spawnArg1.value == ACTOR_01100_AWARENESS_ACTION_SELECTOR) {
        Task*      playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        GameActor* playerWork;

        if (playerTask != NULL) {
            playerWork = playerTask->work;
            if (((gSceneCombatState.signals.bytes.actionFlags ^ 1) == 0) || (((u16)playerWork->movementMode == ACTOR_01100_PLAYER_MOVEMENT_RUN) && distanceSquared <= ACTOR_01100_AWARENESS_ACTION_RUN_RANGE_SQUARED - 1)) {
                engage = 1;
            }
        }
    } else {
        Task*      playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        GameActor* playerWork;

        if (playerTask != NULL) {
            playerWork = playerTask->work;
            if ((((u16)playerWork->movementMode == ACTOR_01100_PLAYER_MOVEMENT_RUN) && distanceSquared <= ACTOR_01100_AWARENESS_RUN_RANGE_SQUARED - 1) || distanceSquared <= ACTOR_01100_AWARENESS_WALK_RANGE_SQUARED - 1) {
                engage = 1;
            }
        }
    }

    targetNode                                     = &enemy->node;
    enemy->node.state.parts.flags                  = 0;
    PARENT_OF(targetNode, Enemy, node)->coord      = &task->extra.tmd->coords[ACTOR_01100_PART_CHEST];
    PARENT_OF(targetNode, Enemy, node)->bodyPos.vx = 0;
    PARENT_OF(targetNode, Enemy, node)->bodyPos.vy = -0xC8;
    PARENT_OF(targetNode, Enemy, node)->bodyPos.vz = 0xC8;
    if (_actor01100ProcessHits(enemy, task, work, scratch) != 0) {
        engage = 1;
    }
    if (engage && (work->hp > 0)) {
        work->field_BAA = 0;
        _actor01100DisableHandBodies(work);
        if (work->mode == ACTOR_01100_MODE_UNAWARE) {
            work->mode      = ACTOR_01100_MODE_ENGAGED;
            work->state     = ACTOR_01100_STATE_NOTICE;
            work->stateStep = 0;
        }
    }
}

/// Extends an upper-arm basis with weaker growth across its other two axes.
///
/// `stretch` is a signed Q12 increment: column 0 uses ONE+stretch and columns
/// 1/2 use ONE+(stretch>>2), with an arithmetic shift. Requires live, disjoint
/// matrix, extension and writable vector storage. Each column rereads the
/// extension and narrows the GTE result to halfwords. Translation is preserved;
/// the caller invalidates coordinate composition. Clobbers the GTE and scratch.
static __inline__ void _actor01100StretchArmBasis(MATRIX* armMatrix, SVECTOR* columnScratch, const s16* stretch)
{
    SCALE_COL(armMatrix, columnScratch, 0, *stretch + ONE);
    SCALE_COL(armMatrix, columnScratch, 1, (*stretch >> 2) + ONE);
    SCALE_COL(armMatrix, columnScratch, 2, (*stretch >> 2) + ONE);
}

/// Adds the look twist, shoulder swelling and arm stretch to the current animated pose.
///
/// Angles use 4096 units per turn. Look yaw clamps to +/-1536 and is divided
/// between the head and its two torso ancestors. Each shoulder parent grows by
/// ONE+swell in Q12; the upper-arm child receives its reciprocal to preserve
/// its size. Arm stretch then scales column 0 by ONE+stretch and columns 1/2
/// by ONE+(stretch>>2). Requires the current frame's unadjusted pose and the
/// model hierarchy at parts 4, 6 and 10. Borrows the caller's vector workspace
/// and clobbers the GTE; no pointer is retained.
static void _actor01100ApplyPoseAdjustments(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_LOOK_YAW_LIMIT        = 1536,
        ACTOR_01100_LOOK_HEAD_YAW_LIMIT   = 512,
        ACTOR_01100_LOOK_SHARED_YAW_LIMIT = 767
    };
    GfxCoord* headCoord;
    GfxCoord* upperArm;
    s32       lookYaw;
    s32       torsoYaw;
    s32       shoulderScale;

    lookYaw   = work->lookYaw;
    headCoord = task->extra.tmd->coords + ACTOR_01100_PART_HEAD;
    if (lookYaw < -ACTOR_01100_LOOK_YAW_LIMIT) {
        lookYaw = -ACTOR_01100_LOOK_YAW_LIMIT;
    } else if (lookYaw >= ACTOR_01100_LOOK_YAW_LIMIT + 1) {
        lookYaw = ACTOR_01100_LOOK_YAW_LIMIT;
    }
    if ((u32)(lookYaw + ACTOR_01100_LOOK_SHARED_YAW_LIMIT) < 2 * ACTOR_01100_LOOK_SHARED_YAW_LIMIT + 1U) {
        torsoYaw = lookYaw / 3;
        gfxRotMatrixY(&headCoord->coord, lookYaw - torsoYaw, GRAPHICS_ROTATION_COMPOSE);
        headCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else if (lookYaw > 0) {
        gfxRotMatrixY(&headCoord->coord, ACTOR_01100_LOOK_HEAD_YAW_LIMIT, GRAPHICS_ROTATION_COMPOSE);
        headCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        torsoYaw                = lookYaw - ACTOR_01100_LOOK_HEAD_YAW_LIMIT;
    } else {
        gfxRotMatrixY(&headCoord->coord, -ACTOR_01100_LOOK_HEAD_YAW_LIMIT, GRAPHICS_ROTATION_COMPOSE);
        headCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        torsoYaw                = lookYaw + ACTOR_01100_LOOK_HEAD_YAW_LIMIT;
    }
    torsoYaw >>= 1;
    gfxRotMatrixY(&headCoord->parent->coord, torsoYaw, GRAPHICS_ROTATION_COMPOSE);
    headCoord->parent->composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixY(&headCoord->parent->parent->coord, torsoYaw, GRAPHICS_ROTATION_COMPOSE);
    headCoord->parent->parent->composeStamp = GRAPHICS_COORD_DIRTY;

    // Enlarge each shoulder while compensating the arm child by its reciprocal.
    if (work->rightShoulderSwell != 0) {
        GfxCoord* modelCoords;
        s32       inverseScale;

        shoulderScale      = work->rightShoulderSwell + ONE;
        modelCoords        = task->extra.tmd->coords;
        scratch->vector.vz = shoulderScale;
        scratch->vector.vy = shoulderScale;
        scratch->vector.vx = shoulderScale;
        upperArm           = &modelCoords[ACTOR_01100_PART_RIGHT_UPPER_ARM];
        _gfxScaleMatrixColumns(&upperArm->parent->coord, &scratch->vector);
        upperArm->parent->composeStamp = GRAPHICS_COORD_DIRTY;
        inverseScale                   = (ONE * ONE) / shoulderScale;
        scratch->vector.vz             = inverseScale;
        scratch->vector.vy             = inverseScale;
        scratch->vector.vx             = inverseScale;
        _gfxScaleMatrixColumns(&upperArm->coord, &scratch->vector);
    }

    if (work->leftShoulderSwell != 0) {
        GfxCoord* modelCoords;
        s32       inverseScale;

        shoulderScale      = work->leftShoulderSwell + ONE;
        modelCoords        = task->extra.tmd->coords;
        scratch->vector.vz = shoulderScale;
        scratch->vector.vy = shoulderScale;
        scratch->vector.vx = shoulderScale;
        upperArm           = &modelCoords[ACTOR_01100_PART_LEFT_UPPER_ARM];
        _gfxScaleMatrixColumns(&upperArm->parent->coord, &scratch->vector);
        upperArm->parent->composeStamp = GRAPHICS_COORD_DIRTY;
        inverseScale                   = (ONE * ONE) / shoulderScale;
        scratch->vector.vz             = inverseScale;
        scratch->vector.vy             = inverseScale;
        scratch->vector.vx             = inverseScale;
        _gfxScaleMatrixColumns(&upperArm->coord, &scratch->vector);
    }

    // Stretch each upper arm along its first axis, with weaker cross-axis growth.
    if (work->rightArmStretch != 0) {
        MATRIX*   armMatrix;
        SVECTOR*  columnScratch;
        GfxCoord* modelCoords;

        columnScratch = &scratch->shortVector;
        modelCoords   = task->extra.tmd->coords;
        armMatrix     = &modelCoords[ACTOR_01100_PART_RIGHT_UPPER_ARM].coord;
        _actor01100StretchArmBasis(armMatrix, columnScratch, &work->rightArmStretch);
        modelCoords[ACTOR_01100_PART_RIGHT_UPPER_ARM].composeStamp = GRAPHICS_COORD_DIRTY;
    }

    if (work->leftArmStretch != 0) {
        MATRIX*   armMatrix;
        SVECTOR*  columnScratch;
        GfxCoord* modelCoords;

        columnScratch = &scratch->shortVector;
        modelCoords   = task->extra.tmd->coords;
        armMatrix     = &modelCoords[ACTOR_01100_PART_LEFT_UPPER_ARM].coord;
        _actor01100StretchArmBasis(armMatrix, columnScratch, &work->leftArmStretch);
        modelCoords[ACTOR_01100_PART_LEFT_UPPER_ARM].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Per-frame state handlers `_actor01100Tick` copies to its stack and
/// indexes by `_Actor01100Work::state`.
static const _Actor01100StateTable Actor01100_D00064 = { {
    _actor01100Idle,
    _actor01100IdleSwingLeft,
    _actor01100IdleSwingRight,
    _actor01100IdleTurn,
    _actor01100IdleRest,
    _actor01100Idle,
    _actor01100Idle,
    _actor01100Idle,
    _actor01100Idle,
    _actor01100Idle,
    _actor01100NoticePlayer,
    Actor01100_Fn04DB4,
    _actor01100PunchLeft,
    _actor01100PunchRight,
    _actor01100Advance,
    _actor01100FacePlayer,
    _actor01100Idle,
    _actor01100Idle,
    _actor01100Idle,
    _actor01100Idle,
    Actor01100_Fn07014,
    _actor01100Flinch,
    Actor01100_Fn07148,
    _actor01100Rise,
    Actor01100_Fn05678,
    Actor01100_Fn05CFC,
} };

/// Advances Mossback animation, behavior, hit reactions and presentation for one frame.
///
/// Hidden models skip the whole tick. Running combat actors tick slots 1..20,
/// blend directional flinch poses except on the upper arms, dispatch the behavior
/// state and apply its pose changes, then process the current combat mode.
/// Paused combat actors only resolve root contacts; other control values still
/// reach lighting and drawing. Water-room splashes follow the selected part's
/// surface crossings. Lighting samples the cached root translation with 800
/// taken from Y. Finished death mode exits the task after presentation.
///
/// Requires initialized work, a state in the 26-entry behavior table and live
/// scratch storage for this call. Both animation rigs and their clip data stay
/// loaded; cached player and model positions share one composition frame.
static void _actor01100Tick(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_FLINCH_WEIGHT_MAX          = 64,
        ACTOR_01100_FLINCH_WEIGHT_Q12_SHIFT    = 5,
        ACTOR_01100_REACTION_BLEND_LIMIT       = 16,
        ACTOR_01100_MOTION_STUNNED_FRONT       = 21,
        ACTOR_01100_MOTION_STUNNED_BEHIND      = 22,
        ACTOR_01100_RECOVERY_STRETCH_STEP      = 1024,
        ACTOR_01100_RECOVERY_SWELL_STEP        = 256,
        ACTOR_01100_RECOVERY_LOOK_STEP         = 48,
        ACTOR_01100_FLINCH_WEIGHT_STEP         = 4,
        ACTOR_01100_NORMAL_BLEND_FRAMES        = 8,
        ACTOR_01100_ARM_REACTION_BLEND_FRAMES  = 30,
        ACTOR_01100_ARM_REACTION_RECORD_OFFSET = 3,
        ACTOR_01100_RIPPLE_FRAME_MASK          = 15,
        ACTOR_01100_RIPPLE_ARGUMENT            = 192,
        ACTOR_01100_RIPPLE_Y_OFFSET            = -480,
        ACTOR_01100_HAND_SPRAY_ARGUMENT        = 0x11402300,
        ACTOR_01100_CHEST_SPRAY_ARGUMENT       = 0x11602480,
        ACTOR_01100_SPLASH_HAND_OFFSET         = 400,
        ACTOR_01100_SPLASH_CHEST_SPAN          = 1200,
        ACTOR_01100_LIGHT_SAMPLE_Y_OFFSET      = -800,
        ACTOR_01100_SHADOW_SIZE                = 1536,
        ACTOR_01100_SPLASH_SPRAY_TICKS         = 5,
        ACTOR_01100_SOUND_SPLASH               = 0x404B000D
    };
    _Actor01100StateTable stateHandlers;
    GfxCoord*             partCoord;
    GfxCoord*             rootCoord;
    s32                   restartMotion;
    s32                   transitionJitter;
    s32                   slotIndex;
    AnimationPose*        bodyPose;
    s32                   flinchWeightQ12;
    s32                   flinchMotion;
    s32                   savedRootY;
    s32                   sprayArgument;
    s16                   surfaceDeltaY;
    s16                   lookYaw;

    stateHandlers = Actor01100_D00064;
    restartMotion = 0;
    if (work->hidden != 0) {
        return;
    }
    actorRenderComposeCoord(&task->extra.tmd->coords[ACTOR_01100_PART_CHEST]);
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        transitionJitter  = rand() & 1;
        work->motionEnded = 0;
        work->prevMode    = work->mode;
        if (work->motion != work->startedMotion) {
            restartMotion       = 1;
            work->startedMotion = work->motion;
        }
        // Blend flinch playback over the body without suppressing arm attack poses.
        switch (work->flinchPhase) {
            case ACTOR_01100_FLINCH_OFF:
                work->flinchWeight = 0;
                break;
            case ACTOR_01100_FLINCH_START:
                flinchMotion = ACTOR_01100_MOTION_FLINCH_BEHIND;
                if (work->hitFromBehind == 0) {
                    flinchMotion = ACTOR_01100_MOTION_FLINCH_FRONT;
                }
                slotIndex = 1;
                do {
                    animationCaptureSlotWithBlend(&work->flinchRig.anim, slotIndex, &scratch->poses[1], flinchMotion, 0, 0, 0);
                    slotIndex++;
                } while (slotIndex < ARRAY_SIZE(work->flinchRig.slots));
                work->flinchPhase++;
                break;
            case ACTOR_01100_FLINCH_FADE_IN:
                if (work->flinchWeight < ACTOR_01100_FLINCH_WEIGHT_MAX) {
                    work->flinchWeight += ACTOR_01100_FLINCH_WEIGHT_STEP;
                }
                if (work->flinchRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                    work->flinchPhase++;
                }
                break;
            case ACTOR_01100_FLINCH_FADE_OUT:
                if (work->flinchWeight > 0) {
                    work->flinchWeight -= ACTOR_01100_FLINCH_WEIGHT_STEP;
                    if (work->flinchWeight > 0) {
                        break;
                    }
                }
                work->flinchPhase++;
                break;
            default:
                work->flinchWeight = 0;
                work->flinchPhase  = ACTOR_01100_FLINCH_OFF;
                break;
        }
        slotIndex = 1;
        do {
            if ((slotIndex == ACTOR_01100_PART_RIGHT_UPPER_ARM) || (slotIndex == ACTOR_01100_PART_LEFT_UPPER_ARM)) {
                bodyPose = 0;
            } else {
                bodyPose = 0;
                if (work->flinchWeight != 0) {
                    bodyPose = &scratch->poses[0];
                }
            }
            if (restartMotion != 0) {
                if ((u32)(work->reaction - ACTOR_01100_REACTION_FLINCH) >= (ACTOR_01100_REACTION_BLEND_LIMIT - ACTOR_01100_REACTION_FLINCH)) {
                    animationCaptureSlotWithBlend(&work->rig.anim, slotIndex, bodyPose, work->motion, 0, 0, transitionJitter + ACTOR_01100_NORMAL_BLEND_FRAMES);
                } else if ((u32)((u8)work->motion - ACTOR_01100_MOTION_STUNNED_FRONT) < (ACTOR_01100_MOTION_STUNNED_BEHIND - ACTOR_01100_MOTION_STUNNED_FRONT + 1)) {
                    bodyPose = 0;
                    animationResetSlot(&work->rig.anim, slotIndex, work->motion);
                } else if ((slotIndex != ACTOR_01100_PART_RIGHT_UPPER_ARM) && (slotIndex != ACTOR_01100_PART_LEFT_UPPER_ARM)) {
                    animationCaptureSlotWithBlend(&work->rig.anim, slotIndex, bodyPose, work->motion, 0, 0, 1);
                } else {
                    animationCaptureSlotWithBlend(&work->rig.anim, slotIndex, bodyPose, work->motion, ACTOR_01100_ARM_REACTION_RECORD_OFFSET, 0, ACTOR_01100_ARM_REACTION_BLEND_FRAMES);
                }
            } else {
                animationTickSlotPose(&work->rig.anim, slotIndex, bodyPose, 0);
            }
            if (bodyPose != 0) {
                flinchWeightQ12 = work->flinchWeight << ACTOR_01100_FLINCH_WEIGHT_Q12_SHIFT;
                animationTickSlotPose(&work->flinchRig.anim, slotIndex, &scratch->poses[1], 0);
                animationApplyBlendedPose(&work->rig.anim, slotIndex, &scratch->poses[0],
                                          &scratch->poses[1], ONE - flinchWeightQ12, flinchWeightQ12);
            }
            slotIndex++;
        } while (slotIndex < ARRAY_SIZE(work->rig.slots));
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->motionEnded = 1;
        }
        partCoord      = task->extra.tmd->coords;
        partCoord     += ACTOR_01100_PART_BODY;
        scratch->pan   = worldCoordGetOriginAudioPan(partCoord);
        scratch->depth = worldCoordGetOriginAudioDepth(partCoord);
        // Behavior selects the pose additions before mode-specific hit processing.
        stateHandlers.funcs[work->state](enemy, task, work, scratch);
        _actor01100ApplyPoseAdjustments(enemy, task, work, scratch);
        switch (work->mode) {
            case ACTOR_01100_MODE_UNAWARE:
                _actor01100UpdateAwareness(enemy, task, work, scratch);
                break;
            case ACTOR_01100_MODE_ENGAGED: {
                GfxCoord*        modelCoords;
                WorldTargetNode* targetNode;

                enemy->node.state.parts.flags = 0;
                modelCoords                   = task->extra.tmd->coords;
                targetNode                    = &enemy->node;

                PARENT_OF(targetNode, Enemy, node)->bodyPos.vy = -0xC8;
                PARENT_OF(targetNode, Enemy, node)->bodyPos.vx = 0;
                PARENT_OF(targetNode, Enemy, node)->bodyPos.vz = 0xC8;
                PARENT_OF(targetNode, Enemy, node)->coord      = modelCoords + ACTOR_01100_PART_CHEST;
                if (_actor01100ProcessHits(enemy, task, work, scratch) == 0 && task->spawnArg1.value == 0 && work->prevMode == ACTOR_01100_MODE_ENGAGED && work->motionEnded == 1 && _actor01100MeasurePlayerDistanceSquared(task->extra.tmd->coords) > ACTOR_01100_PUNCH_RANGE_SQUARED) {
                    _actor01100DisableHandBodies(work);
                    work->mode      = ACTOR_01100_MODE_UNAWARE;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 0x10) & 0xF) < 0xC) {
                        work->state = ACTOR_01100_STATE_IDLE_REST;
                    } else {
                        work->state = ACTOR_01100_STATE_IDLE;
                    }
                    work->stateStep = 0;
                }
                break;
            }
            case ACTOR_01100_MODE_REACTING: {
                GfxCoord*        modelCoords;
                WorldTargetNode* targetNode;

                if (work->reaction != ACTOR_01100_REACTION_TWITCH) {
                    if (work->rightArmStretch >= ACTOR_01100_RECOVERY_STRETCH_STEP) {
                        work->rightArmStretch -= ACTOR_01100_RECOVERY_STRETCH_STEP;
                    } else {
                        work->rightArmStretch = 0;
                    }
                    if (work->leftArmStretch >= ACTOR_01100_RECOVERY_STRETCH_STEP) {
                        work->leftArmStretch -= ACTOR_01100_RECOVERY_STRETCH_STEP;
                    } else {
                        work->leftArmStretch = 0;
                    }
                    if (work->leftShoulderSwell >= ACTOR_01100_RECOVERY_SWELL_STEP) {
                        work->leftShoulderSwell -= ACTOR_01100_RECOVERY_SWELL_STEP;
                    } else {
                        work->leftShoulderSwell = 0;
                    }
                    if (work->rightShoulderSwell >= ACTOR_01100_RECOVERY_SWELL_STEP) {
                        work->rightShoulderSwell -= ACTOR_01100_RECOVERY_SWELL_STEP;
                    } else {
                        work->rightShoulderSwell = 0;
                    }
                    if (work->downState != ACTOR_01100_DOWN_STANDING) {
                        lookYaw = work->lookYaw;
                        if (lookYaw > ACTOR_01100_RECOVERY_LOOK_STEP) {
                            work->lookYaw -= ACTOR_01100_RECOVERY_LOOK_STEP;
                        } else if (lookYaw < -ACTOR_01100_RECOVERY_LOOK_STEP) {
                            work->lookYaw += ACTOR_01100_RECOVERY_LOOK_STEP;
                        }
                    }
                }
                modelCoords = task->extra.tmd->coords;
                targetNode  = &enemy->node;

                PARENT_OF(targetNode, Enemy, node)->bodyPos.vy = -0xC8;
                PARENT_OF(targetNode, Enemy, node)->bodyPos.vx = 0;
                PARENT_OF(targetNode, Enemy, node)->bodyPos.vz = 0xC8;
                PARENT_OF(targetNode, Enemy, node)->coord      = modelCoords + ACTOR_01100_PART_CHEST;
                _actor01100ProcessHits(enemy, task, work, scratch);
                break;
            }
        }
        // Convert the selected limb from view to world space for water-surface crossings.
        if (work->waterRoom == 1) {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->viewInverse);
            if (!(gDisplayState.animFrame & ACTOR_01100_RIPPLE_FRAME_MASK)) {
                GfxCoord* modelCoords;

                modelCoords             = task->extra.tmd->coords;
                scratch->shortVector.vx = 0;
                scratch->shortVector.vy = ACTOR_01100_RIPPLE_Y_OFFSET;
                scratch->shortVector.vz = 0;
                effectSpawn(gRoomEffectWaterRippleId, modelCoords, ACTOR_01100_RIPPLE_ARGUMENT, &scratch->shortVector);
            }
            if (scratch->splashPart != 0) {
                if (work->splashPart == 0 || (scratch->splashPart == ACTOR_01100_PART_CHEST && work->splashPart != scratch->splashPart)) {
                    work->splashHeight = 0;
                }
                work->splashPart = scratch->splashPart;
            }
            if (work->sprayFrames != 0 || scratch->splashPart != 0) {
                sprayArgument = ACTOR_01100_HAND_SPRAY_ARGUMENT;
                partCoord     = &task->extra.tmd->coords[work->splashPart];
                actorRenderComposeCoord(partCoord);
                scratch->shortVector.vx = 0;
                scratch->shortVector.vy = 0;
                scratch->shortVector.vz = 0;
                if (work->splashPart == ACTOR_01100_PART_RIGHT_HAND) {
                    scratch->shortVector.vx = ACTOR_01100_SPLASH_HAND_OFFSET;
                } else if (work->splashPart == ACTOR_01100_PART_LEFT_HAND) {
                    scratch->shortVector.vx = -ACTOR_01100_SPLASH_HAND_OFFSET;
                }
                _gfxRotateSv(&partCoord->workm, &scratch->shortVector);
                scratch->shortVector.vx += partCoord->workm.t[0];
                scratch->shortVector.vy += partCoord->workm.t[1];
                scratch->shortVector.vz += partCoord->workm.t[2];
                scratch->shortVector.vx -= gGfxViewCoord.workm.t[0];
                scratch->shortVector.vy -= gGfxViewCoord.workm.t[1];
                scratch->shortVector.vz -= gGfxViewCoord.workm.t[2];
                _gfxRotateSv(&scratch->viewInverse, &scratch->shortVector);
                surfaceDeltaY = gGameSession->waterY - scratch->shortVector.vy;
                if (work->splashHeight * surfaceDeltaY < 0) {
                    work->sprayFrames = ACTOR_01100_SPLASH_SPRAY_TICKS;
                }
                work->splashHeight = surfaceDeltaY;
                if (work->splashPart == ACTOR_01100_PART_CHEST) {
                    scratch->shortVector.vx += rand() % ACTOR_01100_SPLASH_CHEST_SPAN - ACTOR_01100_SPLASH_CHEST_SPAN / 2;
                    sprayArgument            = ACTOR_01100_CHEST_SPRAY_ARGUMENT;
                    scratch->shortVector.vz += rand() % ACTOR_01100_SPLASH_CHEST_SPAN - ACTOR_01100_SPLASH_CHEST_SPAN / 2;
                }
                if (work->sprayFrames != 0) {
                    work->sprayFrames--;
                    if (work->sprayFrames == 0) {
                        work->splashPart = 0;
                    }
                    effectSpawn(gRoomEffectWaterSprayId, &gGfxViewCoord, sprayArgument, &scratch->shortVector);
                    sndEvtRequestScriptStart(((u8)work->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT) | ACTOR_01100_SOUND_SPLASH, (s8)scratch->pan, (s8)scratch->depth);
                }
            }
        }
    } else if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        rootCoord  = task->extra.tmd->coords;
        savedRootY = rootCoord->coord.t[1];
        actorRenderComposeCoord(rootCoord);
        if (_actor01100PushOut(rootCoord, work->contacts[ACTOR_01100_BODY_ROOT])) {
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        rootCoord->coord.t[1] = savedRootY;
        worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_ROOT]);
    }
    // Presentation also runs while actor control is paused.
    rootCoord = task->extra.tmd->coords;
    actorRenderComposeCoord(rootCoord);
    scratch->vector.vx = rootCoord->workm.t[0];
    scratch->vector.vy = rootCoord->workm.t[1] + ACTOR_01100_LIGHT_SAMPLE_Y_OFFSET;
    scratch->vector.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &scratch->vector, 0, 0);
    if (!(task->extra.tmd->flags & TMD_OBJECT_SEMI_TRANS)) {
        actorRenderDrawGroundShadow(task->extra.tmd->coords, ACTOR_01100_SHADOW_SIZE, NULL);
    }
    if (work->mode >= ACTOR_01100_MODE_FINISHED) {
        taskCallExit(task);
    }
}

/// Plays the idle left-arm swing, enabling its attack sphere during the swing.
///
/// A contact disables both hand attacks for the remaining active frames. The
/// animation boundary returns to idle; the left hand is watched for splashes.
static void _actor01100IdleSwingLeft(Enemy* enemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_SWING_ENABLE_FRAME  = 26,
        ACTOR_01100_SWING_DISABLE_FRAME = 55
    };
    WorldCollisionBody* leftHandBody;
    u16                 swingFrame;

    if (work->stateStep == 0) {
        work->motion       = ACTOR_01100_MOTION_IDLE_SWING_LEFT;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
    }
    swingFrame         = (u16)work->stateCounter + 1;
    work->stateCounter = swingFrame;
    if ((s16)swingFrame == ACTOR_01100_SWING_ENABLE_FRAME) {
        leftHandBody         = &work->bodies[ACTOR_01100_BODY_LEFT_HAND];
        leftHandBody->key    = damagePackEnemyAttackKey(enemy, ACTOR_01100_ATTACK_IDLE_LEFT);
        leftHandBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((u32)((u16)work->stateCounter - (ACTOR_01100_SWING_ENABLE_FRAME + 1)) < (ACTOR_01100_SWING_DISABLE_FRAME - ACTOR_01100_SWING_ENABLE_FRAME - 1)) {
        if ((work->stateStep == 1) && (worldCollisionFindContactIndex(work->contacts[ACTOR_01100_BODY_LEFT_HAND], WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
            _actor01100DisableHandBodies(work);
            work->stateStep = (u8)work->stateStep + 1;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_LEFT_HAND;
    if (work->stateCounter == ACTOR_01100_SWING_DISABLE_FRAME) {
        _actor01100DisableHandBodies(work);
    }
    if (work->motionEnded == 1) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Plays the idle right-arm swing, enabling its attack sphere during the swing.
///
/// A contact disables both hand attacks for the remaining active frames. This
/// side retains the key installed at setup despite computing its own attack row.
/// The animation boundary returns to idle; the right hand is watched for splashes.
static void _actor01100IdleSwingRight(Enemy* enemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_SWING_ENABLE_FRAME  = 26,
        ACTOR_01100_SWING_DISABLE_FRAME = 55
    };
    WorldCollisionBody* rightHandBody;
    u16                 swingFrame;

    if (work->stateStep == 0) {
        work->motion       = ACTOR_01100_MOTION_IDLE_SWING_RIGHT;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
    }
    swingFrame         = (u16)work->stateCounter + 1;
    work->stateCounter = swingFrame;
    if ((s16)swingFrame == ACTOR_01100_SWING_ENABLE_FRAME) {
        rightHandBody = &work->bodies[ACTOR_01100_BODY_RIGHT_HAND];
        // The original discards this result and keeps the setup attack key.
        damagePackEnemyAttackKey(enemy, ACTOR_01100_ATTACK_IDLE_RIGHT);
        rightHandBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((u32)((u16)work->stateCounter - (ACTOR_01100_SWING_ENABLE_FRAME + 1)) < (ACTOR_01100_SWING_DISABLE_FRAME - ACTOR_01100_SWING_ENABLE_FRAME - 1)) {
        if ((work->stateStep == 1) && (worldCollisionFindContactIndex(work->contacts[ACTOR_01100_BODY_RIGHT_HAND], WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
            _actor01100DisableHandBodies(work);
            work->stateStep = (u8)work->stateStep + 1;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    if (work->stateCounter == ACTOR_01100_SWING_DISABLE_FRAME) {
        _actor01100DisableHandBodies(work);
    }
    if (work->motionEnded == 1) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Turns on the spot through a randomly chosen signed angle, then returns to idle.
///
/// Chooses one of +/-512, +/-1024 and +/-1536 in 4096 units per turn, and
/// consumes 16 units each tick. The counter's sign opposes the applied root yaw.
static void _actor01100IdleTurn(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    GfxCoord* root;
    s32       turnChoice;
    u32       randomState;
    u16       rootYaw;

    root = task->extra.tmd->coords;
    if (work->stateStep == 0) {
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomState;
        turnChoice      = (randomState >> 0x10) & 0xF;
        if (turnChoice < 3) {
            work->stateCounter = 0x200;
        } else if (turnChoice < 6) {
            work->stateCounter = -0x200;
        } else if (turnChoice < 9) {
            work->stateCounter = 0x400;
        } else if (turnChoice < 0xC) {
            work->stateCounter = -0x400;
        } else if (turnChoice < 0xE) {
            work->stateCounter = 0x600;
        } else {
            work->stateCounter = -0x600;
        }
        work->motion    = ACTOR_01100_MOTION_TURN;
        work->stateStep = (u8)work->stateStep + 1;
    }
    if (work->stateCounter > 0) {
        rootYaw            = ((u16)root->param.rot.vy - ACTOR_01100_BODY_TURN_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
        root->param.rot.vy = rootYaw;
        gfxRotMatrixY(&root->coord, rootYaw, 1);
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateCounter = (u16)work->stateCounter - ACTOR_01100_BODY_TURN_STEP;
    } else {
        rootYaw            = ((u16)root->param.rot.vy + ACTOR_01100_BODY_TURN_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
        root->param.rot.vy = rootYaw;
        gfxRotMatrixY(&root->coord, rootYaw, 1);
        root->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateCounter = (u16)work->stateCounter + ACTOR_01100_BODY_TURN_STEP;
    }
    if (work->stateCounter == 0) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Returns the controlled player's bearing about `self`'s cached axes.
///
/// Both coordinate caches must be in the same frame. The signed-halfword
/// position delta and Q12 basis follow `_actorAngleBearingInFrame`'s contract;
/// the result is in [-2048, 2048], with 4096 units per turn. Returns zero when
/// the player actor slot is empty. Borrows 64 scratch-stack bytes until return.
static __inline__ s32 _actor01100BearingToPlayer(const GfxCoord* self)
{
    GfxCoord*            playerCoord;
    ActorBearingScratch* scratch;
    s32                  angle;

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        angle = 0;
    } else {
        playerCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
        angle       = _actorAngleBearingInFrame(scratch, self, playerCoord);
        SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    }
    return angle;
}

/// Measures the player's bearing and eases the upper-body look yaw toward it.
///
/// Caches the full bearing in `work->playerBearing`, then limits the target
/// look yaw to +/-768 and approaches it by at most 192 units per call, with
/// 4096 units per turn. Requires a live model root with a composed cache and
/// `_actor01100BearingToPlayer`'s scratch-stack space. The enemy and caller's
/// scratch arguments are unused members of the state-handler signature.
static void _actor01100TrackPlayerLookYaw(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    enum { ACTOR_01100_LOOK_YAW_LIMIT = 0x300,
           ACTOR_01100_LOOK_YAW_STEP  = 0xC0 };
    s16 bearing;
    s32 targetLookYaw;
    s16 currentLookYaw;

    bearing             = _actor01100BearingToPlayer(task->extra.tmd->coords);
    work->playerBearing = bearing;
    targetLookYaw       = bearing;
    if (targetLookYaw < -ACTOR_01100_LOOK_YAW_LIMIT) {
        targetLookYaw = -ACTOR_01100_LOOK_YAW_LIMIT;
    } else if (targetLookYaw >= ACTOR_01100_LOOK_YAW_LIMIT + 1) {
        targetLookYaw = ACTOR_01100_LOOK_YAW_LIMIT;
    }

    currentLookYaw = work->lookYaw;
    if (currentLookYaw < targetLookYaw - ACTOR_01100_LOOK_YAW_STEP) {
        work->lookYaw += ACTOR_01100_LOOK_YAW_STEP;
    } else if (targetLookYaw + ACTOR_01100_LOOK_YAW_STEP < currentLookYaw) {
        work->lookYaw -= ACTOR_01100_LOOK_YAW_STEP;
    } else {
        work->lookYaw = targetLookYaw;
    }
}

/// Returns squared cached-frame distance to the registered player task.
///
/// Returns `INT_MAX` when its task slot is empty. Each XYZ difference narrows
/// to a signed halfword before the GTE dot product; subtraction and the sum
/// must fit signed 32 bits for distance comparisons. Both caches must share a
/// frame. Borrows one `SVECTOR` from the initialized scratch stack until return.
static __inline__ s32 _actor01100DistSqToPlayer(const GfxCoord* self)
{
    Task*     player;
    GfxCoord* playerCoord;
    SVECTOR*  delta;
    s32       distanceSquared;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (player == NULL) {
        return INT_MAX;
    }
    playerCoord     = player->extra.tmd->coords;
    delta           = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    delta->vx       = playerCoord->workm.t[0] - self->workm.t[0];
    delta->vy       = playerCoord->workm.t[1] - self->workm.t[1];
    delta->vz       = playerCoord->workm.t[2] - self->workm.t[2];
    distanceSquared = gfxDotProduct(delta, delta);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return distanceSquared;
}

/// Looks toward the newly noticed player, pauses, then turns and selects an attack.
///
/// The look is limited to +/-1536 and moves by 192 angle units per tick;
/// the body turns by at most 16, in 4096 units per turn. An ordinary spawn
/// advances when outside punch range; a nonzero spawn payload picks randomly
/// between either punch and a spit. Requires composed model and player caches
/// in the same frame and the initialized scratch stack used by the bearing tests.
static void _actor01100NoticePlayer(Enemy* unusedEnemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    enum {
        ACTOR_01100_NOTICE_LOOK       = 1,
        ACTOR_01100_NOTICE_PAUSE      = 2,
        ACTOR_01100_NOTICE_TURN       = 3,
        ACTOR_01100_NOTICE_LOOK_LIMIT = 1536,
        ACTOR_01100_NOTICE_LOOK_STEP  = 192
    };
    GfxCoord* root;
    s16       playerBearing;
    s32       initialLookGoal;
    s32       bearing;
    s16       currentLookYaw;
    s16       nextLookYaw;
    s8        noticePhase;
    s32       attackChoice;

    if (work->stateStep == 0) {
        work->motion        = ACTOR_01100_MOTION_IDLE;
        work->playerBearing = _actor01100BearingToPlayer(task->extra.tmd->coords);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateCounter  = ((gRandomLcgState >> 0x10) & 0x1F) + 2;
        work->stateStep++;
    }

    noticePhase = work->stateStep;
    if (noticePhase == ACTOR_01100_NOTICE_LOOK) {
        initialLookGoal = work->playerBearing;
        if (initialLookGoal < -ACTOR_01100_NOTICE_LOOK_LIMIT) {
            initialLookGoal = -ACTOR_01100_NOTICE_LOOK_LIMIT;
        } else if (initialLookGoal >= (ACTOR_01100_NOTICE_LOOK_LIMIT + 1)) {
            initialLookGoal = ACTOR_01100_NOTICE_LOOK_LIMIT;
        }
        currentLookYaw = work->lookYaw;
        if (currentLookYaw < initialLookGoal) {
            nextLookYaw   = work->lookYaw + ACTOR_01100_NOTICE_LOOK_STEP;
            work->lookYaw = nextLookYaw;
            if (initialLookGoal < nextLookYaw) {
                work->lookYaw = initialLookGoal;
            }
        } else if (initialLookGoal < currentLookYaw) {
            nextLookYaw   = work->lookYaw - ACTOR_01100_NOTICE_LOOK_STEP;
            work->lookYaw = nextLookYaw;
            if (nextLookYaw < initialLookGoal) {
                work->lookYaw = initialLookGoal;
            }
        } else {
            sceneEngageBattle(1);
            work->stateStep++;
        }
    } else if (noticePhase == ACTOR_01100_NOTICE_PAUSE) {
        if (--work->stateCounter < 0) {
            work->motion = ACTOR_01100_MOTION_TURN;
            work->stateStep++;
        }
    }

    // After the pause, keep tracking while bringing the whole body to face the player.
    if (work->stateStep == ACTOR_01100_NOTICE_TURN) {
        root                = task->extra.tmd->coords;
        playerBearing       = _actor01100BearingToPlayer(root);
        bearing             = playerBearing;
        work->playerBearing = playerBearing;
        if (bearing < -ACTOR_01100_NOTICE_LOOK_LIMIT) {
            bearing = -ACTOR_01100_NOTICE_LOOK_LIMIT;
        } else if (bearing >= (ACTOR_01100_NOTICE_LOOK_LIMIT + 1)) {
            bearing = ACTOR_01100_NOTICE_LOOK_LIMIT;
        }
        currentLookYaw = work->lookYaw;
        if (currentLookYaw < bearing - ACTOR_01100_NOTICE_LOOK_STEP) {
            work->lookYaw += ACTOR_01100_NOTICE_LOOK_STEP;
        } else if (bearing + ACTOR_01100_NOTICE_LOOK_STEP < currentLookYaw) {
            work->lookYaw -= ACTOR_01100_NOTICE_LOOK_STEP;
        } else {
            work->lookYaw = bearing;
        }

        bearing = work->playerBearing;
        if (bearing >= ACTOR_01100_BODY_TURN_STEP + 1) {
            root->param.rot.vy += ACTOR_01100_BODY_TURN_STEP;
        } else if (bearing < -ACTOR_01100_BODY_TURN_STEP) {
            root->param.rot.vy -= ACTOR_01100_BODY_TURN_STEP;
        } else {
            root->param.rot.vy += bearing;
        }
        root->param.rot.vy &= ACTOR_TRANSFORM_ANGLE_MASK;
        gfxRotMatrixY(&root->coord, root->param.rot.vy, 1);
        root->composeStamp = GRAPHICS_COORD_DIRTY;

        if ((u16)(work->playerBearing + (ACTOR_01100_FACING_TOLERANCE - 1)) < (ACTOR_01100_FACING_TOLERANCE * 2 - 1)) {
            if (task->spawnArg1.value != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                attackChoice    = (u16)((gRandomLcgState >> 0x10) % 3);
                if (attackChoice <= 0) {
                    work->state = ACTOR_01100_STATE_PUNCH_LEFT;
                } else if (attackChoice < 2) {
                    work->state = ACTOR_01100_STATE_PUNCH_RIGHT;
                } else {
                    work->state = ACTOR_01100_STATE_SPIT;
                }
            } else if (_actor01100DistSqToPlayer(task->extra.tmd->coords) <= (ACTOR_01100_PUNCH_RANGE_SQUARED - 1)) {
                if (bearing >= 0) {
                    work->state = ACTOR_01100_STATE_PUNCH_RIGHT;
                } else {
                    work->state = ACTOR_01100_STATE_PUNCH_LEFT;
                }
            } else {
                work->state = ACTOR_01100_STATE_ADVANCE;
            }
            work->stateStep = 0;
        }
    }
}

/// Turns toward the player and punches when facing them, or spits after a wait.
///
/// A large variant waits 10 ticks between range checks; the standard waits 60.
/// The wait borrows the task countdown and uses the variant's spit range. Yaw
/// advances at most 16 units per tick, with 4096 per turn. Requires the same
/// composed-coordinate and scratch-stack contract as the bearing/distance helpers.
static void _actor01100FacePlayer(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum { ACTOR_01100_SPIT_WAIT_STANDARD = 60,
           ACTOR_01100_SPIT_WAIT_LARGE    = 10 };
    GfxCoord* root;
    s32       playerBearing;
    u16       rootYaw;
    s32       distanceSquared;
    s8        entryId;
    u16       spitWaitFrames;
    u32       randomState;

    spitWaitFrames = ACTOR_01100_SPIT_WAIT_STANDARD;
    if (work->entryId == ACTOR_01100_ENTRY_LARGE) {
        spitWaitFrames = ACTOR_01100_SPIT_WAIT_LARGE;
    }
    if (work->stateStep == 0) {
        work->motion = ACTOR_01100_MOTION_TURN;
        work->stateStep++;
        task->killCountdown = spitWaitFrames;
    }
    _actor01100TrackPlayerLookYaw(enemy, task, work, scratch);

    playerBearing = work->playerBearing;
    root          = task->extra.tmd->coords;
    if (playerBearing > ACTOR_01100_BODY_TURN_STEP) {
        root->param.rot.vy += ACTOR_01100_BODY_TURN_STEP;
    } else if (playerBearing < -ACTOR_01100_BODY_TURN_STEP) {
        root->param.rot.vy -= ACTOR_01100_BODY_TURN_STEP;
    } else {
        root->param.rot.vy += playerBearing;
    }
    rootYaw            = root->param.rot.vy & ACTOR_TRANSFORM_ANGLE_MASK;
    root->param.rot.vy = rootYaw;
    gfxRotMatrixY(&root->coord, rootYaw, 1);
    root->composeStamp = GRAPHICS_COORD_DIRTY;

    if (work->playerBearing > -ACTOR_01100_FACING_TOLERANCE && work->playerBearing < ACTOR_01100_FACING_TOLERANCE) {
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomState;
        work->state     = ((randomState >> 0x10) & 4) ? ACTOR_01100_STATE_PUNCH_LEFT : ACTOR_01100_STATE_PUNCH_RIGHT;
        sceneEngageBattle(1);
        work->stateStep = 0;
        return;
    }

    task->killCountdown--;
    if (task->killCountdown > 0) {
        return;
    }

    // An expired facing wait can become a ranged attack instead.
    distanceSquared = _actor01100DistSqToPlayer(task->extra.tmd->coords);
    entryId         = work->entryId;
    if (((entryId == ACTOR_01100_ENTRY_STANDARD) && (distanceSquared <= (ACTOR_01100_SPIT_RANGE_SQUARED - 1))) || ((entryId == ACTOR_01100_ENTRY_LARGE) && (distanceSquared <= (ACTOR_01100_LARGE_SPIT_RANGE_SQUARED - 1)))) {
        sceneEngageBattle(1);
        work->state         = ACTOR_01100_STATE_SPIT;
        work->stateStep     = 0;
        task->killCountdown = 0;
        return;
    }
    task->killCountdown = spitWaitFrames;
}

/// Scale `Actor01100_Fn05678` applies to the model's matrix: 0x10 on each axis.
static const VECTOR Actor01100_D000CC = { 0x10, 0x10, 0x10, 0 };

/// Stretches the left arm into a punch aimed at the player's initial distance.
///
/// The upper-arm-to-player offset narrows to signed halfwords; both cached
/// coordinates must share a frame. Below 900 units the Q12 stretch target is
/// zero; above 2700 it is 8192. Between those bounds it is 512 per 100 excess
/// units, reaching 9216 at 2700 before dropping to the far-distance target.
/// The hand sphere is active for frames 22..43. A landed punch usually repeats;
/// a miss advances. Requires per-call scratch and the bearing helper's stack.
static void _actor01100PunchLeft(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_PUNCH_ENABLE_FRAME    = 22,
        ACTOR_01100_PUNCH_CUE_FRAME       = 32,
        ACTOR_01100_PUNCH_STRETCH_FRAME   = 30,
        ACTOR_01100_PUNCH_DISABLE_FRAME   = 44,
        ACTOR_01100_PUNCH_CONNECTED       = 3,
        ACTOR_01100_PUNCH_MIN_DISTANCE    = 900,
        ACTOR_01100_PUNCH_MAX_DISTANCE    = 2700,
        ACTOR_01100_PUNCH_FAR_STRETCH     = ONE * 2,
        ACTOR_01100_PUNCH_MAX_SWELL       = ONE / 2,
        ACTOR_01100_PUNCH_SWELL_STEP      = ONE / 64,
        ACTOR_01100_PUNCH_SWELL_RETRACT   = ONE / 16,
        ACTOR_01100_PUNCH_STRETCH_RETRACT = ONE / 8
    };
    SVECTOR*            playerOffset;
    GfxCoord*           modelCoords;
    GfxCoord*           playerModelCoords;
    GfxCoord*           upperArm;
    GfxCoord*           playerBody;
    GfxCoord*           root;
    WorldCollisionBody* leftHandBody;
    Task*               player;
    s32                 playerDistance;
    s32                 playerBearing;
    u16                 rootYaw;
    u16                 punchFrame;
    u16                 stretchGoal;
    u32                 randomState;

    if (work->stateStep == 0) {
        work->motion       = ACTOR_01100_MOTION_PUNCH_LEFT;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
        // Measure reach from the arm attachment, after rotating its local offset.
        player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (player == NULL) {
            work->stretchGoal = 0;
        } else {
            playerOffset            = &scratch->shortVector;
            modelCoords             = task->extra.tmd->coords;
            playerModelCoords       = player->extra.tmd->coords;
            scratch->shortVector.vx = 0x12C;
            scratch->shortVector.vy = 0;
            scratch->shortVector.vz = 0;
            upperArm                = &modelCoords[ACTOR_01100_PART_LEFT_UPPER_ARM];
            playerBody              = &playerModelCoords[ACTOR_01100_PART_BODY];
            _gfxLoadRotSv(&upperArm->workm, &scratch->shortVector);
            gte_rtv0();
            gte_stsv(playerOffset);
            scratch->shortVector.vx += (u16)playerBody->workm.t[0] - (u16)upperArm->workm.t[0];
            scratch->shortVector.vy += (u16)playerBody->workm.t[1] - (u16)upperArm->workm.t[1];
            scratch->shortVector.vz += (u16)playerBody->workm.t[2] - (u16)upperArm->workm.t[2];
            playerDistance           = SquareRoot0(gfxDotProduct(playerOffset, playerOffset));
            if (playerDistance < ACTOR_01100_PUNCH_MIN_DISTANCE) {
                work->stretchGoal = 0;
            } else if (playerDistance >= (ACTOR_01100_PUNCH_MAX_DISTANCE + 1)) {
                work->stretchGoal = ACTOR_01100_PUNCH_FAR_STRETCH;
            } else {
                work->stretchGoal = ((playerDistance - ACTOR_01100_PUNCH_MIN_DISTANCE) << 9) / 100;
            }
        }
    }
    work->stateCounter = (u16)work->stateCounter + 1;
    _actor01100TrackPlayerLookYaw(enemy, task, work, scratch);
    playerBearing = work->playerBearing;
    root          = task->extra.tmd->coords;
    if (playerBearing >= ACTOR_01100_BODY_TURN_STEP + 1) {
        root->param.rot.vy = (u16)root->param.rot.vy + ACTOR_01100_BODY_TURN_STEP;
    } else if (playerBearing < -ACTOR_01100_BODY_TURN_STEP) {
        root->param.rot.vy = (u16)root->param.rot.vy - ACTOR_01100_BODY_TURN_STEP;
    } else {
        root->param.rot.vy = (u16)root->param.rot.vy + playerBearing;
    }
    rootYaw            = (u16)root->param.rot.vy & ACTOR_TRANSFORM_ANGLE_MASK;
    root->param.rot.vy = rootYaw;
    gfxRotMatrixY(&root->coord, rootYaw, 1);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->stateCounter == ACTOR_01100_PUNCH_ENABLE_FRAME) {
        leftHandBody         = &work->bodies[ACTOR_01100_BODY_LEFT_HAND];
        leftHandBody->key    = damagePackEnemyAttackKey(enemy, ACTOR_01100_ATTACK_PUNCH_LEFT);
        leftHandBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else if (work->stateCounter == ACTOR_01100_PUNCH_CUE_FRAME) {
        sndEvtRequestScriptStart((work->waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | (((u8)work->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT) | ACTOR_01100_SOUND_PUNCH), (s8)scratch->pan, (s8)scratch->depth);
    }
    if (((u32)((u16)work->stateCounter - (ACTOR_01100_PUNCH_ENABLE_FRAME + 1)) < (ACTOR_01100_PUNCH_DISABLE_FRAME - ACTOR_01100_PUNCH_ENABLE_FRAME - 1)) && (work->stateStep == 1) &&
        (worldCollisionCountContactsByKind(work->contacts[ACTOR_01100_BODY_LEFT_HAND], WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0)) {
        work->stateStep = ACTOR_01100_PUNCH_CONNECTED;
    }
    // Swell the shoulder before extending the arm, then retract after the hit window.
    punchFrame = work->stateCounter;
    if ((u32)(punchFrame - 1) < (ACTOR_01100_PUNCH_STRETCH_FRAME - 1)) {
        if (work->leftShoulderSwell < ACTOR_01100_PUNCH_MAX_SWELL) {
            work->leftShoulderSwell = (s16)((u16)work->leftShoulderSwell + ACTOR_01100_PUNCH_SWELL_STEP);
        }
    } else if ((s16)punchFrame >= ACTOR_01100_PUNCH_STRETCH_FRAME) {
        if (work->leftShoulderSwell >= ACTOR_01100_PUNCH_SWELL_RETRACT) {
            work->leftShoulderSwell = (s16)((u16)work->leftShoulderSwell - ACTOR_01100_PUNCH_SWELL_RETRACT);
        } else {
            work->leftShoulderSwell = 0;
        }
    }
    punchFrame = work->stateCounter;
    if ((u32)(punchFrame - ACTOR_01100_PUNCH_STRETCH_FRAME) < (ACTOR_01100_PUNCH_DISABLE_FRAME - ACTOR_01100_PUNCH_STRETCH_FRAME)) {
        stretchGoal = work->stretchGoal;
        if (work->leftArmStretch < (s16)stretchGoal) {
            work->leftArmStretch = (s16)((u16)work->leftArmStretch + ((s16)stretchGoal >> 3));
        }
    } else if ((s16)punchFrame >= ACTOR_01100_PUNCH_DISABLE_FRAME) {
        if (work->leftArmStretch >= ACTOR_01100_PUNCH_STRETCH_RETRACT) {
            work->leftArmStretch = (s16)((u16)work->leftArmStretch - ACTOR_01100_PUNCH_STRETCH_RETRACT);
        } else {
            work->leftArmStretch = 0;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_LEFT_HAND;
    if (work->stateCounter == ACTOR_01100_PUNCH_DISABLE_FRAME) {
        _actor01100DisableHandBodies(work);
    }
    if (work->motionEnded != 0) {
        _actor01100SetSlotRates(work, ANIMATION_RATE_ONE);
        if (work->stateStep != ACTOR_01100_PUNCH_CONNECTED) {
            work->state     = ACTOR_01100_STATE_ADVANCE;
            work->stateStep = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomState;
        if (!((randomState >> 0x10) & 3)) {
            work->state = ACTOR_01100_STATE_ADVANCE;
        } else {
            work->startedMotion = ACTOR_01100_MOTION_IDLE;
        }
        work->stateStep = 0;
    }
}

/// Stretches the right arm into a punch aimed at the player's initial distance.
///
/// Uses the left punch's coordinate and Q12 stretch contract, with the right
/// upper arm and active hand frames 35..59. Tracks the player's look twice per
/// tick, and clears the arm stretch at the animation boundary. A landed punch
/// usually repeats; a miss advances toward the player.
static void _actor01100PunchRight(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_PUNCH_ENABLE_FRAME        = 35,
        ACTOR_01100_PUNCH_SPLASH_REPEAT_FRAME = 47,
        ACTOR_01100_PUNCH_CUE_FRAME           = 45,
        ACTOR_01100_PUNCH_STRETCH_FRAME       = 41,
        ACTOR_01100_PUNCH_DISABLE_FRAME       = 60,
        ACTOR_01100_PUNCH_CONNECTED           = 3,
        ACTOR_01100_PUNCH_MIN_DISTANCE        = 900,
        ACTOR_01100_PUNCH_MAX_DISTANCE        = 2700,
        ACTOR_01100_PUNCH_FAR_STRETCH         = ONE * 2,
        ACTOR_01100_PUNCH_MAX_SWELL           = ONE / 2,
        ACTOR_01100_PUNCH_SWELL_STEP          = ONE / 64,
        ACTOR_01100_PUNCH_SWELL_RETRACT       = ONE / 16,
        ACTOR_01100_PUNCH_STRETCH_RETRACT     = ONE / 8
    };
    SVECTOR*            playerOffset;
    GfxCoord*           modelCoords;
    GfxCoord*           playerModelCoords;
    GfxCoord*           upperArm;
    GfxCoord*           playerBody;
    GfxCoord*           root;
    WorldCollisionBody* rightHandBody;
    Task*               player;
    s32                 playerDistance;
    s32                 playerBearing;
    u16                 rootYaw;
    u16                 punchFrame;
    u16                 stretchGoal;
    u32                 randomState;

    if (work->stateStep == 0) {
        work->motion       = ACTOR_01100_MOTION_PUNCH_RIGHT;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
        // Measure reach from the arm attachment, after rotating its local offset.
        player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (player == NULL) {
            work->stretchGoal = 0;
        } else {
            playerOffset            = &scratch->shortVector;
            modelCoords             = task->extra.tmd->coords;
            playerModelCoords       = player->extra.tmd->coords;
            scratch->shortVector.vx = 0x12C;
            scratch->shortVector.vy = 0;
            scratch->shortVector.vz = 0;
            upperArm                = &modelCoords[ACTOR_01100_PART_RIGHT_UPPER_ARM];
            playerBody              = &playerModelCoords[ACTOR_01100_PART_BODY];
            _gfxLoadRotSv(&upperArm->workm, &scratch->shortVector);
            gte_rtv0();
            gte_stsv(playerOffset);
            scratch->shortVector.vx += (u16)playerBody->workm.t[0] - (u16)upperArm->workm.t[0];
            scratch->shortVector.vy += (u16)playerBody->workm.t[1] - (u16)upperArm->workm.t[1];
            scratch->shortVector.vz += (u16)playerBody->workm.t[2] - (u16)upperArm->workm.t[2];
            playerDistance           = SquareRoot0(gfxDotProduct(playerOffset, playerOffset));
            if (playerDistance < ACTOR_01100_PUNCH_MIN_DISTANCE) {
                work->stretchGoal = 0;
            } else if (playerDistance >= (ACTOR_01100_PUNCH_MAX_DISTANCE + 1)) {
                work->stretchGoal = ACTOR_01100_PUNCH_FAR_STRETCH;
            } else {
                work->stretchGoal = ((playerDistance - ACTOR_01100_PUNCH_MIN_DISTANCE) << 9) / 100;
            }
        }
    }
    _actor01100TrackPlayerLookYaw(enemy, task, work, scratch);
    playerBearing = work->playerBearing;
    root          = task->extra.tmd->coords;
    if (playerBearing >= ACTOR_01100_BODY_TURN_STEP + 1) {
        root->param.rot.vy = (u16)root->param.rot.vy + ACTOR_01100_BODY_TURN_STEP;
    } else if (playerBearing < -ACTOR_01100_BODY_TURN_STEP) {
        root->param.rot.vy = (u16)root->param.rot.vy - ACTOR_01100_BODY_TURN_STEP;
    } else {
        root->param.rot.vy = (u16)root->param.rot.vy + playerBearing;
    }
    rootYaw            = (u16)root->param.rot.vy & ACTOR_TRANSFORM_ANGLE_MASK;
    root->param.rot.vy = rootYaw;
    gfxRotMatrixY(&root->coord, rootYaw, 1);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    work->stateCounter = (u16)work->stateCounter + 1;
    _actor01100TrackPlayerLookYaw(enemy, task, work, scratch);
    if (work->stateCounter == ACTOR_01100_PUNCH_ENABLE_FRAME) {
        rightHandBody         = &work->bodies[ACTOR_01100_BODY_RIGHT_HAND];
        rightHandBody->key    = damagePackEnemyAttackKey(enemy, ACTOR_01100_ATTACK_PUNCH_RIGHT);
        rightHandBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else if (work->stateCounter == ACTOR_01100_PUNCH_CUE_FRAME) {
        sndEvtRequestScriptStart((work->waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | (((u8)work->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT) | ACTOR_01100_SOUND_PUNCH), (s8)scratch->pan, (s8)scratch->depth);
    }
    if (((u32)((u16)work->stateCounter - (ACTOR_01100_PUNCH_ENABLE_FRAME + 1)) < (ACTOR_01100_PUNCH_DISABLE_FRAME - ACTOR_01100_PUNCH_ENABLE_FRAME - 1)) && (work->stateStep == 1) &&
        (worldCollisionCountContactsByKind(work->contacts[ACTOR_01100_BODY_RIGHT_HAND], WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0)) {
        work->stateStep = ACTOR_01100_PUNCH_CONNECTED;
    }
    // Swell the shoulder before extending the arm, then retract after the hit window.
    punchFrame = work->stateCounter;
    if ((u32)(punchFrame - 1) < (ACTOR_01100_PUNCH_STRETCH_FRAME - 1)) {
        if (work->rightShoulderSwell < ACTOR_01100_PUNCH_MAX_SWELL) {
            work->rightShoulderSwell = (s16)((u16)work->rightShoulderSwell + ACTOR_01100_PUNCH_SWELL_STEP);
        }
    } else if ((s16)punchFrame >= ACTOR_01100_PUNCH_STRETCH_FRAME) {
        if (work->rightShoulderSwell >= ACTOR_01100_PUNCH_SWELL_RETRACT) {
            work->rightShoulderSwell = (s16)((u16)work->rightShoulderSwell - ACTOR_01100_PUNCH_SWELL_RETRACT);
        } else {
            work->rightShoulderSwell = 0;
        }
    }
    punchFrame = work->stateCounter;
    if ((u32)(punchFrame - ACTOR_01100_PUNCH_STRETCH_FRAME) < (ACTOR_01100_PUNCH_DISABLE_FRAME - ACTOR_01100_PUNCH_STRETCH_FRAME)) {
        stretchGoal = work->stretchGoal;
        if (work->rightArmStretch < (s16)stretchGoal) {
            work->rightArmStretch = (s16)((u16)work->rightArmStretch + ((s16)stretchGoal >> 3));
        }
    } else if ((s16)punchFrame >= ACTOR_01100_PUNCH_DISABLE_FRAME) {
        if (work->rightArmStretch >= ACTOR_01100_PUNCH_STRETCH_RETRACT) {
            work->rightArmStretch = (s16)((u16)work->rightArmStretch - ACTOR_01100_PUNCH_STRETCH_RETRACT);
        } else {
            work->rightArmStretch = 0;
        }
    }
    if (work->stateCounter == ACTOR_01100_PUNCH_SPLASH_REPEAT_FRAME) {
        scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    }
    scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    if (work->stateCounter == ACTOR_01100_PUNCH_DISABLE_FRAME) {
        _actor01100DisableHandBodies(work);
    }
    if (work->motionEnded != 0) {
        _actor01100SetSlotRates(work, ANIMATION_RATE_ONE);
        work->rightArmStretch = 0;
        if (work->stateStep != ACTOR_01100_PUNCH_CONNECTED) {
            work->state     = ACTOR_01100_STATE_ADVANCE;
            work->stateStep = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomState;
        if (!((randomState >> 0x10) & 3)) {
            work->state = ACTOR_01100_STATE_ADVANCE;
        } else {
            work->startedMotion = ACTOR_01100_MOTION_IDLE;
        }
        work->stateStep = 0;
    }
}

/// Countdown handler for `stateStep`. While the previous count is below 0x1F,
/// `leftShoulderSwell` climbs by 0x40 toward 0x800; from 0x20 it falls by 0x80,
/// and each frame is mirrored into `rightShoulderSwell`.
///
/// Frame 0x20 aims a yaw at actor slot 0 — scratchpad delta, transpose,
/// `ratan2`, wrapped into [-0x800, 0x800) — then spawns from
/// `Actor01100_D155E0`. Placement `entryId` 0x31 is a pair of shots; otherwise
/// one fan of three, each at ±0x12C on model part 4, with cue `0x400B000A`. A
/// set `motionEnded` stages state 0xE.
static void Actor01100_Fn04DB4(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    GfxCoord* part;
    Task*     spawned;
    s32       nOuter;
    s32       yaw;
    s32       kind;
    s32       nInner;
    s32       i;
    s32       j;
    s32       dir;
    u16       prev;
    u16       time;

    if (work->stateStep == 0) {
        work->motion       = 8;
        work->stateCounter = 0;
        work->stateStep++;
    }
    prev               = work->stateCounter;
    time               = prev + 1;
    work->stateCounter = time;
    if (prev < 0x1F) {
        if (work->leftShoulderSwell < 0x800) {
            work->leftShoulderSwell += 0x40;
        }
    } else if ((s16)time >= 0x20) {
        if (work->leftShoulderSwell >= 0x80) {
            work->leftShoulderSwell -= 0x80;
        } else {
            work->leftShoulderSwell = 0;
        }
    }
    work->rightShoulderSwell = work->leftShoulderSwell;
    if (work->stateCounter == 0x20) {
        yaw = _actor01100BearingToPlayer(task->extra.tmd->coords);

        kind = 1;
        if (work->entryId == 0x31) {
            kind = 2;
        }
        if (kind == 1) {
            nOuter = 1;
            nInner = 3;
        } else {
            nOuter = 2;
            nInner = 1;
        }
        for (i = 0; i < nOuter; i++) {
            dir = i;
            if (kind == 1) {
                dir = yaw >= 0;
            }
            part = &task->extra.tmd->coords[4];
            for (j = 0; j < nInner; j++) {
                scratch->shortVector.vx = (dir != 0) ? 0x12C : -0x12C;
                scratch->shortVector.vy = 0;
                scratch->shortVector.vz = 0;
                spawned                 = taskSpawnFromTable(Actor01100_D155E0, kind, yaw, 0);
                if (spawned != NULL) {
                    actorRenderCopyCoordBodyTransform(spawned, part, &scratch->shortVector);
                    taskReparent(task, spawned);
                }
                sndEvtRequestScriptStart((work->waterRoom << 22) | (((u8)work->placeIndex << 8) | 0x400B000A), (s8)scratch->pan, (s8)scratch->depth);
            }
        }
    }
    _actor01100TrackPlayerLookYaw(enemy, task, work, scratch);
    if (work->motionEnded != 0) {
        work->state     = ACTOR_01100_STATE_ADVANCE;
        work->stateStep = 0;
        work->field_BAA = work->field_BAA + 1;
    }
}

/// Reads the shared scratch-stack cursor as a byte address without reserving storage.
///
/// The scratch stack must be initialized; its returned address may be the empty
/// cursor slot. Only a separately reserved block may be dereferenced as data.
static __inline__ u8* _actor01100ReadScratchCursor(void)
{
    return SCRATCH_STACK_CURSOR(u8);
}

/// Replaces the shared scratch-stack cursor with a byte address.
///
/// `cursor` must stay within the initialized stack, aligned for its next block.
/// A lower address reserves storage; restoring a higher saved cursor releases
/// intervening blocks in reverse order. Performs no clearing or bounds check.
static __inline__ void _actor01100WriteScratchCursor(u8* cursor)
{
    SCRATCH_STACK_CURSOR(u8) = cursor;
}

#define Actor104900_DistToPlayer(arg0, out)                                    \
    {                                                                          \
        u8*       head;                                                        \
        SVECTOR*  vec;                                                         \
        GfxCoord* coord;                                                       \
        Task*     slot;                                                        \
                                                                               \
        slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);                         \
        if (slot == NULL) {                                                    \
            out = 0x7FFFFFFF;                                                  \
        } else {                                                               \
            coord   = slot->extra.tmd->coords;                                 \
            head    = _actor01100ReadScratchCursor();                          \
            vec     = (SVECTOR*)(head - 8);                                    \
            vec->vx = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0];          \
            vec->vy = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1];          \
            _actor01100WriteScratchCursor((u8*)vec);                           \
            vec->vz = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2];          \
            out     = gfxDotProduct(vec, vec);                                 \
            _actor01100WriteScratchCursor(_actor01100ReadScratchCursor() + 8); \
        }                                                                      \
    }

/// Scales a model rotation's forward axis into a signed-halfword displacement.
///
/// Reads matrix column 2 in Q12 and multiplies by `distance` in coordinate
/// units through GTE GPF12, including its signed-halfword saturation. Writes
/// only XYZ of `scaledForward`; its fourth halfword and the matrix stay intact.
/// Inputs must be live and nonoverlapping. Clobbers the GTE's IR and MAC state.
static __inline__ void _actor01100ScaleForwardAxis(const MATRIX* matrix, SVECTOR* scaledForward, s32 distance)
{
    gte_ReadMatrixColumn(matrix, 2, scaledForward);
    gte_lddp(distance);
    gte_ldsv(scaledForward);
    gte_gpf12();
    gte_stsv(scaledForward);
}

/// Queues a walking cue for this enemy at the tick's sampled pan and depth.
///
/// `soundId` is a base script id: water selects bit 22 and the low placement
/// byte supplies bits 8..15. Pan/depth narrow to signed bytes. Reads the live
/// per-call scratch block immediately and retains no pointer to either input.
static __inline__ void _actor01100StrideSound(const _Actor01100Work* work, const _Actor01100Scratch* scratch, s32 soundId)
{
    sndEvtRequestScriptStart((work->waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | soundId | ((u8)work->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT), (s8)scratch->pan, (s8)scratch->depth);
}

/// Turns the walking root by a bounded relative yaw and rebuilds its pure yaw rotation.
///
/// relativeBearing is a signed yaw error in 4096 units per turn. Each call adds
/// at most 16 units in either direction and wraps the stored yaw to 0..4095.
/// Replacing the rotation removes existing pitch, roll and scale; translation
/// stays in the parent frame and the composition cache becomes dirty. Requires
/// a live root; steering runs independently of the movement-freeze gate.
static __inline__ void _actor01100TurnStrideRoot(GfxCoord* root, s32 relativeBearing)
{
    u16 rootYaw;
    if (relativeBearing >= ACTOR_01100_BODY_TURN_STEP + 1) {
        root->param.rot.vy = (u16)root->param.rot.vy + ACTOR_01100_BODY_TURN_STEP;
    } else if (relativeBearing < -ACTOR_01100_BODY_TURN_STEP) {
        root->param.rot.vy = (u16)root->param.rot.vy - ACTOR_01100_BODY_TURN_STEP;
    } else {
        root->param.rot.vy = (u16)root->param.rot.vy + relativeBearing;
    }
    rootYaw            = (u16)root->param.rot.vy & ACTOR_TRANSFORM_ANGLE_MASK;
    root->param.rot.vy = rootYaw;
    gfxRotMatrixY(&root->coord, rootYaw, GRAPHICS_ROTATION_REPLACE);
    root->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Walks toward the player, steering during each stride and choosing a follow-up attack.
///
/// Frames 1..46 turn by at most 16 angle units and apply an approximately
/// 27-unit ground-plane step along the model's Q12 forward axis. Frozen actors
/// still steer and sound their strides. At frame 46, distance, bearing, grid
/// obstruction and random choices select a punch, spit or facing state, or keep
/// walking. Requires composed caches in a common frame and scratch-stack space
/// for one extra SVECTOR while measuring the player distance.
static void _actor01100Advance(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_STRIDE_INACTIVE        = -1,
        ACTOR_01100_STRIDE_FIRST_FRAME     = 1,
        ACTOR_01100_STRIDE_LAST_FRAME      = 46,
        ACTOR_01100_STRIDE_BLOCKED_LIMIT   = 11,
        ACTOR_01100_STRIDE_PUNCH_YAW_LIMIT = 768
    };
    GfxCoord*      modelCoords;
    GfxCoord*      movementRoot;
    GfxCoord*      turnRoot;
    AnimationSlot* bodySlot;
    s32            initialDistanceSquared;
    s32            arrivalDistanceSquared;
    s32            playerBearing;
    s32            punchBearing;
    s32            stepDistance;
    s32            strideFrame;
    s32            strideChoice;
    u32            randomState;

    if (work->stateStep == 0) {
        modelCoords = task->extra.tmd->coords;
        Actor104900_DistToPlayer(modelCoords, initialDistanceSquared);
        if ((task->spawnArg1.value == 0) && (initialDistanceSquared <= (ACTOR_01100_PUNCH_RANGE_SQUARED - 1))) {
            rand();
            work->state     = ACTOR_01100_STATE_FACE_PLAYER;
            work->stateStep = 0;
            return;
        }
        if (initialDistanceSquared <= (ACTOR_01100_ADVANCE_MIN_RANGE_SQUARED - 1)) {
            work->state     = ACTOR_01100_STATE_FACE_PLAYER;
            work->stateStep = 0;
            return;
        }
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomState;
        work->motion    = ACTOR_01100_MOTION_ADVANCE;
        strideChoice    = (randomState >> 16) & 0xF;
        if (strideChoice < 5) {
            work->stateCounter = 1;
        } else if (strideChoice < 0xC) {
            work->stateCounter = 2;
        } else {
            work->stateCounter = 3;
        }
        work->strideFrame = ACTOR_01100_STRIDE_INACTIVE;
        work->stateStep   = (u8)work->stateStep + 1;
    } else {
        bodySlot = &work->rig.slots[1];
        if ((work->rig.slots[1].currentPose.indices.setIndex != work->motion) ||
            (work->strideFrame = (u8)work->strideFrame + 1, (bodySlot->currentPose.indices.recordIndex > bodySlot->nextPose.indices.recordIndex))) {
            work->strideFrame = ACTOR_01100_STRIDE_INACTIVE;
        }
    }

    // Track the clip separately from its motion request so strides restart at a wrap.
    _actor01100TrackPlayerLookYaw(enemy, task, work, scratch);
    if ((u32)((u8)work->strideFrame - ACTOR_01100_STRIDE_FIRST_FRAME) < ACTOR_01100_STRIDE_LAST_FRAME) {
        playerBearing = work->playerBearing;
        turnRoot      = task->extra.tmd->coords;
        _actor01100TurnStrideRoot(turnRoot, playerBearing);
        strideFrame = work->strideFrame;
        // Difference successive truncated distances to avoid cumulative stride drift.
        stepDistance = ((strideFrame - 13) * 900) / 33 - ((strideFrame - 14) * 900) / 33;
        movementRoot = task->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 0) {
            _actor01100ScaleForwardAxis(&movementRoot->coord, &scratch->shortVector, stepDistance);
            movementRoot->coord.t[0]  += scratch->shortVector.vx;
            movementRoot->coord.t[2]  += scratch->shortVector.vz;
            movementRoot->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    if (work->strideFrame == ACTOR_01100_STRIDE_LAST_FRAME) {
        _actor01100StrideSound(work, scratch, ACTOR_01100_SOUND_STRIDE_END);
    } else if (work->strideFrame == ACTOR_01100_STRIDE_FIRST_FRAME) {
        _actor01100StrideSound(work, scratch, ACTOR_01100_SOUND_STRIDE_BEGIN);
    }
    if (work->strideFrame == ACTOR_01100_STRIDE_LAST_FRAME) {
        modelCoords = task->extra.tmd->coords;
        Actor104900_DistToPlayer(modelCoords, arrivalDistanceSquared);
        if (rand() & 7) {
            if (work->blockedFrames >= ACTOR_01100_STRIDE_BLOCKED_LIMIT || work->playerBearing < -ACTOR_01100_STRIDE_PUNCH_YAW_LIMIT || work->playerBearing >= ACTOR_01100_STRIDE_PUNCH_YAW_LIMIT + 1) {
                if (arrivalDistanceSquared <= (ACTOR_01100_SPIT_RANGE_SQUARED - 1)) {
                    work->state = ACTOR_01100_STATE_SPIT;
                } else {
                    work->state = ACTOR_01100_STATE_FACE_PLAYER;
                }
                work->stateStep = 0;
                return;
            }
            if (arrivalDistanceSquared <= (ACTOR_01100_PUNCH_RANGE_SQUARED - 1)) {
                punchBearing = work->playerBearing;
                if (!(rand() & 7)) {
                    punchBearing = -punchBearing;
                }
                if (punchBearing < 0) {
                    work->state = ACTOR_01100_STATE_PUNCH_LEFT;
                } else {
                    work->state = ACTOR_01100_STATE_PUNCH_RIGHT;
                }
                work->stateStep = 0;
            }
        }
    }
}

/// Spawns a detached death-burst model with the enemy's texture placement.
///
/// Stores the borrowed model source in bank 1's mutable descriptor slot before
/// spawning the flying-part effect at the right upper arm. The source geometry
/// must remain loaded for the effect's lifetime. A successful spawn copies the
/// enemy's texture-page and CLUT-row offsets and rebuilds both primitive-buffer
/// halves when a buffer exists. The descriptor keeps the source even on failure.
static __inline__ void _actor01100SpawnBurstPart(Task* task, TmdSource* model)
{
    enum { ACTOR_01100_BURST_MODEL_TASK_INDEX = 0x32,
           ACTOR_01100_BURST_PUFF_SIZE        = 512 };
    EffectWork* effect;
    TmdObject*  enemyModel;
    TmdObject*  partModel;

    // The effect's descriptor, entry 0x32 of bank 1, takes its model from the caller.
    D_800670D0[ACTOR_01100_BURST_MODEL_TASK_INDEX].data.model = model;
    effect                                                    = effectSpawn(EFFECT_FLYING_BODY_PART, &task->extra.tmd->coords[ACTOR_01100_PART_RIGHT_UPPER_ARM], ACTOR_01100_BURST_PUFF_SIZE, 0);
    if (effect != NULL) {
        enemyModel                   = task->extra.tmd;
        partModel                    = effect->task->extra.tmd;
        partModel->texturePageOffset = enemyModel->texturePageOffset;
        partModel->clutRowOffset     = enemyModel->clutRowOffset;
        if (partModel->buffer != NULL) {
            tmdBuildBufferHalf(partModel);
            tmdBuildBufferHalf(partModel);
        }
    }
}

static void Actor01100_Fn05678(
    Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    TmdObject*          extra;
    GfxCoord*           coords;
    PlayerStatus*       status;
    GameActor*          actor;
    VECTOR              scale;
    s16                 time;
    s16                 walk;
    s32                 i;
    WorldCollisionBody* obj;

    extra = task->extra.tmd;
    if (((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 24, 0, 0)) && (work->roomNotified == 0)) {
        actor  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
        status = &gPlayerStatus;
        if ((actor->mode != GAME_ACTOR_MODE_SCRIPTED) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE) && (status->hp > 0)) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            work->roomNotified = 1;
        }
    }

    if (work->stateStep == 0) {
        work->hp  = 0;
        enemy->hp = 0;
        for (i = 0; i < ACTOR_01100_BODY_COUNT; i++) {
            obj         = &work->bodies[i];
            obj->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        }
        if (enemy->spawnState == 0x10) {
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            enemy->spawnState = 0;
        } else {
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
        }
        if (enemy->spawnState == 0) {
            enemy->spawnState = work->hitFromBehind + 1;
        }
        work->mode                    = ACTOR_01100_MODE_DYING;
        work->reaction                = ACTOR_01100_REACTION_DYING;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        if (enemy->spawnState == 3) {
            _actor01100SpawnBurstPart(task, &_gActor01100BruteMossbackBurstArm);
            _actor01100SpawnBurstPart(task, &_gActor01100BruteMossbackBurstHead);
            _actor01100SpawnBurstPart(task, &_gActor01100BruteMossbackBurstArm);
            work->stateCounter = 0x34;
            work->stateStep++;
        } else {
            if (enemy->spawnState == 1) {
                work->motion = 0x11;
            } else {
                work->motion = 0x12;
            }
            work->stateCounter = 0x20;
            work->stateStep++;
        }
    } else if (work->stateStep == 1) {
        time               = work->stateCounter - 1;
        work->stateCounter = time;
        if (time == 0xC) {
            effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords, 5, 0);
        } else if (time <= 0) {
            extra->flags |= TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            work->stateCounter = 0x20;
            work->stateStep++;
        }
    } else if (work->stateStep == 2) {
        time               = work->stateCounter - 1;
        work->stateCounter = time;
        if (time == 0) {
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->stateCounter      = 4;
            work->stateStep++;
        }
    } else {
        time               = work->stateCounter - 1;
        work->stateCounter = time;
        if ((time == 0) && ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(5, 24, 0, 0))) {
            work->mode = ACTOR_01100_MODE_FINISHED;
        }
    }

    walk = work->lookYaw;
    if (walk >= 0x31) {
        work->lookYaw -= 0x30;
    } else if (walk < -0x30) {
        work->lookYaw += 0x30;
    }

    scratch->splashPart = ACTOR_01100_PART_CHEST;
    if (enemy->spawnState == 3) {
        coords = task->extra.tmd->coords;
        scale  = Actor01100_D000CC;
        _gfxScaleMatrixColumns(&coords[3].coord, &scale);
        coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        if ((enemy->spawnState == 3) && !(extra->flags & TMD_OBJECT_SEMI_TRANS)) {
            return;
        }
    }

    if (work->scaleCoord.coord.m[1][1] >= 0x801) {
        work->scaleCoord.coord.m[1][1] -= 0x20;
        work->scaleCoord.composeStamp   = GRAPHICS_COORD_DIRTY;
        work->scaleCoord.coord.t[1]     = work->scaleCoord.coord.t[1] + 2;
    }
}

/// Countdown handler built around `stateCounter`.
///
/// The first frame arms the motion pair: `motion` takes 0x13, or 0x14 while
/// `hitFromBehind` is set, `startedMotion` and `downState` both take 1 and the
/// countdown is zeroed, with `stateStep` stepped either way. Every later frame
/// moves the countdown up by one and, on the frame it reaches 5, cues the
/// 0x400B0003 event - the low byte of `placeIndex` in bits 8..15 and
/// `waterRoom` in bit 22, pan and depth from the scratch block - then parks the
/// countdown at -0x7FFF so it fires only once. The scratch block's `splashPart`
/// takes 3 either way, and `motionEnded` ends the sub-state: while `hp` still
/// counts it keeps the state on the 0x17 motion with the 0x10 pair when
/// `reactionFlags` has no buildup, and stages the 0x14 motion through `mode`
/// when it does; once that count has run out it hands the frame to
/// `Actor01100_Fn05678` on state 0x18 instead.
static void Actor01100_Fn05CFC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    u16 time;

    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = 0x13;
        } else {
            work->motion = 0x14;
        }
        work->startedMotion = 1;
        work->downState     = ACTOR_01100_DOWN_FALLING;
        work->stateCounter  = 0;
        work->stateStep     = (u8)work->stateStep + 1;
    }
    time               = (u16)work->stateCounter + 1;
    work->stateCounter = time;
    if ((s16)time >= 5) {
        sndEvtRequestScriptStart((work->waterRoom << 22) | (((u8)work->placeIndex << 8) | 0x400B0003), (s8)scratch->pan, (s8)scratch->depth);
        work->stateCounter = -0x7FFF;
    }
    scratch->splashPart = ACTOR_01100_PART_CHEST;
    if (work->motionEnded != 0) {
        work->recentDamage = 0;
        if (work->hp > 0) {
            if (!(enemy->reactionFlags & ENEMY_REACTION_BUILDUP)) {
                work->reaction  = ACTOR_01100_REACTION_RISING;
                work->downState = ACTOR_01100_DOWN_RISING;
                work->state     = ACTOR_01100_STATE_RISE;
            } else {
                work->mode     = ACTOR_01100_MODE_REACTING;
                work->reaction = ACTOR_01100_REACTION_STUNNED;
                work->state    = ACTOR_01100_STATE_STUNNED;
            }
            work->stateStep = 0;
            return;
        }
        work->state     = ACTOR_01100_STATE_DEATH;
        work->stateStep = 0;
        Actor01100_Fn05678(enemy, task, work, scratch);
    }
}

/// Creates a moving spit glob, its glow child and its swept attack capsule.
///
/// The spawn argument is launch yaw in 4096 units per turn. Random climb and
/// speed are rotated through the launch coordinate, then the root rotation is
/// reset to identity. The glob starts a 90-tick flight with 150-unit capsule
/// radii and one contact slot, installs collision teardown and runs flight once
/// immediately. Work belongs to the task; the glow is its child for teardown and
/// is also cached in spawnArg2. Allocation failure exits through the current
/// callback. Requires a coordinate body and one temporary scratch SVECTOR.
static void _actor01100SpitGlobInit(Task* task)
{
    enum {
        ACTOR_01100_SPIT_GLOB_RADIUS              = 150,
        ACTOR_01100_SPIT_LAUNCH_Y_BITS            = 0xE000,
        ACTOR_01100_SPIT_LAUNCH_Y_JITTER_MASK     = 0x1FF,
        ACTOR_01100_SPIT_LAUNCH_SCALE_MIN         = 40,
        ACTOR_01100_SPIT_LAUNCH_SCALE_JITTER_MASK = 31,
        ACTOR_01100_SPIT_INITIAL_Y_JITTER_MASK    = 127
    };
    _Actor01100SpitWork*   work;
    WorldCollisionCapsule* capsule;
    GfxCoord*              coord;
    EffectWork*            glow;
    WorldCollisionBody*    body;
    SVECTOR*               launchVector;
    s32                    launchYaw;

    coord = task->extra.coordBody->coord;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }
    task->work = work;
    glow       = effectSpawn(EFFECT_PROJECTILE_GLOW_SPRITE, coord, EFFECT_PROJECTILE_GLOW_NEW, 0);
    if (glow == NULL) {
        taskCallExit(task);
        return;
    }
    task->spawnArg2.pointer = glow->task;
    taskReparent(task, glow->task);
    launchYaw           = task->spawnArg1.value;
    task->killCountdown = ACTOR_01100_SPIT_LIFETIME_TICKS;

    // Build velocity in the launch frame before resetting the projectile rotation.
    launchVector     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    launchVector->vy = ACTOR_01100_SPIT_LAUNCH_Y_BITS - ((gRandomLcgState >> 16) & ACTOR_01100_SPIT_LAUNCH_Y_JITTER_MASK);
    launchVector->vx = rsin(launchYaw);
    launchVector->vz = rcos(launchYaw);

    _gfxRotateSv(&coord->coord, launchVector);

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gte_lddp(((gRandomLcgState >> 16) & ACTOR_01100_SPIT_LAUNCH_SCALE_JITTER_MASK) + ACTOR_01100_SPIT_LAUNCH_SCALE_MIN);
    gte_ldsv(launchVector);
    gte_gpf12();
    gte_stsv(&work->velocity);

    gfxSetRotIdentity(&coord->coord);

    coord->coord.t[0]  += work->velocity.vx;
    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    coord->coord.t[1]  += (gRandomLcgState >> 16) & ACTOR_01100_SPIT_INITIAL_Y_JITTER_MASK;
    coord->coord.t[2]  += work->velocity.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    body                  = &work->body;
    capsule               = &work->capsule;
    body->coord           = coord;
    body->context.capsule = capsule;
    body->pos.vx          = 0;
    body->pos.vy          = 0;
    body->pos.vz          = 0;
    body->radius          = 0;
    body->key             = damagePackAttackKey(&Actor01100_D074D0[0], ACTOR_01100_SPIT_ATTACK_INDEX);
    body->flags           = WORLD_COLLISION_BODY_CAPSULE;

    capsule->contacts   = work->contacts;
    capsule->ends[1].vx = 0;
    capsule->ends[1].vy = 0;
    capsule->ends[1].vz = 0;
    capsule->ends[0].vx = 0;
    capsule->ends[0].vy = 0;
    capsule->ends[0].vz = 0;
    capsule->end0Radius = ACTOR_01100_SPIT_GLOB_RADIUS;
    capsule->end1Radius = ACTOR_01100_SPIT_GLOB_RADIUS;
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
    body->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    // Install collision teardown only after the capsule joins the world list.
    task->exitCallback = _actor01100SpitExit;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    task->state += 1;
    _actor01100SpitGlobFly(task);
}

/// Ends the glob's collision tests and starts its post-impact countdown.
///
/// Requests the impact sound at `soundCoord`; `waterRoom` is 0 or 1 and selects
/// its sound variant. Keeps the body linked for the exit callback to unlink.
/// Requires live glob work and a composed audio coordinate. Replaces its
/// lifetime with 30 ticks and advances to the following countdown state; the
/// flight handler consumes the first tick immediately after this returns.
static __inline__ void _actor01100SpitGlobImpact(Task* task, _Actor01100SpitWork* work, GfxCoord* soundCoord, s32 waterRoom)
{
    enum { ACTOR_01100_SPIT_IMPACT_SOUND      = 0x400B000B,
           ACTOR_01100_SPIT_POST_IMPACT_TICKS = 30,
           ACTOR_01100_SOUND_WATER_SHIFT      = 22,
           ACTOR_01100_SOUND_PLACE_SHIFT      = 8 };
    s32 soundId;

    soundId = (waterRoom << ACTOR_01100_SOUND_WATER_SHIFT) | (ACTOR_01100_SPIT_IMPACT_SOUND | (Actor01100_D15670 << ACTOR_01100_SOUND_PLACE_SHIFT));
    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(soundCoord), (s8)worldCoordGetOriginAudioDepth(soundCoord));
    work->body.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->killCountdown = ACTOR_01100_SPIT_POST_IMPACT_TICKS;
    task->state        += 1;
}

/// Advances a spit glob's swept capsule and handles impact or flight expiry.
///
/// Runs movement and its lifetime only while combat actors run. Velocity is
/// in world units per tick; gravity adds 10 to Y after movement. The single
/// contact ends flight: a player/companion hit or a response Y >= -3072 requests
/// the glow child's particle burst (3); response Y below -3072 requests its
/// animated quad burst (2). Impact starts a 30-tick countdown, stepped once here.
/// Requires initialized glob work, a model root and a one-entry contact table.
static void _actor01100SpitGlobFly(Task* task)
{
    enum { ACTOR_01100_SPIT_GRAVITY        = 10,
           ACTOR_01100_SPIT_FLOOR_NORMAL_Y = -0xC00 };
    _Actor01100SpitWork*   work;
    WorldCollisionCapsule* capsule;
    WorldCollisionContact* contacts;
    GfxCoord*              coord;
    Task*                  glowTask;
    u32                    stageAreaKey;
    s32                    waterRoom;
    s16                    countdown;

    work          = task->work;
    stageAreaKey  = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    coord         = task->extra.tmd->coords;
    capsule       = &work->capsule;
    waterRoom     = stageAreaKey == GAME_LOCATION_KEY(3, 32, 0, 0);
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        // The trailing capsule end spans the movement before gravity changes it.
        capsule->ends[1].vx = -work->velocity.vx;
        capsule->ends[1].vy = -work->velocity.vy;
        capsule->ends[1].vz = -work->velocity.vz;
        coord->coord.t[0]  += work->velocity.vx;
        contacts            = work->contacts;
        coord->coord.t[1]  += work->velocity.vy;
        coord->coord.t[2]  += work->velocity.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->velocity.vy   = work->velocity.vy + ACTOR_01100_SPIT_GRAVITY;
        // Signal the glow child before disabling the glob for its countdown state.
        if (worldCollisionCountContactsByKind(contacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
            glowTask = task->firstChild;
            if (glowTask != NULL) {
                glowTask->spawnArg1.value = EFFECT_PROJECTILE_GLOW_PARTICLE_BURST;
            }
            _actor01100SpitGlobImpact(task, work, coord, waterRoom);
        } else if (worldCollisionFindContactIndex(contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            glowTask = task->firstChild;
            if (glowTask != NULL) {
                if (contacts->response.direction.vy >= ACTOR_01100_SPIT_FLOOR_NORMAL_Y) {
                    glowTask->spawnArg1.value = EFFECT_PROJECTILE_GLOW_PARTICLE_BURST;
                } else {
                    glowTask->spawnArg1.value = EFFECT_PROJECTILE_GLOW_QUAD_BURST;
                }
            }
            _actor01100SpitGlobImpact(task, work, coord, waterRoom);
        }
        worldCollisionClearContacts(work->contacts);
        countdown           = task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            taskCallExit(task);
        }
    }
}

/// Creates a stationary spit cloud and links its spherical attack body.
///
/// A 750-unit sphere is placed 48 units above the launch point with one contact
/// slot. The 90 active-tick lifetime includes the tick run here. Smoke tasks are
/// children of the cloud for teardown; smoke allocation failure leaves the cloud
/// active. Work belongs to the task, and its exit callback unlinks the body.
/// Requires a coordinate body and initialized world collision lists.
static void _actor01100SpitCloudInit(Task* task)
{
    enum {
        ACTOR_01100_SPIT_CLOUD_RADIUS   = 750,
        ACTOR_01100_SPIT_CLOUD_Y_OFFSET = 48,
        ACTOR_01100_SOUND_SPIT_CLOUD    = 0x400B000C
    };
    EffectWork*            smoke;
    s32                    waterRoom;
    s32                    soundBaseId;
    WorldCollisionContact* contacts;
    _Actor01100SpitWork*   work;
    s32                    stageAreaKey;
    s32                    soundId;
    s32                    pan;
    WorldCollisionBody*    body;
    GfxCoord*              coord;

    coord        = task->extra.coordBody->coord;
    stageAreaKey = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
    waterRoom    = stageAreaKey == GAME_LOCATION_KEY(3, 32, 0, 0);
    work         = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }
    task->work  = work;
    soundBaseId = (waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | ACTOR_01100_SOUND_SPIT_CLOUD;
    soundId     = soundBaseId | (Actor01100_D15670 << ACTOR_01100_SOUND_PLACEMENT_SHIFT);
    pan         = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    smoke = effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_01100_SPIT_CLOUD_SMOKE_ARGUMENT, NULL);
    if (smoke != NULL) {
        taskReparent(task, smoke->task);
    }
    task->killCountdown = ACTOR_01100_SPIT_LIFETIME_TICKS;
    body                = &work->body;
    gfxSetRotIdentity(&coord->coord);
    contacts               = work->contacts;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]     += ACTOR_01100_SPIT_CLOUD_Y_OFFSET;
    body->coord            = coord;
    body->context.contacts = contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->radius           = ACTOR_01100_SPIT_CLOUD_RADIUS;
    body->key              = damagePackAttackKey(Actor01100_D074F8, ACTOR_01100_SPIT_ATTACK_INDEX);
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionInitContacts(contacts, ARRAY_SIZE(work->contacts), 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
    body->flags       |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = _actor01100SpitExit;
    task->state++;
    _actor01100SpitCloudTick(task);
}

/// Dispatches the enemy task with its enemy record, typed work and temporary scratch.
///
/// Task states 0..2 initialize, wait to arm collisions, then tick the enemy.
/// The descriptor creates its model; spawnArg2 carries the enemy. Initialization
/// may receive NULL work. Scratch is reserved only for the handler call, with
/// splashPart reset to none, then released even when the handler exits the task.
static void _actor01100Task(Task* task)
{
    _Actor01100TaskStateTable taskStates;
    Enemy*                    enemy;
    _Actor01100Work*          work;
    _Actor01100Scratch*       scratch;

    taskStates = Actor01100_D00004;
    enemy      = task->spawnArg2.pointer;
    work       = task->work;
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_Actor01100Scratch);

    scratch->splashPart = 0;
    taskStates.funcs[task->state](enemy, task, work, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01100Scratch);
}

/// State handlers of the secondary task this entry spawns: set-up
/// (`_actor01100SpitGlobInit`), per-frame tick (`_actor01100SpitGlobFly`) and the
/// countdown to exit (`_actor01100SpitGlobCountdown`).
static const TaskFuncTable3 Actor01100_D000DC = { {
    _actor01100SpitGlobInit,
    _actor01100SpitGlobFly,
    _actor01100SpitGlobCountdown,
} };

/// Dispatches the spit glob's initialization, flight or post-impact countdown.
///
/// Task state must be 0..2. The coordinate-body descriptor supplies the root;
/// initialization allocates work and installs the collision exit callback.
static void _actor01100SpitGlobTask(Task* task)
{
    TaskFuncTable3 states;

    states = Actor01100_D000DC;
    states.funcs[task->state](task);
}

/// Dispatches the spit cloud's initialization or active-lifetime tick.
///
/// Task state must be 0 or 1. The coordinate-body descriptor supplies the root;
/// initialization allocates work and installs the collision exit callback.
static void _actor01100SpitCloudTask(Task* task)
{
    TaskFunc states[2] = {
        _actor01100SpitCloudInit,
        _actor01100SpitCloudTick,
    };

    states[task->state](task);
}

/// Unlinks the enemy's four bodies and restores its model hierarchy for teardown.
///
/// Requires initialized enemy work. Reparents part 1 to the root so the model
/// no longer refers to the work block's scale coordinate before `enemyDestroy`
/// releases the enemy task's resources.
static void _actor01100Exit(Task* task)
{
    _Actor01100Work* work;
    Enemy*           enemy;
    GfxCoord*        root;
    s32              bodyIndex;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    for (bodyIndex = 0; bodyIndex < ACTOR_01100_BODY_COUNT; bodyIndex++) {
        worldCollisionUnlinkBody(&work->bodies[bodyIndex]);
    }
    root           = task->extra.tmd->coords;
    root[1].parent = root;
    enemyDestroy(enemy, task);
}

/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` for the Mossback's model and bodies.
///
/// `drawEnabled` is 0 to hide or 1 to show; unchanged requests do nothing.
/// Hiding saves the lock-on flags, prevents locking and disables every body's
/// grid/pair tests. Showing restores those flags and enables only the root and
/// chest bodies; hand attacks stay state-controlled. Requires initialized work.
/// Ignores the message ID and second payload and returns zero.
static s32 _actor01100SetModelDraw(Task* task, s32 unusedMessageId, s32 drawEnabled, s32 unusedSecondArg)
{
    _Actor01100Work*    work;
    Enemy*              enemy;
    TmdObject*          model;
    WorldCollisionBody* body;
    s32                 bodyIndex;
    s32                 hidden;

    hidden = drawEnabled ^ 1;
    work   = task->work;
    model  = task->extra.tmd;
    enemy  = task->spawnArg2.pointer;
    if (work->hidden != hidden) {
        work->hidden = hidden;
        if (work->hidden == 0) {
            model->flags                 &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = work->savedTargetFlags;
            body                          = &work->bodies[ACTOR_01100_BODY_ROOT];
            body->flags                  |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            body                          = &work->bodies[ACTOR_01100_BODY_CHEST];
            body->flags                  |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
        } else {
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->savedTargetFlags        = enemy->node.state.parts.flags;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            for (bodyIndex = 0; bodyIndex < ACTOR_01100_BODY_COUNT; bodyIndex++) {
                body         = &work->bodies[bodyIndex];
                body->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
        }
    }
    return 0;
}

/// Scales a matrix's three basis columns by the matching Q12 vector components.
///
/// Uses the GTE's saturating signed-halfword GPF result (4096 is unit scale).
/// Translation stays intact. Matrix and scale must be live and disjoint;
/// borrows one `SVECTOR` from the initialized scratch stack until return.
static void _actor01100ScaleMatrixColumns(MATRIX* matrix, VECTOR* scale)
{
    void**   cursorSlot;
    SVECTOR* column;

    cursorSlot                           = SCRATCH_HEAD_ADDR;
    column                               = SCRATCH_HEAD_AT(cursorSlot, SVECTOR) - 1;
    SCRATCH_HEAD_AT(cursorSlot, SVECTOR) = column;

    SCALE_COL(matrix, column, 0, scale->vx);
    SCALE_COL(matrix, column, 1, scale->vy);
    SCALE_COL(matrix, column, 2, scale->vz);

    SCRATCH_POP_AT(cursorSlot, SVECTOR);
}

/// Returns a player or companion slot's bearing about `self`'s cached axes.
///
/// `playerSlot` is `PLAYER_ACTOR_TASK_PLAYER` or `PLAYER_ACTOR_TASK_COMPANION`.
/// Returns zero for an empty slot. Both composed caches must share a frame;
/// position narrowing and Q12 rotation follow `_actorAngleBearingInFrame`.
/// The result is in [-2048, 2048], with 4096 units per turn. Borrows 64 bytes
/// from the initialized scratch stack and releases them before return.
static s32 _actor01100BearingToPlayerSlot(const GfxCoord* self, s32 playerSlot)
{
    Task*                playerTask;
    GfxCoord*            playerCoord;
    ActorBearingScratch* scratch;
    s32                  angle;

    playerTask = gPlayerActorTasks[playerSlot];
    if (playerTask == NULL) {
        return 0;
    }
    playerCoord = playerTask->extra.tmd->coords;
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    angle       = _actorAngleBearingInFrame(scratch, self, playerCoord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return angle;
}

/// Returns squared cached-frame distance to the registered player task.
///
/// Returns `INT_MAX` when its slot is empty. XYZ differences use the low 16
/// bits of each translation and narrow to signed halfwords before the GTE dot
/// product. Both caches must share a frame; the sum must fit signed 32 bits for
/// distance comparisons. Borrows one `SVECTOR` from the scratch stack until return.
static s32 _actor01100MeasurePlayerDistanceSquared(const GfxCoord* self)
{
    void**    cursorSlot;
    SVECTOR*  cursor;
    SVECTOR*  delta;
    GfxCoord* playerCoord;
    Task*     playerTask;
    s32       distanceSquared;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask != NULL) {
        playerCoord                          = playerTask->extra.tmd->coords;
        cursorSlot                           = SCRATCH_HEAD_ADDR;
        cursor                               = SCRATCH_HEAD_AT(cursorSlot, SVECTOR);
        delta                                = cursor - 1;
        delta->vx                            = (u16)playerCoord->workm.t[0] - (u16)self->workm.t[0];
        delta->vy                            = (u16)playerCoord->workm.t[1] - (u16)self->workm.t[1];
        SCRATCH_HEAD_AT(cursorSlot, SVECTOR) = delta;
        delta->vz                            = (u16)playerCoord->workm.t[2] - (u16)self->workm.t[2];
        distanceSquared                      = gfxDotProduct(delta, delta);
        SCRATCH_POP_AT(cursorSlot, SVECTOR);
    } else {
        distanceSquared = INT_MAX;
    }
    return distanceSquared;
}

/// Moves a coordinate along its Q12 forward axis on the ground plane.
///
/// Distance is in parent-coordinate units. GTE GPF12 narrows and saturates the
/// scaled column into the caller's short-vector workspace; only X/Z translation
/// changes and the cache is marked dirty. Movement runs only when actorsFrozen
/// is zero. Requires live coordinate and scratch storage and clobbers the GTE.
/// This retained standalone body currently has no caller.
static void _actor01100StepForward(GfxCoord* coord, _Actor01100Scratch* scratch, s32 distance)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 0) {
        _actor01100ScaleForwardAxis(&coord->coord, &scratch->shortVector, distance);
        coord->coord.t[0]  += scratch->shortVector.vx;
        coord->coord.t[2]  += scratch->shortVector.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Points the enemy's body at the model's fourth part coordinate - the same
/// `TmdObject::coords[3]` that `worldTargetUpdatePlayerRelativePositions` reads back through
/// `Enemy.coord` - and sets the body position the actor spawns inside, and
/// clears the lock-on node's flags.
///
/// The restart path then needs three things at once: `_actor01100ProcessHits` idle,
/// `Task::spawnArg1` clear, and the work block's trigger pair (`prevMode`,
/// `motionEnded`) both at 1. With them, and only while the squared distance to
/// the player's slot-3 coordinate stays above 0xA62B10, the 0xC000 pair is
/// masked back out of both `WorldCollisionBody` nodes in the motion block, and
/// one LCG draw picks the next state: 4 for three draws in four, else 0.
/// `stateStep` is cleared either way, so the sub-state re-arms from the top.
static void Actor01100_Fn06C0C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    WorldTargetNode* lockNode;
    u8               trigger;

    lockNode                            = &enemy->node;
    enemy->node.state.parts.flags       = 0;
    GP_NODE_ENEMY(lockNode)->coord      = &task->extra.tmd->coords[3];
    GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
    GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
    GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
    if ((_actor01100ProcessHits(enemy, task, work, scratch) == 0) && (task->spawnArg1.value == 0)) {
        trigger = work->prevMode;
        if ((trigger == 1) && (work->motionEnded == trigger)) {
            if (_actor01100MeasurePlayerDistanceSquared(task->extra.tmd->coords) > 0xA62B10) {
                _actor01100DisableHandBodies(work);
                work->mode      = ACTOR_01100_MODE_UNAWARE;
                gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 0x10) & 0xF) < 0xC) {
                    work->state = ACTOR_01100_STATE_IDLE_REST;
                } else {
                    work->state = ACTOR_01100_STATE_IDLE;
                }
                work->stateStep = 0;
            }
        }
    }
}

/// Decays the two arm stretches and the two shoulder swells - the axis pair by
/// 0x400, the two after them by 0x100, each clamped at zero once it falls below
/// its step - and walks `lookYaw` 0x30 back toward zero from either end of the
/// +-0x30 band. `downState` gates the walk, `reaction` the whole block, which
/// is why the locals read signed for the test and unsigned for the step.
///
/// Either way the link transform is re-armed exactly as `Actor01100_Fn06C0C`
/// arms it - model part 3 through `TmdObject::coords[3]` as `coord`, the
/// 0xC8-box local offset through `src` - and `_actor01100ProcessHits` runs last.
static void Actor01100_Fn06D3C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    WorldTargetNode* lockNode;
    s16              walk;

    if (work->reaction != ACTOR_01100_REACTION_TWITCH) {
        if (work->rightArmStretch >= 0x400) {
            work->rightArmStretch = (s16)((u16)work->rightArmStretch - 0x400);
        } else {
            work->rightArmStretch = 0;
        }
        if (work->leftArmStretch >= 0x400) {
            work->leftArmStretch = (s16)((u16)work->leftArmStretch - 0x400);
        } else {
            work->leftArmStretch = 0;
        }
        if (work->leftShoulderSwell >= 0x100) {
            work->leftShoulderSwell = (s16)((u16)work->leftShoulderSwell - 0x100);
        } else {
            work->leftShoulderSwell = 0;
        }
        if (work->rightShoulderSwell >= 0x100) {
            work->rightShoulderSwell = (s16)((u16)work->rightShoulderSwell - 0x100);
        } else {
            work->rightShoulderSwell = 0;
        }
        if (work->downState != ACTOR_01100_DOWN_STANDING) {
            walk = work->lookYaw;
            if (walk >= 0x31) {
                work->lookYaw = (s16)((u16)work->lookYaw - 0x30);
            } else if (walk < -0x30) {
                work->lookYaw = (s16)((u16)work->lookYaw + 0x30);
            }
        }
    }
    lockNode                            = &enemy->node;
    GP_NODE_ENEMY(lockNode)->coord      = &task->extra.tmd->coords[3];
    GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
    GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
    GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
    _actor01100ProcessHits(enemy, task, work, scratch);
}

/// Waits in the idle motion, then randomly selects an arm swing, turn or rest.
///
/// The signed-halfword countdown starts at 2, 60, 120 or 180 ticks and includes
/// this call's decrement. Shared by the unused state-table slots as well as idle.
static void _actor01100Idle(Enemy* unusedEnemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    enum {
        ACTOR_01100_IDLE_WAIT_BRIEF  = 2,
        ACTOR_01100_IDLE_WAIT_SHORT  = 60,
        ACTOR_01100_IDLE_WAIT_MEDIUM = 120,
        ACTOR_01100_IDLE_WAIT_LONG   = 180
    };
    s32 waitChoice;
    s32 gestureChoice;
    s16 framesLeft;

    if (work->stateStep == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        waitChoice      = (gRandomLcgState >> 0x10) & 0xF;
        if (waitChoice < 4) {
            work->stateCounter = ACTOR_01100_IDLE_WAIT_BRIEF;
        } else if (waitChoice < 8) {
            work->stateCounter = ACTOR_01100_IDLE_WAIT_SHORT;
        } else if (waitChoice < 0xE) {
            work->stateCounter = ACTOR_01100_IDLE_WAIT_MEDIUM;
        } else {
            work->stateCounter = ACTOR_01100_IDLE_WAIT_LONG;
        }
        work->motion    = ACTOR_01100_MOTION_IDLE;
        work->stateStep = (u8)work->stateStep + 1;
    }
    framesLeft         = (u16)work->stateCounter - 1;
    work->stateCounter = framesLeft;
    if (framesLeft == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gestureChoice   = (gRandomLcgState >> 0x10) & 0xF;
        if (gestureChoice < 3) {
            work->state = ACTOR_01100_STATE_IDLE_SWING_LEFT;
        } else if (gestureChoice < 6) {
            work->state = ACTOR_01100_STATE_IDLE_SWING_RIGHT;
        } else if (gestureChoice < 0xE) {
            work->state = ACTOR_01100_STATE_IDLE_TURN;
        } else {
            work->state = ACTOR_01100_STATE_IDLE_REST;
        }
        work->stateStep = 0;
    }
}

/// Plays the idle rest motion with a delayed cue and alternating hand splash checks.
///
/// The cue sounds after 100 subsequent ticks. Animation completion returns to
/// idle; the display animation-frame parity chooses the splash hand each tick.
static void _actor01100IdleRest(Enemy* unusedEnemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum { ACTOR_01100_REST_CUE_DELAY = 100 };
    if (work->stateStep == 0) {
        work->motion       = ACTOR_01100_MOTION_REST;
        work->stateCounter = ACTOR_01100_REST_CUE_DELAY;
        work->stateStep    = (u8)work->stateStep + 1;
        return;
    }
    if (work->stateCounter != 0) {
        work->stateCounter--;
        if (work->stateCounter == 0) {
            sndEvtRequestScriptStart((work->waterRoom << ACTOR_01100_SOUND_WATER_VARIANT_SHIFT) | (((u8)work->placeIndex << ACTOR_01100_SOUND_PLACEMENT_SHIFT) | ACTOR_01100_SOUND_REST), (s8)scratch->pan, (s8)scratch->depth);
        }
    }
    if (gDisplayState.animFrame & 1) {
        scratch->splashPart = ACTOR_01100_PART_LEFT_HAND;
    } else {
        scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    }
    if (work->motionEnded == 1) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Arms the 0x15 / 0x16 motion pair on the first frame of the sub-state. Once
/// slot 1 reports `ANIMATION_SLOT_FOLLOWED_JUMP`, it clears `recentDamage`.
/// While `hp` is still positive and `reactionFlags` value 2 is clear, it moves
/// to motion 0x17; once that count has run out it hands the frame to
/// `Actor01100_Fn05678` on motion 0x18.
static void Actor01100_Fn07014(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    AnimationSlot* motion = &work->rig.slots[1];

    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = 0x15;
        } else {
            work->motion = 0x16;
        }
        work->startedMotion = 1;
        work->stateCounter  = 0xA;
        work->stateStep     = (u8)work->stateStep + 1;
    }
    if (motion->status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
        work->recentDamage = 0;
        if (work->hp > 0) {
            if (!(enemy->reactionFlags & ENEMY_REACTION_BUILDUP)) {
                work->mode      = ACTOR_01100_MODE_ENGAGED;
                work->reaction  = ACTOR_01100_REACTION_RISING;
                work->state     = ACTOR_01100_STATE_RISE;
                work->stateStep = 0;
                work->downState = ACTOR_01100_DOWN_RISING;
            }
        } else {
            work->state     = ACTOR_01100_STATE_DEATH;
            work->stateStep = 0;
            Actor01100_Fn05678(enemy, task, work, scratch);
        }
    }
}

/// Plays the directional hit recoil, then clears its reaction and faces the player.
///
/// Restarts the front or rear flinch clip according to the last hit direction.
/// Completion clears accumulated damage and restores the engaged mode.
static void _actor01100Flinch(Enemy* unusedEnemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = ACTOR_01100_MOTION_FLINCH_FRONT;
        } else {
            work->motion = ACTOR_01100_MOTION_FLINCH_BEHIND;
        }
        work->startedMotion = ACTOR_01100_MOTION_IDLE;
        work->stateStep     = (u8)work->stateStep + 1;
    }
    if (work->motionEnded != 0) {
        work->mode         = ACTOR_01100_MODE_ENGAGED;
        work->recentDamage = 0;
        work->reaction     = ACTOR_01100_REACTION_NONE;
        work->state        = ACTOR_01100_STATE_FACE_PLAYER;
        work->stateStep    = 0;
    }
}

/// Countdown handler built around `stateCounter`, the entry before
/// `_actor01100Rise` in `Actor01100_D00064`.
///
/// The first frame arms the motion pair: `motion` takes 0xA, or 0xD while
/// `hitFromBehind` is set, `downState` takes 1 and the countdown is zeroed,
/// with `stateStep` stepped in both cases. Every later frame moves the
/// countdown up by one and, on the frame it reaches 0xD, cues the 0x400B0003
/// event - the low byte of `placeIndex` in bits 8..15 and `waterRoom` in bit
/// 22, pan and depth from the scratch block - then parks the countdown at -0x7FFF
/// so it fires only once. The scratch block's `splashPart` takes 3 either
/// way, and `motionEnded` ends the sub-state: while `hp` still counts it keeps
/// the state on the 0x17 motion with the 0x10 pair when `reactionFlags` has no
/// buildup, and stages the 0x14 motion through `mode` when it does; once that
/// count has run out it hands the frame to `Actor01100_Fn05678` on the 0x18
/// motion instead.
static void Actor01100_Fn07148(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    u16 time;

    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = 0xA;
        } else {
            work->motion = 0xD;
        }
        work->downState    = ACTOR_01100_DOWN_FALLING;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
    }
    time               = (u16)work->stateCounter + 1;
    work->stateCounter = time;
    if ((s16)time >= 0xD) {
        sndEvtRequestScriptStart((work->waterRoom << 22) | (((u8)work->placeIndex << 8) | 0x400B0003), (s8)scratch->pan, (s8)scratch->depth);
        work->stateCounter = -0x7FFF;
    }
    scratch->splashPart = ACTOR_01100_PART_CHEST;
    if (work->motionEnded != 0) {
        work->recentDamage = 0;
        if (work->hp > 0) {
            if (!(enemy->reactionFlags & ENEMY_REACTION_BUILDUP)) {
                work->reaction  = ACTOR_01100_REACTION_RISING;
                work->state     = ACTOR_01100_STATE_RISE;
                work->stateStep = 0;
                work->downState = ACTOR_01100_DOWN_RISING;
                return;
            }
            work->mode      = ACTOR_01100_MODE_REACTING;
            work->reaction  = ACTOR_01100_REACTION_STUNNED;
            work->state     = ACTOR_01100_STATE_STUNNED;
            work->stateStep = 0;
            return;
        }
        work->state     = ACTOR_01100_STATE_DEATH;
        work->stateStep = 0;
        Actor01100_Fn05678(enemy, task, work, scratch);
    }
}

/// Plays the directional rise and restores the enemy's standing combat state.
///
/// The first call starts the front or rear rise motion. Subsequent ticks clear
/// the rising hit-vulnerability state at frame 40 or 60 respectively and watch
/// the chest for splashes. Animation completion clears the reaction and lock-on
/// flags, enters engaged mode and selects facing the player.
static void _actor01100Rise(Enemy* enemy, Task* unusedTask, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    enum {
        ACTOR_01100_MOTION_RISE_FRONT          = 15,
        ACTOR_01100_MOTION_RISE_BEHIND         = 16,
        ACTOR_01100_RISE_FRONT_STANDING_FRAME  = 40,
        ACTOR_01100_RISE_BEHIND_STANDING_FRAME = 60
    };
    u16 riseFrame;

    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = ACTOR_01100_MOTION_RISE_FRONT;
        } else {
            work->motion = ACTOR_01100_MOTION_RISE_BEHIND;
        }
        work->downState    = ACTOR_01100_DOWN_RISING;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
        return;
    }
    riseFrame          = (u16)work->stateCounter + 1;
    work->stateCounter = riseFrame;
    if (work->hitFromBehind == 0) {
        if ((s16)riseFrame == ACTOR_01100_RISE_FRONT_STANDING_FRAME) {
            work->downState = ACTOR_01100_DOWN_STANDING;
        }
    } else {
        if ((s16)riseFrame == ACTOR_01100_RISE_BEHIND_STANDING_FRAME) {
            work->downState = ACTOR_01100_DOWN_STANDING;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_CHEST;
    if (work->motionEnded != 0) {
        enemy->node.state.parts.flags = 0;
        work->mode                    = ACTOR_01100_MODE_ENGAGED;
        work->reaction                = ACTOR_01100_REACTION_NONE;
        work->state                   = ACTOR_01100_STATE_FACE_PLAYER;
        work->stateStep               = 0;
    }
}

/// Counts down a spit glob's remaining post-impact lifetime and exits when expired.
///
/// Runs on every dispatch, including while combat actors are paused. The timer's
/// low 16 bits are decremented and tested as a signed halfword; zero or negative
/// values invoke the installed collision exit callback.
static void _actor01100SpitGlobCountdown(Task* task)
{
    u16 countdownBits;

    countdownBits       = task->killCountdown - 1;
    task->killCountdown = countdownBits;
    if ((countdownBits << 0x10) <= 0) {
        taskCallExit(task);
    }
}

/// Unlinks a spit glob or cloud's collision body before releasing its task resources.
///
/// Requires allocated spit work whose body has been linked. taskKill tears down
/// the task's children and owns release of its work and coordinate body.
static void _actor01100SpitExit(Task* task)
{
    _Actor01100SpitWork* work;

    work = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}

/// Emits spit-cloud smoke and expires its attack body and lifetime on active ticks.
///
/// Above 20 remaining ticks, even ticks emit a smoke child. A player-body contact
/// or reaching 20 ticks disables grid and pair tests; the body stays linked until
/// exit. The signed-halfword lifetime is decremented only while combat actors
/// run. Requires initialized cloud work and its one-entry contact table.
static void _actor01100SpitCloudTick(Task* task)
{
    enum { ACTOR_01100_SPIT_CLOUD_FADE_TICKS = 20 };
    _Actor01100SpitWork* work;
    GfxCoord*            coord;
    WorldCollisionBody*  body;
    EffectWork*          smoke;
    s16                  countdown;

    work  = task->work;
    coord = task->extra.coordBody->coord;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (task->killCountdown >= ACTOR_01100_SPIT_CLOUD_FADE_TICKS + 1) {
            if (((u16)task->killCountdown & 1) == 0) {
                smoke = effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_01100_SPIT_CLOUD_SMOKE_ARGUMENT, NULL);
                if (smoke != NULL) {
                    taskReparent(task, smoke->task);
                }
            }
            if (worldCollisionCountContactsByKind(&work->contacts[0], WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
                body         = &work->body;
                body->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
        }
        if (task->killCountdown == ACTOR_01100_SPIT_CLOUD_FADE_TICKS) {
            body         = &work->body;
            body->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        }
        countdown           = (u16)task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            taskCallExit(task);
        }
    }
}
