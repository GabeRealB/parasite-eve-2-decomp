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
#include "gameplay/geometry.h"
#include "gameplay/loading.h"
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
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"

/// Uniform model-root scale for yaw rebuilds, with 12 fractional bits.
enum { ACTOR_01900_ROOT_SCALE = 0x1194 };

/// Clips used for walking, running, staggering and lying down.
enum {
    ACTOR_01900_ANIM_WALK = 2,
    ACTOR_01900_ANIM_RUN  = 3,
    ACTOR_01900_ANIM_FALL = 10,
    ACTOR_01900_ANIM_DOWN = 11
};

/// Collision-sphere radii in game-coordinate units.
enum {
    ACTOR_01900_BODY_RADIUS         = 384,
    ACTOR_01900_COMPACT_BODY_RADIUS = 192
};

/// Hit-effect argument halves; each selected effect kind interprets the low half.
enum {
    ACTOR_01900_HIT_EFFECT_ARGUMENT_LOW = 0x300,
    ACTOR_01900_HIT_EFFECT_REPEAT_COUNT = 2
};

/// Values of `_Actor01900Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Five numbers have no handler and are never selected. `UNUSED_0C` and
/// `UNUSED_0D` are still tested when a hit lands: with `playerHeld` set, the
/// hit sends the player the message that ends a scripted hold.
enum {
    ACTOR_01900_STATE_HIDDEN           = 0x00, // not drawn, not lockable and not collided with
    ACTOR_01900_STATE_PLAY_WALK        = 0x01, // loops the walk animation in place; never selected
    ACTOR_01900_STATE_PLAY_RUN         = 0x02, // loops the run animation in place; never selected
    ACTOR_01900_STATE_PLAY_DOWN        = 0x03, // holds the lying animation; never selected
    ACTOR_01900_STATE_STATUS_HOLD      = 0x04, // twitches in place until the status buildup runs out
    ACTOR_01900_STATE_FLINCH           = 0x05, // recoils from damage over time, then chases
    ACTOR_01900_STATE_ALERT            = 0x06, // turns to the player and raises the combat alert, then chases
    ACTOR_01900_STATE_CHASE            = 0x07, // runs at the player; strikes when close, sidesteps a distant player who faces it
    ACTOR_01900_STATE_CIRCLE           = 0x08, // runs round the player, speeding up and then easing off
    ACTOR_01900_STATE_TURN_AROUND      = 0x09, // swings `turnYaw` round to `turnYawTarget`, then circles
    ACTOR_01900_STATE_SIDESTEP         = 0x0A, // hops along `sidestepDir`, then chases
    ACTOR_01900_STATE_STRIKE           = 0x0B, // swings at the player with `attackBody` enabled
    ACTOR_01900_STATE_UNUSED_0C        = 0x0C, // has no handler and is never selected
    ACTOR_01900_STATE_UNUSED_0D        = 0x0D, // has no handler and is never selected
    ACTOR_01900_STATE_STEP_BACK        = 0x0E, // steps back from a player still close when the strike ends
    ACTOR_01900_STATE_RISE             = 0x0F, // gets up after `DOWN`, then chases
    ACTOR_01900_STATE_UNUSED_10        = 0x10, // has no handler and is never selected
    ACTOR_01900_STATE_DOWN             = 0x11, // lies where it fell until `stateTimer` runs out
    ACTOR_01900_STATE_APPROACH         = 0x12, // runs at the player on a half-rate animation; never selected
    ACTOR_01900_STATE_FALL             = 0x13, // staggers backward and falls
    ACTOR_01900_STATE_UNUSED_14        = 0x14, // has no handler and is never selected
    ACTOR_01900_STATE_DEATH_BURN       = 0x15, // burns away: the corpse-burn effect, then the model flattens and fades
    ACTOR_01900_STATE_UNUSED_16        = 0x16, // has no handler and is never selected
    ACTOR_01900_STATE_DORMANT_SCRIPTED = 0x17, // loops an animation of its own until the player comes within `noticeRange` or makes noise
    ACTOR_01900_STATE_PATROL           = 0x18, // walks between the two `patrolPoints`, watching for the player
    ACTOR_01900_STATE_BACK_OFF         = 0x19, // faces the player, backs away and turns aside; never selected
    ACTOR_01900_STATE_SLIDE            = 0x1A, // coasts forward by the shrinking `runStep`, then turns around
    ACTOR_01900_STATE_ALERT_REPEAT     = 0x1B, // replays the alert animation in place after a long chase in sight of the player, then chases again
    ACTOR_01900_STATE_SCRIPTED_WATCH   = 0x1C, // holds a pose where a room command placed it, `lookYawTarget` following the player
    ACTOR_01900_STATE_DEATH_BURST      = 0x1D, // bursts into body parts where it stands
    ACTOR_01900_STATE_DEATH_BURST_WALK = 0x1E, // walks a few steps, bursts and burns away
    ACTOR_01900_STATE_REFALL           = 0x1F, // drops back down: knocked down while in `RISE`, or killed in `STATUS_HOLD`
    ACTOR_01900_STATE_COUNT                    // number of states, and of the handlers in `_Actor01900StateTable`
};

/// Values of `_Actor01900Work::animRequest` and `_Actor01900Work::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
/// The blend rig is never asked to blend in.
enum {
    ACTOR_01900_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames its transition table gives
    ACTOR_01900_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ACTOR_01900_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Local message slot ignored by this actor; its sender-side purpose is unproven.
enum { ACTOR_01900_MESSAGE_IGNORED = 2015 };

/// Number of active height-clamp rows; the trailing zero row is not scanned.
enum { ACTOR_01900_HEIGHT_CLAMP_COUNT = 2 };

/// Work block of the Grinning Stranger task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`; the
/// exit callback unlinks its three collision bodies. It holds the state
/// machine, both animation rigs and their driver's state, the bodies with
/// their contact records, storage for the model's matrices and the values the
/// pursuit states share.
///
/// Angles are 4096ths of a turn and positions are in the root coordinate's
/// parent space unless a field says otherwise. Animation ids index the
/// package's animation bank; rates are sixteenths of a frame per tick.
typedef struct {
    s16                   state;             // `ACTOR_01900_STATE_*`
    s16                   prevState;         // `state` the tick last ran; -1 makes the next tick enter `state` afresh
    s16                   stateEntered;      // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16                   stateTimer;        // frame counter of the state: most count up from 0, `DOWN` counts down; the task also counts its three-frame wait after setup here
    s16                   stateCounter;      // second counter of `CHASE` and `CIRCLE`, cleared as either begins: in `CHASE` the ticks since sight of the player was last blocked, 0x5B of which lead to `ALERT_REPEAT`; in `CIRCLE` the ticks on which `gridContacts` moved the root, seven of which end it
    byte                  field_A[0x2];      // never accessed
    ActorPatrolPoint      patrolPoints[2];   // the spawn position and a point 2000 units ahead along the spawn facing
    s16                   patrolTarget;      // index into `patrolPoints` of the end being walked toward
    s16                   placedYaw;         // heading of the root after the last placement message; never read
    byte                  field_18[0x4];     // never accessed
    ActorAnimRig19        rig;               // playback of the model's parts; slot 1's status and frame time the states
    ActorAnimRig19        blend;             // second playback of the same model, mixed into slots 1 to 10 while `blendActive`
    s32                   dormantAnimFrame;  // slot 1's frame on the last `DORMANT_SCRIPTED` tick, so the sound of frame 15 and the effect of frame 5 each fire once
    s16                   animRequest;       // `ACTOR_01900_ANIM_REQUEST_*` for `rig`
    s16                   blendActive;       // 1 while `blend`'s animation is mixed in; cleared when its slot 1 settles
    s16                   appliedAnim;       // animation `rig` was last started on
    s16                   animId;            // animation requested of `rig`
    u16                   animFrames;        // ticks since `animRequest` was last applied; never read
    s16                   animRate;          // playback rate of `rig`'s slots; negative plays backward
    s16                   baseRate;          // `animRate` that `RISE` starts on; 0x10 from setup
    s16                   blendRequest;      // `ACTOR_01900_ANIM_REQUEST_*` for `blend`; only `RESET` is requested
    s16                   blendAnimId;       // animation requested of `blend`
    s16                   blendRate;         // playback rate of `blend`'s slots, 0x30 from each restart
    s16                   blendWeight;       // share of `blend`'s pose in the mix, of 0x1000; 0x800 from each restart
    s16                   lookYawTarget;     // bearing to what the state faces, relative to the facing
    s16                   lookYaw;           // eased toward `lookYawTarget` by 0x100 a tick; within +-0x400, parts 5 and 2 turn by 2/3 and 1/2 of it
    s32                   lastCueFrame;      // slot 1's frame as the sound cues last saw it, so a held cue frame fires once; 0 from each applied request
    EffectSpawnArg        effectArg;         // argument record of the hit and dormant effects, hung off part 1
    byte                  field_8C0[0x8];    // never accessed
    WorldCollisionBody    hitBody;           // sphere at part 2; takes the hits and is pushed off other bodies
    WorldCollisionContact hitContacts[12];   // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    gridBody;          // sphere 0x100 above the root that the room grid pushes the root with
    WorldCollisionContact gridContacts[12];  // contacts of `gridBody`
    WorldCollisionBody    attackBody;        // sphere on part 4 carrying the key of attack 0; enabled only on ticks 0x16 to 0x1C of `STRIKE`
    WorldCollisionContact attackContacts[3]; // contacts of `attackBody`, which is given the first as a one-record table; `STRIKE` scans all three, and a kind-0x10000 record ends the swing
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    byte                  field_BF0[0x20];   // never accessed
    s16                   hitCooldown;       // ticks before another hit is taken; set from the hit's id parameter 2
    s16                   recentDamage;      // damage of the hits taken while `recentHitFrames` runs; 0x4C or more turns a flinch into a fall
    s16                   recentHitFrames;   // ticks left before `recentDamage` is forgotten, 5 from each hit; while it runs a hit pushes the root back 0x19 instead of 0x64
    byte                  field_C16[0x2];    // never accessed
    SVECTOR               sidestepDir;       // unit direction of the sidestep, to one side of the bearing to the player
    s16                   turnYaw;           // heading the turn-around holds the root at, moved 0x89 a tick
    s16                   turnYawTarget;     // heading the turn-around ends on: the facing plus twice the bearing to the player
    s16                   runStep;           // forward step per tick: `CIRCLE` sets it to 8 times `animRate`, halved while `blendActive` and 2 once it has been pushed; `SLIDE` takes 10 off it each tick
    s16                   circleRateStep;    // change of `animRate` per tick of `CIRCLE`: 8 until the rate reaches 0x18, -1 back down to 0x12, then 0 for its last five ticks
    s16                   sidestepSide;      // side the next sidestep takes (1 or -1), flipped by each; 0 draws one at random. `CHASE` veers 0x300 to the same side while its sight of the player is blocked
    s16                   sidestepStep;      // length of the sidestep's step, 0xDE; halved while `blendActive` and by each push of `gridContacts`
    s16                   downFramesBase;    // ticks `DOWN` lasts, before a random 0..15 more; first value of the variant record
    s16                   sidestepAngle;     // angle between the bearing to the player and `sidestepDir`; second value of the variant record
    s16                   sidestepDelay;     // `stateTimer` value `CHASE` must pass before it sidesteps, raised by half `sidestepCount`; third value of the variant record
    s16                   noticeRange;       // distance at which `DORMANT_SCRIPTED` and `PATROL` notice the player; fourth value of the variant record
    u8                    commandBytes[3];   // first three bytes of the last actor command received: its 16-bit kind and the low byte of its sub-code
    u8                    field_C37;         // counted down to 0 by `CHASE`; nothing raises it, role unproven
    Task*                 childTask0;        // killed by the exit callback when set; nothing sets it
    Task*                 childTask1;        // killed by the exit callback when set; nothing sets it
    s16                   circleCount;       // times `CIRCLE` has begun since `CHASE`, `STRIKE` or a hit cleared it; from the second, a turn-around ending within 0x384 of the player does not circle again
    s16                   sidestepCount;     // sidesteps since the last hit; the first is thrown 0x171 wider
    s16                   playerHeld;        // 1 makes a hit in `UNUSED_0C` or `UNUSED_0D` end the player's scripted hold; nothing sets it
    byte                  field_C46[0x2];    // never accessed
    SVECTOR               bodyPosHistory[7]; // ring of part 2's view-space position on the last seven ticks; during a sidestep the enemy's target point is the oldest
    byte                  field_C80[0x18];   // never accessed
    s16                   bodyPosCursor;     // index of the next entry of `bodyPosHistory` to write
} _Actor01900Work;
STATIC_ASSERT_SIZEOF(_Actor01900Work, 0xC9C);

/// The actor's state handlers, indexed by `_Actor01900Work::state`.
///
/// The package defines one table. The per-frame tick copies it to the stack
/// by assignment, which is why the array is wrapped in a struct, and then
/// calls the entry of the current state. The call is unconditional, so the
/// `NULL` entries of the five `ACTOR_01900_STATE_UNUSED_*` values mark states
/// the actor must not be in when the tick dispatches.
typedef struct {
    TaskFunc handlers[ACTOR_01900_STATE_COUNT]; // Handler of each `ACTOR_01900_STATE_*`, taking the actor's task
} _Actor01900StateTable;
STATIC_ASSERT_SIZEOF(_Actor01900StateTable, ACTOR_01900_STATE_COUNT * sizeof(TaskFunc));

extern EnemyParams          Actor01900_D0AC54;
extern ActorStrangerVariant Actor01900_D0AC64[];
extern AnimationSet*        Actor01900_D17174[46];
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01900_D1728C[8];
static AnimationSet     _gActor01900Actor101900Animation16960;
extern ActorHeightClamp Actor01900_D172CC[];
/// Twelve preset hit-reaction directions `_actor01900SpawnHitEffect` copies from;
/// `pad` carries the index of the coordinate the effect is attached to.
extern SVECTOR   Actor01900_D1722C[];
static TmdSource _gActor01900StrangerBurstHand;
extern s16       Actor01900_D172FC;

#include "../../shared/actor_contacts.h"

static void Actor01900_Fn02A50(Task* arg0);
static void _actor01900SpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey);
static void _actor01900UpdateAnimation(Task* task);
static void Actor01900_Fn0AB1C(Task* arg0);
static void _actor01900Exit(Task* task);
static s32  _actor01900ApplyBodyPushback(Task* task, const WorldCollisionContact* contacts, s16 contactCount);
static void Actor01900_Fn08724(Task* arg0);
static void _actor01900StatePlayWalk(Task* task);
static void _actor01900ClampRootHeight(const GameLocationKey* location, GfxCoord* coord);
static s32  _actor01900HandleAnimationRequest(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unused);
static s32  _actor01900ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused);

static TmdSource _gActor01900GrinningStrangerBody;
static s32       _actor01900AcknowledgeHoldRelease(Task* task, s32 messageId, s32 unusedPayload, s32 unused);
static s32       _actor01900IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unused);
void             Actor01900_Fn0ABE4(Task*);

DamageAttack Actor01900_D0AC4C[2] = {
    { 16, 7 },
    { 22, 0 },
};

EnemyParams Actor01900_D0AC54 = { Actor01900_D0AC4C, 160, 42, 48, 4, 100, 10, 100, 0 };

ActorStrangerVariant Actor01900_D0AC64[3] = {
    { 15, 400, 8, 2000, { 0, 0, 0, 0 } },
    { 0, 300, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 200, 7, 3000, { 0, 0, 0, 0 } },
};

static TmdBone _gActor01900GrinningStrangerBodySkeleton[19] = {
#include "assets/grinning_stranger_body_skeleton.inc"
};

static u32 _gActor01900GrinningStrangerBodyPartVerts[19] = {
#include "assets/grinning_stranger_body_partVerts.inc"
};

static SVECTOR _gActor01900GrinningStrangerBodyVerts[306] = {
#include "assets/grinning_stranger_body_verts.inc"
};

static SVECTOR _gActor01900GrinningStrangerBodyNormals[365] = {
#include "assets/grinning_stranger_body_normals.inc"
};

static u32 _gActor01900GrinningStrangerBodyStream[3988] = {
#include "assets/grinning_stranger_body_stream.inc"
};

static TmdSource _gActor01900GrinningStrangerBody = {
    0,
    20032,
    7672,
    19,
    _gActor01900GrinningStrangerBodyPartVerts,
    _gActor01900GrinningStrangerBodyVerts,
    _gActor01900GrinningStrangerBodyNormals,
    _gActor01900GrinningStrangerBodySkeleton,
    _gActor01900GrinningStrangerBodyStream,
};

