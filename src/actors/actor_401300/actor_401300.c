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
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"

/// Uniform model-root scale for yaw rebuilds, with 12 fractional bits.
enum { ACTOR_401300_ROOT_SCALE = 0x1964 };

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

/// Two rest/target rotation pairs `func_actor_401300_80133834` blends by
/// `0x200 - t` (in 1/512ths) into coord 7 and coord 8.
extern SVECTOR D_actor_401300_801589F8[2];
extern SVECTOR D_actor_401300_80158A08[2];

/// Data `func_actor_401300_80134454` wires up at init: the enemy parameter
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

/// Twelve vectors `func_actor_401300_80134BA4` picks from by LCG, grouped by
/// `|arg1|`: 0-4 below 0x200, 5-7 above 0x600, else 8-9 / 10-11 by sign.
/// `pad` is the model coordinate index passed to `effectSpawnHit`.
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

/// Gameplay slot `effectSpawn` effects read their model data from; set before
/// each spawn in `func_actor_401300_8013B6E8`.

/// The records closing three of the overlay's model streams, which
/// `func_actor_401300_8013B6E8` points `D_80114B34[5].data.model` at before spawning.
static TmdSource _gActor401300HornedStrangerEffect1;
static TmdSource _gActor401300HornedStrangerBurstHead;
static TmdSource _gActor401300HornedStrangerEffect2;

/// Overlay-data word `func_actor_401300_801397F8` points
/// `D_actor_401300_80158838[16]` at on entering its state.
extern AnimationSet gActor401300Animation20D98;

static void func_actor_401300_80141758(Task* task);
static void func_actor_401300_8014192C(Task* arg0);
static void func_actor_401300_801419B8(Task* arg0);
static void func_actor_401300_80141A60(Task* arg0);
static void func_actor_401300_80141B0C(Task* arg0);
static void func_actor_401300_80141BC8(Task* arg0);
static void func_actor_401300_80141C80(Task* arg0);
static void func_actor_401300_80141C88(Task* arg0);
static void func_actor_401300_80141D50(Task* arg0);
static void func_actor_401300_80141DF4(Task* arg0);
static void func_actor_401300_80141EF8(Task* task);

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
s32                 func_actor_401300_80132554(Task*, s32, ActorCommand*, s32);
s32                 func_actor_401300_80141494(Task*, s32, AnimationPlayRequest*, s32);
s32                 func_actor_401300_80141614(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32                 func_actor_401300_8014148C(Task*, s32, s32, s32);
void                func_actor_401300_80141F2C(Task*);

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
    { 2015, func_actor_401300_8014148C },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_401300_80141494 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, func_actor_401300_80141614 },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_401300_80132554 },
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

TaskDesc D_actor_401300_80158A18 = { { { TASK_BODY_TMD, 96 } }, func_actor_401300_80141F2C, { .model = &_gActor401300HornedStrangerBody } };

SVECTOR ActorContact_ScratchPosition = { 0 };

static s32             func_actor_401300_80132910(Task* arg0, WorldCollisionContact* recs, s16 count);
static void            func_actor_401300_80132BE4(GameLocationKey* session, GfxCoord* coord);
static __inline__ s32  Actor401300_HasHeightClamp(GameLocationKey* session);
static s32             func_actor_401300_80132C78(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3);
static void            func_actor_401300_80133254(Task* arg0);
static void            func_actor_401300_80133324(Task* arg0);
static s32             func_actor_401300_8013346C(_Actor401300Work* work);
static void            func_actor_401300_80133834(Task* arg0, s16 arg1);
static __inline__ void Actor401300_SpawnEff(s32 id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z);
static __inline__ void Actor401300_SpawnEffZero(s32 id, GfxCoord* coord, s32 flags);
static __inline__ void Actor401300_SpawnEffVar(s32* id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z);
static __inline__ void Actor401300_SpawnEffZeroVar(s32* id, GfxCoord* coord, s32 flags);
static __inline__ s32  Actor401300_InRange(Task* arg0);
static __inline__ void Actor401300_ResetAnim(Task* arg0);
static __inline__ void Actor401300_ResetBlendAnim(Task* arg0);
static __inline__ void Actor401300_TickAnim(Task* arg0);
static void            func_actor_401300_80133A3C(Task* arg0);
static __inline__ void Actor401300_BindMatrices(Task* actor);
static __inline__ void Actor401300_InitPose(GfxCoord* coord, _Actor401300Work* work);
static void            func_actor_401300_80134454(Enemy* enemy, Task* actor);
static void            func_actor_401300_80134BA4(Task* arg0, s16 arg1, s32 arg2);
static void            func_actor_401300_80134F90(Task* arg0);
static void            func_actor_401300_80135DDC(Task* arg0);
static void            func_actor_401300_80135FC4(Task* arg0);
static void            func_actor_401300_80136238(Task* arg0);
static void            func_actor_401300_801365F8(Task* arg0);
static __inline__ void Actor401300_MoveBy(GfxCoord* coord, s16 amount);
static __inline__ s32  Actor401300_Abs(s32 x);
static void            func_actor_401300_80136CE8(Task* arg0);
static void            func_actor_401300_801376E4(Task* arg0);
static void            func_actor_401300_80137D78(Task* arg0);
static void            func_actor_401300_80138160(Task* arg0);
static void            func_actor_401300_80138800(Task* arg0);
static void            func_actor_401300_80138B24(Task* arg0);
static void            func_actor_401300_80138CF8(Task* arg0);
static void            func_actor_401300_80138FCC(Task* arg0);
static __inline__ void Actor401300_RescaleYawXZ(GfxCoord* coord, s32 xz, s16 y);
static void            func_actor_401300_80139134(Task* arg0);
static void            func_actor_401300_80139520(Task* arg0);
static void            func_actor_401300_801397F8(Task* arg0);
static void            func_actor_401300_80139AB0(Task* arg0);
static void            func_actor_401300_8013A208(Task* arg0);
static void            func_actor_401300_8013A5C0(Task* arg0);
static void            func_actor_401300_8013AAE8(Task* arg0);
static void            func_actor_401300_8013AE48(Task* arg0);
static void            func_actor_401300_8013B6E8(Task* arg0);
static void            func_actor_401300_8013BB30(Task* arg0);
static void            func_actor_401300_8013CBAC(Task* arg0);
static void            func_actor_401300_8013D2AC(Task* arg0);
static void            func_actor_401300_8013D6C4(Task* arg0);
static __inline__ void Actor401300_ResetActorYaw(Task* actor);
static __inline__ s32  Actor401300_Yaw(GfxCoord* coord);
static __inline__ void Actor401300_MoveForwardSave(McSaveData* save, GfxCoord* coord, s16 amount);
static void            func_actor_401300_8013DADC(Task* arg0);
static __inline__ void Actor401300_MoveForwardNonzeroSave(McSaveData* save, GfxCoord* coord, s16 amount);
static void            func_actor_401300_8013E930(Task* arg0);
static __inline__ void Actor401300_ScaleMatrix(MATRIX* m, s16 scale);
static void            func_actor_401300_8013F628(Task* arg0);
static void            func_actor_401300_80140300(Task* arg0);
static void            func_actor_401300_8014046C(Task* arg0);
static __inline__ s32  Actor401300_InRangeFlag(Task* arg0);
static __inline__ s32  Actor401300_HasRec10000(WorldCollisionContact* recs);
static __inline__ void Actor401300_SnapPlayerHeight(Task* actor);
static void            func_actor_401300_801405DC(Enemy* enemy, Task* actor);
static s32             func_actor_401300_801417F0(Task* arg0);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

s32 func_actor_401300_80132554(Task* arg0, s32 arg1, ActorCommand* arg2, s32 arg3)
{
    _Actor401300Work* work  = arg0->work;
    Enemy*            enemy = arg0->spawnArg2.pointer;

    work->commandBytes[0] = arg2->context.loc.stage;
    work->commandBytes[1] = arg2->context.loc.area;
    work->commandBytes[2] = (u8)arg2->command;
    if (arg2->context.key == 0x301) {
        if (arg2->command == 1) {
            work->state = ACTOR_401300_STATE_DORMANT_SCRIPTED;
            return 1;
        }
    } else if (arg2->context.key == 0xB05) {
        switch (arg2->command) {
            case 0:
                work->state = ACTOR_401300_STATE_HIDDEN;
                return 1;
            case 0xB:
                work->state     = ACTOR_401300_STATE_LEAP_IN;
                work->prevState = -1;
                return 1;
            case 0xC:
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                    work->state = ACTOR_401300_STATE_ALERT;
                    enemy->hp   = D_actor_401300_80141FA0.hpMax;
                    (sceneAcquireBattleRef)(0);
                }
                return 1;
        }
    } else if (arg2->context.key == 0x1D05) {
        switch (arg2->command) {
            case 0:
                work->state = ACTOR_401300_STATE_HIDDEN;
                return 1;
            case 0xB:
                work->state     = ACTOR_401300_STATE_LEAP_IN;
                work->prevState = -1;
                return 1;
        }
    }
    return 0;
}

#include "../../shared/player_detection_reach.inc.c"

/// Pushes the root coordinate by a quarter of each kind 0x10000 / 0x30000 record's
/// offset (skipping 0x3000D), walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update keeps `count`'s sign extension in the loop,
/// as in `Actor01900_Fn03FF8`.
static s32 func_actor_401300_80132910(Task* arg0, WorldCollisionContact* recs, s16 count)
{
    ActorBodyPushScratch* head;
    ActorBodyPushScratch* s;
    ActorBodyPushScratch* blk;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->extra.tmd->coords[1].composeStamp    = GRAPHICS_COORD_DIRTY;
    head                                       = SCRATCH_STACK_CURSOR(ActorBodyPushScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorBodyPushScratch) = blk;
    s                                          = blk;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
    s->position.vx = arg0->extra.tmd->coords[1].workm.t[0];
    s->position.vy = arg0->extra.tmd->coords[1].workm.t[1];
    s->position.vz = arg0->extra.tmd->coords[1].workm.t[2];
    s->hit         = 0;
    for (s->recordIndex = 0; s->recordIndex < count; s->recordIndex++) {
        if (recs[s->recordIndex].key.value == 0) {
            s->marks[s->recordIndex] = ACTOR_BODY_PUSH_MARK_END;
            break;
        }
        s->kind = recs[s->recordIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if ((s->kind == 0x10000 || s->kind == 0x30000) && recs[s->recordIndex].key.value != 0x3000D) {
            if (s->kind == 0x10000) {
                s->hit = 1;
            }
            worldCollisionCalcContactWorldOffset(&s->position, &recs[s->recordIndex], &s->offset);
            s->offsetLength = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->offsetLength = SquareRoot0(s->offsetLength);
            if (s->offsetLength >= 0x140) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x140);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz >> 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz >> 2;
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return s->hit;
}

static void func_actor_401300_80132BE4(GameLocationKey* session, GfxCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->stage && session->area == row->area) {
            lo     = row->minY;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->maxY < offset) {
                coord->coord.t[1] = row->maxY;
            }
            return;
        }
    }
}

static __inline__ s32 Actor401300_HasHeightClamp(GameLocationKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->stage && session->area == row->area) {
            return 1;
        }
    }
    return 0;
}

static s32 func_actor_401300_80132C78(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3)
{
    ActorContactCappedPushScratch* head;
    ActorContactCappedPushScratch* s;
    ActorContactCappedPushScratch* blk;
    s16                            vy;
    SVECTOR*                       step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    head                                                = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    blk                                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = blk;
    s                                                   = blk;
    s->moved                                            = 0;
    if (worldCollisionResolvePushback(rec, &s->delta, arg2, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        s->step.vx = head[-1].delta.fixed.vx.word >> 16;
        s->step.vy = s->delta.fixed.vy.word >> 16;
        s->step.vz = s->delta.fixed.vz.word >> 16;
        if (Actor401300_HasHeightClamp(&gGameSession->location.loc)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x15E) {
                s->step.vy = (vy <= 0) ? -0x15E : 0x15E;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->stepLength      = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->stepLength      = SquareRoot0(s->stepLength);
        step               = &s->step;
        if (s->stepLength >= 0xAF) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xAF);
            gte_ldsv(step);
            gte_gpf12();
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.fixed.vx.word & 0xFFFF) {
            if (s->delta.fixed.vx.word > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.fixed.vz.word & 0xFFFF) {
            if (s->delta.fixed.vz.word > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401300_HasHeightClamp(&gGameSession->location.loc)) {
        func_actor_401300_80132BE4(&gGameSession->location.loc, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.fixed.vx.word != 0 || s->delta.fixed.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return s->moved;
}

#include "../../shared/player_detection_sight.inc.c"

static void func_actor_401300_80133254(Task* arg0)
{
    s32               i;
    _Actor401300Work* work;

    work = arg0->work;

    if (work->appliedAnim != work->animId) {
        for (i = 1; i < 0x13; i++) {
            work->rig.slots[i].rate = work->animRate;
            if (i < 7) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0,
                                           D_actor_401300_8015804C[work->appliedAnim][work->animId]);
            } else if (i >= 9) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0,
                                           D_actor_401300_8015804C[work->appliedAnim][work->animId]);
            }
        }
        work->appliedAnim = work->animId;
    }
}

static void func_actor_401300_80133324(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    s16               weight;
    s16               i;
    _Actor401300Work* work;

    work   = arg0->work;
    weight = work->blendWeight;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->blendRate;
            work->rig.slots[i].rate   = (work->animRate - 3);
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            do {
                animationTickSlotPose(&work->rig.anim, i, &pose, 0);
                animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
                animationApplyPoseWithBlendedRotation(&work->rig.anim, i, &pose, &blendPose, weight, 0x1000 - weight);
            } while (0);
        } else {
            work->rig.slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Returns the sound to play when the current animation (`animId`) reaches
/// one of its cue frames, once per frame reached; `lastCueFrame` holds the last cue
/// frame seen. The frame is re-read at every use: caching it in a local moves
/// CSE's choice of register for the repeat-frame store.
static s32 func_actor_401300_8013346C(_Actor401300Work* work)
{
    switch ((s16)(work->animId - 2)) {
        case 1:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xF) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0004;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x15) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0003;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 0:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x11) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0002;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x1A) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0001;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 23:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xB) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D000C;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xE) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0001;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 24:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xB) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D000C;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 10:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x7) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0005;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 32:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x4) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0005;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 9:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x5) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0005;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 7:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x7) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0006;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 30:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xB) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D000A;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 31:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xD) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D000B;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 25:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xE) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0004;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x14) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0003;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 26:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x12) {
                if (work->lastCueFrame != 0xE) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0004;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 27:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xD) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0002;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xF) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0001;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x11) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0002;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            } else if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x14) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0001;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
        case 28:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x9) {
                if (work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
                    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                    return 0x400D0012;
                }
                work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
                break;
            }
            work->lastCueFrame = 0;
            break;
    }
    return 0;
}

