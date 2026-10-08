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
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
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

/// Animation requests made by the actor's behavior states.
enum {
    ACTOR_01200_ANIM_WALK   = 2,
    ACTOR_01200_ANIM_RUN    = 3,
    ACTOR_01200_ANIM_SETTLE = 4,
    ACTOR_01200_ANIM_IDLE   = 5,
    ACTOR_01200_ANIM_ROUSE  = 6,
    ACTOR_01200_ANIM_BURST  = 10,
};

/// Character-bank scripts; the tick adds the placement's instance byte.
enum {
    ACTOR_01200_SOUND_STEP   = SOUND_CHARACTER(12, 1),
    ACTOR_01200_SOUND_BURST  = SOUND_CHARACTER(12, 4),
    ACTOR_01200_SOUND_SETTLE = SOUND_CHARACTER(12, 5),
};

/// Shared distances in parent-coordinate units and burst parameters.
enum {
    ACTOR_01200_NOTICE_RADIUS           = 2000,
    ACTOR_01200_BURST_TRIGGER_RADIUS    = 1000,
    ACTOR_01200_BURST_WAVE_KEY          = WORLD_COLLISION_CONTACT_ATTACK | 0x2121,
    ACTOR_01200_BURST_EFFECT_ATTACK_KEY = 0x1001, // Hit-effect lookup uses weapon row 1; the other bits are ignored
    ACTOR_01200_BURST_WAVE_HEIGHT       = 400,
    ACTOR_01200_BURST_WAVE_START_RADIUS = 200,
    ACTOR_01200_BURST_WAVE_MID_RADIUS   = 400,
    ACTOR_01200_BURST_RADIUS            = 800,
    ACTOR_01200_BURST_FRAME_LIMIT       = 1024,
    ACTOR_01200_BURST_MAX_SCALE         = 2 * ONE,
    ACTOR_01200_BURST_EFFECT_REPEAT     = 2,
    ACTOR_01200_BURST_RING_STYLE        = 1,
    ACTOR_01200_BURST_PARTICLE_VARIANT  = 1 << 16, // EFFECT_030 high-half variant
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

static void _actor01200StateHidden(Enemy* enemy, Task* task);
static void _actor01200StateSettle(Enemy* enemy, Task* task);
static void _actor01200StateRouse(Enemy* enemy, Task* task);
static void _actor01200StateWalkInPlace(Enemy* enemy, Task* task);

static TmdSource _gActor01200BoneSucklerBody;
static void      _actor01200Task(Task* task);

static s32 _actor01200SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32 _actor01200ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg);

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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor01200SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor01200ApplyCommand },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor01200_D07078 = { { { TASK_BODY_TMD, 96 } }, _actor01200Task, { .model = &_gActor01200BoneSucklerBody } };

SVECTOR ActorContact_ScratchPosition = { 0 };

