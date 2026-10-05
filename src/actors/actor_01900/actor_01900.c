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
/// Twelve preset hit-reaction directions `Actor01900_Fn02664` copies from;
/// `pad` carries the index of the coordinate the effect is attached to.
extern SVECTOR   Actor01900_D1722C[];
static TmdSource _gActor01900StrangerBurstHand;
extern s16       Actor01900_D172FC;

#include "../../shared/actor_contacts.h"

static void Actor01900_Fn02A50(Task* arg0);
static void Actor01900_Fn02664(Task* arg0, s16 yaw, s32 id);
static void Actor01900_Fn01C94(Task* arg0);
static void Actor01900_Fn0AB1C(Task* arg0);
static void Actor01900_Fn0A6CC(Task* task);
static s32  Actor01900_Fn03FF8(Task* arg0, WorldCollisionContact* recs, s16 count);
static void Actor01900_Fn08724(Task* arg0);
static void Actor01900_Fn0A7C0(Task* arg0);
static void Actor01900_Fn03C04(GameLocationKey* session, GfxCoord* coord);
s32         Actor01900_Fn0A31C(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3);
s32         Actor01900_Fn0A5A4(Task* arg0, s32 arg1, u16* arg2, s32 arg3);

/* Inline bodies behind `Actor01900_Fn080A8`. Same shapes as
 * `actor_400100_facing.h` and `ActorsShared80135a60`; inlining is what keeps
 * each `SCRATCH_STACK_CURSOR_SLOT` access out of a register CSE would share. */

static TmdSource _gActor01900GrinningStrangerBody;
s32              Actor01900_Fn0A31C(Task*, s32, AnimationPlayRequest*, s32);
s32              Actor01900_Fn0A59C(Task*, s32, s32, s32);
s32              Actor01900_Fn0A5A4(Task*, s32, u16*, s32);
s32              Actor01900_Fn0A314(Task*, s32, s32, s32);
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
    { 2015, Actor01900_Fn0A314 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, Actor01900_Fn0A31C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { 2014, Actor01900_Fn0A59C },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor01900_Fn0A5A4 },
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

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// Cross-fade lengths in frames, indexed by the clip being left and the clip
/// being entered. `Actor01900_Fn01C94` reads one entry per animation change.
extern s8 Actor01900_D16988[][0x2D];

static SVECTOR ActorContact_ScratchPosition;

static const _Actor01900StateTable Actor01900_D001BC;

static void Actor01900_Fn03710(Task* arg0);

static void Actor01900_Fn03854(Task* arg0);

static void Actor01900_Fn042BC(Task* arg0);

static void Actor01900_Fn04D14(Task* arg0);

static void Actor01900_Fn0551C(Task* arg0);

static void Actor01900_Fn05B4C(Task* arg0);

static void Actor01900_Fn05F38(Task* arg0);

static void Actor01900_Fn06100(Task* arg0);

static void Actor01900_Fn06634(Task* arg0);

static void Actor01900_Fn06904(Task* arg0);

static void Actor01900_Fn06B4C(Task* arg0);

static void Actor01900_Fn06F40(Task* arg0);

static void Actor01900_Fn07810(Task* arg0);

static void Actor01900_Fn07BA8(Task* arg0);

static void Actor01900_Fn080A8(Task* arg0);

static void Actor01900_Fn083E8(Task* arg0);

static void Actor01900_Fn0892C(Task* arg0);

static void Actor01900_Fn09694(Task* arg0);

static void Actor01900_Fn09BE8(Task* arg0);

static void Actor01900_Fn0A764(Task* arg0);

static void Actor01900_Fn0A868(Task* arg0);

static void Actor01900_Fn0A914(Task* arg0);

static void Actor01900_Fn0A9C0(Task* arg0);

static void Actor01900_Fn0AA78(Task* arg0);

static void Actor01900_Fn0ABA0(Enemy* enemy, Task* task);

static __inline__ void Actor01900_MoveForward(GfxCoord* coord, s16 amount);
static __inline__ void Actor01900_StepForward(GfxCoord* coord, s16 amount);
static __inline__ void Actor01900_StepForwardHead(GfxCoord* coord, s16 amount);
static __inline__ void Actor01900_ResetYaw(GfxCoord* coord);

static void            Actor01900_Fn01950(Task* arg0);
static s32             Actor01900_Fn01A7C(_Actor01900Work* work);
static __inline__ void Actor01900_BindMatrices(Task* actor);
static void            Actor01900_Fn02018(Enemy* enemy, Task* actor);
static __inline__ s32  Actor01900_FindHit(WorldCollisionContact* records, SVECTOR* pos);
static __inline__ s32  Actor01900_ArmIfPlayerLevel(Task* arg0);
static __inline__ s32  Actor01900_HasHeightClamp(GameLocationKey* session);
static s32             Actor01900_Fn03C98(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3);
static __inline__ s32  Actor01900_HasHit(WorldCollisionContact* records);
static void            Actor01900_Fn09D3C(Enemy* enemy, Task* actor);

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `actorMoveForwardNonzero`.
static __inline__ void Actor01900_MoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
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

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen, without `Actor01900_MoveForward`'s zero-amount guard. Same body as
/// `actorMoveForward`.
static __inline__ void Actor01900_StepForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
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