static void func_actor_401300_80133834(Task* arg0, s16 arg1)
{
    SVECTOR* sc;

    sc     = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    sc->vx = D_actor_401300_801589F8[1].vx +
             ((D_actor_401300_801589F8[0].vx - D_actor_401300_801589F8[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_801589F8[1].vy +
             ((D_actor_401300_801589F8[0].vy - D_actor_401300_801589F8[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_801589F8[1].vz +
             ((D_actor_401300_801589F8[0].vz - D_actor_401300_801589F8[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->extra.tmd->coords[7].coord);
    sc->vx = D_actor_401300_80158A08[1].vx +
             ((D_actor_401300_80158A08[0].vx - D_actor_401300_80158A08[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_80158A08[1].vy +
             ((D_actor_401300_80158A08[0].vy - D_actor_401300_80158A08[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_80158A08[1].vz +
             ((D_actor_401300_80158A08[0].vz - D_actor_401300_80158A08[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->extra.tmd->coords[8].coord);
    arg0->extra.tmd->coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(8);
    arg0->extra.tmd->coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns effect `id` on `coord` at the offset (`x`, `y`, `z`).
static __inline__ void Actor401300_SpawnEff(s32 id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    effectSpawn(id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZero(s32 id, GfxCoord* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    effectSpawn(id, coord, flags, &pos);
}

/// `Actor401300_SpawnEff` for an effect id held in a global. Taking the
/// global's address rather than its value is a matching requirement: the `lui`
/// is then evaluated with the arguments and the load itself after them, which
/// is the order the scheduler needs.
static __inline__ void Actor401300_SpawnEffVar(s32* id, GfxCoord* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    effectSpawn(*id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZeroVar(s32* id, GfxCoord* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    effectSpawn(*id, coord, flags, &pos);
}

/// 1 when coordinate 1's view-space Z is in [-299, 2300): the body of
/// `func_actor_401300_801417F0`, with `actorTransformToView` written
/// out so `outp` is initialised after `svp`. The `if` that re-tests `ret` keeps
/// jump from folding the result into a bare `sltiu`.
static __inline__ s32 Actor401300_InRange(Task* arg0)
{
    SVECTOR   out;
    SVECTOR   sv;
    VECTOR    vec;
    s32       flag;
    SVECTOR*  svp;
    GfxCoord* view;
    VECTOR*   vecp;
    s32*      flagp;
    SVECTOR*  outp;
    GfxCoord* p;
    s32       ret;

    memset(&out, 0, 8);
    p     = &arg0->extra.tmd->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
    for (;;) {
        if (p->parent != NULL) {
            if (p != view) {
                gte_SetTransMatrix(&p->coord);
                gte_SetRotMatrix(&p->coord);
                gte_ldv0(svp);
                gte_rtv0tr();
                gte_stlvnl(vecp);
                gte_stflg(flagp);
                sv.vx = vec.vx;
                sv.vy = vec.vy;
                sv.vz = vec.vz;
                p     = p->parent;
                continue;
            }
            outp->vx = sv.vx;
            outp->vy = sv.vy;
            outp->vz = sv.vz;
        }
        break;
    }
    ret = (u16)(out.vz + 0x12B) < 0xA27;
    if (ret != 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

static __inline__ void Actor401300_ResetAnim(Task* arg0)
{
    s32               i;
    _Actor401300Work* work;

    work = arg0->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = work->animRate;
        if (i < 7) {
            animationResetRemappedSlot(&work->rig.anim, i, work->animId, i, i);
        } else if (i >= 9) {
            animationResetRemappedSlot(&work->rig.anim, i, work->animId, i - 2, i);
        }
    }
    work->appliedAnim = work->animId;
}

static __inline__ void Actor401300_ResetBlendAnim(Task* arg0)
{
    s32               i;
    _Actor401300Work* work;

    work              = arg0->work;
    work->blendRate   = 0x30;
    work->blendWeight = 0x800;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = work->blendRate;
        if (i < 7) {
            animationResetRemappedSlot(&work->blend.anim, i, work->blendAnimId, i, i);
        } else if (i >= 9) {
            animationResetRemappedSlot(&work->blend.anim, i, work->blendAnimId, i - 2, i);
        }
    }
}

static __inline__ void Actor401300_TickAnim(Task* arg0)
{
    s32               i;
    _Actor401300Work* work;

    work = arg0->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = work->animRate;
        if (i < 7) {
            animationTickSlot(&work->rig.anim, i);
        } else if (i >= 9) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Per-frame animation and effect update: restarts or ticks the animation
/// slots, eases the yaw of coordinates 5/2 and the blend weight, then spawns
/// the current animation's effects and plays its cue sound.
static void func_actor_401300_80133A3C(Task* arg0)
{
    s32               i;
    s32               snd;
    s16               yaw;
    s32               inRange;
    _Actor401300Work* work;
    Enemy*            enemy;

    /* Set here so CSE keeps `inRange` distinct from the helper's result. */
    inRange = 0;
    work    = arg0->work;
    enemy   = arg0->spawnArg2.pointer;
    if (work->animRequest == ACTOR_401300_ANIM_REQUEST_BLEND) {
        func_actor_401300_80133254(arg0);
        work->animRequest  = ACTOR_401300_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (work->animRequest == ACTOR_401300_ANIM_REQUEST_RESET) {
        Actor401300_ResetAnim(arg0);
        work->animRequest  = ACTOR_401300_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_401300_ANIM_REQUEST_RESET) {
        Actor401300_ResetBlendAnim(arg0);
        work->blendRequest = ACTOR_401300_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    if (work->blendActive == 0) {
        Actor401300_TickAnim(arg0);
    } else {
        func_actor_401300_80133324(arg0);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->blendActive = 0;
        }
    }
    if (work->lookYawTarget > work->lookYaw) {
        if (work->lookYawTarget - work->lookYaw > 0x100) {
            work->lookYaw += 0x100;
        } else {
            work->lookYaw = work->lookYawTarget;
        }
    } else if (-(work->lookYawTarget - work->lookYaw) > 0x100) {
        work->lookYaw -= 0x100;
    } else {
        work->lookYaw = work->lookYawTarget;
    }
    if (work->lookYaw != 0) {
        yaw = work->lookYaw;
        if (work->lookYaw > 0x400) {
            yaw = 0x400;
        }
        if (work->lookYaw < -0x400) {
            yaw = -0x400;
        }
        _actorRenderYawJointInWorld(&arg0->extra.tmd->coords[5], (yaw * 2) / 3);
        _actorRenderYawJointInWorld(&arg0->extra.tmd->coords[2], yaw / 2);
        arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
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
    func_actor_401300_80133834(arg0, work->jointPairBlend);
    snd     = func_actor_401300_8013346C(work);
    inRange = Actor401300_InRange(arg0);
    if (inRange == 1) {
        if (work->animId == 2) {
            if (gDisplayState.animFrame % 6 == 0) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->animId == 3) {
            if ((gDisplayState.animFrame & 1) == inRange) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[18], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (!(gDisplayState.animFrame & 1)) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[15], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->animId == 9 || work->animId == 25 || work->animId == 26) {
            if (gDisplayState.animFrame % 5 == 0) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&gRoomEffectWaterRippleId, &arg0->extra.tmd->coords[15], 0x40, 0, 0x1C2, -100);
            }
        }
        if (snd != 0 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            switch (snd) {
                case 0x400D0001:
                case 0x400D0003:
                    Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[18], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0006;
                    break;
                case 0x400D0002:
                case 0x400D0004:
                    Actor401300_SpawnEffVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[15], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0007;
                    break;
                case 0x400D0005:
                case 0x400D000B:
                    Actor401300_SpawnEffZeroVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&gRoomEffectWaterSprayId, &arg0->extra.tmd->coords[1], 0x1202180);
                    snd = 0x551D0005;
                    break;
            }
        }
    }
    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
        switch (snd) {
            case 0x400D0001:
            case 0x400D0003:
                Actor401300_SpawnEff(0x60054, &arg0->extra.tmd->coords[18], 0x800022C0, 0, 0x15E, -100);
                break;
            case 0x400D0002:
            case 0x400D0004:
                Actor401300_SpawnEff(0x60054, &arg0->extra.tmd->coords[15], 0x800022F0, 0, 0x15E, -100);
                break;
            case 0x400D0005:
            case 0x400D000B:
                Actor401300_SpawnEffZero(0x60054, &arg0->extra.tmd->coords[1], 0x80004800);
                Actor401300_SpawnEffZero(0x60054, &arg0->extra.tmd->coords[1], 0x80004800);
                break;
        }
    }
    if (snd != 0) {
        i = snd | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        sndEvtRequestScriptStart(i, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401300_BindMatrices(Task* actor)
{
    _Actor401300Work* work;
    TmdObject*        obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

/// Rebuilds the root coordinate's scaled Y rotation and seeds the combat
/// defaults while the rotation scratch block is still held.
static __inline__ void Actor401300_InitPose(GfxCoord* coord, _Actor401300Work* work)
{
    ActorScaleRotScratch* top;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    top                                        = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = top - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;
    ang                                        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw                                   = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->rotation, &blk->scale);
    coord->coord.m[0][0]  = (u16)(top - 1)->rotation.m[0][0];
    coord->coord.m[0][1]  = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2]  = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0]  = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1]  = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2]  = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0]  = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1]  = (u16)blk->rotation.m[2][1];
    m22                   = (u16)blk->rotation.m[2][2];
    coord->composeStamp   = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2]  = m22;
    work->bodyPosCursor   = 0;
    work->deathPending    = 0;
    work->playerAnim      = D_actor_401300_80158914;
    work->jointPairBlend  = 0x100;
    work->jointPairTarget = 0x170;
    work->jointPairStep   = 0x20;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

static void func_actor_401300_80134454(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    _Actor401300Work*   work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(_Actor401300Work), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    if ((actor->spawnArg1.value >> 16) != 2) {
        (sceneAcquireBattleRef)(0);
    }
    actor->exitCallback = func_actor_401300_80141758;
    Actor401300_BindMatrices(actor);
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
    animationBindContext(&work->rig.anim, D_actor_401300_80158838, obj,
                         work->rig.poses, work->rig.slots);
    animationBindContext(&work->blend.anim, D_actor_401300_80158838, obj,
                         work->blend.poses, work->blend.slots);
    work->animRequest   = ACTOR_401300_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = 2;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->chaseRate     = 0x10;
    work->animRate      = 0x10;
    if ((s16)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) == 1) {
        work->chaseRate++;
    } else {
        work->chaseRate--;
    }
    func_actor_401300_80133A3C(actor);

    work->gridCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->gridCoord.coord);
    work->gridCoord.coord.t[0]   = actor->extra.tmd->coords->coord.t[0];
    work->gridCoord.coord.t[1]   = actor->extra.tmd->coords->coord.t[1] - 0x15E;
    work->gridCoord.coord.t[2]   = actor->extra.tmd->coords->coord.t[2];
    work->gridCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->gridCoord);

    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = &work->gridCoord;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x3000D;
    work->gridBody.radius           = 0x15E;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->hitCooldown     = 0;
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    body                   = &work->hitBody;
    body->context.contacts = work->hitContacts;
    body->key              = 0x30000;
    body->coord            = &gGfxViewCoord;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->radius           = 0x280;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->attackBody;
    head->coord            = &actor->extra.tmd->coords[3];
    head->context.contacts = work->attackContacts;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x200;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, head);
    worldCollisionInitContacts(head->context.contacts, ARRAY_SIZE(work->attackContacts), 0);

    work->patrolTarget      = 0;
    work->patrolPoints[0].x = actor->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->patrolPoints[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->patrolPoints[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;

    actor->msgTable    = D_actor_401300_80158988;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
    switch ((u8)(actor->spawnArg1.value >> 16)) {
        case 2:
            work->prevState = -1;
            work->state     = ACTOR_401300_STATE_HIDDEN;
            enemy->hp       = -999;
            break;
        case 4:
            work->prevState = -1;
            work->state     = ACTOR_401300_STATE_DORMANT;
            break;
        case 0x20:
            work->prevState = -1;
            work->state     = ACTOR_401300_STATE_WOUNDED;
            enemy->hp       = 0x50;
            break;
        default:
            work->prevState = -1;
            work->state     = ACTOR_401300_STATE_PATROL;
            tmdAllocPrimitiveBuffer(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
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

    Actor401300_InitPose(actor->extra.tmd->coords, work);
    actor->state++;
}

static void func_actor_401300_80134BA4(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*          sc;
    s32               mag;
    _Actor401300Work* work;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401300_80158928[0];
                break;
            case 1:
                *sc = D_actor_401300_80158928[1];
                break;
            case 2:
                *sc = D_actor_401300_80158928[2];
                break;
            case 3:
                *sc = D_actor_401300_80158928[3];
                break;
            default:
                *sc = D_actor_401300_80158928[4];
                break;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401300_80158928[5];
                break;
            case 1:
                *sc = D_actor_401300_80158928[6];
                break;
            default:
                *sc = D_actor_401300_80158928[7];
                break;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[8];
        } else {
            *sc = D_actor_401300_80158928[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[10];
        } else {
            *sc = D_actor_401300_80158928[11];
        }
    }
    work->effectArg.coord      = &arg0->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
    effectSpawnHit(damageGetPlayerAttackEffectId(arg2), &arg0->extra.tmd->coords[sc->pad], sc, &work->effectArg);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_401300_80134F90(Task* arg0)
{
    PlayerStatus*     config = &gPlayerStatus;
    _Actor401300Work* work;
    Enemy*            enemy;
    ActorHitScratch*  head;
    ActorHitScratch*  s;
    GfxCoord*         coord;
    Task*             player;
    SVECTOR*          dir;
    s16               z;
    s32               yaw;
    s32               dx;
    s32               dy;
    s32               dz;
    s32               deathSound;
    s32               deathPan;
    s32               hitSound;
    s32               hitPan;
    s32               mag;
    s16               state;
    s16               effect;
    u32               damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0 && (work->state != ACTOR_401300_STATE_WITHDRAW || work->animId != 0x20)) {
        head      = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s         = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->hitKey = actorFindHit(&head[-1].hitPos, work->hitContacts);
        if (s->hitKey == 0) {
            s->hitKey = actorFindHit(&s->hitPos, work->gridContacts);
        }
        if (s->hitKey != 0) {
            work->field_D1C      = 0;
            work->sidestepCount  = 0;
            work->hitBody.radius = 0x280;
            if (s->hitKey & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            s->hitOffset.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->hitOffset.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z               = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->hitOffset.vz = z;
            yaw             = ratan2(s->hitOffset.vx, z);
            coord           = arg0->extra.tmd->coords;
            s->hitYaw       = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->hitYaw       = _actorAngleNormalizeYaw(s->hitYaw);
            func_actor_401300_80134BA4(arg0, s->hitYaw, s->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            s->criticalEffect   = -1;
            state               = work->state;
            if (state != ACTOR_401300_STATE_FALL_BACK && state != ACTOR_401300_STATE_FALL_FRONT && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_DOWN && state != ACTOR_401300_STATE_RISE_BACK && state != ACTOR_401300_STATE_RISE_FRONT &&
                state != ACTOR_401300_STATE_WOUNDED && state != ACTOR_401300_STATE_STATUS_HOLD) {
                s->towardHit = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->towardHit, s->hitYaw, 0);
                dir = &s->hitOffset;
                gfxReadMatrixZAxis(&s->towardHit, dir);
                VectorNormalSS(dir, dir);
                if (work->blendActive == 1) {
                    gte_lddp(-5);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-10);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                }
                arg0->extra.tmd->coords->coord.t[0]  += s->hitOffset.vx;
                arg0->extra.tmd->coords->coord.t[1]  += s->hitOffset.vy;
                arg0->extra.tmd->coords->coord.t[2]  += s->hitOffset.vz;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            dx                = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            s->toPlayer.vx    = dx;
            dy                = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            s->toPlayer.vy    = dy;
            dz                = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            s->toPlayer.vz    = dz;
            s->playerDistance = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage         = damageComputePlayerAttack(s->hitKey, s->playerDistance, 0, 0);
            if (damageRollCriticalHit(enemy, s->hitKey, 0) != 0) {
                s->critical       = 1;
                s->criticalEffect = 0;
                s->damage        *= 4;
            } else {
                s->critical = 0;
            }
            mag = s->hitYaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->state;
                if (state != ACTOR_401300_STATE_FALL_BACK) {
                    if (state != ACTOR_401300_STATE_FALL_FRONT && state != ACTOR_401300_STATE_DOWN && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_RISE_BACK && state != ACTOR_401300_STATE_RISE_FRONT && state != ACTOR_401300_STATE_WOUNDED && state != ACTOR_401300_STATE_STATUS_HOLD) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->criticalEffect = 4;
                        }
                    }
                }
            }
            damageAccumulateLifeDrainHp(enemy, s->hitKey, s->damage, 0);
            effect = s->criticalEffect;
            if (effect != -1) {
                effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            enemy->hp -= s->damage;
            worldTargetAddReadoutAmount(&enemy->node, s->damage, 0);
            if (work->state == ACTOR_401300_STATE_DORMANT_SCRIPTED) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            if ((work->state == ACTOR_401300_STATE_GRAB_PULL || work->state == ACTOR_401300_STATE_GRAB_STRIKE || work->state == ACTOR_401300_STATE_GRAB_DONE) && config->hp > 0 && work->playerHeld == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400D0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400D0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->hitCooldown = damageGetPlayerAttackHitCooldown(s->hitKey);
            switch (damageGetPlayerAttackReaction(s->hitKey) & 0xFFFF) {
                case 4:
                    state = work->state;
                    if (state != ACTOR_401300_STATE_FALL_BACK && state != ACTOR_401300_STATE_FALL_FRONT && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_WOUNDED && state != ACTOR_401300_STATE_STATUS_HOLD && state != ACTOR_401300_STATE_DOWN) {
                        mag = s->hitYaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->state = ACTOR_401300_STATE_FALL_BACK;
                        } else {
                            work->state = ACTOR_401300_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_NONE:
                case 5:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                case 8:
                case 9:
                    if (work->state == ACTOR_401300_STATE_STATUS_HOLD) {
                        work->prevState = -1;
                    } else if (work->state != ACTOR_401300_STATE_RISE_BACK && work->state != ACTOR_401300_STATE_RISE_FRONT) {
                        if (work->state == ACTOR_401300_STATE_FALL_BACK || work->state == ACTOR_401300_STATE_FALL_FRONT || work->state == ACTOR_401300_STATE_WOUNDED || work->state == ACTOR_401300_STATE_STATUS_HOLD || work->state == ACTOR_401300_STATE_DOWN) {
                            if (work->animId == 0xB || work->animId == 0x17 || work->animId == 8 || work->animId == 0xA) {
                                work->blendActive = 1;
                                work->blendAnimId = 0xB;
                            } else {
                                work->blendActive = 1;
                                work->blendAnimId = 0x22;
                            }
                            work->blendRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                        } else if (s->critical == 1) {
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
                    damageStartEnemyBuildup(enemy, s->hitKey, 0);
                    state = work->state;
                    if (state == ACTOR_401300_STATE_DOWN || state == ACTOR_401300_STATE_WOUNDED || state == ACTOR_401300_STATE_STATUS_HOLD) {
                        work->state     = ACTOR_401300_STATE_STATUS_HOLD;
                        work->prevState = -1;
                    } else {
                        mag = s->hitYaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->state = ACTOR_401300_STATE_FALL_BACK;
                        } else {
                            work->state = ACTOR_401300_STATE_FALL_FRONT;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    state = work->state;
                    if (state == ACTOR_401300_STATE_PATROL || state == ACTOR_401300_STATE_DORMANT || state == ACTOR_401300_STATE_DORMANT_SCRIPTED) {
                        work->state = ACTOR_401300_STATE_FLINCH;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, s->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    state                 = work->state;
                    if (state != ACTOR_401300_STATE_FALL_BACK && state != ACTOR_401300_STATE_FALL_FRONT && state != ACTOR_401300_STATE_WOUNDED && state != ACTOR_401300_STATE_STATUS_HOLD && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_DOWN) {
                        if (state == ACTOR_401300_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state = ACTOR_401300_STATE_REFALL_BACK;
                        } else if (work->state == ACTOR_401300_STATE_RISE_FRONT && work->stateTimer < 0xC) {
                            work->state = ACTOR_401300_STATE_REFALL_FRONT;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->state = ACTOR_401300_STATE_FALL_BACK;
                            } else {
                                work->state = ACTOR_401300_STATE_FALL_FRONT;
                            }
                        }
                    }
                    break;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                enemy->hp -= s->damage;
                worldTargetAddReadoutAmount(&enemy->node, s->damage, 0);
                if (work->state == ACTOR_401300_STATE_CHASE || work->state == ACTOR_401300_STATE_STALK || work->state == ACTOR_401300_STATE_GRAB || work->state == ACTOR_401300_STATE_GRAB_WINDUP) {
                    work->state = ACTOR_401300_STATE_FLINCH;
                } else if (work->state == ACTOR_401300_STATE_STATUS_HOLD) {
                    work->prevState = -1;
                } else {
                    if (work->state == ACTOR_401300_STATE_FALL_BACK || work->state == ACTOR_401300_STATE_FALL_FRONT || work->state == ACTOR_401300_STATE_RISE_BACK || work->state == ACTOR_401300_STATE_RISE_FRONT || work->state == ACTOR_401300_STATE_WOUNDED || work->state == ACTOR_401300_STATE_DOWN) {
                        if (work->animId == 0xB || work->animId == 0x17 || work->animId == 8 || work->animId == 0xA) {
                            work->blendActive = 1;
                            work->blendAnimId = 0xB;
                        } else if (work->animId == 0x22 || work->animId == 0x18 || work->animId == 0x16 || work->animId == 0xC) {
                            work->blendActive = 1;
                            work->blendAnimId = 0x22;
                        } else {
                            work->blendActive = 1;
                            work->blendAnimId = 0xD;
                        }
                    } else {
                        work->blendActive = 1;
                        work->blendAnimId = 0xD;
                    }
                    work->blendRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->hitKey != 0) {
                if ((damageGetPlayerAttackReaction(s->hitKey) & 0xFFFF) == 4) {
                    state = work->animId;
                    if (state == 2 || state == 3 || state == 0x1B || state == 0x1C || state == 0x1D) {
                        work->state = ACTOR_401300_STATE_DEATH_BURST_WALK;
                    } else {
                        work->state = ACTOR_401300_STATE_DEATH_BURST;
                    }
                } else {
                    state = work->state;
                    if (state != ACTOR_401300_STATE_FALL_BACK && state != ACTOR_401300_STATE_FALL_FRONT && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_STATUS_HOLD && state != ACTOR_401300_STATE_WOUNDED && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_DOWN) {
                        if (state == ACTOR_401300_STATE_RISE_BACK && work->stateTimer < 0xC) {
                            work->state     = ACTOR_401300_STATE_REFALL_BACK;
                            work->prevState = -1;
                        } else if (work->state == ACTOR_401300_STATE_RISE_FRONT && work->stateTimer < 0xC) {
                            work->state     = ACTOR_401300_STATE_REFALL_FRONT;
                            work->prevState = -1;
                        } else {
                            mag = s->hitYaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->state = ACTOR_401300_STATE_FALL_BACK;
                            } else {
                                work->state = ACTOR_401300_STATE_FALL_FRONT;
                            }
                        }
                    }
                }
            } else {
                state = work->state;
                if (state != ACTOR_401300_STATE_FALL_BACK && state != ACTOR_401300_STATE_FALL_FRONT && state != ACTOR_401300_STATE_STATUS_HOLD && state != ACTOR_401300_STATE_WOUNDED && state != ACTOR_401300_STATE_REFALL_BACK && state != ACTOR_401300_STATE_REFALL_FRONT && state != ACTOR_401300_STATE_DOWN) {
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

static void func_actor_401300_80135DDC(Task* arg0)
{
    _Actor401300Work* work  = arg0->work;
    Enemy*            enemy = arg0->spawnArg2.pointer;
    TmdObject*        tmd;

    if (work->stateEntered != 0) {
        tmd                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->animRequest     = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate        = 0x10;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->animId == 11 || work->animId == 23) {
            work->animId = 0x17;
        } else if (work->animId == 12 || work->animId == 34 || work->animId == 24) {
            work->animId = 0x18;
        }
        if ((u16)(work->animId - 0x17) >= 2) {
            work->animId = 0x17;
        }
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->animId == 0x17 && (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) >= 6) &&
                 !(work->animId == 0x18 && (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) >= 9));
        work->animRate = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->animRate                        = work->animRate / 2;
    if (work->animRate == 1) {
        work->animRate = -0x10;
    }
    if (work->animRate == -1) {
        work->animRate = 0x10;
    }
    func_actor_401300_80133A3C(arg0);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        work->animRate        = 0x10;
        if ((arg0->spawnArg1.value >> 16) == 0x20) {
            work->state = ACTOR_401300_STATE_WOUNDED;
        } else {
            work->state = ACTOR_401300_STATE_DOWN;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DEATH_BURN;
    }
}

static void func_actor_401300_80135FC4(Task* arg0)
{
    _Actor401300Work* work  = arg0->work;
    Enemy*            enemy = arg0->spawnArg2.pointer;
    s16               i     = 0;
    u16               r;
    TmdObject*        tmd;

    if (work->stateEntered != 0) {
        tmd                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        tmdAllocPrimitiveBuffer(tmd);
        work->animRequest     = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate        = 0x10;
        work->animId          = 0x17;
        work->jointPairTarget = 0x40;
        work->jointPairBlend  = 0x40;
        work->jointPairStep   = 0x20;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->rig.slots[1].status.fields.flags & 0x100) && ++i < 0xFF);
        work->animRate = 0x20;
        return;
    }
    if (++work->stateTimer == 0) {
        work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        r                 = (gRandomLcgState >> 16) % 3;
        switch (r) {
            case 0:
                work->animRate = 0x20;
                break;
            case 1:
                work->animRate = 0x30;
                break;
            case 2:
            default:
                work->animRate = 0x40;
                break;
        }
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->animRate = 0x10;
    } else if (work->stateTimer > 0) {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->animRate                        = work->animRate / 2;
        if (work->animRate == 1) {
            work->animRate = -0x10;
        }
        if (work->animRate == -1) {
            work->animRate = 0x10;
        }
        func_actor_401300_80133A3C(arg0);
    } else if (work->blendActive == 1 || !(work->rig.slots[1].status.fields.flags & 0x100)) {
        work->animRate = 0x10;
        func_actor_401300_80133A3C(arg0);
    }
    if (work->stateTimer >= 7) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer = -((gRandomLcgState >> 16) & 0xFF);
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DOWN;
    }
}

static void func_actor_401300_80136238(Task* arg0)
{
    _Actor401300Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate          = 0x10;
        work->animId            = 9;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        work->hitBody.radius = 0x280;
        sceneEngageBattle(1);
        work->jointPairTarget = 0x200;
        work->jointPairStep   = 0x20;
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == 0x200) {
            work->jointPairStep   = 0x80;
            work->jointPairTarget = 0x190;
        } else {
            work->jointPairTarget = 0x200;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (detectSightBlocked(arg0) == 1 && (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            work->state = ACTOR_401300_STATE_WITHDRAW;
        } else {
            work->state = ACTOR_401300_STATE_CHASE;
        }
    }
    aim->turn           = _actorAngleTurnToPlayer(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn > 0x10) {
        aim->turn = 0x10;
    }
    if (aim->turn < -0x10) {
        aim->turn = -0x10;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    func_actor_401300_80133A3C(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_801365F8(Task* arg0)
{
    _Actor401300Work*       work;
    GameActor*              player;
    PlayerStatus*           config;
    TmdObject*              obj;
    GfxCoord*               coord;
    GfxCoord*               c1;
    GfxCoord*               c2;
    _Actor401300RunScratch* head;
    _Actor401300RunScratch* run;
    SVECTOR*                delta;
    s32                     angle;
    s32                     dist;
    s32                     dx;
    s32                     dy;
    s32                     dz;
    s32                     mask;

    config = &gPlayerStatus;
    work   = arg0->work;
    player = (GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    mask   = 0xF0;
    if (((arg0->spawnArg1.value >> 16) & mask) == 0x10) {
        work->state = ACTOR_401300_STATE_STALK;
        return;
    }
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = 3;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->jointPairTarget = 0x40;
        work->jointPairStep   = 0x10;
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == 0x40) {
            work->jointPairTarget = 0x80;
            work->jointPairStep   = 0x10;
        } else {
            work->jointPairTarget = 0x40;
        }
    }
    work->stateTimer++;
    head                                         = SCRATCH_STACK_CURSOR(_Actor401300RunScratch);
    delta                                        = &head[-1].delta;
    c1                                           = arg0->extra.tmd->coords;
    head[-1].delta.vx                            = gPlayerStatus.coordMtx->t[0] - c1->coord.t[0];
    delta->vy                                    = gPlayerStatus.coordMtx->t[1] - c1->coord.t[1];
    delta->vz                                    = gPlayerStatus.coordMtx->t[2] - c1->coord.t[2];
    SCRATCH_STACK_CURSOR(_Actor401300RunScratch) = head - 1;
    run                                          = head - 1;
    arg0->extra.tmd->coords->composeStamp        = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    c2                  = arg0->extra.tmd->coords;
    angle               = ratan2(head[-1].delta.vx, delta->vz);
    run->turn           = _actorAngleNormalizeYaw(angle - ratan2(-c2->coord.m[2][0], c2->coord.m[2][2]));
    work->lookYawTarget = run->turn;
    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, (work->chaseRate + 2) * 30 * 1.5f / 18.0f)) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, (work->chaseRate + 2) * 30 * 1.5f / 18.0f);
    }
    if (run->turn > 0x30) {
        run->turn = 0x30;
    } else if (run->turn < -0x30) {
        run->turn = -0x30;
    } else {
        run->offset.vx = dx = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        run->offset.vy = dy = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        run->offset.vz = dz = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dist                = SquareRoot0(dx * dx + dy * dy + dz * dz);
        run->distance       = dist;
        if (player->mode != GAME_ACTOR_MODE_SCRIPTED && work->stateTimer >= 0x28) {
            if (dist > 4000) {
                work->state = ACTOR_401300_STATE_CHARGE;
            } else if (dist > 2000) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) < 5) {
                    work->state = ACTOR_401300_STATE_CHARGE;
                } else {
                    work->state = ACTOR_401300_STATE_LEAP;
                }
            } else if (dist < 1000) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) < 7) {
                    work->state = ACTOR_401300_STATE_STRIKE_A;
                } else {
                    work->state = ACTOR_401300_STATE_STRIKE_B;
                }
            }
        }
    }
    coord      = arg0->extra.tmd->coords;
    run->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, run->turn, 1);

    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg0->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300RunScratch);
}

static __inline__ void Actor401300_MoveBy(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* v;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        v                             = vec;
        if (amount != 0) {
            gfxReadMatrixZAxis(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(v);
            gte_gpf12();
            gte_stsv(v);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static __inline__ s32 Actor401300_Abs(s32 x)
{
    if (x < 0) {
        x = -x;
    }
    return x;
}

static void func_actor_401300_80136CE8(Task* arg0)
{
    _Actor401300Work*       work;
    Enemy*                  enemy;
    TmdObject*              obj;
    GfxCoord*               coord;
    _Actor401300RunScratch* head;
    _Actor401300RunScratch* blk;
    _Actor401300RunScratch* run;
    s32                     z;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->blendActive       = 0;
        work->animId            = 3;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate          = work->chaseRate;
        func_actor_401300_80133A3C(arg0);
        work->jointPairTarget = 0x40;
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->jointPairStep   = 0x10;
        z                     = arg0->extra.tmd->coords->coord.t[2];
        if (z > 0x1B58) {
            work->withdrawPoint.vx  = -0xB54;
            work->withdrawPoint.vz  = 0x2198;
            work->withdrawPoint.vy  = 0;
            work->withdrawPoint.pad = 0x400;
        } else if (z > 0x1068) {
            work->withdrawPoint.vx  = 0;
            work->withdrawPoint.vy  = 0;
            work->withdrawPoint.vz  = 0x189C;
            work->withdrawPoint.pad = 0;
        } else if (z > 0x384) {
            work->withdrawPoint.vx  = 0x12C0;
            work->withdrawPoint.vy  = 0;
            work->withdrawPoint.vz  = 0x1AF4;
            work->withdrawPoint.pad = 0;
        } else if (z > -0x898) {
            work->withdrawPoint.vx  = 0x12C0;
            work->withdrawPoint.vz  = -0x1388;
            work->withdrawPoint.vy  = 0;
            work->withdrawPoint.pad = 0x800;
        } else {
            work->withdrawPoint.vx  = -0xB4;
            work->withdrawPoint.vy  = 0;
            work->withdrawPoint.vz  = -0x960;
            work->withdrawPoint.pad = 0;
        }
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == 0x40) {
            work->jointPairTarget = 0x80;
            work->jointPairStep   = 0x10;
        } else {
            work->jointPairTarget = 0x40;
        }
    }
    head = SCRATCH_STACK_CURSOR(_Actor401300RunScratch);
    blk  = head - 1;
    work->stateTimer++;
    SCRATCH_STACK_CURSOR(_Actor401300RunScratch) = blk;
    func_actor_401300_80133A3C(arg0);
    run = blk;
    switch (work->animId) {
        case 3:
            blk->delta.vx      = work->withdrawPoint.vx - arg0->extra.tmd->coords->coord.t[0];
            head[-1].offset.vx = blk->delta.vx;
            blk->delta.vy      = work->withdrawPoint.vy - arg0->extra.tmd->coords->coord.t[1];
            blk->offset.vy     = blk->delta.vy;
            blk->delta.vz      = work->withdrawPoint.vz - arg0->extra.tmd->coords->coord.t[2];
            blk->offset.vz     = blk->delta.vz;
            blk->distance      = SquareRoot0(head[-1].offset.vx * head[-1].offset.vx + blk->offset.vy * blk->offset.vy + blk->offset.vz * blk->offset.vz);
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) != 1) {
                func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
            coord               = arg0->extra.tmd->coords;
            run->turn           = _actorAngleNormalizeYaw(ratan2(head[-1].delta.vx, head[-1].delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            work->lookYawTarget = run->turn;
            if (run->turn > 0x30) {
                run->turn = 0x30;
            } else if (run->turn < -0x30) {
                run->turn = -0x30;
            } else if ((run->distance < 0x898 && Actor401300_Abs(_actorAngleNormalizeYaw(ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]) - work->withdrawPoint.pad)) < 0x200) || work->stateTimer > 0xB4) {
                work->animId      = 0x20;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                sndEvtRequestScriptStart(SOUND_NEO_ARK_WOODLAND_STRANGER_WITHDRAW, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords), (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            run->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, run->turn, 1);
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, (s16)((float)((work->chaseRate + 2) * 30) * 1.5f / 18.0f)) != 0) {
                Actor401300_MoveBy(arg0->extra.tmd->coords, (s16)((float)((work->chaseRate + 2) * 30) * 1.5f / 18.0f));
            }
            _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            break;
        case 0x20:
            run->turn = ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, run->turn, 1);
            _actorMovementStepForward(arg0->extra.tmd->coords, 0x12C);
            _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->state = ACTOR_401300_STATE_HIDDEN;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, enemy->hp, 0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300RunScratch);
}

static void func_actor_401300_801376E4(Task* arg0)
{
    _Actor401300Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* head;
    ActorChaseScratch* chase;

    work = arg0->work;
    if (work->stateEntered != 0) {
        head                                                      = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        obj                                                       = arg0->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = head - 1;
        chase                                                     = head - 1;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 3;
        work->blendActive       = 0;
        work->lookYawTarget     = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
        coord               = arg0->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(head[-1].delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing              = arg0->extra.tmd->coords;
        chase->heading      = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->turnYaw       = chase->heading;
        work->turnYawTarget = chase->heading + (u16)chase->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    chase                                   = head - 1;
    func_actor_401300_80133A3C(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->turnYaw == work->turnYawTarget) {
        if (work->field_D1C < 2 || _actorRangeOutsideRadiusXZ(&chase->delta, 0x384)) {
            work->state = ACTOR_401300_STATE_WITHDRAW;
        } else {
            work->state = ACTOR_401300_STATE_GRAB;
        }
    }
    if (work->turnYaw > work->turnYawTarget) {
        work->turnYaw -= 0x89;
        if (work->turnYaw < work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    if (work->turnYaw < work->turnYawTarget) {
        work->turnYaw += 0x89;
        if (work->turnYaw > work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, work->turnYaw, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x28) != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, 0x28);
        }
    } else {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x14) != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, 0x14);
        }
    }
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) != 1) {
        func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_80137D78(Task* arg0)
{
    _Actor401300Work*  work;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->state = ACTOR_401300_STATE_STALK;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x140;
        work->stateTimer        = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = 1;
            } else {
                work->sidestepSide = -1;
            }
        }
        if (work->sidestepSide == 1) {
            work->animId = 0x15;
            if (work->sidestepCount == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->sidestepAngle + angle;
            } else {
                aim->turn += work->sidestepAngle;
            }
            work->sidestepSide = -1;
        } else {
            work->animId = 0x14;
            if (work->sidestepCount == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->sidestepAngle;
            } else {
                aim->turn -= work->sidestepAngle;
            }
            work->sidestepSide = 1;
        }
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate    = 0xC;
        work->blendActive = 0;
        func_actor_401300_80133A3C(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->sidestepDir;
        gfxReadMatrixZAxis(&mat, dir);
        VectorNormalSS(dir, dir);
        work->sidestepStep = 0xDE;
        work->sidestepCount++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&aim->delta);
    } else {
        gte_lddp(work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&aim->delta);
    }
    if (work->stateTimer >= 0xC && work->stateTimer < 0x16) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
    }
    if (++work->stateTimer >= 0x1E) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_80138160(Task* arg0)
{
    SVECTOR           pos;
    _Actor401300Work* work;
    Enemy*            enemy;
    GfxCoord*         coord;
    GameActor*        player;
    PlayerStatus*     config;
    SVECTOR*          p;
    s16               angle;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    config = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                = 0x10;
        work->animId                  = 4;
        func_actor_401300_80133A3C(arg0);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, _actorAngleTurnToPlayer(arg0, &pos, config), 0);
        _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
        pos.vx                                = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                                = 0;
        pos.vz                                = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x10 && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        angle = actorMatrixPositionYaw(arg0, &pos, gPlayerStatus.coordMtx);
        if (abs(angle) < 0x10 && !_actorRangeOutsideRadiusXZ(&pos, 0x44C)) {
            work->playerAnim.source.sets      = D_actor_401300_801588F0;
            work->playerButtonHold.pressCount = 8;
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                work->state                  = ACTOR_401300_STATE_GRAB_PULL;
                work->playerHeld             = 1;
                work->playerAnim.animationId = 1;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                work->playerMove.displacement.vz   = 0;
                work->playerMove.displacement.vy   = 0;
                work->playerMove.displacement.vx   = 0;
                work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                work->playerMove.keepControl       = 1;
                work->playerAnimFrames             = 0;
            }
        }
    }
    if (work->animId == 4 && (work->rig.slots[1].status.fields.flags & 0x100)) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) > 0x10) {
        p      = &pos;
        pos.vx = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy = 0;
        pos.vz = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        if (!_actorRangeOutsideRadiusXZ(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12();
            gte_stsv(p);
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[0]                    += pos.vx;
            coord                                 = arg0->extra.tmd->coords;
            coord->coord.t[2]                    += pos.vz;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static void func_actor_401300_80138800(Task* arg0)
{
    SVECTOR           dir;
    _Actor401300Work* work  = arg0->work;
    Enemy*            enemy = arg0->spawnArg2.pointer;
    Task*             player;
    SVECTOR*          pdir;

    if (work->stateEntered != 0) {
        player                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->hitBody.radius                    = 0x280;
        work->attackBody.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags                   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags           = 0;
        work->animRequest                       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                          = 0x10;
        work->animId                            = 5;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(player->extra.tmd->coords);
        work->playerPlacement.pos.vx = player->extra.tmd->coords->coord.t[0];
        work->playerPlacement.pos.vy = player->extra.tmd->coords->coord.t[1];
        work->playerPlacement.pos.vz = player->extra.tmd->coords->coord.t[2];
        pdir                         = &dir;
        dir.vx                       = (u16)arg0->extra.tmd->coords->coord.t[0] - (u16)player->extra.tmd->coords->coord.t[0];
        dir.vy                       = 0;
        dir.vz                       = (u16)arg0->extra.tmd->coords->coord.t[2] - (u16)player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        arg0->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + dir.vx;
        arg0->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + dir.vz;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->playerPlacement.rot.vx          = 0;
        work->playerPlacement.rot.vy          = ratan2(dir.vx, dir.vz);
        work->playerPlacement.rot.vz          = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
    }
    func_actor_401300_80133A3C(arg0);
    gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
    gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
    if (work->animId == 5 && (work->rig.slots[1].status.fields.flags & 0x100)) {
        work->effectArg.coord      = &arg0->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = 0x300;
        work->effectArg.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[5], NULL, &work->effectArg);
        work->state = ACTOR_401300_STATE_GRAB_STRIKE;
    }
}

static void func_actor_401300_80138B24(Task* arg0)
{
    _Actor401300Work* work   = arg0->work;
    Enemy*            enemy  = arg0->spawnArg2.pointer;
    Task*             player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (work->stateEntered != 0) {
        work->animRate    = 0x10;
        work->animId      = 6;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
        if ((s16)taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0) == 1) {
            ((GameActor*)player->work)->state = 0xA;
        }
        work->playerAnim.animationId = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->playerAnimFrames = 0;
    }
    if (work->rig.slots[1].status.fields.flags & 2) {
        work->effectArg.coord      = &arg0->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = 0x300;
        work->effectArg.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[5], NULL, &work->effectArg);
        work->state = ACTOR_401300_STATE_GRAB_DONE;
    }
    work->grabAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    func_actor_401300_80133A3C(arg0);
    gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
    gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
}

