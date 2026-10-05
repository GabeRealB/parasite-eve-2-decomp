#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
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

/// Word whose low bits `Actor01100_Fn02960` (bits 0-3) and
/// `Actor01100_Fn06F38` (bit 0) test; what sets it is outside this entry.

/// Actor id the set-up `Actor01100_Fn0097C` stores for the secondary tasks,
/// which shift it into bits 8-15 of their sound ids.
extern u8 Actor01100_D15670;

/// Pair table the spawn state packs into the collision body's `WorldCollisionBody.key`.
extern DamageAttack Actor01100_D074F8[6];

/// Scratchpad stack pointer, initialised by GameMain.

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

static void Actor01100_Fn0097C(Enemy* enemy, Task* task, _Actor01100Work* unusedWork, _Actor01100Scratch* unusedScratch);
static void Actor01100_Fn00CF0(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch);
static s32  Actor01100_Fn00F58(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn02960(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn035E4(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn03740(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn0389C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn039D0(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn03BAC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn041BC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn04410(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn048C8(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn04DB4(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn0516C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn05678(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn05CFC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn05E68(Task* task);
static void Actor01100_Fn06198(Task* task);
static void Actor01100_Fn0638C(Task* task);
static void Actor01100_Fn0668C(Task* task);
static void Actor01100_Fn067C0(MATRIX* arg0, VECTOR* arg1);
static s32  Actor01100_Fn06954(GfxCoord* arg0, s32 arg1);
static s32  Actor01100_Fn06AC8(GfxCoord* arg0);
static void Actor01100_Fn06E4C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn06F38(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn07014(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn070DC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn07148(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn072B8(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void Actor01100_Fn0736C(Task* task);
static void Actor01100_Fn073A8(Task* task);
static void Actor01100_Fn073DC(Task* task);

/// The enemy task's three states - set-up, the per-frame dispatcher and
/// teardown - which `Actor01100_Fn06554` runs by `Task::state`.
static const _Actor01100TaskStateTable Actor01100_D00004 = { {
    Actor01100_Fn0097C,
    Actor01100_Fn00CF0,
    Actor01100_Fn02960,
} };

/// Scale copied onto the stack and passed to `Actor01100_Fn067C0` when the
/// placement `entryId` is 0x31: 0x1400 on each axis.
static const VECTOR Actor01100_D00010 = { 0x1400, 0x1400, 0x1400, 0 };

extern DamageAttack Actor01100_D074F8[6];
s32                 Actor01100_Fn0670C(Task*, s32, s32, s32);
void                Actor01100_Fn06554(Task*);
void                Actor01100_Fn065E4(Task*);
void                Actor01100_Fn0663C(Task*);

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
    { { { TASK_BODY_TMD, 96 } }, Actor01100_Fn06554, { .model = &_gActor01100BruteMossbackBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor01100_Fn065E4, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, Actor01100_Fn0663C, { .value = 0 } },
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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor01100_Fn0670C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8 Actor01100_D15670;

static __inline__ void _actor01100SetSlotRates(_Actor01100Work* work, u8 rate);
static __inline__ s32  _actor01100FindClass2Contact(SVECTOR* out, WorldCollisionContact* contacts);
static __inline__ s32  _actor01100PushOut(GfxCoord* coord, WorldCollisionContact* contacts);
static __inline__ void _actor01100ClearObjPair(_Actor01100Work* work);
static void            Actor01100_Fn01B90(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void            Actor01100_Fn01D98(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static __inline__ s32  _actor01100BearingToPlayer(GfxCoord* self);
static __inline__ s32  _actor01100DistSqToPlayer(GfxCoord* self);
static __inline__ u8*  Actor104900_ScratchRead(void);
static __inline__ void Actor104900_ScratchWrite(u8* p);
static __inline__ void Actor104900_MatrixCol2(MATRIX* arg0, SVECTOR* arg1, s32 scale);
static __inline__ void _actor01100SpawnModelEff(Task* task, TmdSource* model);
static void            Actor01100_Fn06B6C(GfxCoord* arg0, _Actor01100Scratch* arg1, s32 arg2);
static void            Actor01100_Fn06C0C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);
static void            Actor01100_Fn06D3C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch);

#include "../../shared/actor_contacts_find_push.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

/// Sets the playback rate of animation slots 1..20 in both of the work block's
/// animation contexts.
static __inline__ void _actor01100SetSlotRates(_Actor01100Work* work, u8 rate)
{
    AnimationSlot* slot;
    s32            i;

    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        slot       = &work->rig.slots[i];
        slot->rate = rate;
        slot       = &work->flinchRig.slots[i];
        slot->rate = rate;
    }
}

/// First enemy-task state: allocates the work block, enqueues the
/// overlay's sound CD command once while `gSceneCombatState.enemySoundBankQueued` is clear,
/// seeds both animation contexts, and hangs the work coordinate off model
/// part 1. Spawn state 1/2 then writes 0x7F into slots 1..20 of each
/// context. Placement `entryId` 0x31 selects the second param table and
/// scales the identity matrix by 0x1400.
static void Actor01100_Fn0097C(Enemy* enemy, Task* task, _Actor01100Work* unusedWork, _Actor01100Scratch* unusedScratch)
{
    _Actor01100Work*  work;
    TmdObject*        extra;
    GfxCoord*         parts;
    GfxRotationWords* mtx;
    SceneCombatState* combat;
    u8                param1[8];
    u8                param2[8];
    VECTOR            scale;
    s8                entryId;
    u32               placeIndex;
    u32               locationWord;
    u16               hp;
    s32               i;
    GfxCoord*         endCoords;

    extra                  = task->extra.tmd;
    parts                  = extra->coords;
    task->spawnArg1.value &= 0xFFFF0000;
    work                   = memCalloc(sizeof(_Actor01100Work), 0);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }

    locationWord  = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
    locationWord &= GAME_LOCATION_STAGE_AREA_MASK;
    param1[2]     = 0xA;
    param2[0]     = 0xB;
    param1[3]     = 0;
    param2[3]     = 0;
    param2[2]     = 0;
    param2[1]     = 0;
    if (locationWord == GAME_LOCATION_KEY(3, 32, 0, 0)) {
        param1[0]       = 2;
        work->waterRoom = 1;
    } else {
        work->waterRoom = 0;
        param1[0]       = 1;
    }

    combat = &gSceneCombatState;
    if (combat->enemySoundBankQueued == 0) {
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        combat->enemySoundBankQueued = 1;
    }

    task->work    = work;
    entryId       = enemy->place->entryId;
    work->entryId = entryId;
    if (entryId == 0x31) {
        enemy->param = &Actor01100_D07510;
    } else {
        enemy->param = &Actor01100_D074E8;
    }

    placeIndex                = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    work->placeIndex          = placeIndex;
    *(s32*)&Actor01100_D15670 = placeIndex;
    extra->lightMtx           = &work->lightMtx;
    extra->colorMtx           = &work->colorMtx;
    animationInitContext(&work->rig.anim, Actor01100_D15604, extra, work->rig.poses, work->rig.slots);
    animationInitContext(&work->flinchRig.anim, Actor01100_D15604, extra, work->flinchRig.poses, work->flinchRig.slots);
    work->startedMotion = 1;
    work->motion        = 1;

    mtx         = (GfxRotationWords*)&work->scaleCoord.coord;
    mtx->m00M01 = ONE;
    mtx->m02M10 = 0;
    mtx->m11M12 = ONE;
    mtx->m20M21 = 0;
    mtx->m22    = ONE;
    if (work->entryId == 0x31) {
        scale = Actor01100_D00010;
        Actor01100_Fn067C0(&work->scaleCoord.coord, &scale);
    }
    work->scaleCoord.coord.t[0]   = 0;
    work->scaleCoord.coord.t[1]   = 0;
    work->scaleCoord.coord.t[2]   = 0;
    work->scaleCoord.parent       = parts;
    work->scaleCoord.composeStamp = GRAPHICS_COORD_DIRTY;

    hp                                      = enemy->param->hpMax;
    work->hp                                = hp;
    enemy->hp                               = hp;
    task->extra.tmd->coords[1].parent       = &work->scaleCoord;
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;

    i = 1;
    do {
        animationResetSlot(&work->rig.anim, i, work->motion);
        animationResetSlot(&work->flinchRig.anim, i, work->motion);
        i++;
    } while (i < ARRAY_SIZE(work->rig.slots));

    switch (enemy->spawnState) {
        case 1:
            work->hitFromBehind = 0;
            work->state         = ACTOR_01100_STATE_DEATH;
            _actor01100SetSlotRates(work, 0x7F);
            break;
        case 2:
            work->hitFromBehind = 1;
            work->state         = ACTOR_01100_STATE_DEATH;
            _actor01100SetSlotRates(work, 0x7F);
            break;
    }

    sceneAcquireBattleRef(0);
    endCoords                     = task->extra.tmd->coords;
    work->hitEffectArg.spawnArgLo = 0x400;
    work->hitEffectArg.spawnArgHi = 3;
    work->hitEffectArg.coord      = endCoords + 4;
    task->state++;
}

/// Arms the enemy's four collision bodies the first time the state handler runs
/// with the CD command queue idle: the enemy's own link node is put back on the
/// list, node 0 takes the model's root coordinate and node 3 the pose 3 slots
/// along it, both linked as kind 2 with their `flags` halves ORed in and a
/// three-entry collision table each, and nodes 1 and 2 are linked as kind 3
/// with a `Gp_PackObjPair` payload, the first of the two taking pose 0xC and
/// the second pose 8 of the model's 0x50-byte coordinate records. The task then
/// takes `Actor01100_Fn0668C` as its
/// exit callback, the model's hidden bit is lifted, `msgTable` is pointed at
/// this overlay's message table and the state advances.
static void Actor01100_Fn00CF0(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* unusedScratch)
{
    WorldCollisionBody* obj;
    s32                 reach;
    s32                 recOff;
    s32                 i;
    s32                 idx;

    if (CdCmd_IsIdle() & 0xFFFF) {
        worldTargetLinkNode(&enemy->node);
        obj                   = &work->bodies[ACTOR_01100_BODY_ROOT];
        obj->coord            = task->extra.tmd->coords;
        obj->context.contacts = &work->contacts[ACTOR_01100_BODY_ROOT][0];
        obj->pos.vx           = 0;
        obj->pos.vy           = -0x1D8;
        obj->pos.vz           = 0;
        obj->key              = 0x30000;
        obj->radius           = 0x258;
        obj->flags            = WORLD_COLLISION_BODY_SPHERE;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, obj);
        obj->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        worldCollisionInitContacts(obj->context.contacts, 3, 0);

        obj                   = &work->bodies[ACTOR_01100_BODY_CHEST];
        obj->coord            = &task->extra.tmd->coords[3];
        obj->context.contacts = &work->contacts[ACTOR_01100_BODY_CHEST][0];
        obj->pos.vx           = 0;
        obj->pos.vy           = 0;
        obj->pos.vz           = 0;
        obj->key              = 0x3000B;
        obj->radius           = 0x1C2;
        obj->flags            = WORLD_COLLISION_BODY_SPHERE;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, obj);
        obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        worldCollisionInitContacts(obj->context.contacts, 3, 0);
        obj->pos.vx = 0;
        obj->pos.vy = -0xC8;
        obj->pos.vz = 0xC8;

        i      = 0;
        reach  = 0x12C;
        recOff = OFFSET_OF(_Actor01100Work, contacts[ACTOR_01100_BODY_LEFT_HAND]);
        obj    = &work->bodies[ACTOR_01100_BODY_LEFT_HAND];
        do {
            idx = 8;
            if (i == 0) {
                idx = 0xC;
            }
            obj->coord            = &task->extra.tmd->coords[idx];
            obj->context.contacts = (WorldCollisionContact*)((u8*)work + recOff);
            do {
                if (i == 0) {
                    obj->pos.vx = -0x12C;
                } else {
                    obj->pos.vx = reach;
                }
                obj->pos.vy = 0;
                obj->pos.vz = 0;
                obj->radius = reach;
                obj->key    = Gp_PackObjPair(enemy, 1);
                obj->flags  = WORLD_COLLISION_BODY_SPHERE;
                worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, obj);
                obj->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                worldCollisionInitContacts(obj->context.contacts, 3, 0);
                recOff += sizeof(work->contacts[0]);
                i++;
                obj = &work->bodies[i + 1];
            } while (0);
        } while (i < 2);

        enemy->recs            = &work->contacts[ACTOR_01100_BODY_CHEST][0];
        task->exitCallback     = Actor01100_Fn0668C;
        task->extra.tmd->flags = (u16)(task->extra.tmd->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW);
        task->msgTable         = Actor01100_D15660;
        task->state++;
        enemy->reactionFlags = 0;
    }
}

/// Scans one three-entry contact table for its first class-2 contact, stopping
/// at the first empty entry. The contact's position is copied into `out` and
/// its key returned; 0 when there is none.
static __inline__ s32 _actor01100FindClass2Contact(SVECTOR* out, WorldCollisionContact* contacts)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (contacts[i].key.value == 0) {
            break;
        }
        if ((contacts[i].key.value & 0xFFFF0000) == 0x20000) {
            out->vx = contacts[i].point.vx;
            out->vy = contacts[i].point.vy;
            out->vz = contacts[i].point.vz;
            return contacts[i].key.value;
        }
    }
    return 0;
}

/// Pushes `coord` out of the world contacts in `contacts` with
/// `func_800E0C10`, stepping each nonzero fractional X/Z delta one unit away
/// from zero, and raises the height by 0x80 for the caller to restore.
/// Returns nonzero when the push moved the model on X or Z; always 0 while
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` is 1.
static __inline__ s32 _actor01100PushOut(GfxCoord* coord, WorldCollisionContact* contacts)
{
    ActorContactPushScratch* head;
    ActorContactPushScratch* block;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
    block        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
    block->moved = 0;
    if (func_800E0C10(contacts, &block->delta, 3, NULL) != 0) {
        coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
        coord->coord.t[1] += block->delta.fixed.vy.halves.integer;
        coord->coord.t[2] += block->delta.fixed.vz.halves.integer;
        if (head[-1].delta.fixed.vx.word & 0xFFFF) {
            if (head[-1].delta.fixed.vx.word > 0) {
                coord->coord.t[0] += 1;
            } else {
                coord->coord.t[0] -= 1;
            }
        }
        if (block->delta.fixed.vz.word & 0xFFFF) {
            if (block->delta.fixed.vz.word > 0) {
                coord->coord.t[2] += 1;
            } else {
                coord->coord.t[2] -= 1;
            }
        }
    }
    coord->coord.t[1] += 0x80;
    if ((block->delta.fixed.vx.word != 0) || (block->delta.fixed.vz.word != 0)) {
        block->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    return block->moved;
}

/// Disables grid and pair tests on both collision objects, retaining their links.
static __inline__ void _actor01100ClearObjPair(_Actor01100Work* work)
{
    s32 i;

    for (i = 0; i < 2; i++) {
        WorldCollisionBody* obj = &work->bodies[i + 1];
        obj->flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
}

/// Per-frame hit handler. Counts down `hitEffectFrames` (re-spawning the hit
/// sparks every eighth frame), then takes the first class-2 contact from the
/// last contact table: its damage is scaled by the source's distance, doubled
/// in state 4 unless the source key has bit 15 set, and quadrupled by a
/// successful `Gp_RollEnemyChance`. Damage-over-time ticks add to it, and
/// `hitCooldown` discards it. Nonzero damage picks a reaction from the id's
/// kind, the damage and `recentDamage`, subtracts from the hit points (playing
/// the death cue and releasing the placement at zero), and stages the
/// reaction's state. Every frame it then pushes the model out of the first
/// contact table and clears all four. Returns 1 when damage landed.
static s32 Actor01100_Fn00F58(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    s32                    damaged;
    s32                    fromBehind;
    s32                    dotDamage;
    s32                    doubleDamage;
    s32                    rollParam;
    s32                    kind7;
    s32                    kind4or6;
    s32                    died;
    s32                    sndId;
    s32                    rate;
    AnimationSlot*         slot;
    WorldCollisionContact* world;
    GfxCoord*              coord;
    s32                    savedY;
    s32                    moved;
    s16                    timer;
    s16                    hp;
    s32                    dist;
    s32                    key;
    s32                    cooldown;
    s32                    kind;
    s32                    i;
    s32                    n;
    s32                    reaction;
    s32                    sparkLevel;
    s32                    sourceKey;
    s32                    yaw;
    s32                    level;
    u32                    idKind;
    u32                    hitDamage;
    u32                    damage;
    u32                    hitKey;
    u8                     flags;
    u8                     downState;
    u8                     staged;

    damage       = 0;
    sparkLevel   = -1;
    damaged      = 0;
    fromBehind   = 0;
    dotDamage    = 0;
    doubleDamage = 0;
    rollParam    = 1;
    kind7        = 0;
    kind4or6     = 0;
    sourceKey    = 0;
    if (work->recentDamage > 0) {
        work->recentDamage = (u16)work->recentDamage - 1;
    }
    if (work->hitEffectFrames > 0) {
        timer                 = (u16)work->hitEffectFrames - 1;
        work->hitEffectFrames = timer;
        if (!(timer & 7)) {
            func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
            func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
        }
    }
    hitKey = _actor01100FindClass2Contact(&scratch->shortVector, work->contacts[ACTOR_01100_BODY_CHEST]);
    if (hitKey != 0) {
        dist = Actor01100_Fn06AC8(task->extra.tmd->coords);
        for (i = 0; i < 3; i++) {
            key = work->contacts[ACTOR_01100_BODY_CHEST][i].key.value;
            if (key != 0) {
                sourceKey = key;
                break;
            }
        }
        dist = SquareRoot0(dist);
        yaw  = Actor01100_Fn06954(task->extra.tmd->coords, (sourceKey >> 7) & 1);
        if (yaw < 0) {
            yaw = -yaw;
        }
        fromBehind = yaw >= 0x401;
        if ((work->state == ACTOR_01100_STATE_IDLE_REST) && !(sourceKey & 0x8000)) {
            doubleDamage = 1;
            if (sparkLevel < 3) {
                sparkLevel = 3;
            }
            rollParam = 5;
        }
        hitDamage = Gp_ComputeDamage(hitKey, (u32)dist, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, hitKey, rollParam) != 0) {
            if (sparkLevel < 0) {
                sparkLevel = 0;
            }
            hitDamage *= 4;
        }
        damage += hitDamage;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        dotDamage = Gp_TickObjFlag4(enemy);
        if (dotDamage > 0) {
            Gp_SpawnEff(EFFECT_HIT_PUFF, &task->extra.tmd->coords[4], 0x11112400, 0);
            damage += dotDamage;
        }
    }
    if (doubleDamage != 0) {
        damage *= 2;
    }
    cooldown = work->hitCooldown;
    if (cooldown > 0) {
        work->hitCooldown = cooldown - 1;
        damage            = 0;
    } else if (hitKey != 0) {
        work->hitCooldown = Gp_GetIdParam2((s32)hitKey);
    }
    if (damage == 0) {
        kind = Gp_GetIdParam0((s32)hitKey) & 0xFFFF;
        if (kind < 0xA) {
            if (kind >= 8) {
                func_800DA6E8(&enemy->node, 0, 0);
            }
        }
    } else if ((s32)damage > 0) {
        died                  = 0;
        work->hitEffectKind   = Gp_GetIdParam1((s32)hitKey) & 0xFFFF;
        reaction              = ACTOR_01100_REACTION_FLINCH;
        work->hitEffectFrames = 0;
        level                 = (u16)work->recentDamage + damage;
        work->recentDamage    = level;
        if ((s32)damage < 0x1D) {
            reaction = ACTOR_01100_REACTION_TWITCH;
        }
        if ((s16)level < 0x3D) {
            level = 1;
        } else if ((s16)level < 0x65) {
            level = 2;
        } else {
            level = 3;
        }
        damaged = 1;
        if (reaction < level) {
            reaction = level;
        }
        idKind = Gp_GetIdParam0((s32)hitKey) & 0xFFFF;
        switch (idKind) {
            case 0:
                break;
            case 1:
                Gp_SetObjFlag1(enemy);
                break;
            case 2:
                if (!(enemy->reactionFlags & ENEMY_REACTION_BUILDUP)) {
                    Gp_SetObjFlag2(enemy, sourceKey, 0);
                    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->reaction != ACTOR_01100_REACTION_STUNNED)) {
                        reaction = ACTOR_01100_REACTION_FALL;
                    }
                } else {
                    Gp_SetObjFlag2(enemy, sourceKey, 0);
                }
                break;
            case 3:
                Gp_SpawnEff(EFFECT_HIT_PUFF, &task->extra.tmd->coords[4], 0x11112400, 0);
                Gp_SetObjFlag4(enemy, sourceKey, 0);
                break;
            case 5:
                if (doubleDamage == 0) {
                    damage *= 2;
                    if (sparkLevel < 2) {
                        sparkLevel = 2;
                    }
                } else {
                    damage += (s32)damage / 2;
                }
                work->hitEffectFrames = 0x1E;
                break;
            case 4:
            case 6:
                kind4or6 = 1;
                break;
            case 7:
                if (doubleDamage == 0) {
                    damage *= 2;
                    if (sparkLevel < 2) {
                        sparkLevel = 2;
                    }
                } else {
                    damage += (s32)damage / 2;
                }
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[4], 0x80023300, 0);
                kind7 = 1;
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[4], 0x80023300, 0);
                break;
            case 8:
            case 9:
                break;
        }
        if (sparkLevel >= 0) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[4], sparkLevel, 0);
        }
        if ((reaction == ACTOR_01100_REACTION_TWITCH) && (work->mode == ACTOR_01100_MODE_UNAWARE)) {
            reaction = ACTOR_01100_REACTION_FLINCH;
        }
        flags = enemy->reactionFlags;
        if (flags & ENEMY_REACTION_STAGGER) {
            reaction             = ACTOR_01100_REACTION_FALL;
            enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if ((enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) && (Gp_ObjFlag4Expired(enemy) != 0)) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
        func_800E2C78(enemy, (s32)hitKey, (s32)damage, 0);
        func_800DA6E8(&enemy->node, (s32)damage, 0);
        if (work->hp > 0) {
            hp        = (u16)work->hp - damage;
            work->hp  = hp;
            enemy->hp = hp;
            if (work->hp <= 0) {
                if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 24, 0, 0)) {
                    work->roomNotified = 0;
                } else {
                    Gp_ReleaseStateF0Add(task, work->entryId);
                }
                died   = 1;
                sndId  = (work->waterRoom << 0x16) | 0x400B0006;
                sndId |= (u8)work->placeIndex << 8;
                sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                reaction = ACTOR_01100_REACTION_FALL;
                if (work->downState == ACTOR_01100_DOWN_RISING) {
                    reaction = ACTOR_01100_REACTION_FALL_AGAIN;
                }
                if (kind4or6 != 0) {
                    enemy->spawnState = 3;
                } else if (kind7 != 0) {
                    work->hitEffectFrames = 0x5A;
                    enemy->spawnState     = 0x10;
                }
            }
        } else {
            reaction = ACTOR_01100_REACTION_NONE;
        }
        if (died == 0) {
            downState = work->downState;
            if (downState == ACTOR_01100_DOWN_FALLING) {
                reaction = ACTOR_01100_REACTION_NONE;
            } else if (downState == ACTOR_01100_DOWN_RISING) {
                if (reaction >= ACTOR_01100_REACTION_FLINCH) {
                    reaction = ACTOR_01100_REACTION_FALL_AGAIN;
                } else {
                    reaction = ACTOR_01100_REACTION_RISING_LIGHT;
                }
            }
        }
        if ((reaction == ACTOR_01100_REACTION_TWITCH) && (dotDamage != 0)) {
            reaction = ACTOR_01100_REACTION_FLINCH;
        }
        if (work->reaction == ACTOR_01100_REACTION_STUNNED) {
            reaction = ACTOR_01100_REACTION_STUNNED;
        }
        work->reaction = reaction;
        if (reaction != ACTOR_01100_REACTION_NONE) {
            rate = 0x10;
            for (n = 1; n < ARRAY_SIZE(work->rig.slots); n++) {
                slot       = &work->rig.slots[n];
                slot->rate = rate;
                slot       = &work->flinchRig.slots[n];
                slot->rate = rate;
            }
        }
        staged = work->reaction;
        switch (staged) {
            case ACTOR_01100_REACTION_TWITCH:
                work->flinchPhase = 1;
                if (work->hp > 0) {
                    sndId  = (work->waterRoom << 0x16) | 0x400B0007;
                    sndId |= (u8)work->placeIndex << 8;
                    sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                }
                work->hitFromBehind = fromBehind;
                func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_FLINCH:
                if (work->hp > 0) {
                    sndId  = (work->waterRoom << 0x16) | 0x400B0007;
                    sndId |= (u8)work->placeIndex << 8;
                    sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                }
                work->state = ACTOR_01100_STATE_FLINCH;
                _actor01100ClearObjPair(work);
                work->mode          = ACTOR_01100_MODE_REACTING;
                work->stateStep     = 0;
                work->hitFromBehind = fromBehind;
                func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_FALL:
                if (work->hp > 0) {
                    sndId  = (work->waterRoom << 0x16) | 0x400B0007;
                    sndId |= (u8)work->placeIndex << 8;
                    sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                }
                work->flinchPhase = 0;
                work->state       = ACTOR_01100_STATE_FALL;
                if (enemy->spawnState == 3) {
                    work->state = ACTOR_01100_STATE_DEATH;
                }
                _actor01100ClearObjPair(work);
                work->mode          = ACTOR_01100_MODE_REACTING;
                work->stateStep     = 0;
                work->hitFromBehind = fromBehind;
                func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_FALL_AGAIN:
                if (work->hp > 0) {
                    sndId  = (work->waterRoom << 0x16) | 0x400B0007;
                    sndId |= (u8)work->placeIndex << 8;
                    sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                }
                work->flinchPhase = 0;
                work->state       = ACTOR_01100_STATE_FALL_AGAIN;
                if (enemy->spawnState == 3) {
                    if (work->hitFromBehind != 0) {
                        work->motion = 0x14;
                    } else {
                        work->motion = 0x13;
                    }
                    work->state = ACTOR_01100_STATE_DEATH;
                }
                _actor01100ClearObjPair(work);
                work->mode      = ACTOR_01100_MODE_REACTING;
                work->stateStep = 0;
                func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_STUNNED:
                if (work->hp > 0) {
                    sndId  = (work->waterRoom << 0x16) | 0x400B0007;
                    sndId |= (u8)work->placeIndex << 8;
                    sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                }
                func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
                break;
            case ACTOR_01100_REACTION_RISING_LIGHT:
                if (work->hp > 0) {
                    sndId  = (work->waterRoom << 0x16) | 0x400B0007;
                    sndId |= (u8)work->placeIndex << 8;
                    sndEvtRequestScriptStart(sndId, (s8)scratch->pan, (s8)scratch->depth);
                }
                func_800FDB18((u16)work->hitEffectKind, &task->extra.tmd->coords[4], NULL, &work->hitEffectArg);
                break;
        }
    }
    world  = work->contacts[ACTOR_01100_BODY_ROOT];
    coord  = task->extra.tmd->coords;
    savedY = coord->coord.t[1];
    moved  = _actor01100PushOut(coord, world);
    if (moved != 0) {
        coord->composeStamp  = GRAPHICS_COORD_DIRTY;
        work->blockedFrames += 1;
    } else {
        work->blockedFrames = 0;
    }
    coord->coord.t[1] = savedY;
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_ROOT]);
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_LEFT_HAND]);
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_RIGHT_HAND]);
    worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_CHEST]);
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (Gp_TickObjFlag2(enemy) != 0)) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
    }
    return damaged;
}

/// First of the 0xA pair the dispatcher `Actor01100_Fn02960` runs while `mode`
/// is still clear: it re-arms the link transform and decides from the squared
/// distance `Actor01100_Fn06AC8` measures to the model's part-3 coordinate
/// whether the actor closes in this frame.
///
/// `lookYaw` steps back toward zero - 0x10 off either end of the +-0x10 band,
/// or straight to zero inside it - and `field_BAA` is cleared.
/// `Task::spawnArg1` then picks the threshold: 0 takes 0x5F5E0F outright,
/// 0x20000 takes 0x3D08FF, and anything else 0xF423FF while the player's
/// `GameActor::movementMode` reads 3 and 0xF423F otherwise; the 0x20000 case
/// also closes in whenever the player flag at
/// `gSceneCombatState.signals.bytes.actionFlags` reads 1 without measuring at
/// all. Either way the link transform is re-armed exactly as its siblings arm
/// it - model part 3 through `TmdObject::coords[3]`, the 0xC8-box local offset
/// through `src` - and `Actor01100_Fn00F58` runs last; its nonzero answer also
/// closes the actor in.
///
/// Closing in while `hp` still counts masks the 0xC000 pair back out of the two
/// middle collision bodies and, the first time only, stages the 0xA state
/// through `mode`: that is what hands the next frame to `Actor01100_Fn01D98`.
///
/// Each arm declares its own player and actor locals: the two arms must reach
/// the compiler as distinct quantities, since one of them is live across the
/// flag byte's address and cannot share the call's result register.
static void Actor01100_Fn01B90(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    WorldTargetNode* lockNode;
    s32              flag;
    u32              dist;
    s16              walk;

    flag = 0;
    dist = Actor01100_Fn06AC8(task->extra.tmd->coords);
    walk = work->lookYaw;
    if (walk >= 0x11) {
        work->lookYaw = (s16)((u16)work->lookYaw - 0x10);
    } else if (walk < -0x10) {
        work->lookYaw = (s16)((u16)work->lookYaw + 0x10);
    } else {
        work->lookYaw = 0;
    }
    work->field_BAA = 0;

    if (task->spawnArg1.value == 0) {
        if (dist <= 0x5F5E0F) {
            flag = 1;
        }
    } else if (task->spawnArg1.value == 0x20000) {
        Task*      player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        GameActor* actor;

        if (player != NULL) {
            actor = (GameActor*)player->work;
            if (((gSceneCombatState.signals.bytes.actionFlags ^ 1) == 0) || (((u16)actor->movementMode == 3) && dist <= 0x3D08FF)) {
                flag = 1;
            }
        }
    } else {
        Task*      player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        GameActor* actor;

        if (player != NULL) {
            actor = (GameActor*)player->work;
            if ((((u16)actor->movementMode == 3) && dist <= 0xF423FF) || dist <= 0xF423F) {
                flag = 1;
            }
        }
    }

    lockNode                            = &enemy->node;
    enemy->node.state.parts.flags       = 0;
    GP_NODE_ENEMY(lockNode)->coord      = &task->extra.tmd->coords[3];
    GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
    GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
    GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
    if (Actor01100_Fn00F58(enemy, task, work, scratch) != 0) {
        flag = 1;
    }
    if (flag && (work->hp > 0)) {
        work->field_BAA = 0;
        _actor01100ClearObjPair(work);
        if (work->mode == ACTOR_01100_MODE_UNAWARE) {
            work->mode      = ACTOR_01100_MODE_ENGAGED;
            work->state     = ACTOR_01100_STATE_NOTICE;
            work->stateStep = 0;
        }
    }
}

/// State-0xA pose: clamps `lookYaw`, splits it as Y rotations across model
/// parts 4 and its two `parent` nodes, then GPF-scales the arm chains at parts
/// 6 and 10. `rightShoulderSwell` / `leftShoulderSwell` scale the child then
/// the parent by the reciprocal; `rightArmStretch` / `leftArmStretch` scale the
/// parent in place (column 0 at 1+delta, columns 1-2 at 1+delta/4).
static void Actor01100_Fn01D98(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    GfxCoord* part;
    GfxCoord* node;
    s32       walk;
    s32       rest;
    s32       scale;

    walk = work->lookYaw;
    part = task->extra.tmd->coords + 4;
    if (walk < -0x600) {
        walk = -0x600;
    } else if (walk >= 0x601) {
        walk = 0x600;
    }
    if ((u32)(walk + 0x2FF) < 0x5FFU) {
        rest = walk / 3;
        gfxRotMatrixY(&part->coord, walk - rest, 0);
        part->composeStamp = GRAPHICS_COORD_DIRTY;
    } else if (walk > 0) {
        gfxRotMatrixY(&part->coord, 0x200, 0);
        part->composeStamp = GRAPHICS_COORD_DIRTY;
        rest               = walk - 0x200;
    } else {
        gfxRotMatrixY(&part->coord, -0x200, 0);
        part->composeStamp = GRAPHICS_COORD_DIRTY;
        rest               = walk + 0x200;
    }
    rest >>= 1;
    gfxRotMatrixY(&part->parent->coord, rest, 0);
    part->parent->composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixY(&part->parent->parent->coord, rest, 0);
    part->parent->parent->composeStamp = GRAPHICS_COORD_DIRTY;

    if (work->rightShoulderSwell != 0) {
        GfxCoord* coords;
        s32       inv;

        scale              = work->rightShoulderSwell + 0x1000;
        coords             = task->extra.tmd->coords;
        scratch->vector.vz = scale;
        scratch->vector.vy = scale;
        scratch->vector.vx = scale;
        node               = &coords[6];
        gfxScaleMatrixColumns(&node->parent->coord, &scratch->vector);
        node->parent->composeStamp = GRAPHICS_COORD_DIRTY;
        inv                        = 0x01000000 / scale;
        scratch->vector.vz         = inv;
        scratch->vector.vy         = inv;
        scratch->vector.vx         = inv;
        gfxScaleMatrixColumns(&node->coord, &scratch->vector);
    }

    if (work->leftShoulderSwell != 0) {
        GfxCoord* coords;
        s32       inv;

        scale              = work->leftShoulderSwell + 0x1000;
        coords             = task->extra.tmd->coords;
        scratch->vector.vz = scale;
        scratch->vector.vy = scale;
        scratch->vector.vx = scale;
        node               = &coords[10];
        gfxScaleMatrixColumns(&node->parent->coord, &scratch->vector);
        node->parent->composeStamp = GRAPHICS_COORD_DIRTY;
        inv                        = 0x01000000 / scale;
        scratch->vector.vz         = inv;
        scratch->vector.vy         = inv;
        scratch->vector.vx         = inv;
        gfxScaleMatrixColumns(&node->coord, &scratch->vector);
    }

    if (work->rightArmStretch != 0) {
        MATRIX*   m;
        SVECTOR*  sv;
        GfxCoord* c;

        sv = &scratch->shortVector;
        c  = task->extra.tmd->coords;
        m  = &c[6].coord;
        SCALE_COL(m, sv, 0, work->rightArmStretch + 0x1000);
        SCALE_COL(m, sv, 1, (work->rightArmStretch >> 2) + 0x1000);
        SCALE_COL(m, sv, 2, (work->rightArmStretch >> 2) + 0x1000);
        c[6].composeStamp = GRAPHICS_COORD_DIRTY;
    }

    if (work->leftArmStretch != 0) {
        MATRIX*   m;
        SVECTOR*  sv;
        GfxCoord* c;

        sv = &scratch->shortVector;
        c  = task->extra.tmd->coords;
        m  = &c[10].coord;
        SCALE_COL(m, sv, 0, work->leftArmStretch + 0x1000);
        SCALE_COL(m, sv, 1, (work->leftArmStretch >> 2) + 0x1000);
        SCALE_COL(m, sv, 2, (work->leftArmStretch >> 2) + 0x1000);
        c[10].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Per-frame state handlers `Actor01100_Fn02960` copies to its stack and
/// indexes by `_Actor01100Work::state`.
static const _Actor01100StateTable Actor01100_D00064 = { {
    Actor01100_Fn06E4C,
    Actor01100_Fn035E4,
    Actor01100_Fn03740,
    Actor01100_Fn0389C,
    Actor01100_Fn06F38,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn03BAC,
    Actor01100_Fn04DB4,
    Actor01100_Fn04410,
    Actor01100_Fn048C8,
    Actor01100_Fn0516C,
    Actor01100_Fn041BC,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn06E4C,
    Actor01100_Fn07014,
    Actor01100_Fn070DC,
    Actor01100_Fn07148,
    Actor01100_Fn072B8,
    Actor01100_Fn05678,
    Actor01100_Fn05CFC,
} };

/// Per-frame update. Does nothing while `hidden` is set. Otherwise it refreshes
/// model part 3 and, while `gSceneCombatState.actorControl` is 0, steps both
/// animation contexts over parts 1-20 (restarting the motion in `motion` when
/// it changed, and blending the second context in by `flinchWeight`), fills the
/// scratch block's `pan` and `depth` from part 1, runs the handler for `state`,
/// then the arm `mode` selects, and while `waterRoom` is 1 spawns the splash
/// effects and sound. While `gSceneCombatState.actorControl` is 1 it only
/// pushes the root out of its world contacts. Whatever the mode, it then
/// refreshes the root, updates the actor colour from the root position, draws
/// the floor quad unless bit 1 of the model's flags is set, and exits the task
/// once `mode` reaches 0x10.
static void Actor01100_Fn02960(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    _Actor01100StateTable table;
    GfxCoord*             part;
    GfxCoord*             root;
    s32                   restart;
    s32                   randBit;
    s32                   slot;
    AnimationPose*        pose;
    s32                   blend;
    s32                   animId;
    s32                   savedY;
    s32                   eff;
    s16                   dy;
    s16                   walk;

    table   = Actor01100_D00064;
    restart = 0;
    if (work->hidden != 0) {
        return;
    }
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        randBit           = rand() & 1;
        work->motionEnded = 0;
        work->prevMode    = work->mode;
        if (work->motion != work->startedMotion) {
            restart             = 1;
            work->startedMotion = work->motion;
        }
        switch (work->flinchPhase) {
            case 0:
                work->flinchWeight = 0;
                break;
            case 1:
                animId = 0xE;
                if (work->hitFromBehind == 0) {
                    animId = 0xB;
                }
                slot = 1;
                do {
                    animationCaptureSlotWithBlend(&work->flinchRig.anim, slot, &scratch->poses[1], animId, 0, 0, 0);
                    slot++;
                } while (slot < ARRAY_SIZE(work->flinchRig.slots));
                work->flinchPhase++;
                break;
            case 2:
                if (work->flinchWeight < 0x40) {
                    work->flinchWeight += 4;
                }
                if (work->flinchRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                    work->flinchPhase++;
                }
                break;
            case 3:
                if (work->flinchWeight > 0) {
                    work->flinchWeight -= 4;
                    if (work->flinchWeight > 0) {
                        break;
                    }
                }
                work->flinchPhase++;
                break;
            default:
                work->flinchWeight = 0;
                work->flinchPhase  = 0;
                break;
        }
        slot = 1;
        do {
            if ((slot == 6) || (slot == 0xA)) {
                pose = 0;
            } else {
                pose = 0;
                if (work->flinchWeight != 0) {
                    pose = &scratch->poses[0];
                }
            }
            if (restart != 0) {
                if ((u32)(work->reaction - 2) >= 0xE) {
                    animationCaptureSlotWithBlend(&work->rig.anim, slot, pose, work->motion, 0, 0, randBit + 8);
                } else if ((u32)((u8)work->motion - 0x15) < 2) {
                    pose = 0;
                    animationResetSlot(&work->rig.anim, slot, work->motion);
                } else if ((slot != 6) && (slot != 0xA)) {
                    animationCaptureSlotWithBlend(&work->rig.anim, slot, pose, work->motion, 0, 0, 1);
                } else {
                    animationCaptureSlotWithBlend(&work->rig.anim, slot, pose, work->motion, 3, 0, 0x1E);
                }
            } else {
                animationTickSlotPose(&work->rig.anim, slot, pose, 0);
            }
            if (pose != 0) {
                blend = work->flinchWeight << 5;
                animationTickSlotPose(&work->flinchRig.anim, slot, &scratch->poses[1], 0);
                animationApplyBlendedPose(&work->rig.anim, slot, &scratch->poses[0],
                                          &scratch->poses[1], 0x1000 - blend, blend);
            }
            slot++;
        } while (slot < ARRAY_SIZE(work->rig.slots));
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->motionEnded = 1;
        }
        part           = task->extra.tmd->coords;
        part          += 1;
        scratch->pan   = worldCoordGetOriginAudioPan(part);
        scratch->depth = worldCoordGetOriginAudioDepth(part);
        table.funcs[work->state](enemy, task, work, scratch);
        Actor01100_Fn01D98(enemy, task, work, scratch);
        switch (work->mode) {
            case ACTOR_01100_MODE_UNAWARE:
                Actor01100_Fn01B90(enemy, task, work, scratch);
                break;
            case ACTOR_01100_MODE_ENGAGED: {
                GfxCoord*        c;
                WorldTargetNode* lockNode;

                enemy->node.state.parts.flags       = 0;
                c                                   = task->extra.tmd->coords;
                lockNode                            = &enemy->node;
                GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
                GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
                GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
                GP_NODE_ENEMY(lockNode)->coord      = c + 3;
                if (Actor01100_Fn00F58(enemy, task, work, scratch) == 0 && task->spawnArg1.value == 0 && work->prevMode == ACTOR_01100_MODE_ENGAGED && work->motionEnded == 1 && Actor01100_Fn06AC8(task->extra.tmd->coords) > 0xA62B10) {
                    _actor01100ClearObjPair(work);
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
                GfxCoord*        c;
                WorldTargetNode* lockNode;

                if (work->reaction != ACTOR_01100_REACTION_TWITCH) {
                    if (work->rightArmStretch >= 0x400) {
                        work->rightArmStretch -= 0x400;
                    } else {
                        work->rightArmStretch = 0;
                    }
                    if (work->leftArmStretch >= 0x400) {
                        work->leftArmStretch -= 0x400;
                    } else {
                        work->leftArmStretch = 0;
                    }
                    if (work->leftShoulderSwell >= 0x100) {
                        work->leftShoulderSwell -= 0x100;
                    } else {
                        work->leftShoulderSwell = 0;
                    }
                    if (work->rightShoulderSwell >= 0x100) {
                        work->rightShoulderSwell -= 0x100;
                    } else {
                        work->rightShoulderSwell = 0;
                    }
                    if (work->downState != ACTOR_01100_DOWN_STANDING) {
                        walk = work->lookYaw;
                        if (walk > 0x30) {
                            work->lookYaw -= 0x30;
                        } else if (walk < -0x30) {
                            work->lookYaw += 0x30;
                        }
                    }
                }
                c                                   = task->extra.tmd->coords;
                lockNode                            = &enemy->node;
                GP_NODE_ENEMY(lockNode)->bodyPos.vy = -0xC8;
                GP_NODE_ENEMY(lockNode)->bodyPos.vx = 0;
                GP_NODE_ENEMY(lockNode)->bodyPos.vz = 0xC8;
                GP_NODE_ENEMY(lockNode)->coord      = c + 3;
                Actor01100_Fn00F58(enemy, task, work, scratch);
                break;
            }
        }
        if (work->waterRoom == 1) {
            TransposeMatrix(&gGfxViewCoord.workm, &scratch->viewInverse);
            if (!(gDisplayState.animFrame & 0xF)) {
                GfxCoord* c;

                c                       = task->extra.tmd->coords;
                scratch->shortVector.vx = 0;
                scratch->shortVector.vy = -0x1E0;
                scratch->shortVector.vz = 0;
                Gp_SpawnEff(gRoomEffectWaterRippleId, c, 0xC0, &scratch->shortVector);
            }
            if (scratch->splashPart != 0) {
                if (work->splashPart == 0 || (scratch->splashPart == ACTOR_01100_PART_CHEST && work->splashPart != scratch->splashPart)) {
                    work->splashHeight = 0;
                }
                work->splashPart = scratch->splashPart;
            }
            if (work->sprayFrames != 0 || scratch->splashPart != 0) {
                eff  = 0x11402300;
                part = &task->extra.tmd->coords[work->splashPart];
                actorRenderComposeCoord(part);
                scratch->shortVector.vx = 0;
                scratch->shortVector.vy = 0;
                scratch->shortVector.vz = 0;
                if (work->splashPart == ACTOR_01100_PART_RIGHT_HAND) {
                    scratch->shortVector.vx = 0x190;
                } else if (work->splashPart == ACTOR_01100_PART_LEFT_HAND) {
                    scratch->shortVector.vx = -0x190;
                }
                _gfxRotateSv(&part->workm, &scratch->shortVector);
                scratch->shortVector.vx += part->workm.t[0];
                scratch->shortVector.vy += part->workm.t[1];
                scratch->shortVector.vz += part->workm.t[2];
                scratch->shortVector.vx -= gGfxViewCoord.workm.t[0];
                scratch->shortVector.vy -= gGfxViewCoord.workm.t[1];
                scratch->shortVector.vz -= gGfxViewCoord.workm.t[2];
                _gfxRotateSv(&scratch->viewInverse, &scratch->shortVector);
                dy = gGameSession->waterY - scratch->shortVector.vy;
                if (work->splashHeight * dy < 0) {
                    work->sprayFrames = 5;
                }
                work->splashHeight = dy;
                if (work->splashPart == ACTOR_01100_PART_CHEST) {
                    scratch->shortVector.vx += rand() % 1200 - 0x258;
                    eff                      = 0x11602480;
                    scratch->shortVector.vz += rand() % 1200 - 0x258;
                }
                if (work->sprayFrames != 0) {
                    work->sprayFrames--;
                    if (work->sprayFrames == 0) {
                        work->splashPart = 0;
                    }
                    Gp_SpawnEff(gRoomEffectWaterSprayId, &gGfxViewCoord, eff, &scratch->shortVector);
                    sndEvtRequestScriptStart(((u8)work->placeIndex << 8) | 0x404B000D, (s8)scratch->pan, (s8)scratch->depth);
                }
            }
        }
    } else if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_PAUSED) {
        root   = task->extra.tmd->coords;
        savedY = root->coord.t[1];
        actorRenderComposeCoord(root);
        if (_actor01100PushOut(root, work->contacts[ACTOR_01100_BODY_ROOT])) {
            root->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        root->coord.t[1] = savedY;
        worldCollisionClearContacts(work->contacts[ACTOR_01100_BODY_ROOT]);
    }
    root = task->extra.tmd->coords;
    actorRenderComposeCoord(root);
    scratch->vector.vx = root->workm.t[0];
    scratch->vector.vy = root->workm.t[1] - 0x320;
    scratch->vector.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &scratch->vector, 0, 0);
    if (!(task->extra.tmd->flags & TMD_OBJECT_SEMI_TRANS)) {
        actorRenderDrawGroundShadow(task->extra.tmd->coords, 0x600, NULL);
    }
    if (work->mode >= ACTOR_01100_MODE_FINISHED) {
        taskCallExit(task);
    }
}

/// Collision-arm handler: the first frame `stateStep` is still clear it sets
/// motion 2, zeroes `stateCounter` and steps the latch. Every later frame
/// increments that countdown. On frame 0x1A it writes a `Gp_PackObjPair`
/// payload into collision body 1's `key` and ORs the grid and pair test enables
/// into its `flags`. While the countdown sits in `[0x1B, 0x36]` and the latch
/// is still 1, a hit on the left hand's contact table masks those bits back out
/// of both middle collision bodies and steps the latch; frame 0x37 does the
/// same mask unconditionally. The scratch block's `splashPart` takes 0xC
/// either way, and `motionEnded` ends the sub-state by clearing `state` and the
/// latch.
static void Actor01100_Fn035E4(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    WorldCollisionBody* obj;
    u16                 time;

    if (work->stateStep == 0) {
        work->motion       = 2;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
    }
    time               = (u16)work->stateCounter + 1;
    work->stateCounter = time;
    if ((s16)time == 0x1A) {
        obj         = &work->bodies[ACTOR_01100_BODY_LEFT_HAND];
        obj->key    = Gp_PackObjPair(enemy, 1);
        obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((u32)((u16)work->stateCounter - 0x1B) < 0x1C) {
        if ((work->stateStep == 1) && (worldCollisionFindContactIndex(work->contacts[ACTOR_01100_BODY_LEFT_HAND], WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
            _actor01100ClearObjPair(work);
            work->stateStep = (u8)work->stateStep + 1;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_LEFT_HAND;
    if (work->stateCounter == 0x37) {
        _actor01100ClearObjPair(work);
    }
    if (work->motionEnded == 1) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Collision-arm handler for collision body 2: the first frame `stateStep` is
/// still clear it sets motion 3, zeroes `stateCounter` and steps the latch.
/// Every later frame increments that countdown. On frame 0x1A it calls
/// `Gp_PackObjPair` with pair 2 and ORs the grid and pair test enables into
/// collision body 2's `flags`. While the countdown sits in `[0x1B, 0x36]` and
/// the latch is still 1, a hit on the right hand's contact table masks those
/// bits back out of both middle collision bodies and steps the latch; frame
/// 0x37 does the same mask unconditionally. The scratch block's `splashPart`
/// takes 8 either way, and `motionEnded` ends the sub-state by clearing `state`
/// and the latch.
static void Actor01100_Fn03740(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    WorldCollisionBody* obj;
    u16                 time;

    if (work->stateStep == 0) {
        work->motion       = 3;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
    }
    time               = (u16)work->stateCounter + 1;
    work->stateCounter = time;
    if ((s16)time == 0x1A) {
        obj = &work->bodies[ACTOR_01100_BODY_RIGHT_HAND];
        Gp_PackObjPair(enemy, 2);
        obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if ((u32)((u16)work->stateCounter - 0x1B) < 0x1C) {
        if ((work->stateStep == 1) && (worldCollisionFindContactIndex(work->contacts[ACTOR_01100_BODY_RIGHT_HAND], WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
            _actor01100ClearObjPair(work);
            work->stateStep = (u8)work->stateStep + 1;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    if (work->stateCounter == 0x37) {
        _actor01100ClearObjPair(work);
    }
    if (work->motionEnded == 1) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Spin-about handler: on the frame `stateStep` is still clear it draws a
/// nibble from `gRandomLcgState` and arms one of the six spin rates - the 0x200
/// / 0x400 / 0x600 triple, negative on odd draws - into `stateCounter`, then
/// acts its motion 4. Every later frame turns the model's yaw at 0x46 by 0x10
/// towards that countdown, rebuilds the Y rotation over the pose and clears
/// `composeStamp`, and when the countdown reaches zero it drops `state` and the
/// latch, ending the spin about.
static void Actor01100_Fn0389C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    GfxCoord* pose;
    s32       idx;
    u32       rng;
    u16       angle;

    pose = task->extra.tmd->coords;
    if (work->stateStep == 0) {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        idx             = (rng >> 0x10) & 0xF;
        if (idx < 3) {
            work->stateCounter = 0x200;
        } else if (idx < 6) {
            work->stateCounter = -0x200;
        } else if (idx < 9) {
            work->stateCounter = 0x400;
        } else if (idx < 0xC) {
            work->stateCounter = -0x400;
        } else if (idx < 0xE) {
            work->stateCounter = 0x600;
        } else {
            work->stateCounter = -0x600;
        }
        work->motion    = 4;
        work->stateStep = (u8)work->stateStep + 1;
    }
    if (work->stateCounter > 0) {
        angle              = ((u16)pose->param.rot.vy - 0x10) & 0xFFF;
        pose->param.rot.vy = angle;
        gfxRotMatrixY(&pose->coord, angle, 1);
        pose->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateCounter = (u16)work->stateCounter - 0x10;
    } else {
        angle              = ((u16)pose->param.rot.vy + 0x10) & 0xFFF;
        pose->param.rot.vy = angle;
        gfxRotMatrixY(&pose->coord, angle, 1);
        pose->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateCounter = (u16)work->stateCounter + 0x10;
    }
    if (work->stateCounter == 0) {
        work->state     = ACTOR_01100_STATE_IDLE;
        work->stateStep = 0;
    }
}

/// Bearing of the player (actor slot 0) from `self`, measured in `self`'s own
/// frame and folded into -0x800..0x800; 0 when there is no player.
static __inline__ s32 _actor01100BearingToPlayer(GfxCoord* self)
{
    GfxCoord*            other;
    ActorBearingScratch* blk;
    s32                  angle;

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        angle = 0;
    } else {
        other = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        blk   = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
        angle = actorBearingInFrame(blk, self, other);
        SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    }
    return angle;
}

static void Actor01100_Fn039D0(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    s16 bearing;
    s32 angle;
    s16 cur;

    bearing             = _actor01100BearingToPlayer(task->extra.tmd->coords);
    work->playerBearing = bearing;
    angle               = bearing;
    if (angle < -0x300) {
        angle = -0x300;
    } else if (angle >= 0x301) {
        angle = 0x300;
    }

    cur = work->lookYaw;
    if (cur < angle - 0xC0) {
        work->lookYaw += 0xC0;
    } else if (angle + 0xC0 < cur) {
        work->lookYaw -= 0xC0;
    } else {
        work->lookYaw = angle;
    }
}

/// Squared distance from `self` to the player (pointer slot 3), or
/// 0x7FFFFFFF when there is none.
static __inline__ s32 _actor01100DistSqToPlayer(GfxCoord* self)
{
    Task*     player;
    GfxCoord* other;
    SVECTOR*  vec;
    s32       dist;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (player == NULL) {
        return 0x7FFFFFFF;
    }
    other   = player->extra.tmd->coords;
    vec     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    vec->vx = other->workm.t[0] - self->workm.t[0];
    vec->vy = other->workm.t[1] - self->workm.t[1];
    vec->vz = other->workm.t[2] - self->workm.t[2];
    dist    = gfxDotProduct(vec, vec);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return dist;
}

static void Actor01100_Fn03BAC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    GfxCoord* self;
    s16       angle;
    s32       goal;
    s32       yaw;
    s16       cur;
    s16       next;
    s8        latch;
    s32       t;

    if (work->stateStep == 0) {
        work->motion        = 1;
        work->playerBearing = _actor01100BearingToPlayer(task->extra.tmd->coords);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateCounter  = ((gRandomLcgState >> 0x10) & 0x1F) + 2;
        work->stateStep++;
    }

    latch = work->stateStep;
    if (latch == 1) {
        goal = work->playerBearing;
        if (goal < -0x600) {
            goal = -0x600;
        } else if (goal >= 0x601) {
            goal = 0x600;
        }
        cur = work->lookYaw;
        if (cur < goal) {
            next          = work->lookYaw + 0xC0;
            work->lookYaw = next;
            if (goal < next) {
                work->lookYaw = goal;
            }
        } else if (goal < cur) {
            next          = work->lookYaw - 0xC0;
            work->lookYaw = next;
            if (next < goal) {
                work->lookYaw = goal;
            }
        } else {
            Gp_ArmStateF0(1);
            work->stateStep++;
        }
    } else if (latch == 2) {
        if (--work->stateCounter < 0) {
            work->motion = 4;
            work->stateStep++;
        }
    }

    if (work->stateStep == 3) {
        self                = task->extra.tmd->coords;
        angle               = _actor01100BearingToPlayer(self);
        yaw                 = angle;
        work->playerBearing = angle;
        if (yaw < -0x600) {
            yaw = -0x600;
        } else if (yaw >= 0x601) {
            yaw = 0x600;
        }
        cur = work->lookYaw;
        if (cur < yaw - 0xC0) {
            work->lookYaw += 0xC0;
        } else if (yaw + 0xC0 < cur) {
            work->lookYaw -= 0xC0;
        } else {
            work->lookYaw = yaw;
        }

        yaw = work->playerBearing;
        if (yaw >= 0x11) {
            self->param.rot.vy += 0x10;
        } else if (yaw < -0x10) {
            self->param.rot.vy -= 0x10;
        } else {
            self->param.rot.vy += yaw;
        }
        self->param.rot.vy &= 0xFFF;
        gfxRotMatrixY(&self->coord, self->param.rot.vy, 1);
        self->composeStamp = GRAPHICS_COORD_DIRTY;

        if ((u16)(work->playerBearing + 0x7F) < 0xFF) {
            if (task->spawnArg1.value != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                t               = (u16)((gRandomLcgState >> 0x10) % 3);
                if (t <= 0) {
                    work->state = ACTOR_01100_STATE_PUNCH_LEFT;
                } else if (t < 2) {
                    work->state = ACTOR_01100_STATE_PUNCH_RIGHT;
                } else {
                    work->state = ACTOR_01100_STATE_SPIT;
                }
            } else if (_actor01100DistSqToPlayer(task->extra.tmd->coords) <= 0xA62B0F) {
                if (yaw >= 0) {
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

static void Actor01100_Fn041BC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    GfxCoord* yaw;
    s32       delta;
    u16       angle;
    s32       dist;
    s8        kind;
    u16       wait;
    u32       rng;

    wait = 0x3C;
    if (work->entryId == 0x31) {
        wait = 0xA;
    }
    if (work->stateStep == 0) {
        work->motion = 4;
        work->stateStep++;
        task->killCountdown = wait;
    }
    Actor01100_Fn039D0(enemy, task, work, scratch);

    delta = work->playerBearing;
    yaw   = task->extra.tmd->coords;
    if (delta > 0x10) {
        yaw->param.rot.vy += 0x10;
    } else if (delta < -0x10) {
        yaw->param.rot.vy -= 0x10;
    } else {
        yaw->param.rot.vy += delta;
    }
    angle             = yaw->param.rot.vy & 0xFFF;
    yaw->param.rot.vy = angle;
    gfxRotMatrixY(&yaw->coord, angle, 1);
    yaw->composeStamp = GRAPHICS_COORD_DIRTY;

    if (work->playerBearing > -0x80 && work->playerBearing < 0x80) {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        work->state     = ((rng >> 0x10) & 4) ? ACTOR_01100_STATE_PUNCH_LEFT : ACTOR_01100_STATE_PUNCH_RIGHT;
        Gp_ArmStateF0(1);
        work->stateStep = 0;
        return;
    }

    task->killCountdown--;
    if (task->killCountdown > 0) {
        return;
    }

    dist = _actor01100DistSqToPlayer(task->extra.tmd->coords);
    kind = work->entryId;
    if (((kind == 0xB) && (dist <= 0x89543F)) || ((kind == 0x31) && (dist <= 0x22550F))) {
        Gp_ArmStateF0(1);
        work->state         = ACTOR_01100_STATE_SPIT;
        work->stateStep     = 0;
        task->killCountdown = 0;
        return;
    }
    task->killCountdown = wait;
}

/// Scale `Actor01100_Fn05678` applies to the model's matrix: 0x10 on each axis.
static const VECTOR Actor01100_D000CC = { 0x10, 0x10, 0x10, 0 };

/// First-frame distance handler: while `stateStep` is still clear it sets
/// motion 6, zeroes `stateCounter` and steps the latch. If actor slot 3 is live
/// it rotates `(0x12C, 0, 0)` through model part 10's `workm`, adds the
/// player-part-1 versus part-10 translation, and maps
/// `SquareRoot0(gfxDotProduct)` into `stretchGoal` — 0 inside 0x384,
/// 0x2000 past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
///
/// Every later frame increments the countdown, asks `Actor01100_Fn039D0` for
/// `playerBearing` and turns the model's `field_46` toward it by at most 0x10,
/// then rebuilds the Y rotation. Frame 0x16 packs pair 3 into collision body 1
/// and ORs the 0xC000 bits; frame 0x20 posts `0x400B0008`. While the countdown
/// sits in `[0x17, 0x2B]` and the latch is still 1, a high-bit hit on the left
/// hand's contact table steps the latch to 3. `leftShoulderSwell` /
/// `leftArmStretch` ramp with the countdown, the scratch block's `splashPart`
/// takes 0xC, and frame 0x2C masks those bits back out of both middle collision
/// bodies. `motionEnded` writes rate 0x10 onto slots `[1, 0x14]` of both
/// animation runs and then either stages state 0xE, or, while the latch is 3, a
/// 1-in-4 draw of that state versus restarting the motion through
/// `startedMotion`.
static void Actor01100_Fn04410(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    SVECTOR*            vec;
    GfxCoord*           actorCoords;
    GfxCoord*           playerCoords;
    GfxCoord*           actorPart;
    GfxCoord*           playerPart;
    GfxCoord*           pose;
    WorldCollisionBody* obj;
    Task*               player;
    s32                 dist;
    s32                 yaw;
    u16                 angle;
    u16                 time;
    u16                 reach;
    u32                 rng;

    if (work->stateStep == 0) {
        work->motion       = 6;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
        player             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (player == NULL) {
            work->stretchGoal = 0;
        } else {
            vec                     = &scratch->shortVector;
            actorCoords             = task->extra.tmd->coords;
            playerCoords            = player->extra.tmd->coords;
            scratch->shortVector.vx = 0x12C;
            scratch->shortVector.vy = 0;
            scratch->shortVector.vz = 0;
            actorPart               = &actorCoords[10];
            playerPart              = &playerCoords[1];
            _gfxLoadRotSv(&actorPart->workm, &scratch->shortVector);
            gte_rtv0();
            gte_stsv(vec);
            scratch->shortVector.vx += (u16)playerPart->workm.t[0] - (u16)actorPart->workm.t[0];
            scratch->shortVector.vy += (u16)playerPart->workm.t[1] - (u16)actorPart->workm.t[1];
            scratch->shortVector.vz += (u16)playerPart->workm.t[2] - (u16)actorPart->workm.t[2];
            dist                     = SquareRoot0(gfxDotProduct(vec, vec));
            if (dist < 0x384) {
                work->stretchGoal = 0;
            } else if (dist >= 0xA8D) {
                work->stretchGoal = 0x2000;
            } else {
                work->stretchGoal = ((dist - 0x384) << 9) / 100;
            }
        }
    }
    work->stateCounter = (u16)work->stateCounter + 1;
    Actor01100_Fn039D0(enemy, task, work, scratch);
    yaw  = work->playerBearing;
    pose = task->extra.tmd->coords;
    if (yaw >= 0x11) {
        pose->param.rot.vy = (u16)pose->param.rot.vy + 0x10;
    } else if (yaw < -0x10) {
        pose->param.rot.vy = (u16)pose->param.rot.vy - 0x10;
    } else {
        pose->param.rot.vy = (u16)pose->param.rot.vy + yaw;
    }
    angle              = (u16)pose->param.rot.vy & 0xFFF;
    pose->param.rot.vy = angle;
    gfxRotMatrixY(&pose->coord, angle, 1);
    pose->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->stateCounter == 0x16) {
        obj         = &work->bodies[ACTOR_01100_BODY_LEFT_HAND];
        obj->key    = Gp_PackObjPair(enemy, 3);
        obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else if (work->stateCounter == 0x20) {
        sndEvtRequestScriptStart((work->waterRoom << 22) | (((u8)work->placeIndex << 8) | 0x400B0008), (s8)scratch->pan, (s8)scratch->depth);
    }
    if (((u32)((u16)work->stateCounter - 0x17) < 0x15U) && (work->stateStep == 1) &&
        (worldCollisionCountContactsByKind(work->contacts[ACTOR_01100_BODY_LEFT_HAND], WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0)) {
        work->stateStep = 3;
    }
    time = work->stateCounter;
    if ((u32)(time - 1) < 0x1DU) {
        if (work->leftShoulderSwell < 0x800) {
            work->leftShoulderSwell = (s16)((u16)work->leftShoulderSwell + 0x40);
        }
    } else if ((s16)time >= 0x1E) {
        if (work->leftShoulderSwell >= 0x100) {
            work->leftShoulderSwell = (s16)((u16)work->leftShoulderSwell - 0x100);
        } else {
            work->leftShoulderSwell = 0;
        }
    }
    time = work->stateCounter;
    if ((u32)(time - 0x1E) < 0xEU) {
        reach = work->stretchGoal;
        if (work->leftArmStretch < ((s32)(reach << 0x10) >> 0x10)) {
            work->leftArmStretch = (s16)((u16)work->leftArmStretch + ((s32)(reach << 0x10) >> 0x13));
        }
    } else if ((s16)time >= 0x2C) {
        if (work->leftArmStretch >= 0x200) {
            work->leftArmStretch = (s16)((u16)work->leftArmStretch - 0x200);
        } else {
            work->leftArmStretch = 0;
        }
    }
    scratch->splashPart = ACTOR_01100_PART_LEFT_HAND;
    if (work->stateCounter == 0x2C) {
        _actor01100ClearObjPair(work);
    }
    if (work->motionEnded != 0) {
        _actor01100SetSlotRates(work, 0x10);
        if (work->stateStep != 3) {
            work->state     = ACTOR_01100_STATE_ADVANCE;
            work->stateStep = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        if (!((rng >> 0x10) & 3)) {
            work->state = ACTOR_01100_STATE_ADVANCE;
        } else {
            work->startedMotion = 1;
        }
        work->stateStep = 0;
    }
}

/// First-frame distance handler: while `stateStep` is still clear it sets
/// motion 7, zeroes `stateCounter` and steps the latch. If actor slot 3 is live
/// it rotates `(0x12C, 0, 0)` through model part 6's `workm`, adds the
/// player-part-1 versus part-6 translation, and maps
/// `SquareRoot0(gfxDotProduct)` into `stretchGoal` — 0 inside 0x384,
/// 0x2000 past 0xA8C, otherwise `((dist - 0x384) << 9) / 100`.
///
/// Every frame then asks `Actor01100_Fn039D0` for `playerBearing` and turns the
/// model's `field_46` toward it by at most 0x10, rebuilds the Y rotation,
/// increments the countdown and asks again. Frame 0x23 packs pair 4 into
/// collision body 2 and ORs the 0xC000 bits; frame 0x2D posts `0x400B0008`.
/// While the countdown sits in `[0x24, 0x3B]` and the latch is still 1, a
/// high-bit hit on the right hand's contact table steps the latch to 3.
/// `rightShoulderSwell` / `rightArmStretch` ramp with the countdown, the
/// scratch block's `splashPart` takes 8 (with a same-value write on frame
/// 0x2F), and frame 0x3C masks those bits back out of both middle collision
/// bodies. `motionEnded` writes rate 0x10 onto slots `[1, 0x14]` of both
/// animation runs, zeroes `rightArmStretch`, and then either stages state 0xE,
/// or, while the latch is 3, a 1-in-4 draw of that state versus restarting the
/// motion through `startedMotion`.
static void Actor01100_Fn048C8(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    SVECTOR*            vec;
    GfxCoord*           actorCoords;
    GfxCoord*           playerCoords;
    GfxCoord*           actorPart;
    GfxCoord*           playerPart;
    GfxCoord*           pose;
    WorldCollisionBody* obj;
    Task*               player;
    s32                 dist;
    s32                 yaw;
    u16                 angle;
    u16                 time;
    u16                 reach;
    u32                 rng;

    if (work->stateStep == 0) {
        work->motion       = 7;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
        player             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (player == NULL) {
            work->stretchGoal = 0;
        } else {
            vec                     = &scratch->shortVector;
            actorCoords             = task->extra.tmd->coords;
            playerCoords            = player->extra.tmd->coords;
            scratch->shortVector.vx = 0x12C;
            scratch->shortVector.vy = 0;
            scratch->shortVector.vz = 0;
            actorPart               = &actorCoords[6];
            playerPart              = &playerCoords[1];
            _gfxLoadRotSv(&actorPart->workm, &scratch->shortVector);
            gte_rtv0();
            gte_stsv(vec);
            scratch->shortVector.vx += (u16)playerPart->workm.t[0] - (u16)actorPart->workm.t[0];
            scratch->shortVector.vy += (u16)playerPart->workm.t[1] - (u16)actorPart->workm.t[1];
            scratch->shortVector.vz += (u16)playerPart->workm.t[2] - (u16)actorPart->workm.t[2];
            dist                     = SquareRoot0(gfxDotProduct(vec, vec));
            if (dist < 0x384) {
                work->stretchGoal = 0;
            } else if (dist >= 0xA8D) {
                work->stretchGoal = 0x2000;
            } else {
                work->stretchGoal = ((dist - 0x384) << 9) / 100;
            }
        }
    }
    Actor01100_Fn039D0(enemy, task, work, scratch);
    yaw  = work->playerBearing;
    pose = task->extra.tmd->coords;
    if (yaw >= 0x11) {
        pose->param.rot.vy = (u16)pose->param.rot.vy + 0x10;
    } else if (yaw < -0x10) {
        pose->param.rot.vy = (u16)pose->param.rot.vy - 0x10;
    } else {
        pose->param.rot.vy = (u16)pose->param.rot.vy + yaw;
    }
    angle              = (u16)pose->param.rot.vy & 0xFFF;
    pose->param.rot.vy = angle;
    gfxRotMatrixY(&pose->coord, angle, 1);
    pose->composeStamp = GRAPHICS_COORD_DIRTY;
    work->stateCounter = (u16)work->stateCounter + 1;
    Actor01100_Fn039D0(enemy, task, work, scratch);
    if (work->stateCounter == 0x23) {
        obj         = &work->bodies[ACTOR_01100_BODY_RIGHT_HAND];
        obj->key    = Gp_PackObjPair(enemy, 4);
        obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else if (work->stateCounter == 0x2D) {
        sndEvtRequestScriptStart((work->waterRoom << 22) | (((u8)work->placeIndex << 8) | 0x400B0008), (s8)scratch->pan, (s8)scratch->depth);
    }
    if (((u32)((u16)work->stateCounter - 0x24) < 0x18U) && (work->stateStep == 1) &&
        (worldCollisionCountContactsByKind(work->contacts[ACTOR_01100_BODY_RIGHT_HAND], WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0)) {
        work->stateStep = 3;
    }
    time = work->stateCounter;
    if ((u32)(time - 1) < 0x28U) {
        if (work->rightShoulderSwell < 0x800) {
            work->rightShoulderSwell = (s16)((u16)work->rightShoulderSwell + 0x40);
        }
    } else if ((s16)time >= 0x29) {
        if (work->rightShoulderSwell >= 0x100) {
            work->rightShoulderSwell = (s16)((u16)work->rightShoulderSwell - 0x100);
        } else {
            work->rightShoulderSwell = 0;
        }
    }
    time = work->stateCounter;
    if ((u32)(time - 0x29) < 0x13U) {
        reach = work->stretchGoal;
        if (work->rightArmStretch < ((s32)(reach << 0x10) >> 0x10)) {
            work->rightArmStretch = (s16)((u16)work->rightArmStretch + ((s32)(reach << 0x10) >> 0x13));
        }
    } else if ((s16)time >= 0x3C) {
        if (work->rightArmStretch >= 0x200) {
            work->rightArmStretch = (s16)((u16)work->rightArmStretch - 0x200);
        } else {
            work->rightArmStretch = 0;
        }
    }
    if (work->stateCounter == 0x2F) {
        scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    }
    scratch->splashPart = ACTOR_01100_PART_RIGHT_HAND;
    if (work->stateCounter == 0x3C) {
        _actor01100ClearObjPair(work);
    }
    if (work->motionEnded != 0) {
        _actor01100SetSlotRates(work, 0x10);
        work->rightArmStretch = 0;
        if (work->stateStep != 3) {
            work->state     = ACTOR_01100_STATE_ADVANCE;
            work->stateStep = 0;
            work->field_BAA = (u8)work->field_BAA + 1;
            return;
        }
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        if (!((rng >> 0x10) & 3)) {
            work->state = ACTOR_01100_STATE_ADVANCE;
        } else {
            work->startedMotion = 1;
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
    Actor01100_Fn039D0(enemy, task, work, scratch);
    if (work->motionEnded != 0) {
        work->state     = ACTOR_01100_STATE_ADVANCE;
        work->stateStep = 0;
        work->field_BAA = work->field_BAA + 1;
    }
}

/// Distance to actor slot 3, squared, through the scratch pool. Each access of
/// `SCRATCH_STACK_CURSOR_SLOT` is its own inline so the address stays a rematerialized
/// `lui`/`lw` of `0x1F8003FC`, and the macro writes the caller's variable so
/// the distance is one pseudo.
static __inline__ u8* Actor104900_ScratchRead(void)
{
    return SCRATCH_STACK_CURSOR(u8);
}

static __inline__ void Actor104900_ScratchWrite(u8* p)
{
    SCRATCH_STACK_CURSOR(u8) = p;
}

#define Actor104900_DistToPlayer(arg0, out)                           \
    {                                                                 \
        u8*       head;                                               \
        SVECTOR*  vec;                                                \
        GfxCoord* coord;                                              \
        Task*     slot;                                               \
                                                                      \
        slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);                \
        if (slot == NULL) {                                           \
            out = 0x7FFFFFFF;                                         \
        } else {                                                      \
            coord   = slot->extra.tmd->coords;                        \
            head    = Actor104900_ScratchRead();                      \
            vec     = (SVECTOR*)(head - 8);                           \
            vec->vx = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0]; \
            vec->vy = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1]; \
            Actor104900_ScratchWrite((u8*)vec);                       \
            vec->vz = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2]; \
            out     = gfxDotProduct(vec, vec);                        \
            Actor104900_ScratchWrite(Actor104900_ScratchRead() + 8);  \
        }                                                             \
    }

/// Copies column 2 of `arg0` into `arg1` and scales it by `scale` through the
/// GTE's GPF.
static __inline__ void Actor104900_MatrixCol2(MATRIX* arg0, SVECTOR* arg1, s32 scale)
{
    gte_ReadMatrixColumn(arg0, 2, arg1);
    gte_lddp(scale);
    gte_ldsv(arg1);
    gte_gpf12();
    gte_stsv(arg1);
}

/// Lunge. The first frame, while `stateStep` is clear, measures the squared
/// distance to actor slot 3. No spawn argument and a target inside 0xA62B0F, or
/// any target inside 0x1DE83F, consumes one `rand` in the first of those cases
/// and stages state 0xF. Otherwise motion 9 is armed, a nibble of
/// `gRandomLcgState` picks a 1/2/3 `stateCounter` (under 5, under 0xC, else),
/// `strideFrame` is armed to -1 and the latch is stepped. The empty asm before
/// the 3 is not a single set, so that arm stays a fallthrough `li`.
///
/// Later frames step `strideFrame` while the motion id still matches and the
/// clip has not finished, then `Actor01100_Fn039D0` supplies `playerBearing`.
/// While the frame sits in [1, 0x2E) the model's `field_46` turns toward that
/// yaw by at most 0x10 and the Y rotation is rebuilt. The same window steps
/// `((frame - 13) * 900) / 33` and, while
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` is clear, adds the
/// scaled facing column's X/Z onto the translation through the scratch block's
/// `shortVector`. Frame 1 cues `0x400B0002` and frame 0x2E cues `0x400B0001`.
///
/// Frame 0x2E remeasures the distance. Seven draws in eight leave the lunge:
/// `blockedFrames` below 0xB and a yaw inside ±0x300 stage 0xC or 0xD from the
/// sign, flipped by a further one draw in eight, and only while the new
/// distance is inside 0xA62B0F. Anything else stages 0xB inside 0x89543F and
/// 0xF beyond it.
static void Actor01100_Fn0516C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    GfxCoord*      actorCoords;
    GfxCoord*      coords;
    GfxCoord*      pose;
    AnimationSlot* motion;
    s32            dist;
    s32            dist2;
    s32            turn;
    s32            yaw;
    s32            yaw2;
    s32            scale;
    s32            frame;
    s32            n;
    s32            snd;
    u16            angle;
    u32            rng;

    if (work->stateStep == 0) {
        actorCoords = task->extra.tmd->coords;
        Actor104900_DistToPlayer(actorCoords, dist);
        if ((task->spawnArg1.value == 0) && (dist <= 0xA62B0F)) {
            rand();
            work->state     = ACTOR_01100_STATE_FACE_PLAYER;
            work->stateStep = 0;
            return;
        }
        if (dist <= 0x1DE83F) {
            work->state     = ACTOR_01100_STATE_FACE_PLAYER;
            work->stateStep = 0;
            return;
        }
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        work->motion    = 9;
        n               = (rng >> 16) & 0xF;
        if (n < 5) {
            work->stateCounter = 1;
        } else if (n < 0xC) {
            work->stateCounter = 2;
        } else {
            work->stateCounter = 3;
        }
        work->strideFrame = -1;
        work->stateStep   = (u8)work->stateStep + 1;
    } else {
        motion = &work->rig.slots[1];
        if ((work->rig.slots[1].currentPose.indices.setIndex != work->motion) ||
            (work->strideFrame = (u8)work->strideFrame + 1, (motion->currentPose.indices.recordIndex > motion->nextPose.indices.recordIndex))) {
            work->strideFrame = -1;
        }
    }

    Actor01100_Fn039D0(enemy, task, work, scratch);
    if ((u32)((u8)work->strideFrame - 1) < 0x2EU) {
        turn = work->playerBearing;
        pose = task->extra.tmd->coords;
        if (turn >= 0x11) {
            pose->param.rot.vy = (u16)pose->param.rot.vy + 0x10;
        } else if (turn < -0x10) {
            pose->param.rot.vy = (u16)pose->param.rot.vy - 0x10;
        } else {
            pose->param.rot.vy = (u16)pose->param.rot.vy + turn;
        }
        angle              = (u16)pose->param.rot.vy & 0xFFF;
        pose->param.rot.vy = angle;
        gfxRotMatrixY(&pose->coord, angle, 1);
        pose->composeStamp = GRAPHICS_COORD_DIRTY;
        frame              = work->strideFrame;
        scale              = ((frame - 13) * 900) / 33 - ((frame - 14) * 900) / 33;
        coords             = task->extra.tmd->coords;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 0) {
            Actor104900_MatrixCol2(&coords->coord, &scratch->shortVector, scale);
            coords->coord.t[0]  += scratch->shortVector.vx;
            coords->coord.t[2]  += scratch->shortVector.vz;
            coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    if (work->strideFrame == 0x2E) {
        snd = 0x400B0001;
        goto do_sound;
    }
    if (work->strideFrame == 1) {
        snd = 0x400B0002;
    do_sound:
        sndEvtRequestScriptStart((work->waterRoom << 22) | snd | ((u8)work->placeIndex << 8), (s8)scratch->pan, (s8)scratch->depth);
    }
    if (work->strideFrame == 0x2E) {
        actorCoords = task->extra.tmd->coords;
        Actor104900_DistToPlayer(actorCoords, dist2);
        if (rand() & 7) {
            if (work->blockedFrames < 0xBU) {
                yaw = work->playerBearing;
                if (yaw < -0x300) {
                    goto far_state;
                }
                if (yaw < 0x301) {
                    goto close_state;
                }
            }
        far_state:
            if (dist2 <= 0x89543F) {
                work->state = ACTOR_01100_STATE_SPIT;
            } else {
                work->state = ACTOR_01100_STATE_FACE_PLAYER;
            }
            work->stateStep = 0;
            return;
        close_state:
            if (dist2 <= 0xA62B0F) {
                yaw2 = yaw;
                if (!(rand() & 7)) {
                    yaw2 = -yaw2;
                }
                if (yaw2 < 0) {
                    work->state = ACTOR_01100_STATE_PUNCH_LEFT;
                } else {
                    work->state = ACTOR_01100_STATE_PUNCH_RIGHT;
                }
                work->stateStep = 0;
            }
        }
    }
}

/// Spawns a 0x10032 effect showing `model` at the task's model part 6, hands
/// it the task model's texture page and CLUT, and, when the effect's model has
/// a stream buffer, processes that stream twice.
static __inline__ void _actor01100SpawnModelEff(Task* task, TmdSource* model)
{
    EffectWork* eff;
    TmdObject*  owner;
    TmdObject*  tmd;

    // The effect's descriptor, entry 0x32 of bank 1, takes its model from the caller.
    D_800670D0[0x32].data.model = model;
    eff                         = Gp_SpawnEff(EFFECT_FLYING_BODY_PART, &task->extra.tmd->coords[6], 0x200, 0);
    if (eff != NULL) {
        owner                  = task->extra.tmd;
        tmd                    = eff->task->extra.tmd;
        tmd->texturePageOffset = owner->texturePageOffset;
        tmd->clutRowOffset     = owner->clutRowOffset;
        if (tmd->buffer != NULL) {
            tmdBuildBufferHalf(tmd);
            tmdBuildBufferHalf(tmd);
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
            Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
            enemy->spawnState = 0;
        } else {
            Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
        }
        if (enemy->spawnState == 0) {
            enemy->spawnState = work->hitFromBehind + 1;
        }
        work->mode                    = ACTOR_01100_MODE_DYING;
        work->reaction                = ACTOR_01100_REACTION_DYING;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        if (enemy->spawnState == 3) {
            _actor01100SpawnModelEff(task, &_gActor01100BruteMossbackBurstArm);
            _actor01100SpawnModelEff(task, &_gActor01100BruteMossbackBurstHead);
            _actor01100SpawnModelEff(task, &_gActor01100BruteMossbackBurstArm);
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
            Gp_SpawnEff(EFFECT_CORPSE_BURN, task->extra.tmd->coords, 5, 0);
        } else if (time <= 0) {
            extra->flags |= TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
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
        gfxScaleMatrixColumns(&coords[3].coord, &scale);
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

/// Spawns the effect this actor's next state rides on and re-homes the actor.
///
/// The 0x58-byte work block goes in `Task::work` and the effect task comes back
/// from `Gp_SpawnEff` as `0x60081` parented to the model's trailing coordinate;
/// that task becomes `Task::spawnArg2` and the actor's parent, and the actor arms
/// its own 0x5A kill countdown.
///
/// The effect's velocity is a random direction in the actor's frame: an SVECTOR
/// is built 8 bytes into the scratchpad pool below its published head, with X
/// and Z from `rsin` / `rcos` of the spawn argument and Y a 9-bit draw hung below
/// 0xE000, rotated through the actor's current `coord` and then scaled by
/// `((gRandomLcgState >> 16) & 0x1F) + 0x28` of 0x1000, which the work block keeps.
/// The coordinate is reset to the identity first - a 0x1000 diagonal, the
/// off-diagonal pairs written as zeroed words - then the velocity's X and Z are
/// added to its translation and a 7-bit draw to the Y, and `composeStamp` is cleared.
///
/// The collision body is linked as kind 3 pointing at the coordinate and at the
/// 0x28 record, which takes 0x96 for `end0Radius` / `end1Radius` and points
/// `contacts` at the one-entry collision table `worldCollisionInitContacts` zeroes, and
/// its `0xC000` flag pair is ORed in on top of `worldCollisionLinkBody`'s `flags = 3`. The
/// actor takes `Actor01100_Fn073A8` as its exit callback and steps on to the
/// next state, which it also runs immediately.
///
/// The stack copy of the vector is what the first `lwc2` pair reads, and it is
/// written with the sibling bodies' raw asm: `gte_ldv0` of a stack local leaves
/// its `addiu` free for sched2 to hoist, which this body's schedule does not.
static void Actor01100_Fn05E68(Task* task)
{
    _Actor01100SpitWork*   work;
    WorldCollisionCapsule* rec;
    GfxCoord*              coord;
    EffectWork*            eff;
    WorldCollisionBody*    obj;
    SVECTOR*               vec;
    s32                    angle;

    coord = task->extra.tmd->coords;
    work  = memCalloc(sizeof(_Actor01100SpitWork), 0);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }
    task->work = work;
    eff        = Gp_SpawnEff(EFFECT_PROJECTILE_GLOW_SPRITE, coord, 0, 0);
    if (eff == NULL) {
        taskCallExit(task);
        return;
    }
    task->spawnArg2.pointer = eff->task;
    taskReparent(task, eff->task);
    angle               = task->spawnArg1.value;
    task->killCountdown = 0x5A;

    vec             = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    vec->vy         = 0xE000 - ((gRandomLcgState >> 16) & 0x1FF);
    vec->vx         = rsin(angle);
    vec->vz         = rcos(angle);

    _gfxRotateSv(&coord->coord, vec);

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gte_lddp(((gRandomLcgState >> 16) & 0x1F) + 0x28);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(&work->velocity);

    gfxSetRotIdentity(&coord->coord);

    coord->coord.t[0]  += work->velocity.vx;
    gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    coord->coord.t[1]  += (gRandomLcgState >> 16) & 0x7F;
    coord->coord.t[2]  += work->velocity.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    obj                  = &work->body;
    rec                  = &work->capsule;
    obj->coord           = coord;
    obj->context.capsule = rec;
    obj->pos.vx          = 0;
    obj->pos.vy          = 0;
    obj->pos.vz          = 0;
    obj->radius          = 0;
    obj->key             = Gp_PackPair(&Actor01100_D074D0[0], 5);
    obj->flags           = WORLD_COLLISION_BODY_CAPSULE;

    rec->contacts   = work->contacts;
    rec->ends[1].vx = 0;
    rec->ends[1].vy = 0;
    rec->ends[1].vz = 0;
    rec->ends[0].vx = 0;
    rec->ends[0].vy = 0;
    rec->ends[0].vz = 0;
    rec->end0Radius = 0x96;
    rec->end1Radius = 0x96;
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, obj);
    obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    task->exitCallback = Actor01100_Fn073A8;
    SCRATCH_STACK_RELEASE_BYTES(8);
    task->state += 1;
    Actor01100_Fn06198(task);
}

static void Actor01100_Fn06198(Task* task)
{
    _Actor01100SpitWork*   work;
    WorldCollisionCapsule* d4;
    WorldCollisionContact* rec;
    GfxCoord*              coord;
    GfxCoord*              soundCoord;
    Task*                  child;
    u32                    stageAreaKey;
    s32                    flag;
    s32                    id;
    s16                    countdown;

    work          = task->work;
    stageAreaKey  = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    coord         = task->extra.tmd->coords;
    soundCoord    = coord;
    d4            = &work->capsule;
    flag          = stageAreaKey == GAME_LOCATION_KEY(3, 32, 0, 0);
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        d4->ends[1].vx      = -work->velocity.vx;
        d4->ends[1].vy      = -work->velocity.vy;
        d4->ends[1].vz      = -work->velocity.vz;
        coord->coord.t[0]  += work->velocity.vx;
        rec                 = work->contacts;
        coord->coord.t[1]  += work->velocity.vy;
        coord->coord.t[2]  += work->velocity.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->velocity.vy   = work->velocity.vy + 0xA;
        if (worldCollisionCountContactsByKind(rec, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
            child = task->firstChild;
            if (child != NULL) {
                child->spawnArg1.value = 3;
            }
            goto fire;
        }
        if (worldCollisionFindContactIndex(rec, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            child = task->firstChild;
            if (child != NULL) {
                if (rec->response.direction.vy >= -0xC00) {
                    child->spawnArg1.value = 3;
                } else {
                    child->spawnArg1.value = 2;
                }
            }
        fire:
            id = (flag << 22) | (0x400B000B | (Actor01100_D15670 << 8));
            sndEvtRequestScriptStart(id, (s8)worldCoordGetOriginAudioPan(soundCoord), (s8)worldCoordGetOriginAudioDepth(soundCoord));
            work->body.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            task->killCountdown = 0x1E;
            task->state        += 1;
        }
        worldCollisionClearContacts(work->contacts);
        countdown           = task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            taskCallExit(task);
        }
    }
}

static void Actor01100_Fn0638C(Task* task)
{
    EffectWork*            effect;
    s32                    variant;
    s32                    soundBase;
    WorldCollisionContact* rec;
    _Actor01100SpitWork*   work;
    s32                    stageAreaKey;
    s32                    sound;
    s32                    pan;
    WorldCollisionBody*    obj;
    GfxCoord*              coord;
    GfxRotationWords*      rotation;

    coord        = task->extra.tmd->coords;
    stageAreaKey = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK;
    variant      = stageAreaKey == GAME_LOCATION_KEY(3, 32, 0, 0);
    work         = memCalloc(sizeof(_Actor01100SpitWork), 0);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }
    task->work = work;
    soundBase  = (variant << 22) | 0x400B000C;
    sound      = soundBase | (Actor01100_D15670 << 8);
    pan        = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    effect = Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xC0031FFF, NULL);
    if (effect != NULL) {
        taskReparent(task, effect->task);
    }
    task->killCountdown   = 0x5A;
    rotation              = (GfxRotationWords*)&coord->coord;
    obj                   = &work->body;
    rotation->m00M01      = ONE;
    rotation->m02M10      = 0;
    rotation->m11M12      = ONE;
    rotation->m20M21      = 0;
    rotation->m22         = ONE;
    rec                   = work->contacts;
    coord->composeStamp   = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]    += 0x30;
    obj->coord            = coord;
    obj->context.contacts = rec;
    obj->pos.vx           = 0;
    obj->pos.vy           = 0;
    obj->pos.vz           = 0;
    obj->radius           = 0x2EE;
    obj->key              = Gp_PackPair(Actor01100_D074F8, 5);
    obj->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionInitContacts(rec, 1, 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, obj);
    obj->flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = Actor01100_Fn073A8;
    task->state++;
    Actor01100_Fn073DC(task);
}

/// Runs the actor's current state handler, copying the table onto the stack
/// before the call. The handler is handed an `_Actor01100Scratch` borrowed
/// from the scratchpad stack for the duration of the call, with its
/// `splashPart` cleared.
void Actor01100_Fn06554(Task* task)
{
    _Actor01100TaskStateTable sp;
    Enemy*                    enemy;
    void*                     work;
    _Actor01100Scratch*       scratch;

    sp      = Actor01100_D00004;
    enemy   = task->spawnArg2.pointer;
    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor01100Scratch);

    scratch->splashPart = 0;
    sp.funcs[task->state](enemy, task, work, scratch);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor01100Scratch);
}

/// State handlers of the secondary task this entry spawns: set-up
/// (`Actor01100_Fn05E68`), per-frame tick (`Actor01100_Fn06198`) and the
/// countdown to exit (`Actor01100_Fn0736C`).
static const TaskFuncTable3 Actor01100_D000DC = { {
    Actor01100_Fn05E68,
    Actor01100_Fn06198,
    Actor01100_Fn0736C,
} };

/// Runs the task's current state handler out of `Actor01100_D000DC`, copying
/// the table onto the stack before the call.
void Actor01100_Fn065E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor01100_D000DC;
    sp.funcs[task->state](task);
}

/// Runs the task's current state handler from a two-entry table built on the
/// stack: set-up (`Actor01100_Fn0638C`), then the per-frame countdown
/// (`Actor01100_Fn073DC`).
void Actor01100_Fn0663C(Task* task)
{
    TaskFunc funcs[2] = {
        Actor01100_Fn0638C,
        Actor01100_Fn073DC,
    };

    funcs[task->state](task);
}

/// Exit callback: unlink the four collision bodies, relink the second part coord
/// under the model's root, then let gameplay tear the enemy down.
static void Actor01100_Fn0668C(Task* task)
{
    _Actor01100Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              i;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    for (i = 0; i < ACTOR_01100_BODY_COUNT; i++) {
        worldCollisionUnlinkBody(&work->bodies[i]);
    }
    coord           = task->extra.tmd->coords;
    coord[1].parent = coord;
    enemyDestroy(enemy, task);
}

/// Message 0x7D5 handler: switches the enemy's model and collision bodies
/// between hidden and shown. `flags ^ 1` is the requested mode, latched in
/// `hidden` so only a change acts. Mode 1 hides the model and releases the
/// enemy's link node slot, saving its `field_4` first, and clears the 0xC000
/// pair off all four collision bodies; mode 0 puts the saved `field_4` back,
/// lifts the hidden bit, and sets those bits on the first and last collision
/// body.
s32 Actor01100_Fn0670C(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    _Actor01100Work*    work;
    Enemy*              enemy;
    TmdObject*          model;
    WorldCollisionBody* obj;
    s32                 i;
    s32                 mode;

    mode  = flags ^ 1;
    work  = task->work;
    model = task->extra.tmd;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work->hidden != mode) {
        work->hidden = mode;
        if (work->hidden == 0) {
            model->flags                 &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = work->savedTargetFlags;
            obj                           = &work->bodies[ACTOR_01100_BODY_ROOT];
            obj->flags                   |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            obj                           = &work->bodies[ACTOR_01100_BODY_CHEST];
            obj->flags                   |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
        } else {
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->savedTargetFlags        = enemy->node.state.parts.flags;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            for (i = 0; i < ACTOR_01100_BODY_COUNT; i++) {
                obj         = &work->bodies[i];
                obj->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
        }
    }
    return 0;
}

static void Actor01100_Fn067C0(MATRIX* arg0, VECTOR* arg1)
{
    void**   scratch;
    SVECTOR* head;
    SVECTOR* vec;

    scratch                           = SCRATCH_HEAD_ADDR;
    head                              = SCRATCH_HEAD_AT(scratch, SVECTOR);
    vec                               = head - 1;
    SCRATCH_HEAD_AT(scratch, SVECTOR) = vec;

    gte_ReadMatrixColumn(arg0, 0, vec);
    gte_lddp(arg1->vx);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, arg0, 0);

    gte_ReadMatrixColumn(arg0, 1, vec);
    gte_lddp(arg1->vy);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, arg0, 1);

    gte_ReadMatrixColumn(arg0, 2, vec);
    gte_lddp(arg1->vz);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, arg0, 2);

    head                              = SCRATCH_HEAD_AT(scratch, SVECTOR);
    SCRATCH_HEAD_AT(scratch, SVECTOR) = head + 1;
}