/// `Actor01900_StepForward` with the X component read back through `head`,
/// as `Actor01900_MoveForward` does.
static __inline__ void Actor01900_StepForwardHead(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
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

/// Advances the actor's animation one tick. Joints 1-10 are sampled from both
/// the main and the blend animation and passed to `Gp_AnimWritePoseCopy` with
/// weights `blendWeight` and 0x1000 - `blendWeight`; joints 11-18 tick the main
/// animation alone. The per-joint rates come from `animRate` and `blendRate`.
static void Actor01900_Fn01950(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    _Actor01900Work*  work;

    work   = arg0->work;
    weight = work->blendWeight;
    anim   = &work->rig.anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blend.slots[i].rate = work->blendRate;
            work->rig.slots[i].rate   = (work->animRate - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blend.anim, i, &blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

static s32 Actor01900_Fn01A7C(_Actor01900Work* work)
{
    s32 id;
    s32 prev;

    switch (work->animId) {
        case 20:
        case 21:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 7) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0010;
                }
                work->lastCueFrame = id;
            } else if (id == 0x10) {
                prev = work->lastCueFrame;
                if (prev != id) {
                    work->lastCueFrame = id;
                    return 0x400A0011;
                }
                work->lastCueFrame = prev;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case 7:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 0xF) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0010;
                }
                work->lastCueFrame = id;
            } else if (id == 0x14) {
                prev = work->lastCueFrame;
                if (prev != id) {
                    work->lastCueFrame = id;
                    return 0x400A0011;
                }
                work->lastCueFrame = prev;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case 2:
        case 3:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 0x24) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0002;
                }
                work->lastCueFrame = id;
            } else if (id == 0x2C) {
                prev = work->lastCueFrame;
                if (prev != id) {
                    work->lastCueFrame = id;
                    return 0x400A0001;
                }
                work->lastCueFrame = prev;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case 9:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 4 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A0006;
            }
            work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            break;
        case 4:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 0xC && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A000C;
            }
            work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            break;
        case 11:
            id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            if (id == 4 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A0005;
            }
            work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            break;
        default:
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
            work->lastCueFrame = prev;
            break;
    }
    return 0;
}

