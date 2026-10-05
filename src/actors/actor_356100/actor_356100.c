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
#include "gameplay/object_fields.h"
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
s32              func_actor_356100_80169E5C(Task*, s32, s32, s32);
s32              func_actor_356100_8016A0B8(Task*, s32, ActorCommand*, s32);
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
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_356100_80169E5C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { 2014, actorMsgReleaseHold },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_356100_8016A0B8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u16 D_actor_356100_80173290 = 0;

TaskDesc D_actor_356100_80173294 = { { { TASK_BODY_TMD, 96 } }, func_actor_356100_8016A910, { .model = &_gActor356100HornedStrangerBody } };

static SVECTOR ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_356100_801732A8 = { NULL, 0, 0 };

_Actor356100TransformStorage D_actor_356100_801732B0;

GameActorButtonPressHold D_actor_356100_801732D0;

/// Blends pose slots 1..0x14: the first eleven copy the two clip ids into
/// their slot records and are written from both animation contexts with
/// `0x1000 - blendWeight` as the blend weight, the rest only tick. Same body as
/// `Actor01900_Fn01950` / `func_actor_403000_801336B4` with this overlay's
/// slot count.
static void func_actor_356100_801633DC(Task* arg0);

/// Reseeds changed clips, ticks or blends their slots, and eases the two
/// upper-body coordinates toward the requested yaw.
static void func_actor_356100_80163508(Task* arg0);

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

/// Effect record `func_actor_356100_80167818` fills for `func_800FDB18`:
/// coordinate index 5 of the model, scale 0x100 and count 2. Same shape and
/// roles as `_Actor401300Work.effectArg`.
extern EffectSpawnArg D_actor_356100_801732A8;

/// Animation-set table bound to both work-block contexts by `animationInitContext`.
/// Same role as `Actor01900_D17174`.
extern u8 D_actor_356100_801730B8[];

/// Enemy descriptor `func_actor_356100_8016382C` publishes in the enemy's
/// `field_50` slot and takes `field_40` off. Same role as `Actor01900_D0AC54`.
extern EnemyParams D_actor_356100_8016A984;

/// The three rows `func_actor_356100_8016382C` picks its re-entry pair from on
/// the spawn sub-type. Same role as `Actor01900_D0AC64`.
extern ActorHornedStrangerVariant D_actor_356100_8016A994[];

extern TaskMessageEntry D_actor_356100_80173258[7];

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The spawn
/// sub-type picks the clip and re-entry pair, and the finished entry advances
/// the state.
static void func_actor_356100_8016382C(Enemy* enemy, Task* actor);

/// Separation tick: when the work block's `stateEntered` flag is set, pushes this
/// actor one normalised unit away from the player along the player-to-actor
/// direction in XZ (recentring it on the player first), clears the model's
/// root `composeStamp`, and sends the player message 0x3E9 with its own position and
/// the resulting heading. Bit 0 of `field_68` then forces `state` to 0xD.
static void func_actor_356100_801666B4(Task* arg0);

/// Approach tick, and the sibling of `func_actor_356100_80167584` above it. Going
/// live clears `D_actor_356100_80173170[16]` and re-seeds the animation slots at
/// clip 2 / speed 0x10 with the enemy's link node cleared; otherwise a single
/// sound 0x51030008 is queued the first time through, keyed on the enemy's
/// `field_8 >> 12` bank. Each frame then snapshots `field_5A & 0x3FF` into
/// `lastCueFrame`, and the frame that first lands on clip 4 spawns the
/// `D_actor_356100_801732A8` effect at model coordinate 5. Once the player is
/// further than 3000 away it plays 0x51030008 as a type-7 event and enters
/// state 6. Same shape as `func_actor_401300_801397F8`.
static void func_actor_356100_80167818(Task* arg0);