static void func_actor_401300_80138CF8(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animId                  = 0xA;
        work->blendActive             = 0;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->jointPairTarget = 0x20;
        work->jointPairStep   = 8;
        work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->animId == 0xA && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x57) != 0) {
        _actorMovementStepForward(arg0->extra.tmd->coords, -0x57);
    }
    func_actor_401300_80133A3C(arg0);
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (work->animId == 0xA) {
            work->animId      = 0xB;
            work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
            func_actor_401300_80133A3C(arg0);
        }
        if ((work->rig.slots[1].status.fields.flags & 0x100) && work->animId == 0xB) {
            work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            if (enemy->hp <= 0) {
                work->state = ACTOR_401300_STATE_DEATH_BURN;
            } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = ACTOR_401300_STATE_STATUS_HOLD;
            } else {
                work->state = ACTOR_401300_STATE_DOWN;
            }
        }
    }
}

static void func_actor_401300_80138FCC(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animId                  = 0xC;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->jointPairTarget = 0x20;
        work->jointPairStep   = 8;
        work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_401300_80133A3C(arg0);
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ACTOR_401300_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ACTOR_401300_STATE_STATUS_HOLD;
        } else {
            work->state = ACTOR_401300_STATE_DOWN;
        }
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, scaled by `xz` on X/Z
/// and `y` on Y. `_actorRenderRescaleYaw` with a separate Y scale.
static __inline__ void Actor401300_RescaleYawXZ(GfxCoord* coord, s32 xz, s16 y)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = xz;
    blk->scale.vy = y;
    blk->scale.vz = xz;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Collapse state: spawns effect 0x600A5 at the actor's view-space position on