/// Per-frame animation driver: services a pending clip change, advances the
/// body and blend animations, eases the head toward its target yaw and emits
/// whatever sound event the current clip has reached.
///
/// `animRequest` is the pending-change request: `BLEND` cross-fades into
/// `animId` over the table's frame count, `RESET` restarts it outright, and
/// both settle to `PLAYING`. `blendRequest` does the same for the blend
/// animation and `blendAnimId`, which is only ever restarted.
static void Actor01900_Fn01C94(Task* arg0)
{
    _Actor01900Work* work;
    _Actor01900Work* w1;
    _Actor01900Work* w2;
    _Actor01900Work* w3;
    Enemy*           enemy;
    s16              cur;
    s16              dst;
    s16              raw;
    s32              clamped;
    s32              i;
    s32              i2;
    s32              i3;
    s32              i4;
    s32              snd;
    s32              id;
    s32              pan;
    u16              cur_u;
    u16              dst_u;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->animRequest == ACTOR_01900_ANIM_REQUEST_BLEND) {
        w1 = work;
        if (work->appliedAnim != work->animId) {
            for (i = 1; i < 0x13; i++) {
                w1->rig.slots[i].rate = w1->animRate;
                animationSeekSlotWithBlend(&w1->rig.anim, i, w1->animId, 0,
                                           (s32)Actor01900_D16988[w1->appliedAnim][w1->animId]);
            }
            /* Keeps this store from being merged with the identical one the
               `RESET` path makes just below. */
            w1->appliedAnim = (s16)(u16)w1->animId;
        }
        goto block_9;
    }
    if (work->animRequest == ACTOR_01900_ANIM_REQUEST_RESET) {
        w2 = work;
        i2 = 1;
        do {
            w2->rig.slots[i2].rate = w2->animRate;
            animationResetSlot(&w2->rig.anim, i2, w2->animId);
            i2++;
        } while (i2 < 0x13);
        w2->appliedAnim = (s16)(u16)w2->animId;
    block_9:
        work->animRequest  = ACTOR_01900_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueFrame = 0;
    }
    if (work->blendRequest == ACTOR_01900_ANIM_REQUEST_RESET) {
        i3              = 1;
        w1              = arg0->work;
        w1->blendRate   = 0x30;
        w1->blendWeight = 0x800;
        do {
            w1->rig.slots[i3].rate = w1->blendRate;
            animationResetSlot(&w1->blend.anim, i3, w1->blendAnimId);
            i3++;
        } while (i3 < 0x13);
        work->blendRequest = ACTOR_01900_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        w3 = arg0->work;
        i4 = 1;
        do {
            w3->rig.slots[i4].rate = w3->animRate;
            animationTickSlot(&w3->rig.anim, i4);
            i4++;
        } while (i4 < 0x13);
    } else {
        Actor01900_Fn01950(arg0);
        if (work->blend.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
            work->blendActive = 0;
        }
    }
    dst   = work->lookYawTarget;
    cur   = work->lookYaw;
    dst_u = (u16)work->lookYawTarget;
    cur_u = (u16)work->lookYaw;
    if (dst > cur) {
        if ((dst - cur) >= 0x101) {
            work->lookYaw = cur_u + 0x100;
        } else {
            goto block_25;
        }
    } else if ((cur - dst) >= 0x101) {
        work->lookYaw = cur_u - 0x100;
    } else {
    block_25:
        work->lookYaw = (s16)dst_u;
    }
    raw     = work->lookYaw;
    clamped = (u16)work->lookYaw;
    if (raw != 0) {
        if (raw >= 0x401) {
            clamped = 0x400;
        }
        if (raw < -0x400) {
            clamped = -0x400;
        }
        ActorContact_TurnJoint(arg0->extra.tmd->coords + 5, (s16)(((s16)clamped * 2) / 3));
        ActorContact_TurnJoint(arg0->extra.tmd->coords + 2,
                               (s16)((s32)((s16)clamped + ((u32)(clamped << 0x10) >> 0x1F)) >> 1));
        arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    snd = Actor01900_Fn01A7C(work);
    if (snd != 0) {
        id  = snd | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

/// Binds the actor model's light and colour matrices to the pair kept in its
/// work block.
static __inline__ void Actor01900_BindMatrices(Task* actor)
{
    _Actor01900Work* work;
    TmdObject*       obj;

    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

/// Enemy init: allocates the work block, sets up both animation contexts,
/// the three hit/body `WorldCollisionBody` nodes and the patrol points, then picks the
/// starting state from the spawn flags and rescales the model.
static void Actor01900_Fn02018(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    VECTOR              pos;
    SVECTOR*            v;
    TmdObject*          obj;
    GfxCoord*           root;
    _Actor01900Work*    work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    s32                 kind;

    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(_Actor01900Work), 0);
    actor->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    actor->exitCallback = Actor01900_Fn0A6CC;
    Actor01900_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)Actor01900_D0AC54.hpMax;
    enemy->param                  = &Actor01900_D0AC54;
    enemy->recs                   = work->hitContacts;
    animationInitContext(&work->rig.anim, Actor01900_D17174, obj,
                         work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, Actor01900_D17174, obj,
                         work->blend.poses, work->blend.slots);
    work->animRequest   = ACTOR_01900_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = 2;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = 0x10;
    work->animRate      = 0x10;
    Actor01900_Fn01C94(actor);

    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.coord            = root;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0x100;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30013;
    work->gridBody.radius           = 0x180;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->gridBody);
    work->hitCooldown    = 0;
    work->gridBody.flags = (work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED) & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    body                   = &work->hitBody;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->hitContacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x30000;
    body->radius           = 0x180;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    dir.vx                 = 0;
    dir.vy                 = 0;
    dir.vz                 = 0;
    head                   = &work->attackBody;
    head->coord            = &actor->extra.tmd->coords[4];
    head->context.contacts = work->attackContacts;
    v                      = &dir;
    head->pos.vx           = v->vx;
    head->pos.vy           = v->vy;
    head->pos.vz           = v->vz;
    head->radius           = 0x180;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, head);
    worldCollisionInitContacts(head->context.contacts, 1, 0);
    work->attackBody.key = Gp_PackObjPair(enemy, 0);

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

    actor->msgTable    = Actor01900_D1728C;
    root->parent       = &gGfxViewCoord;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->effectArg.coord      = &actor->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
    kind                       = actor->spawnArg1.value >> 16;
    switch (kind & 0xF) {
        case 2:
            work->prevState = -1;
            work->state     = ACTOR_01900_STATE_HIDDEN;
            break;
        case 4:
            work->prevState = -1;
            work->state     = ACTOR_01900_STATE_DORMANT_SCRIPTED;
            break;
        default:
            work->prevState = -1;
            work->state     = ACTOR_01900_STATE_PATROL;
            tmdAllocPrimitiveBuffer(obj);
            break;
    }
    switch (actor->spawnArg1.value & 0xF) {
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

    actorRescaleYaw(actor->extra.tmd->coords, 0x1194);
    work->bodyPosCursor = 0;
    actor->state++;
}

/// Spawns the hit-reaction effect for a blow arriving at `yaw`: carves one
/// `SVECTOR` off the scratch head, fills it with one of the twelve presets in
/// `Actor01900_D1722C` picked from the magnitude and sign of `yaw` plus a
/// random draw, hands it to `func_800FDB18` together with the parameter of
/// `id`, and releases the scratch again.
static void Actor01900_Fn02664(Task* arg0, s16 yaw, s32 id)
{
    SVECTOR*         dir;
    s32              absAng;
    _Actor01900Work* work;

    dir    = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    absAng = (yaw >= 0) ? yaw : -yaw;
    work   = arg0->work;
    if (absAng < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *dir = Actor01900_D1722C[0];
                break;
            case 1:
                *dir = Actor01900_D1722C[1];
                break;
            case 2:
                *dir = Actor01900_D1722C[2];
                break;
            case 3:
                *dir = Actor01900_D1722C[3];
                break;
            default:
                *dir = Actor01900_D1722C[4];
                break;
        }
    } else if (absAng > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *dir = Actor01900_D1722C[5];
                break;
            case 1:
                *dir = Actor01900_D1722C[6];
                break;
            default:
                *dir = Actor01900_D1722C[7];
                break;
        }
    } else if (yaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *dir = Actor01900_D1722C[8];
        } else {
            *dir = Actor01900_D1722C[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *dir = Actor01900_D1722C[10];
        } else {
            *dir = Actor01900_D1722C[11];
        }
    }
    work->effectArg.coord      = &arg0->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x300;
    work->effectArg.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, &arg0->extra.tmd->coords[dir->pad], dir, &work->effectArg);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// First `WorldCollisionContact` among the twelve at `records` whose id has high word 2,