static s32  _actor01200PollAnimationSound(_Actor01200Work* work);
static void Actor01200_Fn00A6C(Enemy* arg0, Task* arg1);
static void _actor01200StateIdle(Enemy* enemy, Task* task);
static void _actor01200StateChase(Enemy* enemy, Task* task);
static void _actor01200StateSelfBurst(Enemy* enemy, Task* task);
static void _actor01200StateDeathBurst(Enemy* enemy, Task* task);
static void Actor01200_Fn026A0(Task* arg0, s16 arg1, u32 arg2);
static void Actor01200_Fn02918(Enemy* arg0, Task* arg1);
static void _actor01200StatePatrol(Enemy* enemy, Task* task);
static void _actor01200StateReturn(Enemy* enemy, Task* task);
static void Actor01200_Fn036B0(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// Selects an animation cue's sound request, or zero when no sound is due.
///
/// Walk/run cues latch slot 1's low-ten-bit record index: a held cue reports once,
/// and another index clears the latch. Settle reports on each boundary tick.
/// Requires the task's initialized rig; the caller adds the placement-instance byte.
static s32 _actor01200PollAnimationSound(_Actor01200Work* work)
{
    enum {
        ACTOR_01200_WALK_CUE_FIRST  = 21,
        ACTOR_01200_WALK_CUE_SECOND = 17,
        ACTOR_01200_RUN_CUE_FIRST   = 13,
        ACTOR_01200_RUN_CUE_SECOND  = 18,
    };
    u16 cueIndex;
    s32 cueValue;

    switch (work->driver.requestedSet) {
        case ACTOR_01200_ANIM_WALK:
            cueIndex = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            cueValue = cueIndex;
            if (cueValue != ACTOR_01200_WALK_CUE_FIRST) {
                goto otherWalkCue;
            }
        checkCue:
            if (work->lastSoundCueIndex == cueValue) {
                goto sameCue;
            }
            work->lastSoundCueIndex = cueIndex;
            return ACTOR_01200_SOUND_STEP;
        otherWalkCue:
            if (cueValue == ACTOR_01200_WALK_CUE_SECOND) {
                goto checkCue;
            }
        clearCue:
            work->lastSoundCueIndex = 0;
            break;
        case ACTOR_01200_ANIM_RUN:
            cueIndex = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            cueValue = cueIndex;
            if (cueValue != ACTOR_01200_RUN_CUE_FIRST && cueValue != ACTOR_01200_RUN_CUE_SECOND) {
                goto clearCue;
            }
            goto checkCue;
        sameCue:
            work->lastSoundCueIndex = cueIndex;
            break;
        case ACTOR_01200_ANIM_SETTLE:
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                return ACTOR_01200_SOUND_SETTLE;
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

/// Enables the actor's movement and hit bodies while disabling its burst bodies.
///
/// Requires initialized work. Retains shape bits and unrelated participation flags.
static __inline__ void _actor01200EnableWalkingBodies(_Actor01200Work* work)
{
    work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

/// Disables the actor's hit, burst-attack and burst-wave pair collision passes.
///
/// Requires initialized work. Grid participation and other body flags remain set.
static __inline__ void _actor01200DisablePairBodies(_Actor01200Work* work)
{
    work->hitBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Loops the idle animation until the player approaches or a random rouse succeeds.
///
/// On entry restores targeting and movement collisions and restarts idle playback.
/// After 25 loops, each jump tick draws a one-in-eight rouse chance. Player distance
/// is measured in parent-frame X/Z units and must be strictly below the notice radius.
static void _actor01200StateIdle(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01200_IDLE_MIN_LOOPS   = 25,
        ACTOR_01200_IDLE_RANDOM_MASK = 7,
    };
    _Actor01200Work* work;
    GfxCoord*        rootCoord;
    SVECTOR          playerOffset;
    SVECTOR*         playerDelta;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_IDLE;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        _actor01200EnableWalkingBodies(work);
        _animDriverTick(task);
        return;
    }
    _animDriverTick(task);
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount >= ACTOR_01200_IDLE_MIN_LOOPS) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & ACTOR_01200_IDLE_RANDOM_MASK)) {
            work->state = ACTOR_01200_STATE_ROUSE;
        }
    }
    rootCoord       = task->extra.tmd->coords;
    playerDelta     = &playerOffset;
    playerOffset.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerDelta->vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    playerDelta->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(playerDelta, ACTOR_01200_NOTICE_RADIUS)) {
        sceneEngageBattle(1);
        work->state = ACTOR_01200_STATE_ROUSE;
    }
}