static TmdBone _gActor01900StrangerBurstHandSkeleton[3] = {
#include "assets/stranger_burst_hand_skeleton.inc"
};

static u32 _gActor01900StrangerBurstHandPartVerts[3] = {
#include "assets/stranger_burst_hand_partVerts.inc"
};

static SVECTOR _gActor01900StrangerBurstHandVerts[33] = {
#include "assets/stranger_burst_hand_verts.inc"
};

static SVECTOR _gActor01900StrangerBurstHandNormals[45] = {
#include "assets/stranger_burst_hand_normals.inc"
};

static u32 _gActor01900StrangerBurstHandStream[357] = {
#include "assets/stranger_burst_hand_stream.inc"
};

static TmdSource _gActor01900StrangerBurstHand = {
    0,
    2008,
    304,
    3,
    _gActor01900StrangerBurstHandPartVerts,
    _gActor01900StrangerBurstHandVerts,
    _gActor01900StrangerBurstHandNormals,
    _gActor01900StrangerBurstHandSkeleton,
    _gActor01900StrangerBurstHandStream,
};

static AnimationPackedPose _gActor01900Actor101900Animation115E8Bank1[19] = {
#include "assets/actor_101900_animation_115E8_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation115E8Bank4[259] = {
#include "assets/actor_101900_animation_115E8_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation115E8Records[337] = {
#include "assets/actor_101900_animation_115E8_records.inc"
};

