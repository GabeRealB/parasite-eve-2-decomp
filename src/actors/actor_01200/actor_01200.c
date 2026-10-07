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
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"
/// This file's `_actorContactApplyAvoidancePushback` returns `s16`.
///
/// Defined before `actor_contacts.h`, which otherwise declares the return as
/// `s32`. Callers here compare the return with 1, and that comparison
/// sign-extends a 16-bit result.
#define ACTOR_CONTACT_STEER_RESULT s16
#include "../../shared/actor_contacts.h"

/// Values of `_Actor01200Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Animation numbers are indices into the package's animation-set table. Both
/// burst states end the same way: the model swells and its colour fades while
/// `burstAttackBody` and `burstWaveBody` are switched on for a few ticks, the
/// model is hidden and the actor goes to `HIDDEN`.
enum {
    ACTOR_01200_STATE_HIDDEN        = 0, // not drawn, not lockable, all four bodies off
    ACTOR_01200_STATE_SETTLE        = 1, // plays animation 4 to its boundary, then idles
    ACTOR_01200_STATE_IDLE          = 2, // loops animation 5; rouses at random after 25 loops, or when the player comes within 2000
    ACTOR_01200_STATE_ROUSE         = 3, // plays animation 6 to its boundary, then patrols
    ACTOR_01200_STATE_CHASE         = 4, // runs at the player on animation 3; bursts after 21 ticks within 1000, returns after 240 ticks farther away
    ACTOR_01200_STATE_SELF_BURST    = 5, // bursts of its own accord: plays animation 10, reddens from tick 0x17 and bursts from tick 0x29
    ACTOR_01200_STATE_DEATH_BURST   = 6, // entered when the hit points run out or a moving state's steering reports a blocking contact: plays animation 10 fast and bursts from tick 13
    ACTOR_01200_STATE_PATROL        = 7, // walks between the two `patrolPoints` on animation 2 until it notices the player; settles at random after 20 loops
    ACTOR_01200_STATE_RETURN        = 8, // walks back to `spawnPos` on animation 2, then patrols
    ACTOR_01200_STATE_WALK_IN_PLACE = 9, // loops animation 2 without moving itself; entered only by actor command 4
    ACTOR_01200_STATE_COUNT              // number of states, and of the handlers in `_Actor01200StateTable`
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the state machine, the animation rig and its driver's state, four
/// collision bodies with their contact records, the points the actor walks
/// between, and storage for the model's matrices.
///
/// Everything up to and including `driver` is laid out as `AnimDriverWork`,
/// through which the shared animation driver reaches the block: its
/// `carrierState` bytes are the five words and the gap ahead of `rig` here. A
/// cue index is the low ten bits of the record a slot's current pose names
/// (`ANIMATION_POSE_CUE_INDEX_MASK`).
typedef struct {
    s16           state;                          // `ACTOR_01200_STATE_*`
    s16           prevState;                      // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16           stateEntered;                   // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16           stateFrame;                     // tick counter of the running state, cleared on entry by the states that time themselves; `PATROL` counts its blocked ticks in it
    s16           chaseNearFrames;                // consecutive `CHASE` ticks with the player nearer than 1000; at 21 the actor bursts
    byte          pad_A[2];                       // never accessed
    ActorAnimRig6 rig;                            // playback of the model's parts; slot 1's status and cue index time the states
    struct {
        s16 state;                                // `ANIM_DRIVER_STATE_*` (0 idle, 1 or 2 restart requested, 3 playing)
        s16 playingSet;                           // animation the slots were last restarted on
        s16 requestedSet;                         // animation the next restart plays; also what the sound cues are keyed on
        s16 rate;                                 // playback rate in sixteenths of a frame per tick; from spawn 16 plus the placement index (odd placements) or minus half of it (even), until `DEATH_BURST` sets 44
        s16 rateBias;                             // added to `rate`; `CHASE` runs with 16, `SELF_BURST` with 8, every other state with 0
        s16 tickCount;                            // advancing ticks since the last restart
        s16 jumpCount;                            // of those, ticks on which slot 1 followed a control jump: loops of a looping animation
    } driver;                                     // the animation driver's state, member for member `AnimDriverWork`'s
    s16                   field_17E;              // cleared at spawn and never accessed again; role unproven
    byte                  pad_180[0x14];          // never accessed
    u8                    lastCommandStage;       // stage tag of the last actor command received, whatever its namespace; never read
    u8                    lastCommandArea;        // area tag of that command; never read
    u8                    lastCommand;            // low byte of that command's selector; never read
    byte                  pad_197[1];             // never accessed
    u16                   field_198;              // 5 at spawn, offset by the placement index the way `driver.rate` is; never accessed again; role unproven
    u16                   field_19A;              // 20 at spawn, offset the same way; never accessed again; role unproven
    byte                  pad_19C[0xC];           // never accessed
    EffectSpawnArg        effectArg;              // argument record of the effects a hit and a burst spawn; names the part they hang off
    SVECTOR               hitEffectOffset;        // offset of a hit's effect from its part, picked at random by the side the hit came from; `pad` holds the part index
    WorldCollisionContact gridContacts[5];        // contacts of `gridBody`, which push the root out of what it walks into
    WorldCollisionBody    gridBody;               // sphere of radius 180 on the root that the room grid tests; off in `HIDDEN` and `DEATH_BURST`
    WorldCollisionContact hitContacts[5];         // contacts of `hitBody`; also the enemy's hit records, which damage and steering read
    WorldCollisionBody    hitBody;                // sphere of radius 360 on part 2 that takes the hits; off in `HIDDEN` and both bursts
    WorldCollisionContact burstAttackContacts[1]; // contact of `burstAttackBody`
    WorldCollisionBody    burstAttackBody;        // world-space sphere at the root carrying the key of attack 0; on, with radius 800, only for one tick of a burst
    WorldCollisionContact burstWaveContacts[1];   // contact of `burstWaveBody`; never cleared by the tick
    WorldCollisionBody    burstWaveBody;          // world-space sphere 400 above the root with key 0x22121 that grows from 200 to 800 through a burst
    SVECTOR               spawnPos;               // root position at spawn; `RETURN` walks back to it
    SVECTOR               patrolPoints[2];        // `spawnPos` plus (0) and minus (1) 1000 along the facing at spawn; only X and Z are used
    s16                   patrolIndex;            // `patrolPoints` entry `PATROL` walks toward; swapped on arrival or after 97 blocked ticks
    byte                  pad_372[2];             // never accessed
    MATRIX                lightMtx;               // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;               // storage for the model's `TmdObject::colorMtx`
    MATRIX                savedColorMtx;          // `colorMtx` as a burst state found it; each tick reddens and scales a fresh copy
    u16                   lastSoundCueIndex;      // slot 1's cue index when the walk or run cue sound last fired, so a held cue sounds once; 0 on any other cue
    byte                  pad_3D6[2];             // never accessed
    s8                    field_3D8;              // never written by the package, so 0; nonzero would have the tick rebuild the model's colour matrix from the room lights after the state handler; role unproven
    byte                  pad_3D9[3];             // never accessed
    s16                   chaseFarFrames;         // consecutive `CHASE` ticks with the player at 1000 or farther; past 240 the actor returns
    byte                  pad_3DE[2];             // never accessed
} _Actor01200Work;
STATIC_ASSERT_SIZEOF(_Actor01200Work, 0x3E0);
STATIC_ASSERT(OFFSET_OF(_Actor01200Work, rig) == OFFSET_OF(AnimDriverWork, rig), Actor01200Work_rig);
STATIC_ASSERT(OFFSET_OF(_Actor01200Work, driver) == OFFSET_OF(AnimDriverWork, state), Actor01200Work_driver);

/// The actor's state handlers, indexed by `_Actor01200Work::state`.
///
/// The package defines one table. The per-frame tick copies it to the stack
/// before calling the entry of the current state with the enemy and its task.
/// The call is unconditional and every entry is a handler.
typedef struct {
    EnemyTaskFunc handlers[ACTOR_01200_STATE_COUNT]; // Handler of each `ACTOR_01200_STATE_*`
} _Actor01200StateTable;
STATIC_ASSERT_SIZEOF(_Actor01200StateTable, ACTOR_01200_STATE_COUNT * sizeof(EnemyTaskFunc));

extern EnemyParams               Actor01200_D04034;
extern PadScriptCmd              Actor01200_D04044[3];
extern PadScriptVibrationSegment Actor01200_D04050[3];
extern AnimationSet*             Actor01200_D06F98[19]; // animation bank handed to `animationInitContext`
// Typed callback views for the task message dispatcher.

extern TaskMessageEntry Actor01200_D07058[4];

/// Integer part of the last movement step `_actorContactApplyGridPushback` applied.
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

static void Actor01200_Fn03D58(Enemy* arg0, Task* arg1);
static void Actor01200_Fn03DC0(Enemy* arg0, Task* arg1);
static void Actor01200_Fn03E78(Enemy* arg0, Task* arg1);
static void Actor01200_Fn03F30(Enemy* arg0, Task* arg1);

static TmdSource _gActor01200BoneSucklerBody;
void             Actor01200_Fn03FD4(Task*);

s32 Actor01200_Fn03A00(Task*, s32, s32, s32);
s32 Actor01200_Fn03ABC(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

DamageAttack Actor01200_D04030[1] = {
    { 24, 7 },
};

EnemyParams Actor01200_D04034 = { Actor01200_D04030, 1, 6, 20, 3, 100, 0, 100, 0 };

PadScriptCmd Actor01200_D04044[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment Actor01200_D04050[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor01200BoneSucklerBodySkeleton[6] = {
#include "assets/bone_suckler_body_skeleton.inc"
};

static u32 _gActor01200BoneSucklerBodyPartVerts[6] = {
#include "assets/bone_suckler_body_partVerts.inc"
};

static SVECTOR _gActor01200BoneSucklerBodyVerts[102] = {
#include "assets/bone_suckler_body_verts.inc"
};

static SVECTOR _gActor01200BoneSucklerBodyNormals[139] = {
#include "assets/bone_suckler_body_normals.inc"
};

static u32 _gActor01200BoneSucklerBodyStream[1048] = {
#include "assets/bone_suckler_body_stream.inc"
};

static TmdSource _gActor01200BoneSucklerBody = {
    0,
    5984,
    1264,
    6,
    _gActor01200BoneSucklerBodyPartVerts,
    _gActor01200BoneSucklerBodyVerts,
    _gActor01200BoneSucklerBodyNormals,
    _gActor01200BoneSucklerBodySkeleton,
    _gActor01200BoneSucklerBodyStream,
};

static AnimationPackedPose _gActor01200Actor101200Animation05A94Bank1[4] = {
#include "assets/actor_101200_animation_05A94_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation05A94Bank4[21] = {
#include "assets/actor_101200_animation_05A94_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation05A94Records[43] = {
#include "assets/actor_101200_animation_05A94_records.inc"
};

static u16 _gActor01200Actor101200Animation05A94Indices[6] = {
#include "assets/actor_101200_animation_05A94_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation05A94 = {
    _gActor01200Actor101200Animation05A94Records,
    _gActor01200Actor101200Animation05A94Indices,
    { NULL, _gActor01200Actor101200Animation05A94Bank1, NULL, NULL, _gActor01200Actor101200Animation05A94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation05D58Bank1[19] = {
#include "assets/actor_101200_animation_05D58_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation05D58Bank4[36] = {
#include "assets/actor_101200_animation_05D58_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation05D58Records[71] = {
#include "assets/actor_101200_animation_05D58_records.inc"
};

static u16 _gActor01200Actor101200Animation05D58Indices[6] = {
#include "assets/actor_101200_animation_05D58_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation05D58 = {
    _gActor01200Actor101200Animation05D58Records,
    _gActor01200Actor101200Animation05D58Indices,
    { NULL, _gActor01200Actor101200Animation05D58Bank1, NULL, NULL, _gActor01200Actor101200Animation05D58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation05FFCBank1[20] = {
#include "assets/actor_101200_animation_05FFC_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation05FFCBank4[31] = {
#include "assets/actor_101200_animation_05FFC_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation05FFCRecords[65] = {
#include "assets/actor_101200_animation_05FFC_records.inc"
};

static u16 _gActor01200Actor101200Animation05FFCIndices[6] = {
#include "assets/actor_101200_animation_05FFC_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation05FFC = {
    _gActor01200Actor101200Animation05FFCRecords,
    _gActor01200Actor101200Animation05FFCIndices,
    { NULL, _gActor01200Actor101200Animation05FFCBank1, NULL, NULL, _gActor01200Actor101200Animation05FFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation061DCBank1[6] = {
#include "assets/actor_101200_animation_061DC_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation061DCBank4[38] = {
#include "assets/actor_101200_animation_061DC_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation061DCRecords[51] = {
#include "assets/actor_101200_animation_061DC_records.inc"
};

static u16 _gActor01200Actor101200Animation061DCIndices[6] = {
#include "assets/actor_101200_animation_061DC_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation061DC = {
    _gActor01200Actor101200Animation061DCRecords,
    _gActor01200Actor101200Animation061DCIndices,
    { NULL, _gActor01200Actor101200Animation061DCBank1, NULL, NULL, _gActor01200Actor101200Animation061DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06314Bank1[4] = {
#include "assets/actor_101200_animation_06314_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06314Bank4[13] = {
#include "assets/actor_101200_animation_06314_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06314Records[40] = {
#include "assets/actor_101200_animation_06314_records.inc"
};

static u16 _gActor01200Actor101200Animation06314Indices[6] = {
#include "assets/actor_101200_animation_06314_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06314 = {
    _gActor01200Actor101200Animation06314Records,
    _gActor01200Actor101200Animation06314Indices,
    { NULL, _gActor01200Actor101200Animation06314Bank1, NULL, NULL, _gActor01200Actor101200Animation06314Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation064B4Bank1[7] = {
#include "assets/actor_101200_animation_064B4_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation064B4Bank4[28] = {
#include "assets/actor_101200_animation_064B4_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation064B4Records[42] = {
#include "assets/actor_101200_animation_064B4_records.inc"
};

static u16 _gActor01200Actor101200Animation064B4Indices[6] = {
#include "assets/actor_101200_animation_064B4_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation064B4 = {
    _gActor01200Actor101200Animation064B4Records,
    _gActor01200Actor101200Animation064B4Indices,
    { NULL, _gActor01200Actor101200Animation064B4Bank1, NULL, NULL, _gActor01200Actor101200Animation064B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation066C8Bank1[6] = {
#include "assets/actor_101200_animation_066C8_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation066C8Bank4[42] = {
#include "assets/actor_101200_animation_066C8_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation066C8Records[60] = {
#include "assets/actor_101200_animation_066C8_records.inc"
};

static u16 _gActor01200Actor101200Animation066C8Indices[6] = {
#include "assets/actor_101200_animation_066C8_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation066C8 = {
    _gActor01200Actor101200Animation066C8Records,
    _gActor01200Actor101200Animation066C8Indices,
    { NULL, _gActor01200Actor101200Animation066C8Bank1, NULL, NULL, _gActor01200Actor101200Animation066C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06AD8Bank1[17] = {
#include "assets/actor_101200_animation_06AD8_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06AD8Bank4[85] = {
#include "assets/actor_101200_animation_06AD8_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06AD8Records[111] = {
#include "assets/actor_101200_animation_06AD8_records.inc"
};

static u16 _gActor01200Actor101200Animation06AD8Indices[6] = {
#include "assets/actor_101200_animation_06AD8_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06AD8 = {
    _gActor01200Actor101200Animation06AD8Records,
    _gActor01200Actor101200Animation06AD8Indices,
    { NULL, _gActor01200Actor101200Animation06AD8Bank1, NULL, NULL, _gActor01200Actor101200Animation06AD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06D98Bank1[12] = {
#include "assets/actor_101200_animation_06D98_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06D98Bank4[52] = {
#include "assets/actor_101200_animation_06D98_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06D98Records[75] = {
#include "assets/actor_101200_animation_06D98_records.inc"
};

static u16 _gActor01200Actor101200Animation06D98Indices[6] = {
#include "assets/actor_101200_animation_06D98_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06D98 = {
    _gActor01200Actor101200Animation06D98Records,
    _gActor01200Actor101200Animation06D98Indices,
    { NULL, _gActor01200Actor101200Animation06D98Bank1, NULL, NULL, _gActor01200Actor101200Animation06D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor01200Actor101200Animation06F70Bank1[5] = {
#include "assets/actor_101200_animation_06F70_bank1.inc"
};

static AnimationPackedRotation _gActor01200Actor101200Animation06F70Bank4[19] = {
#include "assets/actor_101200_animation_06F70_bank4.inc"
};

static AnimationRecord _gActor01200Actor101200Animation06F70Records[71] = {
#include "assets/actor_101200_animation_06F70_records.inc"
};

static u16 _gActor01200Actor101200Animation06F70Indices[6] = {
#include "assets/actor_101200_animation_06F70_indices.inc"
};

static AnimationSet _gActor01200Actor101200Animation06F70 = {
    _gActor01200Actor101200Animation06F70Records,
    _gActor01200Actor101200Animation06F70Indices,
    { NULL, _gActor01200Actor101200Animation06F70Bank1, NULL, NULL, _gActor01200Actor101200Animation06F70Bank4, NULL, NULL, NULL },
};

AnimationSet* Actor01200_D06F98[19] = {
    NULL,
    &_gActor01200Actor101200Animation05A94,
    &_gActor01200Actor101200Animation05D58,
    &_gActor01200Actor101200Animation05FFC,
    &_gActor01200Actor101200Animation061DC,
    &_gActor01200Actor101200Animation06314,
    &_gActor01200Actor101200Animation064B4,
    &_gActor01200Actor101200Animation066C8,
    &_gActor01200Actor101200Animation06AD8,
    &_gActor01200Actor101200Animation06D98,
    &_gActor01200Actor101200Animation06F70,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

u8 Actor01200_D06FE4[116] = {
    6,
    0,
    0,
    0,
    0,
    0,
    9,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    6,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

TaskMessageEntry Actor01200_D07058[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor01200_Fn03A00 },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor01200_Fn03ABC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor01200_D07078 = { { { TASK_BODY_TMD, 96 } }, Actor01200_Fn03FD4, { .model = &_gActor01200BoneSucklerBody } };

SVECTOR ActorContact_ScratchPosition = { 0 };

static s32             Actor01200_Fn00990(_Actor01200Work* arg0);
static void            Actor01200_Fn00A6C(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn01040(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn01234(Enemy* arg0, Task* arg1);
static __inline__ void Actor01200_FaceScale(GfxCoord* coord, s16 s);
static void            Actor01200_Fn017DC(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn01FDC(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn026A0(Task* arg0, s16 arg1, u32 arg2);
static void            Actor01200_Fn02918(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn02BE8(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn03294(Enemy* arg0, Task* arg1);
static void            Actor01200_Fn036B0(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// Sound check for the tick: for animations 2 and 3 (`driver.requestedSet`) it
/// returns sound 0x400C0001 the first time rig slot 1's cue index reaches one
/// of that animation's two trigger values, latching the value in
/// `lastSoundCueIndex` so it reports once; for animation 4 it returns 0x400C0005 while rig slot 1
/// reports `ANIMATION_SLOT_REACHED_BOUNDARY`. Returns 0 otherwise.
static s32 Actor01200_Fn00990(_Actor01200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->driver.requestedSet) {
        case 2:
            id = arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->lastSoundCueIndex == v) {
                goto same;
            }
            arg0->lastSoundCueIndex = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->lastSoundCueIndex = 0;
            break;
        case 3:
            id = arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->lastSoundCueIndex = id;
            break;
        case 4:
            if (arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

static void Actor01200_Fn00A6C(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    _Actor01200Work*       work;
    WorldCollisionContact* hits;
    SVECTOR                sv;
    VECTOR                 pos;
    SVECTOR*               p;
    SVECTOR*               q;
    WorldCollisionBody*    o1;
    WorldCollisionBody*    o2;
    WorldCollisionBody*    o3;
    WorldCollisionBody*    o4;

    obj        = arg1->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(_Actor01200Work), 0);
    arg1->work = work;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->msgTable = Actor01200_D07058;
    coord->parent  = &gGfxViewCoord;
    obj->flags     = 0;
    animationInitContext(&work->rig.anim, Actor01200_D06F98, obj, work->rig.poses, work->rig.slots);

    o1                   = &work->gridBody;
    o1->context.contacts = work->gridContacts;
    o1->pos.vy           = -0x34;
    o1->coord            = coord;
    o1->pos.vx           = 0;
    o1->pos.vz           = 0;
    o1->key              = 0x3000C;
    o1->radius           = 0xB4;
    o1->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, o1);
    o1->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(o1->context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    o2                   = &work->hitBody;
    sv.vx                = 0;
    sv.vy                = -0x168;
    sv.vz                = 0;
    p                    = &sv;
    hits                 = work->hitContacts;
    o2->coord            = arg1->extra.tmd->coords + 2;
    o2->context.contacts = hits;
    o2->pos.vx           = p->vx;
    o2->pos.vy           = p->vy;
    o2->pos.vz           = p->vz;
    o2->key              = 0x3000C;
    o2->radius           = 0x168;
    o2->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, o2);
    o2->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(o2->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    o3                   = &work->burstAttackBody;
    sv.vx                = 0;
    sv.vy                = 0;
    sv.vz                = 0;
    o3->coord            = &gGfxViewCoord;
    o3->context.contacts = work->burstAttackContacts;
    o3->pos.vx           = p->vx;
    o3->pos.vy           = p->vy;
    o3->pos.vz           = p->vz;
    o3->radius           = 0x500;
    o3->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, o3);
    worldCollisionInitContacts(o3->context.contacts, ARRAY_SIZE(work->burstAttackContacts), 0);

    o4                   = &work->burstWaveBody;
    o4->coord            = &gGfxViewCoord;
    o4->context.contacts = work->burstWaveContacts;
    o4->pos.vx           = p->vx;
    o4->pos.vy           = p->vy;
    o4->pos.vz           = p->vz;
    o4->radius           = 0x80;
    o4->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, o4);
    worldCollisionInitContacts(o4->context.contacts, ARRAY_SIZE(work->burstWaveContacts), 0);

    arg0->field_4    = &coord->coord;
    arg0->field_48   = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->coord      = arg1->extra.tmd->coords + 2;
    worldTargetLinkNode(&arg0->node);
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    arg0->hp = arg0->hpMax = 1;
    arg0->reactionFlags    = 0;
    arg0->hp = arg0->hpMax    = Actor01200_D04034.hpMax;
    arg0->param               = &Actor01200_D04034;
    arg0->recs                = hits;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.requestedSet = 1;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(arg1);
    work->field_17E       = 0;
    work->chaseNearFrames = 0;
    obj->lightMtx         = &work->lightMtx;
    obj->colorMtx         = &work->colorMtx;
    coord->composeStamp   = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);
    work->field_198 = 5;
    work->field_19A = 0x14;
    if ((u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 2 == 1) {
        work->driver.rate += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_19A   += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_198   += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= (u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_19A   -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_198   -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }
    work->spawnPos.vx = arg1->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = arg1->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = arg1->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12();
    gte_stsv(q);
    work->patrolPoints[0].vx = arg1->extra.tmd->coords->coord.t[0] + sv.vx;
    work->patrolPoints[0].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrolPoints[0].vz = arg1->extra.tmd->coords->coord.t[2] + sv.vz;
    work->patrolPoints[1].vx = arg1->extra.tmd->coords->coord.t[0] - sv.vx;
    work->patrolPoints[1].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrolPoints[1].vz = arg1->extra.tmd->coords->coord.t[2] - sv.vz;
    (sceneAcquireBattleRef)(0);
    if ((arg1->spawnArg1.value >> 16) == 0) {
        work->state = ACTOR_01200_STATE_PATROL;
    } else if ((arg1->spawnArg1.value >> 16) == 1) {
        work->state = ACTOR_01200_STATE_IDLE;
    } else {
        work->state = ACTOR_01200_STATE_PATROL;
    }
    work->prevState            = -1;
    part                       = arg1->extra.tmd->coords;
    work->effectArg.spawnArgLo = 0x80;
    work->effectArg.spawnArgHi = 2;
    work->effectArg.coord      = part + 1;
    arg1->state++;
}

static void Actor01200_Fn01040(Enemy* arg0, Task* arg1)
{
    _Actor01200Work* work;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 5;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        return;
    }
    _animDriverTick(arg1);
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount >= 0x19) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->state = ACTOR_01200_STATE_ROUSE;
        }
    }
    coord    = arg1->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(d, 2000)) {
        sceneEngageBattle(1);
        work->state = ACTOR_01200_STATE_ROUSE;
    }
}

static void Actor01200_Fn01234(Enemy* arg0, Task* arg1)
{
    _Actor01200Work*  work;
    GfxCoord*         coord;
    GfxCoord*         facing;
    GfxCoord*         part;
    TmdObject*        obj;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 3;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0x10;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        work->chaseFarFrames = 0;
        sceneEngageBattle(1);
        return;
    }
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    turn                                   = head - 1;
    _animDriverTick(arg1);
    coord             = arg1->extra.tmd->coords;
    head[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    turn->delta.vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    turn->delta.vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    facing            = arg1->extra.tmd->coords;
    turn->angle       = _actorAngleNormalizeYaw(ratan2(head[-1].delta.vx, turn->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]));
    if (turn->angle > 0x10) {
        turn->angle = 0x10;
    }
    if (turn->angle < -0x10) {
        turn->angle = -0x10;
    }
    part         = arg1->extra.tmd->coords;
    turn->angle += ratan2(-part->coord.m[2][0], part->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, turn->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 0x14);
    _actorContactApplyGridPushback(arg1->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (_actorRangeOutsideRadiusXZ(&turn->delta, 1000)) {
        work->chaseFarFrames++;
    } else {
        work->chaseFarFrames = 0;
    }
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 1000)) {
        work->chaseNearFrames++;
    } else {
        work->chaseNearFrames = 0;
    }
    if (work->chaseNearFrames >= 0x15) {
        work->state = ACTOR_01200_STATE_SELF_BURST;
    }
    if (_actorContactApplyAvoidancePushback(arg1->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turn->delta) == 1) {
        work->state = ACTOR_01200_STATE_DEATH_BURST;
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    turn->delta.vx                        = work->spawnPos.vx - arg1->extra.tmd->coords->coord.t[0];
    turn->delta.vy                        = 0;
    turn->delta.vz                        = work->spawnPos.vz - arg1->extra.tmd->coords->coord.t[2];
    _actorRangeOutsideRadiusXZ(&turn->delta, 3000);
    if (work->chaseFarFrames >= 0xF1) {
        work->state = ACTOR_01200_STATE_RETURN;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static __inline__ void Actor01200_FaceScale(GfxCoord* coord, s16 s)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* sc;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    sc                                         = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;
    sc->yaw                                    = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&sc->rotation, sc->yaw, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->rotation, &head[-1].scale);
    coord->coord.m[0][0] = head[-1].rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

static void Actor01200_Fn017DC(Enemy* arg0, Task* arg1)
{
    SVECTOR          ofs;
    VECTOR           scale;
    _Actor01200Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    memset(&ofs, 0, 8);
    if (work->stateEntered != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->hitBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key    = damagePackEnemyAttackKey(arg0, 0);
        work->burstWaveBody.key      = 0x22121;
        work->stateFrame             = 0;
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx          = work->colorMtx;
        work->driver.requestedSet    = 0xA;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 8;
        _animDriverTick(arg1);
        work->burstWaveBody.pos.vx   = arg1->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = arg1->extra.tmd->coords->coord.t[1] - 0x190;
        work->burstWaveBody.pos.vz   = arg1->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        return;
    }
    _animDriverTick(arg1);
    switch (work->stateFrame) {
        case 0x29:
            arg1->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
            ofs.vx                  = 0x1E;
            ofs.vz                  = 0x1E;
            ofs.vy                  = -0xA;
            effectSpawn(EFFECT_030, arg1->extra.tmd->coords, 0x10100, &ofs);
            ofs.vy = -0x14;
            ofs.vz = -0x50;
            effectSpawn(EFFECT_030, arg1->extra.tmd->coords, 0x10100, &ofs);
            break;
        case 0x2A:
            work->effectArg.coord      = &arg1->extra.tmd->coords[4];
            work->effectArg.spawnArgLo = 0x120;
            work->effectArg.spawnArgHi = 2;
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg1->extra.tmd->coords[4], NULL, &work->effectArg);
            padScriptSpawnDepthScaled(Actor01200_D04044, Actor01200_D04050, (s16)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            work->burstAttackBody.radius = 0x320;
            work->burstWaveBody.radius   = 0xC8;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 0x2B:
            work->burstWaveBody.radius   = 0x190;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 0x2C:
            work->effectArg.coord      = &arg1->extra.tmd->coords[1];
            work->effectArg.spawnArgLo = 0x80;
            work->effectArg.spawnArgHi = 2;
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg1->extra.tmd->coords[1], NULL, &work->effectArg);
            work->burstWaveBody.radius = 0x320;
            break;
        case 0x2E:
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 0x30:
            work->effectArg.coord      = &arg1->extra.tmd->coords[1];
            work->effectArg.spawnArgLo = 0x200;
            work->effectArg.spawnArgHi = 2;
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg1->extra.tmd->coords[1], NULL, &work->effectArg);
            effectSpawn(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, &ofs);
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400C0004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            break;
        case 0x32:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 0x45:
            sceneReleaseBattleRefWithRewards(arg1, 0xC);
            work->state = ACTOR_01200_STATE_HIDDEN;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if (work->stateFrame >= 0x17 && work->stateFrame < 0x29) {
        work->colorMtx.t[0] += (work->stateFrame - 0x16) * 0x60;
    }
    if (work->stateFrame >= 0x2A && work->stateFrame < 0x33) {
        s = 0xBB8 - (work->stateFrame - 0x2A) * 0x258;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(0);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            Actor01200_FaceScale(arg1->extra.tmd->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = (work->stateFrame - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor01200_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if (work->stateFrame < 0x400) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_01200_STATE_HIDDEN;
    }
}

static void Actor01200_Fn01FDC(Enemy* arg0, Task* arg1)
{
    SVECTOR          ofs;
    VECTOR           scale;
    _Actor01200Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->stateEntered != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->hitBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key    = damagePackEnemyAttackKey(arg0, 0);
        work->burstWaveBody.key      = 0x22121;
        work->stateFrame             = 0;
        work->gridBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->savedColorMtx          = work->colorMtx;
        work->driver.requestedSet    = 0xA;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->driver.rate            = 0x2C;
        _animDriverTick(arg1);
        work->burstWaveBody.pos.vx   = arg1->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = arg1->extra.tmd->coords->coord.t[1] - 0x190;
        work->burstWaveBody.pos.vz   = arg1->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        return;
    }
    _animDriverTick(arg1);
    switch (work->stateFrame) {
        case 0xD:
            ofs.vx = 0x1E;
            ofs.vz = 0x1E;
            ofs.vy = -0x3C;
            effectSpawn(EFFECT_030, arg1->extra.tmd->coords, 0x10080, &ofs);
            ofs.vy = -0xA;
            ofs.vz = -0x50;
            effectSpawn(EFFECT_030, arg1->extra.tmd->coords, 0x10030, &ofs);
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x400C0004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 0xE:
            work->burstAttackBody.radius = 0x320;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            padScriptSpawn(Actor01200_D04044, Actor01200_D04050);
            work->effectArg.coord      = &arg1->extra.tmd->coords[4];
            work->effectArg.spawnArgLo = 0x120;
            work->effectArg.spawnArgHi = 2;
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg1->extra.tmd->coords[4], NULL, &work->effectArg);
            break;
        case 0xF:
            work->burstWaveBody.radius   = 0xC8;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 0x10:
            work->burstWaveBody.radius = 0x190;
            work->effectArg.coord      = &arg1->extra.tmd->coords[2];
            work->effectArg.spawnArgLo = 0x100;
            work->effectArg.spawnArgHi = 2;
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg1->extra.tmd->coords[2], NULL, &work->effectArg);
            break;
        case 0x11:
            work->burstWaveBody.radius = 0x320;
            break;
        case 0x13:
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            ofs.vx                     = arg1->extra.tmd->coords->coord.t[0];
            ofs.vy                     = arg1->extra.tmd->coords->coord.t[1];
            ofs.vz                     = arg1->extra.tmd->coords->coord.t[2];
            effectSpawn(EFFECT_RED_GROUND_GLOW, &gGfxViewCoord, 0, &ofs);
            work->effectArg.coord      = &arg1->extra.tmd->coords[1];
            work->effectArg.spawnArgLo = 0x200;
            work->effectArg.spawnArgHi = 2;
            effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg1->extra.tmd->coords[1], NULL, &work->effectArg);
            break;
        case 0x15:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 0x26:
            sceneReleaseBattleRefWithRewards(arg1, 0xC);
            work->state = ACTOR_01200_STATE_HIDDEN;
            break;
    }
    if (work->stateFrame >= 0xD && work->stateFrame < 0x16) {
        s = 0xBB8 - (work->stateFrame - 0xB) * 0x320;
        if (s < 0) {
            s = 0;
        }
        scale.vx = scale.vy = scale.vz = s;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(s);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        s = work->stateFrame * 0xB4 + 0x1000;
        if (s > 0x2000) {
            s = 0x2000;
        }
        Actor01200_FaceScale(arg1->extra.tmd->coords, s);
    }
    if (work->stateFrame < 0x400) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_01200_STATE_HIDDEN;
    }
}

/// Hit spark: picks one of two offsets at random for the quadrant the hit
/// angle `arg1` falls in (front, back, right or left), each with the model
/// coordinate it is relative to, keeps it in `work->hitEffectOffset` and spawns hit id
/// `arg2`'s effect there.
static void Actor01200_Fn026A0(Task* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    _Actor01200Work* work;
    s32              mag;
    GfxCoord*        coord;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    coord                      = &arg0->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x80;
    work->effectArg.spawnArgHi = 2;
    work->effectArg.coord      = coord;
    work->hitEffectOffset      = *sc;
    effectSpawnHit(damageGetPlayerAttackEffectId(arg2), &arg0->extra.tmd->coords[sc->pad], &work->hitEffectOffset, &work->effectArg);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// Hit check: finds the first type-2 record among the five in `hitContacts`, and
/// on a hit applies its damage, turns the model toward it and, once the hit
/// points run out, moves to `ACTOR_01200_STATE_DEATH_BURST`.
/// The first of the five hit records whose kind is 0x20000: copies its point
/// to `pos` and returns its key, or 0 at the first empty record.
static inline s32 Actor01200_FindHit(SVECTOR* pos, WorldCollisionContact* records)
{
    s16 i;

    for (i = 0; i < 5; i++) {
        if (records[i].key.value == 0) {
            break;
        }
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

static void Actor01200_Fn02918(Enemy* arg0, Task* arg1)
{
    ActorHitTakenScratch* sc;
    _Actor01200Work*      work;
    s16                   angle;

    work       = arg1->work;
    sc         = SCRATCH_STACK_RESERVE_BLOCK(ActorHitTakenScratch);
    sc->hitKey = Actor01200_FindHit(&sc->hitPos, work->hitContacts);

    if (sc->hitKey != 0) {
        sc->damage                            = damageComputePlayerAttack(sc->hitKey, 0, 0, 0x1000);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg1->extra.tmd->coords);
        sc->hitOffset.vx = arg1->extra.tmd->coords->workm.t[0];
        sc->hitOffset.vy = arg1->extra.tmd->coords->workm.t[1];
        sc->hitOffset.vz = arg1->extra.tmd->coords->workm.t[2];
        sc->hitOffset.vx = sc->hitPos.vx - arg1->extra.tmd->coords->workm.t[0];
        sc->hitOffset.vy = sc->hitPos.vy - arg1->extra.tmd->coords->workm.t[1];
        sc->hitOffset.vz = sc->hitPos.vz - arg1->extra.tmd->coords->workm.t[2];
        angle            = ratan2(sc->hitOffset.vx, sc->hitOffset.vz) -
                ratan2(-arg1->extra.tmd->coords->workm.m[2][0], arg1->extra.tmd->coords->workm.m[2][2]);
        sc->hitYaw = angle;
        sc->hitYaw = _actorAngleNormalizeYaw(angle);
        Actor01200_Fn026A0(arg1, sc->hitYaw, sc->hitKey);
        worldTargetAddReadoutAmount(&arg0->node, sc->damage, 0);
        arg0->hp -= sc->damage;
        if (arg0->hp <= 0) {
            arg0->spawnState = 0;
            work->state      = ACTOR_01200_STATE_DEATH_BURST;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorHitTakenScratch);
}

/// `ACTOR_01200_STATE_PATROL`: walk between the two `patrolPoints`: turn at most
/// 0x20 toward the current one, step 5 units, and swap points within 400 units
/// or after 0x60 blocked frames; `DEATH_BURST` when `_actorContactApplyAvoidancePushback` reports
/// 1, `CHASE` when the player is within 2000 units and inside a quarter turn or
/// 1000 units, `SETTLE` at random once the walk has looped more than 20 times.
static void Actor01200_Fn02BE8(Enemy* arg0, Task* arg1)
{
    _Actor01200Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    GfxCoord*         coord;
    GfxCoord*         target;
    TmdObject*        obj;
    s16               angle;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 2;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->patrolIndex            = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        work->stateFrame = 0;
        return;
    }
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    turn                                   = head - 1;
    head[-1].delta.vx                      = work->patrolPoints[work->patrolIndex].vx - arg1->extra.tmd->coords->coord.t[0];
    turn->delta.vy                         = 0;
    turn->delta.vz                         = work->patrolPoints[work->patrolIndex].vz - arg1->extra.tmd->coords->coord.t[2];
    coord                                  = arg1->extra.tmd->coords;
    angle                                  = ratan2(head[-1].delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle                            = _actorAngleNormalizeYaw(angle);
    if (turn->angle > 0x20) {
        turn->angle = 0x20;
    }
    if (turn->angle < -0x20) {
        turn->angle = -0x20;
    }
    turn->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, turn->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 5);
    if (_actorContactApplyGridPushback(arg1->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts))) {
        work->stateFrame++;
    }
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 400) || work->stateFrame > 0x60) {
        if (work->patrolIndex == 0) {
            work->patrolIndex = 1;
        } else {
            work->patrolIndex = 0;
        }
        work->stateFrame = 0;
    }
    if (_actorContactApplyAvoidancePushback(arg1->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turn->delta) == 1) {
        work->state = ACTOR_01200_STATE_DEATH_BURST;
    }
    target         = arg1->extra.tmd->coords;
    turn->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    turn->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    turn->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (_actorAngleNormalizeYaw(angle) < 0x400 || !_actorRangeOutsideRadiusXZ(&turn->delta, 1000)) {
            work->state = ACTOR_01200_STATE_CHASE;
        }
    }
    _animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount > 0x14) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->state = ACTOR_01200_STATE_SETTLE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// `ACTOR_01200_STATE_RETURN`: walk back toward `spawnPos`: turn at most 0x10
/// toward it, step 8 units, and hand over to `PATROL` once within 0x50 or after
/// 0xDD frames (`DEATH_BURST` when `_actorContactApplyAvoidancePushback` reports 1).
static void Actor01200_Fn03294(Enemy* arg0, Task* arg1)
{
    _Actor01200Work*  work;
    GfxCoord*         coord;
    GfxCoord*         facing;
    TmdObject*        obj;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 2;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        work->chaseFarFrames = 0;
        work->stateFrame     = 0;
        return;
    }
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    turn                                   = head - 1;
    _animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    head[-1].delta.vx                     = work->spawnPos.vx - arg1->extra.tmd->coords->coord.t[0];
    turn->delta.vy                        = 0;
    turn->delta.vz                        = work->spawnPos.vz - arg1->extra.tmd->coords->coord.t[2];
    coord                                 = arg1->extra.tmd->coords;
    turn->angle                           = _actorAngleNormalizeYaw(ratan2(head[-1].delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (turn->angle > 0x10) {
        turn->angle = 0x10;
    }
    if (turn->angle < -0x10) {
        turn->angle = -0x10;
    }
    facing       = arg1->extra.tmd->coords;
    turn->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, turn->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 8);
    _actorContactApplyGridPushback(arg1->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    work->stateFrame++;
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 0x50) || work->stateFrame >= 0xDD) {
        work->state = ACTOR_01200_STATE_PATROL;
    }
    if (_actorContactApplyAvoidancePushback(arg1->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turn->delta) == 1) {
        work->state = ACTOR_01200_STATE_DEATH_BURST;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static const _Actor01200StateTable Actor01200_D000E4 = {
    {
        Actor01200_Fn03D58,
        Actor01200_Fn03DC0,
        Actor01200_Fn01040,
        Actor01200_Fn03E78,
        Actor01200_Fn01234,
        Actor01200_Fn017DC,
        Actor01200_Fn01FDC,
        Actor01200_Fn02BE8,
        Actor01200_Fn03294,
        Actor01200_Fn03F30,
    }
};

/// Per-frame tick: refreshes the coordinate and color, handles the render
/// mode in `gSceneCombatState.actorControl`, dispatches the handler of
/// `_Actor01200Work::state` and plays its sound.
static void Actor01200_Fn036B0(Enemy* arg0, Task* arg1)
{
    VECTOR                pos;
    _Actor01200StateTable table;
    _Actor01200Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                  = arg1->work;
    table                                 = Actor01200_D000E4;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_01200_STATE_HIDDEN && work->state != ACTOR_01200_STATE_DEATH_BURST && work->state != ACTOR_01200_STATE_SELF_BURST) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_01200_STATE_HIDDEN && work->state != ACTOR_01200_STATE_DEATH_BURST && work->state != ACTOR_01200_STATE_SELF_BURST) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->burstAttackContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->burstAttackContacts);
            return;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    table.handlers[work->state](arg0, arg1);
    if (arg0->hp > 0) {
        Actor01200_Fn02918(arg0, arg1);
        if (arg0->hp <= 0) {
            work->state = ACTOR_01200_STATE_DEATH_BURST;
        }
    }
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->burstAttackContacts);
    id = Actor01200_Fn00990(work);
    if (id != 0) {
        snd = id | ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
    if (work->field_3D8 != 0) {
        worldCoordSetModelLighting(arg1->extra.tmd, arg1->extra.tmd->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// The actor's three task states: spawn, per-frame tick and teardown.
static const EnemyTaskFuncTable3 Actor01200_D0010C = {
    {
        Actor01200_Fn00A6C,
        Actor01200_Fn036B0,
        enemyDestroy,
    }
};

/// Display mode handler for the model (`Task::extra`), selected by `arg2`:
/// 0 hides it and 1 shows it, both reinstating its buffers and moving to
/// `ACTOR_01200_STATE_PATROL`; 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` and 3
/// replaces its flags with `TMD_OBJECT_SKIP_AUTO_BUFFER`, both moving to
/// `ACTOR_01200_STATE_HIDDEN`. `arg1` is unused.
s32 Actor01200_Fn03A00(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    _Actor01200Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_01200_STATE_PATROL;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_01200_STATE_PATROL;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_01200_STATE_HIDDEN;
            break;
        case 3:
            obj->flags  = 0;
            work->state = ACTOR_01200_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

s32 Actor01200_Fn03ABC(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    _Actor01200Work* work;
    Enemy*           ctx;

    work                   = arg0->work;
    ctx                    = arg0->spawnArg2.pointer;
    work->lastCommandStage = request->context.loc.stage;
    work->lastCommandArea  = request->context.loc.area;
    work->lastCommand      = request->command;
    if (request->context.key == 0xB02) {
        switch (request->command) {
            case 0:
                work->state = ACTOR_01200_STATE_HIDDEN;
                break;
            case 1:
            case 2:
                break;
            case 3:
                if (ctx->hp > 0) {
                    work->state                           = ACTOR_01200_STATE_CHASE;
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                break;
            case 4:
                if (ctx->hp > 0) {
                    work->state = ACTOR_01200_STATE_WALK_IN_PLACE;
                }
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// `ACTOR_01200_STATE_HIDDEN`: on entry (`stateEntered` set) it marks the enemy
/// not lockable, hides the model, and turns off the collision bodies the other
/// states enable - the pair pass of `hitBody`, `burstAttackBody` and
/// `burstWaveBody`, and the grid pass of `gridBody`. Nothing happens afterwards.
static void Actor01200_Fn03D58(Enemy* arg0, Task* arg1)
{
    _Actor01200Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->hitBody.flags          = (u16)(work->hitBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->burstAttackBody.flags  = (u16)(work->burstAttackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->burstWaveBody.flags    = (u16)(work->burstWaveBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags         = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

/// `ACTOR_01200_STATE_SETTLE`: on entry (`stateEntered` set) clear the actor and
/// model flags, request animation 4, and set or clear the pass enables of the
/// four collision bodies; afterwards run `_animDriverTick` and move to `IDLE`
/// once rig slot 1 reports `ANIMATION_SLOT_REACHED_BOUNDARY`.
static void Actor01200_Fn03DC0(Enemy* arg0, Task* arg1)
{
    _Actor01200Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 4;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        return;
    }
    _animDriverTick(arg1);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_01200_STATE_IDLE;
    }
}

/// `ACTOR_01200_STATE_ROUSE`: on entry (`stateEntered` set) clear the actor and
/// model flags, request animation 6, and set or clear the pass enables of the
/// four collision bodies; afterwards run `_animDriverTick` and move to `PATROL`
/// once rig slot 1 reports `ANIMATION_SLOT_REACHED_BOUNDARY`.
static void Actor01200_Fn03E78(Enemy* arg0, Task* arg1)
{
    _Actor01200Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 6;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        return;
    }
    _animDriverTick(arg1);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_01200_STATE_PATROL;
    }
}

/// `ACTOR_01200_STATE_WALK_IN_PLACE`: on entry (`stateEntered` set) clear the
/// actor and model flags, request animation 2, and set or clear the pass
/// enables of the four collision bodies; then run `_animDriverTick` and mark the
/// model's root coordinate dirty every frame.
static void Actor01200_Fn03F30(Enemy* arg0, Task* arg1)
{
    _Actor01200Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 2;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    _animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Task entry point: runs the handler for the task's current state from a
/// stack copy of `Actor01200_D0010C`.
void Actor01200_Fn03FD4(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor01200_D0010C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