/// Runs toward the player, bursting nearby or returning after sustained separation.
///
/// Requires the live enemy task, its initialized work and model, and a player root
/// in the same parent frame. Turns at most 16 of 4096 angle units per tick and moves
/// 20 coordinate units. Near/far counters use the offset sampled before movement;
/// 21 near ticks select self-burst, 241 far ticks select return. Later state writes
/// retain priority over earlier ones, including a blocking contact's death burst.
static void _actor01200StateChase(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01200_CHASE_YAW_STEP      = 16,
        ACTOR_01200_CHASE_STEP_DISTANCE = 20,
        ACTOR_01200_CHASE_NEAR_TICKS    = 21,
        ACTOR_01200_CHASE_FAR_TICKS     = 241,
    };
    _Actor01200Work*  work;
    GfxCoord*         rootCoord;
    GfxCoord*         facingCoord;
    GfxCoord*         yawCoord;
    TmdObject*        model;
    ActorTurnScratch* turn;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_RUN;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = ANIMATION_RATE_ONE;
        _actor01200EnableWalkingBodies(work);
        _animDriverTick(task);
        work->chaseFarFrames = 0;
        sceneEngageBattle(1);
        return;
    }
    // Sample the player offset before steering and collision correction.
    turn = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    _animDriverTick(task);
    rootCoord      = task->extra.tmd->coords;
    turn->delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    turn->delta.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    turn->delta.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    facingCoord    = task->extra.tmd->coords;
    turn->angle    = _actorAngleNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]));
    if (turn->angle > ACTOR_01200_CHASE_YAW_STEP) {
        turn->angle = ACTOR_01200_CHASE_YAW_STEP;
    }
    if (turn->angle < -ACTOR_01200_CHASE_YAW_STEP) {
        turn->angle = -ACTOR_01200_CHASE_YAW_STEP;
    }
    yawCoord     = task->extra.tmd->coords;
    turn->angle += ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turn->angle, GRAPHICS_ROTATION_REPLACE);
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, ACTOR_01200_CHASE_STEP_DISTANCE);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (_actorRangeOutsideRadiusXZ(&turn->delta, ACTOR_01200_BURST_TRIGGER_RADIUS)) {
        work->chaseFarFrames++;
    } else {
        work->chaseFarFrames = 0;
    }
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, ACTOR_01200_BURST_TRIGGER_RADIUS)) {
        work->chaseNearFrames++;
    } else {
        work->chaseNearFrames = 0;
    }
    if (work->chaseNearFrames >= ACTOR_01200_CHASE_NEAR_TICKS) {
        work->state = ACTOR_01200_STATE_SELF_BURST;
    }
    if (_actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turn->delta) == 1) {
        work->state = ACTOR_01200_STATE_DEATH_BURST;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Retain the spawn-range probe: it writes scratch storage despite its unused result.
    turn->delta.vx = work->spawnPos.vx - task->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = work->spawnPos.vz - task->extra.tmd->coords->coord.t[2];
    _actorRangeOutsideRadiusXZ(&turn->delta, 3000);
    if (work->chaseFarFrames >= ACTOR_01200_CHASE_FAR_TICKS) {
        work->state = ACTOR_01200_STATE_RETURN;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Plays the voluntary burst, enabling its attack briefly before hiding the actor.
///
/// Requires initialized work, animation slots and model coordinates 0..5. Entry
/// saves room lighting and snapshots the burst bodies in the root's parent frame.
/// Subsequent calls count ticks independently of animation speed. The attack lasts
/// one tick; the expanding wave lasts four. Rewards release the battle hold at tick
/// 69. Color and root scale use signed Q12 factors; the task survives hidden.
static void _actor01200StateSelfBurst(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01200_SELF_BURST_TRANS_TICK      = 41,
        ACTOR_01200_SELF_BURST_ATTACK_TICK     = 42,
        ACTOR_01200_SELF_BURST_ATTACK_END_TICK = 43,
        ACTOR_01200_SELF_BURST_WAVE_GROW_TICK  = 44,
        ACTOR_01200_SELF_BURST_WAVE_END_TICK   = 46,
        ACTOR_01200_SELF_BURST_GLOW_TICK       = 48,
        ACTOR_01200_SELF_BURST_HIDE_TICK       = 50,
        ACTOR_01200_SELF_BURST_REWARD_TICK     = 69,
        ACTOR_01200_SELF_BURST_RED_TICK        = 23,
        ACTOR_01200_SELF_BURST_FADE_END_TICK   = 51,
    };
    SVECTOR          effectOffset;
    VECTOR           colorScaleVector;
    _Actor01200Work* work;
    TmdObject*       model;
    s16              scaleFactor; // Q12 color intensity, then root scale
    s32              audioPan;
    s32              soundId;

    work  = task->work;
    model = task->extra.tmd;
    memset(&effectOffset, 0, sizeof(effectOffset));
    if (work->stateEntered != 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        _actor01200DisablePairBodies(work);
        work->burstAttackBody.key = damagePackEnemyAttackKey(enemy, 0);
        work->burstWaveBody.key   = ACTOR_01200_BURST_WAVE_KEY;
        work->stateFrame          = 0;
        work->gridBody.flags     |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx       = work->colorMtx;
        work->driver.requestedSet = ACTOR_01200_ANIM_BURST;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias     = ANIMATION_RATE_ONE / 2;
        _animDriverTick(task);
        work->burstWaveBody.pos.vx   = task->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = task->extra.tmd->coords->coord.t[1] - ACTOR_01200_BURST_WAVE_HEIGHT;
        work->burstWaveBody.pos.vz   = task->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = task->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = task->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = task->extra.tmd->coords->coord.t[2];
        return;
    }
    _animDriverTick(task);
    // Collision windows and effects are timed by state ticks, not pose cues.
    switch (work->stateFrame) {
        case ACTOR_01200_SELF_BURST_TRANS_TICK:
            task->extra.tmd->flags |= TMD_OBJECT_SEMI_TRANS;
            effectOffset.vx         = 0x1E;
            effectOffset.vz         = 0x1E;
            effectOffset.vy         = -0xA;
            effectSpawn(EFFECT_030, task->extra.tmd->coords, ACTOR_01200_BURST_PARTICLE_VARIANT | 256, &effectOffset);
            effectOffset.vy = -0x14;
            effectOffset.vz = -0x50;
            effectSpawn(EFFECT_030, task->extra.tmd->coords, ACTOR_01200_BURST_PARTICLE_VARIANT | 256, &effectOffset);
            break;
        case ACTOR_01200_SELF_BURST_ATTACK_TICK:
            work->effectArg.coord      = &task->extra.tmd->coords[4];
            work->effectArg.spawnArgLo = 0x120;
            work->effectArg.spawnArgHi = ACTOR_01200_BURST_EFFECT_REPEAT;
            effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01200_BURST_EFFECT_ATTACK_KEY), &task->extra.tmd->coords[4], NULL, &work->effectArg);
            padScriptSpawnDepthScaled(Actor01200_D04044, Actor01200_D04050, (s16)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            work->burstAttackBody.radius = ACTOR_01200_BURST_RADIUS;
            work->burstWaveBody.radius   = ACTOR_01200_BURST_WAVE_START_RADIUS;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[2], ACTOR_01200_BURST_RING_STYLE, NULL);
            break;
        case ACTOR_01200_SELF_BURST_ATTACK_END_TICK:
            work->burstWaveBody.radius   = ACTOR_01200_BURST_WAVE_MID_RADIUS;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_01200_SELF_BURST_WAVE_GROW_TICK:
            work->effectArg.coord      = &task->extra.tmd->coords[1];
            work->effectArg.spawnArgLo = 0x80;
            work->effectArg.spawnArgHi = ACTOR_01200_BURST_EFFECT_REPEAT;
            effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01200_BURST_EFFECT_ATTACK_KEY), &task->extra.tmd->coords[1], NULL, &work->effectArg);
            work->burstWaveBody.radius = ACTOR_01200_BURST_RADIUS;
            break;
        case ACTOR_01200_SELF_BURST_WAVE_END_TICK:
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_01200_SELF_BURST_GLOW_TICK:
            work->effectArg.coord      = &task->extra.tmd->coords[1];
            work->effectArg.spawnArgLo = 0x200;
            work->effectArg.spawnArgHi = ACTOR_01200_BURST_EFFECT_REPEAT;
            effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01200_BURST_EFFECT_ATTACK_KEY), &task->extra.tmd->coords[1], NULL, &work->effectArg);
            effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, &effectOffset);
            soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01200_SOUND_BURST;
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        case ACTOR_01200_SELF_BURST_HIDE_TICK:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_01200_SELF_BURST_REWARD_TICK:
            sceneReleaseBattleRefWithRewards(task, 12);
            work->state = ACTOR_01200_STATE_HIDDEN;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    // Restore the saved room color before applying this tick's red tint and fade.
    if (work->stateFrame >= ACTOR_01200_SELF_BURST_RED_TICK && work->stateFrame < ACTOR_01200_SELF_BURST_TRANS_TICK) {
        work->colorMtx.t[0] += (work->stateFrame - 0x16) * 0x60;
    }
    if (work->stateFrame >= ACTOR_01200_SELF_BURST_ATTACK_TICK && work->stateFrame < ACTOR_01200_SELF_BURST_FADE_END_TICK) {
        scaleFactor = 0xBB8 - (work->stateFrame - ACTOR_01200_SELF_BURST_ATTACK_TICK) * 0x258;
        if (scaleFactor < 0x4B0) {
            colorScaleVector.vx = colorScaleVector.vy = colorScaleVector.vz = 0;
            ScaleMatrix(&work->colorMtx, &colorScaleVector);
            gte_lddp(0);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            _actorRenderRescaleYaw(task->extra.tmd->coords, ONE);
        } else {
            colorScaleVector.vx = colorScaleVector.vy = colorScaleVector.vz = scaleFactor;
            work->colorMtx                                                  = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &colorScaleVector);
            gte_lddp(scaleFactor);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            scaleFactor = (work->stateFrame - 0x28) * 0x400 + ONE;
            if (scaleFactor > ACTOR_01200_BURST_MAX_SCALE) {
                scaleFactor = ACTOR_01200_BURST_MAX_SCALE;
            }
            _actorRenderRescaleYaw(task->extra.tmd->coords, scaleFactor);
        }
    }
    if (work->stateFrame < ACTOR_01200_BURST_FRAME_LIMIT) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_01200_STATE_HIDDEN;
    }
}

