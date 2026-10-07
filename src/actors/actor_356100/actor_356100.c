#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
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
#include "main/random.h"
#include "main/gfx.h"
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
#include "../../shared/actor_messages.h"

/// Uniform model-root scale for yaw rebuilds, with 12 fractional bits.
enum { ACTOR_356100_ROOT_SCALE = 0x1194 };

/// Freeze value that suppresses coordinate steps and guarded contact correction.
enum { ACTOR_MOVEMENT_FROZEN = 1 };

/// Low fractional bits of the contact resolver's signed 16.16 correction.
enum { ACTOR_CONTACT_ROOT_FRACTION_MASK = 0xFFFF };

/// Animation-set indices and driver limits for this package.
enum {
    ACTOR_356100_ANIM_INITIAL    = 1,
    ACTOR_356100_ANIM_ALERT      = 9,
    ACTOR_356100_ANIM_DOWN_BACK  = 11,
    ACTOR_356100_ANIM_DOWN_FRONT = 12,
    ACTOR_356100_BLEND_SLOT_END  = 11,
    ACTOR_356100_BLEND_RATE_BIAS = 3,
    ACTOR_356100_LOOK_YAW_STEP   = 0x100,
    ACTOR_356100_ALERT_TURN_STEP = 0x10,
    ACTOR_356100_LOOK_YAW_LIMIT  = ACTOR_TRANSFORM_ANGLE_TURN / 4
};

/// Animation sets selected by the reviewed movement and grab states.
enum {
    ACTOR_356100_ANIM_WALK                  = 2,
    ACTOR_356100_ANIM_RUN                   = 3,
    ACTOR_356100_ANIM_GRAB_PULL             = 5,
    ACTOR_356100_ANIM_GRAB_RELEASE          = 7,
    ACTOR_356100_ANIM_DORMANT               = 14,
    ACTOR_356100_ANIM_DORMANT_FIDGET        = 15,
    ACTOR_356100_ANIM_BACK_OFF              = 17,
    ACTOR_356100_ANIM_GRAB_WINDUP           = 19,
    ACTOR_356100_ANIM_SIDESTEP_NEGATIVE_YAW = 20,
    ACTOR_356100_ANIM_SIDESTEP_POSITIVE_YAW = 21
};

/// Parent-coordinate Y offset added by the movement states' contact correction.
enum { ACTOR_356100_CONTACT_HEIGHT_OFFSET = 16 };

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Values of `_Actor356100Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// No state selects `FALL_BACK` or `FALL_FRONT`, and `STATUS_HOLD`, `DOWN`,
/// the two rises and `DEATH_BURN` are selected only on the way out of those.
enum {
    ACTOR_356100_STATE_HIDDEN           = 0x00, // not drawn and not lockable
    ACTOR_356100_STATE_PLAY_WALK        = 0x01, // loops the walk animation in place; never selected
    ACTOR_356100_STATE_PLAY_RUN         = 0x02, // loops the run animation in place; never selected
    ACTOR_356100_STATE_PLAY_DOWN        = 0x03, // holds the lying animation; never selected
    ACTOR_356100_STATE_STATUS_HOLD      = 0x04, // twitches in place until the status buildup runs out
    ACTOR_356100_STATE_PLAY_DOWN_2      = 0x05, // the same as `PLAY_DOWN`; never selected
    ACTOR_356100_STATE_ALERT            = 0x06, // turns to the player and raises the combat alert
    ACTOR_356100_STATE_CHASE            = 0x07, // runs at the player; grabs when close, sidesteps a distant player who faces it
    ACTOR_356100_STATE_CIRCLE           = 0x08, // runs round the player, speeding up and then easing off
    ACTOR_356100_STATE_TURN_AROUND      = 0x09, // swings `turnYaw` round to `turnYawTarget`
    ACTOR_356100_STATE_SIDESTEP         = 0x0A, // hops along `sidestepDir`, then chases
    ACTOR_356100_STATE_GRAB             = 0x0B, // reaches for the player; a player in reach is held
    ACTOR_356100_STATE_GRAB_PULL        = 0x0C, // places the held player and itself for the strike
    ACTOR_356100_STATE_GRAB_STRIKE      = 0x0D, // damages the held player
    ACTOR_356100_STATE_GRAB_RELEASE     = 0x0E, // lets the player go and steps back
    ACTOR_356100_STATE_RISE_BACK        = 0x0F, // gets up after `FALL_BACK`
    ACTOR_356100_STATE_RISE_FRONT       = 0x10, // gets up after `FALL_FRONT`
    ACTOR_356100_STATE_DOWN             = 0x11, // lies where it fell until `stateTimer` runs out
    ACTOR_356100_STATE_APPROACH         = 0x12, // runs at the player on a half-rate animation and grabs when close; never selected
    ACTOR_356100_STATE_FALL_BACK        = 0x13, // falls backward
    ACTOR_356100_STATE_FALL_FRONT       = 0x14, // falls forward
    ACTOR_356100_STATE_DEATH_BURN       = 0x15, // burns away: the corpse-burn effect, then the model flattens and fades
    ACTOR_356100_STATE_DORMANT          = 0x16, // idles until the player comes near
    ACTOR_356100_STATE_DORMANT_SCRIPTED = 0x17, // the same wait on an animation of its own; never selected
    ACTOR_356100_STATE_PATROL           = 0x18, // walks between the two `patrolPoints` until the combat alert is raised
    ACTOR_356100_STATE_BACK_OFF         = 0x19, // faces the player, backs away and turns aside; never selected
    ACTOR_356100_STATE_SLIDE            = 0x1A, // coasts forward by the shrinking `runStep`, then turns around
    ACTOR_356100_STATE_GRAB_WINDUP      = 0x1B, // turns to the player for eleven frames, then grabs; never selected
    ACTOR_356100_STATE_HEAD_TURN        = 0x1C, // holds still while `lookYawTarget` comes round to the player, then grabs; never selected
    ACTOR_356100_STATE_UNUSED_1D        = 0x1D, // has no handler and is never selected
    ACTOR_356100_STATE_SCRIPTED_DEATH   = 0x1E  // the death scene a room command starts, played out from the parent's origin
};

/// Values of `_Actor356100Work::animRequest` and `_Actor356100Work::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
/// Nothing requests anything of the blend rig.
enum {
    ACTOR_356100_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames its transition table gives
    ACTOR_356100_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ACTOR_356100_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Work block of this package's Horned Stranger task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the state machine, both animation rigs and their driver's state, the
/// contact tables, storage for the model's matrices and the values the
/// pursuit and grab states share. The package sets up no collision body, so
/// nothing fills the contact tables.
///
/// Angles are 4096ths of a turn and positions are in the root coordinate's
/// parent space unless a field says otherwise. Animation ids index the
/// package's animation bank; rates are sixteenths of a frame per tick.
typedef struct {
    s16                   state;             // `ACTOR_356100_STATE_*`
    s16                   prevState;         // `state` the tick last ran; -1 makes the next tick enter `state` afresh
    s16                   stateEntered;      // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16                   stateTimer;        // frame counter of the state: most count up from 0, `DOWN` counts down
    s16                   stateCounter;      // second value of `CHASE` and `CIRCLE`, cleared as either begins: 1 once `CHASE` has switched to its turn animation; in `CIRCLE` the ticks on which `pushContacts` moved the root, seven of which end it
    byte                  field_A[0x2];      // never accessed
    ActorPatrolPoint      patrolPoints[2];   // the spawn position and a point 2000 units ahead along the spawn facing
    s16                   patrolTarget;      // index into `patrolPoints` of the end being walked toward
    s16                   placedYaw;         // heading of the root after the last placement message; never read
    byte                  field_18[0x4];     // never accessed
    ActorAnimRig21        rig;               // playback of the model's parts; slot 1's status and frame time the states
    ActorAnimRig21        blend;             // second playback of the same model, mixed into slots 1 to 10 while `blendActive`
    s32                   grabAnimFrame;     // slot 1's frame on each tick of the grab's strike; never read
    s16                   animRequest;       // `ACTOR_356100_ANIM_REQUEST_*` for `rig`
    s16                   blendActive;       // nonzero while `blend`'s animation is mixed in; cleared when its slot 1 reaches a boundary. Nothing raises it
    s16                   appliedAnim;       // animation `rig` was last started on
    s16                   animId;            // animation requested of `rig`
    u16                   animFrames;        // ticks since `animRequest` was last applied; never read
    s16                   animRate;          // playback rate of `rig`'s slots; negative plays backward
    s16                   baseRate;          // `animRate` the dormant and rising states start on; 0x10 from setup
    s16                   blendRequest;      // `ACTOR_356100_ANIM_REQUEST_*` for `blend`; only `RESET` is acted on
    s16                   blendAnimId;       // animation requested of `blend`
    s16                   blendRate;         // playback rate of `blend`'s slots, 0x30 from each restart
    s16                   blendWeight;       // share of `blend`'s pose in the mix, of 0x1000; 0x800 from each restart
    s16                   lookYawTarget;     // bearing to what the state faces, relative to the facing
    s16                   lookYaw;           // eased toward `lookYawTarget` by 0x100 a tick; within +-0x400, parts 5 and 2 turn by 2/3 and 1/2 of it
    byte                  field_992[0x2];    // never accessed
    s32                   lastCueFrame;      // slot 1's frame on the last `DORMANT_SCRIPTED` tick, so its frame-4 effect fires once; 0 from each applied request
    byte                  field_998[0x24];   // never accessed
    s16                   hitRadius;         // 0x180 from most states' setup, 0xC0 from `CIRCLE` and `SIDESTEP`; never read. Sits where a hit sphere's radius would, directly before `hitContacts`, but no body is set up here, so the role is unproven
    byte                  field_9BE[0x2];    // never accessed
    WorldCollisionContact hitContacts[1];    // first record of the table published as the enemy's contact records; how far the table runs into the bytes after it is unproven
    byte                  field_9D8[0x80];   // never accessed by name
    WorldCollisionContact pushContacts[3];   // contacts the moving states average into a push on the root
    byte                  field_AA0[0x38];   // never accessed
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                savedColorMtx;     // `colorMtx` as `DORMANT` found it; never read
    byte                  field_B38[0x2];    // never accessed
    s16                   field_B3A;         // tested as a fall ends: positive leads to `STATUS_HOLD`, otherwise `DOWN`; nothing writes it, role unproven
    byte                  field_B3C[0x4];    // never accessed
    SVECTOR               sidestepDir;       // unit direction of the sidestep, to one side of the bearing to the player
    s16                   turnYaw;           // heading the turn-around holds the root at, moved 0x89 a tick
    s16                   turnYawTarget;     // heading the turn-around ends on: the facing plus twice the bearing to the player
    s16                   runStep;           // forward step per tick: `CIRCLE` sets it to 8 times `animRate`, halved while `blendActive` and 2 once it has been pushed; `SLIDE` takes 10 off it each tick
    s16                   circleRateStep;    // change of `animRate` per tick of `CIRCLE`: 8 until the rate reaches 0x18, -1 back down to 0x12, then 0 for its last five ticks
    s16                   sidestepSide;      // side the next sidestep takes (1 or -1), flipped by each; 0 draws one at random
    u16                   sidestepStep;      // length of the sidestep's step, 0xDE; halved while `blendActive`
    u16                   downFramesBase;    // ticks `DOWN` lasts, before a random 0..15 more; first value of the variant record, 0 in all three
    u16                   sidestepAngle;     // angle between the bearing to the player and `sidestepDir`; second value of the variant record
    u8                    commandBytes[3];   // first three bytes of the last actor command received; never read
    byte                  field_B5B[0x1];    // never accessed
    Task*                 childTask0;        // killed by the exit callback when set; nothing sets it
    Task*                 childTask1;        // killed by the exit callback when set; nothing sets it
    s16                   circleCount;       // times `CIRCLE` has begun since `CHASE` last did; from the second, a turn-around ending near the player grabs instead of circling again
    s16                   sidestepCount;     // sidesteps since the last grab; the first is thrown 0x171 wider, and `CHASE` waits 3 ticks plus half this count before the next
    s16                   playerHeld;        // 1 while the player is in a hold this enemy started
    byte                  field_B6A[0x2];    // never accessed
    SVECTOR               bodyPosHistory[7]; // ring of part 2's view-space position on the last seven ticks; a sidestep aims the enemy's target point at the oldest
    byte                  field_BA4[0x18];   // never accessed
    s16                   bodyPosCursor;     // index of the next entry of `bodyPosHistory` to write
    byte                  field_BBE[0x2];    // never accessed
} _Actor356100Work;
STATIC_ASSERT_SIZEOF(_Actor356100Work, 0xBC0);

/// Static storage for the placement the grab sends the player.
///
/// `placement` is the payload of `GAME_ACTOR_MESSAGE_PLACE`, lent to the player
/// for the length of the dispatch, which consumes it. The grab fills it in as
/// it pulls the player in: the player keeps their position and takes the
/// bearing to the enemy as their yaw, and the enemy stands 1000 units away
/// along that bearing.
///
/// Eight zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    ActorTransform placement;     // Player's own position, with the yaw of the bearing from the player to the enemy and no pitch or roll
    u8             unknown_18[8]; // Zero in the image; no access established and role unproven
} _Actor356100TransformStorage;
STATIC_ASSERT_SIZEOF(_Actor356100TransformStorage, 32);

static TmdSource _gActor356100HornedStrangerBody;
static s32       _actor356100IgnoreAnimationMessage(Task* task, s32 messageId, s32 unusedPayload, s32 unusedResponse);
static s32       _actor356100ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused);
void             func_actor_356100_8016A910(Task*);