/// frame 30, switches the light mode on 30/42, and from frame 26 squashes the
/// root coordinate's Y scale; state 0x24 follows after frame 64.
static void func_actor_401300_80139134(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    SVECTOR           pos;
    s16               t;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer              = 0;
        work->animRate                = 8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->stateTimer <= 0x400) {
        switch (++work->stateTimer) {
            case 30:
                gfxSetRotIdentity(&work->burnCoord.coord);
                pos.vx = 0;
                pos.vy = 0;
                pos.vz = 0;
                actorTransformToView(&arg0->extra.tmd->coords[2], &pos);
                work->burnCoord.parent       = &gGfxViewCoord;
                work->burnCoord.coord.t[0]   = pos.vx;
                work->burnCoord.coord.t[1]   = arg0->extra.tmd->coords->coord.t[1];
                work->burnCoord.coord.t[2]   = pos.vz;
                work->burnCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&work->burnCoord);
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                effectSpawn(EFFECT_CORPSE_BURN, &work->burnCoord, 3, NULL);
                break;
            case 48:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 42:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 64:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        t = work->stateTimer;
        if (t >= 0x1A) {
            Actor401300_RescaleYawXZ(arg0->extra.tmd->coords, 0x1964, 0x1964 - (t - 0x14) * 16);
        }
        if (work->stateTimer > 0x40 && work->playerHeld == 0) {
            work->state = ACTOR_401300_STATE_DEAD;
        }
    }
}

