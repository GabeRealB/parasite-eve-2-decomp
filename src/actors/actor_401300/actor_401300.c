#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/player_detection.h"
static s32 _actorMsgPlaceRecordYaw(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);
#define ACTOR_MESSAGE_PLACE_RECORD_YAW _actorMsgPlaceRecordYaw
#define ACTOR_MESSAGE_YAW_WORK_TYPE    _Actor401300Work
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"

/// Uniform model-root scale for yaw rebuilds, with 12 fractional bits.
enum { ACTOR_401300_ROOT_SCALE = 0x1964 };

/// HP marker of a reusable placed actor, consumed by the roaming encounter pools.
enum { ACTOR_401300_HP_AVAILABLE = -999 };

/// Previous-state marker forcing the next tick to enter the requested state afresh.
enum { ACTOR_401300_FORCE_STATE_ENTRY = -1 };

/// High spawn-argument word selecting an initially hidden, reusable encounter actor.
enum { ACTOR_401300_SPAWN_REUSABLE = 2 };

/// Values of `_Actor401300Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// A knockdown keeps the side it fell on through the rest and the recovery:
/// a hit from the front leads through the `_BACK` states, one from behind
/// through the `_FRONT` states.
enum {
    ACTOR_401300_STATE_HIDDEN           = 0x00, // not drawn and not collided with; a spawn that starts here reports itself dead
    ACTOR_401300_STATE_PLAY_WALK        = 0x01, // loops the walk animation in place; never selected
    ACTOR_401300_STATE_PLAY_RUN         = 0x02, // loops the run animation in place; never selected
    ACTOR_401300_STATE_PLAY_DOWN        = 0x03, // holds the lying animation; never selected
    ACTOR_401300_STATE_STATUS_HOLD      = 0x04, // twitches in place until the status buildup runs out
    ACTOR_401300_STATE_FLINCH           = 0x05, // recoils from a hit, then chases
    ACTOR_401300_STATE_ALERT            = 0x06, // turns to the player and raises the combat alert
    ACTOR_401300_STATE_CHASE            = 0x07, // runs at the player and picks an attack by distance
    ACTOR_401300_STATE_WITHDRAW         = 0x08, // runs to `withdrawPoint` and leaps out of the room
    ACTOR_401300_STATE_TURN_AROUND      = 0x09, // swings `turnYaw` round to `turnYawTarget`; selected only by `SLIDE`
    ACTOR_401300_STATE_SIDESTEP         = 0x0A, // hops along `sidestepDir`, then chases; never selected
    ACTOR_401300_STATE_GRAB             = 0x0B, // reaches for the player; a player in reach is held
    ACTOR_401300_STATE_GRAB_PULL        = 0x0C, // places the held player and itself for the strike
    ACTOR_401300_STATE_GRAB_STRIKE      = 0x0D, // damages the held player
    ACTOR_401300_STATE_GRAB_DONE        = 0x0E, // does nothing; only a hit leaves it
    ACTOR_401300_STATE_RISE_BACK        = 0x0F, // gets up after `FALL_BACK`
    ACTOR_401300_STATE_RISE_FRONT       = 0x10, // gets up after `FALL_FRONT`
    ACTOR_401300_STATE_DOWN             = 0x11, // lies where it fell until `stateTimer` runs out
    ACTOR_401300_STATE_UNUSED_12        = 0x12, // has no handler and is never selected
    ACTOR_401300_STATE_FALL_BACK        = 0x13, // hit from the front: staggers backward and falls
    ACTOR_401300_STATE_FALL_FRONT       = 0x14, // hit from behind: falls forward
    ACTOR_401300_STATE_DEATH_BURN       = 0x15, // burns away: the corpse-burn effect, then the model flattens and fades
    ACTOR_401300_STATE_DORMANT          = 0x16, // idles until the player comes near or makes noise
    ACTOR_401300_STATE_DORMANT_SCRIPTED = 0x17, // the same wait, entered by a room command, on an animation of its own
    ACTOR_401300_STATE_PATROL           = 0x18, // walks between the two `patrolPoints`
    ACTOR_401300_STATE_BACK_OFF         = 0x19, // faces the player, backs away and turns aside; never selected
    ACTOR_401300_STATE_SLIDE            = 0x1A, // coasts forward by the shrinking `slideStep`; never selected
    ACTOR_401300_STATE_GRAB_WINDUP      = 0x1B, // turns to the player for eleven frames, then grabs; never selected
    ACTOR_401300_STATE_BEND_OVER        = 0x1C, // bends parts 1 to 5 forward, then straightens and chases; never selected
    ACTOR_401300_STATE_DEATH_BURST      = 0x1D, // bursts into body parts where it stands
    ACTOR_401300_STATE_STALK            = 0x1E, // walks at the player and grabs once close and facing
    ACTOR_401300_STATE_STRIKE_A         = 0x1F, // close attack carrying attack 0 on `attackBody`
    ACTOR_401300_STATE_STRIKE_B         = 0x20, // close attack carrying attack 1 on `attackBody`
    ACTOR_401300_STATE_CHARGE           = 0x21, // runs the player down and knocks them over
    ACTOR_401300_STATE_LEAP             = 0x22, // crouches, then leaps `leapStep` a frame at the player
    ACTOR_401300_STATE_LEAP_IN          = 0x23, // leaps into the room with its colors ramping up from black
    ACTOR_401300_STATE_DEAD             = 0x24, // reports the enemy dead and waits to be destroyed
    ACTOR_401300_STATE_REFALL_BACK      = 0x25, // knocked down again while in `RISE_BACK`
    ACTOR_401300_STATE_REFALL_FRONT     = 0x26, // knocked down again while in `RISE_FRONT`
    ACTOR_401300_STATE_WOUNDED          = 0x27, // spawned lying and twitching; rests in `DOWN` once its health is gone
    ACTOR_401300_STATE_DEATH_BURST_WALK = 0x28, // walks a few steps, bursts and burns away
    ACTOR_401300_STATE_COUNT                    // number of states, and of the handlers in `_Actor401300StateTable`
};