#include "../../shared/actor_contacts.h"

DamageAttack D_actor_356100_8016A96C[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams D_actor_356100_8016A984 = { D_actor_356100_8016A96C, 420, 115, 200, 5, 100, 10, 100, 10 };

ActorHornedStrangerVariant D_actor_356100_8016A994[3] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

static TmdBone _gActor356100HornedStrangerBodySkeleton[21] = {
#include "assets/horned_stranger_body_skeleton.inc"
};

static u32 _gActor356100HornedStrangerBodyPartVerts[21] = {
#include "assets/horned_stranger_body_partVerts.inc"
};

static SVECTOR _gActor356100HornedStrangerBodyVerts[293] = {
#include "assets/horned_stranger_body_verts.inc"
};

static SVECTOR _gActor356100HornedStrangerBodyNormals[363] = {
#include "assets/horned_stranger_body_normals.inc"
};

static u32 _gActor356100HornedStrangerBodyStream[3776] = {
#include "assets/horned_stranger_body_stream.inc"
};

static TmdSource _gActor356100HornedStrangerBody = {
    0,
    18488,
    7600,
    21,
    _gActor356100HornedStrangerBodyPartVerts,
    _gActor356100HornedStrangerBodyVerts,
    _gActor356100HornedStrangerBodyNormals,
    _gActor356100HornedStrangerBodySkeleton,
    _gActor356100HornedStrangerBodyStream,
};

static AnimationPackedPose _gActor356100Animation0FE9CBank1[75] = {
#include "assets/actor_356100_animation_0FE9C_bank1.inc"
};

static AnimationPackedRotation _gActor356100Animation0FE9CBank4[657] = {
#include "assets/actor_356100_animation_0FE9C_bank4.inc"
};

static AnimationRecord _gActor356100Animation0FE9CRecords[1164] = {
#include "assets/actor_356100_animation_0FE9C_records.inc"
};

static u16 _gActor356100Animation0FE9CIndices[22] = {
#include "assets/actor_356100_animation_0FE9C_indices.inc"
};

static AnimationSet _gActor356100Animation0FE9C = {
    _gActor356100Animation0FE9CRecords,
    _gActor356100Animation0FE9CIndices,
    { NULL, _gActor356100Animation0FE9CBank1, NULL, NULL, _gActor356100Animation0FE9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor356100Animation10A84Bank1[28] = {
#include "assets/actor_356100_animation_10A84_bank1.inc"
};

static AnimationPackedRotation _gActor356100Animation10A84Bank4[263] = {
#include "assets/actor_356100_animation_10A84_bank4.inc"
};

static AnimationRecord _gActor356100Animation10A84Records[394] = {
#include "assets/actor_356100_animation_10A84_records.inc"
};

static u16 _gActor356100Animation10A84Indices[22] = {
#include "assets/actor_356100_animation_10A84_indices.inc"
};

static AnimationSet _gActor356100Animation10A84 = {
    _gActor356100Animation10A84Records,
    _gActor356100Animation10A84Indices,
    { NULL, _gActor356100Animation10A84Bank1, NULL, NULL, _gActor356100Animation10A84Bank4, NULL, NULL, NULL },
};

s8 D_actor_356100_801728CC[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 8, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

u8 D_actor_356100_801730B8[184] = {
    0,
    0,
    0,
    0,
    188,
    28,
    23,
    128,
    164,
    40,
    23,
    128,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// A second animation-set table, which no animation context is bound to.
///
/// Every slot is empty. Entering `ACTOR_356100_STATE_DORMANT_SCRIPTED` clears
/// slot 16 and nothing else accesses the table, so the write does not reach
/// playback. The other Stranger packages, at the same point of the same state,
/// install that state's animation set in slot 16 of the table their rigs are
/// bound to.
///
/// The extent is not established by an access: it is the 46 words between
/// `D_actor_356100_801730B8` and the next object, the length of the bound
/// table here and in every other Stranger package.
AnimationSet* D_actor_356100_80173170[46] = { NULL };

AnimationSet* D_actor_356100_80173228[7] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationPlayRequest D_actor_356100_80173244 = { { .sets = D_actor_356100_80173228 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

TaskMessageEntry D_actor_356100_80173258[7] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor356100IgnoreAnimationMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor356100ApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u16 D_actor_356100_80173290 = 0;

TaskDesc D_actor_356100_80173294 = { { { TASK_BODY_TMD, 96 } }, func_actor_356100_8016A910, { .model = &_gActor356100HornedStrangerBody } };

static SVECTOR ActorContact_ScratchPosition = { 0 };

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

EffectSpawnArg D_actor_356100_801732A8 = { NULL, 0, 0 };

_Actor356100TransformStorage D_actor_356100_801732B0;

GameActorButtonPressHold D_actor_356100_801732D0;

static void _actor356100TickBlendedAnimation(Task* actor);

static void _actor356100UpdateAnimation(Task* actor);

/// Per-clip transition values indexed by the current and requested clip.
extern s8 D_actor_356100_801728CC[][45];

/// Scratch-stack block of the per-frame tick.
///
/// The tick reserves it on the frames it runs the state handler and releases
/// it once the body position is in the work block's history. Nothing clears
/// the block, and the tick reads `viewPos` once more after the release.
typedef struct {
    GfxCoord shadowCoord;      // Unrotated coordinate under the view coordinate, at part 1's X and Z and height zero, where the scripted death draws its ground shadow; filled in on those ticks alone
    byte     unknown_50[0x10]; // Never accessed
    SVECTOR  viewPos;          // View-space position of a part's origin: part 1's while the shadow is placed, then part 2's, which is recorded as the body position; `pad` is never written
} _Actor356100TickScratch;
STATIC_ASSERT_SIZEOF(_Actor356100TickScratch, 0x68);

extern _Actor356100TransformStorage D_actor_356100_801732B0;

/// Reply buffer for the message-0x3F8 query above; the six words after it are
/// zero in the image.
extern GameActorButtonPressHold D_actor_356100_801732D0;

/// Player-character flag selecting which animation block
/// `func_actor_356100_80166018` points `D_actor_356100_80173244.field_0` at:
/// the second block when it is 1, the first otherwise.
extern AnimationSet* D_actor_356100_80173228[7];

/// Free-running scroll `func_actor_356100_80164ACC` accumulates `runStep`
/// into each frame, and zeroes on the live-actor entry. Same role as
/// `Actor01900_D172FC`.
extern u16 D_actor_356100_80173290;

/// Whole-unit part of the last movement step `func_actor_356100_80162AEC`
/// applied, rounded away from zero when the step had a fraction.
static SVECTOR ActorContact_ScratchPosition;

/// Effect record `func_actor_356100_80167818` fills for `effectSpawnHit`:
/// coordinate index 5 of the model, scale 0x100 and count 2. Same shape and
/// roles as `_Actor401300Work.effectArg`.
extern EffectSpawnArg D_actor_356100_801732A8;

/// Animation-set table bound to both work-block contexts by `animationInitContext`.
/// Same role as `Actor01900_D17174`.
extern u8 D_actor_356100_801730B8[];

/// Enemy descriptor `_actor356100Initialize` publishes in the enemy's
/// `field_50` slot and takes `field_40` off. Same role as `Actor01900_D0AC54`.
extern EnemyParams D_actor_356100_8016A984;

/// The three rows `_actor356100Initialize` picks its re-entry pair from on
/// the spawn sub-type. Same role as `Actor01900_D0AC64`.
extern ActorHornedStrangerVariant D_actor_356100_8016A994[];

extern TaskMessageEntry D_actor_356100_80173258[7];

static void _actor356100Initialize(Enemy* enemy, Task* actor);

/// Places the held player facing the enemy and the enemy 1000 units away.
///
/// Entry snapshots the player's parent-frame position and normalizes the
/// horizontal player-to-enemy offset, narrowing it to signed halfwords.
/// Only the enemy's X/Z translation changes. The player consumes the static
/// placement payload during message dispatch; no pointer is retained here.
/// The pull animation's boundary selects the strike. Requires live initialized
/// work and model, the player task and the placement message handler.
static void _actor356100GrabPull(Task* actor);

/// Approach tick, and the sibling of `_actor356100Dormant` above it. Going
/// live clears `D_actor_356100_80173170[16]` and re-seeds the animation slots at
/// clip 2 / speed 0x10 with the enemy's link node cleared; otherwise a single
/// sound 0x51030008 is queued the first time through, keyed on the enemy's
/// `field_8 >> 12` bank. Each frame then snapshots `field_5A & 0x3FF` into
/// `lastCueFrame`, and the frame that first lands on clip 4 spawns the
/// `D_actor_356100_801732A8` effect at model coordinate 5. Once the player is
/// further than 3000 away it plays 0x51030008 as a type-7 event and enters
/// state 6. Same shape as `func_actor_401300_801397F8`.
static void func_actor_356100_80167818(Task* arg0);

static void _actor356100Exit(Task* task);

/// When the work block's `stateEntered` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `_actor00100HideState` without extra flag masks.
static void func_actor_356100_8016A1D8(Task* arg0);

/// When the work block's `stateEntered` flag is set, clears the enemy's link node,
/// reallocates the model buffers and writes the animation request
/// fields; otherwise clears the model's root `composeStamp`. Same shape as
/// `Actor01900_Fn0A7C0` without the two `WorldCollisionBody` flag masks.
static void func_actor_356100_8016A21C(Task* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `animId = 3`.
static void func_actor_356100_8016A2AC(Task* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `animId = 0xB`.
static void func_actor_356100_8016A340(Task* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `animId = 0xB`.
static void func_actor_356100_8016A3D4(Task* arg0);

/// Message 0x3FF payload `func_actor_356100_8016A468` sends the slot-3 task.
/// `field_0` points at `D_actor_356100_80173228`; the function overwrites
/// `field_4` with 2 before the dispatch.
extern AnimationPlayRequest D_actor_356100_80173244;

/// When the work block's `stateEntered` flag is set, writes the animation request
/// fields, sends message 0x3FF then 0x3F9 at slot 3, and snapshots
/// `field_5A & 0x3FF` into `grabAnimFrame`. Bit 1 of `field_68` forces `state`
/// to 0xE.
static void func_actor_356100_8016A468(Task* arg0);

/// When the work block's `stateEntered` flag is set, clears the model's `field_C`,
/// clears the enemy's link node and writes the animation request fields
/// with `hitRadius` forced to 0x180. Bit 0 of `field_68` forces `state` to 7.
/// Same shape as `Actor01900_Fn0AA78` without its two `WorldCollisionBody` flag masks.
static void func_actor_356100_8016A5DC(Task* arg0);

static void _actor356100StatusHold(Task* actor);

static void _actor356100Alert(Task* actor);

/// Runs toward the player, selecting a grab or sidestep from range and facing.
///
/// Requires initialized work, a live model and the live player in the same
/// parent frame. Offsets narrow to signed halfwords; angles use 4096 units per
/// turn. Later ticks correct contacts, turn at most 64 angle units and step
/// 120 units, or 30 during a blend. Facing comparisons retain their different
/// wrapping and the grab test's one-sided turn bound. Uses 16 scratch bytes
/// plus the contact and movement helpers' temporary blocks.
static void _actor356100Chase(Task* actor);

/// Turn-aim tick of the state-8 clip run, the 356100 twin of
/// `_actor01900StateCircle`.
static void func_actor_356100_80164ACC(Task* arg0);

/// Turns through twice the entry bearing, then resumes circling or grabs.
///
/// Entry captures the starting yaw and twice the relative player bearing in
/// signed halfwords. Later ticks approach that stored target by 137 angle
/// units, step 40 parent-coordinate units (20 during a blend) and correct
/// contacts. Completion is tested before that tick's turn, with a grab chosen
/// only after two circles and within 900 units. Requires live initialized
/// work, model and player state, with 16 scratch bytes plus helper storage.
/// Shares the reflected-heading turn with `_actor01900StateTurnAround`.
static void _actor356100TurnAround(Task* actor);

/// Hops along an alternating oblique direction toward the player, then chases.
///
/// Entry selects a side randomly only when none is set, then flips it for the
/// next hop. The first hop adds 369 angle units to the variant's deflection.
/// The cached direction is normalized with 12 fractional bits; movement uses
/// 222 parent-coordinate units, halved during a blend. Only timer values
/// 12..21 move the root; the increment reaching 30 selects chase. Requires
/// live work, model and player in a common parent frame, 16 scratch bytes and
/// contact-helper storage. The direct X/Z step does not check actor freeze.
/// Shares the alternating-hop sequence with `_actor01900StateSidestep`.
static void _actor356100Sidestep(Task* actor);

/// Tick of the state-0xB aim run.
static void func_actor_356100_80166018(Task* arg0);

/// Translates along normalized local Z unless the supplied save freezes actors.
///
/// `save` is a live borrowed save-state view. `stepDistance` is signed
/// parent-coordinate units; negative moves backward and zero still marks
/// composition dirty. Requires a live writable coordinate and eight free
/// scratch-stack bytes. The GTE produces signed halfword step components;
/// the scratch vector is released before return.
static __inline__ void _actorMovementStepLocalZFromSave(McSaveData* save, GfxCoord* coord, s16 stepDistance)
{
    SVECTOR* scratchEnd;
    SVECTOR* displacement;

    if (save->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        scratchEnd                    = SCRATCH_STACK_CURSOR(SVECTOR);
        displacement                  = scratchEnd - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = displacement;
        gfxReadMatrixZAxis(&coord->coord, displacement);
        _actorMovementBuildDisplacement(displacement, stepDistance);
        coord->coord.t[0]  += displacement->vx;
        coord->coord.t[1]  += displacement->vy;
        coord->coord.t[2]  += displacement->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Adds one unit in a fractional 16.16 correction's sign after its integer half.
///
/// `translation` is a live writable parent-frame component. Negative fractions
/// take an extra negative unit below the signed integer half's flooring.
/// The corrected translation must fit a signed word; no pointers are retained.
static __inline__ void _actorContactApplyRootFraction(s32 correctionWord, long* translation)
{
    if ((correctionWord & ACTOR_CONTACT_ROOT_FRACTION_MASK) != 0) {
        if (correctionWord > 0) {
            (*translation)++;
        } else {
            (*translation)--;
        }
    }
}

/// Applies grid-contact correction and height unless the supplied save freezes actors.
///
/// `save` is borrowed live state. Reads exactly `contactCount` records, in
/// 1..32768, under `worldCollisionResolvePushback`'s room-frame contract.
/// XYZ take signed integer halves; fractional X/Z add one unit in their sign,
/// including the extra negative unit below flooring. Adds `heightOffset` to Y
/// even without a grid hit. Requires live disjoint inputs and 72 free scratch
/// bytes; leaves rotation and composition stamp intact. Freeze value 1 skips
/// both correction and height.
static __inline__ void _actorContactPushRootFromSave(McSaveData* save, GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s16 heightOffset)
{
    ActorContactPushScratch* scratchEnd;
    ActorContactPushScratch* push;
    s32                      correctionWord;

    if (save->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        scratchEnd = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
        SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
        push        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
        push->moved = 0;
        if (worldCollisionResolvePushback(contacts, &push->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
            coord->coord.t[0] += scratchEnd[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += push->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += push->delta.fixed.vz.halves.integer;
            correctionWord     = scratchEnd[-1].delta.fixed.vx.word;
            _actorContactApplyRootFraction(correctionWord, &coord->coord.t[0]);
            correctionWord = push->delta.fixed.vz.word;
            _actorContactApplyRootFraction(correctionWord, &coord->coord.t[2]);
        }
        coord->coord.t[1] += heightOffset;
        if (push->delta.fixed.vx.word != 0 || push->delta.fixed.vz.word != 0) {
            push->moved = 1;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    }
}

/// Releases the held player and retreats during the release animation.
///
/// Entry restarts the enemy's release set and requests player animation 3
/// while the player is alive. Cue indices 16..22 step backward 120 units and
/// correct contacts unless actors are frozen. Playback ending clears a live
/// hold and chooses alert for an untargeted enemy, sidestep otherwise. Requires
/// live initialized work, model, enemy and player; temporary scratch belongs
/// to the movement and contact helpers.
static void _actor356100GrabRelease(Task* actor);

/// Tick that dispatches message 0x3F1 and clears the `playerHeld` latch.
static void func_actor_356100_8016A550(Task* arg0);

/// Tick that decrements `stateTimer` and reloads it from `downFramesBase` plus a
/// 4-bit `gRandomLcgState` draw.
static void func_actor_356100_8016A668(Task* arg0);

/// Approaches at half-rate run playback and grabs a player within 900 units.
///
/// Entry enables the model and requests the run set at eight sixteenths of a
/// frame per tick. Later ticks correct contacts, turn by at most 64 angle
/// units and step 120 parent-coordinate units (60 during a blend). The grab
/// test retains its one-sided turn bound. Requires live initialized work,
/// model and player in the same parent frame, 16 scratch bytes and helper
/// storage. This handler has a dispatch slot but no recovered transition in.
static void _actor356100Approach(Task* actor);

/// Tick of the state-0x13 clip run.
static void func_actor_356100_8016A710(Task* arg0);

/// Tick that picks clip 4 or 0x11 off `field_B3A` once the enemy is still
/// alive.
static void func_actor_356100_8016A834(Task* arg0);

/// Per-frame tick of the state-0x15 clip run.
static void func_actor_356100_80167358(Task* arg0);

/// Idles and occasionally fidgets until the player is within 3000 units.
///
/// Requires live initialized work, model, enemy and player in a common parent
/// frame. The idle control jump randomly requests the fidget; its boundary
/// returns to idle. After the timer passes 2400 ticks, a random zero nibble
/// skips the tick, so fifteen of sixteen draws still run it. Entry saves the
/// colour matrix but does not restore it. No scratch block is retained.
static void _actor356100Dormant(Task* actor);

/// Walks between the two stored patrol points, switching within 160 units.
///
/// Requires patrolTarget in 0..1 and initialized live work, model and player
/// state. The waypoint delta narrows to signed halfwords in the root's parent
/// frame; switching keeps the old delta for this tick. Turns by at most 32
/// units of a 4096-unit turn, steps 10 units only without an active blend and
/// then corrects contacts. Uses 12 scratch bytes plus helper storage.
static void _actor356100Patrol(Task* actor);

/// Faces the player, backs away briefly, then turns aside and resumes chasing.
///
/// Requires live initialized work, model and player in their common parent
/// frame. Alignment within 128 angle units starts the retreat set; its timer
/// reaching 19 adds a 1200-unit yaw to one side. The per-tick turn halves the
/// clamped value except when the original negative turn is below -128, which
/// retains -128. Retreat steps are -16 parent-coordinate units, suppressed
/// with contact correction by actor freeze. Uses 16 scratch bytes and helper
/// storage. No recovered transition selects this dispatch slot.
static void _actor356100BackOff(Task* actor);

/// Tick that pushes the actor off any collision record and turns it onto the
/// player.
static void func_actor_356100_8016804C(Task* arg0);

/// Turns toward the player for eleven ticks or an animation boundary, then grabs.
///
/// Requires live initialized work, model and player. Entry requests the windup
/// set at normal speed; later ticks turn the root by at most 32 units of a
/// 4096-unit turn and rebuild its uniform scale, retaining translation. The
/// boundary test precedes this tick's animation update. Uses 16 scratch bytes;
/// no coordinate step or contact correction is performed. No recovered
/// transition selects this dispatch slot.
static void _actor356100GrabWindup(Task* actor);

/// Turn tick that slews the root yaw onto the player in one step and rescales
/// the root coordinate to 0x1194.
static void func_actor_356100_80168E44(Task* arg0);

/// The death-throes tick: runs the per-frame clip, walks part 1's coordinate
/// and fires the 0x600FB effect burst over the model's part coordinates.
static void func_actor_356100_80169180(Task* arg0);

/// The actor's state handlers, stored as a value for whole-table copies.
///
/// `_Actor356100Work::state` is the index. The package defines one table; the
/// per-frame tick copies it to the stack and calls the current state's handler
/// with the actor's task. The call has no terminator or bounds check, so the
/// `NULL` entry of `ACTOR_356100_STATE_UNUSED_1D` marks a state the actor must
/// not be in when the tick dispatches.
typedef struct {
    TaskFunc handlers[ACTOR_356100_STATE_SCRIPTED_DEATH + 1]; // Handler of each `ACTOR_356100_STATE_*`, in state order
} _Actor356100StateTable;
STATIC_ASSERT_SIZEOF(_Actor356100StateTable, 0x7C);

/// The per-frame tick: rebinds the model colour matrix from part 1's world
/// translation, draws the ground quad under the model unless the actor is
/// already dying, walks part 1's parent chain into a scratch coordinate and
/// draws the second quad there, advances the state and dispatches it through
/// the table copy, then walks part 2's chain and rings the result into
/// `_Actor356100Work::bodyPosHistory` as the enemy's next local position.
static void func_actor_356100_80169854(Enemy* arg0, Task* arg1);

static __inline__ void _actor356100BindLightingMatrices(Task* actor);
static __inline__ void _actorPositionDeltaToLivePlayer(const GfxCoord* coord, SVECTOR* toPlayer);

static __inline__ void _actorMovementStepLocalZ(GfxCoord* coord, s16 stepDistance);
static __inline__ void _actorContactPushRoot(GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s16 heightOffset);
static __inline__ s32  _actorContactPushRootAlways(GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s16 heightOffset);
static __inline__ void _actorMovementStepLocalZNonzero(GfxCoord* coord, s16 stepDistance);

/// Binds the actor model to lighting matrices owned by its work block.
///
/// The live model and initialized `_Actor356100Work` must outlive drawing;
/// no matrices are copied or initialized here.
static __inline__ void _actor356100BindLightingMatrices(Task* actor)
{
    _Actor356100Work* work;
    TmdObject*        model;

    work            = actor->work;
    model           = actor->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Writes player minus actor translation in their common parent frame.
///
/// `coord` and the live player's root matrix must share that frame. XYZ narrow
/// to signed 16-bit game units; `toPlayer->pad` is untouched. Inputs must be
/// live and separate from the writable result. No composition is performed.
static __inline__ void _actorPositionDeltaToLivePlayer(const GfxCoord* coord, SVECTOR* toPlayer)
{
    toPlayer->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    toPlayer->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    toPlayer->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
}

/// The overlay's only message-0x3E9 instance; all eight words are zero in the
/// image, so it is a work area rather than a table.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Event-handler table `_actor356100Initialize` hands the task as
/// `Task::msgTable`. Same shape and role as `Actor01900_D1728C`.
// Message-table callbacks use the argument views required by this TU.

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

/// Advances both rigs and mixes their poses on model slots 1..10.
///
/// Slots 11..20 only advance the primary rig. The rigs must be initialized
/// against a live 21-coordinate model and loaded tracks. Primary rates use
/// `animRate - 3`, narrowed to signed bytes; the secondary rate also narrows.
/// `blendWeight` is the primary pose's rotation weight, with ONE as unity.
/// Slot zero is untouched. Reverse ticks require bank-backed current poses.
static void _actor356100TickBlendedAnimation(Task* actor)
{
    AnimationPose     primaryPose;
    AnimationPose     blendPose;
    AnimationContext* primaryContext;
    s16               primaryWeight;
    s16               slotIndex;
    _Actor356100Work* work;

    work           = actor->work;
    primaryWeight  = work->blendWeight;
    primaryContext = &work->rig.anim;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex < ACTOR_356100_BLEND_SLOT_END) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = (work->animRate - ACTOR_356100_BLEND_RATE_BIAS);
            animationTickSlotPose(primaryContext, slotIndex, &primaryPose, 0);
            animationTickSlotPose(&work->blend.anim, slotIndex, &blendPose, 0);
            animationApplyPoseWithBlendedRotation(primaryContext, slotIndex, &primaryPose, &blendPose, primaryWeight, ONE - primaryWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - ACTOR_356100_BLEND_RATE_BIAS);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Seeks driven slots to a changed requested set using its transition duration.
///
/// The actor must own an initialized live work block. Both set indices must
/// be in 0..44 and select loaded tracks, fitting the transition table. An unchanged set is left alone;
/// otherwise each slot captures its current pose and seeks to record offset zero.
/// Leaves slot zero and the request counters intact.
static __inline__ void _actor356100SeekRequestedAnimation(Task* actor)
{
    _Actor356100Work* animationWork;
    s32               slotIndex;

    animationWork = actor->work;
    if (animationWork->appliedAnim != animationWork->animId) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(animationWork->rig.slots); slotIndex++) {
            animationWork->rig.slots[slotIndex].rate = animationWork->animRate;
            animationSeekSlotWithBlend(&animationWork->rig.anim, slotIndex, animationWork->animId, 0,
                                       D_actor_356100_801728CC[animationWork->appliedAnim][animationWork->animId]);
        }
        animationWork->appliedAnim = animationWork->animId;
    }
}

/// Restarts primary slots 1..20 on the requested set and records it as applied.
///
/// Requires a live initialized rig and loaded tracks. Slot reset replaces the
/// stored requested rate with ANIMATION_RATE_ONE; the next tick reinstalls it.
/// Leaves slot zero and the request counters intact.
static __inline__ void _actor356100ResetAnimationSlots(Task* actor)
{
    _Actor356100Work* animationWork;
    s32               slotIndex;

    animationWork = actor->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(animationWork->rig.slots); slotIndex++) {
        animationWork->rig.slots[slotIndex].rate = animationWork->animRate;
        animationResetSlot(&animationWork->rig.anim, slotIndex, animationWork->animId);
    }
    animationWork->appliedAnim = animationWork->animId;
}

/// Restarts secondary slots 1..20 and seeds the next blend's rate and weight.
///
/// Both rigs must be initialized against the live model and loaded tracks.
/// Slot reset leaves the secondary rate at ANIMATION_RATE_ONE. The seeded
/// weight gives the primary rotation half the mix, with ONE as unity. The retained
/// primary-rig rate store precedes each secondary reset; it is overwritten by
/// the next primary tick. Neither the blend-active latch nor request changes.
static __inline__ void _actor356100ResetBlendSlots(Task* actor)
{
    _Actor356100Work* blendWork;
    s32               slotIndex;

    blendWork              = actor->work;
    blendWork->blendRate   = 3 * ANIMATION_RATE_ONE;
    blendWork->blendWeight = ONE / 2;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(blendWork->blend.slots); slotIndex++) {
        // Retain the primary-rig rate store while restarting the secondary rig.
        blendWork->rig.slots[slotIndex].rate = blendWork->blendRate;
        animationResetSlot(&blendWork->blend.anim, slotIndex, blendWork->blendAnimId);
    }
}

/// Advances primary slots 1..20 at the narrowed requested playback rate.
///
/// Requires an initialized live rig with loaded tracks. Rates narrow to signed
/// bytes in sixteenths of a frame; reverse ticks require bank-backed current
/// poses. Slot zero and the request counters are left intact.
static __inline__ void _actor356100TickAnimationSlots(Task* actor)
{
    _Actor356100Work* tickWork;
    s32               slotIndex;

    tickWork = actor->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(tickWork->rig.slots); slotIndex++) {
        tickWork->rig.slots[slotIndex].rate = tickWork->animRate;
        animationTickSlot(&tickWork->rig.anim, slotIndex);
    }
}

/// Applies animation requests, advances the rigs and eases the upper-body yaw.
///
/// The actor must own initialized 21-slot rigs and a live model. Requested
/// and applied set indices must select loaded tracks and fit the 45-column
/// transition table; no bounds are checked. Slots 1..20 are driven, leaving
/// slot zero intact. Rates narrow to signed bytes in sixteenths of a frame;
/// reverse ticks require bank-backed current poses. Yaw is in 4096ths of a
/// turn, eased by 256 and clamped to a quarter turn for the two joints.
/// Request counters reset even for a blend request selecting the same set;
/// `animFrames` retains its low 16 bits on overflow.
static void _actor356100UpdateAnimation(Task* actor)
{
    _Actor356100Work* work;
    s16               jointYaw;

    work = actor->work;
    // Apply pending requests before advancing the newly selected tracks.
    if (work->animRequest == ACTOR_356100_ANIM_REQUEST_BLEND) {
        _actor356100SeekRequestedAnimation(actor);
        work->animRequest  = ACTOR_356100_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (work->animRequest == ACTOR_356100_ANIM_REQUEST_RESET) {
        _actor356100ResetAnimationSlots(actor);
        work->animRequest  = ACTOR_356100_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_356100_ANIM_REQUEST_RESET) {
        _actor356100ResetBlendSlots(actor);
        work->blendRequest = ACTOR_356100_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    if (work->blendActive == 0) {
        _actor356100TickAnimationSlots(actor);
    } else {
        _actor356100TickBlendedAnimation(actor);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blendActive = 0;
        }
    }
    // Ease the look target separately from the root facing.
    if (work->lookYawTarget > work->lookYaw) {
        if (work->lookYawTarget - work->lookYaw > ACTOR_356100_LOOK_YAW_STEP) {
            work->lookYaw += ACTOR_356100_LOOK_YAW_STEP;
        } else {
            work->lookYaw = work->lookYawTarget;
        }
    } else if (work->lookYaw - work->lookYawTarget > ACTOR_356100_LOOK_YAW_STEP) {
        work->lookYaw -= ACTOR_356100_LOOK_YAW_STEP;
    } else {
        work->lookYaw = work->lookYawTarget;
    }
    if (work->lookYaw != 0) {
        jointYaw = work->lookYaw;
        if (work->lookYaw > ACTOR_356100_LOOK_YAW_LIMIT) {
            jointYaw = ACTOR_356100_LOOK_YAW_LIMIT;
        }
        if (work->lookYaw < -ACTOR_356100_LOOK_YAW_LIMIT) {
            jointYaw = -ACTOR_356100_LOOK_YAW_LIMIT;
        }
        _actorRenderYawJointInWorld(&actor->extra.tmd->coords[5], (jointYaw * 2) / 3);
        _actorRenderYawJointInWorld(&actor->extra.tmd->coords[2], jointYaw / 2);
        actor->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        actor->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        actor->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Allocates and initializes this package's Horned Stranger enemy task.
///
/// Requires a live enemy and model with coordinates 0..20. The task owns its
/// zeroed work block; both rigs borrow package-lifetime animation data.
/// Allocation failure destroys the enemy and task. Spawn argument high bits
/// 16..19 select hidden (2), dormant (4), or patrol (others); low bits 0..3
/// select fall/sidestep tuning. Patrol endpoints are 2000 parent-frame units
/// apart. Publishes target contacts, binds messages and retains the enemy
/// through the task's spawn payload; no collision body is initialized here.
static void _actor356100Initialize(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_356100_SPAWN_SELECTOR_MASK = 0xF,
        ACTOR_356100_SPAWN_HIDDEN        = 2,
        ACTOR_356100_SPAWN_DORMANT       = 4,
        ACTOR_356100_TUNING_FIRST        = 2,
        ACTOR_356100_TUNING_THIRD        = 1,
        ACTOR_356100_TUNING_MIXED        = 0,
        ACTOR_356100_PATROL_DISTANCE     = 2000
    };
    SVECTOR           patrolStep;
    SVECTOR*          step;
    VECTOR            viewPosition;
    TmdObject*        model;
    GfxCoord*         root;
    _Actor356100Work* work;
    s32               spawnMode;

    // Allocate actor-owned state before publishing pointers into it.
    root        = actor->extra.tmd->coords;
    model       = actor->extra.tmd;
    work        = memCalloc(sizeof(_Actor356100Work), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->exitCallback = _actor356100Exit;
    _actor356100BindLightingMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_356100_8016A984.hpMax;
    enemy->param                  = &D_actor_356100_8016A984;
    // The contact table is published without constructing a collision body.
    enemy->recs = work->hitContacts;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_356100_801730B8, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)D_actor_356100_801730B8, model, work->blend.poses, work->blend.slots);
    work->animRequest   = ACTOR_356100_ANIM_REQUEST_RESET;
    work->animId        = ACTOR_356100_ANIM_INITIAL;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    _actor356100UpdateAnimation(actor);
    // Build patrol endpoints in the spawn root's parent coordinate frame.
    work->patrolTarget      = 0;
    work->patrolPoints[0].x = actor->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &patrolStep);
    patrolStep.vy = 0;
    step          = &patrolStep;
    VectorNormalSS(step, step);
    gte_lddp(ACTOR_356100_PATROL_DISTANCE);
    gte_ldsv(step);
    gte_gpf12();
    gte_stsv(step);
    work->patrolPoints[1].x = actor->extra.tmd->coords->coord.t[0] + patrolStep.vx;
    work->patrolPoints[1].z = actor->extra.tmd->coords->coord.t[2] + patrolStep.vz;
    actor->msgTable         = D_actor_356100_80173258;
    root->parent            = &gGfxViewCoord;
    root->composeStamp      = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    viewPosition.vx = root->workm.t[0];
    viewPosition.vy = root->workm.t[1];
    viewPosition.vz = root->workm.t[2];
    worldCoordUpdateActorColor(enemy, &viewPosition, 0, 0);
    D_actor_356100_801732A8.coord      = actor->extra.tmd->coords;
    D_actor_356100_801732A8.spawnArgLo = 0x100;
    D_actor_356100_801732A8.spawnArgHi = 2;
    spawnMode                          = actor->spawnArg1.value >> 16;
    switch (spawnMode & ACTOR_356100_SPAWN_SELECTOR_MASK) {
        case ACTOR_356100_SPAWN_HIDDEN:
            work->prevState = -1;
            work->state     = ACTOR_356100_STATE_HIDDEN;
            break;
        case ACTOR_356100_SPAWN_DORMANT:
            work->prevState = -1;
            work->state     = ACTOR_356100_STATE_DORMANT;
            break;
        default:
            work->prevState = -1;
            work->state     = ACTOR_356100_STATE_PATROL;
            tmdAllocPrimitiveBuffer(model);
            break;
    }
    switch (actor->spawnArg1.value & ACTOR_356100_SPAWN_SELECTOR_MASK) {
        case ACTOR_356100_TUNING_FIRST:
            work->downFramesBase = D_actor_356100_8016A994[0].downFramesBase;
            work->sidestepAngle  = D_actor_356100_8016A994[0].sidestepAngle;
            break;
        case ACTOR_356100_TUNING_THIRD:
            work->downFramesBase = D_actor_356100_8016A994[2].downFramesBase;
            work->sidestepAngle  = D_actor_356100_8016A994[2].sidestepAngle;
            break;
        case ACTOR_356100_TUNING_MIXED:
        default:
            // The second tuning, but with the third's sidestep angle.
            work->downFramesBase = D_actor_356100_8016A994[1].downFramesBase;
            work->sidestepAngle  = D_actor_356100_8016A994[2].sidestepAngle;
            break;
    }
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_356100_ROOT_SCALE);
    work->bodyPosCursor = 0;
    actor->state++;
}

/// Oscillates a downed enemy's animation until status buildup expires.
///
/// On state entry, advances the existing back/front down set to cue record
/// 6/9 before returning. Requires that one of those sets is already selected
/// and that playback can reach its cue. Later ticks halve the signed rate,
/// switching between forward and reverse normal speed at +1/-1. Rates are
/// sixteenths of a frame; reverse playback requires bank-backed endpoints.
/// Expiry clears the buildup reaction and selects the down state.
static void _actor356100StatusHold(Task* actor)
{
    _Actor356100Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    s16               backDownSet;
    s16               frontDownSet;
    s32               halvedRate;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        backDownSet                   = ACTOR_356100_ANIM_DOWN_BACK;
        frontDownSet                  = ACTOR_356100_ANIM_DOWN_FRONT;
        model                         = actor->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animRate    = ANIMATION_RATE_ONE;
        do {
            _actor356100UpdateAnimation(actor);
        } while (((work->animId != backDownSet) || ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) < 6U)) &&
                 ((work->animId != frontDownSet) || ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) < 9U)));
        work->animRate = 2 * ANIMATION_RATE_ONE;
        return;
    }
    // Alternate forward and reverse playback as the rate magnitude decays.
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    halvedRate                             = (s16)work->animRate / 2;
    work->animRate                         = (u16)halvedRate;
    if (halvedRate == 1) {
        work->animRate = -(u32)ANIMATION_RATE_ONE;
    }
    if ((s16)work->animRate == -1) {
        work->animRate = ANIMATION_RATE_ONE;
    }
    _actor356100UpdateAnimation(actor);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_356100_STATE_DOWN;
    }
}

/// Adds a relative turn to the current yaw and rebuilds the uniformly scaled root.
///
/// The live writable halfword uses 4096 units per turn and receives the absolute
/// yaw, narrowed to its low 16 bits. Requires initialized live model coordinates
/// disjoint from the turn. Translation survives; pitch and roll are discarded.
static __inline__ void _actor356100ApplyRootTurn(Task* actor, s16* relativeYaw)
{
    GfxCoord* rootCoord;

    rootCoord     = actor->extra.tmd->coords;
    *relativeYaw += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, *relativeYaw, 1);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_356100_ROOT_SCALE);
}

/// Limits a relative bearing and rebuilds the actor root at the resulting yaw.
///
/// `*relativeYaw` and nonnegative `turnLimit` use 4096ths of a turn.
/// The writable turn becomes the absolute yaw, narrowed to a signed halfword.
/// Root pitch and roll are discarded, translation is retained and composition
/// is invalidated. Requires initialized live coordinates and a separate writable
/// turn. No pointer is retained and the scratch stack is not changed.
static __inline__ void _actor356100TurnRoot(Task* actor, s16* relativeYaw, s16 turnLimit)
{
    if (*relativeYaw >= turnLimit + 1) {
        *relativeYaw = turnLimit;
    }
    if (*relativeYaw < -turnLimit) {
        *relativeYaw = -turnLimit;
    }
    _actor356100ApplyRootTurn(actor, relativeYaw);
}

/// Plays the alert animation while turning toward the player, then starts chasing.
///
/// Requires initialized actor work and a live model. Entry enables drawing
/// and targeting, requests the alert set at normal speed and engages battle.
/// Later ticks record the player's relative bearing and turn the root by at
/// most 16 units per call, in 4096ths of a turn, preserving its translation
/// and rebuilding its uniform scale. A slot-1 boundary selects chase.
static void _actor356100Alert(Task* actor)
{
    _Actor356100Work*  work;
    Enemy*             enemy;
    TmdObject*         model;
    ActorChaseScratch* turnScratch;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = ANIMATION_RATE_ONE;
        work->blendActive = 0;
        work->animId      = ACTOR_356100_ANIM_ALERT;
        _actor356100UpdateAnimation(actor);
        work->hitRadius = 0x180;
        sceneEngageBattle(1);
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    turnScratch                            = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_356100_STATE_CHASE;
    }
    turnScratch->turn   = _actorAngleTurnToPlayer(actor, &turnScratch->delta, &gPlayerStatus);
    work->lookYawTarget = turnScratch->turn;
    _actor356100TurnRoot(actor, &turnScratch->turn, ACTOR_356100_ALERT_TURN_STEP);
    _actor356100UpdateAnimation(actor);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Translates a coordinate along its normalized local Z axis.
///
/// `stepDistance` is signed parent-coordinate units; negative moves backward.
/// Live actor freeze value 1 skips all work. Otherwise even zero normalizes
/// and marks composition dirty. Requires a live writable coordinate and an
/// initialized scratch stack with eight free bytes. GTE quantization narrows
/// the step to signed halfwords; no pointer is retained.
static __inline__ void _actorMovementStepLocalZ(GfxCoord* coord, s16 stepDistance)
{
    SVECTOR* scratchEnd;
    SVECTOR* displacement;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        scratchEnd                    = SCRATCH_STACK_CURSOR(SVECTOR);
        SCRATCH_STACK_CURSOR(SVECTOR) = scratchEnd - 1;
        displacement                  = scratchEnd - 1;
        gfxReadMatrixZAxis(&coord->coord, displacement);
        _actorMovementBuildDisplacement(displacement, stepDistance);
        coord->coord.t[0]  += displacement->vx;
        coord->coord.t[1]  += displacement->vy;
        coord->coord.t[2]  += displacement->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Applies grid-contact correction and a vertical offset unless actors are frozen.
///
/// Reads exactly `contactCount` records, in 1..32768, with
/// `worldCollisionResolvePushback`'s bounds and room-frame contract. Adds each
/// signed integer half to XYZ, then one further unit in the sign of each
/// fractional X/Z word. Negative fractions therefore step below flooring.
/// `heightOffset` is added to Y even with no grid hit. Requires live disjoint
/// inputs and 72 free scratch-stack bytes; leaves rotation and composition
/// stamp intact. Freeze value 1 skips the correction and height together.
static __inline__ void _actorContactPushRoot(GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s16 heightOffset)
{
    void**                   scratchCursor;
    ActorContactPushScratch* scratchEnd;
    ActorContactPushScratch* push;
    s32                      correctionWord;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        scratchCursor = SCRATCH_HEAD_ADDR;
        scratchEnd    = SCRATCH_HEAD_AT(scratchCursor, ActorContactPushScratch);
        SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
        push        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
        push->moved = 0;
        if (worldCollisionResolvePushback(contacts, &push->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
            coord->coord.t[0] += scratchEnd[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += push->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += push->delta.fixed.vz.halves.integer;
            correctionWord     = scratchEnd[-1].delta.fixed.vx.word;
            _actorContactApplyRootFraction(correctionWord, &coord->coord.t[0]);
            correctionWord = push->delta.fixed.vz.word;
            _actorContactApplyRootFraction(correctionWord, &coord->coord.t[2]);
        }
        coord->coord.t[1] += heightOffset;
        if (push->delta.fixed.vx.word != 0 || push->delta.fixed.vz.word != 0) {
            push->moved = 1;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    }
}

/// Applies grid-contact correction and height, returning nonzero horizontal correction.
///
/// Reads exactly `contactCount` records, in 1..32768, under
/// `worldCollisionResolvePushback`'s room-frame contract. XYZ take signed
/// integer halves; fractional X/Z add one further unit in their sign, so
/// negative fractions step below flooring. Adds `heightOffset` to Y even
/// without a grid hit. The result is 1 for nonzero resolved X/Z, else 0;
/// height alone does not count. Does not check actor freeze. Requires live
/// disjoint inputs and 72 free scratch bytes; leaves the composition stamp
/// intact. Reads the released block before any scratch reuse.
static __inline__ s32 _actorContactPushRootAlways(GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s16 heightOffset)
{
    void**                   scratchCursor;
    ActorContactPushScratch* scratchEnd;
    ActorContactPushScratch* push;
    s32                      correctionWord;

    scratchCursor = SCRATCH_HEAD_ADDR;
    scratchEnd    = SCRATCH_HEAD_AT(scratchCursor, ActorContactPushScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
    push        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
    push->moved = 0;
    if (worldCollisionResolvePushback(contacts, &push->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        coord->coord.t[0] += scratchEnd[-1].delta.fixed.vx.halves.integer;
        coord->coord.t[1] += push->delta.fixed.vy.halves.integer;
        coord->coord.t[2] += push->delta.fixed.vz.halves.integer;
        correctionWord     = scratchEnd[-1].delta.fixed.vx.word;
        _actorContactApplyRootFraction(correctionWord, &coord->coord.t[0]);
        correctionWord = push->delta.fixed.vz.word;
        _actorContactApplyRootFraction(correctionWord, &coord->coord.t[2]);
    }
    coord->coord.t[1] += heightOffset;
    if (push->delta.fixed.vx.word != 0 || push->delta.fixed.vz.word != 0) {
        push->moved = 1;
    }
    // No scratch reservation or call intervenes before reading the released result.
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    return push->moved;
}

static void _actor356100Chase(Task* actor)
{
    enum { RUN_RATE                        = 18,
           MIN_SIDESTEP_DELAY              = 3,
           PLAYER_FACING_TOLERANCE         = 0x44,
           PLAYER_TURN_ANIMATION_THRESHOLD = 0x200,
           SIDESTEP_FACING_LIMIT           = 0x80,
           SIDESTEP_MIN_DISTANCE           = 1800,
           GRAB_MAX_DISTANCE               = 1100,
           GRAB_TURN_LIMIT                 = 0x200,
           ROOT_TURN_STEP                  = 0x40,
           RUN_STEP                        = 120,
           BLENDED_RUN_STEP                = 30
    };
    _Actor356100Work*  work;
    ActorChaseScratch* chase;
    TmdObject*         model;
    Enemy*             enemy;
    s32                unwrappedPlayerFacingDelta;
    s32                wrappedPlayerFacingDelta;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = RUN_RATE;
        work->blendActive = 0;
        work->animId      = ACTOR_356100_ANIM_RUN;
        _actor356100UpdateAnimation(actor);
        work->circleCount  = 0;
        work->stateTimer   = 0;
        work->stateCounter = 0;
        return;
    }
    work->stateTimer = (u16)work->stateTimer + 1;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    _actorContactPushRoot(actor->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &chase->delta);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor356100UpdateAnimation(actor);
    // Compare the player facing with the reverse bearing before choosing a move.
    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer       = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer       = _actorAngleNormalizeYaw(chase->yawFromPlayer);
    chase->turn                = _actorAngleTurnToOffset(actor->extra.tmd->coords, chase->delta.vx, chase->delta.vz);
    work->lookYawTarget        = chase->turn;
    unwrappedPlayerFacingDelta = chase->yawFromPlayer - chase->playerYaw;
    if (ABS(unwrappedPlayerFacingDelta) < PLAYER_FACING_TOLERANCE && (((s16)work->sidestepCount / 2) + MIN_SIDESTEP_DELAY) < work->stateTimer && ABS(chase->turn) < SIDESTEP_FACING_LIMIT) {
        if (_actorRangeOutsideRadiusXZ(&chase->delta, SIDESTEP_MIN_DISTANCE)) {
            work->state = ACTOR_356100_STATE_SIDESTEP;
        }
    }
    wrappedPlayerFacingDelta = _actorAngleNormalizeYaw(chase->yawFromPlayer - chase->playerYaw);
    if (ABS(wrappedPlayerFacingDelta) >= PLAYER_TURN_ANIMATION_THRESHOLD + 1 && (((s16)work->sidestepCount / 2) + MIN_SIDESTEP_DELAY) < work->stateTimer && work->stateCounter == 0) {
        work->stateCounter = 1;
        work->animId       = ACTOR_356100_ANIM_ALERT;
        work->animRequest  = ACTOR_356100_ANIM_REQUEST_BLEND;
    }
    if (chase->turn < GRAB_TURN_LIMIT) {
        if (!_actorRangeOutsideRadiusXZ(&chase->delta, GRAB_MAX_DISTANCE)) {
            work->state = ACTOR_356100_STATE_GRAB;
        }
    }
    // Turn and advance while retaining the signed, one-sided grab predicate.
    _actor356100TurnRoot(actor, &chase->turn, ROOT_TURN_STEP);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ACTOR_356100_ANIM_RUN) {
        if (work->blendActive == 0) {
            _actorMovementStepLocalZ(actor->extra.tmd->coords, RUN_STEP);
        } else {
            _actorMovementStepLocalZ(actor->extra.tmd->coords, BLENDED_RUN_STEP);
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = ACTOR_356100_ANIM_RUN;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn-aim state body, the 356100 twin of `_actor01900StateCircle`: take a 0x10
/// chase scratch off the scratch stack and, on the live-actor flag, key the
/// animation nodes, the frame counter and the `circleRateStep` clip phase. Once
/// `stateCounter` has counted 7 frames the arm aims at the player — the player's
/// own facing yaw goes in `playerYaw`, the wrapped yaw from the player back to
/// the actor in `yawFromPlayer` — and the root is turned by the facing
/// yaw plus a +-0x60 clamp of the turn's 1000 bias. The forward draw
/// `runStep` is the doubled frame parameter (halved while `blendActive` is
/// up, forced to 2 while the frame counter runs), and the actor slides along
/// it unless movement is frozen. `circleRateStep` walks 8 -> -1 -> 0 as `animRate`
/// passes 0x18 and 0x12, and the 0 arm runs the five-frame exit window that
/// re-aims once more and picks state 0xB when the actor faces away from the
/// player, else state 0x1A.
static void func_actor_356100_80164ACC(Task* arg0)
{
    _Actor356100Work*      work;
    ActorChaseScratch*     head;
    ActorChaseScratch*     chase;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              facing;
    GfxCoord*              pushCoord;
    s32                    turn;
    s32                    diffPos;
    s32                    diffNeg;
    s32                    yaw;
    WorldCollisionContact* records;
    s32                    hit;
    s32                    paused;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0xC0;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->blendActive = 0;
        work->animId      = 3;
        _actor356100UpdateAnimation(arg0);
        work->circleRateStep    = 8;
        work->stateTimer        = 0;
        work->stateCounter      = 0;
        D_actor_356100_80173290 = 0;
        work->circleCount++;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    chase                                   = head - 1;
    arg0->extra.tmd->coords->composeStamp   = GRAPHICS_COORD_DIRTY;
    _actor356100UpdateAnimation(arg0);
    paused    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
    pushCoord = arg0->extra.tmd->coords;
    records   = work->pushContacts;
    if (paused == 1) {
        hit = 0;
    } else {
        hit = _actorContactPushRootAlways(pushCoord, records, ARRAY_SIZE(work->pushContacts), 0x10);
    }
    if (hit != 0) {
        work->stateCounter++;
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->stateCounter >= 7) {
        chase->playerYaw     = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
        work->state          = ACTOR_356100_STATE_SLIDE;
    }
    coord       = arg0->extra.tmd->coords;
    chase->turn = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn        = chase->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (((diffPos < 0) ? -diffPos : diffPos) < 0x60) {
            chase->heading = chase->turn - 1000;
        } else if (diffPos > 0) {
            chase->heading = 0x60;
        } else {
            chase->heading = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (((diffNeg < 0) ? -diffNeg : diffNeg) < 0x60) {
            chase->heading = chase->turn + 1000;
        } else if (diffNeg > 0) {
            chase->heading = 0x60;
        } else {
            chase->heading = -0x60;
        }
    }
    facing          = arg0->extra.tmd->coords;
    chase->heading += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->heading, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_356100_ROOT_SCALE);
    coord                                 = arg0->extra.tmd->coords;
    work->lookYawTarget                   = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->runStep                         = work->animRate * 8;
    if (work->blendActive != 0) {
        work->runStep = work->runStep >> 1;
    }
    if (work->stateCounter != 0) {
        work->runStep = 2;
    }
    _actorMovementTranslateForwardNonzero(arg0->extra.tmd->coords, work->runStep);
    D_actor_356100_80173290 += work->runStep;
    if (work->circleRateStep == 8 && work->animRate >= 0x18) {
        work->circleRateStep = -1;
    }
    if (work->circleRateStep == -1 && work->animRate == 0x12) {
        work->circleRateStep = 0;
        work->stateTimer     = 0;
    }
    if (work->circleRateStep == 0) {
        if (++work->stateTimer == 5) {
            chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            _actorPositionDeltaToPlayer(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
            chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
            yaw                  = _actorAngleNormalizeYaw(chase->yawFromPlayer);
            chase->yawFromPlayer = yaw;
            yaw                  = yaw - chase->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw >= 0x401) {
                work->state = ACTOR_356100_STATE_GRAB;
            } else {
                work->state     = ACTOR_356100_STATE_SLIDE;
                work->prevState = -1;
            }
        }
    }
    work->animRate += (u16)work->circleRateStep;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void _actor356100TurnAround(Task* actor)
{
    enum { TURN_STEP            = 137,
           GRAB_MAX_DISTANCE    = 900,
           FORWARD_STEP         = 40,
           BLENDED_FORWARD_STEP = 20 };
    _Actor356100Work*  work;
    TmdObject*         model;
    Enemy*             enemy;
    GfxCoord*          bearingCoord;
    GfxCoord*          playerOffsetCoord;
    GfxCoord*          headingCoord;
    ActorChaseScratch* scratchEnd;
    ActorChaseScratch* turnScratch;
    s32                nextState;

    work = actor->work;
    if (work->stateEntered != 0) {
        scratchEnd                              = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        model                                   = actor->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch) = scratchEnd - 1;
        turnScratch                             = scratchEnd - 1;
        enemy                                   = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags           = 0;
        model->flags                            = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius     = 0x180;
        work->animRequest   = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate      = ANIMATION_RATE_ONE;
        work->blendActive   = 0;
        work->animId        = ACTOR_356100_ANIM_RUN;
        work->lookYawTarget = 0;
        _actor356100UpdateAnimation(actor);
        playerOffsetCoord       = actor->extra.tmd->coords;
        scratchEnd[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - playerOffsetCoord->coord.t[0];
        turnScratch->delta.vy   = gPlayerStatus.coordMtx->t[1] - playerOffsetCoord->coord.t[1];
        turnScratch->delta.vz   = gPlayerStatus.coordMtx->t[2] - playerOffsetCoord->coord.t[2];
        bearingCoord            = actor->extra.tmd->coords;
        turnScratch->turn       = _actorAngleTurnToOffset(bearingCoord, scratchEnd[-1].delta.vx, turnScratch->delta.vz);
        headingCoord            = actor->extra.tmd->coords;
        turnScratch->heading    = ratan2(-headingCoord->coord.m[2][0], headingCoord->coord.m[2][2]);
        work->turnYaw           = turnScratch->heading;
        work->turnYawTarget     = turnScratch->heading + (u16)turnScratch->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    scratchEnd                              = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = scratchEnd - 1;
    turnScratch                             = scratchEnd - 1;
    _actor356100UpdateAnimation(actor);
    playerOffsetCoord       = actor->extra.tmd->coords;
    scratchEnd[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - playerOffsetCoord->coord.t[0];
    turnScratch->delta.vy   = gPlayerStatus.coordMtx->t[1] - playerOffsetCoord->coord.t[1];
    turnScratch->delta.vz   = gPlayerStatus.coordMtx->t[2] - playerOffsetCoord->coord.t[2];
    // Test the stored target before advancing this tick, preserving the extra tick.
    if (work->turnYaw == work->turnYawTarget) {
        if (work->circleCount < 2 || _actorRangeOutsideRadiusXZ(&turnScratch->delta, GRAB_MAX_DISTANCE)) {
            nextState = ACTOR_356100_STATE_CIRCLE;
        } else {
            nextState = ACTOR_356100_STATE_GRAB;
        }
        work->state = nextState;
    }
    if (work->turnYaw > work->turnYawTarget) {
        work->turnYaw -= TURN_STEP;
        if (work->turnYaw < work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    if (work->turnYaw < work->turnYawTarget) {
        work->turnYaw += TURN_STEP;
        if (work->turnYaw > work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    gfxRotMatrixY(&actor->extra.tmd->coords->coord, work->turnYaw, 1);
    _actorRenderRescaleYaw(actor->extra.tmd->coords, ACTOR_356100_ROOT_SCALE);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        _actorMovementStepLocalZ(actor->extra.tmd->coords, FORWARD_STEP);
    } else {
        _actorMovementStepLocalZ(actor->extra.tmd->coords, BLENDED_FORWARD_STEP);
    }
    _actorContactPushRoot(actor->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void _actor356100Sidestep(Task* actor)
{
    enum { FIRST_HOP_EXTRA_YAW = 369,
           HOP_STEP            = 222,
           MOVE_START_TICK     = 12,
           MOVE_TICKS          = 10,
           END_TICK            = 30 };
    _Actor356100Work*  work;
    ActorChaseScratch* scratchEnd;
    ActorChaseScratch* sidestep;
    TmdObject*         model;
    Enemy*             enemy;
    GfxCoord*          stepCoord;
    SVECTOR*           direction;
    MATRIX             directionMatrix;
    u16                firstHopYaw;

    scratchEnd                              = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    work                                    = actor->work;
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = scratchEnd - 1;
    sidestep                                = scratchEnd - 1;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius  = 0xC0;
        work->stateTimer = 0;
        _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &sidestep->delta);
        sidestep->turn = ratan2(scratchEnd[-1].delta.vx, sidestep->delta.vz);
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = 1;
            } else {
                work->sidestepSide = -1;
            }
        }
        if (work->sidestepSide == 1) {
            work->animId = ACTOR_356100_ANIM_SIDESTEP_POSITIVE_YAW;
            if (work->sidestepCount == 0) {
                firstHopYaw    = sidestep->turn + FIRST_HOP_EXTRA_YAW;
                sidestep->turn = work->sidestepAngle + firstHopYaw;
            } else {
                sidestep->turn += work->sidestepAngle;
            }
            work->sidestepSide = -1;
        } else {
            work->animId = ACTOR_356100_ANIM_SIDESTEP_NEGATIVE_YAW;
            if (work->sidestepCount == 0) {
                firstHopYaw    = sidestep->turn - FIRST_HOP_EXTRA_YAW;
                sidestep->turn = firstHopYaw - work->sidestepAngle;
            } else {
                sidestep->turn -= work->sidestepAngle;
            }
            work->sidestepSide = 1;
        }
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 3 * ANIMATION_RATE_ONE / 4;
        work->blendActive = 0;
        _actor356100UpdateAnimation(actor);
        gfxRotMatrixY(&directionMatrix, sidestep->turn, 1);
        direction = &work->sidestepDir;
        gfxReadMatrixZAxis(&directionMatrix, direction);
        VectorNormalSS(direction, direction);
        work->sidestepStep = HOP_STEP;
        work->sidestepCount++;
    }
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor356100UpdateAnimation(actor);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestep->delta);
    } else {
        gte_lddp((s16)work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestep->delta);
    }
    // Timer gates the horizontal hop; contact correction still observes freeze.
    if ((u32)((u16)work->stateTimer - MOVE_START_TICK) < (u32)MOVE_TICKS) {
        stepCoord              = actor->extra.tmd->coords;
        stepCoord->coord.t[0] += sidestep->delta.vx;
        stepCoord              = actor->extra.tmd->coords;
        stepCoord->coord.t[2] += sidestep->delta.vz;
        _actorContactPushRoot(actor->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
    }
    if (++work->stateTimer >= END_TICK) {
        work->state = ACTOR_356100_STATE_CHASE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn-and-close tick, and the sibling of `_actor356100GrabPull` above
/// it. Going live writes the animation request fields with `hitRadius`
/// forced to 0x180 and the enemy's link node cleared, then turns the root
/// coordinate onto the player through `_actorAngleTurnToPlayer` and rebuilds
/// its Y rotation at a uniform 0x1194 scale, re-seeding the offset from the
/// player and clearing the two halfwords next to `playerHeld`. Each frame then
/// re-runs the animation and, while the clip sits on 0x10 and the player is not
/// in mode 2, takes the player offset again through
/// `actorMatrixPositionYaw` and — if the turn is within 0x10 and the
/// player is closer than 0x44C — points `D_actor_356100_80173244.field_0` at
/// one of the two blocks `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` selects, then queries message 0x3F8 and
/// on acceptance moves to state 0xC, sets `playerHeld` and re-sends the handler
/// as message 0x3FF. Bit 0 of `field_68` forces `state` to 7 on clip 4, and
/// past clip 0x10 the actor is pushed one normalised unit away from the player
/// unless it is further than 0x578.
static void func_actor_356100_80166018(Task* arg0)
{
    SVECTOR           pos;
    SVECTOR*          p;
    _Actor356100Work* work;
    Enemy*            enemy;
    GameActor*        player;
    PlayerStatus*     config;
    GfxCoord*         coord;
    s16               angle;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = (GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work;
    config = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate                = 0x10;
        work->animId                  = 4;
        _actor356100UpdateAnimation(arg0);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, _actorAngleTurnToPlayer(arg0, &pos, config), 0);
        _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_356100_ROOT_SCALE);
        pos.vx                                = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                                = 0;
        pos.vz                                = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
    }
    _actor356100UpdateAnimation(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x10 && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        angle = actorMatrixPositionYaw(arg0, &pos, gPlayerStatus.coordMtx);
        if (abs(angle) < 0x10 && !_actorRangeOutsideRadiusXZ(&pos, 0x44C)) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                D_actor_356100_80173244.source.sets = &D_actor_356100_80173228[2];
            } else {
                D_actor_356100_80173244.source.sets = D_actor_356100_80173228;
            }
            D_actor_356100_801732D0.pressCount = 8;
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_356100_801732D0, 0) == 0) {
                work->state                         = ACTOR_356100_STATE_GRAB_PULL;
                work->playerHeld                    = 1;
                D_actor_356100_80173244.animationId = 1;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_356100_80173244, 0);
            }
        }
    }
    if (work->animId == 4 && (work->rig.slots[1].status.fields.flags & 1)) {
        work->state = ACTOR_356100_STATE_CHASE;
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

static void _actor356100GrabPull(Task* actor)
{
    enum { PLAYER_SEPARATION = 1000 };
    _Actor356100Work* work;
    Enemy*            enemy;
    Task*             player;
    SVECTOR*          separationPointer;
    SVECTOR           separation;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        player                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->hitRadius                         = 0x180;
        enemy->node.state.parts.flags           = 0;
        work->animRequest                       = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate                          = ANIMATION_RATE_ONE;
        work->animId                            = ACTOR_356100_ANIM_GRAB_PULL;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(player->extra.tmd->coords);
        D_actor_356100_801732B0.placement.pos.vx = player->extra.tmd->coords->coord.t[0];
        D_actor_356100_801732B0.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
        D_actor_356100_801732B0.placement.pos.vz = player->extra.tmd->coords->coord.t[2];
        separationPointer                        = &separation;
        // Form the horizontal separation before placing either task.
        separation.vx = actor->extra.tmd->coords->coord.t[0] - player->extra.tmd->coords->coord.t[0];
        separation.vy = 0;
        separation.vz = actor->extra.tmd->coords->coord.t[2] - player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(separationPointer, separationPointer);
        gte_lddp(PLAYER_SEPARATION);
        gte_ldsv(separationPointer);
        gte_gpf12();
        gte_stsv(separationPointer);
        actor->extra.tmd->coords->coord.t[0]     = player->extra.tmd->coords->coord.t[0] + separation.vx;
        actor->extra.tmd->coords->coord.t[2]     = player->extra.tmd->coords->coord.t[2] + separation.vz;
        actor->extra.tmd->coords->composeStamp   = GRAPHICS_COORD_DIRTY;
        D_actor_356100_801732B0.placement.rot.vx = 0;
        D_actor_356100_801732B0.placement.rot.vy = ratan2(separation.vx, separation.vz);
        D_actor_356100_801732B0.placement.rot.vz = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_356100_801732B0.placement, 0);
    }
    _actor356100UpdateAnimation(actor);
    if (work->animId == ACTOR_356100_ANIM_GRAB_PULL && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ACTOR_356100_STATE_GRAB_STRIKE;
    }
}

static void _actor356100GrabRelease(Task* actor)
{
    enum { RETREAT_CUE_START        = 16,
           RETREAT_CUE_COUNT        = 7,
           RETREAT_STEP             = -120,
           PLAYER_RELEASE_ANIMATION = 3 };
    _Actor356100Work* work;
    Enemy*            enemy;
    PlayerStatus*     playerStatus;
    McSaveData*       saveData;
    GfxCoord*         stepCoord;
    GfxCoord*         pushCoord;

    work         = actor->work;
    enemy        = actor->spawnArg2.pointer;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->animRate    = ANIMATION_RATE_ONE;
        work->animId      = ACTOR_356100_ANIM_GRAB_RELEASE;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        _actor356100UpdateAnimation(actor);
        D_actor_356100_80173244.animationId = PLAYER_RELEASE_ANIMATION;
        if (playerStatus->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_356100_80173244, 0);
        }
        work->stateTimer = 0;
    } else if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 &&
               playerStatus->hp > 0 && work->playerHeld == 1) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        work->playerHeld = 0;
    }
    // Retreat only during the release cue window, using the live freeze state.
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - RETREAT_CUE_START < (u32)RETREAT_CUE_COUNT) {
        saveData  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        stepCoord = actor->extra.tmd->coords;
        if (saveData->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
            _actorMovementStepLocalZFromSave(saveData, stepCoord, RETREAT_STEP);
        }
        pushCoord = actor->extra.tmd->coords;
        if (saveData->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
            _actorContactPushRootFromSave(saveData, pushCoord, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
        }
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _actor356100UpdateAnimation(actor);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (enemy->node.state.parts.targeted != 1) {
            work->state = ACTOR_356100_STATE_ALERT;
        } else {
            work->state = ACTOR_356100_STATE_SIDESTEP;
        }
        if (playerStatus->hp > 0 && work->playerHeld == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->playerHeld = 0;
        }
    }
}

static void _actor356100Approach(Task* actor)
{
    enum { GRAB_MAX_DISTANCE    = 900,
           GRAB_TURN_LIMIT      = 0x200,
           ROOT_TURN_STEP       = 0x40,
           FORWARD_STEP         = 120,
           BLENDED_FORWARD_STEP = 60 };
    _Actor356100Work*  work;
    TmdObject*         model;
    Enemy*             enemy;
    ActorChaseScratch* chase;
    s16                relativeTurn;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = ANIMATION_RATE_ONE / 2;
        work->blendActive = 0;
        work->animId      = ACTOR_356100_ANIM_RUN;
        _actor356100UpdateAnimation(actor);
        work->circleCount = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    _actorContactPushRoot(actor->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
    _actorPositionDeltaToPlayer(&gPlayerStatus, actor->extra.tmd->coords, &chase->delta);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor356100UpdateAnimation(actor);
    relativeTurn        = _actorAngleTurnToOffset(actor->extra.tmd->coords, chase->delta.vx, chase->delta.vz);
    chase->turn         = relativeTurn;
    work->lookYawTarget = relativeTurn;
    // Preserve the one-sided reach test before limiting the turn.
    if (chase->turn < GRAB_TURN_LIMIT) {
        if (!_actorRangeOutsideRadiusXZ(&chase->delta, GRAB_MAX_DISTANCE)) {
            work->state = ACTOR_356100_STATE_GRAB;
        }
    }
    _actor356100TurnRoot(actor, &chase->turn, ROOT_TURN_STEP);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        _actorMovementStepLocalZ(actor->extra.tmd->coords, FORWARD_STEP);
    } else {
        _actorMovementStepLocalZ(actor->extra.tmd->coords, BLENDED_FORWARD_STEP);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Rotation-collapse tick: going live clears the model's `field_C`, flags the
/// enemy's link node and re-seeds `stateTimer`. Each frame then bumps `stateTimer`
/// and fires its milestone — 0x18 releases state F0 (arg 0xA), 0x1D switches
/// light mode 1 and spawns effect 0x600A5 at model coordinate 2, 0x29 sets
/// `field_C` to 2, 0x2F switches light mode 2 and 0x33 sets `field_C` to 0x80.
/// From 0x1A on, the root rotation is rebuilt in the 0x34-byte scratch block
/// with X/Z held at `ACTOR_356100_ROOT_SCALE` and Y reduced by 0xB per frame past 0x14, and
/// written back into the root coordinate with `composeStamp` cleared. Same body as
/// `Actor01900_Fn06904`.
static void func_actor_356100_80167358(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    s16               cur;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer              = 0;
    }
    if (work->stateTimer < 0x401) {
        switch ((s16)(work->stateTimer++ - 0x18)) {
            case 0:
                sceneReleaseBattleRefWithRewards(arg0, 0xA);
                break;
            case 5:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                effectSpawn(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 17:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 39:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        cur = work->stateTimer;
        if (cur >= 0x1A) {
            _actorRenderRescaleYawY(arg0->extra.tmd->coords, ACTOR_356100_ROOT_SCALE, ACTOR_356100_ROOT_SCALE - (cur - 0x14) * 0xB);
        }
    }
}

static void _actor356100Dormant(Task* actor)
{
    enum { RANDOM_SKIP_AFTER_TICKS = 2400,
           RANDOM_SKIP_MASK        = 0xF,
           WAKE_DISTANCE           = 3000 };
    _Actor356100Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    SVECTOR           toPlayer;
    SVECTOR*          offsetPointer;

    work = actor->work;
    if (work->stateEntered != 0) {
        model        = actor->extra.tmd;
        enemy        = actor->spawnArg2.pointer;
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = ACTOR_356100_ANIM_DORMANT;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate                = work->baseRate;
    }
    // After the idle threshold, only a zero random nibble skips this tick.
    if (work->stateTimer > RANDOM_SKIP_AFTER_TICKS) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & RANDOM_SKIP_MASK)) {
            return;
        }
    } else {
        work->stateTimer = (s16)((u16)work->stateTimer + 1);
    }
    rootCoord     = actor->extra.tmd->coords;
    offsetPointer = &toPlayer;
    _actorPositionDeltaToLivePlayer(rootCoord, offsetPointer);
    if (!_actorRangeOutsideRadiusXZ(offsetPointer, WAKE_DISTANCE)) {
        work->state = ACTOR_356100_STATE_ALERT;
    }
    _actor356100UpdateAnimation(actor);
    if (work->animId == ACTOR_356100_ANIM_DORMANT && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->animId      = ACTOR_356100_ANIM_DORMANT_FIDGET;
            work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
            _actor356100UpdateAnimation(actor);
        }
    }
    if (work->animId == ACTOR_356100_ANIM_DORMANT_FIDGET && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = ACTOR_356100_ANIM_DORMANT;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        _actor356100UpdateAnimation(actor);
    }
}