static void func_actor_401300_80139520(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    SVECTOR           delta;
    SVECTOR*          d;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj        = arg0->extra.tmd;
        enemy      = arg0->spawnArg2.pointer;
        obj->flags = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = 0xE;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                = work->chaseRate;
    }
    if (work->stateTimer > 0x960) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->stateTimer++;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(d, 3000)) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {
        sceneEngageBattle(1);
        work->state = ACTOR_401300_STATE_ALERT;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->animId == 0xE && (work->rig.slots[1].status.fields.flags & 2)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->animId      = 0xF;
            work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
            func_actor_401300_80133A3C(arg0);
        }
    }
    if (work->animId == 0xF && (work->rig.slots[1].status.fields.flags & 0x100)) {
        work->animId      = 0xE;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_801397F8(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    SVECTOR           delta;
    SVECTOR*          d;
    s32               sound;
    s32               pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        D_actor_401300_80158838[16] = &gActor401300Animation20D98;
        work->animId                = 0x10;
        work->animRequest           = ACTOR_401300_ANIM_REQUEST_RESET;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x10;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
    } else if (work->stateTimer == 0) {
        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51030008;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->stateTimer = 1;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 4 && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
        work->effectArg.coord      = arg0->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = 0x300;
        work->effectArg.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->effectArg);
    }
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord              = arg0->extra.tmd->coords;
    d                  = &delta;
    delta.vx           = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy              = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(d, 3000)) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sceneEngageBattle(1);
        work->state = ACTOR_401300_STATE_ALERT;
    }
    if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE) {
        sceneEngageBattle(1);
        work->state = ACTOR_401300_STATE_ALERT;
    }
}

static void func_actor_401300_80139AB0(Task* arg0)
{
    _Actor401300Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* turn;
    GfxCoord*         facing;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 2;
        work->jointPairTarget   = 0x60;
        work->jointPairStep     = 8;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    if (work->jointPairTarget == work->jointPairBlend) {
        if (work->jointPairTarget == 0x60) {
            work->jointPairTarget = 0x20;
        } else {
            work->jointPairTarget = 0x60;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->delta.vx = work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 0xA0)) {
        if (work->patrolTarget == 0) {
            work->patrolTarget = 1;
        } else {
            work->patrolTarget = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    coord               = arg0->extra.tmd->coords;
    turn->angle         = _actorAngleNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->lookYawTarget = turn->angle;
    if (turn->angle > 0x20) {
        turn->angle = 0x20;
    }
    if (turn->angle < -0x20) {
        turn->angle = -0x20;
    }
    facing       = arg0->extra.tmd->coords;
    turn->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    if (work->blendActive == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0xA) != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, 0xA);
        }
    }
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &turn->delta);
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 0x7D0)) {
        work->state = ACTOR_401300_STATE_ALERT;
    } else if (!_actorRangeOutsideRadiusXZ(&turn->delta, 0xFA0)) {
        coord       = arg0->extra.tmd->coords;
        turn->angle = _actorAngleNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        if (ABS(turn->angle) < 0x300) {
            work->state = ACTOR_401300_STATE_ALERT;
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void func_actor_401300_8013A208(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* turn;
    u16               next;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy             = arg0->spawnArg2.pointer;
        obj               = arg0->extra.tmd;
        work->animId      = 0x12;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        obj->flags        = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x1E;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn                = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle         = _actorAngleTurnToPlayer(arg0, &turn->delta, &gPlayerStatus);
    work->lookYawTarget = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, work->slideStep) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, work->slideStep);
    }
    if (work->slideStep > 0) {
        next            = work->slideStep - 0xA;
        work->slideStep = next;
        if ((s16)next < 0) {
            work->slideStep = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->rig.slots[1].status.fields.flags & 0x100) || work->slideStep == 0) {
        work->state = ACTOR_401300_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void func_actor_401300_8013A5C0(Task* arg0)
{
    _Actor401300Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x16;
        work->animId            = 2;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    func_actor_401300_80133A3C(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn           = _actorAngleTurnToPlayer(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->animId == 2) {
        work->animRate    = 0x16;
        work->animId      = 0x11;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        func_actor_401300_80133A3C(arg0);
    }
    if (aim->turn > 0x80) {
        aim->turn = 0x80;
    }
    if (aim->turn < -0x80) {
        aim->turn = -0x80;
    } else {
        aim->turn = aim->turn >> 1;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 0x11) {
        work->stateTimer++;
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x10) != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, -0x10);
        }
        if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
            func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->stateTimer >= 0x13) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->state = ACTOR_401300_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013AAE8(Task* arg0)
{
    _Actor401300Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 0x13;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[1].status.fields.flags & 0x100) || work->stateTimer >= 0xB) {
        work->state = ACTOR_401300_STATE_GRAB;
    }
    aim->turn           = _actorAngleTurnToPlayer(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn > 0x20) {
        aim->turn = 0x20;
    }
    if (aim->turn < -0x20) {
        aim->turn = -0x20;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    func_actor_401300_80133A3C(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013AE48(Task* arg0)
{
    _Actor401300Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate          = 8;
        work->animId            = 0x13;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = _actorAngleTurnToPlayer(arg0, &aim->delta, &gPlayerStatus);
    if (work->lookYawTarget < aim->turn) {
        if (aim->turn - work->lookYawTarget > 0x28) {
            work->lookYawTarget += 0x28;
        } else {
            work->lookYawTarget = aim->turn;
        }
    } else if (work->lookYawTarget - aim->turn > 0x28) {
        work->lookYawTarget -= 0x28;
    } else {
        work->lookYawTarget = aim->turn;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    func_actor_401300_80133A3C(arg0);
    if (work->stateTimer < 0x32) {
        gfxRotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100, GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
    } else {
        gfxRotMatrixX(&arg0->extra.tmd->coords[1].coord, 0x40 >> ((work->stateTimer - 0x31) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, 0x80 >> ((work->stateTimer - 0x30) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, 0x80 >> ((work->stateTimer - 0x2F) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x80 >> ((work->stateTimer - 0x2E) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
        gfxRotMatrixX(&arg0->extra.tmd->coords[5].coord, 0x100 >> ((work->stateTimer - 0x31) / 4), GRAPHICS_ROTATION_COMPOSE);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&arg0->extra.tmd->coords[4]);
        aim->turn = _actorAngleTurnToPlayer(arg0, &aim->delta, &gPlayerStatus);
        if (aim->turn > 0x24) {
            aim->turn = 0x24;
        } else if (aim->turn < -0x24) {
            aim->turn = -0x24;
        }
        if (ABS(aim->turn) < 0x24) {
            work->state = ACTOR_401300_STATE_CHASE;
        }
        coord      = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013B6E8(Task* arg0)
{
    SVECTOR           vec;
    _Actor401300Work* work;
    Enemy*            enemy;
    u16               next;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.radius          = 0x280;
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
    }
    next             = work->stateTimer + 1;
    work->stateTimer = next;
    if ((s16)next == 3) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect1;
        vec.vz                   = 0x64;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
    }
    if (work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect1;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec), enemy);
    }
    if (work->stateTimer == 7) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect2;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
    }
    if (work->stateTimer == 8) {
        D_80114B34[5].data.model = &_gActor401300HornedStrangerBurstHead;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
    }
    if (work->stateTimer >= 0x3D && work->playerHeld == 0) {
        work->state = ACTOR_401300_STATE_DEAD;
    }
}

static void func_actor_401300_8013BB30(Task* arg0)
{
    SVECTOR           vec;
    _Actor401300Work* work;
    Enemy*            enemy;
    u16               next;
    s16               cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->hitBody.radius          = 0x280;
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        work->animId                  = 2;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate                = 0x10;
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        work->stateTimer = 0;
    }
    next             = work->stateTimer + 1;
    work->stateTimer = next;
    switch (work->animId) {
        case 2:
            if ((s16)next >= 0x10 && (work->rig.slots[1].status.fields.flags & 2)) {
                work->animId      = 0x23;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->animRate    = 0x10;
                work->blendActive = 0;
            }
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0xA) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 0xA);
            }
            _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
            if (work->stateTimer == 3) {
                D_80114B34[5].data.model = &_gActor401300HornedStrangerBurstHead;
                vec.vz                   = 0x64;
                vec.vy                   = 0;
                vec.vx                   = 0;
                actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), enemy);
            }
            if (work->stateTimer == 5) {
                D_80114B34[5].data.model = &_gActor401300HornedStrangerEffect2;
                actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
            }
            break;
        case 0x23:
            if (!(work->rig.slots[1].status.fields.flags & 0x100)) {
                work->stateTimer = 0;
            }
            switch (work->stateTimer) {
                case 3:
                    break;
                case 30:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    vec.vx = 0;
                    vec.vy = 0;
                    vec.vz = 0;
                    actorTransformToView(arg0->extra.tmd->coords + 2, &vec);
                    work->burnCoord.parent       = &gGfxViewCoord;
                    work->burnCoord.coord.t[0]   = vec.vx;
                    work->burnCoord.coord.t[1]   = arg0->extra.tmd->coords->coord.t[1];
                    work->burnCoord.coord.t[2]   = vec.vz;
                    work->burnCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(&work->burnCoord);
                    effectSpawn(EFFECT_CORPSE_BURN, &work->burnCoord, 2, NULL);
                    break;
                case 48:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 42:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 64:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->state            = ACTOR_401300_STATE_DEAD;
                    break;
            }
            cur = work->stateTimer;
            if (cur >= 0x1A) {
                Actor401300_RescaleYawXZ(arg0->extra.tmd->coords, 0x1964, 0x1964 - (cur - 0x14) * 16);
            }
            break;
    }
    func_actor_401300_80133A3C(arg0);
    actorResetYaw(arg0->extra.tmd->coords + 2);
    actorResetYaw(arg0->extra.tmd->coords + 3);
    actorResetYaw(arg0->extra.tmd->coords + 4);
    actorResetYaw(arg0->extra.tmd->coords + 5);
    actorResetYaw(arg0->extra.tmd->coords + 6);
    actorResetYaw(arg0->extra.tmd->coords + 7);
    actorResetYaw(arg0->extra.tmd->coords + 8);
    actorResetYaw(arg0->extra.tmd->coords + 9);
    actorResetYaw(arg0->extra.tmd->coords + 10);
    actorResetYaw(arg0->extra.tmd->coords + 11);
    actorResetYaw(arg0->extra.tmd->coords + 12);
}