/// Actor-command handler: copies the command's stage tag, area tag and the low
/// byte of its command word into the work block's `commandBytes`, then applies
/// a command of the Neo Ark forest zone - 1 puts the actor in state 0x1E, 0
/// and 2 in state 0. Anything else returns 0.
s32 func_actor_356100_8016A0B8(Task* arg0, s32 arg1, ActorCommand* arg2, s32 arg3);

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `recs` slot, then `enemyDestroy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `worldCollisionUnlinkBody` calls.
static void func_actor_356100_8016A158(Task* task);

/// When the work block's `stateEntered` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
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

/// Per-frame tick run under the death-throes clip 0xB / 0xC pair.
static void func_actor_356100_80163CD4(Task* arg0);

/// Per-frame tick of the state-6 clip run.
static void func_actor_356100_80163E2C(Task* arg0);

/// Per-frame tick of the state-7 clip run.
static void func_actor_356100_80164158(Task* arg0);

/// Turn-aim tick of the state-8 clip run, the 356100 twin of
/// `Actor01900_Fn04D14`.
static void func_actor_356100_80164ACC(Task* arg0);

/// Turn tick that slews the root yaw 0x89 at a time onto `turnYawTarget`.
static void func_actor_356100_801653F4(Task* arg0);

/// Aim tick: normalises a root colour-matrix column and GPF-scales it by
/// `sidestepStep` into the chase scratch.
static void func_actor_356100_80165B30(Task* arg0);

/// Tick of the state-0xB aim run.
static void func_actor_356100_80166018(Task* arg0);

/// Tick that hands `func_800E0C10` the `pushContacts` collision record.
/// `actorMoveForwardNonzero` testing the freeze flag through a
/// `McSaveData*` rather than `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen`, and without its zero-amount guard.
/// Reads the X component back through `vec`, as `Actor01900_StepForward` does —
/// the `head[-1]` spelling gives the scratch release value a register of its
/// own and costs three instructions here. Same body as
/// `Actor401300_MoveForwardSave`.
static __inline__ void Actor356100_StepForwardSave(McSaveData* save, GfxCoord* coord, s16 amount)
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
        coord->coord.t[0]  += vec->vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// `Actor356100_PushRecords` testing the freeze flag through a `McSaveData*`,
/// and giving the block back through the scratch stack itself rather than a
/// saved `void**` — the saved pointer keeps the 0x1F8003FC constant live in a
/// register across the release.
static __inline__ void Actor356100_PushRecordsSave(McSaveData* save, GfxCoord* coord, WorldCollisionContact* rec, s32 count, s16 height)
{
    ActorContactPushScratch* head;
    ActorContactPushScratch* block;
    s32                      val;

    if (save->state.actorsFrozen != 1) {
        head = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
        SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
        block        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
        block->moved = 0;
        if (func_800E0C10(rec, &block->delta, count, NULL) != 0) {
            coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += block->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += block->delta.fixed.vz.halves.integer;
            val                = head[-1].delta.fixed.vx.word;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            val = block->delta.fixed.vz.word;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
        }
        coord->coord.t[1] += height;
        if (block->delta.fixed.vx.word != 0 || block->delta.fixed.vz.word != 0) {
            block->moved = 1;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    }
}

static void func_actor_356100_801668FC(Task* actor);

/// Tick that dispatches message 0x3F1 and clears the `playerHeld` latch.
static void func_actor_356100_8016A550(Task* arg0);

/// Tick that decrements `stateTimer` and reloads it from `downFramesBase` plus a
/// 4-bit `gRandomLcgState` draw.
static void func_actor_356100_8016A668(Task* arg0);

/// Tick that runs the `animRequest` clip and halves `animRate` once the actor
/// is no longer live.
static void func_actor_356100_80166CF0(Task* arg0);

/// Tick of the state-0x13 clip run.
static void func_actor_356100_8016A710(Task* arg0);

/// Tick that picks clip 4 or 0x11 off `field_B3A` once the enemy is still
/// alive.
static void func_actor_356100_8016A834(Task* arg0);

/// Per-frame tick of the state-0x15 clip run.
static void func_actor_356100_80167358(Task* arg0);

/// Per-frame tick of the state-0x16 clip run.
static void func_actor_356100_80167584(Task* arg0);

/// Per-frame tick of the state-0x18 clip run.
static void func_actor_356100_80167A7C(Task* arg0);

/// Per-frame tick of the state-0x19 clip run.
static void func_actor_356100_801684F0(Task* arg0);

/// Tick that pushes the actor off any collision record and turns it onto the
/// player.
static void func_actor_356100_8016804C(Task* arg0);

/// Turn tick that slews the root yaw onto the player 0x28 at a time.
static void func_actor_356100_80168AFC(Task* arg0);

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

static __inline__ void Actor356100_BindMatrices(Task* actor);
static __inline__ void Actor356100_PositionDelta(GfxCoord* coord, SVECTOR* pos);

static __inline__ void Actor356100_StepForward(GfxCoord* coord, s16 amount);
static __inline__ void Actor356100_PushRecords(GfxCoord* coord, WorldCollisionContact* rec, s32 count, s16 height);
static __inline__ s32  Actor356100_PushRecordsAlways(GfxCoord* coord, WorldCollisionContact* rec, s32 count, s16 height);
static __inline__ void Actor356100_MoveForward(GfxCoord* coord, s16 amount);
static __inline__ void Actor356100_StepForwardSave(McSaveData* save, GfxCoord* coord, s16 amount);
static __inline__ void Actor356100_PushRecordsSave(McSaveData* save, GfxCoord* coord, WorldCollisionContact* rec, s32 count, s16 height);

/// Binds the model's light and colour matrices to the pair kept in the work
/// block. Same body as `Actor01900_BindMatrices`.
static __inline__ void Actor356100_BindMatrices(Task* actor)
{
    _Actor356100Work* work;
    TmdObject*        obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

/// Player-to-`coord` vector, in the 16-bit `SVECTOR` view of both matrices.
static __inline__ void Actor356100_PositionDelta(GfxCoord* coord, SVECTOR* pos)
{
    pos->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    pos->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    pos->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
}

/// The overlay's only message-0x3E9 instance; all eight words are zero in the
/// image, so it is a work area rather than a table.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).

/// Event-handler table `func_actor_356100_8016382C` hands the task as
/// `Task::msgTable`. Same shape and role as `Actor01900_D1728C`.
// Message-table callbacks use the argument views required by this TU.

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

static void func_actor_356100_801633DC(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    _Actor356100Work* work;

    work   = arg0->work;
    weight = work->blendWeight;
    anim   = &work->rig.anim;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->blendRate;
            work->rig.slots[i].rate   = (work->animRate - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
            animationApplyPoseWithBlendedRotation(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

static void func_actor_356100_80163508(Task* arg0)
{
    _Actor356100Work* work;
    s16               yaw;

    work = arg0->work;
    if (work->animRequest == ACTOR_356100_ANIM_REQUEST_BLEND) {
        _Actor356100Work* anim;
        s32               i;

        anim = arg0->work;
        if (work->appliedAnim != work->animId) {
            for (i = 1; i < ARRAY_SIZE(anim->rig.slots); i++) {
                anim->rig.slots[i].rate = anim->animRate;
                animationSeekSlotWithBlend(&anim->rig.anim, i, anim->animId, 0,
                                           D_actor_356100_801728CC[anim->appliedAnim][anim->animId]);
            }
            anim->appliedAnim = anim->animId;
        }
        work->animRequest  = ACTOR_356100_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (work->animRequest == ACTOR_356100_ANIM_REQUEST_RESET) {
        _Actor356100Work* anim;
        s32               i;

        anim = arg0->work;
        for (i = 1; i < ARRAY_SIZE(anim->rig.slots); i++) {
            anim->rig.slots[i].rate = anim->animRate;
            animationResetSlot(&anim->rig.anim, i, anim->animId);
        }
        anim->appliedAnim  = anim->animId;
        work->animRequest  = ACTOR_356100_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_356100_ANIM_REQUEST_RESET) {
        _Actor356100Work* blend;
        s32               i;

        blend              = arg0->work;
        blend->blendRate   = 0x30;
        blend->blendWeight = 0x800;
        for (i = 1; i < ARRAY_SIZE(blend->blend.slots); i++) {
            blend->rig.slots[i].rate = blend->blendRate;
            animationResetSlot(&blend->blend.anim, i, blend->blendAnimId);
        }
        work->blendRequest = ACTOR_356100_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    if (work->blendActive == 0) {
        _Actor356100Work* tick;
        s32               i;

        tick = arg0->work;
        for (i = 1; i < ARRAY_SIZE(tick->rig.slots); i++) {
            tick->rig.slots[i].rate = tick->animRate;
            animationTickSlot(&tick->rig.anim, i);
        }
    } else {
        func_actor_356100_801633DC(arg0);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blendActive = 0;
        }
    }
    if (work->lookYawTarget > work->lookYaw) {
        if (work->lookYawTarget - work->lookYaw > 0x100) {
            work->lookYaw += 0x100;
        } else {
            work->lookYaw = work->lookYawTarget;
        }
    } else if (work->lookYaw - work->lookYawTarget > 0x100) {
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
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[5], (yaw * 2) / 3);
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[2], yaw / 2);
        arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the model's light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The sub-type in
/// the high half of `Task::spawnArg1` picks the clip and the spawn argument the re-entry pair, and the
/// finished entry advances the state. Same body as `Actor01900_Fn02018`.
static void func_actor_356100_8016382C(Enemy* enemy, Task* actor)
{
    SVECTOR           dir;
    SVECTOR*          v;
    VECTOR            pos;
    TmdObject*        obj;
    GfxCoord*         root;
    _Actor356100Work* work;
    s32               kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(_Actor356100Work), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->exitCallback = func_actor_356100_8016A158;
    Actor356100_BindMatrices(actor);
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
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_356100_801730B8, obj, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)D_actor_356100_801730B8, obj, work->blend.poses, work->blend.slots);
    work->animRequest   = ACTOR_356100_ANIM_REQUEST_RESET;
    work->animId        = 1;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = 0x10;
    work->animRate      = 0x10;
    func_actor_356100_80163508(actor);
    work->patrolTarget      = 0;
    work->patrolPoints[0].x = actor->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->patrolPoints[1].x = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->patrolPoints[1].z = actor->extra.tmd->coords->coord.t[2] + dir.vz;
    actor->msgTable         = D_actor_356100_80173258;
    root->parent            = &gGfxViewCoord;
    root->composeStamp      = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_356100_801732A8.coord      = actor->extra.tmd->coords;
    D_actor_356100_801732A8.spawnArgLo = 0x100;
    D_actor_356100_801732A8.spawnArgHi = 2;
    kind                               = actor->spawnArg1.value >> 16;
    switch (kind & 0xF) {
        case 2:
            work->prevState = -1;
            work->state     = ACTOR_356100_STATE_HIDDEN;
            break;
        case 4:
            work->prevState = -1;
            work->state     = ACTOR_356100_STATE_DORMANT;
            break;
        default:
            work->prevState = -1;
            work->state     = ACTOR_356100_STATE_PATROL;
            tmdAllocPrimitiveBuffer(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
        case 2:
            work->downFramesBase = D_actor_356100_8016A994[0].downFramesBase;
            work->sidestepAngle  = D_actor_356100_8016A994[0].sidestepAngle;
            break;
        case 1:
            work->downFramesBase = D_actor_356100_8016A994[2].downFramesBase;
            work->sidestepAngle  = D_actor_356100_8016A994[2].sidestepAngle;
            break;
        case 0:
        default:
            // The second tuning, but with the third's sidestep angle.
            work->downFramesBase = D_actor_356100_8016A994[1].downFramesBase;
            work->sidestepAngle  = D_actor_356100_8016A994[2].sidestepAngle;
            break;
    }
    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->bodyPosCursor = 0;
    actor->state++;
}

/// Runs the clip the work block's `animRequest` halfword selects and holds this
/// state until it ends: while the actor is live, reset the model (`node.state.parts.flags`
/// / `obj->field_C`, `tmdAllocPrimitiveBuffer`), start clip 2 at speed 0x10, and tick
/// until clip 0xB has reached frame 6 or clip 0xC frame 9, then park `animRate`
/// at 0x20. Once the actor is no longer live the same slot is halved per frame as
/// a scale ramp that bounces between 0x10 and -0x10 — ending the state with
/// `state = 0x11` when `Gp_TickObjFlag2` reports the flag has expired.
static void func_actor_356100_80163CD4(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s16               animA;
    s16               animB;
    s32               value;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        animA                       = 0xB;
        animB                       = 0xC;
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        work->animRate    = 0x10;
        do {
            func_actor_356100_80163508(arg0);
        } while (((work->animId != animA) || ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) < 6U)) &&
                 ((work->animId != animB) || ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) < 9U)));
        work->animRate = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    value                                 = (s16)work->animRate / 2;
    work->animRate                        = (u16)value;
    if (value == 1) {
        work->animRate = -0x10U;
    }
    if ((s16)work->animRate == -1) {
        work->animRate = 0x10;
    }
    func_actor_356100_80163508(arg0);
    if (Gp_TickObjFlag2(ctx) == 1) {
        ctx->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state         = ACTOR_356100_STATE_DOWN;
    }
}

/// Turns the actor's facing onto the player in one step and rescales the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 9 state parked in `animId`, otherwise the chase scratch
/// takes the player offset, `actorPositionYaw` gives the wrapped turn,
/// `lookYawTarget` snapshots it, it is clamped to [-0x10, 0x10] and the root yaw is
/// re-derived from it. Same body as `func_actor_401300_8013AAE8`.
static void func_actor_356100_80163E2C(Task* arg0)
{
    _Actor356100Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 9;
        func_actor_356100_80163508(arg0);
        work->hitRadius = 0x180;
        Gp_ArmStateF0(1);
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 1) {
        work->state = ACTOR_356100_STATE_CHASE;
    }
    aim->turn           = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn >= 0x11) {
        aim->turn = 0x10;
    }
    if (aim->turn < -0x10) {
        aim->turn = -0x10;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    func_actor_356100_80163508(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Steps `coord` `amount` units along its own root colour-matrix column unless
/// movement is frozen, normalising the column with the GTE first and giving the
/// 8-byte scratch stack block back afterwards. The guardless sibling of
/// `actorMoveForwardNonzero`, reading the X component back through
/// `vec`; same body as `Actor01900_StepForward` / `actorMoveForward`.
static __inline__ void Actor356100_StepForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
        vec                           = head - 1;
        gfxReadMatrixZAxis(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]  += vec->vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Pushes `coord` out of the `WorldCollisionContact` records `rec` by `func_800E0C10`'s
/// averaged 16.16 delta, then lifts it by `height`. `head` is the scratch
/// cursor read before the `ActorContactPushScratch` block is reserved, the
/// block's end, so the body reaches the block two ways as the original does:
/// back from `head` for the X correction, through `block` for the rest. Same
/// body as `Actor01900_Fn00E00`'s push without its mask argument.
static __inline__ void Actor356100_PushRecords(GfxCoord* coord, WorldCollisionContact* rec, s32 count, s16 height)
{
    void**                   scratch;
    ActorContactPushScratch* head;
    ActorContactPushScratch* block;
    s32                      val;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        scratch = SCRATCH_HEAD_ADDR;
        head    = SCRATCH_HEAD_AT(scratch, ActorContactPushScratch);
        SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
        block        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
        block->moved = 0;
        if (func_800E0C10(rec, &block->delta, count, NULL) != 0) {
            coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
            coord->coord.t[1] += block->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += block->delta.fixed.vz.halves.integer;
            val                = head[-1].delta.fixed.vx.word;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[0]++;
                } else {
                    coord->coord.t[0]--;
                }
            }
            val = block->delta.fixed.vz.word;
            if ((val & 0xFFFF) != 0) {
                if (val > 0) {
                    coord->coord.t[2]++;
                } else {
                    coord->coord.t[2]--;
                }
            }
        }
        coord->coord.t[1] += height;
        if (block->delta.fixed.vx.word != 0 || block->delta.fixed.vz.word != 0) {
            block->moved = 1;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    }
}

/// `Actor356100_PushRecords` without the freeze guard, returning the block's
/// `moved` after the scratch is given back. The caller names `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` first so the
/// compare interleaves with the coordinate load.
static __inline__ s32 Actor356100_PushRecordsAlways(GfxCoord* coord, WorldCollisionContact* rec, s32 count, s16 height)
{
    void**                   scratch;
    ActorContactPushScratch* head;
    ActorContactPushScratch* block;
    s32                      val;

    scratch = SCRATCH_HEAD_ADDR;
    head    = SCRATCH_HEAD_AT(scratch, ActorContactPushScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
    block        = SCRATCH_STACK_CURSOR(ActorContactPushScratch);
    block->moved = 0;
    if (func_800E0C10(rec, &block->delta, count, NULL) != 0) {
        coord->coord.t[0] += head[-1].delta.fixed.vx.halves.integer;
        coord->coord.t[1] += block->delta.fixed.vy.halves.integer;
        coord->coord.t[2] += block->delta.fixed.vz.halves.integer;
        val                = head[-1].delta.fixed.vx.word;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        val = block->delta.fixed.vz.word;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    coord->coord.t[1] += height;
    if (block->delta.fixed.vx.word != 0 || block->delta.fixed.vz.word != 0) {
        block->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    return block->moved;
}

static void func_actor_356100_80164158(Task* arg0)
{
    _Actor356100Work*  work;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    s16                yaw;
    s32                diff;
    s32                range;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 0x12;
        work->blendActive = 0;
        work->animId      = 3;
        func_actor_356100_80163508(arg0);
        work->circleCount  = 0;
        work->stateTimer   = 0;
        work->stateCounter = 0;
        return;
    }
    work->stateTimer = (u16)work->stateTimer + 1;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    Actor356100_PushRecords(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_356100_80163508(arg0);
    aim->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                            (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    yaw                 = ratan2(aim->delta.vx, aim->delta.vz) + 0x800;
    aim->yawFromPlayer  = yaw;
    aim->yawFromPlayer  = actorNormalizeYaw(yaw);
    aim->turn           = actorYawTo(arg0->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
    work->lookYawTarget = aim->turn;
    diff                = aim->yawFromPlayer - aim->playerYaw;
    if (ABS(diff) < 0x44 && (((s16)work->sidestepCount / 2) + 3) < work->stateTimer && ABS(aim->turn) < 0x80) {
        if (overlayOutOfRange(&aim->delta, 0x708)) {
            work->state = ACTOR_356100_STATE_SIDESTEP;
        }
    }
    range = actorNormalizeYaw((u16)aim->yawFromPlayer - (u16)aim->playerYaw);
    if (ABS(range) >= 0x201 && (((s16)work->sidestepCount / 2) + 3) < work->stateTimer && work->stateCounter == 0) {
        work->stateCounter = 1;
        work->animId       = 9;
        work->animRequest  = ACTOR_356100_ANIM_REQUEST_BLEND;
    }
    if (aim->turn < 0x200) {
        if (!overlayOutOfRange(&aim->delta, 0x44C)) {
            work->state = ACTOR_356100_STATE_GRAB;
        }
    }
    if (aim->turn > 0x40) {
        aim->turn = 0x40;
    }
    if (aim->turn < -0x40) {
        aim->turn = -0x40;
    }
    aim->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 3) {
        if (work->blendActive == 0) {
            Actor356100_StepForward(arg0->extra.tmd->coords, 0x78);
        } else {
            Actor356100_StepForward(arg0->extra.tmd->coords, 0x1E);
        }
    } else if (work->rig.slots[1].status.fields.flags & 1) {
        work->animId      = 3;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn-aim state body, the 356100 twin of `Actor01900_Fn04D14`: take a 0x10
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
        func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
    paused    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
    pushCoord = arg0->extra.tmd->coords;
    records   = work->pushContacts;
    if (paused == 1) {
        hit = 0;
    } else {
        hit = Actor356100_PushRecordsAlways(pushCoord, records, ARRAY_SIZE(work->pushContacts), 0x10);
    }
    if (hit != 0) {
        work->stateCounter++;
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->stateCounter >= 7) {
        chase->playerYaw     = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->yawFromPlayer = actorNormalizeYaw(chase->yawFromPlayer);
        work->state          = ACTOR_356100_STATE_SLIDE;
    }
    coord       = arg0->extra.tmd->coords;
    chase->turn = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    coord                                 = arg0->extra.tmd->coords;
    work->lookYawTarget                   = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->runStep                         = work->animRate * 8;
    if (work->blendActive != 0) {
        work->runStep = work->runStep >> 1;
    }
    if (work->stateCounter != 0) {
        work->runStep = 2;
    }
    actorMoveForwardNonzero(arg0->extra.tmd->coords, work->runStep);
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
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
            chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
            yaw                  = actorNormalizeYaw(chase->yawFromPlayer);
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

/// Aim tick: going live resets the model and starts clip 1 at speed 0x10 with
/// the 3 state parked in `animId` and `lookYawTarget` cleared; otherwise the
/// chase scratch takes the player offset, and the wrapped turn from it is paired
/// with the root's own facing yaw — snapshotted into `turnYaw` and, plus
/// twice the turn, into the `turnYawTarget` the yaw is then slewed toward. Same
/// body as `Actor01900_Fn0551C`, whose aim tick this is the live-arm half of:
/// the settling yaw is re-derived from the player each entry while that one
/// only re-seeds the pair.
static void func_actor_356100_801653F4(Task* arg0)
{
    _Actor356100Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          cur;
    GfxCoord*          facing;
    ActorChaseScratch* head;
    ActorChaseScratch* chase;
    s32                value;

    work = arg0->work;
    if (work->stateEntered != 0) {
        head                                                      = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        obj                                                       = arg0->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = head - 1;
        chase                                                     = head - 1;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius     = 0x180;
        work->animRequest   = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate      = 0x10;
        work->blendActive   = 0;
        work->animId        = 3;
        work->lookYawTarget = 0;
        func_actor_356100_80163508(arg0);
        cur                 = arg0->extra.tmd->coords;
        head[-1].delta.vx   = gPlayerStatus.coordMtx->t[0] - cur->coord.t[0];
        chase->delta.vy     = gPlayerStatus.coordMtx->t[1] - cur->coord.t[1];
        chase->delta.vz     = gPlayerStatus.coordMtx->t[2] - cur->coord.t[2];
        coord               = arg0->extra.tmd->coords;
        chase->turn         = actorYawTo(coord, head[-1].delta.vx, chase->delta.vz);
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
    func_actor_356100_80163508(arg0);
    cur               = arg0->extra.tmd->coords;
    head[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - cur->coord.t[0];
    chase->delta.vy   = gPlayerStatus.coordMtx->t[1] - cur->coord.t[1];
    chase->delta.vz   = gPlayerStatus.coordMtx->t[2] - cur->coord.t[2];
    if (work->turnYaw == work->turnYawTarget) {
        if (work->circleCount < 2 || overlayOutOfRange(&chase->delta, 0x384)) {
            value = ACTOR_356100_STATE_CIRCLE;
        } else {
            value = ACTOR_356100_STATE_GRAB;
        }
        work->state = value;
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    Actor356100_PushRecords(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn-and-close tick: going live writes the animation request fields with
/// `hitRadius` forced to 0xC0 and the enemy's link node cleared, takes the
/// player offset into the chase scratch and turns the root onto it with
/// `ratan2`, then settles `sidestepSide` on the 12-bit side the `gRandomLcgState`
/// draw picks and leans the yaw by `sidestepAngle` either way, before rebuilding
/// its Y rotation at the fixed 0xDE GPF scale and bumping `sidestepCount`. Each
/// frame then re-runs the animation and, while the clip sits in 0xC..0x15,
/// takes the aim and pushes the root out of the `pushContacts` collision records
/// by 0x10. Past clip 0x1E the state moves to 7. Same body as
/// `Actor01900_Fn05B4C`, whose `head[-1]` / `aim` spelling of the 0x10-byte
/// scratch block this matches.
static void func_actor_356100_80165B30(Task* arg0)
{
    _Actor356100Work*  work;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;

    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    work                                    = arg0->work;
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius  = 0xC0;
        work->stateTimer = 0;
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
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 0xC;
        work->blendActive = 0;
        func_actor_356100_80163508(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->sidestepDir;
        gfxReadMatrixZAxis(&mat, dir);
        VectorNormalSS(dir, dir);
        work->sidestepStep = 0xDE;
        work->sidestepCount++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_356100_80163508(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&aim->delta);
    } else {
        gte_lddp((s16)work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&aim->delta);
    }
    if ((u32)((u16)work->stateTimer - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        Actor356100_PushRecords(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    }
    if (++work->stateTimer >= 0x1E) {
        work->state = ACTOR_356100_STATE_CHASE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn-and-close tick, and the sibling of `func_actor_356100_801666B4` above
/// it. Going live writes the animation request fields with `hitRadius`
/// forced to 0x180 and the enemy's link node cleared, then turns the root
/// coordinate onto the player through `actorPositionYaw` and rebuilds
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
        func_actor_356100_80163508(arg0);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, actorPositionYaw(arg0, &pos, config), 0);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        pos.vx                                = arg0->extra.tmd->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                                = 0;
        pos.vz                                = arg0->extra.tmd->coords->coord.t[2] - config->coordMtx->t[2];
        work->lookYawTarget                   = 0;
        work->lookYaw                         = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->sidestepCount                   = 0;
        work->playerHeld                      = 0;
    }
    func_actor_356100_80163508(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x10 && player->mode != GAME_ACTOR_MODE_SCRIPTED) {
        angle = actorMatrixPositionYaw(arg0, &pos, gPlayerStatus.coordMtx);
        if (abs(angle) < 0x10 && !overlayOutOfRange(&pos, 0x44C)) {
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
        if (!overlayOutOfRange(p, 0x578)) {
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

static void func_actor_356100_801666B4(Task* arg0)
{
    _Actor356100Work* work;
    Enemy*            enemy;
    Task*             player;
    SVECTOR*          vecp;
    SVECTOR           vec;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        player                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->hitRadius                         = 0x180;
        enemy->node.state.parts.flags           = 0;
        work->animRequest                       = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate                          = 0x10;
        work->animId                            = 5;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(player->extra.tmd->coords);
        D_actor_356100_801732B0.placement.pos.vx = player->extra.tmd->coords->coord.t[0];
        D_actor_356100_801732B0.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
        D_actor_356100_801732B0.placement.pos.vz = player->extra.tmd->coords->coord.t[2];
        vecp                                     = &vec;
        /* Order matters: the vy store must follow the vx loads in RTL, or
           sched1 fills its anti-dependency chain from the earlier stores and
           hoists it above the D.z store. */
        vec.vx = (u16)arg0->extra.tmd->coords->coord.t[0] - (u16)player->extra.tmd->coords->coord.t[0];
        vec.vy = 0;
        vec.vz = (u16)arg0->extra.tmd->coords->coord.t[2] - (u16)player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(vecp, vecp);
        gte_lddp(0x3E8);
        gte_ldsv(vecp);
        gte_gpf12();
        gte_stsv(vecp);
        arg0->extra.tmd->coords->coord.t[0]      = player->extra.tmd->coords->coord.t[0] + vec.vx;
        arg0->extra.tmd->coords->coord.t[2]      = player->extra.tmd->coords->coord.t[2] + vec.vz;
        arg0->extra.tmd->coords->composeStamp    = GRAPHICS_COORD_DIRTY;
        D_actor_356100_801732B0.placement.rot.vx = 0;
        D_actor_356100_801732B0.placement.rot.vy = ratan2(vec.vx, vec.vz);
        D_actor_356100_801732B0.placement.rot.vz = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_356100_801732B0.placement, 0);
    }
    func_actor_356100_80163508(arg0);
    if (work->animId == 5 && (work->rig.slots[1].status.fields.flags & 1)) {
        work->state = ACTOR_356100_STATE_GRAB_STRIKE;
    }
}

static void func_actor_356100_801668FC(Task* actor)
{
    _Actor356100Work* work;
    Enemy*            enemy;
    PlayerStatus*     playerStatus;
    McSaveData*       saveData;
    GfxCoord*         coord;
    GfxCoord*         root;

    work         = actor->work;
    enemy        = actor->spawnArg2.pointer;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->animRate    = 0x10;
        work->animId      = 7;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
        func_actor_356100_80163508(actor);
        D_actor_356100_80173244.animationId = 3;
        if (playerStatus->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &D_actor_356100_80173244, 0);
        }
        work->stateTimer = 0;
    } else if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 &&
               playerStatus->hp > 0 && work->playerHeld == 1) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        work->playerHeld = 0;
    }
    if ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 0x10 < 7U) {
        saveData = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        coord    = actor->extra.tmd->coords;
        if (saveData->state.actorsFrozen != 1) {
            Actor356100_StepForwardSave(saveData, coord, -0x78);
        }
        root = actor->extra.tmd->coords;
        if (saveData->state.actorsFrozen != 1) {
            Actor356100_PushRecordsSave(saveData, root, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
        }
        actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    func_actor_356100_80163508(actor);
    if (work->rig.slots[1].status.fields.flags & 1) {
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

/// Release tick, the sibling of `func_actor_356100_801684F0` below it and the
/// same body as `Actor01900_Fn06100`. Going live resets the model and starts
/// clip 3 at speed 8 with `blendActive` cleared and `circleCount` zeroed;
/// otherwise the chase scratch takes the player offset, the root is pushed out of
/// the `pushContacts` collision records and `actorYawTo` gives the wrapped
/// turn, which `lookYawTarget` snapshots. A turn under 0x200 while the player is
/// still within 0x384 moves the state to 0xB; the turn is then clamped to
/// [-0x40, 0x40], the root yaw is re-derived from it and the root rescaled to a
/// uniform 0x1194 before being stepped 0x78 along its own column, or 0x3C when
/// `blendActive` is set.
static void func_actor_356100_80166CF0(Task* arg0)
{
    _Actor356100Work*  work;
    TmdObject*         obj;
    ActorChaseScratch* aim;
    s16                ang;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 8;
        work->blendActive = 0;
        work->animId      = 3;
        func_actor_356100_80163508(arg0);
        work->circleCount = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    Actor356100_PushRecords(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_356100_80163508(arg0);
    ang                 = actorYawTo(arg0->extra.tmd->coords, aim->delta.vx, aim->delta.vz);
    aim->turn           = ang;
    work->lookYawTarget = ang;
    if (aim->turn < 0x200) {
        if (!overlayOutOfRange(&aim->delta, 0x384)) {
            work->state = ACTOR_356100_STATE_GRAB;
        }
    }
    if (aim->turn > 0x40) {
        aim->turn = 0x40;
    }
    if (aim->turn < -0x40) {
        aim->turn = -0x40;
    }
    aim->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x78);
    } else {
        Actor356100_StepForward(arg0->extra.tmd->coords, 0x3C);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Rotation-collapse tick: going live clears the model's `field_C`, flags the
/// enemy's link node and re-seeds `stateTimer`. Each frame then bumps `stateTimer`
/// and fires its milestone — 0x18 releases state F0 (arg 0xA), 0x1D switches
/// light mode 1 and spawns effect 0x600A5 at model coordinate 2, 0x29 sets
/// `field_C` to 2, 0x2F switches light mode 2 and 0x33 sets `field_C` to 0x80.
/// From 0x1A on, the root rotation is rebuilt in the 0x34-byte scratch block
/// as a uniform 0x1194 scale whose Y shrinks by 0xB per frame past 0x14, and
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
                Gp_ReleaseStateF0Add(arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 17:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 39:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        cur = work->stateTimer;
        if (cur >= 0x1A) {
            actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
        }
    }
}

/// Range tick, and the sibling of `func_actor_356100_80167818` below it. Going
/// live clears the model's `field_C`, reallocates its buffers, clears the
/// enemy's link node, saves the `colorMtx` colour matrix into `savedColorMtx` and
/// starts clip 0xE at speed 1 with `hitRadius` forced to 0x180. Each frame then
/// bumps `stateTimer` until it passes 0x960, after which a 4-bit `gRandomLcgState`
/// draw thins the tick to one frame in 16. A tick that runs drops to state 6
/// while the player is still within 3000 of the actor, then flips the clip
/// between 0xE and 0xF on a 50/50 draw gated by bits 2 and 1 of `field_68`.
/// Same shape as `func_actor_401300_80139520`.
static void func_actor_356100_80167584(Task* arg0)
{
    _Actor356100Work* work;
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
        work->hitRadius               = 0x180;
        enemy->node.state.parts.flags = 0;
        work->stateTimer              = 0;
        work->savedColorMtx           = work->colorMtx;
        work->animId                  = 0xE;
        work->animRequest             = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate                = work->baseRate;
    }
    if (work->stateTimer > 0x960) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->stateTimer = (s16)((u16)work->stateTimer + 1);
    }
    coord = arg0->extra.tmd->coords;
    d     = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!overlayOutOfRange(d, 3000)) {
        work->state = ACTOR_356100_STATE_ALERT;
    }
    func_actor_356100_80163508(arg0);
    if (work->animId == 0xE && (work->rig.slots[1].status.fields.flags & 2)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            work->animId      = 0xF;
            work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
            func_actor_356100_80163508(arg0);
        }
    }
    if (work->animId == 0xF && (work->rig.slots[1].status.fields.flags & 1)) {
        work->animId      = 0xE;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 4 && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
        D_actor_356100_801732A8.coord      = arg0->extra.tmd->coords;
        D_actor_356100_801732A8.spawnArgLo = 0x100;
        D_actor_356100_801732A8.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL,
                      &D_actor_356100_801732A8);
    }
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord              = arg0->extra.tmd->coords;
    d                  = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!overlayOutOfRange(d, 3000)) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        work->state = ACTOR_356100_STATE_ALERT;
    }
}

static __inline__ void Actor356100_MoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR) - 1;
        vec                           = head;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        if (amount != 0) {
            gfxReadMatrixZAxis(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(vec);
            gte_gpf12();
            gte_stsv(vec);
            coord->coord.t[0]  += head->vx;
            coord->coord.t[1]  += vec->vy;
            coord->coord.t[2]  += vec->vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

static void func_actor_356100_80167A7C(Task* arg0)
{
    _Actor356100Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    s16               angle;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 2;
        func_actor_356100_80163508(arg0);
        return;
    }
    head              = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    head[-1].delta.vx = work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0];
    turn              = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn->delta.vy    = 0;
    turn->delta.vz    = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
    if (!overlayOutOfRange(&turn->delta, 0xA0)) {
        if (work->patrolTarget == 0) {
            work->patrolTarget = 1;
        } else {
            work->patrolTarget = 0;
        }
    }
    func_actor_356100_80163508(arg0);
    coord               = arg0->extra.tmd->coords;
    angle               = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle         = actorNormalizeYaw(angle);
    work->lookYawTarget = turn->angle;
    if (turn->angle >= 0x21) {
        turn->angle = 0x20;
    }
    if (turn->angle < -0x20) {
        turn->angle = -0x20;
    }
    turn->angle = (u16)turn->angle + ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    if (work->blendActive == 0) {
        Actor356100_MoveForward(arg0->extra.tmd->coords, 10);
    }
    Actor356100_PushRecords(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turn-and-push tick, the sibling of `func_actor_356100_80163E2C` above it and
/// the same body as `func_actor_401300_8013A208`. The live branch resets the
/// model and starts clip 1 at speed 0x10 with the 0x12 state parked in
/// `animId`; otherwise the turn scratch takes the player offset,
/// `actorPositionYaw` gives the wrapped turn, `lookYawTarget` snapshots it,
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
    turn->angle         = actorPositionYaw(arg0, &turn->delta, &gPlayerStatus);
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
    Actor356100_PushRecords(arg0->extra.tmd->coords, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
    actorMoveForwardNonzero(arg0->extra.tmd->coords, work->runStep);
    if (work->runStep > 0) {
        next          = work->runStep - 0xA;
        work->runStep = next;
        if ((s16)next < 0) {
            work->runStep = 0;
        }
    }
    func_actor_356100_80163508(arg0);
    if ((work->rig.slots[1].status.fields.flags & 1) || work->runStep == 0) {
        work->state = ACTOR_356100_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Turn-and-rescale tick, the sibling of `func_actor_356100_8016804C` above it
/// and the same body as `func_actor_401300_8013A5C0`. Going live resets the
/// model and starts clip 1 at speed 0x10 with the 0x13 state parked in
/// `animId`; otherwise the chase scratch takes the player offset,
/// `actorYawTo` gives the wrapped turn, `lookYawTarget` snapshots it, it is
/// clamped to [-0x80, 0x80] and halved, the root yaw is re-derived from it and
/// the root coordinate rescaled to a uniform 0x1194. Once the state has settled
/// on 0x11 the collision step pushes the root out of the `pushContacts` records
/// and one normalised unit back along its own Y column, both frozen while the
/// save flag is set, and past clip 0x13 the actor is leaned by ±0x4B0 into
/// state 7.
static void func_actor_356100_801684F0(Task* arg0)
{
    _Actor356100Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          cur;
    GfxCoord*          root;
    void**             scratch;
    ActorChaseScratch* head;
    ActorChaseScratch* aim;
    McSaveData*        save;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 0x16;
        work->blendActive = 0;
        work->animId      = 2;
        func_actor_356100_80163508(arg0);
        return;
    }
    func_actor_356100_80163508(arg0);
    scratch             = SCRATCH_HEAD_ADDR;
    cur                 = arg0->extra.tmd->coords;
    head                = SCRATCH_HEAD_AT(scratch, ActorChaseScratch);
    head[-1].delta.vx   = gPlayerStatus.coordMtx->t[0] - cur->coord.t[0];
    aim                 = (SCRATCH_HEAD_AT(scratch, ActorChaseScratch) = head - 1);
    aim->delta.vy       = gPlayerStatus.coordMtx->t[1] - cur->coord.t[1];
    aim->delta.vz       = gPlayerStatus.coordMtx->t[2] - cur->coord.t[2];
    aim->turn           = actorYawTo(arg0->extra.tmd->coords, head[-1].delta.vx, aim->delta.vz);
    work->lookYawTarget = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->animId == 2) {
        work->animRate    = 0x16;
        work->animId      = 0x11;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        func_actor_356100_80163508(arg0);
    }
    if (aim->turn > 0x80) {
        aim->turn = 0x80;
    }
    if (aim->turn < -0x80) {
        aim->turn = -0x80;
    } else {
        aim->turn = aim->turn >> 1;
    }
    aim->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 0x11) {
        work->stateTimer++;
        save  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        coord = arg0->extra.tmd->coords;
        if (save->state.actorsFrozen != 1) {
            Actor356100_StepForwardSave(save, coord, -0x10);
        }
        root = arg0->extra.tmd->coords;
        if (save->state.actorsFrozen != 1) {
            Actor356100_PushRecordsSave(save, root, work->pushContacts, ARRAY_SIZE(work->pushContacts), 0x10);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((s16)work->stateTimer >= 0x13) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->state = ACTOR_356100_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn the actor's facing onto the player in one step and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 0x13 state parked in `animId`, otherwise `stateTimer`
/// ticks over for the 0xB-frame transition, the chase scratch takes the player
/// offset, `actorPositionYaw` gives the wrapped turn, `lookYawTarget`
/// snapshots it, the turn is clamped to [-0x20, 0x20] and the root yaw is
/// re-derived from it before the work block's `state` takes the local `state` once the count-down
/// expires. Same body as `func_actor_401300_8013AAE8`.
static void func_actor_356100_80168AFC(Task* arg0)
{
    _Actor356100Work*  work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;
    int                state;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitRadius   = 0x180;
        work->animRequest = ACTOR_356100_ANIM_REQUEST_BLEND;
        work->animRate    = 0x10;
        work->blendActive = 0;
        work->animId      = 0x13;
        func_actor_356100_80163508(arg0);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer = (s16)((u16)work->stateTimer + 1);
    // One constant is both the eleven-frame limit and `ACTOR_356100_STATE_GRAB`.
    state = 0xB;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[1].status.fields.flags & 1) || ((s16)work->stateTimer >= state)) {
        work->state = state;
    }
    aim->turn           = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn >= 0x21) {
        aim->turn = 0x20;
    }
    if (aim->turn < -0x20) {
        aim->turn = -0x20;
    }
    coord     = arg0->extra.tmd->coords;
    aim->turn = (u16)aim->turn + ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    func_actor_356100_80163508(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn the actor's facing onto the player in 0x28 steps and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 2 at
/// speed 0x10 with the 0x13 state parked in `animId`, otherwise the aim
/// scratch takes the player offset, `actorPositionYaw` gives the wrapped
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
        func_actor_356100_80163508(arg0);
        func_actor_356100_80163508(arg0);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
    func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
    arg0->extra.tmd->coords[1].coord.t[0]  += 0x1044;
    arg0->extra.tmd->coords[1].coord.t[2]  += 0x4AA;
    arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[1]);
    if (work->stateTimer == 0x31) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_STRANGER_DEATH_IMPACT, pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
        Gp_SpawnPadLerp(6, 0xFF, 0x80);
    }
    if (work->stateTimer == 0x4D) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 2), pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
        Gp_SpawnPadLerp(8, 0x7F, 0x30);
    }
    if (work->stateTimer == 0x58) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_356100, 1), pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
        Gp_SpawnPadLerp(6, 0x7F, 0x30);
    }
    if (work->stateTimer == 0xCE) {
        s32 pan;

        pan = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_STRANGER_DEATH_END, pan, (s8)worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]));
    }
    if ((u32)((u16)work->stateTimer - 0x29) < 5U) {
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[3], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[1], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x12], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[2], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[3], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[4], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[1], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x13], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
        Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x12], 0, 0);
    }
    if ((u32)((u16)work->stateTimer - 0x2E) < 4U) {
        if (!(*(volatile u16*)&work->stateTimer & 1)) {
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[2], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[3], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[4], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
        } else {
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[1], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x13], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x11], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x10], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[5], 0, 0);
            Gp_SpawnEff(EFFECT_NEO_ARK_FOREST_FALLING_LEAF, &arg0->extra.tmd->coords[0x12], 0, 0);
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
        func_actor_356100_80163CD4,
        func_actor_356100_8016A3D4,
        func_actor_356100_80163E2C,
        func_actor_356100_80164158,
        func_actor_356100_80164ACC,
        func_actor_356100_801653F4,
        func_actor_356100_80165B30,
        func_actor_356100_80166018,
        func_actor_356100_801666B4,
        func_actor_356100_8016A468,
        func_actor_356100_801668FC,
        func_actor_356100_8016A550,
        func_actor_356100_8016A5DC,
        func_actor_356100_8016A668,
        func_actor_356100_80166CF0,
        func_actor_356100_8016A710,
        func_actor_356100_8016A834,
        func_actor_356100_80167358,
        func_actor_356100_80167584,
        func_actor_356100_80167818,
        func_actor_356100_80167A7C,
        func_actor_356100_801684F0,
        func_actor_356100_8016804C,
        func_actor_356100_80168AFC,
        func_actor_356100_80168E44,
        NULL,
        func_actor_356100_80169180,
    }
};

/// The enemy's three task-state handlers, which `func_actor_356100_8016A910`
/// runs by `Task::state`: setup, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_actor_356100_80161F40 = {
    func_actor_356100_8016382C,
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
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
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
        MATRIX* m;

        blk->viewPos.vx = blk->viewPos.vy = blk->viewPos.vz = 0;
        actorTransformToView(&arg1->extra.tmd->coords[1], &blk->viewPos);
        m                             = &blk->shadowCoord.coord;
        MATRIX_PAIR(m, 0, 0)          = 0x1000;
        MATRIX_PAIR(m, 0, 2)          = 0;
        MATRIX_PAIR(m, 1, 1)          = 0x1000;
        MATRIX_PAIR(m, 2, 0)          = 0;
        m->m[2][2]                    = 0x1000;
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
    actorTransformToView(&arg1->extra.tmd->coords[2], &blk->viewPos);
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

s32 func_actor_356100_80169E5C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/actor_messages_release_hold.inc.c"

s32 func_actor_356100_8016A0B8(Task* arg0, s32 arg1, ActorCommand* arg2, s32 arg3)
{
    _Actor356100Work* work = arg0->work;
    s32               code;

    work->commandBytes[0] = arg2->context.loc.stage;
    work->commandBytes[1] = arg2->context.loc.area;
    work->commandBytes[2] = arg2->command;
    if (arg2->context.loc.stage == GAME_STAGE_SHELTER_NEO_ARK && arg2->context.loc.area == GAME_AREA_NEO_ARK_FOREST_ZONE) {
        code = arg2->command;
        switch (code) {
            case 0:
            case 2:
                work->state = ACTOR_356100_STATE_HIDDEN;
                return 1;
            case 1:
                work->state       = ACTOR_356100_STATE_SCRIPTED_DEATH;
                work->animId      = code;
                work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}

static void func_actor_356100_8016A158(Task* task)
{
    _Actor356100Work* work;
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
        enemy->recs = 0;
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
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_356100_80163508(arg0);
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
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_356100_80163508(arg0);
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
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_356100_80163508(arg0);
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
        func_actor_356100_80163508(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        func_actor_356100_80163508(arg0);
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
        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 0), 0);
    }
    if (work->rig.slots[1].status.fields.flags & 2) {
        work->state = ACTOR_356100_STATE_GRAB_RELEASE;
    }
    work->grabAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
    if (work->rig.slots[1].status.fields.flags & 1) {
        if (work->animId == 0xA) {
            work->animId      = 0xB;
            work->animRequest = ACTOR_356100_ANIM_REQUEST_RESET;
            func_actor_356100_80163508(arg0);
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
    func_actor_356100_80163508(arg0);
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