static void func_actor_356100_80167818(Task* arg0)
{
    _Actor356100Work* work;
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
        D_actor_356100_80173170[16] = NULL;
        work->animId                = 0x10;
        work->animRequest           = ACTOR_356100_ANIM_REQUEST_RESET;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius               = 0x180;
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
    _actor356100UpdateAnimation(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 4 && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
        D_actor_356100_801732A8.coord      = arg0->extra.tmd->coords;
        D_actor_356100_801732A8.spawnArgLo = 0x100;
        D_actor_356100_801732A8.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), arg0->extra.tmd->coords + 5, NULL,
                       &D_actor_356100_801732A8);
    }
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord              = arg0->extra.tmd->coords;
    d                  = &delta;
    _actorPositionDeltaToLivePlayer(coord, d);
    if (!_actorRangeOutsideRadiusXZ(d, 3000)) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        work->state = ACTOR_356100_STATE_ALERT;
    }
}

/// Translates along normalized local Z for a nonzero signed distance.
///
/// `stepDistance` is in parent-coordinate units. Live freeze value 1 skips
/// all work; zero otherwise reserves and releases the eight-byte scratch
/// vector but leaves the coordinate intact. Requires a live writable
/// coordinate and initialized scratch stack; changes GTE state for a step.
static __inline__ void _actorMovementStepLocalZNonzero(GfxCoord* coord, s16 stepDistance)
{
    SVECTOR* step;
    SVECTOR* displacement;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
        step                          = SCRATCH_STACK_CURSOR(SVECTOR) - 1;
        displacement                  = step;
        SCRATCH_STACK_CURSOR(SVECTOR) = displacement;
        if (stepDistance != 0) {
            gfxReadMatrixZAxis(&coord->coord, displacement);
            _actorMovementBuildDisplacement(displacement, stepDistance);
            coord->coord.t[0]  += step->vx;
            coord->coord.t[1]  += displacement->vy;
            coord->coord.t[2]  += displacement->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static void _actor356100Patrol(Task* actor)
{
    enum { WAYPOINT_DISTANCE = 160,
           ROOT_TURN_STEP    = 0x20,
           WALK_STEP         = 10 };
    _Actor356100Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    ActorTurnScratch* scratchEnd;
    ActorTurnScratch* patrolTurn;
    s16               relativeYaw;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = ANIMATION_RATE_ONE;
        work->blendActive = 0;
        work->animId      = ACTOR_356100_ANIM_WALK;
        _actor356100UpdateAnimation(actor);
        return;
    }
    scratchEnd              = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    scratchEnd[-1].delta.vx = work->patrolPoints[work->patrolTarget].x - actor->extra.tmd->coords->coord.t[0];
    patrolTurn              = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    patrolTurn->delta.vy    = 0;
    patrolTurn->delta.vz    = work->patrolPoints[work->patrolTarget].z - actor->extra.tmd->coords->coord.t[2];
    // Switch endpoints without replacing this tick's already-sampled delta.
    if (!_actorRangeOutsideRadiusXZ(&patrolTurn->delta, WAYPOINT_DISTANCE)) {
        if (work->patrolTarget == 0) {
            work->patrolTarget = 1;
        } else {
            work->patrolTarget = 0;
        }
    }
    _actor356100UpdateAnimation(actor);
    rootCoord           = actor->extra.tmd->coords;
    relativeYaw         = ratan2(patrolTurn->delta.vx, patrolTurn->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    patrolTurn->angle   = _actorAngleNormalizeYaw(relativeYaw);
    work->lookYawTarget = patrolTurn->angle;
    _actor356100TurnRoot(actor, &patrolTurn->angle, ROOT_TURN_STEP);
    if (work->blendActive == 0) {
        _actorMovementStepLocalZNonzero(actor->extra.tmd->coords, WALK_STEP);
    }
    _actorContactPushRoot(actor->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turn-and-push tick, the sibling of `_actor356100Alert` above it and
/// the same body as `func_actor_401300_8013A208`. The live branch resets the
/// model and starts clip 1 at speed 0x10 with the 0x12 state parked in
/// `animId`; otherwise the turn scratch takes the player offset,
/// `_actorAngleTurnToPlayer` gives the wrapped turn, `lookYawTarget` snapshots it,
/// it is clamped to [-0x40, 0x40] and the root yaw is re-derived from it. The
/// root is then pushed out of the `pushContacts` collision records and one
/// normalised unit along its own Y column scaled by `runStep`, which decays
/// by 0xA per frame — once it reaches zero, or bit 0 of `field_68` is set, the
/// state moves to 9 and the turn scratch is given back.
static void func_actor_356100_8016804C(Task* arg0)
{
    _Actor356100Work* work;
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
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        obj->flags        = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius               = 0x180;
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
    _actorContactPushRoot(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    _actorMovementTranslateForwardNonzero(arg0->extra.tmd->coords, work->runStep);
    if (work->runStep > 0) {
        next          = work->runStep - 0xA;
        work->runStep = next;
        if ((s16)next < 0) {
            work->runStep = 0;
        }
    }
    _actor356100UpdateAnimation(arg0);
    if ((work->rig.slots[1].status.fields.flags & 1) || work->runStep == 0) {
        work->state = ACTOR_356100_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void _actor356100BackOff(Task* actor)
{
    enum { ALIGNMENT_LIMIT = 0x80,
           RETREAT_RATE    = 22,
           RETREAT_STEP    = -16,
           RETREAT_TICKS   = 19,
           EXIT_TURN       = 1200 };
    _Actor356100Work*  work;
    TmdObject*         model;
    Enemy*             enemy;
    GfxCoord*          stepCoord;
    GfxCoord*          playerOffsetCoord;
    GfxCoord*          pushCoord;
    void**             scratchCursor;
    ActorChaseScratch* scratchEnd;
    ActorChaseScratch* backOff;
    McSaveData*        saveData;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = RETREAT_RATE;
        work->blendActive = 0;
        work->animId      = ACTOR_356100_ANIM_WALK;
        _actor356100UpdateAnimation(actor);
        return;
    }
    _actor356100UpdateAnimation(actor);
    scratchCursor           = SCRATCH_HEAD_ADDR;
    playerOffsetCoord       = actor->extra.tmd->coords;
    scratchEnd              = SCRATCH_HEAD_AT(scratchCursor, ActorChaseScratch);
    scratchEnd[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - playerOffsetCoord->coord.t[0];
    backOff                 = (SCRATCH_HEAD_AT(scratchCursor, ActorChaseScratch) = scratchEnd - 1);
    backOff->delta.vy       = gPlayerStatus.coordMtx->t[1] - playerOffsetCoord->coord.t[1];
    backOff->delta.vz       = gPlayerStatus.coordMtx->t[2] - playerOffsetCoord->coord.t[2];
    backOff->turn           = _actorAngleTurnToOffset(actor->extra.tmd->coords, scratchEnd[-1].delta.vx, backOff->delta.vz);
    work->lookYawTarget     = backOff->turn;
    if (ABS(backOff->turn) <= ALIGNMENT_LIMIT && work->animId == ACTOR_356100_ANIM_WALK) {
        work->animRate    = RETREAT_RATE;
        work->animId      = ACTOR_356100_ANIM_BACK_OFF;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        _actor356100UpdateAnimation(actor);
    }
    // The negative clamp bypasses halving; retain that asymmetric branch.
    if (backOff->turn > ALIGNMENT_LIMIT) {
        backOff->turn = ALIGNMENT_LIMIT;
    }
    if (backOff->turn < -ALIGNMENT_LIMIT) {
        backOff->turn = -ALIGNMENT_LIMIT;
    } else {
        backOff->turn = backOff->turn >> 1;
    }
    _actor356100ApplyRootTurn(actor, &backOff->turn);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ACTOR_356100_ANIM_BACK_OFF) {
        work->stateTimer++;
        saveData  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        stepCoord = actor->extra.tmd->coords;
        if (saveData->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
            _actorMovementStepLocalZFromSave(saveData, stepCoord, RETREAT_STEP);
        }
        pushCoord = actor->extra.tmd->coords;
        if (saveData->state.actorsFrozen != ACTOR_MOVEMENT_FROZEN) {
            _actorContactPushRootFromSave(saveData, pushCoord, work->pushContacts, ARRAY_SIZE(work->pushContacts), ACTOR_356100_CONTACT_HEIGHT_OFFSET);
        }
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((s16)work->stateTimer >= RETREAT_TICKS) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&actor->extra.tmd->coords->coord, EXIT_TURN, 0);
            } else {
                gfxRotMatrixY(&actor->extra.tmd->coords->coord, -EXIT_TURN, 0);
            }
            work->state = ACTOR_356100_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void _actor356100GrabWindup(Task* actor)
{
    enum { ROOT_TURN_STEP = 0x20,
           WINDUP_TICKS   = 11 };
    _Actor356100Work*  work;
    TmdObject*         model;
    Enemy*             enemy;
    ActorChaseScratch* windupTurn;
    int                nextState;

    work = actor->work;
    if (work->stateEntered != 0) {
        model                         = actor->extra.tmd;
        enemy                         = actor->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = ANIMATION_RATE_ONE;
        work->blendActive = 0;
        work->animId      = ACTOR_356100_ANIM_GRAB_WINDUP;
        _actor356100UpdateAnimation(actor);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer = (s16)((u16)work->stateTimer + 1);
    nextState        = ACTOR_356100_STATE_GRAB;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    windupTurn                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) || ((s16)work->stateTimer >= WINDUP_TICKS)) {
        work->state = nextState;
    }
    windupTurn->turn    = _actorAngleTurnToPlayer(actor, &windupTurn->delta, &gPlayerStatus);
    work->lookYawTarget = windupTurn->turn;
    _actor356100TurnRoot(actor, &windupTurn->turn, ROOT_TURN_STEP);
    _actor356100UpdateAnimation(actor);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn the actor's facing onto the player in 0x28 steps and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 2 at
/// speed 0x10 with the 0x13 state parked in `animId`, otherwise the aim
/// scratch takes the player offset, `_actorAngleTurnToPlayer` gives the wrapped
/// turn, `lookYawTarget` walks toward it by at most 0x28 and the state flips to 0xB
/// once it has caught up. Same body as `func_actor_401300_8013AE48`.
static void func_actor_356100_80168E44(Task* arg0)
{
    _Actor356100Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 0x13;
        _actor356100UpdateAnimation(arg0);
        _actor356100UpdateAnimation(arg0);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
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
    if (work->lookYawTarget == aim->turn) {
        work->state = ACTOR_356100_STATE_GRAB;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ACTOR_356100_ROOT_SCALE);
    work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
    _actor356100UpdateAnimation(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// The overlay's death-throes tick, the sibling of `func_actor_356100_80168E44`:
/// going live re-seeds the model (the enemy's link node, `obj->field_C`, the
/// animation request fields) and queues sound 0x550B0007 against the root
/// part, whose coordinate the live arm clears outright. Each frame then bumps
/// `stateTimer`, runs the clip and walks part 1's coordinate by a fixed 0x1044 /
/// 0x4AA per frame. Four frames each fire their own sound (0x550B0008 with the
/// 6/0xFF/0x80 pad rumble, 0x400D0002 with 8/0x7F/0x30, 0x400D0001 with
/// 6/0x7F/0x30, 0x550B0009 bare), and `stateTimer` 0x29..0x2D drives a 16-effect
/// 0x600FB burst over the model's part coordinates — 0x2E..0x31 the same burst
/// with six effects, alternating on the frame's parity.
///
/// The parity test re-reads `stateTimer` from memory rather than reusing the range
/// test's value (the two reads are what the original emits), so the read is
/// spelled volatile.
static void func_actor_356100_80169180(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            ctx;
    GfxCoord*         coord;

    work = arg0->work;
    if (work->stateEntered != 0) {
        s32 pan;

        ctx                    = arg0->spawnArg2.pointer;
        arg0->extra.tmd->flags = 0;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animId                = 1;
        work->animRequest           = ACTOR_356100_ANIM_REQUEST_RESET;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x800, 1);
        coord                                 = arg0->extra.tmd->coords;
        coord->coord.t[2]                     = 0;
        coord->coord.t[1]                     = 0;
        coord->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg0->extra.tmd->coords);
        work->stateTimer = 0;
        pan              = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_STRANGER_DEATH_START, pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
    }
    work->stateTimer = (s16)((u16)work->stateTimer + 1);
    _actor356100UpdateAnimation(arg0);
    arg0->extra.tmd->coords[1].coord.t[0]  += 0x1044;
    arg0->extra.tmd->coords[1].coord.t[2]  += 0x4AA;
    arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
    if (work->stateTimer == 0x31) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_STRANGER_DEATH_IMPACT, pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
        padScriptSpawnVariableMotorRamp(6, 0xFF, 0x80);
    }
    if (work->stateTimer == 0x4D) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 2), pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
        padScriptSpawnVariableMotorRamp(8, 0x7F, 0x30);
    }
    if (work->stateTimer == 0x58) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 1), pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
        padScriptSpawnVariableMotorRamp(6, 0x7F, 0x30);
    }
    if (work->stateTimer == 0xCE) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_STRANGER_DEATH_END, pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
    }
    if ((u32)((u16)work->stateTimer - 0x29) < 5U) {
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[3], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[1], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x12], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[2], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[3], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[4], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[1], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x13], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
        effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x12], 0, 0);
    }
    if ((u32)((u16)work->stateTimer - 0x2E) < 4U) {
        if (!(*(volatile u16*)&work->stateTimer & 1)) {
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[2], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[3], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[4], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        } else {
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[1], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x13], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
            effectSpawn(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x12], 0, 0);
        }
    }
}