static void func_actor_401300_8013CBAC(Task* arg0)
{
    _Actor401300Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    GfxCoord*          root;
    GfxCoord*          root2;
    PlayerStatus*      config;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    s16                yaw;
    s32                angle;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x24;
        work->animId            = 2;
        work->blendActive       = 0;
        work->field_D1C         = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        sceneEngageBattle(1);
        work->stateTimer    = 0;
        work->blockedFrames = 0;
    }
    work->stateTimer++;
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
        func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    config                                = &gPlayerStatus;
    root                                  = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = config->coordMtx->t[0] - root->coord.t[0];
    aim->delta.vy                         = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz                         = config->coordMtx->t[2] - root->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    aim->playerYaw      = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                 (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    root2               = arg0->extra.tmd->coords;
    head[-1].delta.vx   = config->coordMtx->t[0] - root2->coord.t[0];
    aim->delta.vy       = config->coordMtx->t[1] - root2->coord.t[1];
    aim->delta.vz       = config->coordMtx->t[2] - root2->coord.t[2];
    yaw                 = ratan2(head[-1].delta.vx, aim->delta.vz) + 0x800;
    aim->yawFromPlayer  = yaw;
    aim->yawFromPlayer  = _actorAngleNormalizeYaw(yaw);
    coord               = arg0->extra.tmd->coords;
    angle               = ratan2(aim->delta.vx, aim->delta.vz);
    aim->turn           = _actorAngleNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->lookYawTarget = aim->turn;
    if (aim->turn < 0x200) {
        if (!_actorRangeOutsideRadiusXZ(&aim->delta, 0x44C)) {
            work->state = ACTOR_401300_STATE_GRAB;
        }
    }
    if (aim->turn > 0x20) {
        aim->turn = 0x20;
    }
    if (aim->turn < -0x20) {
        aim->turn = -0x20;
    }
    facing     = arg0->extra.tmd->coords;
    aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 2) {
        if (work->blendActive == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x16) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 0x16);
            }
        } else {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 5) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 5);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->animId      = 2;
        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void func_actor_401300_8013D2AC(Task* arg0)
{
    _Actor401300Work*  work;
    Enemy*             enemy;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    ActorChaseScratch* aim;
    s32                angle;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 0x19;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->attackBody.key  = damagePackEnemyAttackKey(enemy, 0);
        work->jointPairTarget = 0x200;
        work->jointPairStep   = 0x80;
        return;
    }
    func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
    if (work->stateTimer >= 0x29) {
        work->jointPairTarget = 0;
        work->jointPairStep   = 0x40;
    }
    work->stateTimer++;
    switch (work->stateTimer) {
        case 0x19:
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 0x28:
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (work->stateTimer < 0xE) {
        coord               = arg0->extra.tmd->coords;
        angle               = ratan2(aim->delta.vx, aim->delta.vz);
        aim->turn           = _actorAngleNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        coord2     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_401300_8013D6C4(Task* arg0)
{
    _Actor401300Work*  work;
    Enemy*             enemy;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    ActorChaseScratch* aim;
    s32                angle;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 0x1A;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->attackBody.key  = damagePackEnemyAttackKey(enemy, 1);
        work->jointPairTarget = 0x200;
        work->jointPairStep   = 0x80;
        return;
    }
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (work->stateTimer >= 0x26) {
        work->jointPairTarget = 0;
        work->jointPairStep   = 0x40;
    }
    work->stateTimer++;
    switch (work->stateTimer) {
        case 0x15:
            work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 0x25:
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (work->stateTimer < 0xE) {
        coord               = arg0->extra.tmd->coords;
        angle               = ratan2(aim->delta.vx, aim->delta.vz);
        aim->turn           = _actorAngleNormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        coord2     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_401300_ROOT_SCALE);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_401300_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// `_actorRenderRescaleYaw` at 0x1964 on the actor's root coordinate, with
/// the root's `composeStamp` cleared again before the scratch block is released.
static __inline__ void Actor401300_ResetActorYaw(Task* actor)
{
    GfxCoord*             coord;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    coord                                      = actor->extra.tmd->coords;
    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0]                   = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1]                   = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2]                   = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0]                   = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1]                   = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2]                   = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0]                   = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1]                   = (u16)blk->rotation.m[2][1];
    m22                                    = (u16)blk->rotation.m[2][2];
    coord->composeStamp                    = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2]                   = m22;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Facing yaw of `coord`.
static __inline__ s32 Actor401300_Yaw(GfxCoord* coord)
{
    return ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

/// `_actorMovementStepForward` testing the flag byte through a `McSaveData*`.
static __inline__ void Actor401300_MoveForwardSave(McSaveData* save, GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (save->state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static void func_actor_401300_8013DADC(Task* arg0)
{
    _Actor401300Work*          work;
    Task*                      task;
    GameActor*                 player;
    PlayerStatus*              config;
    Enemy*                     enemy;
    TmdObject*                 obj;
    GfxCoord*                  root;
    void**                     scratch;
    _Actor401300ChargeScratch* head;
    _Actor401300ChargeScratch* charge;
    s16                        amount;
    s16                        ret;
    McSaveData*                save;

    work   = arg0->work;
    task   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player = (GameActor*)task->work;
    enemy  = arg0->spawnArg2.pointer;

    if (work->stateEntered != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 0x1B;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->blockedFrames   = 0;
        work->jointPairTarget = 0;
        work->jointPairStep   = 0x40;
        return;
    }
    scratch = SCRATCH_HEAD_ADDR;
    config  = &gPlayerStatus;
    work->stateTimer++;
    root              = arg0->extra.tmd->coords;
    head              = SCRATCH_HEAD_AT(scratch, _Actor401300ChargeScratch);
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    charge            = (SCRATCH_HEAD_AT(scratch, _Actor401300ChargeScratch) = head - 1);
    charge->delta.vy  = config->coordMtx->t[1] - root->coord.t[1];
    charge->delta.vz  = config->coordMtx->t[2] - root->coord.t[2];
    save              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    func_actor_401300_80133A3C(arg0);
    switch (work->animId) {
        case 0x1B:
            if ((charge->gridPushed = func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57)) != 0 &&
                work->stateTimer >= 0x15) {
                work->blockedFrames++;
            } else {
                if (charge->gridPushed != 1) {
                    func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
                }
                work->blockedFrames = 0;
            }
            if (work->blockedFrames >= 7) {
                work->animId      = 0x1E;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            work->lookYawTarget = 0;
            charge->turn        = actorYawTo(arg0->extra.tmd->coords, charge->delta.vx, charge->delta.vz);
            if (work->stateTimer >= 0xB) {
                if (abs(charge->turn) < 0x200) {
                    if (!_actorRangeOutsideRadiusXZ(&charge->delta, 0x7D0)) {
                        work->animId      = 0x1C;
                        work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                        work->stateTimer  = 0;
                    }
                }
            }
            if (abs(charge->turn) > 0x400) {
                work->animId      = 0x1C;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_BLEND;
                work->stateTimer  = 0;
            }
            if (charge->turn > 6) {
                charge->turn = 6;
            } else if (charge->turn < -6) {
                charge->turn = -6;
            }
            charge->turn += Actor401300_Yaw(arg0->extra.tmd->coords);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, charge->turn, 1);
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x70) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 0x70);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1C:
            if ((charge->gridPushed = func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57)) != 0 &&
                work->stateTimer >= 0x15) {
                work->blockedFrames++;
            } else {
                if (charge->gridPushed != 1) {
                    func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
                }
                work->blockedFrames = 0;
            }
            if (work->blockedFrames >= 7) {
                work->animId      = 0x1E;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            charge->turn = actorYawTo(arg0->extra.tmd->coords, charge->delta.vx, charge->delta.vz);
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->animId      = 0x1D;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            if (func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 0 && player->mode != GAME_ACTOR_MODE_SCRIPTED && abs(charge->turn) < 0x100 &&
                enemy->hp > 0) {
                work->playerButtonHold.pressCount = 0x7F;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                    padScriptSpawnVariableMotorRamp(0x10, 8, 0xFF);
                    work->playerHeld                   = 1;
                    work->playerAnim.source.sets       = D_actor_401300_801588F0;
                    work->playerMove.displacement.vz   = 0;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vx   = 0;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                    charge->delta.vx                   = -charge->delta.vx;
                    charge->delta.vy                   = -charge->delta.vy;
                    charge->delta.vz                   = -charge->delta.vz;
                    charge->turn                       = actorYawTo(task->extra.tmd->coords, charge->delta.vx, charge->delta.vz);
                    if (abs(charge->turn) < 0x400) {
                        amount                       = -0x64;
                        work->playerAnim.animationId = 4;
                        work->playerPlacement.rot.vy = charge->turn + Actor401300_Yaw(task->extra.tmd->coords);
                        ret                          = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 4), 0);
                    } else {
                        amount                       = 0x64;
                        work->playerAnim.animationId = 5;
                        work->playerPlacement.rot.vy = charge->turn + Actor401300_Yaw(task->extra.tmd->coords) + 0x800;
                        ret                          = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 5), 0);
                    }
                    if (ret == 1) {
                        player->state = 0xA;
                    }
                    work->playerPlacement.pos.vx = task->extra.tmd->coords->coord.t[0];
                    work->playerPlacement.pos.vy = task->extra.tmd->coords->coord.t[1];
                    work->playerPlacement.pos.vz = task->extra.tmd->coords->coord.t[2];
                    work->playerPlacement.rot.vx = 0;
                    work->playerPlacement.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(task, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                    work->playerAnimFrames = 0;
                    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &charge->delta);
                    charge->delta.vy = 0;
                    VectorNormalSS(&charge->delta, &charge->delta);
                    gte_lddp(amount);
                    gte_ldsv(&charge->delta);
                    gte_gpf12();
                    gte_stsv(&charge->delta);
                    work->playerMove.displacement.vx   = charge->delta.vx;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vz   = charge->delta.vz;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                    work->animId                       = 0x1E;
                    work->animRequest                  = ACTOR_401300_ANIM_REQUEST_RESET;
                    work->stateTimer                   = 0;
                }
            }
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0xA8) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 0xA8);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1E:
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
            if (work->stateTimer < 8) {
                if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x79) != 0) {
                    Actor401300_MoveForwardSave(save, arg0->extra.tmd->coords, -0x79);
                }
            } else if ((u16)(work->stateTimer - 8) < 6) {
                if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, -0x19) != 0) {
                    Actor401300_MoveForwardSave(save, arg0->extra.tmd->coords, -0x19);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        case 0x1D:
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        default:
            work->state = ACTOR_401300_STATE_PATROL;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300ChargeScratch);
}