/// Bearing of the actor in slot `arg1` of `gPlayerActorTasks` from `arg0`,
/// measured in `arg0`'s own frame and folded into -0x800..0x800; 0 when the
/// slot is empty.
static s32 Actor01100_Fn06954(GfxCoord* arg0, s32 arg1)
{
    Task*                actor;
    GfxCoord*            coord;
    ActorBearingScratch* blk;
    s32                  angle;

    actor = gPlayerActorTasks[arg1];
    if (actor == NULL) {
        return 0;
    }
    coord = actor->extra.tmd->coords;
    blk   = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    angle = actorBearingInFrame(blk, arg0, coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return angle;
}

/// Squared distance from `arg0` to the slot-3 (player) task's root part coord,
/// or `0x7FFFFFFF` when that task is gone. The delta is staged in an `SVECTOR`
/// carved off the scratchpad stack and squared with `gfxDotProduct`.
static s32 Actor01100_Fn06AC8(GfxCoord* arg0)
{
    void**    scratch;
    u8*       head;
    SVECTOR*  vec;
    GfxCoord* coord;
    Task*     task;
    s32       ret;

    task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (task != NULL) {
        coord                          = task->extra.tmd->coords;
        scratch                        = SCRATCH_HEAD_ADDR;
        head                           = SCRATCH_HEAD_AT(scratch, void);
        vec                            = (SVECTOR*)(head - 8);
        vec->vx                        = (u16)coord->workm.t[0] - (u16)arg0->workm.t[0];
        vec->vy                        = (u16)coord->workm.t[1] - (u16)arg0->workm.t[1];
        SCRATCH_HEAD_AT(scratch, void) = vec;
        vec->vz                        = (u16)coord->workm.t[2] - (u16)arg0->workm.t[2];
        ret                            = gfxDotProduct(vec, vec);
        SCRATCH_POP_BYTES_AT(scratch, 8);
    } else {
        ret = 0x7FFFFFFF;
    }
    return ret;
}

/// Steps the actor along its own forward axis: column 2 of the coordinate's
/// rotation is copied into the scratch block's `shortVector`, scaled by GPF
/// with `arg2` as the distance, and added back into the coordinate's X and Z
/// translation. Y is left alone, so the step stays in the ground plane, and
/// clearing `composeStamp` asks the next coordinate update to rebuild the world
/// matrix. Does nothing while the movement freeze flag is set.
///
/// Nothing in the entry calls it. `ACTOR_01100_STATE_ADVANCE` carries the same
/// step inline, on its own coordinate and scratch block.
static void Actor01100_Fn06B6C(GfxCoord* arg0, _Actor01100Scratch* arg1, s32 arg2)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 0) {
        gte_ReadMatrixColumn(&arg0->coord, 2, &arg1->shortVector);
        gte_lddp(arg2);
        gte_ldsv(&arg1->shortVector);
        gte_gpf12();
        gte_stsv(&arg1->shortVector);
        arg0->coord.t[0]  += arg1->shortVector.vx;
        arg0->coord.t[2]  += arg1->shortVector.vz;
        arg0->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Points the enemy's body at the model's fourth part coordinate - the same
/// `TmdObject::coords[3]` that `Gp_UpdateLinkXforms` reads back through
/// `Enemy.coord` - and sets the body position the actor spawns inside, and
/// clears the lock-on node's flags.
///
/// The restart path then needs three things at once: `Actor01100_Fn00F58` idle,
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
    if ((Actor01100_Fn00F58(enemy, task, work, scratch) == 0) && (task->spawnArg1.value == 0)) {
        trigger = work->prevMode;
        if ((trigger == 1) && (work->motionEnded == trigger)) {
            if (Actor01100_Fn06AC8(task->extra.tmd->coords) > 0xA62B10) {
                _actor01100ClearObjPair(work);
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
/// 0xC8-box local offset through `src` - and `Actor01100_Fn00F58` runs last.
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
    Actor01100_Fn00F58(enemy, task, work, scratch);
}

static void Actor01100_Fn06E4C(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    s32 t;
    s32 t2;
    u16 timer;

    if (work->stateStep == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        t               = (gRandomLcgState >> 0x10) & 0xF;
        if (t < 4) {
            work->stateCounter = 2;
        } else if (t < 8) {
            work->stateCounter = 0x3C;
        } else if (t < 0xE) {
            work->stateCounter = 0x78;
        } else {
            work->stateCounter = 0xB4;
        }
        work->motion    = 1;
        work->stateStep = (u8)work->stateStep + 1;
    }
    timer              = (u16)work->stateCounter - 1;
    work->stateCounter = timer;
    if (((u32)timer << 0x10) == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        t2              = (gRandomLcgState >> 0x10) & 0xF;
        if (t2 < 3) {
            work->state = ACTOR_01100_STATE_IDLE_SWING_LEFT;
        } else if (t2 < 6) {
            work->state = ACTOR_01100_STATE_IDLE_SWING_RIGHT;
        } else if (t2 < 0xE) {
            work->state = ACTOR_01100_STATE_IDLE_TURN;
        } else {
            work->state = ACTOR_01100_STATE_IDLE_REST;
        }
        work->stateStep = 0;
    }
}

/// First frame of the sub-state arms motion 5 and a 0x64-frame countdown in
/// `stateCounter`, then bumps `stateStep`. Every later frame steps that
/// countdown, and on the frame it reaches zero cues the 0x400B0004 event - the
/// low byte of `placeIndex` in bits 8..15, `waterRoom` in bit 22, pan and depth
/// from the scratch block - through `sndEvtRequestScriptStart`. The scratch block's
/// `splashPart` then takes 0xC while bit 0 of `gDisplayState.animFrame` is set
/// and 8 otherwise, and `motionEnded` ends the sub-state by clearing both the
/// state and the latch.
static void Actor01100_Fn06F38(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    if (work->stateStep == 0) {
        work->motion       = 5;
        work->stateCounter = 0x64;
        work->stateStep    = (u8)work->stateStep + 1;
        return;
    }
    if (work->stateCounter != 0) {
        work->stateCounter--;
        if (work->stateCounter == 0) {
            sndEvtRequestScriptStart((work->waterRoom << 22) | (((u8)work->placeIndex << 8) | 0x400B0004), (s8)scratch->pan, (s8)scratch->depth);
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

/// Arms the motion pair for the current sub-state when `stateStep` is still
/// clear, and switches to state 0xF when `motionEnded` is set.
static void Actor01100_Fn070DC(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = 0xB;
        } else {
            work->motion = 0xE;
        }
        work->startedMotion = 1;
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
/// `Actor01100_Fn072B8` in `Actor01100_D00064`.
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

/// Sub-state handler built around `stateCounter`.
///
/// The first frame arms it: `motion` takes 0x10, or 0xF while `hitFromBehind`
/// is clear, `downState` is set to 2 and the countdown is zeroed, and
/// `stateStep` is stepped. Every later frame moves the countdown up by one and,
/// on the frame it reaches 0x28 - 0x3C while `hitFromBehind` is set - clears
/// `downState` again. The scratch block's `splashPart` takes 3 either
/// way, and `motionEnded` ends the sub-state by dropping the enemy out of its
/// link-node slot, latching `mode` and switching `state` to 0xF.
///
/// Both halves store into the field from inside each arm rather than through a
/// shared local: a local's first definition would land before the branch on
/// `hitFromBehind`, and jump.c's arm collapse hoists one arm's constant in
/// front of that branch, which then keeps the flag and the value in separate
/// registers.
static void Actor01100_Fn072B8(Enemy* enemy, Task* task, _Actor01100Work* work, _Actor01100Scratch* scratch)
{
    u16 count;

    if (work->stateStep == 0) {
        if (work->hitFromBehind == 0) {
            work->motion = 0xF;
        } else {
            work->motion = 0x10;
        }
        work->downState    = ACTOR_01100_DOWN_RISING;
        work->stateCounter = 0;
        work->stateStep    = (u8)work->stateStep + 1;
        return;
    }
    count              = (u16)work->stateCounter + 1;
    work->stateCounter = count;
    if (work->hitFromBehind == 0) {
        if ((s16)count == 0x28) {
            work->downState = ACTOR_01100_DOWN_STANDING;
        }
    } else {
        if ((s16)count == 0x3C) {
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

/// Teardown state: counts `killCountdown` down and calls the task's exit
/// callback once it reaches zero.
static void Actor01100_Fn0736C(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        taskCallExit(arg0);
    }
}

/// Exit callback of the secondary tasks: takes the work block's collision body
/// back off the object list and kills the task.
static void Actor01100_Fn073A8(Task* arg0)
{
    worldCollisionUnlinkBody(&((_Actor01100SpitWork*)arg0->work)->body);
    taskKill(arg0);
}

/// Per-frame state of the actor's two-state controller (state 1). While
/// `gSceneCombatState.actorControl` is zero the actor runs its self-destruct countdown: from
/// `killCountdown` 0x15 and above it throws an effect burst (0x60070) at the
/// model's root coordinate on every other frame and reparents the spawned
/// effect onto itself, and a collision hit on the work block's `WorldCollisionContact` table
/// -- masked to the 0x10000 slot -- or the countdown reaching 0x14 clears the
/// two 0xC000 bits the spawn state set in the object's flags. The countdown
/// then ticks down and the task calls its exit callback once it reaches zero.
static void Actor01100_Fn073DC(Task* task)
{
    _Actor01100SpitWork* work;
    GfxCoord*            coord;
    WorldCollisionBody*  obj;
    EffectWork*          eff;
    s16                  countdown;

    work  = task->work;
    coord = task->extra.tmd->coords;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (task->killCountdown >= 0x15) {
            if (((u16)task->killCountdown & 1) == 0) {
                eff = Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xC0031FFF, NULL);
                if (eff != NULL) {
                    taskReparent(task, eff->task);
                }
            }
            if (worldCollisionCountContactsByKind(&work->contacts[0], WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
                obj         = &work->body;
                obj->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
        }
        if (task->killCountdown == 0x14) {
            obj         = &work->body;
            obj->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        }
        countdown           = (u16)task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            taskCallExit(task);
        }
    }
}