/// The 31 state handlers `func_actor_356100_80169854` dispatches through, in
/// state order; entry 0x1D has no handler and the `state` values the ticks
/// park (0, 6, 7, 8, 9, 0xB, 0xC, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x19,
/// 0x1E) are its live entries. Same role as `Actor01900_D1728C`.
static const _Actor356100StateTable D_actor_356100_80161EC4 = {
    {
        func_actor_356100_8016A1D8,
        func_actor_356100_8016A21C,
        func_actor_356100_8016A2AC,
        func_actor_356100_8016A340,
        _actor356100StatusHold,
        func_actor_356100_8016A3D4,
        _actor356100Alert,
        _actor356100Chase,
        func_actor_356100_80164ACC,
        _actor356100TurnAround,
        _actor356100Sidestep,
        func_actor_356100_80166018,
        _actor356100GrabPull,
        func_actor_356100_8016A468,
        _actor356100GrabRelease,
        func_actor_356100_8016A550,
        func_actor_356100_8016A5DC,
        func_actor_356100_8016A668,
        _actor356100Approach,
        func_actor_356100_8016A710,
        func_actor_356100_8016A834,
        func_actor_356100_80167358,
        _actor356100Dormant,
        func_actor_356100_80167818,
        _actor356100Patrol,
        _actor356100BackOff,
        func_actor_356100_8016804C,
        _actor356100GrabWindup,
        func_actor_356100_80168E44,
        NULL,
        func_actor_356100_80169180,
    }
};