/// Values of `_Actor401300Work::animRequest` and `_Actor401300Work::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
/// The blend rig is never asked to blend in.
enum {
    ACTOR_401300_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames its transition table gives
    ACTOR_401300_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ACTOR_401300_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Work block of the Horned Stranger task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`; the
/// exit callback unlinks its three collision bodies. It holds the state
/// machine, both animation rigs and their driver's state, the bodies with
/// their contact records, storage for the model's matrices, and the records
/// of the messages that hold, place, push and animate the player.
///
/// Angles are 4096ths of a turn and positions are in the root coordinate's
/// parent space unless a field says otherwise. Animation ids index the
/// package's animation bank; rates are sixteenths of a frame per tick.
typedef struct {
    s16                      state;             // `ACTOR_401300_STATE_*`
    s16                      prevState;         // `state` the tick last ran; -1 makes the next tick enter `state` afresh
    s16                      stateEntered;      // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16                      stateTimer;        // frame counter of the state: most count up from 0, `DOWN` counts down, `WOUNDED` runs up from a negative draw
    s16                      blockedFrames;     // consecutive ticks, from the charge's 21st, on which the room grid moved the root; seven end it in a recoil
    byte                     field_A[0x2];      // never accessed
    ActorPatrolPoint         patrolPoints[2];   // the spawn position and a point 2000 units ahead along the spawn facing
    s16                      leapStep;          // forward step per frame of the leap: the player's distance plus 1000, clamped to 3000..5000, over 18
    s16                      patrolTarget;      // index into `patrolPoints` of the end being walked toward
    s16                      placedYaw;         // heading of the root after the last placement message; never read
    byte                     field_1A[0x6];     // never accessed
    ActorAnimRig19           rig;               // playback of the model's parts; slot 1's status and frame time the states
    ActorAnimRig19           blend;             // second playback of the same model, mixed into parts 1 to 6, 9 and 10 while `blendActive`
    s32                      grabAnimFrame;     // slot 1's frame on each tick of the grab's strike; never read
    s16                      animRequest;       // `ACTOR_401300_ANIM_REQUEST_*` for `rig`
    s16                      blendActive;       // 1 while `blend`'s animation is mixed in; cleared when its slot 1 settles
    s16                      appliedAnim;       // animation `rig` was last started on
    s16                      animId;            // animation requested of `rig`
    s16                      animFrames;        // ticks since `animRequest` was last applied; never read
    s16                      animRate;          // playback rate of `rig`'s slots; negative plays backward
    s16                      chaseRate;         // `animRate` of the pursuit, dormant and recovery states: 17 on odd place indices, 15 on even; also scales the chase step
    s16                      blendRequest;      // `ACTOR_401300_ANIM_REQUEST_*` for `blend`; only `RESET` is requested
    s16                      blendAnimId;       // animation requested of `blend`
    s16                      blendRate;         // playback rate of `blend`'s slots, 0x30 from each restart
    s16                      blendWeight;       // share of `blend`'s pose in the mix, of 0x1000; 0x800 from each restart
    s16                      lookYawTarget;     // bearing to what the state faces, relative to the facing
    s16                      lookYaw;           // eased toward `lookYawTarget` by 0x100 a tick; within +-0x400, parts 5 and 2 turn by 2/3 and 1/2 of it
    s16                      jointPairTarget;   // value `jointPairBlend` moves toward
    s16                      jointPairBlend;    // pose of the mirrored parts 7 and 8, which no animation drives: 0 their first fixed rotation, 0x200 their second
    s16                      jointPairStep;     // change of `jointPairBlend` per tick
    s32                      lastCueFrame;      // slot 1's frame when a cue last fired, so a held frame fires once; 0 off a cue frame
    GfxCoord                 burnCoord;         // view-space anchor of the corpse-burn effect, placed under part 2 at the root's height
    EffectSpawnArg           effectArg;         // argument record of the hit effects, hung off part 1
    byte                     field_918[0x8];    // never accessed
    GfxCoord                 gridCoord;         // unrotated view-space coordinate at the root's position less 0x15E in Y, carrying `gridBody`
    WorldCollisionBody       hitBody;           // sphere at part 1 in view space; takes the hits and is pushed off other bodies
    WorldCollisionContact    hitContacts[12];   // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody       gridBody;          // sphere on `gridCoord` that the room grid pushes the root with
    WorldCollisionContact    gridContacts[12];  // contacts of `gridBody`
    WorldCollisionBody       attackBody;        // sphere on part 3 carrying the attack key; enabled only during a strike's swing
    WorldCollisionContact    attackContacts[1]; // the one contact of `attackBody`; a kind-0x10000 record there ends the swing
    MATRIX                   lightMtx;          // storage for the model's `TmdObject::lightMtx`
    MATRIX                   colorMtx;          // storage for the model's `TmdObject::colorMtx`; `LEAP_IN` scales it down on its first 17 ticks
    MATRIX                   savedColorMtx;     // `colorMtx` as `DORMANT` found it; never read
    s16                      hitCooldown;       // ticks before another hit is taken; set from the hit's id parameter 2
    s16                      deathPending;      // 1 from the hit that empties the health until the battle reference is released, which waits for the player's release
    SVECTOR                  sidestepDir;       // unit direction of the sidestep, to one side of the bearing to the player
    s16                      turnYaw;           // heading the turn-around holds the root at, moved 0x89 a tick
    s16                      turnYawTarget;     // heading the turn-around ends on: the facing plus twice the bearing to the player
    s16                      slideStep;         // forward step of `SLIDE`, 10 less each tick; nothing seeds it
    byte                     field_C9A[0x2];    // never accessed
    s16                      sidestepSide;      // side the next sidestep takes (1 or -1), flipped by each; 0 draws one at random
    s16                      sidestepStep;      // length of the sidestep's step, 0xDE; halved while `blendActive`
    u16                      downFramesBase;    // ticks `DOWN` lasts, before a random 0..15 more; first value of the variant record, 0 in all three
    s16                      sidestepAngle;     // angle between the bearing to the player and `sidestepDir`; second value of the variant record
    s16                      field_CA4;         // third value of the variant record; never read, role unproven
    byte                     field_CA6[0x2];    // never accessed
    u8                       commandBytes[3];   // first three bytes of the last actor command received; never read
    AnimationPlayRequest     playerAnim;        // animation the held player is sent; `animationId` 1..7 also tracks the reaction's stage
    GameActorMoveBy          playerMove;        // the push sent to the knocked-over player each tick, halved as it goes
    ActorTransform           playerPlacement;   // where the player is placed as a hold or a knock-over starts
    GameActorButtonPressHold playerButtonHold;  // the button-press hold sent to the player; only `pressCount` is filled in
    SVECTOR                  leapStartPos;      // root position when `LEAP` or `LEAP_IN` began; its bearing from the player picks the fall a leap's hit causes
    Task*                    childTask0;        // killed by the exit callback when set; nothing sets it
    Task*                    childTask1;        // killed by the exit callback when set; nothing sets it
    SVECTOR                  withdrawPoint;     // where `WITHDRAW` runs to, with the heading to face there in `pad`
    s16                      field_D1C;         // cleared by hits and by the pursuit states and tested `< 2` when the turn-around ends; nothing raises it, role unproven
    s16                      sidestepCount;     // sidesteps since the last hit or grab; the first is thrown 0x171 wider
    s16                      playerHeld;        // 1 while the player is in a hold or knock-over this enemy started
    s16                      playerAnimFrames;  // ticks since `playerAnim` was last sent
    byte                     field_D24[0x4];    // never accessed
    SVECTOR                  bodyPosHistory[7]; // ring of part 2's view-space position on the last seven ticks; a sidestep aims the enemy's target point at the oldest
    byte                     field_D60[0x18];   // never accessed
    s16                      bodyPosCursor;     // index of the next entry of `bodyPosHistory` to write
} _Actor401300Work;
STATIC_ASSERT_SIZEOF(_Actor401300Work, 0xD7C);

/// The actor's state handlers, indexed by `_Actor401300Work::state`.
///
/// The package defines one table. The per-frame tick copies it to the stack
/// before calling the entry of the current state. The call is unconditional,
/// so the `NULL` entry of `ACTOR_401300_STATE_UNUSED_12` marks a state the
/// actor must not be in when the tick dispatches.
typedef struct {
    TaskFunc handlers[ACTOR_401300_STATE_COUNT]; // Handler of each state, taking the actor's task
} _Actor401300StateTable;
STATIC_ASSERT_SIZEOF(_Actor401300StateTable, ACTOR_401300_STATE_COUNT * sizeof(TaskFunc));

extern ActorHeightClamp D_actor_401300_801589C8[];

/// Halfword table in the overlay's data; element 0 is the value the 0xB05/0xC
/// event writes into `Enemy::hp`. Declared as an array: a scalar lets
/// the scheduler hoist its load above the preceding store.

/// Per-animation reset argument for `animationSeekSlotWithBlend`, indexed by the previous
/// and the new animation id (`appliedAnim`, `animId`).
extern s8 D_actor_401300_8015804C[][45];

/// Two rest/target rotation pairs `_actor401300BlendFixedJoints` blends by
/// `0x200 - t` (in 1/512ths) into coord 7 and coord 8.
extern SVECTOR D_actor_401300_801589F8[2];
extern SVECTOR D_actor_401300_80158A08[2];

/// Data `_actor401300Spawn` wires up at init: the enemy parameter
/// record (`Enemy::param`), the three variant records `spawnArg1 & 0xF` picks
/// `downFramesBase`, `sidestepAngle` and `field_CA4` from, the animation bank passed to
/// `animationBindContext`, the 0x3FF message seed, and the task's `field_24`.
extern EnemyParams                D_actor_401300_80141FA0;
extern ActorHornedStrangerVariant D_actor_401300_80141FB0[3];
extern AnimationSet*              D_actor_401300_80158838[46];
/// The animation block the 0x3FF payload in `playerAnim` hands the player.
extern AnimationSet*        D_actor_401300_801588F0[9];
extern AnimationPlayRequest D_actor_401300_80158914;
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_401300_80158988[8];

/// Impact offsets selected by `_actor401300SelectHitOffset` from the incoming yaw.
///
/// Front draws reach 0..3, rear draws 5/7, and side draws 8/9 or 10/11.
/// The retained masks leave 4/6 unreachable. XYZ uses local game units;
/// `pad` selects model part 2, 7 or 9 for the spawned effect.
extern SVECTOR D_actor_401300_80158928[12];

/// The scratch-stack block of the two states that run at a point: the chase,
/// at the player, and the withdrawal, at `_Actor401300Work::withdrawPoint`.
///
/// One block serves one frame of one state, and nothing carries over to the
/// next. A running frame of either state fills `delta` and `turn`. The
/// withdrawal fills `offset` and `distance` on each of those frames as well,
/// widening `delta`; the chase fills them only on a frame whose turn needed no
/// limiting, measuring afresh once the frame's pushes and forward step have
/// moved the actor. The withdrawal's leap out of the room uses `turn` alone.
/// Yaws are 4096 units per turn, and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    VECTOR  offset;          // Target's position minus the actor's at full width, world units; `pad` is never written
    s32     distance;        // Length of `offset`. The chase picks its attack by it; the withdrawal's leap out of the room waits for it to drop under 0x898
    SVECTOR delta;           // Target's position minus the actor's as the frame began, low 16 bits of each axis; `vx` and `vz` give the bearing, and `pad` is never written
    byte    unknown_1C[0x4]; // Reserved with the block and never accessed; role unproven
    s16     turn;            // Wrapped turn from the actor's heading to the target, then that turn limited to 0x30 either way, then the limited turn added to the heading: the yaw the actor's rotation is rebuilt around. The leap out of the room puts the heading itself here
} _Actor401300RunScratch;
STATIC_ASSERT_SIZEOF(_Actor401300RunScratch, 0x24);

/// The scratch-stack block of the leap at the player.
///
/// One block serves one frame of the state, and nothing carries over to the
/// next. Every frame fills `delta` with the player's offset. The crouch turns
/// by it and, on the frame it ends, measures `offset` and `leapDistance` to
/// set the step of the leap. A leap that catches the player takes `delta` and
/// `turn` over to work out which way the player falls and the push that
/// follows. Yaws are 4096 units per turn, and a wrapped one lies in
/// [-0x800, 0x800].
typedef struct {
    VECTOR  offset;          // Player's position minus the actor's at full width as the crouch ends, world units; `pad` is never written
    SVECTOR delta;           // Player's position minus the actor's, low 16 bits of each axis. On a catch it is replaced by the place the leap began minus the player's position, then by the player's forward axis flattened and scaled to the step the player is pushed along it; `pad` is never written
    s32     leapDistance;    // Ground the leap is to cover: the length of `offset` plus 1000, kept within 3000..5000. An eighteenth of it is the step per frame
    byte    unknown_1C[0x4]; // Reserved with the block and never accessed; role unproven
    s16     turn;            // Wrapped turn from the actor's heading to the player, limited to 0x10 either way and added to the heading to steer the crouch; on a catch, the turn from the player's heading to the place the leap began
} _Actor401300LeapScratch;
STATIC_ASSERT_SIZEOF(_Actor401300LeapScratch, 0x24);

/// The scratch-stack block of the charge.
///
/// One block serves one frame of the state, and nothing carries over to the
/// next. It has the size of `ActorChaseScratch` and the same vector and turn,
/// but the charge keeps what the grid push reported where that block holds a
/// heading, and never touches the two words that block keeps for the player's
/// yaws. Yaws are 4096 units per turn, and a wrapped one lies in
/// [-0x800, 0x800].
typedef struct {
    SVECTOR delta;          // Player's position minus the actor's, world units; on a catch it is reversed to turn the player, then replaced by the player's forward axis flattened and scaled to the step the player is knocked along it; `pad` is never written
    byte    unknown_8[0x4]; // Reserved with the block and never accessed; role unproven
    s16     turn;           // Wrapped turn from the actor's heading to the player, limited to 6 either way and added to the heading to steer; on a catch, the turn from the player's heading to the actor
    s16     gridPushed;     // 1 when the room grid's correction moved the actor across the ground this frame, else 0
} _Actor401300ChargeScratch;
STATIC_ASSERT_SIZEOF(_Actor401300ChargeScratch, 0x10);

/// Scratch reservation of the leap into the room; only the final vector is accessed.
///
/// The first sixteen bytes are reserved but untouched, with no established type.
/// The player offset is written at signed-halfword width and never read back.
typedef struct {
    byte    unknown_0[0x10]; // Reserved and untouched; role unproven
    SVECTOR playerOffset;    // Player minus actor in parent-space units; pad is untouched
} _Actor401300LeapInScratch;
STATIC_ASSERT_SIZEOF(_Actor401300LeapInScratch, 0x18);

/// Player knockdown clips, fatal scripted hold and vibration arguments of impact holds.
enum {
    ACTOR_401300_PLAYER_ANIM_FALL_BACK           = 4,
    ACTOR_401300_PLAYER_ANIM_FALL_FRONT          = 5,
    ACTOR_401300_PLAYER_FATAL_HOLD_STATE         = 10,
    ACTOR_401300_KNOCKDOWN_MOTOR_TICKS           = 16,
    ACTOR_401300_KNOCKDOWN_MOTOR_START_INTENSITY = 8,
    ACTOR_401300_KNOCKDOWN_MOTOR_END_INTENSITY   = 255
};

/// Gameplay slot `effectSpawn` effects read their model data from; set before
/// each spawn in `_actor401300StateDeathBurst`.

/// The records closing three of the overlay's model streams, which
/// `_actor401300StateDeathBurst` points `D_80114B34[5].data.model` at before spawning.
static TmdSource _gActor401300HornedStrangerEffect1;
static TmdSource _gActor401300HornedStrangerBurstHead;
static TmdSource _gActor401300HornedStrangerEffect2;

/// Overlay-data word `_actor401300StateDormantScripted` points
/// `D_actor_401300_80158838[16]` at on entering its state.
extern AnimationSet gActor401300Animation20D98;

static void _actor401300ReleaseResources(Task* task);
static void _actor401300StateHidden(Task* task);
static void _actor401300StatePlayWalk(Task* actor);
static void _actor401300StatePlayRun(Task* actor);
static void _actor401300StatePlayDown(Task* actor);
static void _actor401300StateFlinch(Task* actor);
static void _actor401300StateGrabDone(Task* task);
static void _actor401300StateRiseBack(Task* actor);
static void _actor401300StateRiseFront(Task* actor);
static void _actor401300StateDown(Task* actor);
static void _actor401300StateDead(Task* task);

extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

extern DamageAttack D_actor_401300_80141F88[];

static AnimationSet _gActor401300Animation24A48;
static AnimationSet _gActor401300Animation251E4;
static AnimationSet _gActor401300Animation259F0;
static AnimationSet _gActor401300Animation26204;
static TmdSource    _gActor401300HornedStrangerBody;
static s32          _actor401300ApplyCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg);
static s32          _actor401300PlayMessageAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg);
static void         _actor401300IgnoreMessage2015(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);
static void         _actor401300Task(Task* task);

DamageAttack D_actor_401300_80141F88[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams D_actor_401300_80141FA0 = {
    D_actor_401300_80141F88,
    420,
    115,
    200,
    5,
    100,
    10,
    100,
    10,
};

ActorHornedStrangerVariant D_actor_401300_80141FB0[3] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

static TmdBone _gActor401300HornedStrangerBodySkeleton[21] = {
#include "assets/horned_stranger_body_skeleton.inc"
};

static u32 _gActor401300HornedStrangerBodyPartVerts[21] = {
#include "assets/horned_stranger_body_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerBodyVerts[293] = {
#include "assets/horned_stranger_body_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerBodyNormals[363] = {
#include "assets/horned_stranger_body_normals.inc"
};

static u32 _gActor401300HornedStrangerBodyStream[3776] = {
#include "assets/horned_stranger_body_stream.inc"
};

static TmdSource _gActor401300HornedStrangerBody = {
    0,
    18488,
    7600,
    21,
    _gActor401300HornedStrangerBodyPartVerts,
    _gActor401300HornedStrangerBodyVerts,
    _gActor401300HornedStrangerBodyNormals,
    _gActor401300HornedStrangerBodySkeleton,
    _gActor401300HornedStrangerBodyStream,
};

static TmdBone _gActor401300HornedStrangerEffect1Skeleton[1] = {
#include "assets/horned_stranger_effect_1_skeleton.inc"
};

static u32 _gActor401300HornedStrangerEffect1PartVerts[1] = {
#include "assets/horned_stranger_effect_1_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect1Verts[31] = {
#include "assets/horned_stranger_effect_1_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect1Normals[1] = {
#include "assets/horned_stranger_effect_1_normals.inc"
};

static u32 _gActor401300HornedStrangerEffect1Stream[302] = {
#include "assets/horned_stranger_effect_1_stream.inc"
};

static TmdSource _gActor401300HornedStrangerEffect1 = {
    0,
    2004,
    0,
    1,
    _gActor401300HornedStrangerEffect1PartVerts,
    _gActor401300HornedStrangerEffect1Verts,
    _gActor401300HornedStrangerEffect1Normals,
    _gActor401300HornedStrangerEffect1Skeleton,
    _gActor401300HornedStrangerEffect1Stream,
};

static TmdBone _gActor401300HornedStrangerBurstHeadSkeleton[1] = {
#include "assets/horned_stranger_burst_head_skeleton.inc"
};

static u32 _gActor401300HornedStrangerBurstHeadPartVerts[1] = {
#include "assets/horned_stranger_burst_head_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerBurstHeadVerts[70] = {
#include "assets/horned_stranger_burst_head_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerBurstHeadNormals[85] = {
#include "assets/horned_stranger_burst_head_normals.inc"
};

static u32 _gActor401300HornedStrangerBurstHeadStream[660] = {
#include "assets/horned_stranger_burst_head_stream.inc"
};

static TmdSource _gActor401300HornedStrangerBurstHead = {
    0,
    4468,
    0,
    1,
    _gActor401300HornedStrangerBurstHeadPartVerts,
    _gActor401300HornedStrangerBurstHeadVerts,
    _gActor401300HornedStrangerBurstHeadNormals,
    _gActor401300HornedStrangerBurstHeadSkeleton,
    _gActor401300HornedStrangerBurstHeadStream,
};

static TmdBone _gActor401300HornedStrangerEffect2Skeleton[1] = {
#include "assets/horned_stranger_effect_2_skeleton.inc"
};

static u32 _gActor401300HornedStrangerEffect2PartVerts[1] = {
#include "assets/horned_stranger_effect_2_partVerts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect2Verts[7] = {
#include "assets/horned_stranger_effect_2_verts.inc"
};

static SVECTOR _gActor401300HornedStrangerEffect2Normals[7] = {
#include "assets/horned_stranger_effect_2_normals.inc"
};

static u32 _gActor401300HornedStrangerEffect2Stream[84] = {
#include "assets/horned_stranger_effect_2_stream.inc"
};

static TmdSource _gActor401300HornedStrangerEffect2 = {
    0,
    488,
    0,
    1,
    _gActor401300HornedStrangerEffect2PartVerts,
    _gActor401300HornedStrangerEffect2Verts,
    _gActor401300HornedStrangerEffect2Normals,
    _gActor401300HornedStrangerEffect2Skeleton,
    _gActor401300HornedStrangerEffect2Stream,
};

static AnimationPackedPose _gActor401300Animation17714Bank1[28] = {
#include "assets/actor_401300_animation_17714_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation17714Bank4[247] = {
#include "assets/actor_401300_animation_17714_bank4.inc"
};

static AnimationRecord _gActor401300Animation17714Records[362] = {
#include "assets/actor_401300_animation_17714_records.inc"
};

static u16 _gActor401300Animation17714Indices[20] = {
#include "assets/actor_401300_animation_17714_indices.inc"
};

static AnimationSet _gActor401300Animation17714 = {
    _gActor401300Animation17714Records,
    _gActor401300Animation17714Indices,
    { NULL, _gActor401300Animation17714Bank1, NULL, NULL, _gActor401300Animation17714Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation186D8Bank1[29] = {
#include "assets/actor_401300_animation_186D8_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation186D8Bank4[374] = {
#include "assets/actor_401300_animation_186D8_bank4.inc"
};

static AnimationRecord _gActor401300Animation186D8Records[528] = {
#include "assets/actor_401300_animation_186D8_records.inc"
};

static u16 _gActor401300Animation186D8Indices[20] = {
#include "assets/actor_401300_animation_186D8_indices.inc"
};

static AnimationSet _gActor401300Animation186D8 = {
    _gActor401300Animation186D8Records,
    _gActor401300Animation186D8Indices,
    { NULL, _gActor401300Animation186D8Bank1, NULL, NULL, _gActor401300Animation186D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation19700Bank1[27] = {
#include "assets/actor_401300_animation_19700_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation19700Bank4[406] = {
#include "assets/actor_401300_animation_19700_bank4.inc"
};

static AnimationRecord _gActor401300Animation19700Records[527] = {
#include "assets/actor_401300_animation_19700_records.inc"
};

static u16 _gActor401300Animation19700Indices[20] = {
#include "assets/actor_401300_animation_19700_indices.inc"
};

static AnimationSet _gActor401300Animation19700 = {
    _gActor401300Animation19700Records,
    _gActor401300Animation19700Indices,
    { NULL, _gActor401300Animation19700Bank1, NULL, NULL, _gActor401300Animation19700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1A170Bank1[24] = {
#include "assets/actor_401300_animation_1A170_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1A170Bank4[248] = {
#include "assets/actor_401300_animation_1A170_bank4.inc"
};

static AnimationRecord _gActor401300Animation1A170Records[328] = {
#include "assets/actor_401300_animation_1A170_records.inc"
};

static u16 _gActor401300Animation1A170Indices[20] = {
#include "assets/actor_401300_animation_1A170_indices.inc"
};

static AnimationSet _gActor401300Animation1A170 = {
    _gActor401300Animation1A170Records,
    _gActor401300Animation1A170Indices,
    { NULL, _gActor401300Animation1A170Bank1, NULL, NULL, _gActor401300Animation1A170Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1A750Bank1[10] = {
#include "assets/actor_401300_animation_1A750_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1A750Bank4[138] = {
#include "assets/actor_401300_animation_1A750_bank4.inc"
};

static AnimationRecord _gActor401300Animation1A750Records[188] = {
#include "assets/actor_401300_animation_1A750_records.inc"
};

static u16 _gActor401300Animation1A750Indices[20] = {
#include "assets/actor_401300_animation_1A750_indices.inc"
};

static AnimationSet _gActor401300Animation1A750 = {
    _gActor401300Animation1A750Records,
    _gActor401300Animation1A750Indices,
    { NULL, _gActor401300Animation1A750Bank1, NULL, NULL, _gActor401300Animation1A750Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1AD34Bank1[8] = {
#include "assets/actor_401300_animation_1AD34_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1AD34Bank4[151] = {
#include "assets/actor_401300_animation_1AD34_bank4.inc"
};

static AnimationRecord _gActor401300Animation1AD34Records[182] = {
#include "assets/actor_401300_animation_1AD34_records.inc"
};

static u16 _gActor401300Animation1AD34Indices[20] = {
#include "assets/actor_401300_animation_1AD34_indices.inc"
};

static AnimationSet _gActor401300Animation1AD34 = {
    _gActor401300Animation1AD34Records,
    _gActor401300Animation1AD34Indices,
    { NULL, _gActor401300Animation1AD34Bank1, NULL, NULL, _gActor401300Animation1AD34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1B788Bank1[20] = {
#include "assets/actor_401300_animation_1B788_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1B788Bank4[228] = {
#include "assets/actor_401300_animation_1B788_bank4.inc"
};

static AnimationRecord _gActor401300Animation1B788Records[353] = {
#include "assets/actor_401300_animation_1B788_records.inc"
};

static u16 _gActor401300Animation1B788Indices[20] = {
#include "assets/actor_401300_animation_1B788_indices.inc"
};

static AnimationSet _gActor401300Animation1B788 = {
    _gActor401300Animation1B788Records,
    _gActor401300Animation1B788Indices,
    { NULL, _gActor401300Animation1B788Bank1, NULL, NULL, _gActor401300Animation1B788Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1C4CCBank1[23] = {
#include "assets/actor_401300_animation_1C4CC_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1C4CCBank4[334] = {
#include "assets/actor_401300_animation_1C4CC_bank4.inc"
};

static AnimationRecord _gActor401300Animation1C4CCRecords[426] = {
#include "assets/actor_401300_animation_1C4CC_records.inc"
};

static u16 _gActor401300Animation1C4CCIndices[20] = {
#include "assets/actor_401300_animation_1C4CC_indices.inc"
};

static AnimationSet _gActor401300Animation1C4CC = {
    _gActor401300Animation1C4CCRecords,
    _gActor401300Animation1C4CCIndices,
    { NULL, _gActor401300Animation1C4CCBank1, NULL, NULL, _gActor401300Animation1C4CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1CD3CBank1[14] = {
#include "assets/actor_401300_animation_1CD3C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1CD3CBank4[216] = {
#include "assets/actor_401300_animation_1CD3C_bank4.inc"
};

static AnimationRecord _gActor401300Animation1CD3CRecords[262] = {
#include "assets/actor_401300_animation_1CD3C_records.inc"
};

static u16 _gActor401300Animation1CD3CIndices[20] = {
#include "assets/actor_401300_animation_1CD3C_indices.inc"
};

static AnimationSet _gActor401300Animation1CD3C = {
    _gActor401300Animation1CD3CRecords,
    _gActor401300Animation1CD3CIndices,
    { NULL, _gActor401300Animation1CD3CBank1, NULL, NULL, _gActor401300Animation1CD3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1D0CCBank1[5] = {
#include "assets/actor_401300_animation_1D0CC_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1D0CCBank4[82] = {
#include "assets/actor_401300_animation_1D0CC_bank4.inc"
};

static AnimationRecord _gActor401300Animation1D0CCRecords[111] = {
#include "assets/actor_401300_animation_1D0CC_records.inc"
};

static u16 _gActor401300Animation1D0CCIndices[20] = {
#include "assets/actor_401300_animation_1D0CC_indices.inc"
};

static AnimationSet _gActor401300Animation1D0CC = {
    _gActor401300Animation1D0CCRecords,
    _gActor401300Animation1D0CCIndices,
    { NULL, _gActor401300Animation1D0CCBank1, NULL, NULL, _gActor401300Animation1D0CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1D904Bank1[16] = {
#include "assets/actor_401300_animation_1D904_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1D904Bank4[199] = {
#include "assets/actor_401300_animation_1D904_bank4.inc"
};

static AnimationRecord _gActor401300Animation1D904Records[259] = {
#include "assets/actor_401300_animation_1D904_records.inc"
};

static u16 _gActor401300Animation1D904Indices[20] = {
#include "assets/actor_401300_animation_1D904_indices.inc"
};

static AnimationSet _gActor401300Animation1D904 = {
    _gActor401300Animation1D904Records,
    _gActor401300Animation1D904Indices,
    { NULL, _gActor401300Animation1D904Bank1, NULL, NULL, _gActor401300Animation1D904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1DE54Bank1[10] = {
#include "assets/actor_401300_animation_1DE54_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1DE54Bank4[113] = {
#include "assets/actor_401300_animation_1DE54_bank4.inc"
};

static AnimationRecord _gActor401300Animation1DE54Records[177] = {
#include "assets/actor_401300_animation_1DE54_records.inc"
};

static u16 _gActor401300Animation1DE54Indices[20] = {
#include "assets/actor_401300_animation_1DE54_indices.inc"
};

static AnimationSet _gActor401300Animation1DE54 = {
    _gActor401300Animation1DE54Records,
    _gActor401300Animation1DE54Indices,
    { NULL, _gActor401300Animation1DE54Bank1, NULL, NULL, _gActor401300Animation1DE54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1ED6CBank1[26] = {
#include "assets/actor_401300_animation_1ED6C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1ED6CBank4[330] = {
#include "assets/actor_401300_animation_1ED6C_bank4.inc"
};

static AnimationRecord _gActor401300Animation1ED6CRecords[538] = {
#include "assets/actor_401300_animation_1ED6C_records.inc"
};

static u16 _gActor401300Animation1ED6CIndices[20] = {
#include "assets/actor_401300_animation_1ED6C_indices.inc"
};

static AnimationSet _gActor401300Animation1ED6C = {
    _gActor401300Animation1ED6CRecords,
    _gActor401300Animation1ED6CIndices,
    { NULL, _gActor401300Animation1ED6CBank1, NULL, NULL, _gActor401300Animation1ED6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1F110Bank1[10] = {
#include "assets/actor_401300_animation_1F110_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1F110Bank4[73] = {
#include "assets/actor_401300_animation_1F110_bank4.inc"
};

static AnimationRecord _gActor401300Animation1F110Records[110] = {
#include "assets/actor_401300_animation_1F110_records.inc"
};

static u16 _gActor401300Animation1F110Indices[20] = {
#include "assets/actor_401300_animation_1F110_indices.inc"
};

static AnimationSet _gActor401300Animation1F110 = {
    _gActor401300Animation1F110Records,
    _gActor401300Animation1F110Indices,
    { NULL, _gActor401300Animation1F110Bank1, NULL, NULL, _gActor401300Animation1F110Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1F4C4Bank1[6] = {
#include "assets/actor_401300_animation_1F4C4_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1F4C4Bank4[81] = {
#include "assets/actor_401300_animation_1F4C4_bank4.inc"
};

static AnimationRecord _gActor401300Animation1F4C4Records[118] = {
#include "assets/actor_401300_animation_1F4C4_records.inc"
};

static u16 _gActor401300Animation1F4C4Indices[20] = {
#include "assets/actor_401300_animation_1F4C4_indices.inc"
};

static AnimationSet _gActor401300Animation1F4C4 = {
    _gActor401300Animation1F4C4Records,
    _gActor401300Animation1F4C4Indices,
    { NULL, _gActor401300Animation1F4C4Bank1, NULL, NULL, _gActor401300Animation1F4C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1F848Bank1[13] = {
#include "assets/actor_401300_animation_1F848_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1F848Bank4[58] = {
#include "assets/actor_401300_animation_1F848_bank4.inc"
};

static AnimationRecord _gActor401300Animation1F848Records[108] = {
#include "assets/actor_401300_animation_1F848_records.inc"
};

static u16 _gActor401300Animation1F848Indices[20] = {
#include "assets/actor_401300_animation_1F848_indices.inc"
};

static AnimationSet _gActor401300Animation1F848 = {
    _gActor401300Animation1F848Records,
    _gActor401300Animation1F848Indices,
    { NULL, _gActor401300Animation1F848Bank1, NULL, NULL, _gActor401300Animation1F848Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation1FF3CBank1[15] = {
#include "assets/actor_401300_animation_1FF3C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation1FF3CBank4[159] = {
#include "assets/actor_401300_animation_1FF3C_bank4.inc"
};

static AnimationRecord _gActor401300Animation1FF3CRecords[221] = {
#include "assets/actor_401300_animation_1FF3C_records.inc"
};

static u16 _gActor401300Animation1FF3CIndices[20] = {
#include "assets/actor_401300_animation_1FF3C_indices.inc"
};

static AnimationSet _gActor401300Animation1FF3C = {
    _gActor401300Animation1FF3CRecords,
    _gActor401300Animation1FF3CIndices,
    { NULL, _gActor401300Animation1FF3CBank1, NULL, NULL, _gActor401300Animation1FF3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation20D98Bank1[18] = {
#include "assets/actor_401300_animation_20D98_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation20D98Bank4[361] = {
#include "assets/actor_401300_animation_20D98_bank4.inc"
};

static AnimationRecord _gActor401300Animation20D98Records[484] = {
#include "assets/actor_401300_animation_20D98_records.inc"
};

static u16 _gActor401300Animation20D98Indices[20] = {
#include "assets/actor_401300_animation_20D98_indices.inc"
};

AnimationSet gActor401300Animation20D98 = {
    _gActor401300Animation20D98Records,
    _gActor401300Animation20D98Indices,
    { NULL, _gActor401300Animation20D98Bank1, NULL, NULL, _gActor401300Animation20D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation21618Bank1[26] = {
#include "assets/actor_401300_animation_21618_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation21618Bank4[165] = {
#include "assets/actor_401300_animation_21618_bank4.inc"
};

static AnimationRecord _gActor401300Animation21618Records[281] = {
#include "assets/actor_401300_animation_21618_records.inc"
};

static u16 _gActor401300Animation21618Indices[20] = {
#include "assets/actor_401300_animation_21618_indices.inc"
};

static AnimationSet _gActor401300Animation21618 = {
    _gActor401300Animation21618Records,
    _gActor401300Animation21618Indices,
    { NULL, _gActor401300Animation21618Bank1, NULL, NULL, _gActor401300Animation21618Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation21DE0Bank1[24] = {
#include "assets/actor_401300_animation_21DE0_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation21DE0Bank4[150] = {
#include "assets/actor_401300_animation_21DE0_bank4.inc"
};

static AnimationRecord _gActor401300Animation21DE0Records[256] = {
#include "assets/actor_401300_animation_21DE0_records.inc"
};

static u16 _gActor401300Animation21DE0Indices[20] = {
#include "assets/actor_401300_animation_21DE0_indices.inc"
};

static AnimationSet _gActor401300Animation21DE0 = {
    _gActor401300Animation21DE0Records,
    _gActor401300Animation21DE0Indices,
    { NULL, _gActor401300Animation21DE0Bank1, NULL, NULL, _gActor401300Animation21DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation2231CBank1[18] = {
#include "assets/actor_401300_animation_2231C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation2231CBank4[107] = {
#include "assets/actor_401300_animation_2231C_bank4.inc"
};

static AnimationRecord _gActor401300Animation2231CRecords[154] = {
#include "assets/actor_401300_animation_2231C_records.inc"
};

static u16 _gActor401300Animation2231CIndices[20] = {
#include "assets/actor_401300_animation_2231C_indices.inc"
};

static AnimationSet _gActor401300Animation2231C = {
    _gActor401300Animation2231CRecords,
    _gActor401300Animation2231CIndices,
    { NULL, _gActor401300Animation2231CBank1, NULL, NULL, _gActor401300Animation2231CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation22A58Bank1[22] = {
#include "assets/actor_401300_animation_22A58_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation22A58Bank4[147] = {
#include "assets/actor_401300_animation_22A58_bank4.inc"
};

static AnimationRecord _gActor401300Animation22A58Records[230] = {
#include "assets/actor_401300_animation_22A58_records.inc"
};

static u16 _gActor401300Animation22A58Indices[20] = {
#include "assets/actor_401300_animation_22A58_indices.inc"
};

static AnimationSet _gActor401300Animation22A58 = {
    _gActor401300Animation22A58Records,
    _gActor401300Animation22A58Indices,
    { NULL, _gActor401300Animation22A58Bank1, NULL, NULL, _gActor401300Animation22A58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation2303CBank1[15] = {
#include "assets/actor_401300_animation_2303C_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation2303CBank4[129] = {
#include "assets/actor_401300_animation_2303C_bank4.inc"
};

static AnimationRecord _gActor401300Animation2303CRecords[183] = {
#include "assets/actor_401300_animation_2303C_records.inc"
};

static u16 _gActor401300Animation2303CIndices[20] = {
#include "assets/actor_401300_animation_2303C_indices.inc"
};

static AnimationSet _gActor401300Animation2303C = {
    _gActor401300Animation2303CRecords,
    _gActor401300Animation2303CIndices,
    { NULL, _gActor401300Animation2303CBank1, NULL, NULL, _gActor401300Animation2303CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation23444Bank1[6] = {
#include "assets/actor_401300_animation_23444_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation23444Bank4[70] = {
#include "assets/actor_401300_animation_23444_bank4.inc"
};

static AnimationRecord _gActor401300Animation23444Records[150] = {
#include "assets/actor_401300_animation_23444_records.inc"
};

static u16 _gActor401300Animation23444Indices[20] = {
#include "assets/actor_401300_animation_23444_indices.inc"
};

static AnimationSet _gActor401300Animation23444 = {
    _gActor401300Animation23444Records,
    _gActor401300Animation23444Indices,
    { NULL, _gActor401300Animation23444Bank1, NULL, NULL, _gActor401300Animation23444Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation23A98Bank1[18] = {
#include "assets/actor_401300_animation_23A98_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation23A98Bank4[146] = {
#include "assets/actor_401300_animation_23A98_bank4.inc"
};

static AnimationRecord _gActor401300Animation23A98Records[185] = {
#include "assets/actor_401300_animation_23A98_records.inc"
};

static u16 _gActor401300Animation23A98Indices[20] = {
#include "assets/actor_401300_animation_23A98_indices.inc"
};

static AnimationSet _gActor401300Animation23A98 = {
    _gActor401300Animation23A98Records,
    _gActor401300Animation23A98Indices,
    { NULL, _gActor401300Animation23A98Bank1, NULL, NULL, _gActor401300Animation23A98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation24200Bank1[19] = {
#include "assets/actor_401300_animation_24200_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation24200Bank4[174] = {
#include "assets/actor_401300_animation_24200_bank4.inc"
};

static AnimationRecord _gActor401300Animation24200Records[223] = {
#include "assets/actor_401300_animation_24200_records.inc"
};

static u16 _gActor401300Animation24200Indices[20] = {
#include "assets/actor_401300_animation_24200_indices.inc"
};

static AnimationSet _gActor401300Animation24200 = {
    _gActor401300Animation24200Records,
    _gActor401300Animation24200Indices,
    { NULL, _gActor401300Animation24200Bank1, NULL, NULL, _gActor401300Animation24200Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation24A48Bank1[21] = {
#include "assets/actor_401300_animation_24A48_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation24A48Bank4[193] = {
#include "assets/actor_401300_animation_24A48_bank4.inc"
};

static AnimationRecord _gActor401300Animation24A48Records[254] = {
#include "assets/actor_401300_animation_24A48_records.inc"
};

static u16 _gActor401300Animation24A48Indices[20] = {
#include "assets/actor_401300_animation_24A48_indices.inc"
};

static AnimationSet _gActor401300Animation24A48 = {
    _gActor401300Animation24A48Records,
    _gActor401300Animation24A48Indices,
    { NULL, _gActor401300Animation24A48Bank1, NULL, NULL, _gActor401300Animation24A48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation251E4Bank1[20] = {
#include "assets/actor_401300_animation_251E4_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation251E4Bank4[172] = {
#include "assets/actor_401300_animation_251E4_bank4.inc"
};

static AnimationRecord _gActor401300Animation251E4Records[235] = {
#include "assets/actor_401300_animation_251E4_records.inc"
};

static u16 _gActor401300Animation251E4Indices[20] = {
#include "assets/actor_401300_animation_251E4_indices.inc"
};

static AnimationSet _gActor401300Animation251E4 = {
    _gActor401300Animation251E4Records,
    _gActor401300Animation251E4Indices,
    { NULL, _gActor401300Animation251E4Bank1, NULL, NULL, _gActor401300Animation251E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation259F0Bank1[15] = {
#include "assets/actor_401300_animation_259F0_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation259F0Bank4[206] = {
#include "assets/actor_401300_animation_259F0_bank4.inc"
};

static AnimationRecord _gActor401300Animation259F0Records[244] = {
#include "assets/actor_401300_animation_259F0_records.inc"
};

static u16 _gActor401300Animation259F0Indices[20] = {
#include "assets/actor_401300_animation_259F0_indices.inc"
};

static AnimationSet _gActor401300Animation259F0 = {
    _gActor401300Animation259F0Records,
    _gActor401300Animation259F0Indices,
    { NULL, _gActor401300Animation259F0Bank1, NULL, NULL, _gActor401300Animation259F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor401300Animation26204Bank1[14] = {
#include "assets/actor_401300_animation_26204_bank1.inc"
};

static AnimationPackedRotation _gActor401300Animation26204Bank4[207] = {
#include "assets/actor_401300_animation_26204_bank4.inc"
};

static AnimationRecord _gActor401300Animation26204Records[248] = {
#include "assets/actor_401300_animation_26204_records.inc"
};

static u16 _gActor401300Animation26204Indices[20] = {
#include "assets/actor_401300_animation_26204_indices.inc"
};

static AnimationSet _gActor401300Animation26204 = {
    _gActor401300Animation26204Records,
    _gActor401300Animation26204Indices,
    { NULL, _gActor401300Animation26204Bank1, NULL, NULL, _gActor401300Animation26204Bank4, NULL, NULL, NULL },
};

s8 D_actor_401300_8015804C[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 3, 3, 5, 0, 0, 0, 5, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 10, 10, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* D_actor_401300_80158838[46] = { NULL, NULL, &_gActor401300Animation17714, &_gActor401300Animation21618, NULL, NULL, NULL, NULL, &_gActor401300Animation1C4CC, &_gActor401300Animation1D904, &_gActor401300Animation1F110, &_gActor401300Animation1F4C4, &_gActor401300Animation1AD34, &_gActor401300Animation1A750, &_gActor401300Animation1ED6C, &_gActor401300Animation1B788, NULL, &_gActor401300Animation1DE54, &_gActor401300Animation1A170, &_gActor401300Animation1FF3C, NULL, NULL, &_gActor401300Animation1CD3C, &_gActor401300Animation1F4C4, &_gActor401300Animation1AD34, &_gActor401300Animation186D8, &_gActor401300Animation19700, &_gActor401300Animation21DE0, &_gActor401300Animation2231C, &_gActor401300Animation22A58, &_gActor401300Animation2303C, &_gActor401300Animation23444, &_gActor401300Animation23A98, &_gActor401300Animation24200, &_gActor401300Animation1D0CC, &_gActor401300Animation1F848, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

AnimationSet* D_actor_401300_801588F0[9] = {
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor401300Animation24A48,
    &_gActor401300Animation251E4,
    &_gActor401300Animation259F0,
    &_gActor401300Animation26204,
    NULL,
};

AnimationPlayRequest D_actor_401300_80158914 = { { .sets = D_actor_401300_801588F0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

SVECTOR D_actor_401300_80158928[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

TaskMessageEntry D_actor_401300_80158988[8] = {
    { 2015, _actor401300IgnoreMessage2015 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor401300PlayMessageAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor401300ApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorHeightClamp D_actor_401300_801589C8[3] = {
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

SVECTOR D_actor_401300_801589F8[2] = {
    { 937, 44, 712, 0 },
    { 29, -106, 193, 0 },
};

SVECTOR D_actor_401300_80158A08[2] = {
    { 940, -28, -709, 0 },
    { 30, 130, -190, 0 },
};

TaskDesc D_actor_401300_80158A18 = { { { TASK_BODY_TMD, 96 } }, _actor401300Task, { .model = &_gActor401300HornedStrangerBody } };

SVECTOR ActorContact_ScratchPosition = { 0 };

/// The two active height rows exclude the zero-filled trailing record.
enum { ACTOR_401300_HEIGHT_CLAMP_COUNT = 2 };

/// Root Y offset in game units, added after this actor's room height clamp.
enum { ACTOR_401300_GRID_ROOT_Y_OFFSET = 87 };

/// Parts 7 and 8 have fixed poses instead of animation tracks.
enum { ACTOR_401300_FIXED_JOINT_FIRST = 7,
       ACTOR_401300_FIXED_JOINT_END   = 9,
       ACTOR_401300_BLEND_PART_END    = 11 };

/// Loaded animation keys used by the cue and effect update.
enum {
    ACTOR_401300_ANIM_WALK          = 2,
    ACTOR_401300_ANIM_RUN           = 3,
    ACTOR_401300_ANIM_DOWN          = 9,
    ACTOR_401300_ANIM_FALL_BACK     = 11,
    ACTOR_401300_ANIM_FALL_FRONT    = 12,
    ACTOR_401300_ANIM_STRIKE_A      = 25,
    ACTOR_401300_ANIM_STRIKE_B      = 26,
    ACTOR_401300_ANIM_CHARGE_RUN    = 27,
    ACTOR_401300_ANIM_CHARGE_CLOSE  = 28,
    ACTOR_401300_ANIM_CHARGE_FINISH = 29,
    ACTOR_401300_ANIM_CHARGE_RECOIL = 30,
    ACTOR_401300_ANIM_LEAP          = 32,
    ACTOR_401300_ANIM_LAND          = 33,
    ACTOR_401300_ANIM_REFALL_FRONT  = 34
};

/// Packed sound scripts emitted by the animation cues and their water substitutions.
enum {
    ACTOR_401300_SOUND_NONE          = 0,
    ACTOR_401300_SOUND_WALK_STEP18   = 0x400D0001,
    ACTOR_401300_SOUND_WALK_STEP15   = 0x400D0002,
    ACTOR_401300_SOUND_RUN_STEP18    = 0x400D0003,
    ACTOR_401300_SOUND_RUN_STEP15    = 0x400D0004,
    ACTOR_401300_SOUND_FALL          = 0x400D0005,
    ACTOR_401300_SOUND_DOWN          = 0x400D0006,
    ACTOR_401300_SOUND_LEAP          = 0x400D000A,
    ACTOR_401300_SOUND_LAND          = 0x400D000B,
    ACTOR_401300_SOUND_STRIKE        = 0x400D000C,
    ACTOR_401300_SOUND_CHARGE_RECOIL = 0x400D0012,
    ACTOR_401300_SOUND_WATER_BODY    = 0x551D0005,
    ACTOR_401300_SOUND_WATER_STEP18  = 0x551D0006,
    ACTOR_401300_SOUND_WATER_STEP15  = 0x551D0007
};

/// Pose indices, movement limits and fixed-joint blend values (in 1/512 units).
enum {
    ACTOR_401300_ANIM_HOLD_BACK      = 23,
    ACTOR_401300_ANIM_HOLD_FRONT     = 24,
    ACTOR_401300_SPAWN_WOUNDED       = 0x20,
    ACTOR_401300_SPAWN_STALK         = 0x10,
    ACTOR_401300_SPAWN_KIND_MASK     = 0xF0,
    ACTOR_401300_HIT_RADIUS          = 640,
    ACTOR_401300_PLAYER_REACH_RADIUS = 350,
    ACTOR_401300_RUN_TURN_LIMIT      = 48,
    ACTOR_401300_ATTACK_WAIT_TICKS   = 40,
    ACTOR_401300_PAIR_LOW_BLEND      = 64,
    ACTOR_401300_PAIR_HIGH_BLEND     = 128,
    ACTOR_401300_PAIR_SLOW_STEP      = 16,
    ACTOR_401300_PAIR_NORMAL_STEP    = 32
};

/// Grab contact-effect arguments and joint bend shared by the state handlers.
///
/// The effect key selects weapon-property row 1; bit 12 is ignored by lookup.
/// The selected weapon-puff effect reads the high half as its repeat count;
/// the stored low-half magnitude is unused by that effect.
enum {
    ACTOR_401300_GRAB_HIT_EFFECT_KEY   = 0x1001,
    ACTOR_401300_GRAB_EFFECT_MAGNITUDE = 768,
    ACTOR_401300_GRAB_EFFECT_COUNT     = 2,
    ACTOR_401300_GRAB_JOINT_BEND       = 128 // 4096 angle units per turn
};

/// Fixed-joint pose and increment of either fall, in 1/512 units.
enum { ACTOR_401300_FALL_PAIR_BLEND = 32,
       ACTOR_401300_FALL_PAIR_STEP  = 8 };

/// Horizontal notice distance of the two dormant states, in game units.
enum { ACTOR_401300_DORMANT_NOTICE_RADIUS = 3000 };

/// Recovery and recoil clips shared by hit intake and the recovery states.
enum {
    ACTOR_401300_ANIM_RISE_BACK    = 8,
    ACTOR_401300_ANIM_STAGGER_BACK = 10,
    ACTOR_401300_ANIM_FLINCH       = 13,
    ACTOR_401300_ANIM_RISE_FRONT   = 22
};

/// Grid sphere identity and radius/height offset, in game-coordinate units.
enum {
    ACTOR_401300_GRID_KEY    = WORLD_COLLISION_CONTACT_ENEMY_BODY | 13,
    ACTOR_401300_GRID_RADIUS = 350
};

static s32             _actor401300ApplyBodyPushback(Task* actor, const WorldCollisionContact* contacts, s16 contactCount);
static void            _actor401300ClampRoomHeight(const GameLocationKey* location, GfxCoord* coord);
static __inline__ s32  _actor401300HasRoomHeightClamp(const GameLocationKey* location);
static s32             _actor401300ApplyGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset);
static void            _actor401300BlendToAnimation(Task* actor);
static void            _actor401300TickBlendedAnimation(Task* actor);
static s32             _actor401300GetAnimationSoundCue(_Actor401300Work* work);
static void            _actor401300BlendFixedJoints(Task* actor, s16 blendAmount);
static __inline__ void _actor401300SpawnOffsetEffect(s32 effectId, GfxCoord* placementCoord, s32 spawnArg, s16 offsetX, s16 offsetY, s16 offsetZ);
static __inline__ void _actor401300SpawnOriginEffect(s32 effectId, GfxCoord* placementCoord, s32 spawnArg);
static __inline__ void _actor401300SpawnOffsetEffectFromId(const s32* effectId, GfxCoord* placementCoord, s32 spawnArg, s16 offsetX, s16 offsetY, s16 offsetZ);
static __inline__ void _actor401300SpawnOriginEffectFromId(const s32* effectId, GfxCoord* placementCoord, s32 spawnArg);
static __inline__ s32  _actor401300IsWithinEffectDepth(Task* actor);
static __inline__ void _actor401300ResetAnimation(Task* actor);
static __inline__ void _actor401300ResetBlendAnimation(Task* actor);
static __inline__ void _actor401300TickAnimation(Task* actor);
static void            _actor401300UpdateAnimationEffects(Task* actor);
static __inline__ void _actor401300BindLightingMatrices(Task* actor);
static __inline__ void _actor401300InitializePoseAndCombat(GfxCoord* coord, _Actor401300Work* work);
static void            _actor401300Spawn(Enemy* enemy, Task* actor);
static void            _actor401300SpawnHitEffect(Task* actor, s16 hitYaw, s32 attackKey);
static void            _actor401300TakeHit(Task* actor);
static void            _actor401300StateStatusHold(Task* actor);
static void            _actor401300StateWounded(Task* actor);
static void            _actor401300StateAlert(Task* actor);
static void            _actor401300StateChase(Task* actor);
static __inline__ s32  _actor401300Abs(s32 value);
static void            _actor401300StateWithdraw(Task* actor);
static void            _actor401300StateTurnAround(Task* actor);
static void            _actor401300StateSidestep(Task* actor);
static void            _actor401300StateGrab(Task* actor);
static void            _actor401300StateGrabPull(Task* actor);
static void            _actor401300StateGrabStrike(Task* actor);
static void            _actor401300StateFallBack(Task* actor);
static void            _actor401300StateFallFront(Task* actor);
static __inline__ void _actorRenderRescaleYawXZ(GfxCoord* coord, s32 horizontalScale, s16 verticalScale);
static void            _actor401300StateDeathBurn(Task* actor);
static void            _actor401300StateDormant(Task* actor);
static void            _actor401300StateDormantScripted(Task* actor);
static void            _actor401300StatePatrol(Task* actor);
static void            _actor401300StateSlide(Task* actor);
static void            _actor401300StateBackOff(Task* actor);
static void            _actor401300StateGrabWindup(Task* actor);
static void            _actor401300StateBendOver(Task* actor);
static void            _actor401300StateDeathBurst(Task* actor);
static void            _actor401300StateDeathBurstWalk(Task* actor);
static void            _actor401300StateStalk(Task* actor);
static void            _actor401300StateStrikeA(Task* actor);
static void            _actor401300StateStrikeB(Task* actor);
static __inline__ void _actor401300RestoreRootYawScale(Task* actor);
static __inline__ s32  _actorAngleGetLocalYaw(const GfxCoord* coord);
static __inline__ void _actorMovementStepForwardFromSave(const McSaveData* save, GfxCoord* coord, s16 stepDistance);
static void            _actor401300StateCharge(Task* actor);
static __inline__ void _actorMovementTranslateForwardNonzeroFromSave(const McSaveData* save, GfxCoord* coord, s16 stepDistance);
static void            _actor401300StateLeap(Task* actor);
static __inline__ void _actorRenderScaleMatrix(MATRIX* matrix, s16 uniformScale);
static void            _actor401300StateLeapIn(Task* actor);
static void            _actor401300StateRefallBack(Task* actor);
static void            _actor401300StateRefallFront(Task* actor);
static __inline__ s32  _actor401300IsWithinImpactDepth(Task* modelTask);
static __inline__ s32  _actorContactFirstIsPlayerBody(const WorldCollisionContact* contacts);
static __inline__ void _actor401300AlignHeldPlayerHeight(Task* actor);
static void            _actor401300Tick(Enemy* enemy, Task* actor);
static s32             _actor401300TestEffectDepth(Task* modelTask);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Applies the Horned Stranger's stage/area command and reports whether it was handled.
///
/// Requires live actor work and Enemy storage, and a readable ActorCommand through
/// dispatch. Patio command 1 selects scripted dormancy; forest/path commands 0 and
/// 11 hide or leap in. Forest command 12 is handled for every placement, but only
/// placement index 0 alerts, restores maximum HP and acquires a battle reference.
/// Other commands return 0 after copying stage, area and the low command byte.
/// The message ID and second payload are ignored; no payload pointer is retained.
static s32 _actor401300ApplyCommand(Task* task, s32 msgId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_401300_COMMAND_DORMANT = 1,
        ACTOR_401300_COMMAND_HIDE    = 0,
        ACTOR_401300_COMMAND_LEAP_IN = 11,
        ACTOR_401300_COMMAND_ALERT   = 12
    };
    _Actor401300Work* work  = task->work;
    Enemy*            enemy = task->spawnArg2.pointer;

    // Retain the three command bytes before applying the room-specific action.
    work->commandBytes[0] = command->context.loc.stage;
    work->commandBytes[1] = command->context.loc.area;
    work->commandBytes[2] = (u8)command->command;
    if (command->context.loc.stage == GAME_STAGE_ACROPOLIS && command->context.loc.area == GAME_AREA_ACROPOLIS_PATIO) {
        if (command->command == ACTOR_401300_COMMAND_DORMANT) {
            work->state = ACTOR_401300_STATE_DORMANT_SCRIPTED;
            return 1;
        }
    } else if (command->context.loc.stage == GAME_STAGE_SHELTER_NEO_ARK && command->context.loc.area == GAME_AREA_NEO_ARK_FOREST_ZONE) {
        switch (command->command) {
            case ACTOR_401300_COMMAND_HIDE:
                work->state = ACTOR_401300_STATE_HIDDEN;
                return 1;
            case ACTOR_401300_COMMAND_LEAP_IN:
                work->state     = ACTOR_401300_STATE_LEAP_IN;
                work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                return 1;
            case ACTOR_401300_COMMAND_ALERT:
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                    work->state = ACTOR_401300_STATE_ALERT;
                    enemy->hp   = D_actor_401300_80141FA0.hpMax;
                    sceneAcquireBattleRef(0);
                }
                return 1;
        }
    } else if (command->context.loc.stage == GAME_STAGE_SHELTER_NEO_ARK && command->context.loc.area == GAME_AREA_NEO_ARK_WOODLAND_PATH) {
        switch (command->command) {
            case ACTOR_401300_COMMAND_HIDE:
                work->state = ACTOR_401300_STATE_HIDDEN;
                return 1;
            case ACTOR_401300_COMMAND_LEAP_IN:
                work->state     = ACTOR_401300_STATE_LEAP_IN;
                work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                return 1;
        }
    }
    return 0;
}

#include "../../shared/player_detection_reach.inc.c"

/// Applies quarter-offset X/Z pushback from player and enemy body contacts.
///
/// contacts supplies contactCount readable records (0..12); a zero key ends the
/// scan. Queries the view-space position of model part 1 and skips this actor's
/// grid body's key. Root parent axes must match the offsets' world axes. Long
/// horizontal offsets are normalized and scaled to 320 game units before an
/// arithmetic right shift by two; Y is untouched. Returns 1 for a player/companion-body contact,
/// including zero correction, and 0 otherwise. actorsFrozen or viewReady equal
/// to 1 skips the operation. Requires an initialized scratch stack; its one
/// ActorBodyPushScratch reservation is released before the final hit read.
static s32 _actor401300ApplyBodyPushback(Task* actor, const WorldCollisionContact* contacts, s16 contactCount)
{
    enum {
        ACTOR_401300_BODY_PUSH_LIMIT = 320,
        ACTOR_401300_GRID_BODY_KEY   = WORLD_COLLISION_CONTACT_ENEMY_BODY | 13
    };
    ActorBodyPushScratch* scratch;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    actor->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    scratch                                  = SCRATCH_STACK_RESERVE_BLOCK(ActorBodyPushScratch);
    actorRenderComposeCoord(&actor->extra.tmd->coords[1]);
    scratch->position.vx = actor->extra.tmd->coords[1].workm.t[0];
    scratch->position.vy = actor->extra.tmd->coords[1].workm.t[1];
    scratch->position.vz = actor->extra.tmd->coords[1].workm.t[2];
    scratch->hit         = 0;
    // Resolve each body from the same composed part-1 origin.
    for (scratch->recordIndex = 0; scratch->recordIndex < contactCount; scratch->recordIndex++) {
        if (contacts[scratch->recordIndex].key.value == 0) {
            scratch->marks[scratch->recordIndex] = ACTOR_BODY_PUSH_MARK_END;
            break;
        }
        scratch->kind = contacts[scratch->recordIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if ((scratch->kind == WORLD_COLLISION_CONTACT_PLAYER_BODY || scratch->kind == WORLD_COLLISION_CONTACT_ENEMY_BODY) && contacts[scratch->recordIndex].key.value != ACTOR_401300_GRID_BODY_KEY) {
            if (scratch->kind == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                scratch->hit = 1;
            }
            worldCollisionCalcContactWorldOffset(&scratch->position, &contacts[scratch->recordIndex], &scratch->offset);
            scratch->offsetLength = scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz;
            scratch->offsetLength = SquareRoot0(scratch->offsetLength);
            if (scratch->offsetLength >= ACTOR_401300_BODY_PUSH_LIMIT) {
                scratch->offset.vy = 0;
                _actorMovementBuildDisplacement(&scratch->offset, ACTOR_401300_BODY_PUSH_LIMIT);
                actor->extra.tmd->coords->coord.t[0] += scratch->offset.vx >> 2;
                actor->extra.tmd->coords->coord.t[2] += scratch->offset.vz >> 2;
            } else {
                actor->extra.tmd->coords->coord.t[0] += scratch->offset.vx >> 2;
                actor->extra.tmd->coords->coord.t[2] += scratch->offset.vz >> 2;
            }
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return scratch->hit;
}

/// Clamps a root's parent-space Y to this actor's bounds for the supplied room.
///
/// Only the stage and area select a row. Rooms without a row leave Y intact;
/// this helper does not invalidate the coordinate's composition cache.
static void _actor401300ClampRoomHeight(const GameLocationKey* location, GfxCoord* coord)
{
    const ActorHeightClamp* row;
    s32                     rootY;
    s32                     minY;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_401300_HEIGHT_CLAMP_COUNT; rowIndex++) {
        row = &D_actor_401300_801589C8[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            minY  = row->minY;
            rootY = coord->coord.t[1];
            if (rootY < minY) {
                coord->coord.t[1] = minY;
            } else if (row->maxY < rootY) {
                coord->coord.t[1] = row->maxY;
            }
            return;
        }
    }
}

/// Returns 1 when this actor has height bounds for the supplied stage and area.
///
/// View and room-variant bytes do not participate in the lookup.
static __inline__ s32 _actor401300HasRoomHeightClamp(const GameLocationKey* location)
{
    const ActorHeightClamp* row;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_401300_HEIGHT_CLAMP_COUNT; rowIndex++) {
        row = &D_actor_401300_801589C8[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            return 1;
        }
    }
    return 0;
}

/// Applies this actor's capped room-grid correction and reports nonzero X/Z correction.
///
/// Contacts must contain contactCount readable elements (1..32767); current
/// callers supply all twelve grid contacts. Corrections are signed 16.16 in
/// the root's parent frame. X/Z steps are capped at 175 game units before
/// fractional components add one unit in their sign. In a height-bounded room,
/// Y is capped at 350, clamped to the room bounds, then raised by heightOffset.
/// Freeze or view-ready state returns 0 without changing the root. The caller
/// invalidates its composition cache. Scratch storage is released before return.
static s32 _actor401300ApplyGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset)
{
    enum { ACTOR_401300_GRID_Y_STEP_MAX    = 350,
           ACTOR_401300_GRID_XZ_STEP_MAX   = 175,
           ACTOR_401300_GRID_FRACTION_BITS = 16,
           ACTOR_401300_GRID_FRACTION_MASK = 0xFFFF };
    ActorContactCappedPushScratch* savedHead;
    ActorContactCappedPushScratch* scratch;
    ActorContactCappedPushScratch* reservedBlock;
    s16                            verticalStep;
    SVECTOR*                       horizontalStep;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    savedHead                                           = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    reservedBlock                                       = savedHead - 1;
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = reservedBlock;
    scratch                                             = reservedBlock;
    scratch->moved                                      = 0;
    // Apply whole-unit correction before outward rounding of fractional X/Z.
    if (worldCollisionResolvePushback(contacts, &scratch->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        scratch->step.vx = savedHead[-1].delta.fixed.vx.word >> ACTOR_401300_GRID_FRACTION_BITS;
        scratch->step.vy = scratch->delta.fixed.vy.word >> ACTOR_401300_GRID_FRACTION_BITS;
        scratch->step.vz = scratch->delta.fixed.vz.word >> ACTOR_401300_GRID_FRACTION_BITS;
        if (_actor401300HasRoomHeightClamp(&gGameSession->location.loc)) {
            verticalStep = scratch->step.vy;
            if (((verticalStep >= 0) ? verticalStep : -verticalStep) > ACTOR_401300_GRID_Y_STEP_MAX) {
                scratch->step.vy = (verticalStep <= 0) ? -ACTOR_401300_GRID_Y_STEP_MAX : ACTOR_401300_GRID_Y_STEP_MAX;
            }
        }
        coord->coord.t[1]  += scratch->step.vy;
        scratch->stepLength = scratch->step.vx * scratch->step.vx + scratch->step.vz * scratch->step.vz;
        scratch->stepLength = SquareRoot0(scratch->stepLength);
        horizontalStep      = &scratch->step;
        if (scratch->stepLength >= ACTOR_401300_GRID_XZ_STEP_MAX) {
            scratch->step.vy = 0;
            VectorNormalSS(horizontalStep, horizontalStep);
            gte_lddp(ACTOR_401300_GRID_XZ_STEP_MAX);
            gte_ldsv(horizontalStep);
            gte_gpf12();
            gte_stsv(horizontalStep);
            coord->coord.t[0] += scratch->step.vx;
            coord->coord.t[2] += scratch->step.vz;
        } else {
            coord->coord.t[0] += scratch->step.vx;
            coord->coord.t[2] += scratch->step.vz;
        }
        if (scratch->delta.fixed.vx.word & ACTOR_401300_GRID_FRACTION_MASK) {
            if (scratch->delta.fixed.vx.word > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (scratch->delta.fixed.vz.word & ACTOR_401300_GRID_FRACTION_MASK) {
            if (scratch->delta.fixed.vz.word > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    // Room height bounds precede the caller-supplied root-height offset.
    if (_actor401300HasRoomHeightClamp(&gGameSession->location.loc)) {
        _actor401300ClampRoomHeight(&gGameSession->location.loc, coord);
        coord->coord.t[1] += heightOffset;
    }
    if (scratch->delta.fixed.vx.word != 0 || scratch->delta.fixed.vz.word != 0) {
        scratch->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return scratch->moved;
}

#include "../../shared/player_detection_sight.inc.c"

/// Starts a transition to a different requested primary animation.
///
/// The live actor must own its bound nineteen-slot rig and loaded animation
/// bank. Both animation ids must index the 45-by-45 transition table; entries
/// are whole normal-rate blend frames. Slots 1..6 and 9..18 are driven, with
/// 7/8 left for the fixed joint pair. A repeated animation only leaves its
/// existing playback in place.
static void _actor401300BlendToAnimation(Task* actor)
{
    s32               slotIndex;
    _Actor401300Work* work;

    work = actor->work;

    if (work->appliedAnim != work->animId) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            work->rig.slots[slotIndex].rate = work->animRate;
            if (slotIndex < ACTOR_401300_FIXED_JOINT_FIRST) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0,
                                           D_actor_401300_8015804C[work->appliedAnim][work->animId]);
            } else if (slotIndex >= ACTOR_401300_FIXED_JOINT_END) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0,
                                           D_actor_401300_8015804C[work->appliedAnim][work->animId]);
            }
        }
        work->appliedAnim = work->animId;
    }
}

/// Advances both rigs and mixes their rotations on parts 1..6 and 9/10.
///
/// The actor owns two bound nineteen-slot rigs and their loaded clips.
/// Primary translation is retained. blendWeight is the primary rotation's
/// share in 1/4096 units, normally 0..4096; the other share is complementary.
/// Primary playback uses animRate minus three sixteenths per tick; secondary
/// playback uses blendRate. Parts 11..18 use primary playback alone.
static void _actor401300TickBlendedAnimation(Task* actor)
{
    AnimationPose     primaryPose;
    AnimationPose     blendPose;
    s16               primaryWeight;
    s16               slotIndex;
    _Actor401300Work* work;

/// Ticks both rigs for one part and applies primary translation with mixed rotation.
///
/// All arguments must be side-effect-free; work, slot and pose arguments occur
/// repeatedly. Both writable poses must stay live through the three calls.
/// primaryShare is a rotation weight in 1/4096 units. Captures no locals.
#define ACTOR_401300_TICK_BLENDED_PART(actorWork, slot, primary, secondary, primaryShare)                                                    \
    do {                                                                                                                                     \
        animationTickSlotPose(&(actorWork)->rig.anim, (slot), (primary), NULL);                                                              \
        animationTickSlotPose(&(actorWork)->blend.anim, (slot), (secondary), NULL);                                                          \
        animationApplyPoseWithBlendedRotation(&(actorWork)->rig.anim, (slot), (primary), (secondary), (primaryShare), ONE - (primaryShare)); \
    } while (0)

    work          = actor->work;
    primaryWeight = work->blendWeight;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex < ACTOR_401300_BLEND_PART_END) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = (work->animRate - 3);
            if (slotIndex >= ACTOR_401300_FIXED_JOINT_FIRST) {
                if (slotIndex < ACTOR_401300_FIXED_JOINT_END) {
                    continue;
                }
            }
            ACTOR_401300_TICK_BLENDED_PART(work, slotIndex, &primaryPose, &blendPose, primaryWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
#undef ACTOR_401300_TICK_BLENDED_PART
}

/// Selects the packed sound-script request at the primary animation's cue record.
///
/// Returns 0 without a cue. Cue records use the low ten bits of slot 1's
/// record index. A held cue normally emits once; moving off a recognized
/// cue clears the latch, while animations without cue rules retain it.
/// Animation 28 retains its different repeat-gate record.
static s32 _actor401300GetAnimationSoundCue(_Actor401300Work* work)
{

/// Latches the current cue record and returns a sound when the repeat gate differs.
///
/// Use only in a braced cue branch of this function. Captures writable work
/// and may return from the enclosing function. repeatGate is evaluated once;
/// cueSound is evaluated only on a new cue. Both must be side-effect-free.
#define ACTOR_401300_SELECT_SOUND_CUE(repeatGate, cueSound)                                                      \
    if (work->lastCueFrame != (repeatGate)) {                                                                    \
        work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK; \
        return (cueSound);                                                                                       \
    }                                                                                                            \
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK

    switch ((s16)(work->animId - ACTOR_401300_ANIM_WALK)) {
        case ACTOR_401300_ANIM_RUN - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xF) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_RUN_STEP15);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x15) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_RUN_STEP18);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_WALK - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x11) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP15);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1A) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP18);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_STRIKE_A - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xB) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_STRIKE);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xE) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP18);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_STRIKE_B - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xB) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_STRIKE);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_FALL_FRONT - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x7) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_FALL);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_REFALL_FRONT - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x4) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_FALL);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_FALL_BACK - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x5) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_FALL);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_DOWN - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x7) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_DOWN);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_LEAP - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xB) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_LEAP);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_LAND - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xD) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_LAND);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_CHARGE_RUN - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xE) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_RUN_STEP15);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x14) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_RUN_STEP18);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_CHARGE_CLOSE - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x12) {
                // The retained repeat gate compares record 14 while the cue is record 18.
                ACTOR_401300_SELECT_SOUND_CUE(0xE, ACTOR_401300_SOUND_RUN_STEP15);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_CHARGE_FINISH - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xD) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP15);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xF) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP18);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x11) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP15);
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x14) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_WALK_STEP18);
                break;
            }
            work->lastCueFrame = 0;
            break;
        case ACTOR_401300_ANIM_CHARGE_RECOIL - ACTOR_401300_ANIM_WALK:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x9) {
                ACTOR_401300_SELECT_SOUND_CUE((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK), ACTOR_401300_SOUND_CHARGE_RECOIL);
                break;
            }
            work->lastCueFrame = 0;
            break;
    }
    return ACTOR_401300_SOUND_NONE;
#undef ACTOR_401300_SELECT_SOUND_CUE
}

/// Interpolates the XYZ angles of two poses with a blend weight of 512 for pose 1.
///
/// poses supplies two readable rotations; destination may alias either source.
/// Angles use 4096 units per turn; amount 0 selects pose 0 and 512 selects pose 1.
/// Uses signed s32 products and division toward zero, then narrows each result to
/// s16; destination must be writable and extrapolation requires products to fit s32.
/// Does not wrap angle differences across a turn boundary. The destination pad
/// is untouched; no pointer is retained.
static inline void _actor401300InterpolateJointRotation(const SVECTOR* poses, s16 amount, SVECTOR* destination)
{
    enum { ACTOR_401300_JOINT_PAIR_WEIGHT_ONE = 512 };
    destination->vx = poses[1].vx + ((poses[0].vx - poses[1].vx) * (ACTOR_401300_JOINT_PAIR_WEIGHT_ONE - amount)) / ACTOR_401300_JOINT_PAIR_WEIGHT_ONE;
    destination->vy = poses[1].vy + ((poses[0].vy - poses[1].vy) * (ACTOR_401300_JOINT_PAIR_WEIGHT_ONE - amount)) / ACTOR_401300_JOINT_PAIR_WEIGHT_ONE;
    destination->vz = poses[1].vz + ((poses[0].vz - poses[1].vz) * (ACTOR_401300_JOINT_PAIR_WEIGHT_ONE - amount)) / ACTOR_401300_JOINT_PAIR_WEIGHT_ONE;
}

/// Interpolates the two fixed joint rotations on model parts 7 and 8.
///
/// blendAmount normally runs from 0 (first pose) to 512 (second pose),
/// with linear extrapolation outside that range. Rotation components use
/// 4096 units per turn. The actor's model coordinates must be live; one
/// SVECTOR of initialized scratch-stack space is borrowed and released.
static void _actor401300BlendFixedJoints(Task* actor, s16 blendAmount)
{
    SVECTOR* rotation;

    rotation = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    _actor401300InterpolateJointRotation(D_actor_401300_801589F8, blendAmount, rotation);
    RotMatrix_gte(rotation, &actor->extra.tmd->coords[7].coord);
    _actor401300InterpolateJointRotation(D_actor_401300_80158A08, blendAmount, rotation);
    RotMatrix_gte(rotation, &actor->extra.tmd->coords[8].coord);
    actor->extra.tmd->coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    actor->extra.tmd->coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns an effect at a signed local offset from a live placement coordinate.
///
/// effectId is a packed effect bank/id; spawnArg is the effect task's packed
/// 32-bit payload. Offsets use game coordinate units in placementCoord's
/// local frame. Use handlers that consume the offset at spawn: its temporary
/// storage ends with this call. Spawn rejection is intentionally ignored.
static __inline__ void _actor401300SpawnOffsetEffect(s32 effectId, GfxCoord* placementCoord, s32 spawnArg, s16 offsetX, s16 offsetY, s16 offsetZ)
{
    SVECTOR localOffset;

    localOffset.vx = offsetX;
    localOffset.vy = offsetY;
    localOffset.vz = offsetZ;
    effectSpawn(effectId, placementCoord, spawnArg, &localOffset);
}

/// Spawns an effect at the origin of a live placement coordinate.
///
/// effectId and spawnArg are the packed effect selector and task payload.
/// The zero offset is temporary; the selected handler must not follow its
/// retained address after spawn. Spawn rejection is intentionally ignored.
static __inline__ void _actor401300SpawnOriginEffect(s32 effectId, GfxCoord* placementCoord, s32 spawnArg)
{
    SVECTOR localOffset;

    localOffset.vx = localOffset.vy = localOffset.vz = 0;
    effectSpawn(effectId, placementCoord, spawnArg, &localOffset);
}

/// Spawns a room-selected effect at a signed local placement offset.
///
/// effectId borrows a live packed selector and is read at the spawn call;
/// zero selects no effect. spawnArg is the task's packed 32-bit payload,
/// and offsets use placementCoord's local game units. The current ripple
/// and spray handlers use the copied offset, not its temporary address.
/// Spawn rejection is intentionally ignored.
static __inline__ void _actor401300SpawnOffsetEffectFromId(const s32* effectId, GfxCoord* placementCoord, s32 spawnArg, s16 offsetX, s16 offsetY, s16 offsetZ)
{
    SVECTOR localOffset;

    localOffset.vx = offsetX;
    localOffset.vy = offsetY;
    localOffset.vz = offsetZ;
    effectSpawn(*effectId, placementCoord, spawnArg, &localOffset);
}

/// Spawns a room-selected effect at a live placement coordinate's origin.
///
/// effectId borrows a live packed selector and is read at spawn; zero selects
/// no effect. spawnArg is the task's packed 32-bit payload. The current spray
/// handler uses the copied zero offset, not its temporary address. Spawn
/// rejection is intentionally ignored.
static __inline__ void _actor401300SpawnOriginEffectFromId(const s32* effectId, GfxCoord* placementCoord, s32 spawnArg)
{
    SVECTOR localOffset;

    localOffset.vx = localOffset.vy = localOffset.vz = 0;
    effectSpawn(*effectId, placementCoord, spawnArg, &localOffset);
}

/// Returns 1 when model part 1 lies within the effect depth interval [-299, 2300).
///
/// Depth is in view-space game units. The live coordinate chain must be
/// acyclic; each transformed parent step narrows to signed halfwords.
/// A chain that never reaches the view leaves the zero point and passes.
/// This checks depth only, not screen bounds or occlusion.
static __inline__ s32 _actor401300IsWithinEffectDepth(Task* actor)
{
    enum { ACTOR_401300_EFFECT_DEPTH_NEAR = -299,
           ACTOR_401300_EFFECT_DEPTH_FAR  = 2300 };
    SVECTOR   viewPoint;
    SVECTOR   localPoint;
    VECTOR    transformedPoint;
    s32       gteFlags;
    SVECTOR*  localPointPtr;
    GfxCoord* view;
    VECTOR*   transformedPointPtr;
    s32*      gteFlagsPtr;
    SVECTOR*  viewPointPtr;
    GfxCoord* coord;
    s32       withinDepth;

    memset(&viewPoint, 0, sizeof(viewPoint));
    coord               = &actor->extra.tmd->coords[1];
    localPointPtr       = &localPoint;
    viewPointPtr        = &viewPoint;
    view                = &gGfxViewCoord;
    transformedPointPtr = &transformedPoint;
    gteFlagsPtr         = &gteFlags;
    localPoint.vx       = viewPointPtr->vx;
    localPoint.vy       = viewPointPtr->vy;
    localPoint.vz       = viewPointPtr->vz;
    for (;;) {
        if (coord->parent != NULL) {
            if (coord != view) {
                gte_SetTransMatrix(&coord->coord);
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(localPointPtr);
                gte_rtv0tr();
                gte_stlvnl(transformedPointPtr);
                gte_stflg(gteFlagsPtr);
                localPoint.vx = transformedPoint.vx;
                localPoint.vy = transformedPoint.vy;
                localPoint.vz = transformedPoint.vz;
                coord         = coord->parent;
                continue;
            }
            viewPointPtr->vx = localPoint.vx;
            viewPointPtr->vy = localPoint.vy;
            viewPointPtr->vz = localPoint.vz;
        }
        break;
    }
    withinDepth = (u16)(viewPoint.vz - ACTOR_401300_EFFECT_DEPTH_NEAR) < ACTOR_401300_EFFECT_DEPTH_FAR - ACTOR_401300_EFFECT_DEPTH_NEAR;
    if (withinDepth != 0) {
        withinDepth = 1;
    } else {
        withinDepth = 0;
    }
    return withinDepth;
}

/// Restarts the requested primary clip on its model-part tracks.
///
/// The actor must own its bound nineteen-slot rig and loaded requested clip.
/// Slots 1..6 map to equal-numbered tracks and parts; slots 9..18 map to
/// tracks two indices lower. Parts 7/8 are the independently posed joint
/// pair. Remapped resets seed normal rate; the following tick applies animRate.
static __inline__ void _actor401300ResetAnimation(Task* actor)
{
    s32               slotIndex;
    _Actor401300Work* work;

    work = actor->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        if (slotIndex < ACTOR_401300_FIXED_JOINT_FIRST) {
            animationResetRemappedSlot(&work->rig.anim, slotIndex, work->animId, slotIndex, slotIndex);
        } else if (slotIndex >= ACTOR_401300_FIXED_JOINT_END) {
            animationResetRemappedSlot(&work->rig.anim, slotIndex, work->animId, slotIndex - 2, slotIndex);
        }
    }
    work->appliedAnim = work->animId;
}

/// Restarts the secondary clip with a three-frame rate and equal pose weights.
///
/// The actor must own both bound nineteen-slot rigs and a loaded blend clip.
/// Animated slots use the same track remapping as the primary rig. The
/// stored blend rate is 48 sixteenths per tick and weight 2048/4096. Reset
/// writes that rate to the primary slots; secondary remapped resets seed
/// normal rate until the next blended tick assigns blendRate.
static __inline__ void _actor401300ResetBlendAnimation(Task* actor)
{
    s32               slotIndex;
    _Actor401300Work* work;

    work              = actor->work;
    work->blendRate   = 3 * ANIMATION_RATE_ONE;
    work->blendWeight = ONE / 2;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->blendRate;
        if (slotIndex < ACTOR_401300_FIXED_JOINT_FIRST) {
            animationResetRemappedSlot(&work->blend.anim, slotIndex, work->blendAnimId, slotIndex, slotIndex);
        } else if (slotIndex >= ACTOR_401300_FIXED_JOINT_END) {
            animationResetRemappedSlot(&work->blend.anim, slotIndex, work->blendAnimId, slotIndex - 2, slotIndex);
        }
    }
}

/// Advances the primary clip on model parts 1..6 and 9..18.
///
/// The actor must own its bound nineteen-slot rig and loaded clip. animRate
/// is narrowed to each slot's signed-byte rate in sixteenths per tick.
/// Negative rates play backward. Fixed parts 7/8 receive a rate but no tick.
static __inline__ void _actor401300TickAnimation(Task* actor)
{
    s32               slotIndex;
    _Actor401300Work* work;

    work = actor->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        if (slotIndex < ACTOR_401300_FIXED_JOINT_FIRST) {
            animationTickSlot(&work->rig.anim, slotIndex);
        } else if (slotIndex >= ACTOR_401300_FIXED_JOINT_END) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Updates this actor's animation requests, joint poses, sound cues and effects.
///
/// The task owns a live work block, enemy spawn payload and model with both
/// rigs bound to loaded clips. Each call advances playback, even if a state
/// calls it more than once in a frame. Look yaws use 4096 units per turn;
/// the fixed-pair pose uses 512 per full blend. Water effects are depth-gated;
/// room-enabled dust and the spatialized sound request are not.
static void _actor401300UpdateAnimationEffects(Task* actor)
{
    // Ripple half-side 64; spray size 384, period 2, speed 32, upward burst.
    // Dust sizes 704/752/2048, periods 2/2/4, with child puffs enabled.
    enum { ACTOR_401300_RIPPLE_HALF_SIDE      = 64,
           ACTOR_401300_SPRAY_SPAWN_ARG       = 0x1202180,
           ACTOR_401300_DUST_STEP18_SPAWN_ARG = 0x800022C0,
           ACTOR_401300_DUST_STEP15_SPAWN_ARG = 0x800022F0,
           ACTOR_401300_DUST_BODY_SPAWN_ARG   = 0x80004800,
           ACTOR_401300_LOOK_STEP             = 0x100 };
    s32               soundRequest;
    s32               soundCue;
    s16               lookYaw;
    s32               withinEffectDepth;
    _Actor401300Work* work;
    Enemy*            enemy;

    /* Set here so CSE keeps `withinEffectDepth` distinct from the helper's result. */
    withinEffectDepth = 0;
    work              = actor->work;
    enemy             = actor->spawnArg2.pointer;
    // Apply requests before advancing either rig.
    if (work->animRequest == ACTOR_401300_ANIM_REQUEST_BLEND) {
        _actor401300BlendToAnimation(actor);
        work->animRequest  = ACTOR_401300_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (work->animRequest == ACTOR_401300_ANIM_REQUEST_RESET) {
        _actor401300ResetAnimation(actor);
        work->animRequest  = ACTOR_401300_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_401300_ANIM_REQUEST_RESET) {
        _actor401300ResetBlendAnimation(actor);
        work->blendRequest = ACTOR_401300_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    if (work->blendActive == 0) {
        _actor401300TickAnimation(actor);
    } else {
        _actor401300TickBlendedAnimation(actor);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->blendActive = 0;
        }
    }
    // Ease the look direction, then pose the independently driven joint pair.
    if (work->lookYawTarget > work->lookYaw) {
        if (work->lookYawTarget - work->lookYaw > ACTOR_401300_LOOK_STEP) {
            work->lookYaw += ACTOR_401300_LOOK_STEP;
        } else {
            work->lookYaw = work->lookYawTarget;
        }
    } else if (-(work->lookYawTarget - work->lookYaw) > ACTOR_401300_LOOK_STEP) {
        work->lookYaw -= ACTOR_401300_LOOK_STEP;
    } else {
        work->lookYaw = work->lookYawTarget;
    }
    if (work->lookYaw != 0) {
        lookYaw = work->lookYaw;
        if (work->lookYaw > 0x400) {
            lookYaw = 0x400;
        }
        if (work->lookYaw < -0x400) {
            lookYaw = -0x400;
        }
        _actorRenderYawJointInWorld(&actor->extra.tmd->coords[5], (lookYaw * 2) / 3);
        _actorRenderYawJointInWorld(&actor->extra.tmd->coords[2], lookYaw / 2);
        actor->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        actor->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actor->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->jointPairBlend != work->jointPairTarget) {
        if (work->jointPairTarget < work->jointPairBlend) {
            work->jointPairBlend -= work->jointPairStep;
            if (work->jointPairBlend < work->jointPairTarget) {
                work->jointPairBlend = work->jointPairTarget;
            }
        } else {
            work->jointPairBlend += work->jointPairStep;
            if (work->jointPairTarget < work->jointPairBlend) {
                work->jointPairBlend = work->jointPairTarget;
            }
        }
    }
    _actor401300BlendFixedJoints(actor, work->jointPairBlend);
    soundCue          = _actor401300GetAnimationSoundCue(work);
    withinEffectDepth = _actor401300IsWithinEffectDepth(actor);
    // Water placement follows depth and room cues; dust uses the room effect mode.
    if (withinEffectDepth == 1) {
        if (work->animId == ACTOR_401300_ANIM_WALK) {
            if (gDisplayState.animFrame % 6 == 0) {
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterRippleId, &actor->extra.tmd->coords[18], ACTOR_401300_RIPPLE_HALF_SIDE, 0, 0x1C2, -100);
            }
            if (gDisplayState.animFrame % 6 == 3) {
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterRippleId, &actor->extra.tmd->coords[15], ACTOR_401300_RIPPLE_HALF_SIDE, 0, 0x1C2, -100);
            }
        } else if (work->animId == ACTOR_401300_ANIM_RUN) {
            if ((gDisplayState.animFrame & 1) == withinEffectDepth) {
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[18], ACTOR_401300_SPRAY_SPAWN_ARG, 0, 0x1C2, -100);
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterRippleId, &actor->extra.tmd->coords[18], ACTOR_401300_RIPPLE_HALF_SIDE, 0, 0x1C2, -100);
            }
            if (!(gDisplayState.animFrame & 1)) {
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[15], ACTOR_401300_SPRAY_SPAWN_ARG, 0, 0x1C2, -100);
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterRippleId, &actor->extra.tmd->coords[15], ACTOR_401300_RIPPLE_HALF_SIDE, 0, 0x1C2, -100);
            }
        } else if (work->animId == ACTOR_401300_ANIM_DOWN || work->animId == ACTOR_401300_ANIM_STRIKE_A || work->animId == ACTOR_401300_ANIM_STRIKE_B) {
            if (gDisplayState.animFrame % 5 == 0) {
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterRippleId, &actor->extra.tmd->coords[18], ACTOR_401300_RIPPLE_HALF_SIDE, 0, 0x1C2, -100);
            }
            if (gDisplayState.animFrame % 6 == 3) {
                _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterRippleId, &actor->extra.tmd->coords[15], ACTOR_401300_RIPPLE_HALF_SIDE, 0, 0x1C2, -100);
            }
        }
        if (soundCue != 0 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 0)) {
            switch (soundCue) {
                case ACTOR_401300_SOUND_WALK_STEP18:
                case ACTOR_401300_SOUND_RUN_STEP18:
                    _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[18], ACTOR_401300_SPRAY_SPAWN_ARG, 0, 0x1C2, -100);
                    soundCue = ACTOR_401300_SOUND_WATER_STEP18;
                    break;
                case ACTOR_401300_SOUND_WALK_STEP15:
                case ACTOR_401300_SOUND_RUN_STEP15:
                    _actor401300SpawnOffsetEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[15], ACTOR_401300_SPRAY_SPAWN_ARG, 0, 0x1C2, -100);
                    soundCue = ACTOR_401300_SOUND_WATER_STEP15;
                    break;
                case ACTOR_401300_SOUND_FALL:
                case ACTOR_401300_SOUND_LAND:
                    _actor401300SpawnOriginEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[1], ACTOR_401300_SPRAY_SPAWN_ARG);
                    _actor401300SpawnOriginEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[1], ACTOR_401300_SPRAY_SPAWN_ARG);
                    _actor401300SpawnOriginEffectFromId(&gRoomEffectWaterSprayId, &actor->extra.tmd->coords[1], ACTOR_401300_SPRAY_SPAWN_ARG);
                    soundCue = ACTOR_401300_SOUND_WATER_BODY;
                    break;
            }
        }
    }
    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
        switch (soundCue) {
            case ACTOR_401300_SOUND_WALK_STEP18:
            case ACTOR_401300_SOUND_RUN_STEP18:
                _actor401300SpawnOffsetEffect(EFFECT_DUST_PUFF, &actor->extra.tmd->coords[18], ACTOR_401300_DUST_STEP18_SPAWN_ARG, 0, 0x15E, -100);
                break;
            case ACTOR_401300_SOUND_WALK_STEP15:
            case ACTOR_401300_SOUND_RUN_STEP15:
                _actor401300SpawnOffsetEffect(EFFECT_DUST_PUFF, &actor->extra.tmd->coords[15], ACTOR_401300_DUST_STEP15_SPAWN_ARG, 0, 0x15E, -100);
                break;
            case ACTOR_401300_SOUND_FALL:
            case ACTOR_401300_SOUND_LAND:
                _actor401300SpawnOriginEffect(EFFECT_DUST_PUFF, &actor->extra.tmd->coords[1], ACTOR_401300_DUST_BODY_SPAWN_ARG);
                _actor401300SpawnOriginEffect(EFFECT_DUST_PUFF, &actor->extra.tmd->coords[1], ACTOR_401300_DUST_BODY_SPAWN_ARG);
                break;
        }
    }
    // Add this enemy placement's sound-script variant and spatial offsets.
    if (soundCue != ACTOR_401300_SOUND_NONE) {
        soundRequest = soundCue | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        sndEvtRequestScriptStart(soundRequest, (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
    }
}

/// Binds the live model's borrowed light and colour matrices to its actor work.
///
/// The writable work block must outlive the model's use of both matrix pointers.
/// This sets their addresses without initializing their coefficients.
static __inline__ void _actor401300BindLightingMatrices(Task* actor)
{
    _Actor401300Work* work;
    TmdObject*        model;

    work            = actor->work;
    model           = actor->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Initializes the Horned Stranger's root basis and combat pose defaults.
///
/// Requires a writable root coordinate and allocated actor work with bound rigs.
/// Extracts yaw in 4096 units per turn, discards pitch/roll and applies the Q12
/// root scale; translation is preserved and composition becomes dirty. Resets
/// the position-history cursor and pending death flag, copies the player's
/// animation request and seeds the fixed-joint interpolation in 1/512 units.
/// Borrows one ActorScaleRotScratch plus the nested yaw workspace until return.
static __inline__ void _actor401300InitializePoseAndCombat(GfxCoord* coord, _Actor401300Work* work)
{
    enum {
        ACTOR_401300_INITIAL_JOINT_PAIR_BLEND  = 0x100,
        ACTOR_401300_INITIAL_JOINT_PAIR_TARGET = 0x170,
        ACTOR_401300_INITIAL_JOINT_PAIR_STEP   = 0x20
    };
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);
    // Replace the old basis with the spawn heading at the model's root scale.
    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vz = ACTOR_401300_ROOT_SCALE;
    yawScratch->scale.vy = ACTOR_401300_ROOT_SCALE;
    yawScratch->scale.vx = ACTOR_401300_ROOT_SCALE;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);
    _actorRenderCopyRotation(coord, yawScratch->rotation.m);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    // Seed the held-player request and the independently posed joint pair.
    work->bodyPosCursor   = 0;
    work->deathPending    = 0;
    work->playerAnim      = D_actor_401300_80158914;
    work->jointPairBlend  = ACTOR_401300_INITIAL_JOINT_PAIR_BLEND;
    work->jointPairTarget = ACTOR_401300_INITIAL_JOINT_PAIR_TARGET;
    work->jointPairStep   = ACTOR_401300_INITIAL_JOINT_PAIR_STEP;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Seeds two horizontal patrol endpoints from the placed facing, in game units.
///
/// actor and work must be live; direction must point to the writable forward
/// SVECTOR. Arguments occur repeatedly and must be side-effect-free. The block
/// writes temporary XYZ, patrol endpoints and their selector; it retains no pointer.
#define ACTOR_401300_SEED_PATROL_POINTS(actor, work, forward, direction)                   \
    {                                                                                      \
        enum { ACTOR_401300_PATROL_STEP = 2000 };                                          \
        (work)->patrolTarget      = 0;                                                     \
        (work)->patrolPoints[0].x = (actor)->extra.tmd->coords->coord.t[0];                \
        (work)->patrolPoints[0].z = (actor)->extra.tmd->coords->coord.t[2];                \
        gfxReadMatrixZAxis(&(actor)->extra.tmd->coords->coord, (direction));               \
        (forward).vy = 0;                                                                  \
        VectorNormalSS((direction), (direction));                                          \
        gte_lddp(ACTOR_401300_PATROL_STEP);                                                \
        gte_ldsv((direction));                                                             \
        gte_gpf12();                                                                       \
        gte_stsv((direction));                                                             \
        (work)->patrolPoints[1].x = (actor)->extra.tmd->coords->coord.t[0] + (forward).vx; \
        (work)->patrolPoints[1].z = (actor)->extra.tmd->coords->coord.t[2] + (forward).vz; \
    }

/// Allocates and initializes the Horned Stranger's work, collision bodies and behavior.
///
/// Requires a live Enemy and a loaded nineteen-part model. Allocation failure
/// destroys the enemy task. Otherwise the task owns the zeroed work until exit;
/// its collision/contact and matrix pointers remain borrowed from that work.
/// The high spawn word selects reusable, dormant, wounded or patrol entry; the
/// low nibble selects one of three tunings. Patrol points are 2000 horizontal
/// game units apart along the placed facing. Success advances Task::state.
static void _actor401300Spawn(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_401300_ATTACK_RADIUS           = 512,
        ACTOR_401300_SPAWN_DORMANT           = 4,
        ACTOR_401300_WOUNDED_INITIAL_HP      = 80,
        ACTOR_401300_VARIANT_MASK            = 15,
        ACTOR_401300_HIT_EFFECT_ARGUMENT_LOW = 768,
        ACTOR_401300_HIT_EFFECT_COUNT        = 2
    };
    SVECTOR             directionScratch;
    VECTOR              worldPosition;
    SVECTOR*            direction;
    TmdObject*          model;
    GfxCoord*           rootCoord;
    _Actor401300Work*   work;
    WorldCollisionBody* hitBody;
    WorldCollisionBody* attackBody;

    // Allocate task-owned work before linking any collision or target records.
    rootCoord   = actor->extra.tmd->coords;
    model       = actor->extra.tmd;
    work        = memCalloc(sizeof(_Actor401300Work), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    if ((actor->spawnArg1.value >> 16) != ACTOR_401300_SPAWN_REUSABLE) {
        (sceneAcquireBattleRef)(0);
    }
    actor->exitCallback = _actor401300ReleaseResources;
    _actor401300BindLightingMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_401300_80141FA0.hpMax;
    enemy->param                  = &D_actor_401300_80141FA0;
    enemy->recs                   = work->hitContacts;
    animationBindContext(&work->rig.anim, D_actor_401300_80158838, model,
                         work->rig.poses, work->rig.slots);
    animationBindContext(&work->blend.anim, D_actor_401300_80158838, model,
                         work->blend.poses, work->blend.slots);
    work->animRequest   = ACTOR_401300_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = ACTOR_401300_ANIM_WALK;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->chaseRate     = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    if ((s16)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) == 1) {
        work->chaseRate++;
    } else {
        work->chaseRate--;
    }
    _actor401300UpdateAnimationEffects(actor);

    // The grid sphere follows the root; the hit and attack spheres follow model parts.
    work->gridCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->gridCoord.coord);
    work->gridCoord.coord.t[0]   = actor->extra.tmd->coords->coord.t[0];
    work->gridCoord.coord.t[1]   = actor->extra.tmd->coords->coord.t[1] - ACTOR_401300_GRID_RADIUS;
    work->gridCoord.coord.t[2]   = actor->extra.tmd->coords->coord.t[2];
    work->gridCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->gridCoord);

    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = &work->gridCoord;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = ACTOR_401300_GRID_KEY;
    work->gridBody.radius           = ACTOR_401300_GRID_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->hitCooldown     = 0;
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    hitBody                   = &work->hitBody;
    hitBody->context.contacts = work->hitContacts;
    hitBody->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    hitBody->coord            = &gGfxViewCoord;
    hitBody->pos.vx           = 0;
    hitBody->pos.vy           = 0;
    hitBody->pos.vz           = 0;
    hitBody->radius           = ACTOR_401300_HIT_RADIUS;
    hitBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    directionScratch.vx          = 0;
    directionScratch.vy          = 0;
    directionScratch.vz          = 0;
    attackBody                   = &work->attackBody;
    attackBody->coord            = &actor->extra.tmd->coords[3];
    attackBody->context.contacts = work->attackContacts;
    direction                    = &directionScratch;
    attackBody->pos.vx           = direction->vx;
    attackBody->pos.vy           = direction->vy;
    attackBody->pos.vz           = direction->vz;
    attackBody->radius           = ACTOR_401300_ATTACK_RADIUS;
    attackBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);

    // Seed a horizontal patrol segment from the placed facing.
    ACTOR_401300_SEED_PATROL_POINTS(actor, work, directionScratch, direction);

    actor->msgTable         = D_actor_401300_80158988;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = ACTOR_401300_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = ACTOR_401300_HIT_EFFECT_COUNT;
    // The high spawn word selects initial behavior; the low nibble selects tuning.
    switch ((u8)(actor->spawnArg1.value >> 16)) {
        case ACTOR_401300_SPAWN_REUSABLE:
            work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
            work->state     = ACTOR_401300_STATE_HIDDEN;
            enemy->hp       = ACTOR_401300_HP_AVAILABLE;
            break;
        case ACTOR_401300_SPAWN_DORMANT:
            work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
            work->state     = ACTOR_401300_STATE_DORMANT;
            break;
        case ACTOR_401300_SPAWN_WOUNDED:
            work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
            work->state     = ACTOR_401300_STATE_WOUNDED;
            enemy->hp       = ACTOR_401300_WOUNDED_INITIAL_HP;
            break;
        default:
            work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
            work->state     = ACTOR_401300_STATE_PATROL;
            tmdAllocPrimitiveBuffer(model);
            break;
    }
    switch (actor->spawnArg1.value & ACTOR_401300_VARIANT_MASK) {
        case 2:
            work->downFramesBase = D_actor_401300_80141FB0[0].downFramesBase;
            work->sidestepAngle  = D_actor_401300_80141FB0[0].sidestepAngle;
            work->field_CA4      = D_actor_401300_80141FB0[0].sidestepDelay;
            break;
        case 1:
            work->downFramesBase = D_actor_401300_80141FB0[2].downFramesBase;
            work->sidestepAngle  = D_actor_401300_80141FB0[2].sidestepAngle;
            work->field_CA4      = D_actor_401300_80141FB0[2].sidestepDelay;
            break;
        case 0:
        default:
            work->downFramesBase = D_actor_401300_80141FB0[1].downFramesBase;
            work->sidestepAngle  = D_actor_401300_80141FB0[1].sidestepAngle;
            work->field_CA4      = D_actor_401300_80141FB0[1].sidestepDelay;
            break;
    }

    _actor401300InitializePoseAndCombat(actor->extra.tmd->coords, work);
    actor->state++;
}

#undef ACTOR_401300_SEED_PATROL_POINTS

/// Selects one local impact vector and its model part, advancing the LCG once.
///
/// hitYaw is relative to the actor's facing, in 4096 units per turn. Front
/// is |yaw| < 512, rear |yaw| > 1536; threshold equality takes a side branch.
/// hitOffset supplies one writable, word-aligned SVECTOR; all eight bytes
/// are copied, including pad's model-part selector (2, 7 or 9). Front draws
/// choose four offsets, rear draws two and each signed side draws two. The
/// shared LCG advances once, and no pointer is retained.
static __inline__ void _actor401300SelectHitOffset(s16 hitYaw, SVECTOR* hitOffset)
{
    enum { ACTOR_401300_HIT_FRONT_HALF_ANGLE = 512,
           ACTOR_401300_HIT_REAR_MIN_ANGLE   = 1536 };
    s32 absoluteHitYaw;

    absoluteHitYaw = (hitYaw >= 0) ? hitYaw : -hitYaw;
    // Pick a local impact offset and model part from the incoming hit bearing.
    if (absoluteHitYaw < ACTOR_401300_HIT_FRONT_HALF_ANGLE) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *hitOffset = D_actor_401300_80158928[0];
                break;
            case 1:
                *hitOffset = D_actor_401300_80158928[1];
                break;
            case 2:
                *hitOffset = D_actor_401300_80158928[2];
                break;
            case 3:
                *hitOffset = D_actor_401300_80158928[3];
                break;
            default:
                *hitOffset = D_actor_401300_80158928[4];
                break;
        }
    } else if (absoluteHitYaw > ACTOR_401300_HIT_REAR_MIN_ANGLE) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        // The retained mask selects only rear offsets 5 and 7.
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *hitOffset = D_actor_401300_80158928[5];
                break;
            case 1:
                *hitOffset = D_actor_401300_80158928[6];
                break;
            default:
                *hitOffset = D_actor_401300_80158928[7];
                break;
        }
    } else if (hitYaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = D_actor_401300_80158928[8];
        } else {
            *hitOffset = D_actor_401300_80158928[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = D_actor_401300_80158928[10];
        } else {
            *hitOffset = D_actor_401300_80158928[11];
        }
    }
}

/// Spawns a player-attack hit effect at a randomized bearing-dependent model offset.
///
/// Requires live actor work, model parts through index 9 and a live player task.
/// hitYaw is the wrapped incoming bearing relative to the facing, in 4096 units
/// per turn. attackKey must select a valid weapon/PE row for the damage lookup.
/// Advances the LCG once, selects one of the local impact vectors, and uses its
/// pad as the model-part index. The hit-effect argument record stays in the work;
/// the local offset borrows one SVECTOR of scratch space only through the spawn.
static void _actor401300SpawnHitEffect(Task* actor, s16 hitYaw, s32 attackKey)
{
    enum {
        ACTOR_401300_HIT_EFFECT_ARGUMENT_LOW = 0x300,
        ACTOR_401300_HIT_EFFECT_COUNT        = 2
    };
    SVECTOR*          hitOffset;
    _Actor401300Work* work;

    hitOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work      = actor->work;
    _actor401300SelectHitOffset(hitYaw, hitOffset);
    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = ACTOR_401300_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = ACTOR_401300_HIT_EFFECT_COUNT;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &actor->extra.tmd->coords[hitOffset->pad], hitOffset, &work->effectArg);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Applies player contacts, over-time damage and the Horned Stranger's hit reactions.
///
/// Requires live actor/Enemy/player state and initialized contact tables. Runs
/// only with positive enemy HP and outside the withdrawal leap. Reads the first
/// attack from hit contacts, then grid contacts; attachment keys use the player's
/// position. Yaws use 4096 units per turn and range uses game-coordinate units.
/// Rolls criticals, applies rear-hit scaling, recoil, sound and cooldown, then
/// selects fall/blend/status/death states. A death marks rewards pending; the
/// frame tick waits for player release. Borrows one hit scratch block per call.
static void _actor401300TakeHit(Task* actor)
{
    enum {
        ACTOR_401300_HIT_ATTACHMENT_BIT     = 0x8000,
        ACTOR_401300_HIT_REACTION_MASK      = 0xFFFF,
        ACTOR_401300_HIT_REACTION_FALL      = 4,
        ACTOR_401300_HIT_REACTION_HURT_5    = 5,
        ACTOR_401300_HIT_REACTION_HURT_8    = 8,
        ACTOR_401300_HIT_REACTION_HURT_9    = 9,
        ACTOR_401300_HIT_BEHIND_YAW_LIMIT   = 1280,
        ACTOR_401300_HIT_FALL_SIDE_ANGLE    = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        ACTOR_401300_HIT_PUSH_STEP          = 10,
        ACTOR_401300_HIT_BLEND_PUSH_STEP    = 5,
        ACTOR_401300_HIT_REFALL_RISE_TICKS  = 12,
        ACTOR_401300_CRITICAL_EFFECT_NONE   = -1,
        ACTOR_401300_CRITICAL_EFFECT_ROLLED = 0,
        ACTOR_401300_CRITICAL_EFFECT_BEHIND = 4,
        ACTOR_401300_SOUND_HURT             = SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 7),
        ACTOR_401300_SOUND_DEATH            = SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 8)
    };
    const PlayerStatus* playerStatus = &gPlayerStatus;
    _Actor401300Work*   work;
    Enemy*              enemy;
    ActorHitScratch*    savedCursor;
    ActorHitScratch*    hitScratch;
    const GfxCoord*     rootCoord;
    Task*               playerTask;
    SVECTOR*            knockback;
    s16                 hitOffsetZ;
    s32                 hitBearing;
    s32                 playerDeltaX;
    s32                 playerDeltaY;
    s32                 playerDeltaZ;
    s32                 deathSound;
    s32                 deathPan;
    s32                 hitSound;
    s32                 hitPan;
    s32                 absHitYaw;
    s16                 reactionState;
    s16                 animationId;
    s16                 criticalEffectKind;
    u32                 rearDamage;

    enemy = actor->spawnArg2.pointer;
    work  = actor->work;
    // Prefer a hit-body contact, then use the grid contacts as the fallback.
    if (enemy->hp > 0 && (work->state != ACTOR_401300_STATE_WITHDRAW || work->animId != ACTOR_401300_ANIM_LEAP)) {
        savedCursor        = SCRATCH_STACK_CURSOR(ActorHitScratch);
        hitScratch         = (SCRATCH_STACK_CURSOR(ActorHitScratch) = savedCursor - 1);
        hitScratch->hitKey = _actorContactFindAttack(&savedCursor[-1].hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        if (hitScratch->hitKey == 0) {
            hitScratch->hitKey = _actorContactFindAttack(&hitScratch->hitPos, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        }
        if (hitScratch->hitKey != 0) {
            work->field_D1C      = 0;
            work->sidestepCount  = 0;
            work->hitBody.radius = ACTOR_401300_HIT_RADIUS;
            if (hitScratch->hitKey & ACTOR_401300_HIT_ATTACHMENT_BIT) {
                playerTask            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                hitScratch->hitPos.vx = playerTask->extra.tmd->coords->workm.t[0];
                hitScratch->hitPos.vy = playerTask->extra.tmd->coords->workm.t[1];
                hitScratch->hitPos.vz = playerTask->extra.tmd->coords->workm.t[2];
            }
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(actor->extra.tmd->coords);
            hitScratch->hitOffset.vx = hitScratch->hitPos.vx - actor->extra.tmd->coords->workm.t[0];
            hitScratch->hitOffset.vy = hitScratch->hitPos.vy - actor->extra.tmd->coords->workm.t[1];
            hitOffsetZ               = hitScratch->hitPos.vz - actor->extra.tmd->coords->workm.t[2];
            hitScratch->hitOffset.vz = hitOffsetZ;
            hitBearing               = ratan2(hitScratch->hitOffset.vx, hitOffsetZ);
            rootCoord                = actor->extra.tmd->coords;
            hitScratch->hitYaw       = hitBearing - ratan2(-rootCoord->workm.m[2][0], rootCoord->workm.m[2][2]);
            hitScratch->hitYaw       = _actorAngleNormalizeYaw(hitScratch->hitYaw);
            _actor401300SpawnHitEffect(actor, hitScratch->hitYaw, hitScratch->hitKey);
            work->lookYaw              = 0;
            work->lookYawTarget        = 0;
            hitScratch->criticalEffect = ACTOR_401300_CRITICAL_EFFECT_NONE;
            reactionState              = work->state;
            if (reactionState != ACTOR_401300_STATE_FALL_BACK && reactionState != ACTOR_401300_STATE_FALL_FRONT && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_DOWN && reactionState != ACTOR_401300_STATE_RISE_BACK && reactionState != ACTOR_401300_STATE_RISE_FRONT &&
                reactionState != ACTOR_401300_STATE_WOUNDED && reactionState != ACTOR_401300_STATE_STATUS_HOLD) {
                hitScratch->towardHit = actor->extra.tmd->coords->coord;
                gfxRotMatrixY(&hitScratch->towardHit, hitScratch->hitYaw, 0);
                knockback = &hitScratch->hitOffset;
                gfxReadMatrixZAxis(&hitScratch->towardHit, knockback);
                VectorNormalSS(knockback, knockback);
                if (work->blendActive == 1) {
                    gte_lddp(-ACTOR_401300_HIT_BLEND_PUSH_STEP);
                    gte_ldsv(knockback);
                    gte_gpf12();
                    gte_stsv(knockback);
                } else {
                    gte_lddp(-ACTOR_401300_HIT_PUSH_STEP);
                    gte_ldsv(knockback);
                    gte_gpf12();
                    gte_stsv(knockback);
                }
                actor->extra.tmd->coords->coord.t[0]  += hitScratch->hitOffset.vx;
                actor->extra.tmd->coords->coord.t[1]  += hitScratch->hitOffset.vy;
                actor->extra.tmd->coords->coord.t[2]  += hitScratch->hitOffset.vz;
                actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            // Range damage, critical rolls and rear-hit scaling precede reactions.
            playerDeltaX               = playerStatus->coordMtx->t[0] - actor->extra.tmd->coords->coord.t[0];
            hitScratch->toPlayer.vx    = playerDeltaX;
            playerDeltaY               = playerStatus->coordMtx->t[1] - actor->extra.tmd->coords->coord.t[1];
            hitScratch->toPlayer.vy    = playerDeltaY;
            playerDeltaZ               = playerStatus->coordMtx->t[2] - actor->extra.tmd->coords->coord.t[2];
            hitScratch->toPlayer.vz    = playerDeltaZ;
            hitScratch->playerDistance = SquareRoot0(playerDeltaX * playerDeltaX + playerDeltaY * playerDeltaY + playerDeltaZ * playerDeltaZ);
            hitScratch->damage         = damageComputePlayerAttack(hitScratch->hitKey, hitScratch->playerDistance, 0, 0);
            if (damageRollCriticalHit(enemy, hitScratch->hitKey, 0) != 0) {
                hitScratch->critical       = 1;
                hitScratch->criticalEffect = ACTOR_401300_CRITICAL_EFFECT_ROLLED;
                hitScratch->damage        *= 4;
            } else {
                hitScratch->critical = 0;
            }
            absHitYaw = hitScratch->hitYaw;
            if (absHitYaw < 0) {
                absHitYaw = -absHitYaw;
            }
            if (absHitYaw > ACTOR_401300_HIT_BEHIND_YAW_LIMIT) {
                reactionState = work->state;
                if (reactionState != ACTOR_401300_STATE_FALL_BACK) {
                    if (reactionState != ACTOR_401300_STATE_FALL_FRONT && reactionState != ACTOR_401300_STATE_DOWN && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_RISE_BACK && reactionState != ACTOR_401300_STATE_RISE_FRONT && reactionState != ACTOR_401300_STATE_WOUNDED && reactionState != ACTOR_401300_STATE_STATUS_HOLD) {
                        rearDamage         = hitScratch->damage * 2;
                        hitScratch->damage = rearDamage;
                        if (rearDamage != 0) {
                            hitScratch->criticalEffect = ACTOR_401300_CRITICAL_EFFECT_BEHIND;
                        }
                    }
                }
            }
            damageAccumulateLifeDrainHp(enemy, hitScratch->hitKey, hitScratch->damage, 0);
            criticalEffectKind = hitScratch->criticalEffect;
            if (criticalEffectKind != ACTOR_401300_CRITICAL_EFFECT_NONE) {
                effectSpawn(EFFECT_CRITICAL_HIT, &actor->extra.tmd->coords[2], (s32)criticalEffectKind, NULL);
            }
            enemy->hp -= hitScratch->damage;
            worldTargetAddReadoutAmount(&enemy->node, hitScratch->damage, 0);
            if (work->state == ACTOR_401300_STATE_DORMANT_SCRIPTED) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            if ((work->state == ACTOR_401300_STATE_GRAB_PULL || work->state == ACTOR_401300_STATE_GRAB_STRIKE || work->state == ACTOR_401300_STATE_GRAB_DONE) && playerStatus->hp > 0 && work->playerHeld == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_401300_SOUND_DEATH;
                deathPan   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                sndEvtRequestScriptStart(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_401300_SOUND_HURT;
                hitPan   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
            }
            work->hitCooldown = damageGetPlayerAttackHitCooldown(hitScratch->hitKey);
            switch (damageGetPlayerAttackReaction(hitScratch->hitKey) & ACTOR_401300_HIT_REACTION_MASK) {
                case ACTOR_401300_HIT_REACTION_FALL:
                    reactionState = work->state;
                    if (reactionState != ACTOR_401300_STATE_FALL_BACK && reactionState != ACTOR_401300_STATE_FALL_FRONT && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_WOUNDED && reactionState != ACTOR_401300_STATE_STATUS_HOLD && reactionState != ACTOR_401300_STATE_DOWN) {
                        absHitYaw = hitScratch->hitYaw;
                        if (absHitYaw < 0) {
                            absHitYaw = -absHitYaw;
                        }
                        if (absHitYaw < ACTOR_401300_HIT_FALL_SIDE_ANGLE) {
                            work->state = ACTOR_401300_STATE_FALL_BACK;
                        } else {
                            work->state = ACTOR_401300_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_NONE:
                case ACTOR_401300_HIT_REACTION_HURT_5:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                case ACTOR_401300_HIT_REACTION_HURT_8:
                case ACTOR_401300_HIT_REACTION_HURT_9:
                    if (work->state == ACTOR_401300_STATE_STATUS_HOLD) {
                        work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                    } else if (work->state != ACTOR_401300_STATE_RISE_BACK && work->state != ACTOR_401300_STATE_RISE_FRONT) {
                        if (work->state == ACTOR_401300_STATE_FALL_BACK || work->state == ACTOR_401300_STATE_FALL_FRONT || work->state == ACTOR_401300_STATE_WOUNDED || work->state == ACTOR_401300_STATE_STATUS_HOLD || work->state == ACTOR_401300_STATE_DOWN) {
                            if (work->animId == ACTOR_401300_ANIM_FALL_BACK || work->animId == ACTOR_401300_ANIM_HOLD_BACK || work->animId == ACTOR_401300_ANIM_RISE_BACK || work->animId == ACTOR_401300_ANIM_STAGGER_BACK) {
                                work->blendActive = 1;
                                work->blendAnimId = ACTOR_401300_ANIM_FALL_BACK;
                            } else {
                                work->blendActive = 1;
                                work->blendAnimId = ACTOR_401300_ANIM_REFALL_FRONT;
                            }
                            work->blendRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                        } else if (hitScratch->critical == 1) {
                            if (work->state != ACTOR_401300_STATE_STATUS_HOLD && work->state != ACTOR_401300_STATE_WOUNDED && work->state != ACTOR_401300_STATE_DOWN) {
                                work->state = ACTOR_401300_STATE_FLINCH;
                            }
                        } else {
                            work->blendAnimId  = 0xD;
                            work->blendActive  = 1;
                            work->blendRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    damageStartEnemyBuildup(enemy, hitScratch->hitKey, 0);
                    reactionState = work->state;
                    if (reactionState == ACTOR_401300_STATE_DOWN || reactionState == ACTOR_401300_STATE_WOUNDED || reactionState == ACTOR_401300_STATE_STATUS_HOLD) {
                        work->state     = ACTOR_401300_STATE_STATUS_HOLD;
                        work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                    } else {
                        absHitYaw = hitScratch->hitYaw;
                        if (absHitYaw < 0) {
                            absHitYaw = -absHitYaw;
                        }
                        if (absHitYaw < ACTOR_401300_HIT_FALL_SIDE_ANGLE) {
                            work->state = ACTOR_401300_STATE_FALL_BACK;
                        } else {
                            work->state = ACTOR_401300_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    reactionState = work->state;
                    if (reactionState == ACTOR_401300_STATE_PATROL || reactionState == ACTOR_401300_STATE_DORMANT || reactionState == ACTOR_401300_STATE_DORMANT_SCRIPTED) {
                        work->state = ACTOR_401300_STATE_FLINCH;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, hitScratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    reactionState         = work->state;
                    if (reactionState != ACTOR_401300_STATE_FALL_BACK && reactionState != ACTOR_401300_STATE_FALL_FRONT && reactionState != ACTOR_401300_STATE_WOUNDED && reactionState != ACTOR_401300_STATE_STATUS_HOLD && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_DOWN) {
                        if (reactionState == ACTOR_401300_STATE_RISE_BACK && work->stateTimer < ACTOR_401300_HIT_REFALL_RISE_TICKS) {
                            work->state = ACTOR_401300_STATE_REFALL_BACK;
                        } else if (work->state == ACTOR_401300_STATE_RISE_FRONT && work->stateTimer < ACTOR_401300_HIT_REFALL_RISE_TICKS) {
                            work->state = ACTOR_401300_STATE_REFALL_FRONT;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            if (absHitYaw < ACTOR_401300_HIT_FALL_SIDE_ANGLE) {
                                work->state = ACTOR_401300_STATE_FALL_BACK;
                            } else {
                                work->state = ACTOR_401300_STATE_FALL_FRONT;
                            }
                        }
                    }
                    break;
            }
        }
        // Over-time damage runs even when neither contact table supplied a new hit.
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            hitScratch->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (hitScratch->damage != 0) {
                enemy->hp -= hitScratch->damage;
                worldTargetAddReadoutAmount(&enemy->node, hitScratch->damage, 0);
                if (work->state == ACTOR_401300_STATE_CHASE || work->state == ACTOR_401300_STATE_STALK || work->state == ACTOR_401300_STATE_GRAB || work->state == ACTOR_401300_STATE_GRAB_WINDUP) {
                    work->state = ACTOR_401300_STATE_FLINCH;
                } else if (work->state == ACTOR_401300_STATE_STATUS_HOLD) {
                    work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                } else {
                    if (work->state == ACTOR_401300_STATE_FALL_BACK || work->state == ACTOR_401300_STATE_FALL_FRONT || work->state == ACTOR_401300_STATE_RISE_BACK || work->state == ACTOR_401300_STATE_RISE_FRONT || work->state == ACTOR_401300_STATE_WOUNDED || work->state == ACTOR_401300_STATE_DOWN) {
                        if (work->animId == ACTOR_401300_ANIM_FALL_BACK || work->animId == ACTOR_401300_ANIM_HOLD_BACK || work->animId == ACTOR_401300_ANIM_RISE_BACK || work->animId == ACTOR_401300_ANIM_STAGGER_BACK) {
                            work->blendActive = 1;
                            work->blendAnimId = ACTOR_401300_ANIM_FALL_BACK;
                        } else if (work->animId == ACTOR_401300_ANIM_REFALL_FRONT || work->animId == ACTOR_401300_ANIM_HOLD_FRONT || work->animId == ACTOR_401300_ANIM_RISE_FRONT || work->animId == ACTOR_401300_ANIM_FALL_FRONT) {
                            work->blendActive = 1;
                            work->blendAnimId = ACTOR_401300_ANIM_REFALL_FRONT;
                        } else {
                            work->blendActive = 1;
                            work->blendAnimId = ACTOR_401300_ANIM_FLINCH;
                        }
                    } else {
                        work->blendActive = 1;
                        work->blendAnimId = ACTOR_401300_ANIM_FLINCH;
                    }
                    work->blendRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                }
            }
        }
        // Defer rewards to the frame tick until the player hold has ended.
        if (enemy->hp <= 0) {
            if (hitScratch->hitKey != 0) {
                if ((damageGetPlayerAttackReaction(hitScratch->hitKey) & ACTOR_401300_HIT_REACTION_MASK) == ACTOR_401300_HIT_REACTION_FALL) {
                    animationId = work->animId;
                    if (animationId == ACTOR_401300_ANIM_WALK || animationId == ACTOR_401300_ANIM_RUN || animationId == ACTOR_401300_ANIM_CHARGE_RUN || animationId == ACTOR_401300_ANIM_CHARGE_CLOSE || animationId == ACTOR_401300_ANIM_CHARGE_FINISH) {
                        work->state = ACTOR_401300_STATE_DEATH_BURST_WALK;
                    } else {
                        work->state = ACTOR_401300_STATE_DEATH_BURST;
                    }
                } else {
                    reactionState = work->state;
                    if (reactionState != ACTOR_401300_STATE_FALL_BACK && reactionState != ACTOR_401300_STATE_FALL_FRONT && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_STATUS_HOLD && reactionState != ACTOR_401300_STATE_WOUNDED && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_DOWN) {
                        if (reactionState == ACTOR_401300_STATE_RISE_BACK && work->stateTimer < ACTOR_401300_HIT_REFALL_RISE_TICKS) {
                            work->state     = ACTOR_401300_STATE_REFALL_BACK;
                            work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                        } else if (work->state == ACTOR_401300_STATE_RISE_FRONT && work->stateTimer < ACTOR_401300_HIT_REFALL_RISE_TICKS) {
                            work->state     = ACTOR_401300_STATE_REFALL_FRONT;
                            work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
                        } else {
                            absHitYaw = hitScratch->hitYaw;
                            if (absHitYaw < 0) {
                                absHitYaw = -absHitYaw;
                            }
                            if (absHitYaw < ACTOR_401300_HIT_FALL_SIDE_ANGLE) {
                                work->state = ACTOR_401300_STATE_FALL_BACK;
                            } else {
                                work->state = ACTOR_401300_STATE_FALL_FRONT;
                            }
                        }
                    }
                }
            } else {
                reactionState = work->state;
                if (reactionState != ACTOR_401300_STATE_FALL_BACK && reactionState != ACTOR_401300_STATE_FALL_FRONT && reactionState != ACTOR_401300_STATE_STATUS_HOLD && reactionState != ACTOR_401300_STATE_WOUNDED && reactionState != ACTOR_401300_STATE_REFALL_BACK && reactionState != ACTOR_401300_STATE_REFALL_FRONT && reactionState != ACTOR_401300_STATE_DOWN) {
                    work->state = ACTOR_401300_STATE_FALL_BACK;
                }
            }
            if (enemy->hp <= 0) {
                enemy->hp          = 0;
                work->deathPending = 1;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorHitScratch);
    }
}

/// Advances one twitch tick, reversing the held pose as its signed rate reaches one.
///
/// Requires live actor work and bank-backed animation endpoints. Rates are
/// sixteenths of a frame; signed division and halfword stores retain truncation.
static __inline__ void _actor401300OscillateHeldPose(Task* actor, _Actor401300Work* work)
{
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->animRate                         = work->animRate / 2;
    if (work->animRate == 1) {
        work->animRate = -ANIMATION_RATE_ONE;
    }
    if (work->animRate == -1) {
        work->animRate = ANIMATION_RATE_ONE;
    }
    _actor401300UpdateAnimationEffects(actor);
}

/// Holds a buildup-afflicted enemy on a twitching forward/reverse pose until the status expires.
///
/// Requires live actor work, Enemy storage and the loaded animation rigs. Entry
/// resets a front/back fall clip, advances to cue record 6/9, then begins rate
/// oscillation in sixteenths of a frame. These reverse ticks require bank-backed
/// current endpoints. Expired buildup selects WOUNDED for a wounded spawn or
/// DOWN otherwise; depleted HP selects DEATH_BURN after that test.
static void _actor401300StateStatusHold(Task* actor)
{
    enum {
        ACTOR_401300_HOLD_CLIP_COUNT = 2,
        ACTOR_401300_HOLD_BACK_CUE   = 6,
        ACTOR_401300_HOLD_FRONT_CUE  = 9
    };
    _Actor401300Work* work  = actor->work;
    Enemy*            enemy = actor->spawnArg2.pointer;
    TmdObject*        model;

    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest     = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate        = ANIMATION_RATE_ONE;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->animId == ACTOR_401300_ANIM_FALL_BACK || work->animId == ACTOR_401300_ANIM_HOLD_BACK) {
            work->animId = ACTOR_401300_ANIM_HOLD_BACK;
        } else if (work->animId == ACTOR_401300_ANIM_FALL_FRONT || work->animId == ACTOR_401300_ANIM_REFALL_FRONT || work->animId == ACTOR_401300_ANIM_HOLD_FRONT) {
            work->animId = ACTOR_401300_ANIM_HOLD_FRONT;
        }
        if ((u16)(work->animId - ACTOR_401300_ANIM_HOLD_BACK) >= ACTOR_401300_HOLD_CLIP_COUNT) {
            work->animId = ACTOR_401300_ANIM_HOLD_BACK;
        }
        do {
            _actor401300UpdateAnimationEffects(actor);
        } while (!(work->animId == ACTOR_401300_ANIM_HOLD_BACK && (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= ACTOR_401300_HOLD_BACK_CUE) &&
                 !(work->animId == ACTOR_401300_ANIM_HOLD_FRONT && (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= ACTOR_401300_HOLD_FRONT_CUE));
        work->animRate = (2 * ANIMATION_RATE_ONE);
        return;
    }
    _actor401300OscillateHeldPose(actor, work);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        work->animRate        = ANIMATION_RATE_ONE;
        if ((actor->spawnArg1.value >> 16) == ACTOR_401300_SPAWN_WOUNDED) {
            work->state = ACTOR_401300_STATE_WOUNDED;
        } else {
            work->state = ACTOR_401300_STATE_DOWN;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DEATH_BURN;
    }
}

/// Runs the placed wounded enemy's intermittent twitching until its health is exhausted.
///
/// Requires initialized actor work, Enemy storage and loaded animation rigs.
/// Entry advances the back-fall clip to its settled pose, with at most 255 ticks.
/// A signed timer alternates a random 0..255-tick pause and a seven-tick twitch;
/// reverse playback requires bank-backed current endpoints. Zero HP selects DOWN.
static void _actor401300StateWounded(Task* actor)
{
    enum {
        ACTOR_401300_WOUNDED_SEEK_LIMIT   = 255,
        ACTOR_401300_WOUNDED_PAUSE_MASK   = 255,
        ACTOR_401300_WOUNDED_TWITCH_TICKS = 7,
        ACTOR_401300_WOUNDED_RATE_CHOICES = 3
    };
    _Actor401300Work* work      = actor->work;
    Enemy*            enemy     = actor->spawnArg2.pointer;
    s16               seekTicks = 0;
    u16               rateChoice;
    TmdObject*        model;

    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest     = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = ACTOR_401300_ANIM_HOLD_BACK;
        work->jointPairTarget = ACTOR_401300_PAIR_LOW_BLEND;
        work->jointPairBlend  = ACTOR_401300_PAIR_LOW_BLEND;
        work->jointPairStep   = ACTOR_401300_PAIR_NORMAL_STEP;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        do {
            _actor401300UpdateAnimationEffects(actor);
        } while (!(work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && ++seekTicks < ACTOR_401300_WOUNDED_SEEK_LIMIT);
        work->animRate = (2 * ANIMATION_RATE_ONE);
        return;
    }
    if (++work->stateTimer == 0) {
        work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rateChoice        = (gRandomLcgState >> 16) % ACTOR_401300_WOUNDED_RATE_CHOICES;
        switch (rateChoice) {
            case 0:
                work->animRate = (2 * ANIMATION_RATE_ONE);
                break;
            case 1:
                work->animRate = (3 * ANIMATION_RATE_ONE);
                break;
            case 2:
            default:
                work->animRate = (4 * ANIMATION_RATE_ONE);
                break;
        }
        _actor401300UpdateAnimationEffects(actor);
        _actor401300UpdateAnimationEffects(actor);
        work->animRate = ANIMATION_RATE_ONE;
    } else if (work->stateTimer > 0) {
        _actor401300OscillateHeldPose(actor, work);
    } else if (work->blendActive == 1 || !(work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        work->animRate = ANIMATION_RATE_ONE;
        _actor401300UpdateAnimationEffects(actor);
    }
    if (work->stateTimer >= ACTOR_401300_WOUNDED_TWITCH_TICKS) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer = -((gRandomLcgState >> 16) & ACTOR_401300_WOUNDED_PAUSE_MASK);
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DOWN;
    }
}

/// Raises the combat alert while turning toward the player, then chooses pursuit or withdrawal.
///
/// Requires initialized actor work, Enemy storage, both live models and bound
/// rigs. Entry disables attacks/grid correction and engages battle. Later ticks
/// move the fixed-joint pair and turn by at most 16/4096 of a revolution.
/// A settled clip withdraws only behind a sight obstruction in stage 5/area 29;
/// otherwise it chases. Borrows and releases one ActorChaseScratch per turn tick.
static void _actor401300StateAlert(Task* actor)
{
    enum {
        ACTOR_401300_ALERT_TURN_LIMIT = 16,
        ACTOR_401300_ALERT_PAIR_MIN   = 400,
        ACTOR_401300_ALERT_PAIR_MAX   = 512,
        ACTOR_401300_ALERT_PAIR_STEP  = 128
    };
    _Actor401300Work*  work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* aim;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_DOWN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor401300UpdateAnimationEffects(actor);
        work->hitBody.radius = ACTOR_401300_HIT_RADIUS;
        sceneEngageBattle(1);
        work->jointPairTarget = ACTOR_401300_ALERT_PAIR_MAX;
        work->jointPairStep   = ACTOR_401300_PAIR_NORMAL_STEP;
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == ACTOR_401300_ALERT_PAIR_MAX) {
            work->jointPairStep   = ACTOR_401300_ALERT_PAIR_STEP;
            work->jointPairTarget = ACTOR_401300_ALERT_PAIR_MIN;
        } else {
            work->jointPairTarget = ACTOR_401300_ALERT_PAIR_MAX;
        }
    }
    aim                                    = SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (_playerDetectionSightBlocked(actor) == 1 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            work->state = ACTOR_401300_STATE_WITHDRAW;
        } else {
            work->state = ACTOR_401300_STATE_CHASE;
        }
    }
    aim->turn           = _actorAngleTurnToPlayer(actor, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn > ACTOR_401300_ALERT_TURN_LIMIT) {
        aim->turn = ACTOR_401300_ALERT_TURN_LIMIT;
    }
    if (aim->turn < -ACTOR_401300_ALERT_TURN_LIMIT) {
        aim->turn = -ACTOR_401300_ALERT_TURN_LIMIT;
    }
    rootCoord  = actor->extra.tmd->coords;
    aim->turn += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, aim->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    _actor401300UpdateAnimationEffects(actor);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Chooses the next pursuit attack from full-width XYZ distance in game units.
///
/// Caller has checked the turn, pursuit delay and player mode. Draws once only
/// in the intermediate or close band; the inclusive 1000..2000 band retains
/// CHASE. Above 4000 it charges; in (2000, 4000] it charges on five of sixteen
/// draws, otherwise leaps. Below 1000, seven draws select STRIKE_A and nine
/// STRIKE_B. Only the state and the shared random sequence are changed.
static __inline__ void _actor401300ChooseChaseAttack(_Actor401300Work* work, s32 playerDistance)
{
    enum {
        ACTOR_401300_CHARGE_DISTANCE     = 4000,
        ACTOR_401300_LEAP_DISTANCE       = 2000,
        ACTOR_401300_STRIKE_DISTANCE     = 1000,
        ACTOR_401300_ATTACK_DRAW_MASK    = 15,
        ACTOR_401300_CHARGE_DRAW_COUNT   = 5,
        ACTOR_401300_STRIKE_A_DRAW_COUNT = 7
    };
    if (playerDistance > ACTOR_401300_CHARGE_DISTANCE) {
        work->state = ACTOR_401300_STATE_CHARGE;
    } else if (playerDistance > ACTOR_401300_LEAP_DISTANCE) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & ACTOR_401300_ATTACK_DRAW_MASK) < ACTOR_401300_CHARGE_DRAW_COUNT) {
            work->state = ACTOR_401300_STATE_CHARGE;
        } else {
            work->state = ACTOR_401300_STATE_LEAP;
        }
    } else if (playerDistance < ACTOR_401300_STRIKE_DISTANCE) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & ACTOR_401300_ATTACK_DRAW_MASK) < ACTOR_401300_STRIKE_A_DRAW_COUNT) {
            work->state = ACTOR_401300_STATE_STRIKE_A;
        } else {
            work->state = ACTOR_401300_STATE_STRIKE_B;
        }
    }
}

/// Pursues the player and chooses a close strike, charge or leap from the post-movement distance.
///
/// Requires live actor/player models and initialized actor work with contact
/// arrays and loaded rigs. Stalking spawn kinds redirect immediately to STALK.
/// Running ticks apply collision pushback, step along local Z and turn by at most
/// 48/4096 of a revolution. After 40 ticks, attacks require an unclamped turn and
/// a player outside scripted mode. Distance uses full-width XYZ game units;
/// steering uses narrowed signed-halfword XZ. Releases its run scratch on return.
static void _actor401300StateChase(Task* actor)
{
    _Actor401300Work*       work;
    GameActor*              player;
    PlayerStatus*           playerStatus;
    TmdObject*              model;
    GfxCoord*               rootFacing;
    GfxCoord*               rootBeforePush;
    GfxCoord*               rootAfterPush;
    _Actor401300RunScratch* scratchTop;
    _Actor401300RunScratch* run;
    SVECTOR*                playerOffset;
    s32                     targetYaw;
    s32                     playerDistance;
    s32                     playerDeltaX;
    s32                     playerDeltaY;
    s32                     playerDeltaZ;

    playerStatus = &gPlayerStatus;
    work         = actor->work;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    if (((actor->spawnArg1.value >> 16) & ACTOR_401300_SPAWN_KIND_MASK) == ACTOR_401300_SPAWN_STALK) {
        work->state = ACTOR_401300_STATE_STALK;
        return;
    }
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = ACTOR_401300_ANIM_RUN;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        _actor401300UpdateAnimationEffects(actor);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->jointPairTarget = ACTOR_401300_PAIR_LOW_BLEND;
        work->jointPairStep   = ACTOR_401300_PAIR_SLOW_STEP;
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == ACTOR_401300_PAIR_LOW_BLEND) {
            work->jointPairTarget = ACTOR_401300_PAIR_HIGH_BLEND;
            work->jointPairStep   = ACTOR_401300_PAIR_SLOW_STEP;
        } else {
            work->jointPairTarget = ACTOR_401300_PAIR_LOW_BLEND;
        }
    }
    // Stage the target before animation and collision correction move the root.
    work->stateTimer++;
    scratchTop                                   = SCRATCH_STACK_CURSOR(_Actor401300RunScratch);
    playerOffset                                 = &scratchTop[-1].delta;
    rootBeforePush                               = actor->extra.tmd->coords;
    scratchTop[-1].delta.vx                      = gPlayerStatus.coordMtx->t[0] - rootBeforePush->coord.t[0];
    playerOffset->vy                             = gPlayerStatus.coordMtx->t[1] - rootBeforePush->coord.t[1];
    playerOffset->vz                             = gPlayerStatus.coordMtx->t[2] - rootBeforePush->coord.t[2];
    SCRATCH_STACK_CURSOR(_Actor401300RunScratch) = scratchTop - 1;
    run                                          = scratchTop - 1;
    actor->extra.tmd->coords->composeStamp       = GRAPHICS_COORD_DIRTY;
    _actor401300UpdateAnimationEffects(actor);
    if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
        _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    // Steer from the staged target, but choose attacks using the corrected position.
    rootAfterPush       = actor->extra.tmd->coords;
    targetYaw           = ratan2(scratchTop[-1].delta.vx, playerOffset->vz);
    run->turn           = _actorAngleNormalizeYaw(targetYaw - ratan2(-rootAfterPush->coord.m[2][0], rootAfterPush->coord.m[2][2]));
    work->lookYawTarget = run->turn;
    if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, (work->chaseRate + 2) * 30 * 1.5f / 18.0f)) {
        _actorMovementTranslateForwardNonzero(actor->extra.tmd->coords, (work->chaseRate + 2) * 30 * 1.5f / 18.0f);
    }
    if (run->turn > ACTOR_401300_RUN_TURN_LIMIT) {
        run->turn = ACTOR_401300_RUN_TURN_LIMIT;
    } else if (run->turn < -ACTOR_401300_RUN_TURN_LIMIT) {
        run->turn = -ACTOR_401300_RUN_TURN_LIMIT;
    } else {
        run->offset.vx = playerDeltaX = playerStatus->coordMtx->t[0] - actor->extra.tmd->coords->coord.t[0];
        run->offset.vy = playerDeltaY = playerStatus->coordMtx->t[1] - actor->extra.tmd->coords->coord.t[1];
        run->offset.vz = playerDeltaZ = playerStatus->coordMtx->t[2] - actor->extra.tmd->coords->coord.t[2];
        playerDistance                = SquareRoot0(playerDeltaX * playerDeltaX + playerDeltaY * playerDeltaY + playerDeltaZ * playerDeltaZ);
        run->distance                 = playerDistance;
        if (player->mode != GAME_ACTOR_MODE_SCRIPTED && work->stateTimer >= ACTOR_401300_ATTACK_WAIT_TICKS) {
            _actor401300ChooseChaseAttack(work, playerDistance);
        }
    }
    rootFacing = actor->extra.tmd->coords;
    run->turn += ratan2(-rootFacing->coord.m[2][0], rootFacing->coord.m[2][2]);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, run->turn, GRAPHICS_ROTATION_REPLACE);

    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300RunScratch);
}

/// Returns the magnitude of a signed value other than the minimum s32 value.
///
/// The withdrawal alignment test supplies a wrapped turn in [-2048, 2048].
static __inline__ s32 _actor401300Abs(s32 value)
{
    if (value < 0) {
        value = -value;
    }
    return value;
}

/// Selects the woodland exit point and facing for the actor's parent-space Z.
///
/// Stores game-unit XYZ and a yaw in 4096 units per turn in withdrawPoint.pad.
/// The fixed coordinates belong to the withdrawal room's exit routes.
static __inline__ void _actor401300ChooseWithdrawExit(_Actor401300Work* work, s32 rootZ)
{
    if (rootZ > 0x1B58) {
        work->withdrawPoint.vx  = -0xB54;
        work->withdrawPoint.vz  = 0x2198;
        work->withdrawPoint.vy  = 0;
        work->withdrawPoint.pad = ACTOR_TRANSFORM_ANGLE_TURN / 4;
    } else if (rootZ > 0x1068) {
        work->withdrawPoint.vx  = 0;
        work->withdrawPoint.vy  = 0;
        work->withdrawPoint.vz  = 0x189C;
        work->withdrawPoint.pad = 0;
    } else if (rootZ > 0x384) {
        work->withdrawPoint.vx  = 0x12C0;
        work->withdrawPoint.vy  = 0;
        work->withdrawPoint.vz  = 0x1AF4;
        work->withdrawPoint.pad = 0;
    } else if (rootZ > -0x898) {
        work->withdrawPoint.vx  = 0x12C0;
        work->withdrawPoint.vz  = -0x1388;
        work->withdrawPoint.vy  = 0;
        work->withdrawPoint.pad = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    } else {
        work->withdrawPoint.vx  = -0xB4;
        work->withdrawPoint.vy  = 0;
        work->withdrawPoint.vz  = -0x960;
        work->withdrawPoint.pad = 0;
    }
}

/// Runs toward a woodland exit point, leaps out and reports the hidden enemy to the room.
///
/// Requires initialized actor work, Enemy storage, live models and loaded rigs.
/// Entry selects an exit and facing from the root's parent-space Z. Running
/// turns by at most 48/4096 per tick; a nearby aligned exit or a 180-tick timeout
/// starts the leap. The leap steps 300 parent-coordinate units each tick until
/// the clip settles, then selects HIDDEN and sends the remaining HP to the room.
/// Borrows one run scratch block and releases it before return.
static void _actor401300StateWithdraw(Task* actor)
{
    enum {
        ACTOR_401300_WITHDRAW_EXIT_RADIUS     = 2200,
        ACTOR_401300_WITHDRAW_EXIT_TURN_LIMIT = 512,
        ACTOR_401300_WITHDRAW_RUN_TIMEOUT     = 180,
        ACTOR_401300_WITHDRAW_LEAP_STEP       = 300
    };
    _Actor401300Work*       work;
    Enemy*                  enemy;
    TmdObject*              model;
    GfxCoord*               rootCoord;
    _Actor401300RunScratch* scratchTop;
    _Actor401300RunScratch* runScratch;
    _Actor401300RunScratch* run;
    s32                     rootZ;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = ACTOR_401300_ANIM_RUN;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        _actor401300UpdateAnimationEffects(actor);
        work->jointPairTarget = ACTOR_401300_PAIR_LOW_BLEND;
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->jointPairStep   = ACTOR_401300_PAIR_SLOW_STEP;
        rootZ                 = actor->extra.tmd->coords->coord.t[2];
        _actor401300ChooseWithdrawExit(work, rootZ);
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == ACTOR_401300_PAIR_LOW_BLEND) {
            work->jointPairTarget = ACTOR_401300_PAIR_HIGH_BLEND;
            work->jointPairStep   = ACTOR_401300_PAIR_SLOW_STEP;
        } else {
            work->jointPairTarget = ACTOR_401300_PAIR_LOW_BLEND;
        }
    }
    scratchTop = SCRATCH_STACK_CURSOR(_Actor401300RunScratch);
    runScratch = scratchTop - 1;
    work->stateTimer++;
    SCRATCH_STACK_CURSOR(_Actor401300RunScratch) = runScratch;
    _actor401300UpdateAnimationEffects(actor);
    run = runScratch;
    switch (work->animId) {
        case ACTOR_401300_ANIM_RUN:
            // The exit distance intentionally uses the narrowed pre-push offset.
            runScratch->delta.vx     = work->withdrawPoint.vx - actor->extra.tmd->coords->coord.t[0];
            scratchTop[-1].offset.vx = runScratch->delta.vx;
            runScratch->delta.vy     = work->withdrawPoint.vy - actor->extra.tmd->coords->coord.t[1];
            runScratch->offset.vy    = runScratch->delta.vy;
            runScratch->delta.vz     = work->withdrawPoint.vz - actor->extra.tmd->coords->coord.t[2];
            runScratch->offset.vz    = runScratch->delta.vz;
            runScratch->distance     = SquareRoot0(scratchTop[-1].offset.vx * scratchTop[-1].offset.vx + runScratch->offset.vy * runScratch->offset.vy + runScratch->offset.vz * runScratch->offset.vz);
            if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) != 1) {
                _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
            rootCoord           = actor->extra.tmd->coords;
            run->turn           = _actorAngleNormalizeYaw(ratan2(scratchTop[-1].delta.vx, scratchTop[-1].delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
            work->lookYawTarget = run->turn;
            if (run->turn > ACTOR_401300_RUN_TURN_LIMIT) {
                run->turn = ACTOR_401300_RUN_TURN_LIMIT;
            } else if (run->turn < -ACTOR_401300_RUN_TURN_LIMIT) {
                run->turn = -ACTOR_401300_RUN_TURN_LIMIT;
            } else if ((run->distance < ACTOR_401300_WITHDRAW_EXIT_RADIUS && _actor401300Abs(_actorAngleNormalizeYaw(ratan2(-actor->extra.tmd->coords->coord.m[2][0], actor->extra.tmd->coords->coord.m[2][2]) - work->withdrawPoint.pad)) < ACTOR_401300_WITHDRAW_EXIT_TURN_LIMIT) || work->stateTimer > ACTOR_401300_WITHDRAW_RUN_TIMEOUT) {
                work->animId      = ACTOR_401300_ANIM_LEAP;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                sndEvtRequestScriptStart(SOUND_NEO_ARK_WOODLAND_STRANGER_WITHDRAW, (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords), (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
                work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            run->turn += ratan2(-actor->extra.tmd->coords->coord.m[2][0], actor->extra.tmd->coords->coord.m[2][2]);
            gfxRotMatrixY(&actor->extra.tmd->coords->coord, run->turn, GRAPHICS_ROTATION_REPLACE);
            if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, (s16)((float)((work->chaseRate + 2) * 30) * 1.5f / 18.0f)) != 0) {
                _actorMovementTranslateForwardNonzero(actor->extra.tmd->coords, (s16)((float)((work->chaseRate + 2) * 30) * 1.5f / 18.0f));
            }
            _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(actor->extra.tmd->coords);
            break;
        case ACTOR_401300_ANIM_LEAP:
            run->turn = ratan2(-actor->extra.tmd->coords->coord.m[2][0], actor->extra.tmd->coords->coord.m[2][2]);
            gfxRotMatrixY(&actor->extra.tmd->coords->coord, run->turn, GRAPHICS_ROTATION_REPLACE);
            _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_WITHDRAW_LEAP_STEP);
            _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(actor->extra.tmd->coords);
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->state = ACTOR_401300_STATE_HIDDEN;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, enemy->hp, 0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300RunScratch);
}

/// Moves the stored yaw toward its unwrapped target without overshooting.
///
/// Requires writable work with the entry-derived current and target yaws in
/// [-6144, 6144], measured in 4096ths of a turn. Changes the current yaw by at
/// most 137 units. Comparisons use stored signed values without turn wrapping.
static __inline__ void _actor401300StepTurnAroundYaw(_Actor401300Work* work)
{
    enum { ACTOR_401300_TURN_AROUND_YAW_STEP = 137 };

    if (work->turnYaw > work->turnYawTarget) {
        work->turnYaw -= ACTOR_401300_TURN_AROUND_YAW_STEP;
        if (work->turnYaw < work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    if (work->turnYaw < work->turnYawTarget) {
        work->turnYaw += ACTOR_401300_TURN_AROUND_YAW_STEP;
        if (work->turnYaw > work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
}

/// Turns through twice the entry bearing to the player, then withdraws or grabs.
///
/// Requires live work, Enemy/model storage and a player root in the same parent
/// space. The stored signed-halfword target is not wrapped; yaw uses 4096 units
/// per turn. Moves 40 units per tick (20 while blending). On a later tick at the
/// target, a counter below two or an XZ distance above 900 selects WITHDRAW;
/// otherwise GRAB. The counter is only cleared in this package. Borrows and
/// releases a chase scratch block, with nested movement/contact workspace.
static void _actor401300StateTurnAround(Task* actor)
{
    enum {
        ACTOR_401300_TURN_AROUND_GRAB_COUNT   = 2,
        ACTOR_401300_TURN_AROUND_GRAB_RADIUS  = 900,
        ACTOR_401300_TURN_AROUND_FORWARD_STEP = 40,
        ACTOR_401300_TURN_AROUND_BLEND_STEP   = 20,
    };
    _Actor401300Work*  work;
    TmdObject*         model;
    GfxCoord*          headingRoot;
    GfxCoord*          facingRoot;
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* turnScratch;

    work = actor->work;
    if (work->stateEntered != 0) {
        savedCursor                                                = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        model                                                      = actor->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                    = savedCursor - 1;
        turnScratch                                                = savedCursor - 1;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_RUN;
        work->blendActive       = 0;
        work->lookYawTarget     = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &turnScratch->delta);
        headingRoot          = actor->extra.tmd->coords;
        turnScratch->turn    = _actorAngleNormalizeYaw(ratan2(savedCursor[-1].delta.vx, turnScratch->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
        facingRoot           = actor->extra.tmd->coords;
        turnScratch->heading = ratan2(-facingRoot->coord.m[2][0], facingRoot->coord.m[2][2]);
        // Preserve the signed-halfword doubled target without wrapping it.
        work->turnYaw       = turnScratch->heading;
        work->turnYawTarget = turnScratch->heading + (u16)turnScratch->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    turnScratch                             = savedCursor - 1;
    _actor401300UpdateAnimationEffects(actor);
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &turnScratch->delta);
    if (work->turnYaw == work->turnYawTarget) {
        if (work->field_D1C < ACTOR_401300_TURN_AROUND_GRAB_COUNT || _actorRangeOutsideRadiusXZ(&turnScratch->delta, ACTOR_401300_TURN_AROUND_GRAB_RADIUS)) {
            work->state = ACTOR_401300_STATE_WITHDRAW;
        } else {
            work->state = ACTOR_401300_STATE_GRAB;
        }
    }
    _actor401300StepTurnAroundYaw(work);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, work->turnYaw, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_TURN_AROUND_FORWARD_STEP) != 0) {
            _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_TURN_AROUND_FORWARD_STEP);
        }
    } else {
        if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_TURN_AROUND_BLEND_STEP) != 0) {
            _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_TURN_AROUND_BLEND_STEP);
        }
    }
    if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) != 1) {
        _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Builds the normalized Q12 direction of the selected sidestep bearing.
///
/// Requires writable work and reserved chase scratch with an absolute yaw in
/// 4096ths of a turn. Writes sidestepDir from a temporary rotation's Z axis,
/// normalized to Q12. Translation and scratch contents remain intact.
static __inline__ void _actor401300BuildSidestepDirection(_Actor401300Work* work, ActorChaseScratch* sidestep)
{
    MATRIX   sidestepRotation;
    SVECTOR* sidestepDirection;

    gfxRotMatrixY(&sidestepRotation, sidestep->turn, GRAPHICS_ROTATION_REPLACE);
    sidestepDirection = &work->sidestepDir;
    gfxReadMatrixZAxis(&sidestepRotation, sidestepDirection);
    VectorNormalSS(sidestepDirection, sidestepDirection);
}

/// Hops along an alternating side of the player bearing, then resumes pursuit.
///
/// Requires live actor/player roots, Enemy storage and loaded rigs. Stalking
/// spawn kinds select STALK immediately. Entry picks the side once if unset,
/// flips it for next time, and adds 369 angle units to the first hop's variant
/// angle (4096 per turn). Ticks 12..21 translate by a normalized Q12 direction
/// times 222 parent-coordinate units, halved during blending; tick 30 chases.
/// Releases its chase scratch block and nested grid-contact workspace.
static void _actor401300StateSidestep(Task* actor)
{
    enum {
        ACTOR_401300_SIDESTEP_RADIUS          = 320,
        ACTOR_401300_ANIM_SIDESTEP_POSITIVE   = 21,
        ACTOR_401300_ANIM_SIDESTEP_NEGATIVE   = 20,
        ACTOR_401300_SIDESTEP_FIRST_ANGLE     = 369,
        ACTOR_401300_SIDESTEP_RATE            = 12,
        ACTOR_401300_SIDESTEP_STEP            = 222,
        ACTOR_401300_SIDESTEP_MOVE_FIRST_TICK = 12,
        ACTOR_401300_SIDESTEP_MOVE_END_TICK   = 22,
        ACTOR_401300_SIDESTEP_END_TICK        = 30,
    };
    _Actor401300Work*  work;
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* sidestep;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    u16                initialBearing;
    s32                spawnKind;

    spawnKind = (actor->spawnArg1.value >> 16);
    work      = actor->work;
    if ((spawnKind & ACTOR_401300_SPAWN_KIND_MASK) == ACTOR_401300_SPAWN_STALK) {
        work->state = ACTOR_401300_STATE_STALK;
        return;
    }
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    sidestep                                = savedCursor - 1;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_SIDESTEP_RADIUS;
        work->stateTimer        = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &sidestep->delta);
        sidestep->turn = ratan2(savedCursor[-1].delta.vx, sidestep->delta.vz);
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = 1;
            } else {
                work->sidestepSide = -1;
            }
        }
        if (work->sidestepSide == 1) {
            work->animId = ACTOR_401300_ANIM_SIDESTEP_POSITIVE;
            if (work->sidestepCount == 0) {
                initialBearing = sidestep->turn + ACTOR_401300_SIDESTEP_FIRST_ANGLE;
                sidestep->turn = work->sidestepAngle + initialBearing;
            } else {
                sidestep->turn += work->sidestepAngle;
            }
            work->sidestepSide = -1;
        } else {
            work->animId = ACTOR_401300_ANIM_SIDESTEP_NEGATIVE;
            if (work->sidestepCount == 0) {
                initialBearing = sidestep->turn - ACTOR_401300_SIDESTEP_FIRST_ANGLE;
                sidestep->turn = initialBearing - work->sidestepAngle;
            } else {
                sidestep->turn -= work->sidestepAngle;
            }
            work->sidestepSide = 1;
        }
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate    = ACTOR_401300_SIDESTEP_RATE;
        work->blendActive = 0;
        _actor401300UpdateAnimationEffects(actor);
        _actor401300BuildSidestepDirection(work, sidestep);
        work->sidestepStep = ACTOR_401300_SIDESTEP_STEP;
        work->sidestepCount++;
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor401300UpdateAnimationEffects(actor);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestep->delta);
    } else {
        gte_lddp(work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestep->delta);
    }
    // Only the middle ten ticks of the hop move the root.
    if (work->stateTimer >= ACTOR_401300_SIDESTEP_MOVE_FIRST_TICK && work->stateTimer < ACTOR_401300_SIDESTEP_MOVE_END_TICK) {
        rootCoord              = actor->extra.tmd->coords;
        rootCoord->coord.t[0] += sidestep->delta.vx;
        rootCoord              = actor->extra.tmd->coords;
        rootCoord->coord.t[2] += sidestep->delta.vz;
        _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
    }
    if (++work->stateTimer >= ACTOR_401300_SIDESTEP_END_TICK) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Negotiates the player button hold and starts playback only after acceptance.
///
/// Requires live actor work and a player task able to accept an eight-press
/// hold. Rejection leaves the state unchanged; acceptance selects GRAB_PULL,
/// player clip 1 and zero displacement with every collision pass requested.
/// Messages borrow work records synchronously; the animation table is borrowed
/// by the player for playback. Its resources must remain loaded.
static __inline__ void _actor401300StartPlayerGrab(_Actor401300Work* work)
{
    enum { ACTOR_401300_GRAB_PRESS_COUNT    = 8,
           ACTOR_401300_PLAYER_ANIM_GRABBED = 1 };

    work->playerAnim.source.sets      = D_actor_401300_801588F0;
    work->playerButtonHold.pressCount = ACTOR_401300_GRAB_PRESS_COUNT;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
        work->state                  = ACTOR_401300_STATE_GRAB_PULL;
        work->playerHeld             = 1;
        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_GRABBED;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->playerMove.displacement.vz   = 0;
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx   = 0;
        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        work->playerMove.keepControl       = 1;
        work->playerAnimFrames             = 0;
    }
}

/// Reaches for the player and starts an eight-press hold at the grab cue.
///
/// Requires live actor/player work, roots in the same parent space and loaded
/// grab/player animation sets. Cue 16 accepts only a nonscripted player within
/// 1100 XZ units and less than 16/4096 of a turn off facing. An accepted hold
/// starts player clip 1 and GRAB_PULL; a settled grab returns to CHASE. After
/// the cue, the actor eases 10 units away while within 1400 units. Message
/// records are borrowed synchronously; the player's animation table stays live
/// through playback.
static void _actor401300StateGrab(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_GRAB              = 4,
        ACTOR_401300_GRAB_CUE               = 16,
        ACTOR_401300_GRAB_TURN_LIMIT        = 16,
        ACTOR_401300_GRAB_RADIUS            = 1100,
        ACTOR_401300_GRAB_SEPARATION_RADIUS = 1400,
        ACTOR_401300_GRAB_SEPARATION_STEP   = 10,
    };
    SVECTOR           playerOffset;
    _Actor401300Work* work;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    GameActor*        player;
    PlayerStatus*     playerStatus;
    SVECTOR*          separation;
    s16               playerTurn;

    enemy        = actor->spawnArg2.pointer;
    work         = actor->work;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                = ANIMATION_RATE_ONE;
        work->animId                  = ACTOR_401300_ANIM_GRAB;
        _actor401300UpdateAnimationEffects(actor);
        gfxRotMatrixY(&actor->extra.tmd->coords->coord, _actorAngleTurnToPlayer(actor, &playerOffset, playerStatus), GRAPHICS_ROTATION_COMPOSE);
        _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
        playerOffset.vx                        = actor->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        playerOffset.vy                        = 0;
        playerOffset.vz                        = actor->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        work->lookYawTarget                    = 0;
        work->lookYaw                          = 0;
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                    = 0;
        work->playerHeld                       = 0;
    }
    _actor401300UpdateAnimationEffects(actor);
    // The player can reject the hold while its recovery window is active.
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_401300_GRAB_CUE && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        playerTurn = _actorAngleTurnToMatrixPosition(actor, &playerOffset, gPlayerStatus.coordMtx);
        if (abs(playerTurn) < ACTOR_401300_GRAB_TURN_LIMIT && !_actorRangeOutsideRadiusXZ(&playerOffset, ACTOR_401300_GRAB_RADIUS)) {
            _actor401300StartPlayerGrab(work);
        }
    }
    if (work->animId == ACTOR_401300_ANIM_GRAB && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) > ACTOR_401300_GRAB_CUE) {
        separation      = &playerOffset;
        playerOffset.vx = actor->extra.tmd->coords->coord.t[0] - playerStatus->coordMtx->t[0];
        playerOffset.vy = 0;
        playerOffset.vz = actor->extra.tmd->coords->coord.t[2] - playerStatus->coordMtx->t[2];
        if (!_actorRangeOutsideRadiusXZ(separation, ACTOR_401300_GRAB_SEPARATION_RADIUS)) {
            VectorNormalSS(separation, separation);
            gte_lddp(ACTOR_401300_GRAB_SEPARATION_STEP);
            gte_ldsv(separation);
            gte_gpf12();
            gte_stsv(separation);
            rootCoord                              = actor->extra.tmd->coords;
            rootCoord->coord.t[0]                 += playerOffset.vx;
            rootCoord                              = actor->extra.tmd->coords;
            rootCoord->coord.t[2]                 += playerOffset.vz;
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

/// Places the held player and separates the actor horizontally for the strike.
///
/// Requires live actor/Enemy/work storage and the held player's model in the
/// same parent-coordinate frame. Player placement copies the original XYZ;
/// separation is 1000 horizontal game units and angles use 4096ths of a turn.
/// The caller supplies writable Task*, SVECTOR and SVECTOR* temporaries as
/// playerTask, fromPlayer and separation. Requires local constants
/// `ACTOR_401300_ANIM_GRAB_PULL` and `ACTOR_401300_GRAB_SEPARATION`.
/// Arguments occur repeatedly and must be side-effect-free.
/// Expands to one block; the placement payload is borrowed through dispatch.
#define ACTOR_401300_PLACE_GRAB_PAIR(actor, work, enemy, playerTask, fromPlayer, separation)                                   \
    {                                                                                                                          \
        (playerTask)                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);                                \
        (work)->hitBody.radius                        = ACTOR_401300_HIT_RADIUS;                                               \
        (work)->attackBody.flags                     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED); \
        (work)->gridBody.flags                       |= WORLD_COLLISION_BODY_GRID_ENABLED;                                     \
        (enemy)->node.state.parts.flags               = 0;                                                                     \
        (work)->animRequest                           = ACTOR_401300_ANIM_REQUEST_BLEND;                                       \
        (work)->animRate                              = ANIMATION_RATE_ONE;                                                    \
        (work)->animId                                = ACTOR_401300_ANIM_GRAB_PULL;                                           \
        (playerTask)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                                                  \
        actorRenderComposeCoord((playerTask)->extra.tmd->coords);                                                              \
        (work)->playerPlacement.pos.vx = (playerTask)->extra.tmd->coords->coord.t[0];                                          \
        (work)->playerPlacement.pos.vy = (playerTask)->extra.tmd->coords->coord.t[1];                                          \
        (work)->playerPlacement.pos.vz = (playerTask)->extra.tmd->coords->coord.t[2];                                          \
        (separation)                   = &(fromPlayer);                                                                        \
        (fromPlayer).vx                = (actor)->extra.tmd->coords->coord.t[0] - (playerTask)->extra.tmd->coords->coord.t[0]; \
        (fromPlayer).vy                = 0;                                                                                    \
        (fromPlayer).vz                = (actor)->extra.tmd->coords->coord.t[2] - (playerTask)->extra.tmd->coords->coord.t[2]; \
        VectorNormalSS((separation), (separation));                                                                            \
        gte_lddp(ACTOR_401300_GRAB_SEPARATION);                                                                                \
        gte_ldsv((separation));                                                                                                \
        gte_gpf12();                                                                                                           \
        gte_stsv((separation));                                                                                                \
        (actor)->extra.tmd->coords->coord.t[0]   = (playerTask)->extra.tmd->coords->coord.t[0] + (fromPlayer).vx;              \
        (actor)->extra.tmd->coords->coord.t[2]   = (playerTask)->extra.tmd->coords->coord.t[2] + (fromPlayer).vz;              \
        (actor)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;                                                       \
        (work)->playerPlacement.rot.vx           = 0;                                                                          \
        (work)->playerPlacement.rot.vy           = ratan2((fromPlayer).vx, (fromPlayer).vz);                                   \
        (work)->playerPlacement.rot.vz           = 0;                                                                          \
        TASK_MESSAGE_DISPATCH_POINTER((playerTask), GAME_ACTOR_MESSAGE_PLACE, &(work)->playerPlacement, 0);                    \
    }

/// Places the held player facing the actor and plays the pull before the strike.
///
/// Requires live actor/Enemy storage and a held player model in the same root
/// parent space. Entry moves the actor 1000 horizontal units from the player
/// and sends the player's original position plus a facing yaw. The placement
/// record is borrowed through dispatch. Bends parts 2 and 3 by -128/4096 of a
/// turn each tick; when clip 5 settles, spawns the contact effect and selects
/// GRAB_STRIKE. The coordinate array must reach part 5.
static void _actor401300StateGrabPull(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_GRAB_PULL  = 5,
        ACTOR_401300_GRAB_SEPARATION = 1000,
    };
    SVECTOR           fromPlayer;
    _Actor401300Work* work  = actor->work;
    Enemy*            enemy = actor->spawnArg2.pointer;
    Task*             playerTask;
    SVECTOR*          separation;

    if (work->stateEntered != 0) {
        ACTOR_401300_PLACE_GRAB_PAIR(actor, work, enemy, playerTask, fromPlayer, separation);
    }
    _actor401300UpdateAnimationEffects(actor);
    gfxRotMatrixX(&actor->extra.tmd->coords[2].coord, -ACTOR_401300_GRAB_JOINT_BEND, GRAPHICS_ROTATION_COMPOSE);
    actor->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&actor->extra.tmd->coords[2]);
    gfxRotMatrixX(&actor->extra.tmd->coords[3].coord, -ACTOR_401300_GRAB_JOINT_BEND, GRAPHICS_ROTATION_COMPOSE);
    actor->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&actor->extra.tmd->coords[3]);
    if (work->animId == ACTOR_401300_ANIM_GRAB_PULL && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        work->effectArg.coord      = &actor->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = ACTOR_401300_GRAB_EFFECT_MAGNITUDE;
        work->effectArg.spawnArgHi = ACTOR_401300_GRAB_EFFECT_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_401300_GRAB_HIT_EFFECT_KEY), &actor->extra.tmd->coords[5], NULL, &work->effectArg);
        work->state = ACTOR_401300_STATE_GRAB_STRIKE;
    }
}

#undef ACTOR_401300_PLACE_GRAB_PAIR

/// Bends the grab-strike joints while refreshing the opposite joint after each bend.
///
/// Requires live model coordinates through part 5. Adds -128/4096 of a turn
/// around X to part 2, dirties part 4 and composes part 3 before its bend.
/// Then bends part 3, dirties part 5 and composes part 2. Preserve this order:
/// the first composition sees part 3 before this tick's additional X rotation.
static __inline__ void _actor401300BendGrabStrikeJoints(Task* actor)
{
    gfxRotMatrixX(&actor->extra.tmd->coords[2].coord, -ACTOR_401300_GRAB_JOINT_BEND, GRAPHICS_ROTATION_COMPOSE);
    actor->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&actor->extra.tmd->coords[3]);
    gfxRotMatrixX(&actor->extra.tmd->coords[3].coord, -ACTOR_401300_GRAB_JOINT_BEND, GRAPHICS_ROTATION_COMPOSE);
    actor->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&actor->extra.tmd->coords[2]);
}

/// Applies attack zero to the held player and finishes the grab on a clip jump.
///
/// Requires initialized actor/Enemy work and the held player's task and clip
/// table. Entry resets actor clip 6 and requests player clip 2. Fatal damage
/// leaves the player in scripted attack state 10, whose handler stays there
/// while HP is zero. A FOLLOWED_JUMP flag observed before this tick's animation update
/// spawns the contact effect and selects GRAB_DONE. Parts 2/3 are bent each
/// tick; composition retains the order part 3 then part 2. Model storage
/// must reach part 5; player animation resources outlive playback.
static void _actor401300StateGrabStrike(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_GRAB_STRIKE        = 6,
        ACTOR_401300_GRAB_ATTACK_INDEX       = 0,
        ACTOR_401300_PLAYER_ANIM_GRAB_STRUCK = 2,
        ACTOR_401300_PLAYER_FATAL_HOLD_STATE = 10,
    };
    _Actor401300Work* work       = actor->work;
    Enemy*            enemy      = actor->spawnArg2.pointer;
    Task*             playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (work->stateEntered != 0) {
        work->animRate    = ANIMATION_RATE_ONE;
        work->animId      = ACTOR_401300_ANIM_GRAB_STRIKE;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
        if ((s16)taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_401300_GRAB_ATTACK_INDEX), 0) == 1) {
            ((GameActor*)playerTask->work)->state = ACTOR_401300_PLAYER_FATAL_HOLD_STATE;
        }
        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_GRAB_STRUCK;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->playerAnimFrames = 0;
    }
    // Test the previous tick before advancing the actor animation.
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
        work->effectArg.coord      = &actor->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = ACTOR_401300_GRAB_EFFECT_MAGNITUDE;
        work->effectArg.spawnArgHi = ACTOR_401300_GRAB_EFFECT_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_401300_GRAB_HIT_EFFECT_KEY), &actor->extra.tmd->coords[5], NULL, &work->effectArg);
        work->state = ACTOR_401300_STATE_GRAB_DONE;
    }
    work->grabAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    _actor401300UpdateAnimationEffects(actor);
    _actor401300BendGrabStrikeJoints(actor);
}

/// Selects a settled fall's rest, buildup reaction or burning death path.
///
/// Requires live initialized work and Enemy storage. Exhausted HP selects
/// DEATH_BURN, buildup selects STATUS_HOLD, and otherwise DOWN is selected.
static __inline__ void _actor401300ChooseFallExit(_Actor401300Work* work, const Enemy* enemy)
{
    work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DEATH_BURN;
    } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        work->state = ACTOR_401300_STATE_STATUS_HOLD;
    } else {
        work->state = ACTOR_401300_STATE_DOWN;
    }
}

/// Staggers backward, falls onto the back and selects rest, buildup hold or burn.
///
/// Requires initialized work, live Enemy/model storage and loaded clips. The
/// stagger moves -87 parent-coordinate units per tick before animation update;
/// the settled clip resets into FALL_BACK. Hit-body grid contacts take priority
/// over the root grid body until the fall settles. Then grid collision on the
/// hit body is disabled and HP/buildup selects DEATH_BURN, STATUS_HOLD or DOWN.
static void _actor401300StateFallBack(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_STAGGER_BACK = 10,
        ACTOR_401300_STAGGER_BACK_STEP = -87,
    };
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = 0;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_401300_ANIM_STAGGER_BACK;
        work->blendActive             = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->jointPairTarget = ACTOR_401300_FALL_PAIR_BLEND;
        work->jointPairStep   = ACTOR_401300_FALL_PAIR_STEP;
        work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->animId == ACTOR_401300_ANIM_STAGGER_BACK && (s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_STAGGER_BACK_STEP) != 0) {
        _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_STAGGER_BACK_STEP);
    }
    _actor401300UpdateAnimationEffects(actor);
    // The falling body itself gets the first room-grid correction.
    if (_actorContactApplyGridPushback(actor->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (work->animId == ACTOR_401300_ANIM_STAGGER_BACK) {
            work->animId      = ACTOR_401300_ANIM_FALL_BACK;
            work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
            _actor401300UpdateAnimationEffects(actor);
        }
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && work->animId == ACTOR_401300_ANIM_FALL_BACK) {
            _actor401300ChooseFallExit(work, enemy);
        }
    }
}

/// Falls forward and selects rest, buildup hold or burn when the pose settles.
///
/// Requires initialized work, live Enemy/model storage and loaded FALL_FRONT.
/// Hit-body grid contacts take priority over the root grid body's contacts.
/// On settle, disables grid collision on the hit body and selects DEATH_BURN
/// for exhausted HP, STATUS_HOLD for buildup, otherwise DOWN. Unlike the back
/// fall, it retains the existing blend-active value and has no stagger step.
static void _actor401300StateFallFront(Task* actor)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = 0;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_401300_ANIM_FALL_FRONT;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->jointPairTarget = ACTOR_401300_FALL_PAIR_BLEND;
        work->jointPairStep   = ACTOR_401300_FALL_PAIR_STEP;
        work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _actor401300UpdateAnimationEffects(actor);
    if (_actorContactApplyGridPushback(actor->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401300ChooseFallExit(work, enemy);
    }
}

/// Rebuilds a local yaw rotation with separate horizontal and vertical Q12 scales.
///
/// Requires a live writable coordinate and an initialized scratch stack with
/// 0x58 free aligned bytes including the nested axis-rotation workspace. Scale
/// 4096 is unity; products must fit s32 before the SDK narrows to halfwords.
/// Pitch, roll and old scale are discarded; translation, parent and stored Euler
/// angles remain intact. Marks composition dirty and releases all scratch space.
static __inline__ void _actorRenderRescaleYawXZ(GfxCoord* coord, s32 horizontalScale, s16 verticalScale)
{
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);
    // Keep the heading while replacing pitch, roll and all previous scale.
    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vx = horizontalScale;
    yawScratch->scale.vy = verticalScale;
    yawScratch->scale.vz = horizontalScale;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);
    _actorRenderCopyRotation(coord, yawScratch->rotation.m);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Anchors the corpse burn under the body in world coordinates at root height.
///
/// Requires live actor/Enemy/work storage, model part 2 and gGfxViewCoord.
/// XZ comes from the part's world origin, narrowed to halfwords; Y comes from
/// the root's parent-space height. Attaches the resulting anchor to the view
/// coordinate, composes it and starts weighted-color burn playback. The caller
/// supplies a writable SVECTOR burnOrigin and local
/// `ACTOR_401300_BURN_BURST_COUNT`.
/// Arguments occur repeatedly and must be side-effect-free. Expands to one block;
/// the work-owned burnCoord must outlive the spawned effect's use of it.
#define ACTOR_401300_START_CORPSE_BURN(actor, work, enemy, burnOrigin)                            \
    {                                                                                             \
        gfxSetRotIdentity(&(work)->burnCoord.coord);                                              \
        (burnOrigin).vx = 0;                                                                      \
        (burnOrigin).vy = 0;                                                                      \
        (burnOrigin).vz = 0;                                                                      \
        _actorRenderTransformToWorld(&(actor)->extra.tmd->coords[2], &(burnOrigin));              \
        (work)->burnCoord.parent       = &gGfxViewCoord;                                          \
        (work)->burnCoord.coord.t[0]   = (burnOrigin).vx;                                         \
        (work)->burnCoord.coord.t[1]   = (actor)->extra.tmd->coords->coord.t[1];                  \
        (work)->burnCoord.coord.t[2]   = (burnOrigin).vz;                                         \
        (work)->burnCoord.composeStamp = GRAPHICS_COORD_DIRTY;                                    \
        actorRenderComposeCoord(&(work)->burnCoord);                                              \
        worldCoordSetActorColorMode((enemy), ENEMY_COLOR_WEIGHTED);                               \
        effectSpawn(EFFECT_CORPSE_BURN, &(work)->burnCoord, ACTOR_401300_BURN_BURST_COUNT, NULL); \
    }

/// Burns the corpse, flattens its root and hides it before reporting death.
///
/// Requires live Enemy/model/work storage, view coordinates and loaded effects.
/// At tick 30 anchors the burn under part 2 at the root's height; ticks 42/48/64
/// blacken, enable translucency and suppress active drawing. From tick 26 the
/// Y scale shrinks by 16 Q12 units per tick. After tick 64, playerHeld must be
/// clear to select DEAD. The retained 1024 guard permits tick 1025, after which
/// this state stops advancing; the scale arithmetic still narrows to s16.
static void _actor401300StateDeathBurn(Task* actor)
{
    enum {
        ACTOR_401300_BURN_TIMER_LIMIT        = 1024,
        ACTOR_401300_BURN_EFFECT_TICK        = 30,
        ACTOR_401300_BURN_BLACK_TICK         = 42,
        ACTOR_401300_BURN_TRANSLUCENT_TICK   = 48,
        ACTOR_401300_BURN_HIDE_TICK          = 64,
        ACTOR_401300_BURN_FLATTEN_FIRST_TICK = 26,
        ACTOR_401300_BURN_FLATTEN_BASE_TICK  = 20,
        ACTOR_401300_BURN_FLATTEN_SCALE_STEP = 16,
        ACTOR_401300_BURN_ANIMATION_RATE     = ANIMATION_RATE_ONE / 2,
        ACTOR_401300_BURN_BURST_COUNT        = 3, // Three bursts of three flames; also sets each flame's size/motion argument
    };
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    SVECTOR           burnOrigin;
    s16               burnTick;

    work  = actor->work;
    model = actor->extra.tmd;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags        = (work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer              = 0;
        work->animRate                = ACTOR_401300_BURN_ANIMATION_RATE;
    }
    _actor401300UpdateAnimationEffects(actor);
    if (work->stateTimer <= ACTOR_401300_BURN_TIMER_LIMIT) {
        switch (++work->stateTimer) {
            case ACTOR_401300_BURN_EFFECT_TICK:
                ACTOR_401300_START_CORPSE_BURN(actor, work, enemy, burnOrigin);
                break;
            case ACTOR_401300_BURN_TRANSLUCENT_TICK:
                actor->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case ACTOR_401300_BURN_BLACK_TICK:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case ACTOR_401300_BURN_HIDE_TICK:
                actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        burnTick = work->stateTimer;
        if (burnTick >= ACTOR_401300_BURN_FLATTEN_FIRST_TICK) {
            _actorRenderRescaleYawXZ(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE, ACTOR_401300_ROOT_SCALE - (burnTick - ACTOR_401300_BURN_FLATTEN_BASE_TICK) * ACTOR_401300_BURN_FLATTEN_SCALE_STEP);
        }
        if (work->stateTimer > ACTOR_401300_BURN_HIDE_TICK && work->playerHeld == 0) {
            work->state = ACTOR_401300_STATE_DEAD;
        }
    }
}

#undef ACTOR_401300_START_CORPSE_BURN

/// Checks horizontal player proximity and combat noise for ordinary dormancy.
///
/// Requires live actor/player roots in the same parent-coordinate frame and
/// writable work. Reads gPlayerStatus and gSceneCombatState; narrows the player
/// offset to an SVECTOR and tests XZ within 3000 game units. Noise additionally
/// engages battle. rootCoord, delta and toPlayer are writable GfxCoord*, SVECTOR
/// and SVECTOR* caller temporaries. Arguments occur repeatedly and must be
/// side-effect-free. Expands to one block and releases nested range scratch.
#define ACTOR_401300_CHECK_DORMANT_ALERT(actor, work, rootCoord, delta, toPlayer)          \
    {                                                                                      \
        (rootCoord)    = (actor)->extra.tmd->coords;                                       \
        (toPlayer)     = &(delta);                                                         \
        (delta).vx     = gPlayerStatus.coordMtx->t[0] - (rootCoord)->coord.t[0];           \
        (toPlayer)->vy = gPlayerStatus.coordMtx->t[1] - (rootCoord)->coord.t[1];           \
        (toPlayer)->vz = gPlayerStatus.coordMtx->t[2] - (rootCoord)->coord.t[2];           \
        if (!_actorRangeOutsideRadiusXZ((toPlayer), ACTOR_401300_DORMANT_NOTICE_RADIUS)) { \
            (work)->state = ACTOR_401300_STATE_ALERT;                                      \
        }                                                                                  \
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {     \
            sceneEngageBattle(1);                                                          \
            (work)->state = ACTOR_401300_STATE_ALERT;                                      \
        }                                                                                  \
    }

/// Idles between two clips until the player is nearby or combat noise alerts it.
///
/// Requires live actor/Enemy/player roots in the same parent space and loaded
/// dormant clips. XZ notice radius is 3000 game units. Noise also engages battle.
/// After the counter passes 2400, a zero four-bit random draw skips the entire
/// tick, including detection and playback. A clip jump can start the alternate
/// idle; its settled pose returns to the base idle. Entry saves the color matrix
/// by value without restoring it here.
static void _actor401300StateDormant(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_DORMANT           = 14,
        ACTOR_401300_ANIM_DORMANT_ALTERNATE = 15,
        ACTOR_401300_DORMANT_THROTTLE_TICKS = 2400,
        ACTOR_401300_DORMANT_SKIP_DRAW_MASK = 15,
    };
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    SVECTOR           delta;
    SVECTOR*          toPlayer;

    work = actor->work;
    if (work->stateEntered != 0) {
        model        = actor->extra.tmd;
        enemy        = actor->spawnArg2.pointer;
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = ACTOR_401300_ANIM_DORMANT;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    // A skipped long-idle tick also skips player detection.
    if (work->stateTimer > ACTOR_401300_DORMANT_THROTTLE_TICKS) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & ACTOR_401300_DORMANT_SKIP_DRAW_MASK)) {
            return;
        }
    } else {
        work->stateTimer++;
    }
    ACTOR_401300_CHECK_DORMANT_ALERT(actor, work, rootCoord, delta, toPlayer);
    _actor401300UpdateAnimationEffects(actor);
    if (work->animId == ACTOR_401300_ANIM_DORMANT && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->animId      = ACTOR_401300_ANIM_DORMANT_ALTERNATE;
            work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
            _actor401300UpdateAnimationEffects(actor);
        }
    }
    if (work->animId == ACTOR_401300_ANIM_DORMANT_ALTERNATE && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        work->animId      = ACTOR_401300_ANIM_DORMANT;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        _actor401300UpdateAnimationEffects(actor);
    }
}

#undef ACTOR_401300_CHECK_DORMANT_ALERT

/// Stops the dormant sound on proximity and alerts on proximity or combat noise.
///
/// Requires live actor/player roots in the same parent-coordinate frame and
/// writable work. Reads gPlayerStatus and gSceneCombatState; narrows the player
/// offset and tests XZ within 3000 game units. Proximity stops the base sound;
/// both proximity and noise engage battle and alert. rootCoord, delta and
/// toPlayer are writable GfxCoord*, SVECTOR and SVECTOR* caller temporaries.
/// Arguments occur repeatedly and must be side-effect-free. Expands to one block
/// and releases nested range scratch.
#define ACTOR_401300_CHECK_SCRIPTED_DORMANT_ALERT(actor, work, rootCoord, delta, toPlayer)                   \
    {                                                                                                        \
        (rootCoord)    = (actor)->extra.tmd->coords;                                                         \
        (toPlayer)     = &(delta);                                                                           \
        (delta).vx     = gPlayerStatus.coordMtx->t[0] - (rootCoord)->coord.t[0];                             \
        (toPlayer)->vy = gPlayerStatus.coordMtx->t[1] - (rootCoord)->coord.t[1];                             \
        (toPlayer)->vz = gPlayerStatus.coordMtx->t[2] - (rootCoord)->coord.t[2];                             \
        if (!_actorRangeOutsideRadiusXZ((toPlayer), ACTOR_401300_DORMANT_NOTICE_RADIUS)) {                   \
            sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE); \
            sceneEngageBattle(1);                                                                            \
            (work)->state = ACTOR_401300_STATE_ALERT;                                                        \
        }                                                                                                    \
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {                       \
            sceneEngageBattle(1);                                                                            \
            (work)->state = ACTOR_401300_STATE_ALERT;                                                        \
        }                                                                                                    \
    }

/// Plays the room-command idle with a placement-tagged sound and a contact cue.
///
/// Requires live actor/Enemy/player roots, loaded clip/effect data and writable
/// animation table entry 16. Rebinds that entry on first tick; the next tick
/// queues the sound. Cue index 4 fires once per held frame. A player within
/// 3000 XZ units stops the base sound and alerts; combat noise also alerts and
/// engages battle, without stopping the sound on that branch.
static void _actor401300StateDormantScripted(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_DORMANT_SCRIPTED     = 16,
        ACTOR_401300_DORMANT_EFFECT_CUE        = 4,
        ACTOR_401300_DORMANT_SOUND_PLACE_SHIFT = 8,
    };
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    SVECTOR           delta;
    SVECTOR*          toPlayer;
    s32               soundScriptId;
    s32               audioPan;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                                                       = actor->extra.tmd;
        D_actor_401300_80158838[ACTOR_401300_ANIM_DORMANT_SCRIPTED] = &gActor401300Animation20D98;
        work->animId                                                = ACTOR_401300_ANIM_DORMANT_SCRIPTED;
        work->animRequest                                           = ACTOR_401300_ANIM_REQUEST_RESET;
        model->flags                                                = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
    } else if (work->stateTimer == 0) {
        soundScriptId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_401300_DORMANT_SOUND_PLACE_SHIFT) | SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT;
        audioPan      = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
        sndEvtRequestScriptStart(soundScriptId, audioPan, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
        work->stateTimer = 1;
    }
    _actor401300UpdateAnimationEffects(actor);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_401300_DORMANT_EFFECT_CUE && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
        work->effectArg.coord      = actor->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = ACTOR_401300_GRAB_EFFECT_MAGNITUDE;
        work->effectArg.spawnArgHi = ACTOR_401300_GRAB_EFFECT_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_401300_GRAB_HIT_EFFECT_KEY), actor->extra.tmd->coords + 5, NULL, &work->effectArg);
    }
    // Keep the cue index so a held pose cannot emit the effect twice.
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    ACTOR_401300_CHECK_SCRIPTED_DORMANT_ALERT(actor, work, rootCoord, delta, toPlayer);
}

#undef ACTOR_401300_CHECK_SCRIPTED_DORMANT_ALERT

/// Limits the patrol turn and replaces the model root heading.
///
/// Requires live actor/model storage and reserved ActorTurnScratch with a signed
/// relative yaw; turnLimit is positive in 4096ths of a turn. Translation stays
/// intact; the scratch yaw becomes absolute. Restores the uniform root scale.
/// Arguments must be side-effect-free: pointers and limit occur repeatedly.
/// headingRoot must be a writable GfxCoord* local. Expands to one block.
#define ACTOR_401300_TURN_PATROL_ROOT(actor, scratch, headingRoot, turnLimit)                           \
    {                                                                                                   \
        if ((scratch)->angle > (turnLimit)) {                                                           \
            (scratch)->angle = (turnLimit);                                                             \
        }                                                                                               \
        if ((scratch)->angle < -(turnLimit)) {                                                          \
            (scratch)->angle = -(turnLimit);                                                            \
        }                                                                                               \
        (headingRoot)     = (actor)->extra.tmd->coords;                                                 \
        (scratch)->angle += ratan2(-(headingRoot)->coord.m[2][0], (headingRoot)->coord.m[2][2]);        \
        gfxRotMatrixY(&(actor)->extra.tmd->coords->coord, (scratch)->angle, GRAPHICS_ROTATION_REPLACE); \
        _actorRenderRescaleYaw((actor)->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);                    \
    }

/// Walks between the two patrol endpoints until proximity, facing or attacks alert it.
///
/// Requires initialized work/rigs, Enemy/model storage, patrolTarget in 0..1
/// and a live player root in the same parent space. Arrival within 160 XZ units
/// flips the endpoint but retains this tick's old target offset. Turns at most
/// 32/4096 of a turn and steps 10 units outside blending. A player within 2000
/// units, or within 4000 and less than 768 angle units off facing, selects ALERT;
/// attack signals do so regardless of range. Releases its turn scratch block
/// and nested movement, contact and range-test workspace.
static void _actor401300StatePatrol(Task* actor)
{
    enum {
        ACTOR_401300_PATROL_PAIR_HIGH_BLEND = 96,
        ACTOR_401300_PATROL_PAIR_LOW_BLEND  = 32,
        ACTOR_401300_PATROL_PAIR_STEP       = 8,
        ACTOR_401300_PATROL_ARRIVAL_RADIUS  = 160,
        ACTOR_401300_PATROL_TURN_LIMIT      = 32,
        ACTOR_401300_PATROL_STEP            = 10,
        ACTOR_401300_PATROL_NOTICE_RADIUS   = 2000,
        ACTOR_401300_PATROL_FACING_RADIUS   = 4000,
        ACTOR_401300_PATROL_FACING_LIMIT    = 768,
    };
    _Actor401300Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    ActorTurnScratch* patrol;
    GfxCoord*         headingRoot;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_WALK;
        work->jointPairTarget   = ACTOR_401300_PATROL_PAIR_HIGH_BLEND;
        work->jointPairStep     = ACTOR_401300_PATROL_PAIR_STEP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == ACTOR_401300_PATROL_PAIR_HIGH_BLEND) {
            work->jointPairTarget = ACTOR_401300_PATROL_PAIR_LOW_BLEND;
        } else {
            work->jointPairTarget = ACTOR_401300_PATROL_PAIR_HIGH_BLEND;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    patrol           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    patrol->delta.vx = work->patrolPoints[work->patrolTarget].x - actor->extra.tmd->coords->coord.t[0];
    patrol->delta.vy = 0;
    patrol->delta.vz = work->patrolPoints[work->patrolTarget].z - actor->extra.tmd->coords->coord.t[2];
    // Switch endpoints without refreshing the staged offset this tick.
    if (!_actorRangeOutsideRadiusXZ(&patrol->delta, ACTOR_401300_PATROL_ARRIVAL_RADIUS)) {
        if (work->patrolTarget == 0) {
            work->patrolTarget = 1;
        } else {
            work->patrolTarget = 0;
        }
    }
    _actor401300UpdateAnimationEffects(actor);
    rootCoord           = actor->extra.tmd->coords;
    patrol->angle       = _actorAngleNormalizeYaw(ratan2(patrol->delta.vx, patrol->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    work->lookYawTarget = patrol->angle;
    ACTOR_401300_TURN_PATROL_ROOT(actor, patrol, headingRoot, ACTOR_401300_PATROL_TURN_LIMIT);
    if (work->blendActive == 0) {
        if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_PATROL_STEP) != 0) {
            _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_PATROL_STEP);
        }
    }
    if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
        _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &patrol->delta);
    if (!_actorRangeOutsideRadiusXZ(&patrol->delta, ACTOR_401300_PATROL_NOTICE_RADIUS)) {
        work->state = ACTOR_401300_STATE_ALERT;
    } else if (!_actorRangeOutsideRadiusXZ(&patrol->delta, ACTOR_401300_PATROL_FACING_RADIUS)) {
        rootCoord     = actor->extra.tmd->coords;
        patrol->angle = _actorAngleNormalizeYaw(ratan2(patrol->delta.vx, patrol->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
        if (ABS(patrol->angle) < ACTOR_401300_PATROL_FACING_LIMIT) {
            work->state = ACTOR_401300_STATE_ALERT;
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

#undef ACTOR_401300_TURN_PATROL_ROOT

/// Limits the slide turn and replaces the model root heading.
///
/// Requires live actor/model storage and reserved ActorTurnScratch with a signed
/// relative yaw; turnLimit is positive in 4096ths of a turn. Translation stays
/// intact; the scratch yaw becomes absolute. Leaves the root unscaled.
/// Arguments must be side-effect-free: pointers and limit occur repeatedly.
/// headingRoot must be a writable GfxCoord* local. Expands to one block.
#define ACTOR_401300_TURN_SLIDE_ROOT(actor, scratch, headingRoot, turnLimit)                            \
    {                                                                                                   \
        if ((scratch)->angle > (turnLimit)) {                                                           \
            (scratch)->angle = (turnLimit);                                                             \
        }                                                                                               \
        if ((scratch)->angle < -(turnLimit)) {                                                          \
            (scratch)->angle = -(turnLimit);                                                            \
        }                                                                                               \
        (headingRoot)     = (actor)->extra.tmd->coords;                                                 \
        (scratch)->angle += ratan2(-(headingRoot)->coord.m[2][0], (headingRoot)->coord.m[2][2]);        \
        gfxRotMatrixY(&(actor)->extra.tmd->coords->coord, (scratch)->angle, GRAPHICS_ROTATION_REPLACE); \
    }

/// Coasts toward the player by the decaying slide step, then turns around.
///
/// Requires live actor/Enemy/player roots, bound rigs and initialized slideStep.
/// No path in this package seeds the step or selects this state. Turns at most
/// 64/4096 per tick and translates in parent-coordinate units. A positive step
/// loses 10 each tick with signed-halfword saturation at zero; a nonpositive
/// step is retained. A settled clip or zero step selects TURN_AROUND. This yaw
/// replacement leaves the root unscaled. Releases its turn scratch block.
static void _actor401300StateSlide(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_SLIDE           = 18,
        ACTOR_401300_SLIDE_ANIMATION_RATE = 30,
        ACTOR_401300_SLIDE_TURN_LIMIT     = 64,
        ACTOR_401300_SLIDE_DECELERATION   = 10,
    };
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    ActorTurnScratch* turn;
    u16               nextStep;

    work = actor->work;
    if (work->stateEntered != 0) {
        enemy             = actor->spawnArg2.pointer;
        model             = actor->extra.tmd;
        work->animId      = ACTOR_401300_ANIM_SLIDE;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        model->flags      = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ACTOR_401300_SLIDE_ANIMATION_RATE;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn                = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle         = _actorAngleTurnToPlayer(actor, &turn->delta, &gPlayerStatus);
    work->lookYawTarget = turn->angle;
    ACTOR_401300_TURN_SLIDE_ROOT(actor, turn, rootCoord, ACTOR_401300_SLIDE_TURN_LIMIT);
    if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
        _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, work->slideStep) != 0) {
        _actorMovementTranslateForwardNonzero(actor->extra.tmd->coords, work->slideStep);
    }
    // Preserve the unsigned intermediate and signed-halfword zero clamp.
    if (work->slideStep > 0) {
        nextStep        = work->slideStep - ACTOR_401300_SLIDE_DECELERATION;
        work->slideStep = nextStep;
        if ((s16)nextStep < 0) {
            work->slideStep = 0;
        }
    }
    _actor401300UpdateAnimationEffects(actor);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) || work->slideStep == 0) {
        work->state = ACTOR_401300_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

#undef ACTOR_401300_TURN_SLIDE_ROOT

/// Limits the backoff turn and replaces the model root heading.
///
/// Positive turns are clamped then halved; turns below the negative limit
/// retain that full limit. The remaining turns are arithmetically halved.
/// Requires live actor/model storage and reserved ActorChaseScratch with a signed
/// relative yaw; turnLimit is positive in 4096ths of a turn. Translation stays
/// intact; the scratch yaw becomes absolute. Restores the uniform root scale.
/// Arguments must be side-effect-free: pointers and limit occur repeatedly.
/// headingRoot must be a writable GfxCoord* local. Expands to one block.
#define ACTOR_401300_TURN_BACK_OFF_ROOT(actor, scratch, headingRoot, turnLimit)                        \
    {                                                                                                  \
        if ((scratch)->turn > (turnLimit)) {                                                           \
            (scratch)->turn = (turnLimit);                                                             \
        }                                                                                              \
        if ((scratch)->turn < -(turnLimit)) {                                                          \
            (scratch)->turn = -(turnLimit);                                                            \
        } else {                                                                                       \
            (scratch)->turn = (scratch)->turn >> 1;                                                    \
        }                                                                                              \
        (headingRoot)    = (actor)->extra.tmd->coords;                                                 \
        (scratch)->turn += ratan2(-(headingRoot)->coord.m[2][0], (headingRoot)->coord.m[2][2]);        \
        gfxRotMatrixY(&(actor)->extra.tmd->coords->coord, (scratch)->turn, GRAPHICS_ROTATION_REPLACE); \
        _actorRenderRescaleYaw((actor)->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);                   \
    }

/// Faces the player, backs away for nineteen ticks, then turns aside and chases.
///
/// Requires live actor/Enemy/player roots and loaded rigs. No path selects it
/// here. Alignment within 128 angle units starts the retreat clip. The turn
/// clamps positive values to 128 then halves them; values below -128 keep the
/// full negative clamp. Retreat moves -16 parent-coordinate units per tick;
/// exit composes a relative +/-1200 yaw (4096 units per turn). Releases its
/// chase scratch block plus nested movement/contact workspace.
static void _actor401300StateBackOff(Task* actor)
{
    enum {
        ACTOR_401300_BACK_OFF_RATE       = 22,
        ACTOR_401300_ANIM_BACK_OFF       = 17,
        ACTOR_401300_BACK_OFF_TURN_LIMIT = 128,
        ACTOR_401300_BACK_OFF_STEP       = -16,
        ACTOR_401300_BACK_OFF_TICKS      = 19,
        ACTOR_401300_BACK_OFF_EXIT_TURN  = 1200,
    };
    _Actor401300Work*  work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* retreat;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ACTOR_401300_BACK_OFF_RATE;
        work->animId            = ACTOR_401300_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        return;
    }
    _actor401300UpdateAnimationEffects(actor);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    retreat             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    retreat->turn       = _actorAngleTurnToPlayer(actor, &retreat->delta, &gPlayerStatus);
    work->lookYawTarget = retreat->turn;
    if (ABS(retreat->turn) <= ACTOR_401300_BACK_OFF_TURN_LIMIT && work->animId == ACTOR_401300_ANIM_WALK) {
        work->animRate    = ACTOR_401300_BACK_OFF_RATE;
        work->animId      = ACTOR_401300_ANIM_BACK_OFF;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        _actor401300UpdateAnimationEffects(actor);
    }
    ACTOR_401300_TURN_BACK_OFF_ROOT(actor, retreat, rootCoord, ACTOR_401300_BACK_OFF_TURN_LIMIT);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ACTOR_401300_ANIM_BACK_OFF) {
        work->stateTimer++;
        if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_BACK_OFF_STEP) != 0) {
            _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_BACK_OFF_STEP);
        }
        if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
            _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->stateTimer >= ACTOR_401300_BACK_OFF_TICKS) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&actor->extra.tmd->coords->coord, ACTOR_401300_BACK_OFF_EXIT_TURN, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixY(&actor->extra.tmd->coords->coord, -ACTOR_401300_BACK_OFF_EXIT_TURN, GRAPHICS_ROTATION_COMPOSE);
            }
            work->state = ACTOR_401300_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ACTOR_401300_TURN_BACK_OFF_ROOT

/// Limits the grabwindup turn and replaces the model root heading.
///
/// Requires live actor/model storage and reserved ActorChaseScratch with a signed
/// relative yaw; turnLimit is positive in 4096ths of a turn. Translation stays
/// intact; the scratch yaw becomes absolute. Restores the uniform root scale.
/// Arguments must be side-effect-free: pointers and limit occur repeatedly.
/// headingRoot must be a writable GfxCoord* local. Expands to one block.
#define ACTOR_401300_TURN_GRAB_WINDUP_ROOT(actor, scratch, headingRoot, turnLimit)                     \
    {                                                                                                  \
        if ((scratch)->turn > (turnLimit)) {                                                           \
            (scratch)->turn = (turnLimit);                                                             \
        }                                                                                              \
        if ((scratch)->turn < -(turnLimit)) {                                                          \
            (scratch)->turn = -(turnLimit);                                                            \
        }                                                                                              \
        (headingRoot)    = (actor)->extra.tmd->coords;                                                 \
        (scratch)->turn += ratan2(-(headingRoot)->coord.m[2][0], (headingRoot)->coord.m[2][2]);        \
        gfxRotMatrixY(&(actor)->extra.tmd->coords->coord, (scratch)->turn, GRAPHICS_ROTATION_REPLACE); \
        _actorRenderRescaleYaw((actor)->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);                   \
    }

/// Turns toward the player for up to eleven ticks before selecting the grab.
///
/// Requires live actor/Enemy/player roots and loaded rigs; no path selects it
/// here. Entry disables root grid collision and starts clip 19. Subsequent
/// ticks replace yaw by at most 32/4096 of a turn and restore the model scale.
/// The settled flag is tested before playback advances, or tick 11 starts GRAB.
/// Releases its chase scratch block and nested yaw workspace.
static void _actor401300StateGrabWindup(Task* actor)
{
    enum {
        ACTOR_401300_ANIM_GRAB_WINDUP       = 19,
        ACTOR_401300_GRAB_WINDUP_TICKS      = 11,
        ACTOR_401300_GRAB_WINDUP_TURN_LIMIT = 32,
    };
    _Actor401300Work*  work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* windup;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_GRAB_WINDUP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor401300UpdateAnimationEffects(actor);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    windup                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) || work->stateTimer >= ACTOR_401300_GRAB_WINDUP_TICKS) {
        work->state = ACTOR_401300_STATE_GRAB;
    }
    windup->turn        = _actorAngleTurnToPlayer(actor, &windup->delta, &gPlayerStatus);
    work->lookYawTarget = windup->turn;
    ACTOR_401300_TURN_GRAB_WINDUP_ROOT(actor, windup, rootCoord, ACTOR_401300_GRAB_WINDUP_TURN_LIMIT);
    _actor401300UpdateAnimationEffects(actor);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ACTOR_401300_TURN_GRAB_WINDUP_ROOT

/// Adds an X bend to one joint and refreshes the separately selected coordinate.
///
/// Requires both part indices in the live model array. Angles use 4096 units
/// per turn. Arguments must be side-effect-free: actor occurs three times,
/// composedPart twice, bentPart/bendAngle once. Expands to one statement block;
/// composition may walk parents. Undefined after its only state handler.
#define ACTOR_401300_COMPOSE_JOINT_BEND(actor, bentPart, composedPart, bendAngle)                           \
    {                                                                                                       \
        gfxRotMatrixX(&(actor)->extra.tmd->coords[bentPart].coord, (bendAngle), GRAPHICS_ROTATION_COMPOSE); \
        (actor)->extra.tmd->coords[composedPart].composeStamp = GRAPHICS_COORD_DIRTY;                       \
        actorRenderComposeCoord(&(actor)->extra.tmd->coords[composedPart]);                                 \
    }

/// Bends the upper joints, then straightens while turning back toward the player.
///
/// Requires live actor/Enemy storage, loaded rigs and coordinates through part 5.
/// Entry advances clip 19 twice at half rate and disables root grid collision.
/// Ticks 1..49 hold the bends; later ticks halve each bend every four ticks,
/// with staggered starting ticks. Yaws use 4096 units per turn. Alignment
/// within 36 units selects CHASE. Borrows one chase block and nested yaw scratch.
static void _actor401300StateBendOver(Task* actor)
{
    enum {
        ACTOR_401300_BEND_ANIM            = 19,
        ACTOR_401300_BEND_RATE            = ANIMATION_RATE_ONE / 2,
        ACTOR_401300_BEND_LOOK_STEP       = 40,
        ACTOR_401300_BEND_STRAIGHTEN_TICK = 50,
        ACTOR_401300_BEND_TURN_STEP       = 36,
        ACTOR_401300_BEND_SMALL_ANGLE     = 64,
        ACTOR_401300_BEND_MIDDLE_ANGLE    = 128,
        ACTOR_401300_BEND_LARGE_ANGLE     = 256,
        ACTOR_401300_BEND_DECAY_TICKS     = 4,
    };
    _Actor401300Work*  work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* bendScratch;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate          = ACTOR_401300_BEND_RATE;
        work->animId            = ACTOR_401300_BEND_ANIM;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor401300UpdateAnimationEffects(actor);
        _actor401300UpdateAnimationEffects(actor);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    bendScratch       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    bendScratch->turn = _actorAngleTurnToPlayer(actor, &bendScratch->delta, &gPlayerStatus);
    if (work->lookYawTarget < bendScratch->turn) {
        if (bendScratch->turn - work->lookYawTarget > ACTOR_401300_BEND_LOOK_STEP) {
            work->lookYawTarget += ACTOR_401300_BEND_LOOK_STEP;
        } else {
            work->lookYawTarget = bendScratch->turn;
        }
    } else if (work->lookYawTarget - bendScratch->turn > ACTOR_401300_BEND_LOOK_STEP) {
        work->lookYawTarget -= ACTOR_401300_BEND_LOOK_STEP;
    } else {
        work->lookYawTarget = bendScratch->turn;
    }
    rootCoord         = actor->extra.tmd->coords;
    bendScratch->turn = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, bendScratch->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    _actor401300UpdateAnimationEffects(actor);
    // Part 5 is bent, but the retained final invalidation/composition targets part 4.
    if (work->stateTimer < ACTOR_401300_BEND_STRAIGHTEN_TICK) {
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 1, 1, ACTOR_401300_BEND_SMALL_ANGLE);
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 2, 2, ACTOR_401300_BEND_MIDDLE_ANGLE);
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 3, 3, ACTOR_401300_BEND_MIDDLE_ANGLE);
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 4, 4, ACTOR_401300_BEND_MIDDLE_ANGLE);
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 5, 4, ACTOR_401300_BEND_LARGE_ANGLE);
    } else {
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 1, 1, ACTOR_401300_BEND_SMALL_ANGLE >> ((work->stateTimer - (ACTOR_401300_BEND_STRAIGHTEN_TICK - 1)) / ACTOR_401300_BEND_DECAY_TICKS));
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 2, 2, ACTOR_401300_BEND_MIDDLE_ANGLE >> ((work->stateTimer - (ACTOR_401300_BEND_STRAIGHTEN_TICK - 2)) / ACTOR_401300_BEND_DECAY_TICKS));
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 3, 3, ACTOR_401300_BEND_MIDDLE_ANGLE >> ((work->stateTimer - (ACTOR_401300_BEND_STRAIGHTEN_TICK - 3)) / ACTOR_401300_BEND_DECAY_TICKS));
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 4, 4, ACTOR_401300_BEND_MIDDLE_ANGLE >> ((work->stateTimer - (ACTOR_401300_BEND_STRAIGHTEN_TICK - 4)) / ACTOR_401300_BEND_DECAY_TICKS));
        ACTOR_401300_COMPOSE_JOINT_BEND(actor, 5, 4, ACTOR_401300_BEND_LARGE_ANGLE >> ((work->stateTimer - (ACTOR_401300_BEND_STRAIGHTEN_TICK - 1)) / ACTOR_401300_BEND_DECAY_TICKS));
        bendScratch->turn = _actorAngleTurnToPlayer(actor, &bendScratch->delta, &gPlayerStatus);
        if (bendScratch->turn > ACTOR_401300_BEND_TURN_STEP) {
            bendScratch->turn = ACTOR_401300_BEND_TURN_STEP;
        } else if (bendScratch->turn < -ACTOR_401300_BEND_TURN_STEP) {
            bendScratch->turn = -ACTOR_401300_BEND_TURN_STEP;
        }
        if (ABS(bendScratch->turn) < ACTOR_401300_BEND_TURN_STEP) {
            work->state = ACTOR_401300_STATE_CHASE;
        }
        rootCoord          = actor->extra.tmd->coords;
        bendScratch->turn += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
        gfxRotMatrixY(&actor->extra.tmd->coords->coord, bendScratch->turn, GRAPHICS_ROTATION_REPLACE);
        _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ACTOR_401300_COMPOSE_JOINT_BEND

/// Hides the body, bursts four detached models and waits for the player release.
///
/// Requires live work/Enemy/model coordinates through part 12 and loaded effects.
/// Entry starts a size-768 gravity particle in mode 1. Ticks 3/5/7/8 select
/// bank-10 slot 5 models and spawn size-512 debris with placement textures.
/// The model descriptor is shared and must be set immediately before spawning.
/// Tick 61 or later selects DEAD once playerHeld clears. Offset XYZ is copied
/// synchronously; tick 5 retains the stack vector's unwritten Z component.
static void _actor401300StateDeathBurst(Task* actor)
{
    enum {
        ACTOR_401300_BURST_PARTICLE_ARG = 0x10300,
        ACTOR_401300_BURST_PART_SIZE    = 512,
        ACTOR_401300_BURST_OFFSET       = 100,
        ACTOR_401300_BURST_MODEL_SLOT   = 5,
        ACTOR_401300_BURST_FIRST_TICK   = 3,
        ACTOR_401300_BURST_SECOND_TICK  = 5,
        ACTOR_401300_BURST_BODY_TICK    = 7,
        ACTOR_401300_BURST_HEAD_TICK    = 8,
        ACTOR_401300_BURST_DEAD_TICK    = 61,
    };
    SVECTOR           effectOffset;
    _Actor401300Work* work;
    Enemy*            enemy;
    u16               nextTick;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->gridBody.flags          = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->attackBody.flags        = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        effectOffset.vx               = ACTOR_401300_BURST_OFFSET;
        effectOffset.vz               = 0;
        effectOffset.vy               = 0;
        effectSpawn(EFFECT_030, actor->extra.tmd->coords + 1, ACTOR_401300_BURST_PARTICLE_ARG, &effectOffset);
    }
    nextTick         = work->stateTimer + 1;
    work->stateTimer = nextTick;
    if ((s16)nextTick == ACTOR_401300_BURST_FIRST_TICK) {
        D_80114B34[ACTOR_401300_BURST_MODEL_SLOT].data.model = &_gActor401300HornedStrangerEffect1;
        effectOffset.vz                                      = ACTOR_401300_BURST_OFFSET;
        effectOffset.vy                                      = 0;
        effectOffset.vx                                      = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, actor->extra.tmd->coords + 9, ACTOR_401300_BURST_PART_SIZE, &effectOffset), enemy);
    }
    if (work->stateTimer == ACTOR_401300_BURST_SECOND_TICK) {
        D_80114B34[ACTOR_401300_BURST_MODEL_SLOT].data.model = &_gActor401300HornedStrangerEffect1;
        effectOffset.vy                                      = 0;
        effectOffset.vx                                      = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, actor->extra.tmd->coords + 12, ACTOR_401300_BURST_PART_SIZE, &effectOffset), enemy);
    }
    if (work->stateTimer == ACTOR_401300_BURST_BODY_TICK) {
        D_80114B34[ACTOR_401300_BURST_MODEL_SLOT].data.model = &_gActor401300HornedStrangerEffect2;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, actor->extra.tmd->coords + 1, ACTOR_401300_BURST_PART_SIZE, NULL), enemy);
    }
    if (work->stateTimer == ACTOR_401300_BURST_HEAD_TICK) {
        D_80114B34[ACTOR_401300_BURST_MODEL_SLOT].data.model = &_gActor401300HornedStrangerBurstHead;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, actor->extra.tmd->coords + 3, ACTOR_401300_BURST_PART_SIZE, NULL), enemy);
    }
    if (work->stateTimer >= ACTOR_401300_BURST_DEAD_TICK && work->playerHeld == 0) {
        work->state = ACTOR_401300_STATE_DEAD;
    }
}

/// Collapses the burst-walk body's parts 2..12 after animation restores their pose.
///
/// Requires live coordinates through part 12 and nested yaw scratch. Each basis
/// keeps its heading and translation with Q12 scale 1, in ascending part order.
static __inline__ void _actor401300CollapseBurstParts(Task* actor)
{
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 2);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 3);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 4);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 5);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 6);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 7);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 8);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 9);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 10);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 11);
    _actorRenderCollapseYawRotation(actor->extra.tmd->coords + 12);
}

/// Walks through two debris spawns, then burns, flattens, fades and hides the corpse.
///
/// Requires live work/Enemy/model coordinates through part 12 and loaded effects.
/// Walk ticks 3/5 burst models; a control jump from tick 16 starts clip 35.
/// Its timer stays zero until the clip settles, then ticks 30/42/48/64 burn,
/// blacken, enable translucency and select DEAD. From tick 26 root Y scale
/// loses 16 Q12 units per tick. Parts 2..12 collapse after animation each tick.
/// The work-owned burn anchor must outlive the spawned effect.
static void _actor401300StateDeathBurstWalk(Task* actor)
{
    enum {
        ACTOR_401300_BURST_WALK_PARTICLE_ARG = 0x10300,
        ACTOR_401300_BURST_WALK_PART_SIZE    = 512,
        ACTOR_401300_BURST_WALK_OFFSET       = 100,
        ACTOR_401300_BURST_WALK_MODEL_SLOT   = 5,
        ACTOR_401300_BURST_WALK_FIRST_TICK   = 3,
        ACTOR_401300_BURST_WALK_SECOND_TICK  = 5,
        ACTOR_401300_BURST_WALK_FINISH_TICK  = 16,
        ACTOR_401300_BURST_WALK_FINAL_ANIM   = 35,
        ACTOR_401300_BURST_WALK_STEP         = 10,
        ACTOR_401300_BURST_WALK_BURN_TICK    = 30,
        ACTOR_401300_BURST_WALK_BLACK_TICK   = 42,
        ACTOR_401300_BURST_WALK_FADE_TICK    = 48,
        ACTOR_401300_BURST_WALK_HIDE_TICK    = 64,
        ACTOR_401300_BURST_WALK_FLATTEN_TICK = 26,
        ACTOR_401300_BURST_WALK_FLATTEN_BASE = 20,
        ACTOR_401300_BURST_WALK_SCALE_STEP   = 16,
        ACTOR_401300_BURST_WALK_BURN_COUNT   = 2,
    };
    SVECTOR           effectPosition;
    _Actor401300Work* work;
    Enemy*            enemy;
    u16               nextTick;
    s16               burnTick;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->gridBody.flags          = work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED;
        work->attackBody.flags        = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        effectPosition.vx             = ACTOR_401300_BURST_WALK_OFFSET;
        effectPosition.vz             = 0;
        effectPosition.vy             = 0;
        work->animId                  = ACTOR_401300_ANIM_WALK;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                = ANIMATION_RATE_ONE;
        effectSpawn(EFFECT_030, actor->extra.tmd->coords + 1, ACTOR_401300_BURST_WALK_PARTICLE_ARG, &effectPosition);
        work->stateTimer = 0;
    }
    nextTick         = work->stateTimer + 1;
    work->stateTimer = nextTick;
    // Burn events count only ticks spent holding the final pose.
    switch (work->animId) {
        case ACTOR_401300_ANIM_WALK:
            if ((s16)nextTick >= ACTOR_401300_BURST_WALK_FINISH_TICK && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
                work->animId      = ACTOR_401300_BURST_WALK_FINAL_ANIM;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->animRate    = ANIMATION_RATE_ONE;
                work->blendActive = 0;
            }
            if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_BURST_WALK_STEP) != 0) {
                _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_BURST_WALK_STEP);
            }
            _actorContactApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
            if (work->stateTimer == ACTOR_401300_BURST_WALK_FIRST_TICK) {
                D_80114B34[ACTOR_401300_BURST_WALK_MODEL_SLOT].data.model = &_gActor401300HornedStrangerBurstHead;
                effectPosition.vz                                         = ACTOR_401300_BURST_WALK_OFFSET;
                effectPosition.vy                                         = 0;
                effectPosition.vx                                         = 0;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, actor->extra.tmd->coords + 9, ACTOR_401300_BURST_WALK_PART_SIZE, &effectPosition), enemy);
            }
            if (work->stateTimer == ACTOR_401300_BURST_WALK_SECOND_TICK) {
                D_80114B34[ACTOR_401300_BURST_WALK_MODEL_SLOT].data.model = &_gActor401300HornedStrangerEffect2;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, actor->extra.tmd->coords + 1, ACTOR_401300_BURST_WALK_PART_SIZE, NULL), enemy);
            }
            break;
        case ACTOR_401300_BURST_WALK_FINAL_ANIM:
            if (!(work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                work->stateTimer = 0;
            }
            switch (work->stateTimer) {
                // Keep the empty tick-3 branch: it determines the original switch layout.
                case 3:
                    break;
                case ACTOR_401300_BURST_WALK_BURN_TICK:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    // Reuse the effect offset storage for the burn anchor's world origin.
                    effectPosition.vx = 0;
                    effectPosition.vy = 0;
                    effectPosition.vz = 0;
                    _actorRenderTransformToWorld(actor->extra.tmd->coords + 2, &effectPosition);
                    work->burnCoord.parent       = &gGfxViewCoord;
                    work->burnCoord.coord.t[0]   = effectPosition.vx;
                    work->burnCoord.coord.t[1]   = actor->extra.tmd->coords->coord.t[1];
                    work->burnCoord.coord.t[2]   = effectPosition.vz;
                    work->burnCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(&work->burnCoord);
                    effectSpawn(EFFECT_CORPSE_BURN, &work->burnCoord, ACTOR_401300_BURST_WALK_BURN_COUNT, NULL);
                    break;
                case ACTOR_401300_BURST_WALK_FADE_TICK:
                    actor->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case ACTOR_401300_BURST_WALK_BLACK_TICK:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ACTOR_401300_BURST_WALK_HIDE_TICK:
                    actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->state             = ACTOR_401300_STATE_DEAD;
                    break;
            }
            burnTick = work->stateTimer;
            if (burnTick >= ACTOR_401300_BURST_WALK_FLATTEN_TICK) {
                _actorRenderRescaleYawXZ(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE, ACTOR_401300_ROOT_SCALE - (burnTick - ACTOR_401300_BURST_WALK_FLATTEN_BASE) * ACTOR_401300_BURST_WALK_SCALE_STEP);
            }
            break;
    }
    _actor401300UpdateAnimationEffects(actor);
    _actor401300CollapseBurstParts(actor);
}

/// Refreshes the stalking offset and records the player heading and reverse bearing.
///
/// Requires live roots in the same parent frame and a reserved chase block at
/// savedCursor - 1, also addressed by stalk. XYZ narrows to signed halfwords;
/// the reverse bearing is stored before normalization to [-2048, 2048] in
/// 4096 angle units per turn. The actor state does not consume these two yaws.
static __inline__ void _actor401300ReadStalkPlayerBearings(Task* actor, const PlayerStatus* playerStatus, ActorChaseScratch* savedCursor, ActorChaseScratch* stalk)
{
    GfxCoord* updatedRoot;
    s16       yawFromPlayer;

    stalk->playerYaw         = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    updatedRoot              = actor->extra.tmd->coords;
    savedCursor[-1].delta.vx = playerStatus->coordMtx->t[0] - updatedRoot->coord.t[0];
    stalk->delta.vy          = playerStatus->coordMtx->t[1] - updatedRoot->coord.t[1];
    stalk->delta.vz          = playerStatus->coordMtx->t[2] - updatedRoot->coord.t[2];
    yawFromPlayer            = ratan2(savedCursor[-1].delta.vx, stalk->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    stalk->yawFromPlayer     = yawFromPlayer;
    stalk->yawFromPlayer     = _actorAngleNormalizeYaw(yawFromPlayer);
}

/// Walks toward the player, steering until close enough to begin a grab.
///
/// Requires live Enemy/model/work and player roots in the same parent space.
/// Walk rate is 36/16 frames per tick; forward steps are 22 game units, or 5
/// while blending, subject to player reach and actor freeze. Turn is capped
/// at 32/4096 of a revolution per tick. The grab gate retains a signed turn
/// below 512, including negative turns, and XZ range at most 1100. Borrows
/// a chase block; the player-facing yaws are recorded but not read here.
static void _actor401300StateStalk(Task* actor)
{
    enum {
        ACTOR_401300_STALK_ANIM_RATE   = 36,
        ACTOR_401300_STALK_GRAB_ANGLE  = 512,
        ACTOR_401300_STALK_GRAB_RADIUS = 1100,
        ACTOR_401300_STALK_TURN_STEP   = 32,
        ACTOR_401300_STALK_STEP        = 22,
        ACTOR_401300_STALK_BLEND_STEP  = 5,
    };
    _Actor401300Work*  work;
    TmdObject*         model;
    GfxCoord*          turnRoot;
    GfxCoord*          facingRoot;
    GfxCoord*          offsetRoot;
    PlayerStatus*      playerStatus;
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* stalk;
    s32                playerBearing;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ACTOR_401300_STALK_ANIM_RATE;
        work->animId            = ACTOR_401300_ANIM_WALK;
        work->blendActive       = 0;
        work->field_D1C         = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        sceneEngageBattle(1);
        work->stateTimer    = 0;
        work->blockedFrames = 0;
    }
    work->stateTimer++;
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    stalk                                   = savedCursor - 1;
    if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
        _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    playerStatus                           = &gPlayerStatus;
    offsetRoot                             = actor->extra.tmd->coords;
    savedCursor[-1].delta.vx               = playerStatus->coordMtx->t[0] - offsetRoot->coord.t[0];
    stalk->delta.vy                        = playerStatus->coordMtx->t[1] - offsetRoot->coord.t[1];
    stalk->delta.vz                        = playerStatus->coordMtx->t[2] - offsetRoot->coord.t[2];
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor401300UpdateAnimationEffects(actor);
    _actor401300ReadStalkPlayerBearings(actor, playerStatus, savedCursor, stalk);
    turnRoot            = actor->extra.tmd->coords;
    playerBearing       = ratan2(stalk->delta.vx, stalk->delta.vz);
    stalk->turn         = _actorAngleNormalizeYaw(playerBearing - ratan2(-turnRoot->coord.m[2][0], turnRoot->coord.m[2][2]));
    work->lookYawTarget = stalk->turn;
    // The original grab gate uses a signed comparison, not an absolute angle.
    if (stalk->turn < ACTOR_401300_STALK_GRAB_ANGLE) {
        if (!_actorRangeOutsideRadiusXZ(&stalk->delta, ACTOR_401300_STALK_GRAB_RADIUS)) {
            work->state = ACTOR_401300_STATE_GRAB;
        }
    }
    if (stalk->turn > ACTOR_401300_STALK_TURN_STEP) {
        stalk->turn = ACTOR_401300_STALK_TURN_STEP;
    }
    if (stalk->turn < -ACTOR_401300_STALK_TURN_STEP) {
        stalk->turn = -ACTOR_401300_STALK_TURN_STEP;
    }
    facingRoot   = actor->extra.tmd->coords;
    stalk->turn += ratan2(-facingRoot->coord.m[2][0], facingRoot->coord.m[2][2]);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, stalk->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ACTOR_401300_ANIM_WALK) {
        if (work->blendActive == 0) {
            if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_STALK_STEP) != 0) {
                _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_STALK_STEP);
            }
        } else {
            if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_STALK_BLEND_STEP) != 0) {
                _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_STALK_BLEND_STEP);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->animId      = ACTOR_401300_ANIM_WALK;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Steers a strike's root toward the player by at most 48 angle units per tick.
///
/// Requires a reserved chase block containing the parent-space player offset,
/// initialized work and a live model root. Records the full wrapped look turn
/// before clamping the root step, then restores the normal Q12 scale. Angles
/// use 4096 units per turn; the caller limits tracking to the windup ticks.
static __inline__ void _actor401300TrackStrikePlayer(Task* actor, _Actor401300Work* work, ActorChaseScratch* strike)
{
    GfxCoord* turnRoot;
    GfxCoord* facingRoot;
    s32       playerBearing;

    turnRoot            = actor->extra.tmd->coords;
    playerBearing       = ratan2(strike->delta.vx, strike->delta.vz);
    strike->turn        = _actorAngleNormalizeYaw(playerBearing - ratan2(-turnRoot->coord.m[2][0], turnRoot->coord.m[2][2]));
    work->lookYawTarget = strike->turn;
    if (strike->turn > ACTOR_401300_RUN_TURN_LIMIT) {
        strike->turn = ACTOR_401300_RUN_TURN_LIMIT;
    }
    if (strike->turn < -ACTOR_401300_RUN_TURN_LIMIT) {
        strike->turn = -ACTOR_401300_RUN_TURN_LIMIT;
    }
    facingRoot    = actor->extra.tmd->coords;
    strike->turn += ratan2(-facingRoot->coord.m[2][0], facingRoot->coord.m[2][2]);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, strike->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
}

/// Tracks the player during strike A and enables attack 0 for ticks 25..39.
///
/// Requires live work/Enemy, loaded strike clip and the part-3 attack body.
/// Entry sets the enemy attack key and fixed-joint target; tracking on ticks
/// 1..13 turns by at most 48/4096 of a revolution. The joint pair relaxes
/// from tick 42, and a settled clip selects ALERT. Uses the actor room-height
/// clamp for grid correction. Borrows/releases one chase scratch block.
static void _actor401300StateStrikeA(Task* actor)
{
    enum {
        ACTOR_401300_STRIKE_ATTACK_INDEX    = 0,
        ACTOR_401300_STRIKE_ACTIVE_TICK     = 25,
        ACTOR_401300_STRIKE_END_TICK        = 40,
        ACTOR_401300_STRIKE_RELAX_TICK      = 41,
        ACTOR_401300_STRIKE_TRACK_END_TICK  = 14,
        ACTOR_401300_STRIKE_PAIR_TARGET     = 512,
        ACTOR_401300_STRIKE_PAIR_STEP       = 128,
        ACTOR_401300_STRIKE_PAIR_RELAX_STEP = 64,
    };
    _Actor401300Work*  work;
    Enemy*             enemy;
    TmdObject*         model;
    ActorChaseScratch* strike;

    work = actor->work;
    if (work->stateEntered != 0) {
        enemy                         = actor->spawnArg2.pointer;
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_STRIKE_A;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->attackBody.key  = damagePackEnemyAttackKey(enemy, ACTOR_401300_STRIKE_ATTACK_INDEX);
        work->jointPairTarget = ACTOR_401300_STRIKE_PAIR_TARGET;
        work->jointPairStep   = ACTOR_401300_STRIKE_PAIR_STEP;
        return;
    }
    _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
    if (work->stateTimer >= ACTOR_401300_STRIKE_RELAX_TICK) {
        work->jointPairTarget = 0;
        work->jointPairStep   = ACTOR_401300_STRIKE_PAIR_RELAX_STEP;
    }
    work->stateTimer++;
    // The attack sphere is live only during the swing window.
    switch (work->stateTimer) {
        case ACTOR_401300_STRIKE_ACTIVE_TICK:
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case ACTOR_401300_STRIKE_END_TICK:
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    strike = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &strike->delta);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor401300UpdateAnimationEffects(actor);
    if (work->stateTimer < ACTOR_401300_STRIKE_TRACK_END_TICK) {
        _actor401300TrackStrikePlayer(actor, work, strike);
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Tracks the player during strike B and enables attack 1 for ticks 21..36.
///
/// Requires live work/Enemy, loaded strike clip and the part-3 attack body.
/// Entry sets the enemy attack key and fixed-joint target; tracking on ticks
/// 1..13 turns by at most 48/4096 of a revolution. The joint pair relaxes
/// from tick 39, and a settled clip selects ALERT. Uses generic grid correction
/// without the actor room-height clamp. Borrows/releases one chase scratch block.
static void _actor401300StateStrikeB(Task* actor)
{
    enum {
        ACTOR_401300_STRIKE_ATTACK_INDEX    = 1,
        ACTOR_401300_STRIKE_ACTIVE_TICK     = 21,
        ACTOR_401300_STRIKE_END_TICK        = 37,
        ACTOR_401300_STRIKE_RELAX_TICK      = 38,
        ACTOR_401300_STRIKE_TRACK_END_TICK  = 14,
        ACTOR_401300_STRIKE_PAIR_TARGET     = 512,
        ACTOR_401300_STRIKE_PAIR_STEP       = 128,
        ACTOR_401300_STRIKE_PAIR_RELAX_STEP = 64,
    };
    _Actor401300Work*  work;
    Enemy*             enemy;
    TmdObject*         model;
    ActorChaseScratch* strike;

    work = actor->work;
    if (work->stateEntered != 0) {
        enemy                         = actor->spawnArg2.pointer;
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_STRIKE_B;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->attackBody.key  = damagePackEnemyAttackKey(enemy, ACTOR_401300_STRIKE_ATTACK_INDEX);
        work->jointPairTarget = ACTOR_401300_STRIKE_PAIR_TARGET;
        work->jointPairStep   = ACTOR_401300_STRIKE_PAIR_STEP;
        return;
    }
    _actorContactApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (work->stateTimer >= ACTOR_401300_STRIKE_RELAX_TICK) {
        work->jointPairTarget = 0;
        work->jointPairStep   = ACTOR_401300_STRIKE_PAIR_RELAX_STEP;
    }
    work->stateTimer++;
    // The attack sphere is live only during the swing window.
    switch (work->stateTimer) {
        case ACTOR_401300_STRIKE_ACTIVE_TICK:
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case ACTOR_401300_STRIKE_END_TICK:
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    strike = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &strike->delta);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor401300UpdateAnimationEffects(actor);
    if (work->stateTimer < ACTOR_401300_STRIKE_TRACK_END_TICK) {
        _actor401300TrackStrikePlayer(actor, work, strike);
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Restores the live actor root's yaw rotation at its normal Q12 scale of 6500.
///
/// Keeps heading and translation, discards pitch/roll and invalidates composition.
/// Requires the initialized scratch-stack space of `_actorRenderRescaleYaw`.
static __inline__ void _actor401300RestoreRootYawScale(Task* actor)
{
    GfxCoord*             coord;
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    coord           = actor->extra.tmd->coords;
    yawScratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);
    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vz = ACTOR_401300_ROOT_SCALE;
    yawScratch->scale.vy = ACTOR_401300_ROOT_SCALE;
    yawScratch->scale.vx = ACTOR_401300_ROOT_SCALE;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);
    _actorRenderCopyRotation(coord, yawScratch->rotation.m);
    coord->composeStamp                    = GRAPHICS_COORD_DIRTY;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Returns a live coordinate's local matrix heading in 4096 units per turn.
///
/// Reads the horizontal terms of the Z axis without composing the coordinate;
/// pitch, roll and scale are not removed. A zero horizontal pair returns zero.
static __inline__ s32 _actorAngleGetLocalYaw(const GfxCoord* coord)
{
    return ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

/// Translates a coordinate along its normalized local Z axis using a borrowed save's freeze state.
///
/// Requires a live readable save and writable coordinate; neither pointer is
/// retained. Only actorsFrozen equal to 1 suppresses the step. stepDistance is
/// signed parent-coordinate units, including Y; negative moves backward.
/// Displacement narrows to signed halfwords after SDK normalization and GTE
/// scaling. Reserves/releases one aligned SVECTOR; collision correction is left
/// to the caller. Zero distance still runs the GTE calculation and dirties composition.
static __inline__ void _actorMovementStepForwardFromSave(const McSaveData* save, GfxCoord* coord, s16 stepDistance)
{
    enum { ACTOR_MOVEMENT_FROZEN = 1 };
    SVECTOR* displacement;

    if (save->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        displacement = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
        gfxReadMatrixZAxis(&coord->coord, displacement);
        _actorMovementBuildDisplacement(displacement, stepDistance);
        coord->coord.t[0]  += displacement->vx;
        coord->coord.t[1]  += displacement->vy;
        coord->coord.t[2]  += displacement->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Places the knocked player at its existing position and starts the selected fall clip.
///
/// Requires an accepted player hold, work-owned placement yaw and animation
/// request, and live player coordinates in the actor's parent space. playerTask
/// must be the live player slot, which is reacquired for animation. Payloads
/// are borrowed synchronously; the selected animation table outlives playback.
static __inline__ void _actor401300PlaceKnockedPlayer(_Actor401300Work* work, Task* playerTask)
{
    work->playerPlacement.pos.vx = playerTask->extra.tmd->coords->coord.t[0];
    work->playerPlacement.pos.vy = playerTask->extra.tmd->coords->coord.t[1];
    work->playerPlacement.pos.vz = playerTask->extra.tmd->coords->coord.t[2];
    work->playerPlacement.rot.vx = 0;
    work->playerPlacement.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
    work->playerAnimFrames = 0;
}

/// Resolves charge contacts and requests the recoil clip after seven late blocked ticks.
///
/// Requires live actor/work and this frame's charge scratch. Grid corrections
/// take priority over body pushback; only ticks 21 onward count toward recoil.
/// Resets the blocked counter when movement is free. Nested scratch is released.
static __inline__ void _actor401300ResolveChargeContacts(Task* actor, _Actor401300Work* work, _Actor401300ChargeScratch* charge)
{
    enum { ACTOR_401300_CHARGE_BLOCK_COUNT_TICK = 21,
           ACTOR_401300_CHARGE_BLOCK_LIMIT      = 7 };
    if ((charge->gridPushed = _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET)) != 0 &&
        work->stateTimer >= ACTOR_401300_CHARGE_BLOCK_COUNT_TICK) {
        work->blockedFrames++;
    } else {
        if (charge->gridPushed != 1) {
            _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        work->blockedFrames = 0;
    }
    if (work->blockedFrames >= ACTOR_401300_CHARGE_BLOCK_LIMIT) {
        work->animId      = ACTOR_401300_ANIM_CHARGE_RECOIL;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
        work->stateTimer  = 0;
    }
}

/// Charges into the player, negotiates a knockdown and recoils from contact or blockage.
///
/// Requires live actor/Enemy/player work, roots, rigs and player reaction clips.
/// Running steers by at most 6/4096 of a turn and steps 112 units; the close
/// clip steps 168. Seven blocked ticks from tick 21 force recoil. A live
/// player contact within 256 yaw units requests a 127-press hold; acceptance
/// chooses attack/clip 4 or 5 and a 100-unit horizontal push by impact side.
/// Work owns the message records and the borrowed clip table must stay loaded.
/// Releases one charge block after nested movement and yaw workspace.
static void _actor401300StateCharge(Task* actor)
{
    enum {
        ACTOR_401300_CHARGE_CLOSE_TICK          = 11,
        ACTOR_401300_CHARGE_CLOSE_ANGLE         = 512,
        ACTOR_401300_CHARGE_CLOSE_RADIUS        = 2000,
        ACTOR_401300_CHARGE_SIDE_ANGLE          = 1024,
        ACTOR_401300_CHARGE_CATCH_ANGLE         = 256,
        ACTOR_401300_CHARGE_TURN_STEP           = 6,
        ACTOR_401300_CHARGE_RUN_STEP            = 112,
        ACTOR_401300_CHARGE_CLOSE_STEP          = 168,
        ACTOR_401300_CHARGE_KNOCKBACK_STEP      = 100,
        ACTOR_401300_CHARGE_RECOIL_STEP         = -121,
        ACTOR_401300_CHARGE_RECOIL_SLOW_STEP    = -25,
        ACTOR_401300_CHARGE_RECOIL_FAST_TICKS   = 8,
        ACTOR_401300_CHARGE_RECOIL_SLOW_TICKS   = 6,
        ACTOR_401300_CHARGE_PRESS_COUNT         = 127,
        ACTOR_401300_CHARGE_PLAYER_BACK_ATTACK  = 4,
        ACTOR_401300_CHARGE_PLAYER_FRONT_ATTACK = 5,
        ACTOR_401300_CHARGE_PAIR_STEP           = 64,
    };
    _Actor401300Work*          work;
    Task*                      playerTask;
    GameActor*                 player;
    PlayerStatus*              playerStatus;
    Enemy*                     enemy;
    TmdObject*                 model;
    GfxCoord*                  offsetRoot;
    void**                     cursorSlot;
    _Actor401300ChargeScratch* savedCursor;
    _Actor401300ChargeScratch* charge;
    s16                        knockbackStep;
    s16                        damageReply;
    McSaveData*                save;

    work       = actor->work;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player     = playerTask->work;
    enemy      = actor->spawnArg2.pointer;

    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_CHARGE_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->jointPairTarget = 0;
        work->jointPairStep   = ACTOR_401300_CHARGE_PAIR_STEP;
        return;
    }
    cursorSlot   = SCRATCH_HEAD_ADDR;
    playerStatus = &gPlayerStatus;
    work->stateTimer++;
    offsetRoot               = actor->extra.tmd->coords;
    savedCursor              = SCRATCH_HEAD_AT(cursorSlot, _Actor401300ChargeScratch);
    savedCursor[-1].delta.vx = playerStatus->coordMtx->t[0] - offsetRoot->coord.t[0];
    charge                   = (SCRATCH_HEAD_AT(cursorSlot, _Actor401300ChargeScratch) = savedCursor - 1);
    charge->delta.vy         = playerStatus->coordMtx->t[1] - offsetRoot->coord.t[1];
    charge->delta.vz         = playerStatus->coordMtx->t[2] - offsetRoot->coord.t[2];
    save                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    _actor401300UpdateAnimationEffects(actor);
    // Clip changes reset the counter used for proximity, blockage and recoil.
    switch (work->animId) {
        case ACTOR_401300_ANIM_CHARGE_RUN:
            _actor401300ResolveChargeContacts(actor, work, charge);
            work->lookYawTarget = 0;
            charge->turn        = _actorAngleTurnToOffset(actor->extra.tmd->coords, charge->delta.vx, charge->delta.vz);
            if (work->stateTimer >= ACTOR_401300_CHARGE_CLOSE_TICK) {
                if (abs(charge->turn) < ACTOR_401300_CHARGE_CLOSE_ANGLE) {
                    if (!_actorRangeOutsideRadiusXZ(&charge->delta, ACTOR_401300_CHARGE_CLOSE_RADIUS)) {
                        work->animId      = ACTOR_401300_ANIM_CHARGE_CLOSE;
                        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                        work->stateTimer  = 0;
                    }
                }
            }
            if (abs(charge->turn) > ACTOR_401300_CHARGE_SIDE_ANGLE) {
                work->animId      = ACTOR_401300_ANIM_CHARGE_CLOSE;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                work->stateTimer  = 0;
            }
            if (charge->turn > ACTOR_401300_CHARGE_TURN_STEP) {
                charge->turn = ACTOR_401300_CHARGE_TURN_STEP;
            } else if (charge->turn < -ACTOR_401300_CHARGE_TURN_STEP) {
                charge->turn = -ACTOR_401300_CHARGE_TURN_STEP;
            }
            charge->turn += _actorAngleGetLocalYaw(actor->extra.tmd->coords);
            gfxRotMatrixY(&actor->extra.tmd->coords->coord, charge->turn, GRAPHICS_ROTATION_REPLACE);
            if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_CHARGE_RUN_STEP) != 0) {
                _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_CHARGE_RUN_STEP);
            }
            _actor401300RestoreRootYawScale(actor);
            break;
        case ACTOR_401300_ANIM_CHARGE_CLOSE:
            _actor401300ResolveChargeContacts(actor, work, charge);
            charge->turn = _actorAngleTurnToOffset(actor->extra.tmd->coords, charge->delta.vx, charge->delta.vz);
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->animId      = ACTOR_401300_ANIM_CHARGE_FINISH;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            if (_actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 0 && player->mode != GAME_ACTOR_MODE_SCRIPTED && abs(charge->turn) < ACTOR_401300_CHARGE_CATCH_ANGLE &&
                enemy->hp > 0) {
                work->playerButtonHold.pressCount = ACTOR_401300_CHARGE_PRESS_COUNT;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                    padScriptSpawnVariableMotorRamp(ACTOR_401300_KNOCKDOWN_MOTOR_TICKS, ACTOR_401300_KNOCKDOWN_MOTOR_START_INTENSITY, ACTOR_401300_KNOCKDOWN_MOTOR_END_INTENSITY);
                    work->playerHeld                   = 1;
                    work->playerAnim.source.sets       = D_actor_401300_801588F0;
                    work->playerMove.displacement.vz   = 0;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vx   = 0;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                    // Reverse the bearing to place the player, then build a forward-axis push.
                    charge->delta.vx = -charge->delta.vx;
                    charge->delta.vy = -charge->delta.vy;
                    charge->delta.vz = -charge->delta.vz;
                    charge->turn     = _actorAngleTurnToOffset(playerTask->extra.tmd->coords, charge->delta.vx, charge->delta.vz);
                    if (abs(charge->turn) < ACTOR_401300_CHARGE_SIDE_ANGLE) {
                        knockbackStep                = -ACTOR_401300_CHARGE_KNOCKBACK_STEP;
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_FALL_BACK;
                        work->playerPlacement.rot.vy = charge->turn + _actorAngleGetLocalYaw(playerTask->extra.tmd->coords);
                        damageReply                  = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_401300_CHARGE_PLAYER_BACK_ATTACK), 0);
                    } else {
                        knockbackStep                = ACTOR_401300_CHARGE_KNOCKBACK_STEP;
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_FALL_FRONT;
                        work->playerPlacement.rot.vy = charge->turn + _actorAngleGetLocalYaw(playerTask->extra.tmd->coords) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                        damageReply                  = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_401300_CHARGE_PLAYER_FRONT_ATTACK), 0);
                    }
                    if (damageReply == 1) {
                        player->state = ACTOR_401300_PLAYER_FATAL_HOLD_STATE;
                    }
                    _actor401300PlaceKnockedPlayer(work, playerTask);
                    gfxReadMatrixZAxis(&playerTask->extra.tmd->coords->coord, &charge->delta);
                    charge->delta.vy = 0;
                    VectorNormalSS(&charge->delta, &charge->delta);
                    gte_lddp(knockbackStep);
                    gte_ldsv(&charge->delta);
                    gte_gpf12();
                    gte_stsv(&charge->delta);
                    work->playerMove.displacement.vx   = charge->delta.vx;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vz   = charge->delta.vz;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                    work->animId                       = ACTOR_401300_ANIM_CHARGE_RECOIL;
                    work->animRequest                  = ACTOR_401300_ANIM_REQUEST_RESET;
                    work->stateTimer                   = 0;
                }
            }
            if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_CHARGE_CLOSE_STEP) != 0) {
                _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_CHARGE_CLOSE_STEP);
            }
            _actor401300RestoreRootYawScale(actor);
            break;
        case ACTOR_401300_ANIM_CHARGE_RECOIL:
            if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
                _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
            if (work->stateTimer < ACTOR_401300_CHARGE_RECOIL_FAST_TICKS) {
                if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_CHARGE_RECOIL_STEP) != 0) {
                    _actorMovementStepForwardFromSave(save, actor->extra.tmd->coords, ACTOR_401300_CHARGE_RECOIL_STEP);
                }
            } else if ((u16)(work->stateTimer - ACTOR_401300_CHARGE_RECOIL_FAST_TICKS) < ACTOR_401300_CHARGE_RECOIL_SLOW_TICKS) {
                if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_CHARGE_RECOIL_SLOW_STEP) != 0) {
                    _actorMovementStepForwardFromSave(save, actor->extra.tmd->coords, ACTOR_401300_CHARGE_RECOIL_SLOW_STEP);
                }
            }
            _actor401300RestoreRootYawScale(actor);
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        case ACTOR_401300_ANIM_CHARGE_FINISH:
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        default:
            work->state = ACTOR_401300_STATE_PATROL;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300ChargeScratch);
}

/// Translates a coordinate along its normalized local Z axis using a borrowed save's freeze state.
///
/// Requires a live readable save and writable coordinate; neither pointer is
/// retained. Only actorsFrozen equal to 1 suppresses the step. stepDistance is
/// signed parent-coordinate units, including Y; negative moves backward.
/// Displacement narrows to signed halfwords after SDK normalization and GTE
/// scaling. Reserves/releases one aligned SVECTOR; collision correction is left
/// to the caller. Zero distance leaves the coordinate and GTE state unchanged.
static __inline__ void _actorMovementTranslateForwardNonzeroFromSave(const McSaveData* save, GfxCoord* coord, s16 stepDistance)
{
    enum { ACTOR_MOVEMENT_FROZEN = 1 };
    SVECTOR* displacement;

    if (save->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        displacement = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
        if (stepDistance != 0) {
            gfxReadMatrixZAxis(&coord->coord, displacement);
            _actorMovementBuildDisplacement(displacement, stepDistance);
            coord->coord.t[0]  += displacement->vx;
            coord->coord.t[1]  += displacement->vy;
            coord->coord.t[2]  += displacement->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Crouches toward the player, leaps through them and starts a side-dependent knockdown.
///
/// Requires live actor/Enemy/player work, common parent space and loaded clips.
/// Eleven crouch ticks steer by 16 yaw units (4096 per turn); the full XYZ
/// distance plus 1000, clamped to 3000..5000, sets the step over 18 ticks.
/// From leap tick 8 a live player contact can accept a 127-press hold, attacks
/// 2/3, player clips 4/5 and a 70-unit push chosen from the leap start bearing.
/// Landing coasts from 84 to zero over 16 ticks unless held or grid-blocked.
/// Message records remain work-owned; releases the leap block and nested scratch.
static void _actor401300StateLeap(Task* actor)
{
    enum {
        ACTOR_401300_LEAP_CROUCH_ANIM         = 31,
        ACTOR_401300_LEAP_CROUCH_TICKS        = 11,
        ACTOR_401300_LEAP_ABORT_ANGLE         = 1024,
        ACTOR_401300_LEAP_TURN_STEP           = 16,
        ACTOR_401300_LEAP_DISTANCE_EXTRA      = 1000,
        ACTOR_401300_LEAP_DISTANCE_MIN        = 3000,
        ACTOR_401300_LEAP_DISTANCE_MAX        = 5000,
        ACTOR_401300_LEAP_TRAVEL_TICKS        = 18,
        ACTOR_401300_LEAP_HIT_RADIUS          = 320,
        ACTOR_401300_LEAP_CATCH_TICK          = 8,
        ACTOR_401300_LEAP_PRESS_COUNT         = 127,
        ACTOR_401300_LEAP_KNOCKBACK_STEP      = 70,
        ACTOR_401300_LEAP_PLAYER_BACK_ATTACK  = 2,
        ACTOR_401300_LEAP_PLAYER_FRONT_ATTACK = 3,
        ACTOR_401300_LEAP_PAIR_TARGET         = 512,
        ACTOR_401300_LEAP_PAIR_STEP           = 256,
        ACTOR_401300_LEAP_LAND_TICKS          = 16,
        ACTOR_401300_LEAP_LAND_STEP           = 84,
    };
    _Actor401300Work*        work;
    Task*                    playerTask;
    GameActor*               player;
    PlayerStatus*            playerStatus;
    Enemy*                   enemy;
    TmdObject*               model;
    GfxCoord*                offsetRoot;
    void**                   cursorSlot;
    _Actor401300LeapScratch* savedCursor;
    _Actor401300LeapScratch* leap;
    SVECTOR*                 playerOffset;
    SVECTOR*                 knockbackDirection;
    s16                      landTick;
    s16                      knockbackStep;
    s16                      damageReply;
    McSaveData*              save;

    work         = actor->work;
    playerTask   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player       = playerTask->work;
    playerStatus = &gPlayerStatus;
    save         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    enemy        = actor->spawnArg2.pointer;

    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_LEAP_CROUCH_ANIM;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor401300UpdateAnimationEffects(actor);
        work->jointPairTarget = ACTOR_401300_LEAP_PAIR_TARGET;
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->jointPairStep   = ACTOR_401300_LEAP_PAIR_STEP;
        work->leapStartPos.vx = actor->extra.tmd->coords->coord.t[0];
        work->leapStartPos.vy = actor->extra.tmd->coords->coord.t[1];
        work->leapStartPos.vz = actor->extra.tmd->coords->coord.t[2];
        return;
    }
    cursorSlot = SCRATCH_HEAD_ADDR;
    work->stateTimer++;
    offsetRoot               = actor->extra.tmd->coords;
    savedCursor              = SCRATCH_HEAD_AT(cursorSlot, _Actor401300LeapScratch);
    savedCursor[-1].delta.vx = playerStatus->coordMtx->t[0] - offsetRoot->coord.t[0];
    playerOffset             = &savedCursor[-1].delta;
    playerOffset->vy         = playerStatus->coordMtx->t[1] - offsetRoot->coord.t[1];
    leap                     = (SCRATCH_HEAD_AT(cursorSlot, _Actor401300LeapScratch) = savedCursor - 1);
    playerOffset->vz         = playerStatus->coordMtx->t[2] - offsetRoot->coord.t[2];
    _actor401300UpdateAnimationEffects(actor);
    switch (work->animId) {
        case ACTOR_401300_LEAP_CROUCH_ANIM:
            work->hitBody.radius = ACTOR_401300_HIT_RADIUS;
            if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
                _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
            work->lookYawTarget = 0;
            leap->turn          = _actorAngleTurnToOffset(actor->extra.tmd->coords, savedCursor[-1].delta.vx, playerOffset->vz);
            if (work->stateTimer >= ACTOR_401300_LEAP_CROUCH_TICKS) {
                work->animId      = ACTOR_401300_ANIM_LEAP;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                // Measure full-width XYZ once; subsequent movement uses the stored step.
                leap->offset.vx    = playerStatus->coordMtx->t[0] - actor->extra.tmd->coords->coord.t[0];
                leap->offset.vy    = playerStatus->coordMtx->t[1] - actor->extra.tmd->coords->coord.t[1];
                leap->offset.vz    = playerStatus->coordMtx->t[2] - actor->extra.tmd->coords->coord.t[2];
                leap->leapDistance = SquareRoot0(leap->offset.vx * leap->offset.vx + leap->offset.vy * leap->offset.vy + leap->offset.vz * leap->offset.vz) + ACTOR_401300_LEAP_DISTANCE_EXTRA;
                if (leap->leapDistance > ACTOR_401300_LEAP_DISTANCE_MAX) {
                    leap->leapDistance = ACTOR_401300_LEAP_DISTANCE_MAX;
                } else if (leap->leapDistance < ACTOR_401300_LEAP_DISTANCE_MIN) {
                    leap->leapDistance = ACTOR_401300_LEAP_DISTANCE_MIN;
                }
                work->leapStep        = leap->leapDistance / ACTOR_401300_LEAP_TRAVEL_TICKS;
                work->jointPairTarget = 0;
                work->stateTimer      = 0;
                work->jointPairStep   = ACTOR_401300_PAIR_NORMAL_STEP;
            }
            if (abs(leap->turn) > ACTOR_401300_LEAP_ABORT_ANGLE) {
                work->state = ACTOR_401300_STATE_CHASE;
            }
            if (leap->turn > ACTOR_401300_LEAP_TURN_STEP) {
                leap->turn = ACTOR_401300_LEAP_TURN_STEP;
            } else if (leap->turn < -ACTOR_401300_LEAP_TURN_STEP) {
                leap->turn = -ACTOR_401300_LEAP_TURN_STEP;
            }
            leap->turn += _actorAngleGetLocalYaw(actor->extra.tmd->coords);
            gfxRotMatrixY(&actor->extra.tmd->coords->coord, leap->turn, GRAPHICS_ROTATION_REPLACE);
            _actor401300RestoreRootYawScale(actor);
            break;
        case ACTOR_401300_ANIM_LEAP:
            work->hitBody.radius = ACTOR_401300_LEAP_HIT_RADIUS;
            _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->animId      = ACTOR_401300_ANIM_LAND;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            if (_actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 0 && player->mode != GAME_ACTOR_MODE_SCRIPTED && work->stateTimer >= ACTOR_401300_LEAP_CATCH_TICK &&
                enemy->hp > 0) {
                work->playerButtonHold.pressCount = ACTOR_401300_LEAP_PRESS_COUNT;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                    padScriptSpawnVariableMotorRamp(ACTOR_401300_KNOCKDOWN_MOTOR_TICKS, ACTOR_401300_KNOCKDOWN_MOTOR_START_INTENSITY, ACTOR_401300_KNOCKDOWN_MOTOR_END_INTENSITY);
                    sndEvtRequestScriptStart(SOUND_PLAYER_STRUCK, (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords),
                                             (s8)worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords));
                    work->playerHeld             = 1;
                    work->playerAnim.source.sets = D_actor_401300_801588F0;
                    // The launch position, rather than current contact, picks the fall side.
                    leap->delta.vx = work->leapStartPos.vx - playerTask->extra.tmd->coords->coord.t[0];
                    leap->delta.vy = work->leapStartPos.vy - playerTask->extra.tmd->coords->coord.t[1];
                    leap->delta.vz = work->leapStartPos.vz - playerTask->extra.tmd->coords->coord.t[2];
                    leap->turn     = _actorAngleTurnToOffset(playerTask->extra.tmd->coords, savedCursor[-1].delta.vx, playerOffset->vz);
                    if (abs(leap->turn) < ACTOR_401300_LEAP_ABORT_ANGLE) {
                        knockbackStep                = -ACTOR_401300_LEAP_KNOCKBACK_STEP;
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_FALL_BACK;
                        work->playerPlacement.rot.vy = leap->turn + _actorAngleGetLocalYaw(playerTask->extra.tmd->coords);
                        damageReply                  = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_401300_LEAP_PLAYER_BACK_ATTACK), 0);
                    } else {
                        knockbackStep                = ACTOR_401300_LEAP_KNOCKBACK_STEP;
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_FALL_FRONT;
                        work->playerPlacement.rot.vy = leap->turn + _actorAngleGetLocalYaw(playerTask->extra.tmd->coords) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                        damageReply                  = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_401300_LEAP_PLAYER_FRONT_ATTACK), 0);
                    }
                    if (damageReply == 1) {
                        player->state = ACTOR_401300_PLAYER_FATAL_HOLD_STATE;
                    }
                    _actor401300PlaceKnockedPlayer(work, playerTask);
                    knockbackDirection = &leap->delta;
                    gfxReadMatrixZAxis(&playerTask->extra.tmd->coords->coord, knockbackDirection);
                    leap->delta.vy = 0;
                    VectorNormalSS(knockbackDirection, knockbackDirection);
                    gte_lddp(knockbackStep);
                    gte_ldsv(knockbackDirection);
                    gte_gpf12();
                    gte_stsv(knockbackDirection);
                    work->playerMove.displacement.vx   = leap->delta.vx;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vz   = leap->delta.vz;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                }
            }
            if (work->playerHeld == 0) {
                _actorMovementTranslateForwardNonzero(actor->extra.tmd->coords, work->leapStep);
            }
            _actor401300RestoreRootYawScale(actor);
            break;
        case ACTOR_401300_ANIM_LAND:
            work->hitBody.radius = ACTOR_401300_HIT_RADIUS;
            if (_actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET) == 0) {
                landTick = work->stateTimer;
                if (landTick < ACTOR_401300_LEAP_LAND_TICKS + 1 && work->playerHeld == 0) {
                    if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, (s16)(ACTOR_401300_LEAP_LAND_STEP - landTick * ACTOR_401300_LEAP_LAND_STEP / ACTOR_401300_LEAP_LAND_TICKS)) != 0) {
                        _actorMovementTranslateForwardNonzeroFromSave(save, actor->extra.tmd->coords, ACTOR_401300_LEAP_LAND_STEP - work->stateTimer * ACTOR_401300_LEAP_LAND_STEP / ACTOR_401300_LEAP_LAND_TICKS);
                    }
                }
            }
            _actor401300ApplyBodyPushback(actor, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            _actor401300RestoreRootYawScale(actor);
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        default:
            work->state = ACTOR_401300_STATE_PATROL;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300LeapScratch);
}

/// Uniformly scales an actor matrix's basis and translation by a signed Q12 factor.
///
/// matrix must remain writable; 4096 represents one. Translation first narrows
/// to signed halfwords and the scaled components saturate to signed halfwords.
/// Borrows one aligned ActorScaleMatrixScratch and overwrites GTE state. The
/// caller owns refreshing the matrix before applying another scale.
static __inline__ void _actorRenderScaleMatrix(MATRIX* matrix, s16 uniformScale)
{
    ActorScaleMatrixScratch* scaleScratch;

    scaleScratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleMatrixScratch);
    scaleScratch->scale.vz = uniformScale;
    scaleScratch->scale.vy = uniformScale;
    scaleScratch->scale.vx = uniformScale;
    ScaleMatrix(matrix, &scaleScratch->scale);
    // Translation deliberately narrows before the signed Q12 GTE multiply.
    scaleScratch->translation.vx = matrix->t[0];
    scaleScratch->translation.vy = matrix->t[1];
    scaleScratch->translation.vz = matrix->t[2];
    gte_lddp(uniformScale);
    gte_ldsv(&scaleScratch->translation);
    gte_gpf12();
    gte_stsv(&scaleScratch->translation);
    matrix->t[0] = scaleScratch->translation.vx;
    matrix->t[1] = scaleScratch->translation.vy;
    matrix->t[2] = scaleScratch->translation.vz;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleMatrixScratch);
}

/// Leaps into the room from black, sheds leaves and coasts to an alert landing.
///
/// Requires live work/Enemy/model coordinates through part 19, loaded effects
/// and a refreshed color matrix on each tick. Ticks 1..17 scale that matrix
/// by tick/30 in Q12 and emit leaf groups every second tick in the forest or
/// woodland path. The leap steps 120 game units; landing decays an 84-unit
/// step over 16 ticks. Root grid collision stays disabled here. A reserved
/// 24-byte scratch frame records the player offset, which is not read back.
static void _actor401300StateLeapIn(Task* actor)
{
    enum {
        ACTOR_401300_LEAP_IN_PAIR_TARGET         = 512,
        ACTOR_401300_LEAP_IN_PAIR_STEP           = 256,
        ACTOR_401300_LEAP_IN_STEP                = 120,
        ACTOR_401300_LEAP_IN_HIT_RADIUS          = 1280,
        ACTOR_401300_LEAP_IN_COLOR_END_TICK      = 18,
        ACTOR_401300_LEAP_IN_COLOR_DIVISOR       = 30,
        ACTOR_401300_LEAP_IN_LAND_TICKS          = 16,
        ACTOR_401300_LEAP_IN_LAND_STEP           = 84,
        ACTOR_401300_LEAP_IN_COLOR_FRACTION_BITS = 12,
        ACTOR_401300_LEAP_IN_LEAF_PERIOD         = 8,
    };
    _Actor401300Work*          work;
    Enemy*                     enemy;
    TmdObject*                 model;
    PlayerStatus*              playerStatus;
    GfxCoord*                  offsetRoot;
    void**                     cursorSlot;
    _Actor401300LeapInScratch* savedCursor;
    SVECTOR*                   playerOffset;
    s16                        leafPhase;
    s16                        landTick;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model = actor->extra.tmd;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_401300_HIT_RADIUS;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_401300_ANIM_LEAP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor401300UpdateAnimationEffects(actor);
        work->jointPairTarget = ACTOR_401300_LEAP_IN_PAIR_TARGET;
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->jointPairStep   = ACTOR_401300_LEAP_IN_PAIR_STEP;
        work->leapStartPos.vx = actor->extra.tmd->coords->coord.t[0];
        work->leapStartPos.vy = actor->extra.tmd->coords->coord.t[1];
        work->leapStartPos.vz = actor->extra.tmd->coords->coord.t[2];
        _actorRenderScaleMatrix(&work->colorMtx, 0);
        return;
    }
    cursorSlot   = SCRATCH_HEAD_ADDR;
    playerStatus = &gPlayerStatus;
    work->stateTimer++;
    offsetRoot                                             = actor->extra.tmd->coords;
    savedCursor                                            = SCRATCH_HEAD_AT(cursorSlot, _Actor401300LeapInScratch);
    savedCursor[-1].playerOffset.vx                        = playerStatus->coordMtx->t[0] - offsetRoot->coord.t[0];
    playerOffset                                           = &savedCursor[-1].playerOffset;
    playerOffset->vy                                       = playerStatus->coordMtx->t[1] - offsetRoot->coord.t[1];
    playerOffset->vz                                       = playerStatus->coordMtx->t[2] - offsetRoot->coord.t[2];
    SCRATCH_HEAD_AT(cursorSlot, _Actor401300LeapInScratch) = savedCursor - 1;
    // The driver refreshes color before this cumulative-looking scale is applied.
    if (work->stateTimer < ACTOR_401300_LEAP_IN_COLOR_END_TICK) {
        _actorRenderScaleMatrix(&work->colorMtx, (work->stateTimer << ACTOR_401300_LEAP_IN_COLOR_FRACTION_BITS) / ACTOR_401300_LEAP_IN_COLOR_DIVISOR);
        if (gGameSession->location.loc.area == GAME_AREA_NEO_ARK_FOREST_ZONE) {
            if ((work->stateTimer & (ACTOR_401300_LEAP_IN_LEAF_PERIOD - 1)) == 0) {
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 3, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 16, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 1, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 18, 0, NULL);
            } else {
                leafPhase = work->stateTimer % ACTOR_401300_LEAP_IN_LEAF_PERIOD;
                if (leafPhase == 2) {
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 2, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 3, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 4, 0, NULL);
                } else if (leafPhase == 4) {
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 1, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 19, 0, NULL);
                } else if (leafPhase == 6) {
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, actor->extra.tmd->coords + 18, 0, NULL);
                }
            }
        } else if (gGameSession->location.loc.area == GAME_AREA_NEO_ARK_WOODLAND_PATH) {
            if ((work->stateTimer & (ACTOR_401300_LEAP_IN_LEAF_PERIOD - 1)) == 0) {
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 3, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 16, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 1, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 18, 0, NULL);
            } else {
                leafPhase = work->stateTimer % ACTOR_401300_LEAP_IN_LEAF_PERIOD;
                if (leafPhase == 2) {
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 2, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 3, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 4, 0, NULL);
                } else if (leafPhase == 4) {
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 1, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 19, 0, NULL);
                } else if (leafPhase == 6) {
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, actor->extra.tmd->coords + 18, 0, NULL);
                }
            }
        }
    }
    _actor401300UpdateAnimationEffects(actor);
    switch (work->animId) {
        case ACTOR_401300_ANIM_LEAP:
            work->hitBody.radius = ACTOR_401300_LEAP_IN_HIT_RADIUS;
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->animId      = ACTOR_401300_ANIM_LAND;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            if (work->playerHeld == 0 && (s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_LEAP_IN_STEP) != 0) {
                _actorMovementStepForward(actor->extra.tmd->coords, ACTOR_401300_LEAP_IN_STEP);
            }
            _actor401300RestoreRootYawScale(actor);
            break;
        case ACTOR_401300_ANIM_LAND:
            work->hitBody.radius = ACTOR_401300_HIT_RADIUS;
            landTick             = work->stateTimer;
            if (landTick < ACTOR_401300_LEAP_IN_LAND_TICKS + 1 && work->playerHeld == 0) {
                if ((s16)_playerDetectionOutOfReach(actor->extra.tmd->coords, ACTOR_401300_PLAYER_REACH_RADIUS, ACTOR_401300_LEAP_IN_LAND_STEP - landTick * ACTOR_401300_LEAP_IN_LAND_STEP / ACTOR_401300_LEAP_IN_LAND_TICKS) != 0) {
                    _actorMovementTranslateForwardNonzero(actor->extra.tmd->coords, ACTOR_401300_LEAP_IN_LAND_STEP - work->stateTimer * ACTOR_401300_LEAP_IN_LAND_STEP / ACTOR_401300_LEAP_IN_LAND_TICKS);
                }
            }
            _actor401300RestoreRootYawScale(actor);
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        default:
            work->state = ACTOR_401300_STATE_PATROL;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300LeapInScratch);
}

/// Falls backward again after a hit interrupted recovery, then selects rest or death.
///
/// Requires live work/Enemy/model and the loaded fall clip. Entry resets
/// playback and seeds the fixed-joint pair at 200/512 toward 64/512. Hit-body
/// grid correction takes priority over the root grid body. When the clip
/// settles, grid collision on the hit body stops and HP/buildup selects
/// DEATH_BURN, STATUS_HOLD or DOWN. Nested contact scratch is released.
static void _actor401300StateRefallBack(Task* actor)
{
    enum {
        ACTOR_401300_REFALL_PAIR_TARGET = 64,
        ACTOR_401300_REFALL_PAIR_START  = 200,
        ACTOR_401300_REFALL_PAIR_STEP   = 64,
    };
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = 0;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_401300_ANIM_FALL_BACK;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->jointPairTarget = ACTOR_401300_REFALL_PAIR_TARGET;
        work->jointPairBlend  = ACTOR_401300_REFALL_PAIR_START;
        work->jointPairStep   = ACTOR_401300_REFALL_PAIR_STEP;
        work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _actor401300UpdateAnimationEffects(actor);
    if (_actorContactApplyGridPushback(actor->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401300ChooseFallExit(work, enemy);
    }
}

/// Falls forward again after a hit interrupted recovery, then selects rest or death.
///
/// Requires live work/Enemy/model and the loaded fall clip. Entry resets
/// playback and seeds the fixed-joint pair at 200/512 toward 64/512. Hit-body
/// grid correction takes priority over the root grid body. When the clip
/// settles, grid collision on the hit body stops and HP/buildup selects
/// DEATH_BURN, STATUS_HOLD or DOWN. Nested contact scratch is released.
static void _actor401300StateRefallFront(Task* actor)
{
    enum {
        ACTOR_401300_REFALL_PAIR_TARGET = 64,
        ACTOR_401300_REFALL_PAIR_START  = 200,
        ACTOR_401300_REFALL_PAIR_STEP   = 64,
    };
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = 0;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_401300_ANIM_REFALL_FRONT;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->jointPairTarget         = ACTOR_401300_REFALL_PAIR_TARGET;
        work->jointPairBlend          = ACTOR_401300_REFALL_PAIR_START;
        work->jointPairStep           = ACTOR_401300_REFALL_PAIR_STEP;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _actor401300UpdateAnimationEffects(actor);
    if (_actorContactApplyGridPushback(actor->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        _actor401300ApplyGridPushback(actor->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_401300_GRID_ROOT_Y_OFFSET);
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        _actor401300ChooseFallExit(work, enemy);
    }
}

// This is a decompilation attempt by the m2c tool.

/// Returns 1 when model part 1's world Z selects the woodland impact sound.
///
/// Requires a live model with part 1 and an acyclic coordinate chain. Tests
/// [-299, 2300) game units using a modular unsigned-halfword interval. Each
/// parent transform narrows XYZ to signed halfwords and excludes the view matrix.
/// An incomplete chain keeps the zero origin and passes. No bounds or occlusion
/// test is made; no input is retained and GTE state is overwritten.
static __inline__ s32 _actor401300IsWithinImpactDepth(Task* modelTask)
{
    enum { ACTOR_401300_IMPACT_DEPTH_NEAR = -299,
           ACTOR_401300_IMPACT_DEPTH_FAR  = 2300 };
    SVECTOR   worldPoint;
    SVECTOR   parentPoint;
    VECTOR    transformedPoint;
    s32       gteFlags;
    SVECTOR*  parentPointPtr;
    GfxCoord* viewCoord;
    VECTOR*   transformedPointPtr;
    s32*      gteFlagsPtr;
    SVECTOR*  worldPointPtr;
    GfxCoord* coord;
    s32       result;
    s32       withinDepth;

    memset(&worldPoint, 0, sizeof(worldPoint));
    coord               = &modelTask->extra.tmd->coords[1];
    parentPointPtr      = &parentPoint;
    worldPointPtr       = &worldPoint;
    viewCoord           = &gGfxViewCoord;
    transformedPointPtr = &transformedPoint;
    gteFlagsPtr         = &gteFlags;
    parentPoint.vx      = worldPointPtr->vx;
    parentPoint.vy      = worldPointPtr->vy;
    parentPoint.vz      = worldPointPtr->vz;
    for (;;) {
        if (coord->parent != NULL) {
            if (coord != viewCoord) {
                gte_SetTransMatrix(&coord->coord);
                gte_SetRotMatrix(&coord->coord);
                gte_RotTrans(parentPointPtr, transformedPointPtr, gteFlagsPtr);
                parentPoint.vx = transformedPoint.vx;
                parentPoint.vy = transformedPoint.vy;
                parentPoint.vz = transformedPoint.vz;
                coord          = coord->parent;
                continue;
            }
            worldPointPtr->vx = parentPoint.vx;
            worldPointPtr->vy = parentPoint.vy;
            worldPointPtr->vz = parentPoint.vz;
        }
        break;
    }
    withinDepth = (u16)(worldPoint.vz - ACTOR_401300_IMPACT_DEPTH_NEAR) < ACTOR_401300_IMPACT_DEPTH_FAR - ACTOR_401300_IMPACT_DEPTH_NEAR;
    // Keep the comparison and returned flag separate in this branch shape.
    if (withinDepth == 0) {
        result = withinDepth;
        result = 0;
    } else {
        result = withinDepth;
        result = 1;
    }
    return result;
}

/// Returns 1 when the first contact belongs to a player or companion body.
///
/// Requires one readable contact; an empty or different-kind contact returns 0.
/// Does not scan further entries or consume the contact.
static __inline__ s32 _actorContactFirstIsPlayerBody(const WorldCollisionContact* contacts)
{
    s16 contactIndex;

    // The one-entry loop preserves the contact walk's signed halfword test.
    for (contactIndex = 0; contactIndex < 1; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return 1;
        }
    }
    return 0;
}

/// Aligns a held player's root height with the enemy after excessive vertical drift.
///
/// Requires allocated actor work and a live enemy model. A missing player task
/// is ignored. Only a pending push requesting every collision pass permits the
/// snap; an absolute Y difference of at least 801 parent-coordinate units sets
/// the player to the enemy's height and dirties the player's composition.
static __inline__ void _actor401300AlignHeldPlayerHeight(Task* actor)
{
    enum { ACTOR_401300_HELD_PLAYER_HEIGHT_LIMIT = 801 };
    _Actor401300Work* work;
    Task*             playerTask;
    GfxCoord*         playerCoord;
    GfxCoord*         actorCoord;

    work       = actor->work;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((playerTask != NULL) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        playerCoord = playerTask->extra.tmd->coords;
        actorCoord  = actor->extra.tmd->coords;
        if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= ACTOR_401300_HELD_PLAYER_HEIGHT_LIMIT) {
            playerCoord->coord.t[1]                     = actorCoord->coord.t[1];
            playerTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static const _Actor401300StateTable D_actor_401300_80131F34 = { {
    _actor401300StateHidden,
    _actor401300StatePlayWalk,
    _actor401300StatePlayRun,
    _actor401300StatePlayDown,
    _actor401300StateStatusHold,
    _actor401300StateFlinch,
    _actor401300StateAlert,
    _actor401300StateChase,
    _actor401300StateWithdraw,
    _actor401300StateTurnAround,
    _actor401300StateSidestep,
    _actor401300StateGrab,
    _actor401300StateGrabPull,
    _actor401300StateGrabStrike,
    _actor401300StateGrabDone,
    _actor401300StateRiseBack,
    _actor401300StateRiseFront,
    _actor401300StateDown,
    NULL,
    _actor401300StateFallBack,
    _actor401300StateFallFront,
    _actor401300StateDeathBurn,
    _actor401300StateDormant,
    _actor401300StateDormantScripted,
    _actor401300StatePatrol,
    _actor401300StateBackOff,
    _actor401300StateSlide,
    _actor401300StateGrabWindup,
    _actor401300StateBendOver,
    _actor401300StateDeathBurst,
    _actor401300StateStalk,
    _actor401300StateStrikeA,
    _actor401300StateStrikeB,
    _actor401300StateCharge,
    _actor401300StateLeap,
    _actor401300StateLeapIn,
    _actor401300StateDead,
    _actor401300StateRefallBack,
    _actor401300StateRefallFront,
    _actor401300StateWounded,
    _actor401300StateDeathBurstWalk,
} };

/// Plays the selected fall's impact cue and applies the held player's decaying push.
///
/// Requires a held live player and a work-owned move payload. impactTick selects
/// the elapsed player-clip tick on which the sound and optional dust cue fire.
/// The move request is consumed synchronously in parent-coordinate game units.
/// The move is applied before its stored wall-contact result can clear XYZ for
/// subsequent pushes. Actor state tick 10 onward clears vertical push and halves
/// horizontal displacement with signed shifts, after this tick's move.
static __inline__ void _actor401300StepKnockedPlayer(Task* playerTask, _Actor401300Work* work, s16 impactTick)
{
    enum {
        ACTOR_401300_PLAYER_PUSH_DECAY_TICK = 10,
        ACTOR_401300_PLAYER_IMPACT_DUST_ARG = 0x80003A00, // Size 2560, three ticks per cell, with child puffs.
        ACTOR_401300_PLAYER_IMPACT_SOUND    = SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 19)
    };
    if (work->playerAnimFrames == impactTick) {
        if (_actor401300IsWithinImpactDepth(playerTask) == 1) {
            sndEvtRequestScriptStart(SOUND_NEO_ARK_WOODLAND_STRANGER_HIT, (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords),
                                     (s8)worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords));
        } else {
            sndEvtRequestScriptStart(ACTOR_401300_PLAYER_IMPACT_SOUND, (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords),
                                     (s8)worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords));
        }
        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
            effectSpawn(EFFECT_DUST_PUFF, &playerTask->extra.tmd->coords[1], ACTOR_401300_PLAYER_IMPACT_DUST_ARG, NULL);
        }
    }
    // The reply reports stored wall contacts after applying the displacement.
    if (TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) == 1) {
        work->playerMove.displacement.vx = 0;
        work->playerMove.displacement.vy = 0;
        work->playerMove.displacement.vz = 0;
    }
    if (work->stateTimer >= ACTOR_401300_PLAYER_PUSH_DECAY_TICK) {
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx >>= 1;
        work->playerMove.displacement.vz >>= 1;
    }
}

/// Advances Horned Stranger behavior, collision, held-player reactions and target history.
///
/// Requires initialized Enemy/work/model storage and live player resources.
/// state must index a non-NULL behavior handler; bodyPosCursor must index the
/// seven-entry ring. Paused/hidden modes clear contacts and return. Running
/// frames take hits, dispatch state entry, update collision and drive held
/// player clips. Death rewards wait until the hold ends. Part 2's parent-chain
/// origin supplies targeting, using the oldest ring sample during sidesteps.
/// The actor's overlay and player clip bank must remain loaded through holds.
static void _actor401300Tick(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_401300_PLAYER_ANIM_NONE         = 0,
        ACTOR_401300_PLAYER_ANIM_GRABBED      = 1,
        ACTOR_401300_PLAYER_ANIM_GRAB_STRUCK  = 2,
        ACTOR_401300_PLAYER_ANIM_GRAB_RELEASE = 3,
        ACTOR_401300_PLAYER_ANIM_RISE_BACK    = 6,
        ACTOR_401300_PLAYER_ANIM_RISE_FRONT   = 7,
        ACTOR_401300_PLAYER_BACK_IMPACT_TICK  = 15,
        ACTOR_401300_PLAYER_FRONT_IMPACT_TICK = 13,
        ACTOR_401300_GROUND_SHADOW_HALF_SIZE  = 640,
        ACTOR_401300_ANIM_SIDESTEP_POSITIVE   = 21,
        ACTOR_401300_ANIM_SIDESTEP_NEGATIVE   = 20
    };
    VECTOR                    worldPosition;
    _Actor401300StateTable    stateHandlers;
    _Actor401300Work*         work;
    ActorPartPositionScratch* partPosition;
    ActorPartPositionScratch* savedCursor;
    Task*                     playerTask;
    const PlayerStatus*       playerStatus;
    s32                       actorState;
    s32                       playerAnimationId;

    work          = actor->work;
    playerTask    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus  = &gPlayerStatus;
    stateHandlers = D_actor_401300_80131F34;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    worldPosition.vx = actor->extra.tmd->coords->workm.t[0];
    worldPosition.vy = actor->extra.tmd->coords->workm.t[1];
    worldPosition.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    // Paused/hidden frames consume contacts without advancing behavior or holds.
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            actorState = work->state;
            if ((actorState != ACTOR_401300_STATE_HIDDEN) && (actorState != ACTOR_401300_STATE_DEAD) && (actorState != ACTOR_401300_STATE_DEATH_BURN) && (actorState != ACTOR_401300_STATE_DEATH_BURST) && (actorState != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), ACTOR_401300_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
                actorState = work->state;
            }
            if ((actorState == ACTOR_401300_STATE_DEATH_BURST_WALK) && (work->animId == ACTOR_401300_ANIM_WALK)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), ACTOR_401300_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            actorState = work->state;
            if ((actorState != ACTOR_401300_STATE_HIDDEN) && (actorState != ACTOR_401300_STATE_DEAD) && (actorState != ACTOR_401300_STATE_DEATH_BURN) && (actorState != ACTOR_401300_STATE_DEATH_BURST) && (actorState != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), ACTOR_401300_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
                actorState = work->state;
            }
            if ((actorState == ACTOR_401300_STATE_DEATH_BURST_WALK) && (work->animId == ACTOR_401300_ANIM_WALK)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), ACTOR_401300_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->attackContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->attackContacts);
            return;
    }

    savedCursor                                    = SCRATCH_STACK_CURSOR(ActorPartPositionScratch);
    SCRATCH_STACK_CURSOR(ActorPartPositionScratch) = savedCursor - 1;
    partPosition                                   = savedCursor - 1;

    // Take hits before testing state entry, so reactions enter immediately.
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        _actor401300TakeHit(actor);
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    stateHandlers.handlers[work->state](actor);

    // Reposition collision bodies after the state has animated and moved the model.
    actorState = work->state;
    if ((actorState != ACTOR_401300_STATE_DEATH_BURN) && (actorState != ACTOR_401300_STATE_PLAY_DOWN) && (actorState != ACTOR_401300_STATE_HIDDEN) && (actorState != ACTOR_401300_STATE_DEAD) && (actorState != ACTOR_401300_STATE_DEATH_BURST) && (actorState != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
        partPosition->position.vx = 0;
        partPosition->position.vy = 0;
        partPosition->position.vz = 0;
        _actorRenderTransformToWorld(actor->extra.tmd->coords + 1, &partPosition->position);
        work->hitBody.pos.vx         = partPosition->position.vx;
        work->hitBody.pos.vy         = partPosition->position.vy;
        work->hitBody.pos.vz         = partPosition->position.vz;
        work->gridCoord.coord.t[0]   = actor->extra.tmd->coords->coord.t[0];
        work->gridCoord.coord.t[1]   = actor->extra.tmd->coords->coord.t[1] - ACTOR_401300_GRID_RADIUS;
        work->gridCoord.coord.t[2]   = actor->extra.tmd->coords->coord.t[2];
        work->gridCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->gridCoord);
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(actor->extra.tmd->coords);
        actorState = work->state;
    }
    if ((actorState == ACTOR_401300_STATE_DEATH_BURN) || (actorState == ACTOR_401300_STATE_HIDDEN) || (actorState == ACTOR_401300_STATE_DEAD) || (actorState == ACTOR_401300_STATE_DEATH_BURST) || (actorState == ACTOR_401300_STATE_DEATH_BURST_WALK)) {
        work->hitBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        if (work->state == ACTOR_401300_STATE_CHARGE || work->state == ACTOR_401300_STATE_LEAP) {
            work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        } else {
            work->gridBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        }
    }
    if ((_actorContactFirstIsPlayerBody(work->attackContacts) == 1) || (enemy->hp <= 0)) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->attackContacts);

    // Drive the borrowed player clips through release, stopping pushes on collision.
    if (work->playerHeld == 1) {
        actorState = work->state;
        if ((actorState != ACTOR_401300_STATE_DEATH_BURN) && (actorState != ACTOR_401300_STATE_PLAY_DOWN) && (actorState != ACTOR_401300_STATE_HIDDEN) && (actorState != ACTOR_401300_STATE_DEAD) && (actorState != ACTOR_401300_STATE_DEATH_BURST) && (actorState != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
            _actor401300AlignHeldPlayerHeight(actor);
        }
        playerAnimationId = work->playerAnim.animationId;
        work->playerAnimFrames++;
        switch (playerAnimationId) {
            case ACTOR_401300_PLAYER_ANIM_NONE:
            case ACTOR_401300_PLAYER_ANIM_GRABBED:
            case ACTOR_401300_PLAYER_ANIM_GRAB_STRUCK:
            case ACTOR_401300_PLAYER_ANIM_GRAB_RELEASE:
                break;
            case ACTOR_401300_PLAYER_ANIM_FALL_BACK:
                _actor401300StepKnockedPlayer(playerTask, work, ACTOR_401300_PLAYER_BACK_IMPACT_TICK);
                break;
            case ACTOR_401300_PLAYER_ANIM_FALL_FRONT:
                _actor401300StepKnockedPlayer(playerTask, work, ACTOR_401300_PLAYER_FRONT_IMPACT_TICK);
                break;
            case ACTOR_401300_PLAYER_ANIM_RISE_BACK:
            case ACTOR_401300_PLAYER_ANIM_RISE_FRONT:
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->playerAnim.animationId) {
                case ACTOR_401300_PLAYER_ANIM_NONE:
                    break;
                case ACTOR_401300_PLAYER_ANIM_GRABBED:
                    if (playerStatus->hp > 0) {
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_GRAB_STRUCK;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case ACTOR_401300_PLAYER_ANIM_GRAB_STRUCK:
                    if (playerStatus->hp > 0) {
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_GRAB_RELEASE;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case ACTOR_401300_PLAYER_ANIM_FALL_BACK:
                    if (playerStatus->hp > 0) {
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_RISE_BACK;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case ACTOR_401300_PLAYER_ANIM_FALL_FRONT:
                    if (playerStatus->hp > 0) {
                        work->playerAnim.animationId = ACTOR_401300_PLAYER_ANIM_RISE_FRONT;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case ACTOR_401300_PLAYER_ANIM_GRAB_RELEASE:
                case ACTOR_401300_PLAYER_ANIM_RISE_BACK:
                case ACTOR_401300_PLAYER_ANIM_RISE_FRONT:
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->playerHeld = 0;
                    break;
            }
        }
    }
    if ((work->deathPending == 1) && (work->playerHeld == 0)) {
        sceneReleaseBattleRefWithRewards(actor, 0xD);
        work->deathPending = 0;
    }
    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->state == ACTOR_401300_STATE_PATROL)) {
        work->state = ACTOR_401300_STATE_ALERT;
    }

    // Sidesteps publish the oldest ring entry; other clips use the current origin.
    partPosition->position.vx = 0;
    partPosition->position.vy = 0;
    partPosition->position.vz = 0;
    _actorRenderTransformToWorld(actor->extra.tmd->coords + 2, &partPosition->position);

    work->bodyPosHistory[work->bodyPosCursor].vx = partPosition->position.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = partPosition->position.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = partPosition->position.vz;

    // No later call reuses scratch; the current position remains intact after release.
    SCRATCH_STACK_RELEASE_BLOCK(ActorPartPositionScratch);
    work->bodyPosCursor++;
    if (work->bodyPosCursor == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if (work->animId == ACTOR_401300_ANIM_SIDESTEP_NEGATIVE || work->animId == ACTOR_401300_ANIM_SIDESTEP_POSITIVE) {
        enemy->bodyPos.vx = work->bodyPosHistory[work->bodyPosCursor].vx;
        enemy->bodyPos.vy = work->bodyPosHistory[work->bodyPosCursor].vy;
        enemy->bodyPos.vz = work->bodyPosHistory[work->bodyPosCursor].vz;
    } else {
        enemy->bodyPos.vx = partPosition->position.vx;
        enemy->bodyPos.vy = partPosition->position.vy;
        enemy->bodyPos.vz = partPosition->position.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

/// Ignores message 2015 and both payload words without changing the actor.
///
/// The message's purpose is unproven. Callers must ignore its undefined result.
static void _actor401300IgnoreMessage2015(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `_actor401300Task`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
static const EnemyTaskFuncTable3 D_actor_401300_8013201C = { {
    _actor401300Spawn,
    _actor401300Tick,
    enemyDestroy,
} };

/// Selects a message animation and forces entry into the lying/recovery state.
///
/// Requires writable actor work and a readable AnimationPlayRequest. Request IDs
/// 0..4 select bank indices 34, 35, 36, 37 and 39; other IDs retain the current clip.
/// Every request selects DOWN and forces fresh state entry, then returns 0.
/// Only animationId is read; blendFrames, the message ID and second payload are
/// ignored. The selected bank entry must be populated before later playback.
static s32 _actor401300PlayMessageAnimation(Task* task, s32 msgId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum {
        ACTOR_401300_MESSAGE_CLIP_1 = 35,
        ACTOR_401300_MESSAGE_CLIP_2 = 36,
        ACTOR_401300_MESSAGE_CLIP_3 = 37,
        ACTOR_401300_MESSAGE_CLIP_4 = 39
    };
    _Actor401300Work* work = task->work;

    switch (request->animationId) {
        case 0:
            work->animId = ACTOR_401300_ANIM_REFALL_FRONT;
            break;
        case 1:
            work->animId = ACTOR_401300_MESSAGE_CLIP_1;
            break;
        case 2:
            work->animId = ACTOR_401300_MESSAGE_CLIP_2;
            break;
        case 3:
            work->animId = ACTOR_401300_MESSAGE_CLIP_3;
            break;
        case 4:
            work->animId = ACTOR_401300_MESSAGE_CLIP_4;
            break;
    }
    work->state     = ACTOR_401300_STATE_DOWN;
    work->prevState = ACTOR_401300_FORCE_STATE_ENTRY;
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

/// Places this actor's root with XYZ rotation and records its matrix-derived yaw.
///
/// Uses the shared read-only placement contract and the work block's
/// placedYaw member. Returns 1; the placement is borrowed only through dispatch.
#include "../../shared/actor_messages_place_yaw.inc.c"
#undef ACTOR_MESSAGE_PLACE_RECORD_YAW
#undef ACTOR_MESSAGE_YAW_WORK_TYPE

#include "../../shared/actor_messages_release_hold.inc.c"

/// Releases the Horned Stranger's child tasks and collision bodies before destroying its enemy task.
///
/// The exit callback requires the task's Enemy storage to remain live. With
/// allocated work, kills either optional child, unlinks attack/hit/grid bodies
/// and clears the borrowed contact pointer before enemy destruction. A NULL work
/// block skips that cleanup; the task system owns freeing the work allocation.
static void _actor401300ReleaseResources(Task* task)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->childTask0 != NULL) {
            taskKill(work->childTask0);
        }
        if (work->childTask1 != NULL) {
            taskKill(work->childTask1);
        }
        worldCollisionUnlinkBody(&work->attackBody);
        worldCollisionUnlinkBody(&work->hitBody);
        worldCollisionUnlinkBody(&work->gridBody);
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}

/// Returns 1 when model part 1's world Z lies in the effect interval [-299, 2300).
///
/// Requires a live model with part 1 and an acyclic coordinate hierarchy.
/// Parent steps narrow XYZ to signed halfwords and exclude the view matrix;
/// an incomplete chain keeps the zero origin and passes. The unsigned-halfword
/// interval checks only world Z in game units. Retains the standalone body
/// present in this overlay; no current caller uses it.
static s32 _actor401300TestEffectDepth(Task* modelTask)
{
    enum { ACTOR_401300_EFFECT_DEPTH_NEAR = -299,
           ACTOR_401300_EFFECT_DEPTH_FAR  = 2300 };
    SVECTOR worldPoint;

    memset(&worldPoint, 0, sizeof(worldPoint));
    _actorRenderTransformToWorld(&modelTask->extra.tmd->coords[1], &worldPoint);
    return (u16)(worldPoint.vz - ACTOR_401300_EFFECT_DEPTH_NEAR) < ACTOR_401300_EFFECT_DEPTH_FAR - ACTOR_401300_EFFECT_DEPTH_NEAR;
}

/// Hides the actor on entry, then publishes reusable encounter slots as available.
///
/// Requires live actor work, Enemy storage and a writable model. Entry disables
/// targeting, active drawing, attack pairing and grid collision. On later ticks,
/// the reusable spawn kind receives the available-HP marker only after pending
/// death reward processing has completed. Does not change the behavior state.
static void _actor401300StateHidden(Task* task)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = (u16)(model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        return;
    }
    if (enemy->hp != ACTOR_401300_HP_AVAILABLE && work->deathPending == 0 && (task->spawnArg1.value >> 16) == ACTOR_401300_SPAWN_REUSABLE) {
        enemy->hp = ACTOR_401300_HP_AVAILABLE;
    }
}

/// Plays the walk pose in place with attack and grid tests disabled.
///
/// Entry restores drawing/lock-on, allocates primitive storage and resets WALK at normal
/// rate; later ticks dirty the root before animation. Requires live work,
/// Enemy and model storage. This handler never selects another behavior state.
static void _actor401300StatePlayWalk(Task* actor)
{
    TmdObject*        model;
    _Actor401300Work* work;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = ANIMATION_RATE_ONE;
        work->animId           = ACTOR_401300_ANIM_WALK;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        _actor401300UpdateAnimationEffects(actor);
    } else {
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor401300UpdateAnimationEffects(actor);
    }
}

/// Plays the run pose in place with attack and grid tests disabled.
///
/// Entry restores drawing/lock-on, allocates primitive storage and resets RUN at normal
/// rate; later ticks dirty the root before animation. Requires live work,
/// Enemy and model storage. This handler never selects another behavior state.
static void _actor401300StatePlayRun(Task* actor)
{
    TmdObject*        model;
    _Actor401300Work* work;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = ANIMATION_RATE_ONE;
        work->animId           = ACTOR_401300_ANIM_RUN;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        _actor401300UpdateAnimationEffects(actor);
    } else {
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor401300UpdateAnimationEffects(actor);
    }
}

/// Plays the back-fall pose in place with attack and grid tests disabled.
///
/// Requires live work/Enemy/model storage. Entry restores drawing/lock-on,
/// allocates primitive storage, resets FALL_BACK at normal rate and eases the fixed joint pair toward 32/512
/// by 8/512 per tick. This handler never selects another behavior state.
static void _actor401300StatePlayDown(Task* actor)
{
    TmdObject*        model;
    _Actor401300Work* work;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = ANIMATION_RATE_ONE;
        work->animId           = ACTOR_401300_ANIM_FALL_BACK;
        work->jointPairTarget  = ACTOR_401300_FALL_PAIR_BLEND;
        work->jointPairStep    = ACTOR_401300_FALL_PAIR_STEP;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        _actor401300UpdateAnimationEffects(actor);
    } else {
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor401300UpdateAnimationEffects(actor);
    }
}

/// Plays hit recoil at 18/16 frames per tick, then resumes pursuit.
///
/// Requires live work/Enemy/model storage. Entry restores drawing/lock-on,
/// allocates primitives, disables attack/grid tests and resets FLINCH without blending.
/// A settled slot-1 pose selects CHASE after this tick's animation update.
static void _actor401300StateFlinch(Task* actor)
{
    enum { ACTOR_401300_FLINCH_ANIM_RATE = 18 };
    TmdObject*        model;
    _Actor401300Work* work;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                                                      = actor->extra.tmd;
        ((Enemy*)actor->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                               = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = ACTOR_401300_FLINCH_ANIM_RATE;
        work->animId           = ACTOR_401300_ANIM_FLINCH;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor401300UpdateAnimationEffects(actor);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
}

/// Holds the completed-grab state without updating animation or moving the actor.
///
/// The per-frame hit handler and messages can still choose a different state.
static void _actor401300StateGrabDone(Task* task)
{
}

/// Recovers from a backward fall, then pursues or withdraws on the woodland path.
///
/// Requires live work/Enemy/model storage. Entry restores drawing, targeting,
/// the 640-unit hit radius and grid tests, disables attack and resets RISE_BACK
/// at chaseRate (sixteenths of a frame per tick). Settled playback selects
/// WITHDRAW on the Neo Ark woodland path, or CHASE elsewhere.
static void _actor401300StateRiseBack(Task* actor)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = 0;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_401300_ANIM_RISE_BACK;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    _actor401300UpdateAnimationEffects(actor);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 0)) {
            work->state = ACTOR_401300_STATE_WITHDRAW;
        } else {
            work->state = ACTOR_401300_STATE_CHASE;
        }
    }
}

/// Recovers from a forward fall and resumes pursuit when the pose settles.
///
/// Requires live work/Enemy/model storage. Entry restores drawing, targeting,
/// the 640-unit hit radius and grid tests, disables attack and resets RISE_FRONT
/// at chaseRate (sixteenths of a frame per tick). Settled playback selects CHASE.
static void _actor401300StateRiseFront(Task* actor)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        actor->extra.tmd->flags       = 0;
        work->hitBody.radius          = ACTOR_401300_HIT_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_401300_ANIM_RISE_FRONT;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    _actor401300UpdateAnimationEffects(actor);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
}

/// Holds a fallen pose for the variant's rest duration, then chooses its side's rise.
///
/// Requires live work/Enemy/model storage. Entry adds 0..15 random ticks to
/// downFramesBase and eases the fixed joint pair toward 32/512 by 8/512 a tick.
/// The signed-halfword timer decrements immediately; expiry maps the back/front
/// fall and hold clips to their rise states. Unlisted clips retain DOWN.
/// Depleted HP selects DEATH_BURN after the recovery test.
static void _actor401300StateDown(Task* actor)
{
    enum { ACTOR_401300_DOWN_RANDOM_TICKS_MASK = 15 };
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->jointPairTarget = ACTOR_401300_FALL_PAIR_BLEND;
        work->jointPairStep   = ACTOR_401300_FALL_PAIR_STEP;
        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer      = work->downFramesBase + ((gRandomLcgState >> 16) & ACTOR_401300_DOWN_RANDOM_TICKS_MASK);
    }
    _actor401300UpdateAnimationEffects(actor);
    if (--work->stateTimer < 0) {
        switch (work->animId) {
            case ACTOR_401300_ANIM_FALL_BACK:
            case ACTOR_401300_ANIM_HOLD_BACK:
                work->state = ACTOR_401300_STATE_RISE_BACK;
                break;
            case ACTOR_401300_ANIM_FALL_FRONT:
            case ACTOR_401300_ANIM_HOLD_FRONT:
            case ACTOR_401300_ANIM_REFALL_FRONT:
                work->state = ACTOR_401300_STATE_RISE_FRONT;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DEATH_BURN;
    }
}

/// Publishes the finished actor's available-HP marker after pending death rewards.
///
/// Requires live work and Enemy storage. Waits while deathPending is nonzero;
/// otherwise writes -999 once so a roaming encounter can reuse the placed task.
static void _actor401300StateDead(Task* task)
{
    _Actor401300Work* work  = task->work;
    Enemy*            enemy = task->spawnArg2.pointer;

    if (enemy->hp != ACTOR_401300_HP_AVAILABLE && work->deathPending == 0) {
        enemy->hp = ACTOR_401300_HP_AVAILABLE;
    }
}

/// Dispatches the Horned Stranger's spawn, frame-update or destroy phase.
///
/// task->state must index the three-entry enemy task table (0 spawn, 1 update,
/// 2 destroy). spawnArg2 borrows the live Enemy work. Copies the table to the
/// stack before invoking the chosen EnemyTaskFunc; performs no bounds check.
static void _actor401300Task(Task* task)
{
    EnemyTaskFuncTable3 taskStates;

    taskStates = D_actor_401300_8013201C;
    taskStates.funcs[task->state](task->spawnArg2.pointer, task);
}