static u16 _gActor01900Actor101900Animation115E8Indices[20] = {
#include "assets/actor_101900_animation_115E8_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation115E8 = {
    _gActor01900Actor101900Animation115E8Records,
    _gActor01900Actor101900Animation115E8Indices,
    { NULL, _gActor01900Actor101900Animation115E8Bank1, NULL, NULL, _gActor01900Actor101900Animation115E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation119ECBank1[6] = {
#include "assets/actor_101900_animation_119EC_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation119ECBank4[91] = {
#include "assets/actor_101900_animation_119EC_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation119ECRecords[128] = {
#include "assets/actor_101900_animation_119EC_records.inc"
};

static u16 _gActor01900Actor101900Animation119ECIndices[20] = {
#include "assets/actor_101900_animation_119EC_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation119EC = {
    _gActor01900Actor101900Animation119ECRecords,
    _gActor01900Actor101900Animation119ECIndices,
    { NULL, _gActor01900Actor101900Animation119ECBank1, NULL, NULL, _gActor01900Actor101900Animation119ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation125D0Bank1[23] = {
#include "assets/actor_101900_animation_125D0_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation125D0Bank4[297] = {
#include "assets/actor_101900_animation_125D0_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation125D0Records[375] = {
#include "assets/actor_101900_animation_125D0_records.inc"
};

static u16 _gActor01900Actor101900Animation125D0Indices[20] = {
#include "assets/actor_101900_animation_125D0_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation125D0 = {
    _gActor01900Actor101900Animation125D0Records,
    _gActor01900Actor101900Animation125D0Indices,
    { NULL, _gActor01900Actor101900Animation125D0Bank1, NULL, NULL, _gActor01900Actor101900Animation125D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation12AF0Bank1[7] = {
#include "assets/actor_101900_animation_12AF0_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation12AF0Bank4[127] = {
#include "assets/actor_101900_animation_12AF0_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation12AF0Records[160] = {
#include "assets/actor_101900_animation_12AF0_records.inc"
};

static u16 _gActor01900Actor101900Animation12AF0Indices[20] = {
#include "assets/actor_101900_animation_12AF0_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation12AF0 = {
    _gActor01900Actor101900Animation12AF0Records,
    _gActor01900Actor101900Animation12AF0Indices,
    { NULL, _gActor01900Actor101900Animation12AF0Bank1, NULL, NULL, _gActor01900Actor101900Animation12AF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation12F44Bank1[8] = {
#include "assets/actor_101900_animation_12F44_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation12F44Bank4[89] = {
#include "assets/actor_101900_animation_12F44_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation12F44Records[144] = {
#include "assets/actor_101900_animation_12F44_records.inc"
};

static u16 _gActor01900Actor101900Animation12F44Indices[20] = {
#include "assets/actor_101900_animation_12F44_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation12F44 = {
    _gActor01900Actor101900Animation12F44Records,
    _gActor01900Actor101900Animation12F44Indices,
    { NULL, _gActor01900Actor101900Animation12F44Bank1, NULL, NULL, _gActor01900Actor101900Animation12F44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation132D0Bank1[10] = {
#include "assets/actor_101900_animation_132D0_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation132D0Bank4[71] = {
#include "assets/actor_101900_animation_132D0_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation132D0Records[106] = {
#include "assets/actor_101900_animation_132D0_records.inc"
};

static u16 _gActor01900Actor101900Animation132D0Indices[20] = {
#include "assets/actor_101900_animation_132D0_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation132D0 = {
    _gActor01900Actor101900Animation132D0Records,
    _gActor01900Actor101900Animation132D0Indices,
    { NULL, _gActor01900Actor101900Animation132D0Bank1, NULL, NULL, _gActor01900Actor101900Animation132D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation13658Bank1[6] = {
#include "assets/actor_101900_animation_13658_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation13658Bank4[77] = {
#include "assets/actor_101900_animation_13658_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation13658Records[111] = {
#include "assets/actor_101900_animation_13658_records.inc"
};

static u16 _gActor01900Actor101900Animation13658Indices[20] = {
#include "assets/actor_101900_animation_13658_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation13658 = {
    _gActor01900Actor101900Animation13658Records,
    _gActor01900Actor101900Animation13658Indices,
    { NULL, _gActor01900Actor101900Animation13658Bank1, NULL, NULL, _gActor01900Actor101900Animation13658Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation139DCBank1[13] = {
#include "assets/actor_101900_animation_139DC_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation139DCBank4[58] = {
#include "assets/actor_101900_animation_139DC_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation139DCRecords[108] = {
#include "assets/actor_101900_animation_139DC_records.inc"
};

static u16 _gActor01900Actor101900Animation139DCIndices[20] = {
#include "assets/actor_101900_animation_139DC_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation139DC = {
    _gActor01900Actor101900Animation139DCRecords,
    _gActor01900Actor101900Animation139DCIndices,
    { NULL, _gActor01900Actor101900Animation139DCBank1, NULL, NULL, _gActor01900Actor101900Animation139DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation14164Bank1[20] = {
#include "assets/actor_101900_animation_14164_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation14164Bank4[166] = {
#include "assets/actor_101900_animation_14164_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation14164Records[236] = {
#include "assets/actor_101900_animation_14164_records.inc"
};

static u16 _gActor01900Actor101900Animation14164Indices[20] = {
#include "assets/actor_101900_animation_14164_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation14164 = {
    _gActor01900Actor101900Animation14164Records,
    _gActor01900Actor101900Animation14164Indices,
    { NULL, _gActor01900Actor101900Animation14164Bank1, NULL, NULL, _gActor01900Actor101900Animation14164Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation14938Bank1[20] = {
#include "assets/actor_101900_animation_14938_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation14938Bank4[177] = {
#include "assets/actor_101900_animation_14938_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation14938Records[244] = {
#include "assets/actor_101900_animation_14938_records.inc"
};

static u16 _gActor01900Actor101900Animation14938Indices[20] = {
#include "assets/actor_101900_animation_14938_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation14938 = {
    _gActor01900Actor101900Animation14938Records,
    _gActor01900Actor101900Animation14938Indices,
    { NULL, _gActor01900Actor101900Animation14938Bank1, NULL, NULL, _gActor01900Actor101900Animation14938Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation15420Bank1[27] = {
#include "assets/actor_101900_animation_15420_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation15420Bank4[250] = {
#include "assets/actor_101900_animation_15420_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation15420Records[347] = {
#include "assets/actor_101900_animation_15420_records.inc"
};

static u16 _gActor01900Actor101900Animation15420Indices[20] = {
#include "assets/actor_101900_animation_15420_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation15420 = {
    _gActor01900Actor101900Animation15420Records,
    _gActor01900Actor101900Animation15420Indices,
    { NULL, _gActor01900Actor101900Animation15420Bank1, NULL, NULL, _gActor01900Actor101900Animation15420Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation15F20Bank1[46] = {
#include "assets/actor_101900_animation_15F20_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation15F20Bank4[218] = {
#include "assets/actor_101900_animation_15F20_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation15F20Records[328] = {
#include "assets/actor_101900_animation_15F20_records.inc"
};

static u16 _gActor01900Actor101900Animation15F20Indices[20] = {
#include "assets/actor_101900_animation_15F20_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation15F20 = {
    _gActor01900Actor101900Animation15F20Records,
    _gActor01900Actor101900Animation15F20Indices,
    { NULL, _gActor01900Actor101900Animation15F20Bank1, NULL, NULL, _gActor01900Actor101900Animation15F20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01900Actor101900Animation16960Bank1[12] = {
#include "assets/actor_101900_animation_16960_bank1.inc"
};

static AnimationPackedRotation _gActor01900Actor101900Animation16960Bank4[254] = {
#include "assets/actor_101900_animation_16960_bank4.inc"
};

static AnimationRecord _gActor01900Actor101900Animation16960Records[346] = {
#include "assets/actor_101900_animation_16960_records.inc"
};

static u16 _gActor01900Actor101900Animation16960Indices[20] = {
#include "assets/actor_101900_animation_16960_indices.inc"
};

static AnimationSet _gActor01900Actor101900Animation16960 = {
    _gActor01900Actor101900Animation16960Records,
    _gActor01900Actor101900Animation16960Indices,
    { NULL, _gActor01900Actor101900Animation16960Bank1, NULL, NULL, _gActor01900Actor101900Animation16960Bank4, NULL, NULL, NULL },
};

s8 Actor01900_D16988[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 3, 0, 0, 0, 0, 15, 5, 0, 0, 8, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 3, 3, 3, 3, 5, 0, 0, 0, 15, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 3, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 3, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

AnimationSet* Actor01900_D17174[46] = {
    NULL,
    NULL,
    &_gActor01900Actor101900Animation15F20,
    &_gActor01900Actor101900Animation15F20,
    &_gActor01900Actor101900Animation115E8,
    NULL,
    NULL,
    &_gActor01900Actor101900Animation15420,
    &_gActor01900Actor101900Animation125D0,
    &_gActor01900Actor101900Animation12AF0,
    &_gActor01900Actor101900Animation132D0,
    &_gActor01900Actor101900Animation13658,
    NULL,
    &_gActor01900Actor101900Animation119EC,
    NULL,
    NULL,
    NULL,
    &_gActor01900Actor101900Animation12F44,
    NULL,
    &_gActor01900Actor101900Animation12AF0,
    &_gActor01900Actor101900Animation14164,
    &_gActor01900Actor101900Animation14938,
    NULL,
    &_gActor01900Actor101900Animation13658,
    &_gActor01900Actor101900Animation139DC,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

SVECTOR Actor01900_D1722C[12] = {
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

TaskMessageEntry Actor01900_D1728C[8] = {
    { ACTOR_01900_MESSAGE_IGNORED, _actor01900IgnoreMessage2015 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor01900HandleAnimationRequest },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor01900AcknowledgeHoldRelease },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor01900ApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorHeightClamp Actor01900_D172CC[3] = {
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, -300, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_WOODLAND_PATH, 0, 300, { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
};

s16 Actor01900_D172FC = 0;

TaskDesc Actor01900_D17300 = { { { TASK_BODY_TMD, 96 } }, Actor01900_Fn0ABE4, { .model = &_gActor01900GrinningStrangerBody } };

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

/// Cross-fade lengths in frames, indexed by the clip being left and the clip
/// being entered. `_actor01900UpdateAnimation` reads one entry per animation change.
extern s8 Actor01900_D16988[][0x2D];

static SVECTOR ActorContact_ScratchPosition;

static const _Actor01900StateTable Actor01900_D001BC;

static void _actor01900StateStatusHold(Task* task);

static void _actor01900StateAlert(Task* task);

static void _actor01900StateChase(Task* task);

static void _actor01900StateCircle(Task* task);

static void _actor01900StateTurnAround(Task* task);

static void _actor01900StateSidestep(Task* task);

static void _actor01900StateStepBack(Task* task);

static void _actor01900StateApproach(Task* task);

static void _actor01900StateFall(Task* task);

static void Actor01900_Fn06904(Task* arg0);

static void Actor01900_Fn06B4C(Task* arg0);

static void _actor01900StatePatrol(Task* task);

static void _actor01900StateSlide(Task* task);

static void _actor01900StateBackOff(Task* task);

static void _actor01900StateAlertRepeat(Task* task);

static void _actor01900StateScriptedWatch(Task* task);

static void Actor01900_Fn0892C(Task* arg0);

static void _actor01900StateStrike(Task* task);

static void _actor01900StateRefall(Task* task);

static void _actor01900StateHidden(Task* task);

static void _actor01900StatePlayRun(Task* task);

static void _actor01900StatePlayDown(Task* task);

static void Actor01900_Fn0A9C0(Task* arg0);

static void Actor01900_Fn0AA78(Task* arg0);

static void Actor01900_Fn0ABA0(Enemy* enemy, Task* task);

static __inline__ void Actor01900_ResetYaw(GfxCoord* coord);

static void            _actor01900TickBlendedAnimSlots(Task* task);
static s32             _actor01900TakeAnimSoundCue(_Actor01900Work* work);
static __inline__ void _actor01900BindLightingMatrices(Task* task);
static void            _actor01900Initialize(Enemy* enemy, Task* task);
static __inline__ s32  _actor01900FindIncomingAttack(const WorldCollisionContact* contacts, SVECTOR* hitPosition);
static __inline__ s32  _actor01900EngageBattleIfPlayerLevel(Task* task);
static __inline__ s32  _actor01900HasHeightClamp(const GameLocationKey* location);
static s32             _actor01900ApplyCappedGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset);
static __inline__ s32  _actor01900HasPlayerBodyContact(const WorldCollisionContact* contacts);
static void            Actor01900_Fn09D3C(Enemy* enemy, Task* actor);

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale.
static __inline__ void Actor01900_ResetYaw(GfxCoord* coord)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)blk->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    coord->coord.m[2][2] = (u16)blk->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/player_detection_sight.inc.c"

/// Ticks both rigs and mixes rotations for body slots 1..10.
///
/// Requires live initialized work, model coordinates and loaded animation sets.
/// Slots 11..18 tick only the primary rig; slot 0 is untouched. Rates are in
/// sixteenths of a frame: primary uses `animRate` minus 3, secondary uses `blendRate`.
/// The primary rotation takes `blendWeight` / `ONE` and the secondary its complement;
/// translation always comes from the primary pose. Stack poses are borrowed only
/// through each apply call. Scratch and GTE state must be available.
static void _actor01900TickBlendedAnimSlots(Task* task)
{
    enum { ACTOR_01900_LAST_BLENDED_SLOT = 10 };

    AnimationPose     primaryPose;
    AnimationPose     blendPose;
    AnimationContext* primaryContext;
    s16               primaryWeight;
    s16               slotIndex;
    _Actor01900Work*  work;

    work           = task->work;
    primaryWeight  = work->blendWeight;
    primaryContext = &work->rig.anim;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex <= ACTOR_01900_LAST_BLENDED_SLOT) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = (work->animRate - 3);
            animationTickSlotPose(primaryContext, slotIndex, &primaryPose, NULL);
            animationTickSlotPose(&work->blend.anim, slotIndex, &blendPose, NULL);
            animationApplyPoseWithBlendedRotation(primaryContext, slotIndex, &primaryPose, &blendPose, primaryWeight, ONE - primaryWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Returns the newly reached animation sound cue, or zero.
///
/// Uses the low ten bits of slot 1's current keyframe record index, not elapsed
/// frames. The returned bank/entry id leaves its instance byte zero for the caller.
/// A held cue is latched in `lastCueFrame` and fires once; two-cue clips clear the
/// latch between cues, while single-cue and other clips remember the current index.
static s32 _actor01900TakeAnimSoundCue(_Actor01900Work* work)
{
    enum {
        ACTOR_01900_CUE_ANIM_WALK        = 2,
        ACTOR_01900_CUE_ANIM_RUN         = 3,
        ACTOR_01900_CUE_ANIM_STRIKE      = 4,
        ACTOR_01900_CUE_ANIM_STEP_BACK   = 7,
        ACTOR_01900_CUE_ANIM_ALERT       = 9,
        ACTOR_01900_CUE_ANIM_DOWN        = 11,
        ACTOR_01900_CUE_ANIM_SIDESTEP_1  = 20,
        ACTOR_01900_CUE_ANIM_SIDESTEP_2  = 21,
        ACTOR_01900_SOUND_GAIT_FIRST     = 0x400A0002,
        ACTOR_01900_SOUND_GAIT_SECOND    = 0x400A0001,
        ACTOR_01900_SOUND_ALERT          = 0x400A0006,
        ACTOR_01900_SOUND_STRIKE         = 0x400A000C,
        ACTOR_01900_SOUND_DOWN           = 0x400A0005,
        ACTOR_01900_SOUND_EVASION_FIRST  = 0x400A0010,
        ACTOR_01900_SOUND_EVASION_SECOND = 0x400A0011
    };

    s32 cueIndex;
    s32 previousCueIndex;

    switch (work->animId) {
        case ACTOR_01900_CUE_ANIM_SIDESTEP_1:
        case ACTOR_01900_CUE_ANIM_SIDESTEP_2:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 7) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return ACTOR_01900_SOUND_EVASION_FIRST;
                }
                work->lastCueFrame = cueIndex;
            } else if (cueIndex == 0x10) {
                previousCueIndex = work->lastCueFrame;
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return ACTOR_01900_SOUND_EVASION_SECOND;
                }
                work->lastCueFrame = previousCueIndex;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case ACTOR_01900_CUE_ANIM_STEP_BACK:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0xF) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return ACTOR_01900_SOUND_EVASION_FIRST;
                }
                work->lastCueFrame = cueIndex;
            } else if (cueIndex == 0x14) {
                previousCueIndex = work->lastCueFrame;
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return ACTOR_01900_SOUND_EVASION_SECOND;
                }
                work->lastCueFrame = previousCueIndex;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case ACTOR_01900_CUE_ANIM_WALK:
        case ACTOR_01900_CUE_ANIM_RUN:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0x24) {
                if (work->lastCueFrame != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return ACTOR_01900_SOUND_GAIT_FIRST;
                }
                work->lastCueFrame = cueIndex;
            } else if (cueIndex == 0x2C) {
                previousCueIndex = work->lastCueFrame;
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrame = cueIndex;
                    return ACTOR_01900_SOUND_GAIT_SECOND;
                }
                work->lastCueFrame = previousCueIndex;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case ACTOR_01900_CUE_ANIM_ALERT:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 4 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return ACTOR_01900_SOUND_ALERT;
            }
            work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case ACTOR_01900_CUE_ANIM_STRIKE:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 0xC && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return ACTOR_01900_SOUND_STRIKE;
            }
            work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case ACTOR_01900_CUE_ANIM_DOWN:
            cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (cueIndex == 4 && work->lastCueFrame != cueIndex) {
                work->lastCueFrame = cueIndex;
                return ACTOR_01900_SOUND_DOWN;
            }
            work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        default:
            previousCueIndex   = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = previousCueIndex;
            break;
    }
    return 0;
}

/// Seeks primary slots 1..18 into the requested clip when its id changes.
///
/// Requires initialized work and loaded clips for both `appliedAnim` and `animId`.
/// The signed transition-table byte is a duration in whole normal-rate frames;
/// track offset is zero. Sets each slot's rate in sixteenths of a frame before
/// the seek captures its old pose, then records the new applied id. Slot 0 is untouched.
static inline void _actor01900BlendToRequestedAnim(Task* task)
{
    _Actor01900Work* work;
    s32              slotIndex;

    work = task->work;
    if (work->appliedAnim != work->animId) {
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            work->rig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0,
                                       (s32)Actor01900_D16988[work->appliedAnim][work->animId]);
        }
        work->appliedAnim = work->animId;
    }
}

/// Restarts primary slots 1..18 on the requested loaded clip.
///
/// Requires initialized work, a live model and a loaded `animId`. Records `appliedAnim`
/// after resetting. The rate writes precede animationResetSlot, which replaces
/// each slot rate with `ANIMATION_RATE_ONE`; the following tick reapplies `animRate`.
/// Slot 0 is untouched.
static inline void _actor01900ResetToRequestedAnim(Task* task)
{
    _Actor01900Work* work;
    s32              slotIndex;

    work = task->work;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationResetSlot(&work->rig.anim, slotIndex, work->animId);
    }
    work->appliedAnim = work->animId;
}

/// Restarts secondary slots 1..18 and restores the half-weight blend defaults.
///
/// Requires initialized work and a loaded `blendAnimId`. `blendRate` becomes three
/// normal frames per tick, `blendWeight` becomes `ONE` / 2. The retained rate writes
/// target primary slots; secondary reset uses the API's normal rate until its
/// next blended tick. Does not enable blending and leaves slot 0 untouched.
static inline void _actor01900ResetBlendAnim(Task* task)
{
    _Actor01900Work* work;
    s32              slotIndex;

    work              = task->work;
    work->blendRate   = 3 * ANIMATION_RATE_ONE;
    work->blendWeight = ONE / 2;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->blend.anim, slotIndex, work->blendAnimId);
    }
}

/// Ticks primary body slots 1..18 at `animRate`, leaving slot 0 untouched.
///
/// Requires initialized work, live model coordinates and loaded referenced clips.
/// The rate uses sixteenths of a frame per call; reverse playback requires
/// bank-backed current endpoints. Writes poses to the primary model coordinates.
static inline void _actor01900TickAnimSlots(Task* task)
{
    _Actor01900Work* work;
    s32              slotIndex;

    work = task->work;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationTickSlot(&work->rig.anim, slotIndex);
    }
}

/// Eases relative look yaw and applies it to the two animated model joints.
///
/// Call after animation writes the coordinates, with this task's initialized work.
/// Angles use 4096 units per turn. Signed linear easing advances at most 256 units
/// per call without wrapping. Joint input clamps to -1024..1024; parts 5 and 2
/// receive two thirds and one half, with division truncating toward zero. The
/// stored eased yaw remains unclamped; affected composition caches become dirty.
static inline void _actor01900UpdateLookYaw(Task* task, _Actor01900Work* work)
{
    enum {
        ACTOR_01900_LOOK_YAW_STEP  = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_01900_LOOK_YAW_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };

    s16 currentYaw;
    s16 targetYaw;
    s16 lookYaw;
    s32 clampedYaw;
    u16 currentYawBits;
    u16 targetYawBits;

    // Ease look direction, then rotate the two joints after animation has written them.
    targetYaw      = work->lookYawTarget;
    currentYaw     = work->lookYaw;
    targetYawBits  = (u16)work->lookYawTarget;
    currentYawBits = (u16)work->lookYaw;
    if (targetYaw > currentYaw) {
        if ((targetYaw - currentYaw) >= (ACTOR_01900_LOOK_YAW_STEP + 1)) {
            work->lookYaw = currentYawBits + ACTOR_01900_LOOK_YAW_STEP;
        } else {
            work->lookYaw = (s16)targetYawBits;
        }
    } else if ((currentYaw - targetYaw) >= (ACTOR_01900_LOOK_YAW_STEP + 1)) {
        work->lookYaw = currentYawBits - ACTOR_01900_LOOK_YAW_STEP;
    } else {
        work->lookYaw = (s16)targetYawBits;
    }
    lookYaw    = work->lookYaw;
    clampedYaw = (u16)work->lookYaw;
    if (lookYaw != 0) {
        if (lookYaw >= (ACTOR_01900_LOOK_YAW_LIMIT + 1)) {
            clampedYaw = ACTOR_01900_LOOK_YAW_LIMIT;
        }
        if (lookYaw < -ACTOR_01900_LOOK_YAW_LIMIT) {
            clampedYaw = -ACTOR_01900_LOOK_YAW_LIMIT;
        }
        _actorRenderYawJointInWorld(task->extra.tmd->coords + 5, (s16)(((s16)clampedYaw * 2) / 3));
        _actorRenderYawJointInWorld(task->extra.tmd->coords + 2, (s16)((s16)clampedYaw / 2));
        task->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Updates clip requests, poses, look direction and spatial animation sounds.
///
/// Requires initialized work, both rigs bound to loaded clips, a live model and
/// an `Enemy` spawn argument. Main requests seek with the transition-table duration
/// or reset outright, then mark the request playing and restart cue accounting.
/// Only reset is handled for the secondary rig. A blended tick ends blending when
/// secondary slot 1 settles. Look yaw uses 4096 units per turn: it approaches the
/// target by at most 256 per call and joint offsets are limited to a quarter-turn.
/// The tick counter wraps at 16 bits. Sound instances use the enemy placement index.
static void _actor01900UpdateAnimation(Task* task)
{
    enum { ACTOR_01900_SOUND_INSTANCE_SHIFT = 8 };

    _Actor01900Work* work;
    Enemy*           enemy;
    s32              soundCue;
    s32              soundScript;
    s32              audioPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Apply pending clip requests before sampling this tick's poses.
    if (work->animRequest == ACTOR_01900_ANIM_REQUEST_BLEND) {
        _actor01900BlendToRequestedAnim(task);
        work->animRequest  = ACTOR_01900_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    } else if (work->animRequest == ACTOR_01900_ANIM_REQUEST_RESET) {
        _actor01900ResetToRequestedAnim(task);
        work->animRequest  = ACTOR_01900_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_01900_ANIM_REQUEST_RESET) {
        _actor01900ResetBlendAnim(task);
        work->blendRequest = ACTOR_01900_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        _actor01900TickAnimSlots(task);
    } else {
        _actor01900TickBlendedAnimSlots(task);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->blendActive = 0;
        }
    }
    _actor01900UpdateLookYaw(task, work);
    // Tag this placement's sound instance and narrow the spatial offsets to signed bytes.
    soundCue = _actor01900TakeAnimSoundCue(work);
    if (soundCue != 0) {
        soundScript = soundCue | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_01900_SOUND_INSTANCE_SHIFT);
        audioPan    = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundScript, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Binds the model's lighting matrices to storage owned by the actor's work block.
///
/// Requires a live TMD model and initialized `_Actor01900Work`. The model borrows
/// both matrix addresses until task teardown releases its work.
static __inline__ void _actor01900BindLightingMatrices(Task* task)
{
    _Actor01900Work* work;
    TmdObject*       model;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Initializes the Grinning Stranger's animation rigs, collision bodies and behavior.
///
/// Requires a live enemy task with its loaded model and spawn arguments. Allocates
/// zeroed task-owned work, borrows the package's clip bank and links three bodies.
/// Allocation failure destroys the enemy and task. Spawn mode selects hidden,
/// scripted dormancy or patrol; the low spawn nibble selects the pursuit tuning.
/// Success installs the exit callback and advances the task to its setup wait.
static void _actor01900Initialize(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01900_ANIM_WALK           = 2,
        ACTOR_01900_SPAWN_MODE_SHIFT    = 16,
        ACTOR_01900_SPAWN_SELECTOR_MASK = 0xF,
        ACTOR_01900_STATE_REENTER       = -1,
        ACTOR_01900_SPAWN_MODE_HIDDEN   = 2,
        ACTOR_01900_SPAWN_MODE_DORMANT  = 4
    };

    SVECTOR             localOffset;
    VECTOR              worldPosition;
    SVECTOR*            stepVector;
    TmdObject*          model;
    GfxCoord*           rootCoord;
    _Actor01900Work*    work;
    WorldCollisionBody* hitBody;
    WorldCollisionBody* attackBody;
    s32                 spawnMode;

    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    work       = memCalloc(sizeof(_Actor01900Work), 0);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    (sceneAcquireBattleRef)(0);
    task->exitCallback = _actor01900Exit;
    _actor01900BindLightingMatrices(task);
    enemy->field_4    = &task->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)Actor01900_D0AC54.hpMax;
    enemy->param                  = &Actor01900_D0AC54;
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, Actor01900_D17174, model,
                         work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, Actor01900_D17174, model,
                         work->blend.poses, work->blend.slots);
    work->animRequest   = ACTOR_01900_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = ACTOR_01900_ANIM_WALK;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    _actor01900UpdateAnimation(task);

    // Keep grid, incoming-hit and outgoing-attack contacts in separate lists.
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = rootCoord;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0x100;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x13;
    work->gridBody.radius           = ACTOR_01900_BODY_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->hitCooldown    = 0;
    work->gridBody.flags = (work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    hitBody                   = &work->hitBody;
    hitBody->coord            = &task->extra.tmd->coords[2];
    hitBody->context.contacts = work->hitContacts;
    hitBody->pos.vx           = 0;
    hitBody->pos.vy           = 0;
    hitBody->pos.vz           = 0;
    hitBody->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    hitBody->radius           = ACTOR_01900_BODY_RADIUS;
    hitBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    // Stage the attack-body offset before reusing the vector for the patrol endpoint.
    localOffset.vx               = 0;
    localOffset.vy               = 0;
    localOffset.vz               = 0;
    attackBody                   = &work->attackBody;
    attackBody->coord            = &task->extra.tmd->coords[4];
    attackBody->context.contacts = work->attackContacts;
    stepVector                   = &localOffset;
    attackBody->pos.vx           = stepVector->vx;
    attackBody->pos.vy           = stepVector->vy;
    attackBody->pos.vz           = stepVector->vz;
    attackBody->radius           = ACTOR_01900_BODY_RADIUS;
    attackBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackBody->context.contacts, 1, 0);
    work->attackBody.key = damagePackEnemyAttackKey(enemy, 0);

    // Patrol between the placement and a point 2000 units ahead in parent space.
    work->patrolTarget      = 0;
    work->patrolPoints[0].x = task->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z = task->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, stepVector);
    localOffset.vy = 0;
    _actorMovementBuildDisplacement(stepVector, 2000);
    work->patrolPoints[1].x = task->extra.tmd->coords->coord.t[0] + localOffset.vx;
    work->patrolPoints[1].z = task->extra.tmd->coords->coord.t[2] + localOffset.vz;

    task->msgTable          = Actor01900_D1728C;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = ACTOR_01900_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = ACTOR_01900_HIT_EFFECT_REPEAT_COUNT;
    spawnMode                  = task->spawnArg1.value >> ACTOR_01900_SPAWN_MODE_SHIFT;
    switch (spawnMode & ACTOR_01900_SPAWN_SELECTOR_MASK) {
        case ACTOR_01900_SPAWN_MODE_HIDDEN:
            work->prevState = ACTOR_01900_STATE_REENTER;
            work->state     = ACTOR_01900_STATE_HIDDEN;
            break;
        case ACTOR_01900_SPAWN_MODE_DORMANT:
            work->prevState = ACTOR_01900_STATE_REENTER;
            work->state     = ACTOR_01900_STATE_DORMANT_SCRIPTED;
            break;
        default:
            work->prevState = ACTOR_01900_STATE_REENTER;
            work->state     = ACTOR_01900_STATE_PATROL;
            tmdAllocPrimitiveBuffer(model);
            break;
    }
    switch (task->spawnArg1.value & ACTOR_01900_SPAWN_SELECTOR_MASK) {
        case 2:
            work->downFramesBase = Actor01900_D0AC64[0].downFramesBase;
            work->sidestepAngle  = Actor01900_D0AC64[0].sidestepAngle;
            work->sidestepDelay  = Actor01900_D0AC64[0].sidestepDelay;
            work->noticeRange    = Actor01900_D0AC64[0].noticeRadius;
            break;
        case 1:
            work->downFramesBase = Actor01900_D0AC64[2].downFramesBase;
            work->sidestepAngle  = Actor01900_D0AC64[2].sidestepAngle;
            work->sidestepDelay  = Actor01900_D0AC64[2].sidestepDelay;
            work->noticeRange    = Actor01900_D0AC64[2].noticeRadius;
            break;
        case 0:
        default:
            work->downFramesBase = Actor01900_D0AC64[1].downFramesBase;
            work->sidestepAngle  = Actor01900_D0AC64[1].sidestepAngle;
            work->sidestepDelay  = Actor01900_D0AC64[1].sidestepDelay;
            work->noticeRange    = Actor01900_D0AC64[1].noticeRadius;
            break;
    }

    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    work->bodyPosCursor = 0;
    task->state++;
}

/// Spawns an incoming attack's hit effect at a bearing-selected model offset.
///
/// `hitYaw` is relative to the actor's facing in 4096 units per turn, normally
/// -2048..2048. `attackKey` must identify a valid player attack. Preset `pad` values
/// select model parts 2, 7 or 9. The spawner borrows the scratch offset through the
/// call and the work-owned argument record; one scratch `SVECTOR` is released on return.
/// The rear random selector retains its mask of 2 and unreachable case 1.
static void _actor01900SpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey)
{
    SVECTOR*         hitOffset;
    s32              yawMagnitude;
    _Actor01900Work* work;

    hitOffset    = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    yawMagnitude = (hitYaw >= 0) ? hitYaw : -hitYaw;
    work         = task->work;
    if (yawMagnitude < (ACTOR_TRANSFORM_ANGLE_TURN / 8)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *hitOffset = Actor01900_D1722C[0];
                break;
            case 1:
                *hitOffset = Actor01900_D1722C[1];
                break;
            case 2:
                *hitOffset = Actor01900_D1722C[2];
                break;
            case 3:
                *hitOffset = Actor01900_D1722C[3];
                break;
            default:
                *hitOffset = Actor01900_D1722C[4];
                break;
        }
    } else if (yawMagnitude > (3 * ACTOR_TRANSFORM_ANGLE_TURN / 8)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *hitOffset = Actor01900_D1722C[5];
                break;
            case 1:
                *hitOffset = Actor01900_D1722C[6];
                break;
            default:
                *hitOffset = Actor01900_D1722C[7];
                break;
        }
    } else if (hitYaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = Actor01900_D1722C[8];
        } else {
            *hitOffset = Actor01900_D1722C[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitOffset = Actor01900_D1722C[10];
        } else {
            *hitOffset = Actor01900_D1722C[11];
        }
    }
    // The preset carries a local offset and the model-part index in its pad word.
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = ACTOR_01900_HIT_EFFECT_ARGUMENT_LOW;
    work->effectArg.spawnArgHi = ACTOR_01900_HIT_EFFECT_REPEAT_COUNT;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitOffset->pad], hitOffset, &work->effectArg);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Returns the first incoming attack key and copies its contact point.
///
/// `contacts` borrows twelve readable records from the hit body. A zero key ends
/// the scan; a kind-attack key returns its full signed word. No attack returns 0
/// and leaves `hitPosition` intact. The output receives view-space XYZ, with its
/// pad word untouched; no pointer is retained.
static __inline__ s32 _actor01900FindIncomingAttack(const WorldCollisionContact* contacts, SVECTOR* hitPosition)
{
    enum { ACTOR_01900_INCOMING_CONTACT_COUNT = ARRAY_SIZE(((_Actor01900Work*)NULL)->hitContacts) };

    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ACTOR_01900_INCOMING_CONTACT_COUNT; contactIndex++) {
        if (!contacts[contactIndex].key.value)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPosition->vx = contacts[contactIndex].point.vx;
            hitPosition->vy = contacts[contactIndex].point.vy;
            hitPosition->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

static void Actor01900_Fn02A50(Task* arg0)
{
    PlayerStatus*    config = &gPlayerStatus;
    _Actor01900Work* work;
    Enemy*           enemy;
    ActorHitScratch* head;
    ActorHitScratch* s;
    GfxCoord*        coord;
    Task*            player;
    SVECTOR*         dir;
    s16              z;
    s32              yaw;
    s32              dx;
    s32              dy;
    s32              dz;
    s32              deathSound;
    s32              deathPan;
    s32              hitSound;
    s32              hitPan;
    s32              mag;
    s16              state;
    s16              effect;
    u32              damage;

    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head      = SCRATCH_STACK_CURSOR(ActorHitScratch);
        s         = (SCRATCH_STACK_CURSOR(ActorHitScratch) = head - 1);
        s->hitKey = _actor01900FindIncomingAttack(work->hitContacts, &head[-1].hitPos);
        if (s->hitKey != 0) {
            if (s->hitKey & 0x8000) {
                player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                s->hitPos.vx = player->extra.tmd->coords->workm.t[0];
                s->hitPos.vy = player->extra.tmd->coords->workm.t[1];
                s->hitPos.vz = player->extra.tmd->coords->workm.t[2];
            }
            work->circleCount                     = 0;
            work->sidestepCount                   = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            s->hitOffset.vx = arg0->extra.tmd->coords->workm.t[0];
            s->hitOffset.vy = arg0->extra.tmd->coords->workm.t[1];
            s->hitOffset.vz = arg0->extra.tmd->coords->workm.t[2];
            s->hitOffset.vx = s->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            s->hitOffset.vy = s->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z               = s->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            s->hitOffset.vz = z;
            yaw             = ratan2(s->hitOffset.vx, z);
            coord           = arg0->extra.tmd->coords;
            s->hitYaw       = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->hitYaw       = _actorAngleNormalizeYaw(s->hitYaw);
            _actor01900SpawnHitEffect(arg0, s->hitYaw, s->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            s->criticalEffect   = -1;
            state               = work->state;
            if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_DOWN && state != ACTOR_01900_STATE_REFALL && state != ACTOR_01900_STATE_RISE && state != ACTOR_01900_STATE_STATUS_HOLD) {
                s->towardHit = arg0->extra.tmd->coords->coord;
                gfxRotMatrixY(&s->towardHit, s->hitYaw, 0);
                dir = &s->hitOffset;
                gfxReadMatrixZAxis(&s->towardHit, dir);
                VectorNormalSS(dir, dir);
                if (work->recentHitFrames > 0) {
                    gte_lddp(-0x19);
                    gte_ldsv(dir);
                    gte_gpf12();
                    gte_stsv(dir);
                } else {
                    gte_lddp(-0x64);
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
                if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_DOWN && state != ACTOR_01900_STATE_REFALL && state != ACTOR_01900_STATE_RISE && state != ACTOR_01900_STATE_STATUS_HOLD) {
                    damage    = s->damage * 2;
                    s->damage = damage;
                    if (damage != 0) {
                        s->criticalEffect = 4;
                    }
                }
            }
            damageAccumulateLifeDrainHp(enemy, s->hitKey, s->damage, 0);
            enemy->hp -= s->damage;
            worldTargetAddReadoutAmount(&enemy->node, s->damage, 0);
            work->recentDamage += s->damage;
            effect              = s->criticalEffect;
            if (effect != -1) {
                effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->state == ACTOR_01900_STATE_DORMANT_SCRIPTED) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            if ((work->state == ACTOR_01900_STATE_UNUSED_0C || work->state == ACTOR_01900_STATE_UNUSED_0D) && config->hp > 0 && work->playerHeld == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (enemy->hp <= 0) {
                deathSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0008;
                deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(deathSound, deathPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            } else {
                hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400A0007;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->hitCooldown = damageGetPlayerAttackHitCooldown(s->hitKey);
            switch (damageGetPlayerAttackReaction(s->hitKey) & 0xFFFF) {
                case 4:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                   = work->state;
                    if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_REFALL && state != ACTOR_01900_STATE_DOWN) {
                        if (state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                            work->state = ACTOR_01900_STATE_REFALL;
                        } else {
                            work->state = ACTOR_01900_STATE_FALL;
                        }
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_NONE:
                case 5:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    if (work->state == ACTOR_01900_STATE_DORMANT_SCRIPTED || work->state == ACTOR_01900_STATE_PATROL) {
                        work->state = ACTOR_01900_STATE_ALERT;
                    }
                    state = work->state;
                    if (state == ACTOR_01900_STATE_FALL || state == ACTOR_01900_STATE_RISE || state == ACTOR_01900_STATE_STATUS_HOLD || state == ACTOR_01900_STATE_DOWN) {
                        work->blendActive  = 1;
                        work->blendAnimId  = 0xB;
                        work->blendRequest = ACTOR_01900_ANIM_REQUEST_RESET;
                    } else if (work->recentDamage >= 0x4C || s->critical == 1) {
                        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        state                   = work->state;
                        if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_REFALL && state != ACTOR_01900_STATE_DOWN) {
                            if (state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                                work->state = ACTOR_01900_STATE_REFALL;
                            } else {
                                work->state = ACTOR_01900_STATE_FALL;
                            }
                        }
                    } else {
                        work->blendActive  = 1;
                        work->blendAnimId  = 0xD;
                        work->blendRequest = ACTOR_01900_ANIM_REQUEST_RESET;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    damageStartEnemyBuildup(enemy, s->hitKey, 0);
                    state = work->state;
                    if (state != ACTOR_01900_STATE_DOWN && state != ACTOR_01900_STATE_STATUS_HOLD) {
                        if (state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                            work->state = ACTOR_01900_STATE_REFALL;
                        } else {
                            work->state = ACTOR_01900_STATE_FALL;
                        }
                    } else {
                        work->state = ACTOR_01900_STATE_STATUS_HOLD;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    if (work->state == ACTOR_01900_STATE_DORMANT_SCRIPTED || work->state == ACTOR_01900_STATE_PATROL) {
                        work->state = ACTOR_01900_STATE_ALERT;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, s->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    enemy->reactionFlags   &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                   = work->state;
                    if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_REFALL && state != ACTOR_01900_STATE_STATUS_HOLD && state != ACTOR_01900_STATE_DOWN) {
                        if (state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                            work->state = ACTOR_01900_STATE_REFALL;
                        } else {
                            work->state = ACTOR_01900_STATE_FALL;
                        }
                    }
                    break;
                case 8:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                   = work->state;
                    if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_REFALL && state != ACTOR_01900_STATE_STATUS_HOLD && state != ACTOR_01900_STATE_DOWN) {
                        mag = s->hitYaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag <= 0x500) {
                            if (state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                                work->state = ACTOR_01900_STATE_REFALL;
                            } else {
                                work->state = ACTOR_01900_STATE_FALL;
                            }
                        }
                    }
                    break;
                case 9:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    state                   = work->state;
                    if (state != ACTOR_01900_STATE_FALL && state != ACTOR_01900_STATE_DOWN) {
                        if (state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                            work->state = ACTOR_01900_STATE_REFALL;
                        } else {
                            work->state = ACTOR_01900_STATE_FALL;
                        }
                    }
                    break;
            }
            work->recentHitFrames = 5;
        } else if (work->recentHitFrames <= 0) {
            work->recentDamage = 0;
        } else {
            work->recentHitFrames--;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            s->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                enemy->hp              -= s->damage;
                worldTargetAddReadoutAmount(&enemy->node, s->damage, 0);
                state = work->state;
                if (state == ACTOR_01900_STATE_CHASE || state == ACTOR_01900_STATE_STRIKE || state == ACTOR_01900_STATE_APPROACH || state == ACTOR_01900_STATE_ALERT_REPEAT) {
                    work->state = ACTOR_01900_STATE_FLINCH;
                } else if (state == ACTOR_01900_STATE_STATUS_HOLD) {
                    work->prevState = -1;
                } else if (state != ACTOR_01900_STATE_RISE) {
                    if (state == ACTOR_01900_STATE_FALL || state == ACTOR_01900_STATE_DOWN) {
                        work->blendActive  = 1;
                        work->blendAnimId  = 0xB;
                        work->blendRequest = ACTOR_01900_ANIM_REQUEST_RESET;
                    } else {
                        work->blendActive  = 1;
                        work->blendAnimId  = 0xD;
                        work->blendRequest = ACTOR_01900_ANIM_REQUEST_RESET;
                    }
                }
            }
        }
        if (enemy->hp <= 0) {
            if (s->hitKey != 0) {
                if ((damageGetPlayerAttackReaction(s->hitKey) & 0xFFFF) == 4 || (damageGetPlayerAttackReaction(s->hitKey) & 0xFFFF) == DAMAGE_PLAYER_REACTION_EXPLOSION) {
                    if (work->animId == 2 || work->animId == 3) {
                        work->state = ACTOR_01900_STATE_DEATH_BURST_WALK;
                    } else {
                        work->state = ACTOR_01900_STATE_DEATH_BURST;
                    }
                } else if (work->state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                    work->state = ACTOR_01900_STATE_REFALL;
                } else if (work->state == ACTOR_01900_STATE_STATUS_HOLD) {
                    work->state = ACTOR_01900_STATE_REFALL;
                } else if (work->state != ACTOR_01900_STATE_FALL && work->state != ACTOR_01900_STATE_REFALL && work->state != ACTOR_01900_STATE_DOWN) {
                    work->state = ACTOR_01900_STATE_FALL;
                }
            } else if (work->state == ACTOR_01900_STATE_RISE && work->stateTimer < 0xC) {
                work->state = ACTOR_01900_STATE_REFALL;
            } else if (work->state != ACTOR_01900_STATE_FALL && work->state != ACTOR_01900_STATE_REFALL && work->state != ACTOR_01900_STATE_DOWN && work->state != ACTOR_01900_STATE_DEATH_BURN && work->state != ACTOR_01900_STATE_HIDDEN && work->state != ACTOR_01900_STATE_DEATH_BURST && work->state != ACTOR_01900_STATE_DEATH_BURST_WALK) {
                work->state = ACTOR_01900_STATE_FALL;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorHitScratch);
    }
}

/// Enables the actor's combat-model drawing and targeting and requests its buffers.
///
/// Both borrowed objects must be live and belong to the same initialized actor.
/// Clears all model and target presentation flags. A missing primitive buffer is
/// allocated from the configured auxiliary heap; existing buffers are retained,
/// allocation failure is ignored here and ownership remains with the model.
static inline void _actor01900EnableCombatModel(Enemy* enemy, TmdObject* model)
{
    enemy->node.state.parts.flags = 0;
    model->flags                  = 0;
    tmdAllocPrimitiveBuffer(model);
}

/// Holds the status-buildup pose with alternating forward and reverse twitches.
///
/// Entry restarts the status clip and advances its low-ten-bit record index to
/// at least 6. Later ticks halve the signed rate, restarting at -1 frame per tick
/// when it reaches +1/16 and at +1 frame when it reaches -1/16. Buildup expiry or
/// death selects `ACTOR_01900_STATE_DOWN`. Reverse playback requires bank-backed
/// current endpoints; the task and loaded rig must remain live throughout.
static void _actor01900StateStatusHold(Task* task)
{
    enum { ACTOR_01900_ANIM_STATUS_HOLD = 23 };

    _Actor01900Work* work;
    Enemy*           enemy;
    TmdObject*       model;
    s32              nextRate;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(enemy, model);
        work->animRequest     = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = ACTOR_01900_ANIM_STATUS_HOLD;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        // Restart on bank poses and advance before the first backward playback.
        do {
            _actor01900UpdateAnimation(task);
        } while ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) < 6U);
        work->animRate = 2 * ANIMATION_RATE_ONE;
        return;
    }
    // Halving the signed rate alternates decaying forward and reverse twitches.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    nextRate                              = (s16)work->animRate / 2;
    work->animRate                        = (u16)nextRate;
    if (nextRate == 1) {
        work->animRate = -ANIMATION_RATE_ONE;
    }
    if ((s16)work->animRate == -1) {
        work->animRate = ANIMATION_RATE_ONE;
    }
    _actor01900UpdateAnimation(task);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_01900_STATE_DOWN;
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_01900_STATE_DOWN;
    }
}

/// Engages battle when a non-scripted player is within 500 vertical units.
///
/// Both live model roots must share a parent frame and the player task must exist.
/// The range is strict and ignores X/Z. Returns 1 when the battle-engage request
/// is made, even if battle was already engaged; otherwise returns 0.
static __inline__ s32 _actor01900EngageBattleIfPlayerLevel(Task* task)
{
    enum { ACTOR_01900_BATTLE_HEIGHT_RANGE = 500 };

    Task* player;
    s32   heightDifference;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        heightDifference = task->extra.tmd->coords->coord.t[1] - player->extra.tmd->coords->coord.t[1];
        if (ABS(heightDifference) < ACTOR_01900_BATTLE_HEIGHT_RANGE) {
            sceneEngageBattle(1);
            return 1;
        }
    }
    return 0;
}

/// Plays the alert and turns toward the player before starting pursuit.
///
/// Entry enables model drawing, disables attack and room-grid response and
/// engages battle outside the cached Patio command namespace. Later ticks turn
/// by at most 16/4096 of a turn, retaining the full player turn for look yaw.
/// A settled alert pose selects `ACTOR_01900_STATE_CHASE`. Requires initialized
/// work and live actor/player roots in the same parent frame.
static void _actor01900StateAlert(Task* task)
{
    enum {
        ACTOR_01900_ANIM_ALERT      = 9,
        ACTOR_01900_ALERT_TURN_STEP = ACTOR_TRANSFORM_ANGLE_TURN / 256
    };

    _Actor01900Work*   work;
    ActorChaseScratch* turnScratch;
    GfxCoord*          rootCoord;
    TmdObject*         model;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->animRequest      = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate         = ANIMATION_RATE_ONE;
        work->animId           = ACTOR_01900_ANIM_ALERT;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        _actor01900UpdateAnimation(task);
        work->hitBody.radius = ACTOR_01900_BODY_RADIUS;
        if (*(u16*)work->commandBytes != ((GAME_AREA_ACROPOLIS_PATIO << 8) | GAME_STAGE_ACROPOLIS)) {
            _actor01900EngageBattleIfPlayerLevel(task);
        }
    } else {
        SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
        turnScratch                           = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->state = ACTOR_01900_STATE_CHASE;
        }
        turnScratch->turn   = _actorAngleTurnToPlayer(task, &turnScratch->delta, &gPlayerStatus);
        work->lookYawTarget = turnScratch->turn;
        if (turnScratch->turn >= ACTOR_01900_ALERT_TURN_STEP + 1) {
            turnScratch->turn = ACTOR_01900_ALERT_TURN_STEP;
        }
        if (turnScratch->turn < -ACTOR_01900_ALERT_TURN_STEP) {
            turnScratch->turn = -ACTOR_01900_ALERT_TURN_STEP;
        }
        rootCoord          = task->extra.tmd->coords;
        turnScratch->turn += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
        gfxRotMatrixY(&task->extra.tmd->coords->coord, turnScratch->turn, GRAPHICS_ROTATION_REPLACE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
        _actor01900UpdateAnimation(task);
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
    }
}

/// Clamps the root's parent-space Y translation for a configured stage and area.
///
/// Scans the two active rows, ignoring view and game mode. The first matching row
/// sets an inclusive minimum/maximum; no match leaves translation unchanged.
/// The caller must invalidate composition after a change.
static void _actor01900ClampRootHeight(const GameLocationKey* location, GfxCoord* coord)
{
    const ActorHeightClamp* row;
    s32                     rootY;
    s32                     minY;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_01900_HEIGHT_CLAMP_COUNT; rowIndex++) {
        row = &Actor01900_D172CC[rowIndex];
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

/// Returns 1 when the stage and area have an active height-clamp row, else 0.
///
/// View and game mode do not participate; the terminal zero row is not scanned.
static __inline__ s32 _actor01900HasHeightClamp(const GameLocationKey* location)
{
    const ActorHeightClamp* row;
    s16                     rowIndex;

    for (rowIndex = 0; rowIndex < ACTOR_01900_HEIGHT_CLAMP_COUNT; rowIndex++) {
        row = &Actor01900_D172CC[rowIndex];
        if (location->stage == row->stage && location->area == row->area) {
            return 1;
        }
    }
    return 0;
}

/// Applies capped room-grid correction and the configured height adjustment.
///
/// Borrows `contactCount` readable records, in 1..12, and a writable coordinate
/// in their room frame. The resolver writes 16.16 corrections; X/Z whole units
/// are capped to a 192-unit step, then fractional remainders add signed units.
/// In height-clamped rooms, Y correction is capped to 384 units, the coordinate
/// is clamped and `heightOffset` parent-space units are added even without a hit.
/// Returns 1 for nonzero raw X/Z correction, else 0; vertical-only changes return
/// 0. Freeze equal to 1 skips all work. Does not invalidate composition. Reserves
/// one scratch block plus nested resolver space and retains no pointers.
static s32 _actor01900ApplyCappedGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount, s16 heightOffset)
{
    enum {
        ACTOR_01900_GRID_VERTICAL_LIMIT   = 384,
        ACTOR_01900_GRID_HORIZONTAL_LIMIT = 192,
        ACTOR_01900_FIXED_FRACTION_BITS   = 16,
        ACTOR_01900_FIXED_FRACTION_MASK   = 0xFFFF
    };

    ActorContactCappedPushScratch* scratchEnd;
    ActorContactCappedPushScratch* scratch;
    s16                            verticalStep;
    SVECTOR*                       displacement;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    scratchEnd                                          = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = scratchEnd - 1;
    scratch                                             = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    scratch->moved                                      = 0;
    if (worldCollisionResolvePushback(contacts, &scratch->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        // Apply whole 16.16 correction units, capping the horizontal displacement.
        scratch->step.vx = scratchEnd[-1].delta.fixed.vx.word >> ACTOR_01900_FIXED_FRACTION_BITS;
        scratch->step.vy = scratch->delta.fixed.vy.word >> ACTOR_01900_FIXED_FRACTION_BITS;
        scratch->step.vz = scratch->delta.fixed.vz.word >> ACTOR_01900_FIXED_FRACTION_BITS;
        if (_actor01900HasHeightClamp(&gGameSession->location.loc)) {
            verticalStep = scratch->step.vy;
            if (((verticalStep >= 0) ? verticalStep : -verticalStep) > ACTOR_01900_GRID_VERTICAL_LIMIT) {
                scratch->step.vy = (verticalStep <= 0) ? -ACTOR_01900_GRID_VERTICAL_LIMIT : ACTOR_01900_GRID_VERTICAL_LIMIT;
            }
        }
        coord->coord.t[1]  += scratch->step.vy;
        scratch->stepLength = scratch->step.vx * scratch->step.vx + scratch->step.vz * scratch->step.vz;
        scratch->stepLength = SquareRoot0(scratch->stepLength);
        displacement        = &scratch->step;
        if (scratch->stepLength >= ACTOR_01900_GRID_HORIZONTAL_LIMIT) {
            scratch->step.vy = 0;
            _actorMovementBuildDisplacement(displacement, ACTOR_01900_GRID_HORIZONTAL_LIMIT);
            coord->coord.t[0] += scratch->step.vx;
            coord->coord.t[2] += scratch->step.vz;
        } else {
            coord->coord.t[0] += scratch->step.vx;
            coord->coord.t[2] += scratch->step.vz;
        }
        // Preserve the one-unit signed correction for each fractional X/Z remainder.
        if (scratch->delta.fixed.vx.word & ACTOR_01900_FIXED_FRACTION_MASK) {
            if (scratch->delta.fixed.vx.word > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (scratch->delta.fixed.vz.word & ACTOR_01900_FIXED_FRACTION_MASK) {
            if (scratch->delta.fixed.vz.word > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (_actor01900HasHeightClamp(&gGameSession->location.loc)) {
        _actor01900ClampRootHeight(&gGameSession->location.loc, coord);
        coord->coord.t[1] += heightOffset;
    }
    if (scratch->delta.fixed.vx.word != 0 || scratch->delta.fixed.vz.word != 0) {
        scratch->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return scratch->moved;
}

/// Pushes the root away from contacted player and enemy bodies.
///
/// `contacts` borrows `contactCount` records, with `contactCount` in 0..12. A zero key
/// ends the scan. Each qualifying record contributes half its X/Z offset, capped
/// to 192 units before halving; Y translation is untouched. The measurement
/// origin is model coordinate 1 composed into view space. Root translations must
/// use those same axes. Returns 1 when a qualifying record was processed, even
/// for a zero offset, otherwise 0. Freeze or viewReady equal to 1 skips the scan.
/// The scratch stack needs one `ActorBodyPushScratch` block; no pointer is retained.
static s32 _actor01900ApplyBodyPushback(Task* task, const WorldCollisionContact* contacts, s16 contactCount)
{
    enum { ACTOR_01900_BODY_PUSH_LIMIT = 192 };

    ActorBodyPushScratch* scratch;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    scratch                                 = SCRATCH_STACK_RESERVE_BLOCK(ActorBodyPushScratch);
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    scratch->position.vx = task->extra.tmd->coords[1].workm.t[0];
    scratch->position.vy = task->extra.tmd->coords[1].workm.t[1];
    scratch->position.vz = task->extra.tmd->coords[1].workm.t[2];
    scratch->hit         = 0;
    for (scratch->recordIndex = 0; scratch->recordIndex < contactCount; scratch->recordIndex++) {
        if (contacts[scratch->recordIndex].key.value == 0) {
            scratch->marks[scratch->recordIndex] = ACTOR_BODY_PUSH_MARK_END;
            break;
        }
        scratch->kind = contacts[scratch->recordIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if (scratch->kind == WORLD_COLLISION_CONTACT_PLAYER_BODY || scratch->kind == WORLD_COLLISION_CONTACT_ENEMY_BODY) {
            scratch->hit = 1;
            worldCollisionCalcContactWorldOffset(&scratch->position, &contacts[scratch->recordIndex], &scratch->offset);
            scratch->offsetLength = scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz;
            scratch->offsetLength = SquareRoot0(scratch->offsetLength);
            if (scratch->offsetLength >= ACTOR_01900_BODY_PUSH_LIMIT) {
                scratch->offset.vy = 0;
                _actorMovementBuildDisplacement(&scratch->offset, ACTOR_01900_BODY_PUSH_LIMIT);
                task->extra.tmd->coords->coord.t[0] += scratch->offset.vx / 2;
                task->extra.tmd->coords->coord.t[2] += scratch->offset.vz / 2;
            } else {
                task->extra.tmd->coords->coord.t[0] += scratch->offset.vx / 2;
                task->extra.tmd->coords->coord.t[2] += scratch->offset.vz / 2;
            }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return scratch->hit;
}

/// Pursues the player, selecting a sidestep, strike or repeated alert.
///
/// Entry enables drawing and grid response, starts the run clip and resets its
/// timers. Subsequent ticks resolve contacts before steering and forward movement.
/// Clear sight permits attack and repeated-alert transitions; blocked sight adds
/// a signed veer. The strike turn test retains its one-sided comparison.
/// Requires initialized work and live actor/player roots in the same parent frame.
static void _actor01900StateChase(Task* task)
{
    enum {
        ACTOR_01900_CHASE_RUN_RATE            = 66,
        ACTOR_01900_CHASE_STEP_DISTANCE       = 40,
        ACTOR_01900_CHASE_BLEND_STEP_DISTANCE = 10,
        ACTOR_01900_CHASE_HIT_BODY_GRID_MODE  = 0x10,
        ACTOR_01900_CHASE_HEIGHT_OFFSET       = 96,
        ACTOR_01900_CHASE_PLAYER_FACING_LIMIT = 68,
        ACTOR_01900_CHASE_SIDESTEP_TURN_LIMIT = 128,
        ACTOR_01900_CHASE_TURN_STEP           = 48,
        ACTOR_01900_CHASE_SIDESTEP_RANGE      = 1800,
        ACTOR_01900_CHASE_STRIKE_RANGE        = 700,
        ACTOR_01900_CHASE_STRIKE_TURN_LIMIT   = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_01900_CHASE_ALERT_TICKS         = 91,
        ACTOR_01900_CHASE_SIDE_SWITCH_TICKS   = 241,
        ACTOR_01900_CHASE_BLOCKED_TURN_BIAS   = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 16
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          facingCoord;
    ActorChaseScratch* chase;
    s32                playerFacingError;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ACTOR_01900_CHASE_RUN_RATE;
        work->animId            = ACTOR_01900_ANIM_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        work->circleCount = 0;
        if (*(u16*)work->commandBytes != ((GAME_AREA_ACROPOLIS_PATIO << 8) | GAME_STAGE_ACROPOLIS)) {
            _actor01900EngageBattleIfPlayerLevel(task);
        }
        work->stateTimer   = 0;
        work->stateCounter = 0;
        if ((task->spawnArg1.value >> 16) == ACTOR_01900_CHASE_HIT_BODY_GRID_MODE) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    work->stateTimer++;
    work->stateCounter++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    // Resolve the grid before body overlaps, then choose pursuit or evasion.
    if (_actor01900ApplyCappedGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), ACTOR_01900_CHASE_HEIGHT_OFFSET) != 1) {
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
            _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor01900UpdateAnimation(task);
    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
    rootCoord            = task->extra.tmd->coords;
    chase->turn          = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    work->lookYawTarget  = chase->turn;
    playerFacingError    = chase->yawFromPlayer - chase->playerYaw;
    if (ABS(playerFacingError) < ACTOR_01900_CHASE_PLAYER_FACING_LIMIT && work->sidestepDelay + work->sidestepCount / 2 < work->stateTimer && ABS(chase->turn) < ACTOR_01900_CHASE_SIDESTEP_TURN_LIMIT) {
        if (_actorRangeOutsideRadiusXZ(&chase->delta, ACTOR_01900_CHASE_SIDESTEP_RANGE)) {
            work->state = ACTOR_01900_STATE_SIDESTEP;
        }
    }
    if (_playerDetectionSightBlocked(task) != 1) {
        work->stateTimer++;
        rootCoord           = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn < ACTOR_01900_CHASE_STRIKE_TURN_LIMIT) {
            if (!_actorRangeOutsideRadiusXZ(&chase->delta, ACTOR_01900_CHASE_STRIKE_RANGE)) {
                work->state = ACTOR_01900_STATE_STRIKE;
            }
        }
        if (work->stateCounter >= ACTOR_01900_CHASE_ALERT_TICKS) {
            work->state = ACTOR_01900_STATE_ALERT_REPEAT;
        }
    } else {
        work->stateTimer    = 0;
        work->stateCounter  = 0;
        rootCoord           = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = -1;
            } else {
                work->sidestepSide = 1;
            }
        }
        if (work->sidestepSide == 1) {
            chase->turn += ACTOR_01900_CHASE_BLOCKED_TURN_BIAS;
        } else {
            chase->turn -= ACTOR_01900_CHASE_BLOCKED_TURN_BIAS;
        }
        if (work->stateTimer >= ACTOR_01900_CHASE_SIDE_SWITCH_TICKS) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    // Steer the root and rebuild its uniform scale before translating it.
    if (chase->turn > ACTOR_01900_CHASE_TURN_STEP) {
        chase->turn = ACTOR_01900_CHASE_TURN_STEP;
    }
    if (chase->turn < -ACTOR_01900_CHASE_TURN_STEP) {
        chase->turn = -ACTOR_01900_CHASE_TURN_STEP;
    }
    facingCoord  = task->extra.tmd->coords;
    chase->turn += ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ACTOR_01900_ANIM_RUN) {
        if (work->blendActive == 0) {
            _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_CHASE_STEP_DISTANCE);
        } else {
            _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_CHASE_BLEND_STEP_DISTANCE);
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->animId      = ACTOR_01900_ANIM_RUN;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
    }
    if (work->field_C37 != 0) {
        work->field_C37--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Measures the player's heading and wrapped bearing back to this actor.
///
/// `scratch->delta` is the signed-halfword player-minus-actor offset in the live
/// roots' common parent frame. The registered player must have a live model root.
/// Writes `playerYaw` and `yawFromPlayer` in 4096ths of a turn, the latter in
/// [-2048, 2048]. Borrows caller-owned scratch without reserving or retaining it.
static inline void _actor01900MeasurePlayerFacing(ActorChaseScratch* scratch)
{
    scratch->playerYaw     = ratan2(-gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0],
                                    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
    scratch->yawFromPlayer = ratan2(scratch->delta.vx, scratch->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    scratch->yawFromPlayer = _actorAngleNormalizeYaw(scratch->yawFromPlayer);
}

/// Runs around the player while accelerating and then easing its animation rate.
///
/// Steers toward a heading about 1000/4096 turn from the player bearing, with
/// each turn limited to 96/4096. Grid corrections count toward a slide; after
/// deceleration, a player facing within a quarter-turn can also select slide.
/// Forward distance follows the animation rate, halves while blending and drops
/// to two units after a grid correction. Requires initialized work and live roots.
static void _actor01900StateCircle(Task* task)
{
    enum {
        ACTOR_01900_CIRCLE_ACCELERATION        = 8,
        ACTOR_01900_CIRCLE_DECELERATION        = -1,
        ACTOR_01900_CIRCLE_PEAK_RATE           = 24,
        ACTOR_01900_CIRCLE_CRUISE_RATE         = 18,
        ACTOR_01900_CIRCLE_COAST_TICKS         = 5,
        ACTOR_01900_CIRCLE_GRID_PUSH_LIMIT     = 7,
        ACTOR_01900_STATE_REENTER              = -1,
        ACTOR_01900_CIRCLE_TANGENT_YAW         = 1000,
        ACTOR_01900_CIRCLE_TURN_STEP           = 96,
        ACTOR_01900_CIRCLE_PLAYER_FACING_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          facingCoord;
    ActorChaseScratch* chase;
    s32                playerTurn;
    s32                positiveTangentError;
    s32                negativeTangentError;
    s32                playerFacingError;
    s32                normalizedPlayerBearing;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_COMPACT_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animId            = ACTOR_01900_ANIM_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        work->circleRateStep = ACTOR_01900_CIRCLE_ACCELERATION;
        work->stateTimer     = 0;
        work->stateCounter   = 0;
        Actor01900_D172FC    = 0;
        work->circleCount++;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor01900UpdateAnimation(task);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
        work->stateCounter++;
    } else {
        _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    if (work->stateCounter >= ACTOR_01900_CIRCLE_GRID_PUSH_LIMIT) {
        _actor01900MeasurePlayerFacing(chase);
        work->state = ACTOR_01900_STATE_SLIDE;
    }
    rootCoord   = task->extra.tmd->coords;
    chase->turn = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    // Stay approximately a quarter-turn from the player bearing to circle them.
    playerTurn = chase->turn;
    if (playerTurn >= 0) {
        positiveTangentError = playerTurn - ACTOR_01900_CIRCLE_TANGENT_YAW;
        if (((positiveTangentError < 0) ? -positiveTangentError : positiveTangentError) < ACTOR_01900_CIRCLE_TURN_STEP) {
            chase->heading = chase->turn - ACTOR_01900_CIRCLE_TANGENT_YAW;
        } else if (positiveTangentError > 0) {
            chase->heading = ACTOR_01900_CIRCLE_TURN_STEP;
        } else {
            chase->heading = -ACTOR_01900_CIRCLE_TURN_STEP;
        }
    } else {
        negativeTangentError = playerTurn + ACTOR_01900_CIRCLE_TANGENT_YAW;
        if (((negativeTangentError < 0) ? -negativeTangentError : negativeTangentError) < ACTOR_01900_CIRCLE_TURN_STEP) {
            chase->heading = chase->turn + ACTOR_01900_CIRCLE_TANGENT_YAW;
        } else if (negativeTangentError > 0) {
            chase->heading = ACTOR_01900_CIRCLE_TURN_STEP;
        } else {
            chase->heading = -ACTOR_01900_CIRCLE_TURN_STEP;
        }
    }
    facingCoord     = task->extra.tmd->coords;
    chase->heading += ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->heading, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    rootCoord                             = task->extra.tmd->coords;
    work->lookYawTarget                   = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->runStep                         = work->animRate * 8;
    if (work->blendActive != 0) {
        work->runStep = work->runStep >> 1;
    }
    if (work->stateCounter != 0) {
        work->runStep = 2;
    }
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, work->runStep);
    Actor01900_D172FC += work->runStep;
    // Accelerate, ease back to the cruising rate, then test whether to slide.
    if (work->circleRateStep == ACTOR_01900_CIRCLE_ACCELERATION && work->animRate >= ACTOR_01900_CIRCLE_PEAK_RATE) {
        work->circleRateStep = ACTOR_01900_CIRCLE_DECELERATION;
    }
    if (work->circleRateStep == ACTOR_01900_CIRCLE_DECELERATION && work->animRate == ACTOR_01900_CIRCLE_CRUISE_RATE) {
        work->circleRateStep = 0;
        work->stateTimer     = 0;
    }
    if (work->circleRateStep == 0) {
        if (++work->stateTimer == ACTOR_01900_CIRCLE_COAST_TICKS) {
            chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
            chase->yawFromPlayer    = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            normalizedPlayerBearing = _actorAngleNormalizeYaw(chase->yawFromPlayer);
            chase->yawFromPlayer    = normalizedPlayerBearing;
            playerFacingError       = normalizedPlayerBearing - chase->playerYaw;
            if (playerFacingError < 0) {
                playerFacingError = -playerFacingError;
            }
            if (playerFacingError <= ACTOR_01900_CIRCLE_PLAYER_FACING_LIMIT) {
                work->state     = ACTOR_01900_STATE_SLIDE;
                work->prevState = ACTOR_01900_STATE_REENTER;
            }
        }
    }
    work->animRate += work->circleRateStep;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turns through the player bearing to a reflected heading before circling.
///
/// Entry stores the root yaw and a signed-halfword target equal to that yaw plus
/// twice the relative player turn. Ticks approach it by 137/4096 turn while moving
/// and resolving contacts. At the target, fewer than two circles or range
/// at least 900 parent-space units selects circle. Yaw comparison retains signed linear
/// ordering without wrapping the difference. Requires initialized work and live roots.
static void _actor01900StateTurnAround(Task* task)
{
    enum {
        ACTOR_01900_TURN_AROUND_STEP_DISTANCE       = 40,
        ACTOR_01900_TURN_AROUND_BLEND_STEP_DISTANCE = 20,
        ACTOR_01900_TURN_AROUND_STEP                = 137,
        ACTOR_01900_TURN_AROUND_CIRCLE_RANGE        = 900
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          facingCoord;
    ActorChaseScratch* scratchEnd;
    ActorChaseScratch* turnScratch;

    work = task->work;
    if (work->stateEntered != 0) {
        scratchEnd                              = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        model                                   = task->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch) = scratchEnd - 1;
        turnScratch                             = scratchEnd - 1;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_ANIM_RUN;
        work->blendActive       = 0;
        work->lookYawTarget     = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &turnScratch->delta);
        rootCoord            = task->extra.tmd->coords;
        turnScratch->turn    = _actorAngleNormalizeYaw(ratan2(scratchEnd[-1].delta.vx, turnScratch->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
        facingCoord          = task->extra.tmd->coords;
        turnScratch->heading = ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
        work->turnYaw        = turnScratch->heading;
        // Reflect the initial heading about the player bearing, preserving halfword wrap.
        work->turnYawTarget = turnScratch->heading + (u16)turnScratch->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    scratchEnd                              = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = scratchEnd - 1;
    turnScratch                             = scratchEnd - 1;
    _actor01900UpdateAnimation(task);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &turnScratch->delta);
    if (work->turnYaw == work->turnYawTarget) {
        if (work->circleCount < 2 || _actorRangeOutsideRadiusXZ(&turnScratch->delta, ACTOR_01900_TURN_AROUND_CIRCLE_RANGE)) {
            work->state = ACTOR_01900_STATE_CIRCLE;
        }
    }
    if (work->turnYaw > work->turnYawTarget) {
        work->turnYaw -= ACTOR_01900_TURN_AROUND_STEP;
        if (work->turnYaw < work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    if (work->turnYaw < work->turnYawTarget) {
        work->turnYaw += ACTOR_01900_TURN_AROUND_STEP;
        if (work->turnYaw > work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->turnYaw, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_TURN_AROUND_STEP_DISTANCE);
    } else {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_TURN_AROUND_BLEND_STEP_DISTANCE);
    }
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Hops along a side-selected direction, then restarts pursuit.
///
/// Entry alternates the side and builds a normalized direction from the player
/// bearing, variant lean and first-hop extra yaw. `stateTimer` 12..21 moves by the stored
/// distance, halved while blending; grid correction halves later steps. Tick 30
/// selects chase and forces state re-entry. Scratch delta changes from player
/// offset to halfword displacement. Requires initialized work and live roots.
static void _actor01900StateSidestep(Task* task)
{
    enum {
        ACTOR_01900_ANIM_SIDESTEP_NEGATIVE   = 20,
        ACTOR_01900_ANIM_SIDESTEP_POSITIVE   = 21,
        ACTOR_01900_FIRST_SIDESTEP_EXTRA_YAW = 369,
        ACTOR_01900_SIDESTEP_DISTANCE        = 222,
        ACTOR_01900_SIDESTEP_MOVE_FIRST_TICK = 12,
        ACTOR_01900_SIDESTEP_MOVE_TICKS      = 10U,
        ACTOR_01900_SIDESTEP_TOTAL_TICKS     = 30,
        ACTOR_01900_STATE_REENTER            = -1
    };

    _Actor01900Work*   work;
    ActorChaseScratch* scratchEnd;
    ActorChaseScratch* sidestepScratch;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    SVECTOR*           sidestepDirection;
    MATRIX             sidestepRotation;
    u16                firstSidestepYaw;

    scratchEnd                              = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    work                                    = task->work;
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = scratchEnd - 1;
    sidestepScratch                         = scratchEnd - 1;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_COMPACT_BODY_RADIUS;
        work->stateTimer        = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &sidestepScratch->delta);
        sidestepScratch->turn = ratan2(scratchEnd[-1].delta.vx, sidestepScratch->delta.vz);
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = 1;
            } else {
                work->sidestepSide = -1;
            }
        }
        if (work->sidestepSide == 1) {
            work->animId = ACTOR_01900_ANIM_SIDESTEP_POSITIVE;
            if (work->sidestepCount == 0) {
                firstSidestepYaw      = sidestepScratch->turn + ACTOR_01900_FIRST_SIDESTEP_EXTRA_YAW;
                sidestepScratch->turn = work->sidestepAngle + firstSidestepYaw;
            } else {
                sidestepScratch->turn += work->sidestepAngle;
            }
            work->sidestepSide = -1;
        } else {
            work->animId = ACTOR_01900_ANIM_SIDESTEP_NEGATIVE;
            if (work->sidestepCount == 0) {
                firstSidestepYaw      = sidestepScratch->turn - ACTOR_01900_FIRST_SIDESTEP_EXTRA_YAW;
                sidestepScratch->turn = firstSidestepYaw - work->sidestepAngle;
            } else {
                sidestepScratch->turn -= work->sidestepAngle;
            }
            work->sidestepSide = 1;
        }
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate    = 3 * ANIMATION_RATE_ONE / 4;
        work->blendActive = 0;
        _actor01900UpdateAnimation(task);
        gfxRotMatrixY(&sidestepRotation, sidestepScratch->turn, GRAPHICS_ROTATION_REPLACE);
        sidestepDirection = &work->sidestepDir;
        gfxReadMatrixZAxis(&sidestepRotation, sidestepDirection);
        VectorNormalSS(sidestepDirection, sidestepDirection);
        work->sidestepStep = ACTOR_01900_SIDESTEP_DISTANCE;
        work->sidestepCount++;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor01900UpdateAnimation(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestepScratch->delta);
    } else {
        gte_lddp(work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestepScratch->delta);
    }
    // Only ticks 12..21 translate; a grid push halves each subsequent step.
    if ((u32)((u16)work->stateTimer - ACTOR_01900_SIDESTEP_MOVE_FIRST_TICK) < ACTOR_01900_SIDESTEP_MOVE_TICKS) {
        rootCoord              = task->extra.tmd->coords;
        rootCoord->coord.t[0] += sidestepScratch->delta.vx;
        rootCoord              = task->extra.tmd->coords;
        rootCoord->coord.t[2] += sidestepScratch->delta.vz;
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
            work->sidestepStep >>= 1;
        }
    }
    if (++work->stateTimer >= ACTOR_01900_SIDESTEP_TOTAL_TICKS) {
        work->state     = ACTOR_01900_STATE_CHASE;
        work->prevState = ACTOR_01900_STATE_REENTER;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Retreats during the step-back clip, then sidesteps if targeted or alerts otherwise.
///
/// Requires live initialized work, enemy and model. The backward step is 120
/// parent-space units on pose cues 16..22; grid contacts correct each step.
/// The clip's settled flag selects the next state. Entry does not rebuild buffers.
static void _actor01900StateStepBack(Task* task)
{
    enum {
        ACTOR_01900_STEP_BACK_CLIP      = 7,
        ACTOR_01900_STEP_BACK_FIRST_CUE = 16,
        ACTOR_01900_STEP_BACK_CUE_COUNT = 7,
        ACTOR_01900_STEP_BACK_DISTANCE  = -120
    };

    _Actor01900Work* work;
    Enemy*           enemy;
    GfxCoord*        rootCoord;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->animRate    = ANIMATION_RATE_ONE;
        work->animId      = ACTOR_01900_STEP_BACK_CLIP;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
        work->stateTimer  = 0;
    }
    _actor01900UpdateAnimation(task);
    // Retreat only while the low-ten-bit pose cue is in 16..22.
    if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - ACTOR_01900_STEP_BACK_FIRST_CUE) < ACTOR_01900_STEP_BACK_CUE_COUNT) {
        rootCoord = task->extra.tmd->coords;
        _actorMovementStepForward(rootCoord, ACTOR_01900_STEP_BACK_DISTANCE);
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (enemy->node.state.parts.targeted == 1) {
            work->state = ACTOR_01900_STATE_SIDESTEP;
        } else {
            work->state = ACTOR_01900_STATE_ALERT;
        }
    }
}

/// Runs toward the player with a half-rate animation request and no state transition.
///
/// Requires initialized work, enemy, model and player roots in a common parent
/// frame, plus the scratch stack. Turns by at most 64/4096 turn per tick and
/// steps 40 parent-space units, or 20 while blending, after resolving contacts.
static void _actor01900StateApproach(Task* task)
{
    enum {
        ACTOR_01900_APPROACH_TURN_STEP        = 64,
        ACTOR_01900_APPROACH_STEP             = 40,
        ACTOR_01900_APPROACH_BLEND_STEP       = 20,
        ACTOR_01900_APPROACH_PROBE_TURN_LIMIT = 512,
        ACTOR_01900_APPROACH_PROBE_RADIUS     = 900
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          facingCoord;
    ActorChaseScratch* chase;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE / 2;
        work->animId            = ACTOR_01900_ANIM_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        work->circleCount = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != true) {
        _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor01900UpdateAnimation(task);
    rootCoord           = task->extra.tmd->coords;
    chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    work->lookYawTarget = chase->turn;
    // This retained range probe does not select a state or gate movement.
    if (chase->turn < ACTOR_01900_APPROACH_PROBE_TURN_LIMIT) {
        _actorRangeOutsideRadiusXZ(&chase->delta, ACTOR_01900_APPROACH_PROBE_RADIUS);
    }
    if (chase->turn > ACTOR_01900_APPROACH_TURN_STEP) {
        chase->turn = ACTOR_01900_APPROACH_TURN_STEP;
    }
    if (chase->turn < -ACTOR_01900_APPROACH_TURN_STEP) {
        chase->turn = -ACTOR_01900_APPROACH_TURN_STEP;
    }
    facingCoord  = task->extra.tmd->coords;
    chase->turn += ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_APPROACH_STEP);
    } else {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_APPROACH_BLEND_STEP);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Staggers backward and falls, then selects down, status hold or death burn.
///
/// Requires initialized work, enemy and model. The stagger steps backward by 87
/// parent-space units per tick. Both contact tables correct the root until the
/// down clip settles; then hit-body grid tests are disabled. Entry alerts only
/// for negative HP and three independent cached-command byte mismatches.
static void _actor01900StateFall(Task* task)
{
    enum {
        ACTOR_01900_FALL_NO_ALERT_COMMAND_BYTE = 2,
        ACTOR_01900_FALL_BACKWARD_STEP         = -87
    };

    _Actor01900Work* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ACTOR_01900_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_01900_ANIM_FALL;
        work->blendActive             = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        // Alert requires all three cached command bytes to differ independently.
        if (enemy->hp < 0 && work->commandBytes[0] != GAME_STAGE_ACROPOLIS && work->commandBytes[1] != GAME_AREA_ACROPOLIS_PATIO && work->commandBytes[2] != ACTOR_01900_FALL_NO_ALERT_COMMAND_BYTE) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->animId == ACTOR_01900_ANIM_FALL) {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_FALL_BACKWARD_STEP);
    }
    _actor01900UpdateAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Finish the stagger, then the down clip, before choosing recovery or death.
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (work->animId == ACTOR_01900_ANIM_FALL) {
            work->animId      = ACTOR_01900_ANIM_DOWN;
            work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
            _actor01900UpdateAnimation(task);
        }
        if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) && work->animId == ACTOR_01900_ANIM_DOWN) {
            work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                    work->state = ACTOR_01900_STATE_STATUS_HOLD;
                } else {
                    work->state = ACTOR_01900_STATE_DOWN;
                }
            } else {
                work->state = ACTOR_01900_STATE_DEATH_BURN;
            }
        }
    }
}

static void Actor01900_Fn06904(Task* arg0)
{
    _Actor01900Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    s16              cur;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer              = 0;
    }
    if (work->stateTimer < 0x401) {
        switch ((s16)(work->stateTimer++ - 0x18)) {
            case 0:
                sceneReleaseBattleRefWithRewards(arg0, 0x13);
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
            _actorRenderRescaleYawY(arg0->extra.tmd->coords, ACTOR_01900_ROOT_SCALE, ACTOR_01900_ROOT_SCALE - (cur - 0x14) * 0xB);
        }
    }
}

/// Arms `gSceneCombatState` and returns 1 when the player is within 500 units of the
/// actor's height (and not in `field_954` state 2).
static void Actor01900_Fn06B4C(Task* arg0)
{
    SVECTOR          delta;
    SVECTOR*         d;
    _Actor01900Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              sound;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                   = arg0->extra.tmd;
        Actor01900_D17174[16] = &_gActor01900Actor101900Animation16960;
        work->animId          = 0x10;
        work->animRequest     = ACTOR_01900_ANIM_REQUEST_RESET;
        obj->flags            = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = 0x180;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x10;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        work->dormantAnimFrame        = 0;
    }
    _actor01900UpdateAnimation(arg0);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xF && work->dormantAnimFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) &&
        (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 9, 0, 0)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sound           = 0x51090009;
        if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
            sound = 0x51090008;
        }
        switch ((u8)viewGetMappedIndex()) {
            case 2:
                sndEvtRequestScriptStart(sound, 0x64, 0);
                break;
            case 3:
                sndEvtRequestScriptStart(sound, 0x50, 0x1F);
                break;
            case 4:
            default:
                sndEvtRequestScriptStart(sound, 0x40, 0x4C);
                break;
        }
    }
    if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 5 && work->dormantAnimFrame != (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF)) {
        work->effectArg.coord      = arg0->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = 0x200;
        work->effectArg.spawnArgHi = 2;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 3, 0, 0) || (u8)viewGetMappedIndex() != 0x10) {
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->effectArg);
        }
    }
    work->dormantAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord                  = arg0->extra.tmd->coords;
    d                      = &delta;
    delta.vx               = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy                  = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz                  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(d, work->noticeRange)) {
        sndEvtRequestScriptStop(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        if (_actor01900EngageBattleIfPlayerLevel(arg0) == 1) {
            work->state = ACTOR_01900_STATE_ALERT;
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->state = ACTOR_01900_STATE_ALERT;
    }
}

/// Walks between the spawn waypoint pair while watching for the player.
///
/// Requires initialized work, enemy, model, player roots in a common parent frame
/// and available scratch storage. `patrolTarget` is 0 or 1. Arrival within 160
/// units or 21 aligned grid-correction ticks swaps it. Turns by at most 32/4096
/// per tick and steps ten units unless blending. Visible proximity can engage
/// battle and alert; combat attack signals alert independently.
static void _actor01900StatePatrol(Task* task)
{
    enum {
        ACTOR_01900_PATROL_SPAWN_MODE_SHIFT    = 16,
        ACTOR_01900_PATROL_HIT_BODY_GRID_MODE  = 16,
        ACTOR_01900_PATROL_ARRIVAL_RADIUS      = 160,
        ACTOR_01900_PATROL_BLOCKED_TICKS       = 21,
        ACTOR_01900_PATROL_TURN_STEP           = 32,
        ACTOR_01900_PATROL_STEP                = 10,
        ACTOR_01900_PATROL_BLOCKED_YAW_LIMIT   = 128,
        ACTOR_01900_PATROL_FRONT_NOTICE_RADIUS = 4000,
        ACTOR_01900_PATROL_FRONT_YAW_LIMIT     = 768
    };

    _Actor01900Work*  work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    ActorTurnScratch* turnScratch;
    GfxCoord*         facingCoord;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        work->stateTimer = 0;
        if ((task->spawnArg1.value >> ACTOR_01900_PATROL_SPAWN_MODE_SHIFT) == ACTOR_01900_PATROL_HIT_BODY_GRID_MODE) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turnScratch           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turnScratch->delta.vx = work->patrolPoints[work->patrolTarget].x - task->extra.tmd->coords->coord.t[0];
    turnScratch->delta.vy = 0;
    turnScratch->delta.vz = work->patrolPoints[work->patrolTarget].z - task->extra.tmd->coords->coord.t[2];
    // Swap destinations on arrival or after 21 aligned grid-correction ticks.
    if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, ACTOR_01900_PATROL_ARRIVAL_RADIUS) || work->stateTimer >= ACTOR_01900_PATROL_BLOCKED_TICKS) {
        if (work->patrolTarget == 0) {
            work->patrolTarget = 1;
        } else {
            work->patrolTarget = 0;
        }
        work->stateTimer = 0;
    }
    _actor01900UpdateAnimation(task);
    rootCoord           = task->extra.tmd->coords;
    turnScratch->angle  = _actorAngleNormalizeYaw(ratan2(turnScratch->delta.vx, turnScratch->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    work->lookYawTarget = turnScratch->angle;
    if (turnScratch->angle > ACTOR_01900_PATROL_TURN_STEP) {
        turnScratch->angle = ACTOR_01900_PATROL_TURN_STEP;
    }
    if (turnScratch->angle < -ACTOR_01900_PATROL_TURN_STEP) {
        turnScratch->angle = -ACTOR_01900_PATROL_TURN_STEP;
    }
    facingCoord         = task->extra.tmd->coords;
    turnScratch->angle += ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turnScratch->angle, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    if (work->blendActive == 0) {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_PATROL_STEP);
    }
    if ((task->spawnArg1.value >> ACTOR_01900_PATROL_SPAWN_MODE_SHIFT) != ACTOR_01900_PATROL_HIT_BODY_GRID_MODE) {
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == true && ABS(work->lookYawTarget) < ACTOR_01900_PATROL_BLOCKED_YAW_LIMIT) {
            work->stateTimer++;
        } else {
            _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    } else {
        if ((_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == true ||
             _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == true) &&
            ABS(work->lookYawTarget) < ACTOR_01900_PATROL_BLOCKED_YAW_LIMIT) {
            work->stateTimer++;
        } else {
            _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Sight gates proximity detection; combat signals can alert independently.
    if (_playerDetectionSightBlocked(task) != 1) {
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &turnScratch->delta);
        if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, work->noticeRange)) {
            if (_actor01900EngageBattleIfPlayerLevel(task) == 1) {
                work->state = ACTOR_01900_STATE_ALERT;
            }
        } else if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, ACTOR_01900_PATROL_FRONT_NOTICE_RADIUS)) {
            rootCoord          = task->extra.tmd->coords;
            turnScratch->angle = _actorAngleNormalizeYaw(ratan2(turnScratch->delta.vx, turnScratch->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
            if (ABS(turnScratch->angle) < ACTOR_01900_PATROL_FRONT_YAW_LIMIT) {
                if (_actor01900EngageBattleIfPlayerLevel(task) == 1) {
                    work->state = ACTOR_01900_STATE_ALERT;
                }
            }
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
        work->state = ACTOR_01900_STATE_ALERT;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Coasts toward the player by the carried run step, then turns around.
///
/// Requires initialized work, enemy, model, player roots and scratch storage.
/// Clamps each turn to 64/4096, resolves contacts, moves by `runStep` parent-space
/// units and reduces a positive step by ten with signed-halfword truncation.
/// Zero speed or a settled slide clip selects turn-around. Entry keeps the speed.
static void _actor01900StateSlide(Task* task)
{
    enum {
        ACTOR_01900_SLIDE_CLIP         = 18,
        ACTOR_01900_SLIDE_RATE         = 30,
        ACTOR_01900_SLIDE_TURN_STEP    = 64,
        ACTOR_01900_SLIDE_DECELERATION = 10
    };

    _Actor01900Work*  work;
    Enemy*            enemy;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    ActorTurnScratch* turnScratch;
    u16               nextRunStep;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy             = task->spawnArg2.pointer;
        model             = task->extra.tmd;
        work->animId      = ACTOR_01900_SLIDE_CLIP;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
        model->flags      = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ACTOR_01900_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ACTOR_01900_SLIDE_RATE;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turnScratch         = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turnScratch->angle  = _actorAngleTurnToPlayer(task, &turnScratch->delta, &gPlayerStatus);
    work->lookYawTarget = turnScratch->angle;
    if (turnScratch->angle > ACTOR_01900_SLIDE_TURN_STEP) {
        turnScratch->angle = ACTOR_01900_SLIDE_TURN_STEP;
    }
    if (turnScratch->angle < -ACTOR_01900_SLIDE_TURN_STEP) {
        turnScratch->angle = -ACTOR_01900_SLIDE_TURN_STEP;
    }
    rootCoord           = task->extra.tmd->coords;
    turnScratch->angle += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turnScratch->angle, GRAPHICS_ROTATION_REPLACE);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != true) {
        _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, work->runStep);
    // Keep the halfword subtraction and signed clamp of the carried speed.
    if (work->runStep > 0) {
        nextRunStep   = work->runStep - ACTOR_01900_SLIDE_DECELERATION;
        work->runStep = nextRunStep;
        if ((s16)nextRunStep < 0) {
            work->runStep = 0;
        }
    }
    _actor01900UpdateAnimation(task);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) || work->runStep == 0) {
        work->state = ACTOR_01900_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Faces the player, backs away for nineteen ticks, then turns aside and chases.
///
/// Requires initialized work, enemy, model, player roots and scratch storage.
/// Within 128/4096 turn, switches from walk to the retreat clip. Positive turns
/// are capped at 128 then halved; negative turns below -128 are capped without
/// halving. Retreat steps -16 parent-space units; the exit adds +/-1200/4096 yaw.
static void _actor01900StateBackOff(Task* task)
{
    enum {
        ACTOR_01900_BACK_OFF_RATE       = 22,
        ACTOR_01900_BACK_OFF_CLIP       = 17,
        ACTOR_01900_BACK_OFF_TURN_LIMIT = 128,
        ACTOR_01900_BACK_OFF_STEP       = -16,
        ACTOR_01900_BACK_OFF_TICKS      = 19,
        ACTOR_01900_BACK_OFF_EXIT_YAW   = 1200
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* chase;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ACTOR_01900_BACK_OFF_RATE;
        work->animId            = ACTOR_01900_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        return;
    }
    _actor01900UpdateAnimation(task);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase               = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase->turn         = _actorAngleTurnToPlayer(task, &chase->delta, &gPlayerStatus);
    work->lookYawTarget = chase->turn;
    if (ABS(chase->turn) <= ACTOR_01900_BACK_OFF_TURN_LIMIT && work->animId == ACTOR_01900_ANIM_WALK) {
        work->animRate    = ACTOR_01900_BACK_OFF_RATE;
        work->animId      = ACTOR_01900_BACK_OFF_CLIP;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        _actor01900UpdateAnimation(task);
    }
    // Preserve the asymmetric steering: only the negative clamp skips halving.
    if (chase->turn > ACTOR_01900_BACK_OFF_TURN_LIMIT) {
        chase->turn = ACTOR_01900_BACK_OFF_TURN_LIMIT;
    }
    if (chase->turn < -ACTOR_01900_BACK_OFF_TURN_LIMIT) {
        chase->turn = -ACTOR_01900_BACK_OFF_TURN_LIMIT;
    } else {
        chase->turn = chase->turn >> 1;
    }
    rootCoord    = task->extra.tmd->coords;
    chase->turn += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ACTOR_01900_BACK_OFF_CLIP) {
        work->stateTimer++;
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR_01900_BACK_OFF_STEP);
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != true) {
            _actor01900ApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->stateTimer >= ACTOR_01900_BACK_OFF_TICKS) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&task->extra.tmd->coords->coord, ACTOR_01900_BACK_OFF_EXIT_YAW, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -ACTOR_01900_BACK_OFF_EXIT_YAW, GRAPHICS_ROTATION_COMPOSE);
            }
            work->state = ACTOR_01900_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Replays the alert clip in place, then returns to chase when it settles.