/// The enemy's three task-state handlers, which `func_actor_356100_8016A910`
/// runs by `Task::state`: setup, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_356100_80161F40 = {
    _actor356100Initialize,
    func_actor_356100_80169854,
    enemyDestroy,
};

static void func_actor_356100_80169854(Enemy* arg0, Task* arg1)
{
    VECTOR                   pos;
    _Actor356100StateTable   tbl;
    _Actor356100Work*        work;
    _Actor356100TickScratch* blk;
    s16                      next;

    work   = arg1->work;
    tbl    = D_actor_356100_80161EC4;
    pos.vx = arg1->extra.tmd->coords[1].workm.t[0];
    pos.vy = arg1->extra.tmd->coords[1].workm.t[1];
    pos.vz = arg1->extra.tmd->coords[1].workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_356100_STATE_HIDDEN && work->state != ACTOR_356100_STATE_DEATH_BURN && work->state != ACTOR_356100_STATE_SCRIPTED_DEATH) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_356100_STATE_HIDDEN && work->state != ACTOR_356100_STATE_DEATH_BURN && work->state != ACTOR_356100_STATE_SCRIPTED_DEATH) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(_Actor356100TickScratch);
    blk = SCRATCH_STACK_CURSOR(_Actor356100TickScratch);
    if (work->state == ACTOR_356100_STATE_SCRIPTED_DEATH) {
        blk->viewPos.vx = blk->viewPos.vy = blk->viewPos.vz = 0;
        _actorRenderTransformToWorld(&arg1->extra.tmd->coords[1], &blk->viewPos);
        gfxSetRotIdentity(&blk->shadowCoord.coord);
        blk->shadowCoord.parent       = &gGfxViewCoord;
        blk->shadowCoord.coord.t[0]   = blk->viewPos.vx;
        blk->shadowCoord.coord.t[1]   = 0;
        blk->shadowCoord.coord.t[2]   = blk->viewPos.vz;
        blk->shadowCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&blk->shadowCoord);
        effectDrawGroundShadow(MATRIX_TRANS(&blk->shadowCoord.workm), 0x280, gRoomEffectState->groundShadowShade);
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    tbl.handlers[work->state](arg1);
    if (gSceneCombatState.signals.bytes.enemyAlert == 1) {
        if (work->state == ACTOR_356100_STATE_PATROL) {
            work->state = ACTOR_356100_STATE_ALERT;
        }
    }
    blk->viewPos.vx = 0;
    blk->viewPos.vy = 0;
    blk->viewPos.vz = 0;
    _actorRenderTransformToWorld(&arg1->extra.tmd->coords[2], &blk->viewPos);
    work->bodyPosHistory[work->bodyPosCursor].vx = blk->viewPos.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = blk->viewPos.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = blk->viewPos.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor356100TickScratch);
    next                = (u16)work->bodyPosCursor + 1;
    work->bodyPosCursor = next;
    if (next == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if ((u32)((u16)work->animId - 0x14) < 2U) {
        arg0->bodyPos.vx = work->bodyPosHistory[work->bodyPosCursor].vx;
        arg0->bodyPos.vy = work->bodyPosHistory[work->bodyPosCursor].vy;
        arg0->bodyPos.vz = work->bodyPosHistory[work->bodyPosCursor].vz;
    } else {
        arg0->bodyPos.vx = blk->viewPos.vx;
        arg0->bodyPos.vy = blk->viewPos.vy;
        arg0->bodyPos.vz = blk->viewPos.vz;
    }
    arg0->coord = &gGfxViewCoord;
}