/// Plays the accelerated death burst and releases its battle hold with rewards.
///
/// Requires initialized work, animation slots and model coordinates 0..5. Entry
/// disables grid and hit participation and fixes playback at 44 sixteenths of a
/// frame per tick. The attack lasts one tick, the wave four; the model fades and
/// swells before hiding at tick 21, and rewards are credited at tick 38.
/// The task survives in the hidden state.
static void _actor01200StateDeathBurst(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01200_DEATH_BURST_TRANS_TICK      = 13,
        ACTOR_01200_DEATH_BURST_ATTACK_TICK     = 14,
        ACTOR_01200_DEATH_BURST_ATTACK_END_TICK = 15,
        ACTOR_01200_DEATH_BURST_WAVE_GROW_TICK  = 16,
        ACTOR_01200_DEATH_BURST_WAVE_MAX_TICK   = 17,
        ACTOR_01200_DEATH_BURST_WAVE_END_TICK   = 19,
        ACTOR_01200_DEATH_BURST_HIDE_TICK       = 21,
        ACTOR_01200_DEATH_BURST_REWARD_TICK     = 38,
        ACTOR_01200_DEATH_BURST_FADE_END_TICK   = 22,
        ACTOR_01200_DEATH_BURST_ANIM_RATE       = 44,
    };
    SVECTOR          effectOffset;
    VECTOR           colorScaleVector;
    _Actor01200Work* work;
    TmdObject*       model;
    s16              scaleFactor; // Q12 color intensity, then root scale
    s32              audioPan;
    s32              soundId;

    work  = task->work;
    model = task->extra.tmd;
    if (work->stateEntered != 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        _actor01200DisablePairBodies(work);
        work->burstAttackBody.key = damagePackEnemyAttackKey(enemy, 0);
        work->burstWaveBody.key   = ACTOR_01200_BURST_WAVE_KEY;
        work->stateFrame          = 0;
        work->gridBody.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->savedColorMtx       = work->colorMtx;
        work->driver.requestedSet = ACTOR_01200_ANIM_BURST;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias     = 0;
        work->driver.rate         = ACTOR_01200_DEATH_BURST_ANIM_RATE;
        _animDriverTick(task);
        work->burstWaveBody.pos.vx   = task->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = task->extra.tmd->coords->coord.t[1] - ACTOR_01200_BURST_WAVE_HEIGHT;
        work->burstWaveBody.pos.vz   = task->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = task->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = task->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = task->extra.tmd->coords->coord.t[2];
        return;
    }
    _animDriverTick(task);
    // Collision windows and effects are timed by state ticks, not pose cues.
    switch (work->stateFrame) {
        case ACTOR_01200_DEATH_BURST_TRANS_TICK:
            effectOffset.vx = 0x1E;
            effectOffset.vz = 0x1E;
            effectOffset.vy = -0x3C;
            effectSpawn(EFFECT_030, task->extra.tmd->coords, ACTOR_01200_BURST_PARTICLE_VARIANT | 128, &effectOffset);
            effectOffset.vy = -0xA;
            effectOffset.vz = -0x50;
            effectSpawn(EFFECT_030, task->extra.tmd->coords, ACTOR_01200_BURST_PARTICLE_VARIANT | 48, &effectOffset);
            soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_01200_SOUND_BURST;
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case ACTOR_01200_DEATH_BURST_ATTACK_TICK:
            work->burstAttackBody.radius = ACTOR_01200_BURST_RADIUS;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[2], ACTOR_01200_BURST_RING_STYLE, NULL);
            padScriptSpawn(Actor01200_D04044, Actor01200_D04050);
            work->effectArg.coord      = &task->extra.tmd->coords[4];
            work->effectArg.spawnArgLo = 0x120;
            work->effectArg.spawnArgHi = ACTOR_01200_BURST_EFFECT_REPEAT;
            effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01200_BURST_EFFECT_ATTACK_KEY), &task->extra.tmd->coords[4], NULL, &work->effectArg);
            break;
        case ACTOR_01200_DEATH_BURST_ATTACK_END_TICK:
            work->burstWaveBody.radius   = ACTOR_01200_BURST_WAVE_START_RADIUS;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case ACTOR_01200_DEATH_BURST_WAVE_GROW_TICK:
            work->burstWaveBody.radius = ACTOR_01200_BURST_WAVE_MID_RADIUS;
            work->effectArg.coord      = &task->extra.tmd->coords[2];
            work->effectArg.spawnArgLo = 0x100;
            work->effectArg.spawnArgHi = ACTOR_01200_BURST_EFFECT_REPEAT;
            effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01200_BURST_EFFECT_ATTACK_KEY), &task->extra.tmd->coords[2], NULL, &work->effectArg);
            break;
        case ACTOR_01200_DEATH_BURST_WAVE_MAX_TICK:
            work->burstWaveBody.radius = ACTOR_01200_BURST_RADIUS;
            break;
        case ACTOR_01200_DEATH_BURST_WAVE_END_TICK:
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            effectOffset.vx            = task->extra.tmd->coords->coord.t[0];
            effectOffset.vy            = task->extra.tmd->coords->coord.t[1];
            effectOffset.vz            = task->extra.tmd->coords->coord.t[2];
            effectSpawn(EFFECT_RED_GROUND_GLOW, &gGfxViewCoord, 0, &effectOffset);
            work->effectArg.coord      = &task->extra.tmd->coords[1];
            work->effectArg.spawnArgLo = 0x200;
            work->effectArg.spawnArgHi = ACTOR_01200_BURST_EFFECT_REPEAT;
            effectSpawnHit(damageGetPlayerAttackEffectId(ACTOR_01200_BURST_EFFECT_ATTACK_KEY), &task->extra.tmd->coords[1], NULL, &work->effectArg);
            break;
        case ACTOR_01200_DEATH_BURST_HIDE_TICK:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_01200_DEATH_BURST_REWARD_TICK:
            sceneReleaseBattleRefWithRewards(task, 12);
            work->state = ACTOR_01200_STATE_HIDDEN;
            break;
    }
    // Fade light coefficients and offsets together while swelling the yaw basis.
    if (work->stateFrame >= ACTOR_01200_DEATH_BURST_TRANS_TICK && work->stateFrame < ACTOR_01200_DEATH_BURST_FADE_END_TICK) {
        scaleFactor = 0xBB8 - (work->stateFrame - 0xB) * 0x320;
        if (scaleFactor < 0) {
            scaleFactor = 0;
        }
        colorScaleVector.vx = colorScaleVector.vy = colorScaleVector.vz = scaleFactor;
        work->colorMtx                                                  = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &colorScaleVector);
        gte_lddp(scaleFactor);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        scaleFactor = work->stateFrame * 0xB4 + ONE;
        if (scaleFactor > ACTOR_01200_BURST_MAX_SCALE) {
            scaleFactor = ACTOR_01200_BURST_MAX_SCALE;
        }
        _actorRenderRescaleYaw(task->extra.tmd->coords, scaleFactor);
    }
    if (work->stateFrame < ACTOR_01200_BURST_FRAME_LIMIT) {
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

/// Copies the first attack contact's world position and returns its packed key.
///
/// Reads at most `contactCount` entries, stopping sooner at a zero key. Contacts
/// are borrowed and unchanged; `hitPosition` must be writable and separate from
/// them. A miss returns zero and leaves the position intact. XYZ are copied as
/// signed halfword coordinate units; the vector's fourth halfword is untouched.
static inline s32 _actor01200FindAttackContact(SVECTOR* hitPosition, const WorldCollisionContact* contacts, s32 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPosition->vx = contacts[contactIndex].point.vx;
            hitPosition->vy = contacts[contactIndex].point.vy;
            hitPosition->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
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
    sc->hitKey = _actor01200FindAttackContact(&sc->hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));

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

/// Walks between the two patrol points, watching for the player and an idle break.
///
/// Requires initialized work and a live enemy/model task with a player root in the
/// same parent frame. Turns at most 32 of 4096 angle units and steps 5 coordinate
/// units per tick. A target changes below 400 X/Z units or after 97 grid-blocked
/// ticks. Player awareness uses a signed yaw test below 1024, without an absolute
/// value, or proximity below 1000; both require distance below 2000. Later random
/// settling after more than 20 loops can supersede an earlier state choice.
static void _actor01200StatePatrol(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01200_PATROL_YAW_STEP         = 32,
        ACTOR_01200_PATROL_BLOCKED_LIMIT    = 96,
        ACTOR_01200_PATROL_NOTICE_YAW_LIMIT = 1024,
        ACTOR_01200_PATROL_MIN_LOOPS        = 20,
        ACTOR_01200_PATROL_RANDOM_MASK      = 7,
    };
    _Actor01200Work*  work;
    ActorTurnScratch* turn;
    GfxCoord*         rootCoord;
    GfxCoord*         playerReferenceCoord;
    TmdObject*        model;
    s16               yawDelta;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_WALK;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->patrolIndex             = 0;
        _actor01200EnableWalkingBodies(work);
        _animDriverTick(task);
        work->stateFrame = 0;
        return;
    }
    turn           = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn->delta.vx = work->patrolPoints[work->patrolIndex].vx - task->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = work->patrolPoints[work->patrolIndex].vz - task->extra.tmd->coords->coord.t[2];
    rootCoord      = task->extra.tmd->coords;
    yawDelta       = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    turn->angle    = _actorAngleNormalizeYaw(yawDelta);
    if (turn->angle > ACTOR_01200_PATROL_YAW_STEP) {
        turn->angle = ACTOR_01200_PATROL_YAW_STEP;
    }
    if (turn->angle < -ACTOR_01200_PATROL_YAW_STEP) {
        turn->angle = -ACTOR_01200_PATROL_YAW_STEP;
    }
    turn->angle += ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turn->angle, GRAPHICS_ROTATION_REPLACE);
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 5);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts))) {
        work->stateFrame++;
    }
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, 400) || work->stateFrame > ACTOR_01200_PATROL_BLOCKED_LIMIT) {
        if (work->patrolIndex == 0) {
            work->patrolIndex = 1;
        } else {
            work->patrolIndex = 0;
        }
        work->stateFrame = 0;
    }
    if (_actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turn->delta) == 1) {
        work->state = ACTOR_01200_STATE_DEATH_BURST;
    }
    playerReferenceCoord = task->extra.tmd->coords;
    turn->delta.vx       = gPlayerStatus.coordMtx->t[0] - playerReferenceCoord->coord.t[0];
    turn->delta.vy       = gPlayerStatus.coordMtx->t[1] - playerReferenceCoord->coord.t[1];
    turn->delta.vz       = gPlayerStatus.coordMtx->t[2] - playerReferenceCoord->coord.t[2];
    // The signed bearing test deliberately accepts the whole negative half-turn.
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, ACTOR_01200_NOTICE_RADIUS)) {
        rootCoord = task->extra.tmd->coords;
        yawDelta  = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
        if (_actorAngleNormalizeYaw(yawDelta) < ACTOR_01200_PATROL_NOTICE_YAW_LIMIT || !_actorRangeOutsideRadiusXZ(&turn->delta, ACTOR_01200_BURST_TRIGGER_RADIUS)) {
            work->state = ACTOR_01200_STATE_CHASE;
        }
    }
    _animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount > ACTOR_01200_PATROL_MIN_LOOPS) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & ACTOR_01200_PATROL_RANDOM_MASK)) {
            work->state = ACTOR_01200_STATE_SETTLE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Walks back to the spawn point before resuming patrol.
///
/// Requires the live enemy task and initialized work/model. Turns at most 16 of
/// 4096 angle units and steps 8 coordinate units per tick. Patrol resumes below
/// 80 X/Z units, using the offset sampled before movement, or at tick 221.
/// A blocking body contact takes priority and selects the death burst.
static void _actor01200StateReturn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_01200_RETURN_YAW_STEP       = 16,
        ACTOR_01200_RETURN_ARRIVAL_RADIUS = 80,
        ACTOR_01200_RETURN_TICK_LIMIT     = 221,
    };
    _Actor01200Work*  work;
    GfxCoord*         rootCoord;
    GfxCoord*         facingCoord;
    TmdObject*        model;
    ActorTurnScratch* turn;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_WALK;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        _actor01200EnableWalkingBodies(work);
        _animDriverTick(task);
        work->chaseFarFrames = 0;
        work->stateFrame     = 0;
        return;
    }
    turn = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    _animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    turn->delta.vx                        = work->spawnPos.vx - task->extra.tmd->coords->coord.t[0];
    turn->delta.vy                        = 0;
    turn->delta.vz                        = work->spawnPos.vz - task->extra.tmd->coords->coord.t[2];
    rootCoord                             = task->extra.tmd->coords;
    turn->angle                           = _actorAngleNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    if (turn->angle > ACTOR_01200_RETURN_YAW_STEP) {
        turn->angle = ACTOR_01200_RETURN_YAW_STEP;
    }
    if (turn->angle < -ACTOR_01200_RETURN_YAW_STEP) {
        turn->angle = -ACTOR_01200_RETURN_YAW_STEP;
    }
    facingCoord  = task->extra.tmd->coords;
    turn->angle += ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turn->angle, GRAPHICS_ROTATION_REPLACE);
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 8);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    work->stateFrame++;
    if (!_actorRangeOutsideRadiusXZ(&turn->delta, ACTOR_01200_RETURN_ARRIVAL_RADIUS) || work->stateFrame >= ACTOR_01200_RETURN_TICK_LIMIT) {
        work->state = ACTOR_01200_STATE_PATROL;
    }
    if (_actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turn->delta) == 1) {
        work->state = ACTOR_01200_STATE_DEATH_BURST;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

static const _Actor01200StateTable Actor01200_D000E4 = {
    {
        _actor01200StateHidden,
        _actor01200StateSettle,
        _actor01200StateIdle,
        _actor01200StateRouse,
        _actor01200StateChase,
        _actor01200StateSelfBurst,
        _actor01200StateDeathBurst,
        _actor01200StatePatrol,
        _actor01200StateReturn,
        _actor01200StateWalkInPlace,
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
    id = _actor01200PollAnimationSound(work);
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

/// Applies the actor's draw-mode message and selects patrol or hidden behavior.
///
/// Requires a live TMD task with initialized work. Modes 0/1 hide/show and allocate
/// primitive buffers, selecting patrol. Mode 2 retains flags and disables automatic
/// buffers; mode 3 clears other flags and disables automatic buffers. Both select
/// hidden behavior. Other modes do nothing. The message ID and second payload are
/// ignored; returns zero for every request.
static s32 _actor01200SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum {
        ACTOR_01200_DRAW_SKIP_BUFFER       = 2,
        ACTOR_01200_DRAW_RESET_SKIP_BUFFER = 3,
    };
    TmdObject*       model;
    _Actor01200Work* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_01200_STATE_PATROL;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_01200_STATE_PATROL;
            break;
        case ACTOR_01200_DRAW_SKIP_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_01200_STATE_HIDDEN;
            break;
        case ACTOR_01200_DRAW_RESET_SKIP_BUFFER:
            model->flags  = 0;
            work->state   = ACTOR_01200_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Applies a motel-room-1 actor command and records its context and low selector byte.
///
/// Borrows a complete command through synchronous dispatch. Requires initialized
/// work, a model and the live enemy in `spawnArg2.pointer`. Only the Dryfield daytime
/// motel-room-1 namespace changes behavior: 0 hides, 3 chases and 4 walks in place;
/// the latter two require positive HP. All other selectors are ignored. Context
/// bytes and the truncated selector are recorded for every namespace. The message
/// ID and second payload are ignored; returns zero even for ignored commands.
static s32 _actor01200ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedArg)
{
    enum {
        ACTOR_01200_COMMAND_HIDE          = 0,
        ACTOR_01200_COMMAND_UNUSED_1      = 1,
        ACTOR_01200_COMMAND_UNUSED_2      = 2,
        ACTOR_01200_COMMAND_CHASE         = 3,
        ACTOR_01200_COMMAND_WALK_IN_PLACE = 4,
        ACTOR_01200_COMMAND_CONTEXT       = GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_MOTEL_ROOM_1 << 8),
    };
    _Actor01200Work* work;
    Enemy*           enemy;

    work                   = task->work;
    enemy                  = task->spawnArg2.pointer;
    work->lastCommandStage = request->context.loc.stage;
    work->lastCommandArea  = request->context.loc.area;
    work->lastCommand      = request->command;
    if (request->context.key == ACTOR_01200_COMMAND_CONTEXT) {
        switch (request->command) {
            case ACTOR_01200_COMMAND_HIDE:
                work->state = ACTOR_01200_STATE_HIDDEN;
                break;
            case ACTOR_01200_COMMAND_UNUSED_1:
            case ACTOR_01200_COMMAND_UNUSED_2:
                break;
            case ACTOR_01200_COMMAND_CHASE:
                if (enemy->hp > 0) {
                    work->state                           = ACTOR_01200_STATE_CHASE;
                    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                break;
            case ACTOR_01200_COMMAND_WALK_IN_PLACE:
                if (enemy->hp > 0) {
                    work->state = ACTOR_01200_STATE_WALK_IN_PLACE;
                }
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Hides the model and disables targeting and all four collision passes on entry.
///
/// Requires a live enemy/model task and initialized work. Other model flags remain
/// set. Subsequent ticks leave the hidden task in place without advancing animation.
static void _actor01200StateHidden(Enemy* enemy, Task* task)
{
    _Actor01200Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        _actor01200DisablePairBodies(work);
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

/// Plays the settling animation and enters idle when slot 1 reaches a boundary.
///
/// Entry restores targeting and walking collision passes and restarts playback.
/// Requires the live enemy/model task and its initialized animation work.
static void _actor01200StateSettle(Enemy* enemy, Task* task)
{
    _Actor01200Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_SETTLE;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        _actor01200EnableWalkingBodies(work);
        _animDriverTick(task);
        return;
    }
    _animDriverTick(task);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_01200_STATE_IDLE;
    }
}

/// Plays the rousing animation and enters patrol when slot 1 reaches a boundary.
///
/// Entry restores targeting and walking collision passes and restarts playback.
/// Requires the live enemy/model task and its initialized animation work.
static void _actor01200StateRouse(Enemy* enemy, Task* task)
{
    _Actor01200Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_ROUSE;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        _actor01200EnableWalkingBodies(work);
        _animDriverTick(task);
        return;
    }
    _animDriverTick(task);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_01200_STATE_PATROL;
    }
}

/// Advances the walking animation without translating the actor itself.
///
/// Entry restores targeting and walking collision passes and restarts playback.
/// Requires the live enemy/model task and its initialized work. The root cache is
/// marked dirty after every animation tick.
static void _actor01200StateWalkInPlace(Enemy* enemy, Task* task)
{
    _Actor01200Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_01200_ANIM_WALK;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        _actor01200EnableWalkingBodies(work);
    }
    _animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Dispatches the enemy task's spawn, frame-update or teardown state.
///
/// `task->state` must index the three-entry task table (0 spawn, 1 update, 2 teardown).
/// The live task borrows its enemy through `spawnArg2.pointer`; each callback receives
/// that enemy and task. The table is copied by value before dispatch.
static void _actor01200Task(Task* task)
{
    EnemyTaskFuncTable3 taskStates;

    taskStates = Actor01200_D0010C;
    taskStates.funcs[task->state](task->spawnArg2.pointer, task);
}