///
/// Requires initialized work, enemy, model, player roots and scratch storage.
/// Entry disables attack pairs and grid tests. Later ticks update the look target
/// but rebuild the root at its existing yaw and package scale without translating.
static void _actor01900StateAlertRepeat(Task* task)
{
    enum {
        ACTOR_01900_ALERT_REPEAT_CLIP = 9
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* chase;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_ALERT_REPEAT_CLIP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor01900UpdateAnimation(task);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR_01900_STATE_CHASE;
    }
    // The look follows the player; root yaw stays at its existing heading.
    chase->turn         = _actorAngleTurnToPlayer(task, &chase->delta, &gPlayerStatus);
    work->lookYawTarget = chase->turn;
    if (chase->turn > 0) {
        chase->turn = 0;
    }
    if (chase->turn < 0) {
        chase->turn = 0;
    }
    rootCoord    = task->extra.tmd->coords;
    chase->turn += ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    _actor01900UpdateAnimation(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Eases the scripted pose's look target toward the player by 40/4096 turn.
///
/// Borrows initialized work; yaw differences retain signed linear ordering.
static inline void _actor01900EaseScriptedWatchLook(_Actor01900Work* work, s16 playerTurn)
{
    enum { ACTOR_01900_SCRIPTED_WATCH_LOOK_STEP = 40 };

    if (work->lookYawTarget < playerTurn) {
        if (playerTurn - work->lookYawTarget > ACTOR_01900_SCRIPTED_WATCH_LOOK_STEP) {
            work->lookYawTarget += ACTOR_01900_SCRIPTED_WATCH_LOOK_STEP;
        } else {
            work->lookYawTarget = playerTurn;
        }
    } else if (work->lookYawTarget - playerTurn > ACTOR_01900_SCRIPTED_WATCH_LOOK_STEP) {
        work->lookYawTarget -= ACTOR_01900_SCRIPTED_WATCH_LOOK_STEP;
    } else {
        work->lookYawTarget = playerTurn;
    }
}

/// Holds the scripted watch pose and eases its look target toward the player.
///
/// Requires initialized work, enemy, model, player roots and scratch storage.
/// Entry makes the target non-lockable and disables attack pairs and grid tests.
/// Later ticks approach the relative player bearing by 40/4096 using signed linear
/// comparisons, hold the root's yaw at package scale and restart the watch clip.
/// The placement command, rather than this handler, sets the root position.
static void _actor01900StateScriptedWatch(Task* task)
{
    enum {
        ACTOR_01900_SCRIPTED_WATCH_CLIP = 19
    };

    _Actor01900Work*   work;
    Enemy*             enemy;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    ActorChaseScratch* chase;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_SCRIPTED_WATCH_CLIP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor01900UpdateAnimation(task);
        _actor01900UpdateAnimation(task);
        work->stateTimer = 0;
        work->lookYaw    = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase       = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    chase->turn = _actorAngleTurnToPlayer(task, &chase->delta, &gPlayerStatus);
    _actor01900EaseScriptedWatchLook(work, chase->turn);
    rootCoord   = task->extra.tmd->coords;
    chase->turn = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    // Restart the pose each tick while the look target eases toward the player.
    work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
    _actor01900UpdateAnimation(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn08724(Task* arg0)
{
    SVECTOR          vec;
    EffectWork*      eff;
    _Actor01900Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.radius          = 0x180;
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        sceneReleaseBattleRefWithRewards(arg0, 0x13);
    }
    work->stateTimer++;
    switch (work->stateTimer) {
        case 3:
            D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
            vec.vz                   = 0x64;
            vec.vy                   = 0;
            vec.vx                   = 0;
            eff                      = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec);
            if (eff != NULL) {
                _actorRenderApplyTaskPlacementTextureOffsets(eff->task, enemy);
            }
            break;
        case 4:
            D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
            vec.vy                   = 0;
            vec.vx                   = 0;
            eff                      = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec);
            if (eff != NULL) {
                _actorRenderApplyTaskPlacementTextureOffsets(eff->task, enemy);
            }
            break;
    }
    if (work->stateTimer >= 0x3D) {
        work->state = ACTOR_01900_STATE_HIDDEN;
    }
}

static void Actor01900_Fn0892C(Task* arg0)
{
    SVECTOR          vec;
    EffectWork*      eff;
    _Actor01900Work* work;
    Enemy*           enemy;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->hitBody.radius          = 0x180;
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
        vec.vx                        = 0x64;
        vec.vz                        = 0;
        vec.vy                        = 0;
        work->animId                  = 2;
        work->animRequest             = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate                = 0x10;
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        work->stateTimer = 0;
    }
    work->stateTimer++;
    switch (work->animId) {
        case 2:
            if (work->stateTimer >= 0x10 && (work->rig.slots[1].status.fields.flags & 2)) {
                work->animId      = 0x18;
                work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
                work->animRate    = 0x10;
                work->blendActive = 0;
            }
            _actorMovementStepForward(arg0->extra.tmd->coords, 0xA);
            _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
            if (work->stateTimer == 3) {
                D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
                vec.vz                   = 0x64;
                vec.vy                   = 0;
                vec.vx                   = 0;
                eff                      = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec);
                actorTintEffect(eff, enemy);
            }
            if (work->stateTimer == 5) {
                D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
                eff                      = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL);
                actorTintEffect(eff, enemy);
            }
            break;
        case 0x18:
            if (!(work->rig.slots[1].status.fields.flags & 0x100)) {
                work->stateTimer = 0;
            }
            switch ((s16)(work->stateTimer - 0x19)) {
                case 0:
                    sceneReleaseBattleRefWithRewards(arg0, 0x13);
                    break;
                case 5:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    effectSpawn(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 17:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 39:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->state            = ACTOR_01900_STATE_HIDDEN;
                    break;
            }
            cur = work->stateTimer;
            if (cur >= 0x1A) {
                _actorRenderRescaleYawY(arg0->extra.tmd->coords, ACTOR_01900_ROOT_SCALE, ACTOR_01900_ROOT_SCALE - (cur - 0x14) * 0xB);
            }
            break;
    }
    _actor01900UpdateAnimation(arg0);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 2);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 3);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 4);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 5);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 6);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 7);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 8);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 9);
    Actor01900_ResetYaw(arg0->extra.tmd->coords + 10);
}