/// Declines the play-animation message without changing the actor.
///
/// All arguments are unused and the result is zero.
static s32 _actor356100IgnoreAnimationMessage(Task* task, s32 messageId, s32 unusedPayload, s32 unusedResponse)
{
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/actor_messages_release_hold.inc.c"

/// Applies Neo Ark forest-zone hide and scripted-death actor commands.
///
/// `command` is borrowed read-only for the dispatch. Every command first
/// caches its stage, area and low command byte. Forest commands 0 and 2 hide
/// the actor; command 1 selects scripted death and requests an animation reset.
/// Returns 1 for those commands, 0 for other contexts or codes. The task must
/// own a live work block; `messageId` and the second payload are unused.
static s32 _actor356100ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused)
{
    enum {
        ACTOR_356100_FOREST_COMMAND_HIDE     = 0,
        ACTOR_356100_FOREST_COMMAND_DEATH    = 1,
        ACTOR_356100_FOREST_COMMAND_HIDE_ALT = 2
    };
    _Actor356100Work* work = task->work;
    s32               commandCode;

    work->commandBytes[0] = command->context.loc.stage;
    work->commandBytes[1] = command->context.loc.area;
    work->commandBytes[2] = command->command;
    if (command->context.loc.stage == GAME_STAGE_SHELTER_NEO_ARK && command->context.loc.area == GAME_AREA_NEO_ARK_FOREST_ZONE) {
        commandCode = command->command;
        switch (commandCode) {
            case ACTOR_356100_FOREST_COMMAND_HIDE:
            case ACTOR_356100_FOREST_COMMAND_HIDE_ALT:
                work->state = ACTOR_356100_STATE_HIDDEN;
                return 1;
            case ACTOR_356100_FOREST_COMMAND_DEATH:
                work->state       = ACTOR_356100_STATE_SCRIPTED_DEATH;
                work->animId      = commandCode;
                work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}

/// Kills optional child tasks, withdraws enemy contacts and destroys the enemy.
///
/// The task's spawn payload must still hold its live enemy. A missing actor
/// work block skips child cleanup; `enemyDestroy` releases target locks and
/// the enemy allocation before killing the owning task.
static void _actor356100Exit(Task* task)
{
    _Actor356100Work* work;
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
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}

static void func_actor_356100_8016A1D8(Task* arg0)
{
    TmdObject*        obj;
    _Actor356100Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

static void func_actor_356100_8016A21C(Task* arg0)
{
    TmdObject*        obj;
    _Actor356100Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->blendActive = 0;
        work->animRate    = 0x10;
        work->animId      = 2;
        _actor356100UpdateAnimation(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor356100UpdateAnimation(arg0);
    }
}

static void func_actor_356100_8016A2AC(Task* arg0)
{
    TmdObject*        obj;
    _Actor356100Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 3;
        _actor356100UpdateAnimation(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor356100UpdateAnimation(arg0);
    }
}

static void func_actor_356100_8016A340(Task* arg0)
{
    TmdObject*        obj;
    _Actor356100Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 0xB;
        _actor356100UpdateAnimation(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor356100UpdateAnimation(arg0);
    }
}

static void func_actor_356100_8016A3D4(Task* arg0)
{
    TmdObject*        obj;
    _Actor356100Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 0xB;
        _actor356100UpdateAnimation(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor356100UpdateAnimation(arg0);
    }
}

static void func_actor_356100_8016A468(Task* arg0)
{
    _Actor356100Work*     work;
    Enemy*                enemy;
    AnimationPlayRequest* msg;
    Task*                 playerTask;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->animRate    = 0x10;
        work->animId      = 6;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        msg               = &D_actor_356100_80173244;
        msg->animationId  = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, msg, 0);
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0);
    }
    if (work->rig.slots[1].status.fields.flags & 2) {
        work->state = ACTOR_356100_STATE_GRAB_RELEASE;
    }
    work->grabAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    _actor356100UpdateAnimation(arg0);
}