/// `actorMoveForwardNonzero` testing `actorsFrozen` through a `McSaveData*`.
static __inline__ void Actor401300_MoveForwardNonzeroSave(McSaveData* save, GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (save->state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gteVec                        = vec;
        if (amount != 0) {
            gfxReadMatrixZAxis(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0]  += head[-1].vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static void func_actor_401300_8013E930(Task* arg0)
{
    _Actor401300Work*        work;
    Task*                    task;
    GameActor*               player;
    PlayerStatus*            config;
    Enemy*                   enemy;
    TmdObject*               obj;
    GfxCoord*                root;
    void**                   scratch;
    _Actor401300LeapScratch* head;
    _Actor401300LeapScratch* leap;
    SVECTOR*                 delta;
    SVECTOR*                 vec;
    s16                      cur;
    s16                      amount;
    s16                      ret;
    McSaveData*              save;

    work   = arg0->work;
    task   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player = (GameActor*)task->work;
    config = &gPlayerStatus;
    save   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    enemy  = arg0->spawnArg2.pointer;

    if (work->stateEntered != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 0x1F;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        func_actor_401300_80133A3C(arg0);
        work->jointPairTarget = 0x200;
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->jointPairStep   = 0x100;
        work->leapStartPos.vx = arg0->extra.tmd->coords->coord.t[0];
        work->leapStartPos.vy = arg0->extra.tmd->coords->coord.t[1];
        work->leapStartPos.vz = arg0->extra.tmd->coords->coord.t[2];
        return;
    }
    scratch = SCRATCH_HEAD_ADDR;
    work->stateTimer++;
    root              = arg0->extra.tmd->coords;
    head              = SCRATCH_HEAD_AT(scratch, _Actor401300LeapScratch);
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    delta             = &head[-1].delta;
    delta->vy         = config->coordMtx->t[1] - root->coord.t[1];
    leap              = (SCRATCH_HEAD_AT(scratch, _Actor401300LeapScratch) = head - 1);
    delta->vz         = config->coordMtx->t[2] - root->coord.t[2];
    func_actor_401300_80133A3C(arg0);
    switch (work->animId) {
        case 0x1F:
            work->hitBody.radius = 0x280;
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
            work->lookYawTarget = 0;
            leap->turn          = actorYawTo(arg0->extra.tmd->coords, head[-1].delta.vx, delta->vz);
            if (work->stateTimer >= 0xB) {
                work->animId       = 0x20;
                work->animRequest  = ACTOR_401300_ANIM_REQUEST_BLEND;
                leap->offset.vx    = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
                leap->offset.vy    = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
                leap->offset.vz    = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
                leap->leapDistance = SquareRoot0(leap->offset.vx * leap->offset.vx + leap->offset.vy * leap->offset.vy + leap->offset.vz * leap->offset.vz) + 1000;
                if (leap->leapDistance > 5000) {
                    leap->leapDistance = 5000;
                } else if (leap->leapDistance < 3000) {
                    leap->leapDistance = 3000;
                }
                work->leapStep        = leap->leapDistance / 18;
                work->jointPairTarget = 0;
                work->stateTimer      = 0;
                work->jointPairStep   = 0x20;
            }
            if (abs(leap->turn) > 0x400) {
                work->state = ACTOR_401300_STATE_CHASE;
            }
            if (leap->turn > 0x10) {
                leap->turn = 0x10;
            } else if (leap->turn < -0x10) {
                leap->turn = -0x10;
            }
            leap->turn += Actor401300_Yaw(arg0->extra.tmd->coords);
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, leap->turn, 1);
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x20:
            work->hitBody.radius = 0x140;
            func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->animId      = 0x21;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            if (func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 0 && player->mode != GAME_ACTOR_MODE_SCRIPTED && work->stateTimer >= 8 &&
                enemy->hp > 0) {
                work->playerButtonHold.pressCount = 0x7F;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                    padScriptSpawnVariableMotorRamp(0x10, 8, 0xFF);
                    sndEvtRequestScriptStart(SOUND_PLAYER_STRUCK, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                    work->playerHeld             = 1;
                    work->playerAnim.source.sets = D_actor_401300_801588F0;
                    leap->delta.vx               = work->leapStartPos.vx - task->extra.tmd->coords->coord.t[0];
                    leap->delta.vy               = work->leapStartPos.vy - task->extra.tmd->coords->coord.t[1];
                    leap->delta.vz               = work->leapStartPos.vz - task->extra.tmd->coords->coord.t[2];
                    leap->turn                   = actorYawTo(task->extra.tmd->coords, head[-1].delta.vx, delta->vz);
                    if (abs(leap->turn) < 0x400) {
                        amount                       = -0x46;
                        work->playerAnim.animationId = 4;
                        work->playerPlacement.rot.vy = leap->turn + Actor401300_Yaw(task->extra.tmd->coords);
                        ret                          = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 2), 0);
                    } else {
                        amount                       = 0x46;
                        work->playerAnim.animationId = 5;
                        work->playerPlacement.rot.vy = leap->turn + Actor401300_Yaw(task->extra.tmd->coords) + 0x800;
                        ret                          = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 3), 0);
                    }
                    if (ret == 1) {
                        player->state = 0xA;
                    }
                    work->playerPlacement.pos.vx = task->extra.tmd->coords->coord.t[0];
                    work->playerPlacement.pos.vy = task->extra.tmd->coords->coord.t[1];
                    work->playerPlacement.pos.vz = task->extra.tmd->coords->coord.t[2];
                    work->playerPlacement.rot.vx = 0;
                    work->playerPlacement.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(task, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                    work->playerAnimFrames = 0;
                    vec                    = &leap->delta;
                    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, vec);
                    leap->delta.vy = 0;
                    VectorNormalSS(vec, vec);
                    gte_lddp(amount);
                    gte_ldsv(vec);
                    gte_gpf12();
                    gte_stsv(vec);
                    work->playerMove.displacement.vx   = leap->delta.vx;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vz   = leap->delta.vz;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                }
            }
            if (work->playerHeld == 0) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, work->leapStep);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->hitBody.radius = 0x280;
            if (func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57) == 0) {
                cur = work->stateTimer;
                if (cur < 0x11 && work->playerHeld == 0) {
                    if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, (s16)(0x54 - cur * 0x54 / 16)) != 0) {
                        Actor401300_MoveForwardNonzeroSave(save, arg0->extra.tmd->coords, 0x54 - work->stateTimer * 0x54 / 16);
                    }
                }
            }
            func_actor_401300_80132910(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            Actor401300_ResetActorYaw(arg0);
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        default:
            work->state = ACTOR_401300_STATE_PATROL;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor401300LeapScratch);
}

/// Scale `m` uniformly by `scale` (4.12), translation included.
static __inline__ void Actor401300_ScaleMatrix(MATRIX* m, s16 scale)
{
    ActorScaleMatrixScratch* head;
    ActorScaleMatrixScratch* blk;

    head                                          = SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch);
    blk                                           = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch) = blk;
    blk->scale.vz                                 = scale;
    blk->scale.vy                                 = scale;
    head[-1].scale.vx                             = scale;
    ScaleMatrix(m, &blk->scale);
    blk->translation.vx = m->t[0];
    blk->translation.vy = m->t[1];
    blk->translation.vz = m->t[2];
    gte_lddp(scale);
    gte_ldsv(&blk->translation);
    gte_gpf12();
    gte_stsv(&blk->translation);
    m->t[0] = blk->translation.vx;
    m->t[1] = blk->translation.vy;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleMatrixScratch);
    m->t[2] = blk->translation.vz;
}

static void func_actor_401300_8013F628(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    PlayerStatus*     config;
    GfxCoord*         root;
    SVECTOR**         scratch;
    SVECTOR*          head;
    SVECTOR*          vec;
    s16               mod;
    s16               cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj = arg0->extra.tmd;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x280;
        work->animRequest       = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate          = 0x10;
        work->animId            = 0x20;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        func_actor_401300_80133A3C(arg0);
        work->jointPairTarget = 0x200;
        work->field_D1C       = 0;
        work->stateTimer      = 0;
        work->jointPairStep   = 0x100;
        work->leapStartPos.vx = arg0->extra.tmd->coords->coord.t[0];
        work->leapStartPos.vy = arg0->extra.tmd->coords->coord.t[1];
        work->leapStartPos.vz = arg0->extra.tmd->coords->coord.t[2];
        Actor401300_ScaleMatrix(&work->colorMtx, 0);
        return;
    }
    scratch = (SVECTOR**)SCRATCH_HEAD_ADDR;
    config  = &gPlayerStatus;
    work->stateTimer++;
    root                              = arg0->extra.tmd->coords;
    head                              = SCRATCH_HEAD_AT(scratch, SVECTOR);
    head[-1].vx                       = config->coordMtx->t[0] - root->coord.t[0];
    vec                               = head - 1;
    vec->vy                           = config->coordMtx->t[1] - root->coord.t[1];
    vec->vz                           = config->coordMtx->t[2] - root->coord.t[2];
    SCRATCH_HEAD_AT(scratch, SVECTOR) = head - 3;
    if (work->stateTimer < 0x12) {
        Actor401300_ScaleMatrix(&work->colorMtx, (work->stateTimer << 12) / 30);
        if (gGameSession->location.loc.area == 0xB) {
            if ((work->stateTimer & 7) == 0) {
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
            } else {
                mod = work->stateTimer % 8;
                if (mod == 2) {
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 2, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
                }
            }
        } else if (gGameSession->location.loc.area == 0x1D) {
            if ((work->stateTimer & 7) == 0) {
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
            } else {
                mod = work->stateTimer % 8;
                if (mod == 2) {
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 2, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 3, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 1, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 17, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 16, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 5, 0, NULL);
                    effectSpawn(EFFECT_NEO_ARK_WOODLAND_FALLING_LEAF, arg0->extra.tmd->coords + 18, 0, NULL);
                }
            }
        }
    }
    func_actor_401300_80133A3C(arg0);
    switch (work->animId) {
        case 0x20:
            work->hitBody.radius = 0x500;
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->animId      = 0x21;
                work->animRequest = ACTOR_401300_ANIM_REQUEST_RESET;
                work->stateTimer  = 0;
            }
            if (work->playerHeld == 0 && (s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x78) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 0x78);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->hitBody.radius = 0x280;
            cur                  = work->stateTimer;
            if (cur < 0x11 && work->playerHeld == 0) {
                if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x15E, 0x54 - cur * 0x54 / 16) != 0) {
                    actorMoveForwardNonzero(arg0->extra.tmd->coords, 0x54 - work->stateTimer * 0x54 / 16);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->state = ACTOR_401300_STATE_ALERT;
            }
            break;
        default:
            work->state = ACTOR_401300_STATE_PATROL;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(3 * sizeof(SVECTOR));
}

static void func_actor_401300_80140300(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = 0xB;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->jointPairTarget = 0x40;
        work->jointPairBlend  = 0xC8;
        work->jointPairStep   = 0x40;
        work->hitBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_401300_80133A3C(arg0);
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ACTOR_401300_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ACTOR_401300_STATE_STATUS_HOLD;
        } else {
            work->state = ACTOR_401300_STATE_DOWN;
        }
    }
}

static void func_actor_401300_8014046C(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = 0x22;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->jointPairTarget         = 0x40;
        work->jointPairBlend          = 0xC8;
        work->jointPairStep           = 0x40;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    func_actor_401300_80133A3C(arg0);
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 0) {
        func_actor_401300_80132C78(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x57);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ACTOR_401300_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ACTOR_401300_STATE_STATUS_HOLD;
        } else {
            work->state = ACTOR_401300_STATE_DOWN;
        }
    }
}

// This is a decompilation attempt by the m2c tool.

/// `Actor401300_InRange` with the flag kept apart from the comparison. The
/// dead `ret = cmp` in each arm stops jump from turning the if/else into a
/// store-flag, which would fold `cmp` and `ret` into one register.
static __inline__ s32 Actor401300_InRangeFlag(Task* arg0)
{
    SVECTOR   out;
    SVECTOR   sv;
    VECTOR    vec;
    s32       flag;
    SVECTOR*  svp;
    GfxCoord* view;
    VECTOR*   vecp;
    s32*      flagp;
    SVECTOR*  outp;
    GfxCoord* p;
    s32       ret;
    s32       cmp;

    memset(&out, 0, 8);
    p     = &arg0->extra.tmd->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
    for (;;) {
        if (p->parent != NULL) {
            if (p != view) {
                gte_SetTransMatrix(&p->coord);
                gte_SetRotMatrix(&p->coord);
                gte_ldv0(svp);
                gte_rtv0tr();
                gte_stlvnl(vecp);
                gte_stflg(flagp);
                sv.vx = vec.vx;
                sv.vy = vec.vy;
                sv.vz = vec.vz;
                p     = p->parent;
                continue;
            }
            outp->vx = sv.vx;
            outp->vy = sv.vy;
            outp->vz = sv.vz;
        }
        break;
    }
    cmp = (u16)(out.vz + 0x12B) < 0xA27;
    if (cmp == 0) {
        ret = cmp;
        ret = 0;
    } else {
        ret = cmp;
        ret = 1;
    }
    return ret;
}