/// copying its position to `pos`; 0 at the first empty record.
static __inline__ s32 Actor01900_FindHit(WorldCollisionContact* records, SVECTOR* pos)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
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
        s->hitKey = Actor01900_FindHit(work->hitContacts, &head[-1].hitPos);
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
            s->hitYaw       = actorNormalizeYaw(s->hitYaw);
            Actor01900_Fn02664(arg0, s->hitYaw, s->hitKey);
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
            s->damage         = Gp_ComputeDamage(s->hitKey, s->playerDistance, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->hitKey, 0) != 0) {
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
            func_800E2C78(enemy, s->hitKey, s->damage, 0);
            enemy->hp -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            work->recentDamage += s->damage;
            effect              = s->criticalEffect;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[2], (s32)(effect), NULL);
            }
            if (work->state == ACTOR_01900_STATE_DORMANT_SCRIPTED) {
                SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
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
            work->hitCooldown = Gp_GetIdParam2(s->hitKey);
            switch (Gp_GetIdParam0(s->hitKey) & 0xFFFF) {
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
                case 0:
                case 5:
                case 6:
                case 7:
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
                case 2:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_SetObjFlag2(enemy, s->hitKey, 0);
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
                case 3:
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    if (work->state == ACTOR_01900_STATE_DORMANT_SCRIPTED || work->state == ACTOR_01900_STATE_PATROL) {
                        work->state = ACTOR_01900_STATE_ALERT;
                    }
                    Gp_SetObjFlag4(enemy, s->hitKey, 0);
                    break;
                case 1:
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
            s->damage = Gp_TickObjFlag4(enemy);
            if (Gp_ObjFlag4Expired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            if (s->damage != 0) {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                enemy->hp              -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
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
                if ((Gp_GetIdParam0(s->hitKey) & 0xFFFF) == 4 || (Gp_GetIdParam0(s->hitKey) & 0xFFFF) == 6) {
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

static void Actor01900_Fn03710(Task* arg0)
{
    _Actor01900Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    s32              step;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest     = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate        = 0x10;
        work->animId          = 0x17;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        do {
            Actor01900_Fn01C94(arg0);
        } while ((u32)(work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) < 6U);
        work->animRate = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    step                                  = (s16)work->animRate / 2;
    work->animRate                        = (u16)step;
    if (step == 1) {
        work->animRate = -0x10;
    }
    if ((s16)work->animRate == -1) {
        work->animRate = 0x10;
    }
    Actor01900_Fn01C94(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_01900_STATE_DOWN;
    }
    if (enemy->hp <= 0) {
        work->state = ACTOR_01900_STATE_DOWN;
    }
}

/// Raises the player's weapon when the player is not already in state 2 and
/// stands within 0x1F4 of the actor in Y. Nonzero when it armed.
static __inline__ s32 Actor01900_ArmIfPlayerLevel(Task* arg0)
{
    Task* player;
    s32   dy;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
        dy = arg0->extra.tmd->coords->coord.t[1] - player->extra.tmd->coords->coord.t[1];
        if (ABS(dy) < 0x1F4) {
            Gp_ArmStateF0(1);
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers, arms the player if
/// they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x10 and rescales it by 0x1194.
static void Actor01900_Fn03854(Task* arg0)
{
    _Actor01900Work*   work;
    ActorChaseScratch* aim;
    GfxCoord*          coord;
    TmdObject*         obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate         = 0x10;
        work->animId           = 9;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
        work->hitBody.radius = 0x180;
        if (*(u16*)work->commandBytes != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
    } else {
        SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
        aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->rig.slots[1].status.fields.flags & 0x100) {
            work->state = ACTOR_01900_STATE_CHASE;
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
        Actor01900_Fn01C94(arg0);
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
    }
}

static void Actor01900_Fn03C04(GameLocationKey* session, GfxCoord* coord)
{
    ActorHeightClamp* row;
    s32               offset;
    s32               lo;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
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

/// `Actor01900_Fn03C04`'s row scan without the clamp: nonzero when the
/// current room has an `Actor01900_D172CC` row.
static __inline__ s32 Actor01900_HasHeightClamp(GameLocationKey* session)
{
    ActorHeightClamp* row;
    s16               i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->stage == row->stage && session->area == row->area) {
            return 1;
        }
    }
    return 0;
}

static s32 Actor01900_Fn03C98(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2, s16 arg3)
{
    ActorContactCappedPushScratch* head;
    ActorContactCappedPushScratch* s;
    ActorContactCappedPushScratch* blk;
    s16                            vy;
    SVECTOR*                       step;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head                                                = SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch);
    blk                                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorContactCappedPushScratch) = blk;
    s                                                   = blk;
    s->moved                                            = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.fixed.vx.word >> 16;
        s->step.vy = s->delta.fixed.vy.word >> 16;
        s->step.vz = s->delta.fixed.vz.word >> 16;
        if (Actor01900_HasHeightClamp(&gGameSession->location.loc)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x180) {
                s->step.vy = (vy <= 0) ? -0x180 : 0x180;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->stepLength      = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->stepLength      = SquareRoot0(s->stepLength);
        step               = &s->step;
        if (s->stepLength >= 0xC0) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xC0);
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
    if (Actor01900_HasHeightClamp(&gGameSession->location.loc)) {
        Actor01900_Fn03C04(&gGameSession->location.loc, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.fixed.vx.word != 0 || s->delta.fixed.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactCappedPushScratch);
    return s->moved;
}

/// Pushes the actor's root coordinate by half of each nearby kind 0x10000 /
/// 0x30000 record's offset, walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update is load-bearing: loop.c counts both copies
/// before cross-jumping merges them, which keeps `count`'s sign extension in the loop.
static s32 Actor01900_Fn03FF8(Task* arg0, WorldCollisionContact* recs, s16 count)
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
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            worldCollisionCalcContactViewOffset(&s->position, &recs[s->recordIndex], &s->offset);
            s->offsetLength = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->offsetLength = SquareRoot0(s->offsetLength);
            if (s->offsetLength >= 0xC0) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0xC0);
                gte_ldsv(&s->offset);
                gte_gpf12();
                gte_stsv(&s->offset);
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            } else {
                arg0->extra.tmd->coords->coord.t[0] += s->offset.vx / 2;
                arg0->extra.tmd->coords->coord.t[2] += s->offset.vz / 2;
            }
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return s->hit;
}

/// Circling state: turns toward the player at most 0x30 per step while walking,
/// switching to state 0xA when lined up and far enough, 0xB when close and in
/// front, or 0x1B after 0x5B steps.
static void Actor01900_Fn042BC(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* chase;
    s32                diff;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 0x42;
        work->animId            = 3;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->circleCount = 0;
        if (*(u16*)work->commandBytes != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->stateTimer   = 0;
        work->stateCounter = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    work->stateTimer++;
    work->stateCounter++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    if (Actor01900_Fn03C98(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts), 0x60) != 1) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
            Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->yawFromPlayer = actorNormalizeYaw(chase->yawFromPlayer);
    coord                = arg0->extra.tmd->coords;
    chase->turn          = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->lookYawTarget  = chase->turn;
    diff                 = chase->yawFromPlayer - chase->playerYaw;
    if (ABS(diff) < 0x44 && work->sidestepDelay + work->sidestepCount / 2 < work->stateTimer && ABS(chase->turn) < 0x80) {
        if (overlayOutOfRange(&chase->delta, 0x708)) {
            work->state = ACTOR_01900_STATE_SIDESTEP;
        }
    }
    if (detectSightBlocked(arg0) != 1) {
        work->stateTimer++;
        coord               = arg0->extra.tmd->coords;
        chase->turn         = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn < 0x200) {
            if (!overlayOutOfRange(&chase->delta, 0x2BC)) {
                work->state = ACTOR_01900_STATE_STRIKE;
            }
        }
        if (work->stateCounter >= 0x5B) {
            work->state = ACTOR_01900_STATE_ALERT_REPEAT;
        }
    } else {
        work->stateTimer    = 0;
        work->stateCounter  = 0;
        coord               = arg0->extra.tmd->coords;
        chase->turn         = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
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
            chase->turn += 0x300;
        } else {
            chase->turn -= 0x300;
        }
        if (work->stateTimer >= 0xF1) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    if (chase->turn > 0x30) {
        chase->turn = 0x30;
    }
    if (chase->turn < -0x30) {
        chase->turn = -0x30;
    }
    facing       = arg0->extra.tmd->coords;
    chase->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 3) {
        if (work->blendActive == 0) {
            Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
        } else {
            Actor01900_StepForward(arg0->extra.tmd->coords, 0xA);
        }
    } else if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->animId      = 3;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
    }
    if (work->field_C37 != 0) {
        work->field_C37--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn04D14(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* chase;
    s32                turn;
    s32                diffPos;
    s32                diffNeg;
    s32                yaw;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0xC0;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animId            = 3;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->circleRateStep = 8;
        work->stateTimer     = 0;
        work->stateCounter   = 0;
        Actor01900_D172FC    = 0;
        work->circleCount++;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    chase                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
        work->stateCounter++;
    } else {
        Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->stateCounter >= 7) {
        chase->playerYaw     = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->yawFromPlayer = actorNormalizeYaw(chase->yawFromPlayer);
        work->state          = ACTOR_01900_STATE_SLIDE;
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
    Actor01900_MoveForward(arg0->extra.tmd->coords, work->runStep);
    Actor01900_D172FC += work->runStep;
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
            if (yaw <= 0x400) {
                work->state     = ACTOR_01900_STATE_SLIDE;
                work->prevState = -1;
            }
        }
    }
    work->animRate += work->circleRateStep;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn0551C(Task* arg0)
{
    _Actor01900Work*   work;
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
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 3;
        work->blendActive       = 0;
        work->lookYawTarget     = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
        coord               = arg0->extra.tmd->coords;
        chase->turn         = actorNormalizeYaw(ratan2(head[-1].delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
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
    Actor01900_Fn01C94(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->turnYaw == work->turnYawTarget) {
        if (work->circleCount < 2 || overlayOutOfRange(&chase->delta, 0x384)) {
            work->state = ACTOR_01900_STATE_CIRCLE;
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn05B4C(Task* arg0)
{
    _Actor01900Work*   work;
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
        work->hitBody.radius    = 0xC0;
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
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate    = 0xC;
        work->blendActive = 0;
        Actor01900_Fn01C94(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->sidestepDir;
        gfxReadMatrixZAxis(&mat, dir);
        VectorNormalSS(dir, dir);
        work->sidestepStep = 0xDE;
        work->sidestepCount++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
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
    if ((u32)((u16)work->stateTimer - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
            work->sidestepStep >>= 1;
        }
    }
    if (++work->stateTimer >= 0x1E) {
        work->state     = ACTOR_01900_STATE_CHASE;
        work->prevState = -1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn05F38(Task* arg0)
{
    _Actor01900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->animRate    = 0x10;
        work->animId      = 7;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
        work->stateTimer  = 0;
    }
    Actor01900_Fn01C94(arg0);
    if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 0x10) < 7U) {
        coord = arg0->extra.tmd->coords;
        Actor01900_StepForward(coord, -0x78);
        ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (enemy->node.state.parts.targeted == 1) {
            work->state = ACTOR_01900_STATE_SIDESTEP;
        } else {
            work->state = ACTOR_01900_STATE_ALERT;
        }
    }
}

static void Actor01900_Fn06100(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 8;
        work->animId            = 3;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->circleCount = 0;
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    coord               = arg0->extra.tmd->coords;
    aim->turn           = actorNormalizeYaw(ratan2(aim->delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->lookYawTarget = aim->turn;
    if (aim->turn < 0x200) {
        overlayOutOfRange(&aim->delta, 0x384);
    }
    if (aim->turn > 0x40) {
        aim->turn = 0x40;
    }
    if (aim->turn < -0x40) {
        aim->turn = -0x40;
    }
    facing     = arg0->extra.tmd->coords;
    aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x28);
    } else {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0x14);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn06634(Task* arg0)
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
        work->animRequest             = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animId                  = 0xA;
        work->blendActive             = 0;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0 && work->commandBytes[0] != 1 && work->commandBytes[1] != 3 && work->commandBytes[2] != 2) {
            Gp_SetStateF0Byte3(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    if (work->animId == 0xA) {
        Actor01900_StepForwardHead(arg0->extra.tmd->coords, -0x57);
    }
    Actor01900_Fn01C94(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (work->animId == 0xA) {
            work->animId      = 0xB;
            work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
            Actor01900_Fn01C94(arg0);
        }
        if ((work->rig.slots[1].status.fields.flags & 0x100) && work->animId == 0xB) {
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
                Gp_ReleaseStateF0Add(arg0, 0x13);
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
    Actor01900_Fn01C94(arg0);
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
            func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->effectArg);
        }
    }
    work->dormantAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
    coord                  = arg0->extra.tmd->coords;
    d                      = &delta;
    delta.vx               = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy                  = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz                  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, work->noticeRange)) {
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_PATIO_STRANGER_DORMANT, 1);
        if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
            work->state = ACTOR_01900_STATE_ALERT;
        }
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
        work->state = ACTOR_01900_STATE_ALERT;
    }
}

/// Patrol state: walks toward the waypoint `patrolTarget` selects, turning at most
/// 0x20 per step and swapping waypoints on arrival or after 0x15 steps; switches
/// to state 6 when the player comes within `noticeRange`, or within 0xFA0 and in
/// front.
static void Actor01900_Fn06F40(Task* arg0)
{
    _Actor01900Work*  work;
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
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 2;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->stateTimer = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->delta.vx = work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
    if (!overlayOutOfRange(&turn->delta, 0xA0) || work->stateTimer >= 0x15) {
        if (work->patrolTarget == 0) {
            work->patrolTarget = 1;
        } else {
            work->patrolTarget = 0;
        }
        work->stateTimer = 0;
    }
    Actor01900_Fn01C94(arg0);
    coord               = arg0->extra.tmd->coords;
    turn->angle         = actorNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    if (work->blendActive == 0) {
        Actor01900_StepForward(arg0->extra.tmd->coords, 0xA);
    }
    if ((arg0->spawnArg1.value >> 16) != 0x10) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1 && ABS(work->lookYawTarget) < 0x80) {
            work->stateTimer++;
        } else {
            Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    } else {
        if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1 ||
             ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 1) &&
            ABS(work->lookYawTarget) < 0x80) {
            work->stateTimer++;
        } else {
            Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (detectSightBlocked(arg0) != 1) {
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &turn->delta);
        if (!overlayOutOfRange(&turn->delta, work->noticeRange)) {
            if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
                work->state = ACTOR_01900_STATE_ALERT;
            }
        } else if (!overlayOutOfRange(&turn->delta, 0xFA0)) {
            coord       = arg0->extra.tmd->coords;
            turn->angle = actorNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            if (ABS(turn->angle) < 0x300) {
                if (Actor01900_ArmIfPlayerLevel(arg0) == 1) {
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

static void Actor01900_Fn07810(Task* arg0)
{
    _Actor01900Work*  work;
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
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
        obj->flags        = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = 0x180;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
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
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    Actor01900_MoveForward(arg0->extra.tmd->coords, work->runStep);
    if (work->runStep > 0) {
        next          = work->runStep - 0xA;
        work->runStep = next;
        if ((s16)next < 0) {
            work->runStep = 0;
        }
    }
    Actor01900_Fn01C94(arg0);
    if ((work->rig.slots[1].status.fields.flags & 0x100) || work->runStep == 0) {
        work->state = ACTOR_01900_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static void Actor01900_Fn07BA8(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 0x16;
        work->animId            = 2;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        return;
    }
    Actor01900_Fn01C94(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn           = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (ABS(aim->turn) <= 0x80 && work->animId == 2) {
        work->animRate    = 0x16;
        work->animId      = 0x11;
        work->animRequest = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        Actor01900_Fn01C94(arg0);
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 0x11) {
        work->stateTimer++;
        Actor01900_StepForward(arg0->extra.tmd->coords, -0x10);
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            Actor01900_Fn03FF8(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->stateTimer >= 0x13) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->state = ACTOR_01900_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn080A8(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 9;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        Actor01900_Fn01C94(arg0);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer++;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = ACTOR_01900_STATE_CHASE;
    }
    aim->turn           = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn > 0) {
        aim->turn = 0;
    }
    if (aim->turn < 0) {
        aim->turn = 0;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    Actor01900_Fn01C94(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Turn the actor toward the player at up to 0x28 per call. Takes a 0x10-byte
/// scratch block from the scratch stack for the offset to the player and the
/// yaw, steps `lookYawTarget` toward that yaw, then rebuilds the root coordinate's
/// Y rotation from its own facing. The `stateEntered` branch is the state's entry.
static void Actor01900_Fn083E8(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate          = 0x10;
        work->animId            = 0x13;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        Actor01900_Fn01C94(arg0);
        Actor01900_Fn01C94(arg0);
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
    coord     = arg0->extra.tmd->coords;
    aim->turn = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    work->animRequest = ACTOR_01900_ANIM_REQUEST_RESET;
    Actor01900_Fn01C94(arg0);
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
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
        Gp_ReleaseStateF0Add(arg0, 0x13);
    }
    work->stateTimer++;
    switch (work->stateTimer) {
        case 3:
            D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
            vec.vz                   = 0x64;
            vec.vy                   = 0;
            vec.vx                   = 0;
            eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec);
            goto body;
        case 4:
            D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
            vec.vy                   = 0;
            vec.vx                   = 0;
            eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec);
        body:
            if (eff != NULL) {
                actorTintTask(eff->task, enemy);
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
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
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
            Actor01900_StepForwardHead(arg0->extra.tmd->coords, 0xA);
            ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
            if (work->stateTimer == 3) {
                D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
                vec.vz                   = 0x64;
                vec.vy                   = 0;
                vec.vx                   = 0;
                eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec);
                actorTintEffect(eff, enemy);
            }
            if (work->stateTimer == 5) {
                D_80114B34[5].data.model = &_gActor01900StrangerBurstHand;
                eff                      = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL);
                actorTintEffect(eff, enemy);
            }
            break;
        case 0x18:
            if (!(work->rig.slots[1].status.fields.flags & 0x100)) {
                work->stateTimer = 0;
            }
            switch ((s16)(work->stateTimer - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0x13);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 39:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->state            = ACTOR_01900_STATE_HIDDEN;
                    break;
            }
            cur = work->stateTimer;
            if (cur >= 0x1A) {
                actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    Actor01900_Fn01C94(arg0);
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

/// Whether any of the three `WorldCollisionContact` at `records` carries an id with high
/// word 1, stopping at the first empty record.
static __inline__ s32 Actor01900_HasHit(WorldCollisionContact* records)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Entered from a state change: rebuilds the model buffers and arms the player
/// if they are level with the actor, then each step turns the root coordinate
/// toward the player by at most 0x30, rescales it by 0x1194, and once the
/// actor is out of range of the player hands the work state on.
static void Actor01900_Fn09694(Task* arg0)
{
    _Actor01900Work*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* aim;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = 0x180;
        work->animRequest       = ACTOR_01900_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 4;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Actor01900_Fn01C94(arg0);
        work->circleCount = 0;
        if (*(u16*)work->commandBytes != 0x301) {
            Actor01900_ArmIfPlayerLevel(arg0);
        }
        work->stateTimer        = 0;
        work->stateCounter      = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    work->stateTimer++;
    if (work->stateTimer == 0x16) {
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->stateTimer == 0x1D) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (Actor01900_HasHit(work->attackContacts) == 1) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor01900_Fn01C94(arg0);
    if (work->stateTimer < 0xE) {
        coord = arg0->extra.tmd->coords;
        aim->turn =
            actorNormalizeYaw(ratan2(aim->delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = aim->turn;
        if (aim->turn > 0x30) {
            aim->turn = 0x30;
        }
        if (aim->turn < -0x30) {
            aim->turn = -0x30;
        }
        facing     = arg0->extra.tmd->coords;
        aim->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (overlayOutOfRange(&aim->delta, 0x2BC)) {
            work->state = ACTOR_01900_STATE_ALERT;
        } else {
            work->state = ACTOR_01900_STATE_STEP_BACK;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

static void Actor01900_Fn09BE8(Task* arg0)
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
        work->animId                  = 0xB;
        work->animRate                = 0x10;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        if (enemy->hp < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    Actor01900_Fn01C94(arg0);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & 0x100) {
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
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->state;
            if ((state != ACTOR_01900_STATE_HIDDEN) && (state != ACTOR_01900_STATE_DEATH_BURN) && (state != ACTOR_01900_STATE_DEATH_BURST) && (state != ACTOR_01900_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ACTOR_01900_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->state;
            if ((state != ACTOR_01900_STATE_HIDDEN) && (state != ACTOR_01900_STATE_DEATH_BURN) && (state != ACTOR_01900_STATE_DEATH_BURST) && (state != ACTOR_01900_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ACTOR_01900_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->gridContacts);
            Gp_ClearRec18Occupied(work->hitContacts);
            Gp_ClearRec18Occupied(work->attackContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->gridContacts);
            Gp_ClearRec18Occupied(work->hitContacts);
            Gp_ClearRec18Occupied(work->attackContacts);
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
    Gp_ClearRec18Occupied(work->gridContacts);
    Gp_ClearRec18Occupied(work->hitContacts);
    Gp_ClearRec18Occupied(work->attackContacts);
    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->state == ACTOR_01900_STATE_PATROL)) {
        work->state = ACTOR_01900_STATE_ALERT;
    }

    scratch->position.vx = 0;
    scratch->position.vy = 0;
    scratch->position.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->position);

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

s32 Actor01900_Fn0A314(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The actor's state handlers, indexed by `_Actor01900Work::state`; empty
/// slots are states the actor never enters. `Actor01900_Fn09D3C` copies the
/// table to its frame before dispatching.
static const _Actor01900StateTable Actor01900_D001BC = { {
    Actor01900_Fn0A764,
    Actor01900_Fn0A7C0,
    Actor01900_Fn0A868,
    Actor01900_Fn0A914,
    Actor01900_Fn03710,
    Actor01900_Fn0A9C0,
    Actor01900_Fn03854,
    Actor01900_Fn042BC,
    Actor01900_Fn04D14,
    Actor01900_Fn0551C,
    Actor01900_Fn05B4C,
    Actor01900_Fn09694,
    NULL,
    NULL,
    Actor01900_Fn05F38,
    Actor01900_Fn0AA78,
    NULL,
    Actor01900_Fn0AB1C,
    Actor01900_Fn06100,
    Actor01900_Fn06634,
    NULL,
    Actor01900_Fn06904,
    NULL,
    Actor01900_Fn06B4C,
    Actor01900_Fn06F40,
    Actor01900_Fn07BA8,
    Actor01900_Fn07810,
    Actor01900_Fn080A8,
    Actor01900_Fn083E8,
    Actor01900_Fn08724,
    Actor01900_Fn0892C,
    Actor01900_Fn09BE8,
} };

/// The actor task's dispatcher table, indexed by `Task::state`: spawn
/// (`Actor01900_Fn02018`), a three-frame wait (`Actor01900_Fn0ABA0`), the
/// per-frame tick (`Actor01900_Fn09D3C`) and teardown.
static const EnemyTaskFuncTable4 Actor01900_D0023C = { {
    Actor01900_Fn02018,
    Actor01900_Fn0ABA0,
    Actor01900_Fn09D3C,
    enemyDestroy,
} };

s32 Actor01900_Fn0A31C(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    _Actor01900Work* work = arg0->work;

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
    work->state     = ACTOR_01900_STATE_DOWN;
    work->prevState = -1;
    return 0;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

s32 Actor01900_Fn0A59C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 1;
}

s32 Actor01900_Fn0A5A4(Task* arg0, s32 arg1, u16* arg2, s32 arg3)
{
    u16              room;
    u16              state;
    u16              state2;
    _Actor01900Work* work;

    work                  = arg0->work;
    work->commandBytes[0] = ((u8*)arg2)[0];
    work->commandBytes[1] = ((u8*)arg2)[1];
    work->commandBytes[2] = ((u8*)arg2)[2];
    room                  = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 0:
                work->state = ACTOR_01900_STATE_HIDDEN;
                return 1;
            case 1:
                work->state = ACTOR_01900_STATE_DORMANT_SCRIPTED;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->state = ACTOR_01900_STATE_HIDDEN;
                return 1;
            case 2:
                work->state                         = ACTOR_01900_STATE_SCRIPTED_WATCH;
                arg0->extra.tmd->coords->coord.t[0] = -0x595;
                arg0->extra.tmd->coords->coord.t[1] = 0;
                arg0->extra.tmd->coords->coord.t[2] = -0x5B1;
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

static void Actor01900_Fn0A6CC(Task* task)
{
    _Actor01900Work* work;
    Enemy*           enemy;

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

static void Actor01900_Fn0A764(Task* arg0)
{
    TmdObject*       obj;
    _Actor01900Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags                                    = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags                                      = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

static void Actor01900_Fn0A7C0(Task* arg0)
{
    TmdObject*       obj;
    _Actor01900Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 2;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Actor01900_Fn01C94(arg0);
    }
}

static void Actor01900_Fn0A868(Task* arg0)
{
    TmdObject*       obj;
    _Actor01900Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 3;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Actor01900_Fn01C94(arg0);
    }
}

static void Actor01900_Fn0A914(Task* arg0)
{
    TmdObject*       obj;
    _Actor01900Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest      = ACTOR_01900_ANIM_REQUEST_RESET;
        work->animRate         = 0x10;
        work->animId           = 0xB;
        work->blendActive      = 0;
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags   = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        Actor01900_Fn01C94(arg0);
    } else {
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Actor01900_Fn01C94(arg0);
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
    Actor01900_Fn01C94(arg0);
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
    Actor01900_Fn01C94(arg0);
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