static void func_actor_356100_8016A550(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animId                  = 8;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->baseRate;
    }
    _actor356100UpdateAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & 1) {
        work->state = ACTOR_356100_STATE_CHASE;
    }
}

static void func_actor_356100_8016A5DC(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animId                  = 0x16;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->baseRate;
    }
    _actor356100UpdateAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & 1) {
        work->state = ACTOR_356100_STATE_CHASE;
    }
}

static void func_actor_356100_8016A668(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateTimer = work->downFramesBase + ((gRandomLcgState >> 16) & 0xF);
    }
    if ((s16)--work->stateTimer < 0) {
        switch (work->animId) {
            case 0xB:
                work->state = ACTOR_356100_STATE_RISE_BACK;
                break;
            case 0xC:
                work->state = ACTOR_356100_STATE_RISE_FRONT;
                break;
        }
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_356100_STATE_DEATH_BURN;
    }
}

static void func_actor_356100_8016A710(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animId                  = 0xA;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
    }
    _actor356100UpdateAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & 1) {
        if (work->animId == 0xA) {
            work->animId      = 0xB;
            work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
            _actor356100UpdateAnimation(arg0);
        }
        if ((work->rig.slots[1].status.fields.flags & 1) && (work->animId == 0xB)) {
            if (enemy->hp > 0) {
                if (work->field_B3A <= 0) {
                    work->state = ACTOR_356100_STATE_DOWN;
                } else {
                    work->state = ACTOR_356100_STATE_STATUS_HOLD;
                }
            } else {
                work->state = ACTOR_356100_STATE_DEATH_BURN;
            }
        }
    }
}

static void func_actor_356100_8016A834(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animId                  = 0xC;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
    }
    _actor356100UpdateAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & 1) {
        if (enemy->hp > 0) {
            if (work->field_B3A <= 0) {
                work->state = ACTOR_356100_STATE_DOWN;
            } else {
                work->state = ACTOR_356100_STATE_STATUS_HOLD;
            }
        } else {
            work->state = ACTOR_356100_STATE_DEATH_BURN;
        }
    }
}

/// Runs the handler for the task's current state, copying the table onto the
/// stack before the call.
void func_actor_356100_8016A910(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_356100_80161F40;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