/// Returns 1 when the outgoing attack contacts include a player or companion body.
///
/// Borrows three readable records, stopping at the first zero key. Returns 0 when
/// none has the player-body kind. The collision body produces only the first
/// record here; the other two remain in the zero-initialized work allocation.
static __inline__ s32 _actor01900HasPlayerBodyContact(const WorldCollisionContact* contacts)
{
    enum { ACTOR_01900_ATTACK_CONTACT_SCAN_COUNT = ARRAY_SIZE(((_Actor01900Work*)NULL)->attackContacts) };

    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ACTOR_01900_ATTACK_CONTACT_SCAN_COUNT; contactIndex++) {
        if (!contacts[contactIndex].key.value)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return 1;
        }
    }
    return 0;
}

/// Swings at the player, then alerts if distant or steps back if still close.
///
/// Requires initialized work, enemy, model, player roots and scratch storage.
/// Tracks the player by at most 48/4096 turn on ticks 1..13. Attack pairs are
/// enabled on ticks 22..28 and disabled early on player/companion body contact.
/// The settled clip tests a 700-unit XZ radius. Entry engages battle except in
/// the cached Acropolis Patio context; all three attack-contact records are live.
static void _actor01900StateStrike(Task* task)
{
    enum {
        ACTOR_01900_STRIKE_CLIP              = 4,
        ACTOR_01900_STRIKE_FIRST_ATTACK_TICK = 22,
        ACTOR_01900_STRIKE_END_ATTACK_TICK   = 29,
        ACTOR_01900_STRIKE_AIM_TICKS         = 14,
        ACTOR_01900_STRIKE_TURN_STEP         = 48,
        ACTOR_01900_STRIKE_RETREAT_RADIUS    = 700
    };

    _Actor01900Work*   work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          facingCoord;
    ActorChaseScratch* chase;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->hitBody.radius    = ACTOR_01900_BODY_RADIUS;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_STRIKE_CLIP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actor01900UpdateAnimation(task);
        work->circleCount = 0;
        // Read the aligned cached stage/area bytes as one little-endian context key.
        if (*(u16*)work->commandBytes != ((GAME_AREA_ACROPOLIS_PATIO << 8) | GAME_STAGE_ACROPOLIS)) {
            _actor01900EngageBattleIfPlayerLevel(task);
        }
        work->stateTimer        = 0;
        work->stateCounter      = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    // Attack ticks 22..28 end early on a player or companion body contact.
    work->stateTimer++;
    if (work->stateTimer == ACTOR_01900_STRIKE_FIRST_ATTACK_TICK) {
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateTimer == ACTOR_01900_STRIKE_END_ATTACK_TICK) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (_actor01900HasPlayerBodyContact(work->attackContacts) == 1) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor01900UpdateAnimation(task);
    if (work->stateTimer < ACTOR_01900_STRIKE_AIM_TICKS) {
        rootCoord = task->extra.tmd->coords;
        chase->turn =
            _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn > ACTOR_01900_STRIKE_TURN_STEP) {
            chase->turn = ACTOR_01900_STRIKE_TURN_STEP;
        }
        if (chase->turn < -ACTOR_01900_STRIKE_TURN_STEP) {
            chase->turn = -ACTOR_01900_STRIKE_TURN_STEP;
        }
        facingCoord  = task->extra.tmd->coords;
        chase->turn += ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
        gfxRotMatrixY(&task->extra.tmd->coords->coord, chase->turn, GRAPHICS_ROTATION_REPLACE);
        _actorRenderRescaleYaw(task->extra.tmd->coords, ACTOR_01900_ROOT_SCALE);
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (_actorRangeOutsideRadiusXZ(&chase->delta, ACTOR_01900_STRIKE_RETREAT_RADIUS)) {
            work->state = ACTOR_01900_STATE_ALERT;
        } else {
            work->state = ACTOR_01900_STATE_STEP_BACK;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Drops back into the down clip after interrupted rising or death during status hold.
///
/// Requires initialized work, enemy and model. Entry enables both grid-contact
/// paths, clears look yaw and alerts for negative HP. Once the clip settles,
/// disables hit-body grid tests and selects death burn for nonpositive HP,
/// status hold for surviving buildup, or down otherwise.
static void _actor01900StateRefall(Task* task)
{
    _Actor01900Work* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.radius          = ACTOR_01900_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_01900_ANIM_DOWN;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            sceneSetEnemyAlert(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _actor01900UpdateAnimation(task);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        if (enemy->hp <= 0) {
            work->state = ACTOR_01900_STATE_DEATH_BURN;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = ACTOR_01900_STATE_STATUS_HOLD;
        } else {
            work->state = ACTOR_01900_STATE_DOWN;
        }
    }
}

static void Actor01900_Fn09D3C(Enemy* enemy, Task* actor)
{
    VECTOR                    pos;
    _Actor01900StateTable     states;
    _Actor01900Work*          work;
    ActorPartPositionScratch* scratch;
    ActorPartPositionScratch* head;
    s32                       state;

    work   = actor->work;
    states = Actor01900_D001BC;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->state;
            if ((state != ACTOR_01900_STATE_HIDDEN) && (state != ACTOR_01900_STATE_DEATH_BURN) && (state != ACTOR_01900_STATE_DEATH_BURST) && (state != ACTOR_01900_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ACTOR_01900_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->state;
            if ((state != ACTOR_01900_STATE_HIDDEN) && (state != ACTOR_01900_STATE_DEATH_BURN) && (state != ACTOR_01900_STATE_DEATH_BURST) && (state != ACTOR_01900_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ACTOR_01900_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
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
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else {
        Actor01900_Fn02A50(actor);
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (u16)work->state;
    state           = work->state;
    if ((state == ACTOR_01900_STATE_SCRIPTED_WATCH) || (state == ACTOR_01900_STATE_DEATH_BURN) || (state == ACTOR_01900_STATE_HIDDEN) || (state == ACTOR_01900_STATE_DEATH_BURST) || (state == ACTOR_01900_STATE_DEATH_BURST_WALK)) {
        work->hitBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    states.handlers[work->state](actor);
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->attackContacts);
    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->state == ACTOR_01900_STATE_PATROL)) {
        work->state = ACTOR_01900_STATE_ALERT;
    }

    scratch->position.vx = 0;
    scratch->position.vy = 0;
    scratch->position.vz = 0;
    _actorRenderTransformToWorld(actor->extra.tmd->coords + 2, &scratch->position);

    work->bodyPosHistory[work->bodyPosCursor].vx = scratch->position.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = scratch->position.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = scratch->position.vz;

    SCRATCH_STACK_RELEASE_BLOCK(ActorPartPositionScratch);
    work->bodyPosCursor = (u16)work->bodyPosCursor + 1;
    if (work->bodyPosCursor == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if ((u32)((u16)work->animId - 0x14) < 2U) {
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

/// Ignores message 2015 without accessing the receiver or either argument.
///
/// The matched callback falls off its signed-return body without setting a result.
/// Senders must ignore the result; the message's sender-side purpose is unproven.
static s32 _actor01900IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unused)
{
}

/// The actor's state handlers, indexed by `_Actor01900Work::state`; empty
/// slots are states the actor never enters. `Actor01900_Fn09D3C` copies the
/// table to its frame before dispatching.
static const _Actor01900StateTable Actor01900_D001BC = { {
    _actor01900StateHidden,
    _actor01900StatePlayWalk,
    _actor01900StatePlayRun,
    _actor01900StatePlayDown,
    _actor01900StateStatusHold,
    Actor01900_Fn0A9C0,
    _actor01900StateAlert,
    _actor01900StateChase,
    _actor01900StateCircle,
    _actor01900StateTurnAround,
    _actor01900StateSidestep,
    _actor01900StateStrike,
    NULL,
    NULL,
    _actor01900StateStepBack,
    Actor01900_Fn0AA78,
    NULL,
    Actor01900_Fn0AB1C,
    _actor01900StateApproach,
    _actor01900StateFall,
    NULL,
    Actor01900_Fn06904,
    NULL,
    Actor01900_Fn06B4C,
    _actor01900StatePatrol,
    _actor01900StateBackOff,
    _actor01900StateSlide,
    _actor01900StateAlertRepeat,
    _actor01900StateScriptedWatch,
    Actor01900_Fn08724,
    Actor01900_Fn0892C,
    _actor01900StateRefall,
} };

/// The actor task's dispatcher table, indexed by `Task::state`: spawn
/// (`_actor01900Initialize`), a three-frame wait (`Actor01900_Fn0ABA0`), the
/// per-frame tick (`Actor01900_Fn09D3C`) and teardown.
static const EnemyTaskFuncTable4 Actor01900_D0023C = { {
    _actor01900Initialize,
    Actor01900_Fn0ABA0,
    Actor01900_Fn09D3C,
    enemyDestroy,
} };

/// Maps a requested animation id and forces entry into the down state.
///
/// Requires live initialized work and a borrowed request. IDs 0..4 map to local
/// clips 34, 35, 36, 37 and 39; other IDs leave `animId` unchanged. No animation
/// request flag is set, and no bank is installed here. All IDs select `ACTOR_01900_STATE_DOWN` and
/// force its next state entry. Other request fields and argument words are ignored.
/// Returns 0. The mapped clips' contents are unproven; the static bank leaves them empty.
static s32 _actor01900HandleAnimationRequest(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unused)
{
    enum { ACTOR_01900_REQUEST_CLIP_BASE = 0x22,
           ACTOR_01900_FORCE_STATE_ENTRY = -1 };

    _Actor01900Work* work = task->work;

    switch (request->animationId) {
        case 0:
            work->animId = ACTOR_01900_REQUEST_CLIP_BASE;
            break;
        case 1:
            work->animId = ACTOR_01900_REQUEST_CLIP_BASE + 1;
            break;
        case 2:
            work->animId = ACTOR_01900_REQUEST_CLIP_BASE + 2;
            break;
        case 3:
            work->animId = ACTOR_01900_REQUEST_CLIP_BASE + 3;
            break;
        case 4:
            work->animId = ACTOR_01900_REQUEST_CLIP_BASE + 5;
            break;
    }
    work->state     = ACTOR_01900_STATE_DOWN;
    work->prevState = ACTOR_01900_FORCE_STATE_ENTRY;
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

/// Acknowledges `ACTOR_MESSAGE_RELEASE_HOLD` by returning 1.
///
/// Does not access the task or arguments, or change any player-hold state.
static s32 _actor01900AcknowledgeHoldRelease(Task* task, s32 messageId, s32 unusedPayload, s32 unused)
{
    return 1;
}

/// Applies an actor `command` in the Patio or Dryfield Toilet namespace.
///
/// Requires live initialized work and a model; `command` borrows a complete four-byte
/// `ActorCommand` for this call. Caches its context and only the low `command` byte,
/// including unsupported commands. Command 0 hides in either namespace; Patio
/// `command` 1 selects scripted dormancy, Toilet `command` 2 places the scripted watch
/// pose at (-1429, 0, -1457), yaw -1024 in parent space. Returns 1 for these actions,
/// zero otherwise. Does not force state re-entry. Other argument words are ignored.
static s32 _actor01900ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused)
{
    enum {
        ACTOR_01900_COMMAND_HIDE          = 0,
        ACTOR_01900_COMMAND_PATIO_DORMANT = 1,
        ACTOR_01900_COMMAND_TOILET_WATCH  = 2
    };

    u16              contextKey;
    u16              patioCommand;
    u16              toiletCommand;
    _Actor01900Work* work;

    work = task->work;
    // Cache only three bytes; the context and command tests retain their full widths.
    work->commandBytes[0] = command->context.loc.stage;
    work->commandBytes[1] = command->context.loc.area;
    work->commandBytes[2] = (u8)command->command;
    contextKey            = command->context.key;
    if (contextKey == ((GAME_AREA_ACROPOLIS_PATIO << 8) | GAME_STAGE_ACROPOLIS)) {
        patioCommand = command->command;
        switch (patioCommand) {
            case ACTOR_01900_COMMAND_HIDE:
                work->state = ACTOR_01900_STATE_HIDDEN;
                return 1;
            case ACTOR_01900_COMMAND_PATIO_DORMANT:
                work->state = ACTOR_01900_STATE_DORMANT_SCRIPTED;
                return 1;
            default:
                return 0;
        }
    } else if (contextKey == ((GAME_AREA_DRYFIELD_TOILET << 8) | GAME_STAGE_DRYFIELD)) {
        toiletCommand = command->command;
        switch (toiletCommand) {
            case ACTOR_01900_COMMAND_HIDE:
                work->state = ACTOR_01900_STATE_HIDDEN;
                return 1;
            case ACTOR_01900_COMMAND_TOILET_WATCH:
                work->state                         = ACTOR_01900_STATE_SCRIPTED_WATCH;
                task->extra.tmd->coords->coord.t[0] = -0x595;
                task->extra.tmd->coords->coord.t[1] = 0;
                task->extra.tmd->coords->coord.t[2] = -0x5B1;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_REPLACE);
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

/// Unlinks the Stranger's collision bodies and releases its enemy and task.
///
/// Requires the live enemy stored in `spawnArg2.pointer`; work may be NULL.
/// Kills either retained auxiliary task, unlinks all three bodies and clears the
/// enemy's borrowed contact pointer before destruction. Task teardown frees work;
/// the task, enemy and work pointers must not be used after this call.
static void _actor01900Exit(Task* task)
{
    _Actor01900Work* work;
    Enemy*           enemy;

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

/// Hides the model and disables targeting, attack pairs and grid tests on entry.
///
/// Requires initialized work, enemy and model. Later ticks do nothing. The outer
/// actor tick disables hit-body pairs for this state; buffers remain owned by the model.
static void _actor01900StateHidden(Task* task)
{
    Enemy*           enemy;
    TmdObject*       model;
    _Actor01900Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

/// Plays the walk clip in place without selecting another state.
///
/// Requires initialized work, enemy and model. Entry enables drawing and targeting,
/// requests a normal-rate restart and disables attack pairs and grid tests.
/// Later ticks dirty the root and advance playback without moving it.
static void _actor01900StatePlayWalk(Task* task)
{
    TmdObject*       model;
    _Actor01900Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor01900UpdateAnimation(task);
    } else {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor01900UpdateAnimation(task);
    }
}

/// Plays the run clip in place without selecting another state.
///
/// Requires initialized work, enemy and model. Entry enables drawing and targeting,
/// requests a normal-rate restart and disables attack pairs and grid tests.
/// Later ticks dirty the root and advance playback without moving it.
static void _actor01900StatePlayRun(Task* task)
{
    TmdObject*       model;
    _Actor01900Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_ANIM_RUN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor01900UpdateAnimation(task);
    } else {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor01900UpdateAnimation(task);
    }
}

/// Plays the down clip in place and holds its boundary pose.
///
/// Requires initialized work, enemy and model. Entry enables drawing and targeting,
/// requests a normal-rate restart and disables attack pairs and grid tests.
/// Later ticks dirty the root and advance playback without a state transition.
static void _actor01900StatePlayDown(Task* task)
{
    TmdObject*       model;
    _Actor01900Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor01900EnableCombatModel(task->spawnArg2.pointer, model);
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ACTOR_01900_ANIM_DOWN;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        _actor01900UpdateAnimation(task);
    } else {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        _actor01900UpdateAnimation(task);
    }
}

static void Actor01900_Fn0A9C0(Task* arg0)
{
    _Actor01900Work* work;
    TmdObject*       obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate          = 0x12;
        work->animId            = 0xD;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor01900UpdateAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_01900_STATE_CHASE;
    }
}

static void Actor01900_Fn0AA78(Task* arg0)
{
    _Actor01900Work* work;
    Enemy*           enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->hitBody.radius          = 0x180;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animId                  = 8;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animRate                = work->baseRate;
    }
    _actor01900UpdateAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_01900_STATE_CHASE;
    }
}

static void Actor01900_Fn0AB1C(Task* arg0)
{
    _Actor01900Work* work;
    Enemy*           enemy;
    u32              rng;
    s16              timer;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        rng              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = rng;
        work->stateTimer = ((rng >> 16) & 0xF) + work->downFramesBase;
    }
    timer            = work->stateTimer - 1;
    work->stateTimer = timer;
    if (timer < 0) {
        work->state = ACTOR_01900_STATE_RISE;
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_01900_STATE_DEATH_BURN;
    }
}

static void Actor01900_Fn0ABA0(Enemy* enemy, Task* task)
{
    u16              count;
    _Actor01900Work* work;

    work             = task->work;
    count            = work->stateTimer + 1;
    work->stateTimer = count;
    if ((s16)count >= 3) {
        task->state++;
    }
}

void Actor01900_Fn0ABE4(Task* arg0)
{
    EnemyTaskFuncTable4 sp;

    sp = Actor01900_D0023C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