/// 1 when the first of `recs` is a kind 0x10000 record.
static __inline__ s32 Actor401300_HasRec10000(WorldCollisionContact* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].key.value)
            break;
        if ((recs[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Snaps the player's height to the actor's when the pending push enables
/// every collision pass and the player has drifted 0x321 or more away.
static __inline__ void Actor401300_SnapPlayerHeight(Task* actor)
{
    _Actor401300Work* work;
    Task*             slot;
    GfxCoord*         playerCoord;
    GfxCoord*         actorCoord;

    work = actor->work;
    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((slot != NULL) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        playerCoord = slot->extra.tmd->coords;
        actorCoord  = actor->extra.tmd->coords;
        if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x321) {
            playerCoord->coord.t[1]               = actorCoord->coord.t[1];
            slot->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static const _Actor401300StateTable D_actor_401300_80131F34 = { {
    func_actor_401300_8014192C,
    func_actor_401300_801419B8,
    func_actor_401300_80141A60,
    func_actor_401300_80141B0C,
    func_actor_401300_80135DDC,
    func_actor_401300_80141BC8,
    func_actor_401300_80136238,
    func_actor_401300_801365F8,
    func_actor_401300_80136CE8,
    func_actor_401300_801376E4,
    func_actor_401300_80137D78,
    func_actor_401300_80138160,
    func_actor_401300_80138800,
    func_actor_401300_80138B24,
    func_actor_401300_80141C80,
    func_actor_401300_80141C88,
    func_actor_401300_80141D50,
    func_actor_401300_80141DF4,
    NULL,
    func_actor_401300_80138CF8,
    func_actor_401300_80138FCC,
    func_actor_401300_80139134,
    func_actor_401300_80139520,
    func_actor_401300_801397F8,
    func_actor_401300_80139AB0,
    func_actor_401300_8013A5C0,
    func_actor_401300_8013A208,
    func_actor_401300_8013AAE8,
    func_actor_401300_8013AE48,
    func_actor_401300_8013B6E8,
    func_actor_401300_8013CBAC,
    func_actor_401300_8013D2AC,
    func_actor_401300_8013D6C4,
    func_actor_401300_8013DADC,
    func_actor_401300_8013E930,
    func_actor_401300_8013F628,
    func_actor_401300_80141EF8,
    func_actor_401300_80140300,
    func_actor_401300_8014046C,
    func_actor_401300_80135FC4,
    func_actor_401300_8013BB30,
} };

static void func_actor_401300_801405DC(Enemy* enemy, Task* actor)
{
    VECTOR                    pos;
    _Actor401300StateTable    states;
    _Actor401300Work*         work;
    ActorPartPositionScratch* scratch;
    ActorPartPositionScratch* head;
    Task*                     player;
    PlayerStatus*             config;
    s32                       state;
    s32                       action;

    work   = actor->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config = &gPlayerStatus;
    states = D_actor_401300_80131F34;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->state;
            if ((state != ACTOR_401300_STATE_HIDDEN) && (state != ACTOR_401300_STATE_DEAD) && (state != ACTOR_401300_STATE_DEATH_BURN) && (state != ACTOR_401300_STATE_DEATH_BURST) && (state != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ACTOR_401300_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->state;
            if ((state != ACTOR_401300_STATE_HIDDEN) && (state != ACTOR_401300_STATE_DEAD) && (state != ACTOR_401300_STATE_DEATH_BURN) && (state != ACTOR_401300_STATE_DEATH_BURST) && (state != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ACTOR_401300_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
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

    head                                           = SCRATCH_STACK_CURSOR(ActorPartPositionScratch);
    SCRATCH_STACK_CURSOR(ActorPartPositionScratch) = head - 1;
    scratch                                        = head - 1;

    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        func_actor_401300_80134F90(actor);
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    states.handlers[work->state](actor);

    state = work->state;
    if ((state != ACTOR_401300_STATE_DEATH_BURN) && (state != ACTOR_401300_STATE_PLAY_DOWN) && (state != ACTOR_401300_STATE_HIDDEN) && (state != ACTOR_401300_STATE_DEAD) && (state != ACTOR_401300_STATE_DEATH_BURST) && (state != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
        scratch->position.vx = 0;
        scratch->position.vy = 0;
        scratch->position.vz = 0;
        actorTransformToView(actor->extra.tmd->coords + 1, &scratch->position);
        work->hitBody.pos.vx         = scratch->position.vx;
        work->hitBody.pos.vy         = scratch->position.vy;
        work->hitBody.pos.vz         = scratch->position.vz;
        work->gridCoord.coord.t[0]   = actor->extra.tmd->coords->coord.t[0];
        work->gridCoord.coord.t[1]   = actor->extra.tmd->coords->coord.t[1] - 0x15E;
        work->gridCoord.coord.t[2]   = actor->extra.tmd->coords->coord.t[2];
        work->gridCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->gridCoord);
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(actor->extra.tmd->coords);
        state = work->state;
    }
    if ((state == ACTOR_401300_STATE_DEATH_BURN) || (state == ACTOR_401300_STATE_HIDDEN) || (state == ACTOR_401300_STATE_DEAD) || (state == ACTOR_401300_STATE_DEATH_BURST) || (state == ACTOR_401300_STATE_DEATH_BURST_WALK)) {
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
    if ((Actor401300_HasRec10000(work->attackContacts) == 1) || (enemy->hp <= 0)) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->attackContacts);

    if (work->playerHeld == 1) {
        state = work->state;
        if ((state != ACTOR_401300_STATE_DEATH_BURN) && (state != ACTOR_401300_STATE_PLAY_DOWN) && (state != ACTOR_401300_STATE_HIDDEN) && (state != ACTOR_401300_STATE_DEAD) && (state != ACTOR_401300_STATE_DEATH_BURST) && (state != ACTOR_401300_STATE_DEATH_BURST_WALK)) {
            Actor401300_SnapPlayerHeight(actor);
        }
        action = work->playerAnim.animationId;
        work->playerAnimFrames++;
        switch (action) {
            case 0:
            case 1:
            case 2:
            case 3:
                break;
            case 4:
                if (work->playerAnimFrames == 0xF) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        sndEvtRequestScriptStart(SOUND_NEO_ARK_WOODLAND_STRANGER_HIT, (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                                 (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    } else {
                        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 0x13), (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                                 (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    }
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        effectSpawn(EFFECT_DUST_PUFF, &player->extra.tmd->coords[1], 0x80003A00, NULL);
                    }
                }
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) == 1) {
                    work->playerMove.displacement.vx = 0;
                    work->playerMove.displacement.vy = 0;
                    work->playerMove.displacement.vz = 0;
                }
                if (work->stateTimer >= 10) {
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vx >>= 1;
                    work->playerMove.displacement.vz >>= 1;
                }
                break;
            case 5:
                if (work->playerAnimFrames == 0xD) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        sndEvtRequestScriptStart(SOUND_NEO_ARK_WOODLAND_STRANGER_HIT, (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                                 (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    } else {
                        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 0x13), (s8)worldCoordGetOriginAudioPan(player->extra.tmd->coords),
                                                 (s8)worldCoordGetOriginAudioDepth(player->extra.tmd->coords));
                    }
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        effectSpawn(EFFECT_DUST_PUFF, &player->extra.tmd->coords[1], 0x80003A00, NULL);
                    }
                }
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) == 1) {
                    work->playerMove.displacement.vx = 0;
                    work->playerMove.displacement.vy = 0;
                    work->playerMove.displacement.vz = 0;
                }
                if (work->stateTimer >= 10) {
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vx >>= 1;
                    work->playerMove.displacement.vz >>= 1;
                }
                break;
            case 6:
            case 7:
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->playerAnim.animationId) {
                case 0:
                    break;
                case 1:
                    if (config->hp > 0) {
                        work->playerAnim.animationId = 2;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case 2:
                    if (config->hp > 0) {
                        work->playerAnim.animationId = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case 4:
                    if (config->hp > 0) {
                        work->playerAnim.animationId = 6;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case 5:
                    if (config->hp > 0) {
                        work->playerAnim.animationId = 7;
                        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->playerAnimFrames = 0;
                    }
                    break;
                case 3:
                case 6:
                case 7:
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

    scratch->position.vx = 0;
    scratch->position.vy = 0;
    scratch->position.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->position);

    work->bodyPosHistory[work->bodyPosCursor].vx = scratch->position.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = scratch->position.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = scratch->position.vz;

    SCRATCH_STACK_RELEASE_BLOCK(ActorPartPositionScratch);
    work->bodyPosCursor++;
    if (work->bodyPosCursor == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if (work->animId == 0x14 || work->animId == 0x15) {
        enemy->bodyPos.vx = work->bodyPosHistory[work->bodyPosCursor].vx;
        enemy->bodyPos.vy = work->bodyPosHistory[work->bodyPosCursor].vy;
        enemy->bodyPos.vz = work->bodyPosHistory[work->bodyPosCursor].vz;
    } else {
        enemy->bodyPos.vx = scratch->position.vx;
        enemy->bodyPos.vy = scratch->position.vy;
        enemy->bodyPos.vz = scratch->position.vz;
    }
    enemy->coord = &gGfxViewCoord;
}

s32 func_actor_401300_8014148C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The task's handlers, indexed by `Task::state` in
/// `func_actor_401300_80141F2C`: the first allocates and sets up the work
/// block, the second runs the per-state logic every frame, and the third tears
/// the enemy down.
static const EnemyTaskFuncTable3 D_actor_401300_8013201C = { {
    func_actor_401300_80134454,
    func_actor_401300_801405DC,
    enemyDestroy,
} };

s32 func_actor_401300_80141494(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    _Actor401300Work* work = arg0->work;

    switch (arg2->animationId) {
        case 0:
            work->animId = 0x22;
            break;
        case 1:
            work->animId = 0x23;
            break;
        case 2:
            work->animId = 0x24;
            break;
        case 3:
            work->animId = 0x25;
            break;
        case 4:
            work->animId = 0x27;
            break;
    }
    work->state     = ACTOR_401300_STATE_DOWN;
    work->prevState = -1;
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

/// Places the model's root coordinate from `placement`: sets its translation,
/// applies the X, Y and Z rotations in turn, and caches the resulting heading
/// (`ratan2` of the matrix Z axis) in `_Actor401300Work::placedYaw`. Always returns 1.
s32 func_actor_401300_80141614(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*         coord;
    s32               mx;
    s32               mz;
    _Actor401300Work* work;

    work                                = task->work;
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = task->extra.tmd->coords;
    mx                                    = coord->coord.m[2][0];
    mz                                    = coord->coord.m[2][2];
    work->placedYaw                       = ratan2(-mx, mz);
    return 1;
}

#include "../../shared/actor_messages_release_hold.inc.c"

static void func_actor_401300_80141758(Task* task)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
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
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}

static s32 func_actor_401300_801417F0(Task* arg0)
{
    SVECTOR out;

    memset(&out, 0, 8);
    actorTransformToView(&arg0->extra.tmd->coords[1], &out);
    return (u16)(out.vz + 0x12B) < 0xA27;
}

static void func_actor_401300_8014192C(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;
    TmdObject*        obj;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        return;
    }
    if (enemy->hp != -0x3E7 && work->deathPending == 0 && (arg0->spawnArg1.value >> 16) == 2) {
        enemy->hp = -0x3E7;
    }
}

static void func_actor_401300_801419B8(Task* arg0)
{
    TmdObject*        obj;
    _Actor401300Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 2;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_80141A60(Task* arg0)
{
    TmdObject*        obj;
    _Actor401300Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 3;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_80141B0C(Task* arg0)
{
    TmdObject*        obj;
    _Actor401300Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 0xB;
        work->jointPairTarget  = 0x20;
        work->jointPairStep    = 8;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_401300_80133A3C(arg0);
    }
}

static void func_actor_401300_80141BC8(Task* arg0)
{
    TmdObject*        obj;
    _Actor401300Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animRate         = 0x12;
        work->animId           = 0xD;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_401300_80133A3C(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
}

static void func_actor_401300_80141C80(Task* arg0)
{
}

static void func_actor_401300_80141C88(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = 8;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 29, 0, 0)) {
            work->state = ACTOR_401300_STATE_WITHDRAW;
        } else {
            work->state = ACTOR_401300_STATE_CHASE;
        }
    }
}

static void func_actor_401300_80141D50(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x280;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_401300_ANIM_REQUEST_RESET;
        work->animId                  = 0x16;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->chaseRate;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_401300_STATE_CHASE;
    }
}

static void func_actor_401300_80141DF4(Task* arg0)
{
    _Actor401300Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->jointPairTarget = 0x20;
        work->jointPairStep   = 8;
        gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer      = work->downFramesBase + ((gRandomLcgState >> 16) & 0xF);
    }
    func_actor_401300_80133A3C(arg0);
    if (--work->stateTimer < 0) {
        switch (work->animId) {
            case 11:
            case 23:
                work->state = ACTOR_401300_STATE_RISE_BACK;
                break;
            case 12:
            case 24:
            case 34:
                work->state = ACTOR_401300_STATE_RISE_FRONT;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_401300_STATE_DEATH_BURN;
    }
}

static void func_actor_401300_80141EF8(Task* task)
{
    _Actor401300Work* work  = task->work;
    Enemy*            enemy = task->spawnArg2.pointer;

    if (enemy->hp != -0x3E7 && work->deathPending == 0) {
        enemy->hp = -0x3E7;
    }
}

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401300_80141F2C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_401300_8013201C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
