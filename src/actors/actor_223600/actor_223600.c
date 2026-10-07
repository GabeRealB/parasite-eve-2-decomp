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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
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

#include "overlay.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"

/// Values of `_Actor223600Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Animation numbers are indices into the package's animation-set table, and a
/// placement is the enemy's placement index, the part of `Enemy::placeKey`
/// above `ENEMY_PLACE_INDEX_SHIFT`. No state ends of its own accord: the
/// model-draw message selects `WALK` or `HIDDEN`, and the
/// `ACTOR_223600_COMMAND_*` actor commands select `DROP_IN` or `HIDDEN`.
enum {
    ACTOR_223600_STATE_HIDDEN  = 0, // not drawn, not lockable
    ACTOR_223600_STATE_WALK    = 1, // from a start point fixed per placement (0 and 1; others stay where they are), walks on animation 2 toward `walkTarget`, turning at most 0x10 a tick
    ACTOR_223600_STATE_DROP_IN = 2  // from a point above the floor fixed per placement, falls to the floor on animation 14 and plays the landing on animation 15
};

/// `ActorCommand::context.key` of the commands this enemy acts on.
///
/// Its room, Dryfield's general store, broadcasts its commands to every actor
/// under its own stage and area. The enemy records a command of any namespace
/// and acts on this one alone.
enum {
    ACTOR_223600_COMMAND_CONTEXT = GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_GENERAL_STORE << 8)
};

/// `ActorCommand::command` selectors of that namespace, as this enemy answers them.
enum {
    ACTOR_223600_COMMAND_IGNORED             = 0, // a command of the room's that this enemy accepts and does nothing with
    ACTOR_223600_COMMAND_DROP_IN             = 1, // selects `ACTOR_223600_STATE_DROP_IN`
    ACTOR_223600_COMMAND_HIDE                = 2, // follows `DROP_IN` 0x5A frames later; selects `ACTOR_223600_STATE_HIDDEN`
    ACTOR_223600_COMMAND_HIDE_UNTIL_CUTSCENE = 9  // sent when the room starts with its cutscene still to play; selects `ACTOR_223600_STATE_HIDDEN`
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the state machine, the animation rig and its driver's state, the
/// point the walk heads for, and storage for the model's matrices.
///
/// Everything up to and including `driver` is laid out as `AnimDriverWork`,
/// through which the shared animation driver reaches the block: its
/// `carrierState` bytes are the five words and the gap ahead of `rig` here. A
/// cue index is the low ten bits of the record a slot's current pose names
/// (`ANIMATION_POSE_CUE_INDEX_MASK`). World Y grows downward, so a fall adds
/// to it.
typedef struct {
    s16           state;        // `ACTOR_223600_STATE_*`
    s16           prevState;    // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16           stateEntered; // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16           stateFrame;   // tick counter of the running state: `WALK` counts up from 0 and never reads it; `DROP_IN` starts it at a value fixed per placement, times the drop by it and restarts it at 0 for the landing
    s16           field_8;      // cleared at spawn and never accessed again; role unproven
    byte          pad_A[2];     // never accessed
    ActorAnimRig6 rig;          // playback of the model's parts; slot 1's status and cue index time the sound cues
    struct {
        s16 state;              // `ANIM_DRIVER_STATE_*` (0 idle, 1 or 2 restart requested, 3 playing)
        s16 playingSet;         // animation the slots were last restarted on
        s16 requestedSet;       // animation the next restart plays; also what the sound cues and the phases of `DROP_IN` are keyed on
        s16 rate;               // playback rate in sixteenths of a frame per tick; from spawn 16 plus the placement index (odd placements) or minus half of it (even), until `DROP_IN` sets its own
        s16 rateBias;           // added to `rate`; always 0 here
        s16 tickCount;          // advancing ticks since the last restart
        s16 jumpCount;          // of those, ticks on which slot 1 followed a control jump: loops of a looping animation
    } driver;                   // the animation driver's state, member for member `AnimDriverWork`'s
    s16     field_17E;          // cleared at spawn and never accessed again; role unproven
    u8      lastCommandStage;   // stage tag of the last actor command received, whatever its namespace; never read
    u8      lastCommandArea;    // area tag of that command; never read
    u8      lastCommand;        // low byte of that command's selector; never read
    byte    pad_183[1];         // never accessed
    u16     field_184;          // 5 at spawn, offset by the placement index the way `driver.rate` is; never accessed again; role unproven
    u16     field_186;          // 20 at spawn, offset the same way; never accessed again; role unproven
    byte    pad_188[0xC];       // never accessed
    SVECTOR spawnPos;           // root position at spawn; never read
    SVECTOR walkTarget;         // point `WALK` heads for, set on entering it; only X and Z are used
    byte    pad_1A4[4];         // never accessed
    MATRIX  lightMtx;           // storage for the model's `TmdObject::lightMtx`
    MATRIX  colorMtx;           // storage for the model's `TmdObject::colorMtx`
    byte    pad_1E8[0x20];      // never accessed
    u16     lastSoundCueIndex;  // slot 1's cue index when the cue sound of animation 2 or 3 last fired, so a held cue sounds once; 0 on any other cue
    byte    pad_20A[2];         // never accessed
    s8      relightPending;     // 1 when the last tick left the root coordinate awaiting composition (it moved, or the view changed); the next tick then rebuilds `colorMtx` from the room lights (0 otherwise)
    byte    pad_20D[5];         // never accessed
    s16     fallSpeed;          // base downward step per tick of the fall in `DROP_IN`, fixed per placement on entry; each tick adjusts it by a term that grows with `stateFrame`
} _Actor223600Work;
STATIC_ASSERT_SIZEOF(_Actor223600Work, 0x214);
STATIC_ASSERT(OFFSET_OF(_Actor223600Work, rig) == OFFSET_OF(AnimDriverWork, rig), Actor223600Work_rig);
STATIC_ASSERT(OFFSET_OF(_Actor223600Work, driver) == OFFSET_OF(AnimDriverWork, state), Actor223600Work_driver);

/// The scratch-stack block of the drop-in state: one frame's step along one of
/// the model's own axes.
///
/// The state reserves one block a frame and reuses it for every step of that
/// frame. An axis of the root coordinate is read into `step`, normalised to
/// 4096 = 1.0 and scaled to the step's length in world units, and the result
/// is added to, or taken from, the coordinate's translation.
typedef struct {
    SVECTOR step;           // Axis of the root coordinate, then that axis scaled into the displacement applied to the translation
    byte    unknown_8[0x4]; // Reserved with the block and never accessed; role unproven
} _Actor223600AxisStepScratch;
STATIC_ASSERT_SIZEOF(_Actor223600AxisStepScratch, 0xC);

/// Effect record the spawn handler fills with the instance's own coordinate
/// and the 0x100 / 1 argument pair.
extern EffectSpawnArg D_actor_223600_80150B5C;

/// Enemy parameters the spawn handler installs at `Enemy::param`.
extern EnemyParams D_actor_223600_8014CFCC;

/// Animation-set table bound to the work block's context by `animationInitContext`.
extern AnimationSet* D_actor_223600_801509C0[26];

/// Message table the spawn handler publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_223600_80150B28[4];

/// Integer part of the last movement step `func_actor_223600_8014AA04`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static s32  _actor223600PollSoundCue(_Actor223600Work* work);
static void _actor223600Spawn(Enemy* enemy, Task* task);
static void _actor223600Walk(Enemy* enemy, Task* task);
static void _actor223600DropIn(Enemy* enemy, Task* task);
static void _actor223600Update(Enemy* enemy, Task* task);
static void _actor223600Hide(Enemy* enemy, Task* task);
static void _actor223600Task(Task* task);
static s32  _actor223600SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32  _actor223600ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

/// Loaded animation-set indices used by this package's state handlers.
enum {
    ACTOR_223600_ANIM_REQUEST_INITIAL = 1,
    ACTOR_223600_ANIM_REQUEST_WALK    = 2,
    ACTOR_223600_ANIM_REQUEST_FALL    = 14,
    ACTOR_223600_ANIM_REQUEST_LAND    = 15,
};

/// Applies one signed root-axis step within `_actor223600DropIn`.
///
/// Captures its live `task`, reserved `scratchTop` block and the `axisStep` /
/// `gteStep` views of that block's vector. `readAxis` must be a matrix-axis
/// reader; it and `stepDistance` are evaluated once. Distance uses parent-
/// coordinate units. Normalization removes matrix scale; GTE state is overwritten.
/// The step includes Y, ignores the freeze gate and leaves invalidation to the caller.
/// Expands to a compound block; invoke only within an already braced body.
#define ACTOR_223600_STEP_LOCAL_AXIS(readAxis, stepDistance)           \
    {                                                                  \
        (readAxis)(&task->extra.tmd->coords->coord, axisStep);         \
        VectorNormalSS(axisStep, axisStep);                            \
        gte_lddp(stepDistance);                                        \
        gte_ldsv(gteStep);                                             \
        gte_gpf12();                                                   \
        gte_stsv(gteStep);                                             \
        task->extra.tmd->coords->coord.t[0] += scratchTop[-1].step.vx; \
        task->extra.tmd->coords->coord.t[1] += axisStep->vy;           \
        task->extra.tmd->coords->coord.t[2] += axisStep->vz;           \
    }

static TmdSource _gActor223600BloodSucklerBody;

#include "../../shared/actor_contacts.h"

DamageAttack D_actor_223600_8014CFC8[1] = {
    { 24, 7 },
};

EnemyParams D_actor_223600_8014CFCC = { D_actor_223600_8014CFC8, 1, 6, 20, 3, 100, 0, 100, 0 };

PadScriptCmd D_actor_223600_8014CFDC[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_223600_8014CFE8[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor223600BloodSucklerBodySkeleton[6] = {
#include "assets/blood_suckler_body_skeleton.inc"
};

static u32 _gActor223600BloodSucklerBodyPartVerts[6] = {
#include "assets/blood_suckler_body_partVerts.inc"
};

static SVECTOR _gActor223600BloodSucklerBodyVerts[94] = {
#include "assets/blood_suckler_body_verts.inc"
};

static SVECTOR _gActor223600BloodSucklerBodyNormals[94] = {
#include "assets/blood_suckler_body_normals.inc"
};

static u32 _gActor223600BloodSucklerBodyStream[999] = {
#include "assets/blood_suckler_body_stream.inc"
};

static TmdSource _gActor223600BloodSucklerBody = {
    0,
    5624,
    1228,
    6,
    _gActor223600BloodSucklerBodyPartVerts,
    _gActor223600BloodSucklerBodyVerts,
    _gActor223600BloodSucklerBodyNormals,
    _gActor223600BloodSucklerBodySkeleton,
    _gActor223600BloodSucklerBodyStream,
};

static AnimationPackedPose _gActor223600Animation049A0Bank1[4] = {
#include "assets/actor_223600_animation_049A0_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation049A0Bank4[21] = {
#include "assets/actor_223600_animation_049A0_bank4.inc"
};

static AnimationRecord _gActor223600Animation049A0Records[43] = {
#include "assets/actor_223600_animation_049A0_records.inc"
};

static u16 _gActor223600Animation049A0Indices[6] = {
#include "assets/actor_223600_animation_049A0_indices.inc"
};

static AnimationSet _gActor223600Animation049A0 = {
    _gActor223600Animation049A0Records,
    _gActor223600Animation049A0Indices,
    { NULL, _gActor223600Animation049A0Bank1, NULL, NULL, _gActor223600Animation049A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation04C64Bank1[19] = {
#include "assets/actor_223600_animation_04C64_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation04C64Bank4[36] = {
#include "assets/actor_223600_animation_04C64_bank4.inc"
};

static AnimationRecord _gActor223600Animation04C64Records[71] = {
#include "assets/actor_223600_animation_04C64_records.inc"
};

static u16 _gActor223600Animation04C64Indices[6] = {
#include "assets/actor_223600_animation_04C64_indices.inc"
};

static AnimationSet _gActor223600Animation04C64 = {
    _gActor223600Animation04C64Records,
    _gActor223600Animation04C64Indices,
    { NULL, _gActor223600Animation04C64Bank1, NULL, NULL, _gActor223600Animation04C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation04F08Bank1[20] = {
#include "assets/actor_223600_animation_04F08_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation04F08Bank4[31] = {
#include "assets/actor_223600_animation_04F08_bank4.inc"
};

static AnimationRecord _gActor223600Animation04F08Records[65] = {
#include "assets/actor_223600_animation_04F08_records.inc"
};

static u16 _gActor223600Animation04F08Indices[6] = {
#include "assets/actor_223600_animation_04F08_indices.inc"
};

static AnimationSet _gActor223600Animation04F08 = {
    _gActor223600Animation04F08Records,
    _gActor223600Animation04F08Indices,
    { NULL, _gActor223600Animation04F08Bank1, NULL, NULL, _gActor223600Animation04F08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation050E8Bank1[6] = {
#include "assets/actor_223600_animation_050E8_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation050E8Bank4[38] = {
#include "assets/actor_223600_animation_050E8_bank4.inc"
};

static AnimationRecord _gActor223600Animation050E8Records[51] = {
#include "assets/actor_223600_animation_050E8_records.inc"
};

static u16 _gActor223600Animation050E8Indices[6] = {
#include "assets/actor_223600_animation_050E8_indices.inc"
};

static AnimationSet _gActor223600Animation050E8 = {
    _gActor223600Animation050E8Records,
    _gActor223600Animation050E8Indices,
    { NULL, _gActor223600Animation050E8Bank1, NULL, NULL, _gActor223600Animation050E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation05220Bank1[4] = {
#include "assets/actor_223600_animation_05220_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation05220Bank4[13] = {
#include "assets/actor_223600_animation_05220_bank4.inc"
};

static AnimationRecord _gActor223600Animation05220Records[40] = {
#include "assets/actor_223600_animation_05220_records.inc"
};

static u16 _gActor223600Animation05220Indices[6] = {
#include "assets/actor_223600_animation_05220_indices.inc"
};

static AnimationSet _gActor223600Animation05220 = {
    _gActor223600Animation05220Records,
    _gActor223600Animation05220Indices,
    { NULL, _gActor223600Animation05220Bank1, NULL, NULL, _gActor223600Animation05220Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation053C0Bank1[7] = {
#include "assets/actor_223600_animation_053C0_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation053C0Bank4[28] = {
#include "assets/actor_223600_animation_053C0_bank4.inc"
};

static AnimationRecord _gActor223600Animation053C0Records[42] = {
#include "assets/actor_223600_animation_053C0_records.inc"
};

static u16 _gActor223600Animation053C0Indices[6] = {
#include "assets/actor_223600_animation_053C0_indices.inc"
};

static AnimationSet _gActor223600Animation053C0 = {
    _gActor223600Animation053C0Records,
    _gActor223600Animation053C0Indices,
    { NULL, _gActor223600Animation053C0Bank1, NULL, NULL, _gActor223600Animation053C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation055D4Bank1[6] = {
#include "assets/actor_223600_animation_055D4_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation055D4Bank4[42] = {
#include "assets/actor_223600_animation_055D4_bank4.inc"
};

static AnimationRecord _gActor223600Animation055D4Records[60] = {
#include "assets/actor_223600_animation_055D4_records.inc"
};

static u16 _gActor223600Animation055D4Indices[6] = {
#include "assets/actor_223600_animation_055D4_indices.inc"
};

static AnimationSet _gActor223600Animation055D4 = {
    _gActor223600Animation055D4Records,
    _gActor223600Animation055D4Indices,
    { NULL, _gActor223600Animation055D4Bank1, NULL, NULL, _gActor223600Animation055D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation059E4Bank1[17] = {
#include "assets/actor_223600_animation_059E4_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation059E4Bank4[85] = {
#include "assets/actor_223600_animation_059E4_bank4.inc"
};

static AnimationRecord _gActor223600Animation059E4Records[111] = {
#include "assets/actor_223600_animation_059E4_records.inc"
};

static u16 _gActor223600Animation059E4Indices[6] = {
#include "assets/actor_223600_animation_059E4_indices.inc"
};

static AnimationSet _gActor223600Animation059E4 = {
    _gActor223600Animation059E4Records,
    _gActor223600Animation059E4Indices,
    { NULL, _gActor223600Animation059E4Bank1, NULL, NULL, _gActor223600Animation059E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation05CA4Bank1[12] = {
#include "assets/actor_223600_animation_05CA4_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation05CA4Bank4[52] = {
#include "assets/actor_223600_animation_05CA4_bank4.inc"
};

static AnimationRecord _gActor223600Animation05CA4Records[75] = {
#include "assets/actor_223600_animation_05CA4_records.inc"
};

static u16 _gActor223600Animation05CA4Indices[6] = {
#include "assets/actor_223600_animation_05CA4_indices.inc"
};

static AnimationSet _gActor223600Animation05CA4 = {
    _gActor223600Animation05CA4Records,
    _gActor223600Animation05CA4Indices,
    { NULL, _gActor223600Animation05CA4Bank1, NULL, NULL, _gActor223600Animation05CA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation05E7CBank1[5] = {
#include "assets/actor_223600_animation_05E7C_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation05E7CBank4[19] = {
#include "assets/actor_223600_animation_05E7C_bank4.inc"
};

static AnimationRecord _gActor223600Animation05E7CRecords[71] = {
#include "assets/actor_223600_animation_05E7C_records.inc"
};

static u16 _gActor223600Animation05E7CIndices[6] = {
#include "assets/actor_223600_animation_05E7C_indices.inc"
};

static AnimationSet _gActor223600Animation05E7C = {
    _gActor223600Animation05E7CRecords,
    _gActor223600Animation05E7CIndices,
    { NULL, _gActor223600Animation05E7CBank1, NULL, NULL, _gActor223600Animation05E7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation061F8Bank1[15] = {
#include "assets/actor_223600_animation_061F8_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation061F8Bank4[70] = {
#include "assets/actor_223600_animation_061F8_bank4.inc"
};

static AnimationRecord _gActor223600Animation061F8Records[95] = {
#include "assets/actor_223600_animation_061F8_records.inc"
};

static u16 _gActor223600Animation061F8Indices[6] = {
#include "assets/actor_223600_animation_061F8_indices.inc"
};

static AnimationSet _gActor223600Animation061F8 = {
    _gActor223600Animation061F8Records,
    _gActor223600Animation061F8Indices,
    { NULL, _gActor223600Animation061F8Bank1, NULL, NULL, _gActor223600Animation061F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation065A8Bank1[14] = {
#include "assets/actor_223600_animation_065A8_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation065A8Bank4[64] = {
#include "assets/actor_223600_animation_065A8_bank4.inc"
};

static AnimationRecord _gActor223600Animation065A8Records[117] = {
#include "assets/actor_223600_animation_065A8_records.inc"
};

static u16 _gActor223600Animation065A8Indices[6] = {
#include "assets/actor_223600_animation_065A8_indices.inc"
};

static AnimationSet _gActor223600Animation065A8 = {
    _gActor223600Animation065A8Records,
    _gActor223600Animation065A8Indices,
    { NULL, _gActor223600Animation065A8Bank1, NULL, NULL, _gActor223600Animation065A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation067E4Bank1[14] = {
#include "assets/actor_223600_animation_067E4_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation067E4Bank4[28] = {
#include "assets/actor_223600_animation_067E4_bank4.inc"
};

static AnimationRecord _gActor223600Animation067E4Records[60] = {
#include "assets/actor_223600_animation_067E4_records.inc"
};

static u16 _gActor223600Animation067E4Indices[6] = {
#include "assets/actor_223600_animation_067E4_indices.inc"
};

static AnimationSet _gActor223600Animation067E4 = {
    _gActor223600Animation067E4Records,
    _gActor223600Animation067E4Indices,
    { NULL, _gActor223600Animation067E4Bank1, NULL, NULL, _gActor223600Animation067E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation06A28Bank1[10] = {
#include "assets/actor_223600_animation_06A28_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation06A28Bank4[40] = {
#include "assets/actor_223600_animation_06A28_bank4.inc"
};

static AnimationRecord _gActor223600Animation06A28Records[62] = {
#include "assets/actor_223600_animation_06A28_records.inc"
};

static u16 _gActor223600Animation06A28Indices[6] = {
#include "assets/actor_223600_animation_06A28_indices.inc"
};

static AnimationSet _gActor223600Animation06A28 = {
    _gActor223600Animation06A28Records,
    _gActor223600Animation06A28Indices,
    { NULL, _gActor223600Animation06A28Bank1, NULL, NULL, _gActor223600Animation06A28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor223600Animation06B78Bank1[6] = {
#include "assets/actor_223600_animation_06B78_bank1.inc"
};

static AnimationPackedRotation _gActor223600Animation06B78Bank4[20] = {
#include "assets/actor_223600_animation_06B78_bank4.inc"
};

static AnimationRecord _gActor223600Animation06B78Records[33] = {
#include "assets/actor_223600_animation_06B78_records.inc"
};

static u16 _gActor223600Animation06B78Indices[6] = {
#include "assets/actor_223600_animation_06B78_indices.inc"
};

static AnimationSet _gActor223600Animation06B78 = {
    _gActor223600Animation06B78Records,
    _gActor223600Animation06B78Indices,
    { NULL, _gActor223600Animation06B78Bank1, NULL, NULL, _gActor223600Animation06B78Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_223600_801509C0[26] = {
    NULL,
    &_gActor223600Animation049A0,
    &_gActor223600Animation04C64,
    &_gActor223600Animation04F08,
    &_gActor223600Animation050E8,
    &_gActor223600Animation05220,
    &_gActor223600Animation053C0,
    &_gActor223600Animation055D4,
    &_gActor223600Animation059E4,
    NULL,
    &_gActor223600Animation05E7C,
    &_gActor223600Animation061F8,
    &_gActor223600Animation065A8,
    &_gActor223600Animation067E4,
    &_gActor223600Animation06A28,
    &_gActor223600Animation06B78,
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

u8 D_actor_223600_80150A28[256] = {
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

TaskMessageEntry D_actor_223600_80150B28[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor223600SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor223600ApplyCommand },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_223600_80150B48 = { { { TASK_BODY_TMD, 96 } }, _actor223600Task, { .model = &_gActor223600BloodSucklerBody } };

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

EffectSpawnArg D_actor_223600_80150B5C = { 0 };

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// Selects a sound script from slot 1's animation cue, or returns zero.
///
/// WALK and set 3 emit bank 0x400C entry 1 once per held trigger cue; leaving
/// a trigger cue clears that latch. Set 5 emits entry 5 on a control-jump tick.
/// Other sets leave the latch unchanged. Requires initialized rig slot 1; the
/// returned id has a zero instance byte, which the update supplies.
static s32 _actor223600PollSoundCue(_Actor223600Work* work)
{
    enum {
        ACTOR_223600_SOUND_ANIM_CUE_PAIR     = 3,
        ACTOR_223600_SOUND_ANIM_CONTROL_JUMP = 5,
        ACTOR_223600_WALK_CUE_FIRST          = 0x11,
        ACTOR_223600_WALK_CUE_SECOND         = 0x15,
        ACTOR_223600_PAIR_CUE_FIRST          = 0xD,
        ACTOR_223600_PAIR_CUE_SECOND         = 0x12,
        ACTOR_223600_SOUND_CUE_POSE          = 0x400C0001,
        ACTOR_223600_SOUND_CUE_CONTROL_JUMP  = 0x400C0005,
    };
    u16 cueIndex;
    s32 promotedCueIndex;

    switch (work->driver.requestedSet) {
        case ACTOR_223600_ANIM_REQUEST_WALK:
            cueIndex         = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            promotedCueIndex = cueIndex;
            if (promotedCueIndex != ACTOR_223600_WALK_CUE_SECOND) {
                goto checkOtherWalkCue;
            }
        latchCue:
            if (work->lastSoundCueIndex == promotedCueIndex) {
                goto heldCue;
            }
            work->lastSoundCueIndex = cueIndex;
            return ACTOR_223600_SOUND_CUE_POSE;
        checkOtherWalkCue:
            if (promotedCueIndex == ACTOR_223600_WALK_CUE_FIRST) {
                goto latchCue;
            }
        clearCueLatch:
            work->lastSoundCueIndex = 0;
            break;
        case ACTOR_223600_SOUND_ANIM_CUE_PAIR:
            cueIndex         = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            promotedCueIndex = cueIndex;
            if (promotedCueIndex != ACTOR_223600_PAIR_CUE_FIRST && promotedCueIndex != ACTOR_223600_PAIR_CUE_SECOND) {
                goto clearCueLatch;
            }
            goto latchCue;
        heldCue:
            work->lastSoundCueIndex = cueIndex;
            break;
        case ACTOR_223600_SOUND_ANIM_CONTROL_JUMP:
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return ACTOR_223600_SOUND_CUE_CONTROL_JUMP;
            }
            break;
    }
    return 0;
}

/// Initializes this placement's enemy model, animation rig and owned task work.
///
/// Requires a live TMD model with coordinates 0..5 and a live enemy in the
/// task's second spawn argument. Allocation failure destroys the task and enemy.
/// On success, the task owns the work and lighting matrices until teardown;
/// the rig borrows the loaded animation table and the root borrows the view
/// coordinate. Starts hidden, publishes the effect anchor and advances task state.
static void _actor223600Spawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_223600_PREVIOUS_STATE_UNSET = -1 };
    SVECTOR           initialFacingStep;
    _Actor223600Work* work;
    TmdObject*        model;
    GfxCoord*         coord;
    u32               placementIndex;
    u32               placementIsOdd;
    s32               hitPoints;

    // The task owns the work allocation; allocation failure destroys the instance.
    model      = task->extra.tmd;
    coord      = model->coords;
    work       = memCalloc(sizeof(_Actor223600Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    coord->parent  = &gGfxViewCoord;
    task->msgTable = D_actor_223600_80150B28;
    model->flags   = 0;
    animationInitContext(&work->rig.anim, D_actor_223600_801509C0, model, work->rig.poses, work->rig.slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->hpMax                  = 1;
    enemy->hp                     = 1;
    enemy->reactionFlags          = 0;
    hitPoints                     = D_actor_223600_8014CFCC.hpMax;
    enemy->param                  = &D_actor_223600_8014CFCC;
    enemy->recs                   = 0;
    enemy->hpMax                  = hitPoints;
    enemy->hp                     = hitPoints;

    work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_INITIAL;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(task);
    work->field_17E     = 0;
    work->field_8       = 0;
    model->lightMtx     = &work->lightMtx;
    model->colorMtx     = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_184     = 5;
    work->field_186     = 0x14;

    // Odd placements speed up playback; even placements subtract half their index.
    placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    placementIsOdd = placementIndex & 1;
    if (placementIsOdd == 1) {
        work->driver.rate += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_186   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_184   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= placementIndex >> 1;
        work->field_186   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_184   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }

    work->spawnPos.vx = task->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = task->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = task->extra.tmd->coords->coord.t[2];

    // Keep the initial horizontal-facing calculation: it also updates GTE state.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &initialFacingStep);
    initialFacingStep.vy = 0;
    _actorMovementBuildDisplacement(&initialFacingStep, 0x3E8);

    work->state     = ACTOR_223600_STATE_HIDDEN;
    work->prevState = ACTOR_223600_PREVIOUS_STATE_UNSET;

    D_actor_223600_80150B5C.coord      = task->extra.tmd->coords;
    D_actor_223600_80150B5C.spawnArgLo = 0x100;
    D_actor_223600_80150B5C.spawnArgHi = 1;
    task->state++;
}

/// Turns the model root toward the walk waypoint by at most 16 units per tick.
///
/// `task` must have a live model with an initialized root rotation. The caller's
/// reserved `turnScratch` holds the waypoint's signed 16-bit X/Z offset from
/// the root in world units. The offset is unchanged and Y is ignored.
/// Angles use 4096 units per turn. The wrapped bearing difference is clamped to
/// [-16, 16], then `turnScratch->angle` holds the resulting absolute yaw,
/// without wrapping that sum again. Rebuilds a pure Y rotation and preserves
/// translation.
///
/// Requires 0x24 free bytes below the block on the initialized scratch stack
/// for the rotation routine. The caller releases the block and marks the root
/// `GRAPHICS_COORD_DIRTY`; no pointer is retained.
static __inline__ void _actor223600TurnTowardWalkTarget(Task* task, ActorTurnScratch* turnScratch)
{
    enum { ACTOR_223600_WALK_TURN_LIMIT = 16 }; // 4096 angle units per turn
    GfxCoord* rootCoord;

    // Limit the signed turn before converting it back to an absolute heading.
    rootCoord          = task->extra.tmd->coords;
    turnScratch->angle = _actorAngleNormalizeYaw(ratan2(turnScratch->delta.vx, turnScratch->delta.vz) -
                                                 ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    if (turnScratch->angle > ACTOR_223600_WALK_TURN_LIMIT) {
        turnScratch->angle = ACTOR_223600_WALK_TURN_LIMIT;
    }
    if (turnScratch->angle < -ACTOR_223600_WALK_TURN_LIMIT) {
        turnScratch->angle = -ACTOR_223600_WALK_TURN_LIMIT;
    }
    turnScratch->angle += ratan2(-task->extra.tmd->coords->coord.m[2][0],
                                 task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turnScratch->angle, GRAPHICS_ROTATION_REPLACE);
}

/// Initializes or advances the hidden-to-walk state toward its fixed waypoint.
///
/// Requires initialized work and a live model. Placements 0 and 1 have fixed
/// start positions; other indices retain their position. Turns by at most
/// 16/4096 of a revolution and steps five parent-coordinate units per tick.
/// The movement freeze gate skips translation only; turning and animation continue.
/// Reserves one turn block, plus the nested movement helper's vector.
static void _actor223600Walk(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_223600_WALK_DISTANCE = 5, // Parent-coordinate units per tick
    };
    _Actor223600Work* work;
    ActorTurnScratch* scratchTop;
    ActorTurnScratch* turnScratch;
    TmdObject*        model;
    u32               placementIndex;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->walkTarget.vx = 0x115D;
        work->walkTarget.vy = 1;
        work->walkTarget.vz = 0x12D5;

        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA8C;
                task->extra.tmd->coords->coord.t[1] = 1;
                task->extra.tmd->coords->coord.t[2] = 0xA28;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x384;
                task->extra.tmd->coords->coord.t[1] = placementIndex;
                task->extra.tmd->coords->coord.t[2] = 0x960;
                break;
        }
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0, GRAPHICS_ROTATION_REPLACE);
        work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_WALK;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }

    // Turn toward the fixed waypoint before stepping along the new facing.
    work->stateFrame++;
    scratchTop                             = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    scratchTop[-1].delta.vx                = work->walkTarget.vx - task->extra.tmd->coords->coord.t[0];
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = scratchTop - 1;
    turnScratch                            = scratchTop - 1;
    turnScratch->delta.vy                  = 0;
    turnScratch->delta.vz                  = work->walkTarget.vz - task->extra.tmd->coords->coord.t[2];

    _actor223600TurnTowardWalkTarget(task, turnScratch);
    _actorMovementStepForward(task->extra.tmd->coords, ACTOR_223600_WALK_DISTANCE);
    _animDriverTick(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Runs the placement-specific walk lead-in, fall and landing sequence.
///
/// Placements 0..2 have defined entry poses; other indices retain the existing
/// pose and animation. World Y grows downward: falling starts at frame 40 and
/// reaching positive Y selects the landing set with its frame counter reset.
/// Placement 0 has the longer landing sway. Landing uses root-axis steps and
/// rotates coordinate 1; its steps continue while actor translation is frozen.
/// Requires initialized work, coordinates 0..5, loaded animation sets and
/// scratch room for one axis-step block plus nested helper storage.
static void _actor223600DropIn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_223600_DROP_WALK_START_FRAME  = 19,
        ACTOR_223600_DROP_APPROACH_FRAME    = 35,
        ACTOR_223600_DROP_FALL_START_FRAME  = 40,
        ACTOR_223600_DROP_WALK_END_FRAME    = 50,
        ACTOR_223600_DROP_STEP_CYCLE_FRAMES = 5,
        ACTOR_223600_DROP_SLOW_STEP_FRAMES  = 2,
        ACTOR_223600_LAND_FORWARD_END_FRAME = 11,
        ACTOR_223600_LAND_LIFT_START_FRAME  = 5,
        ACTOR_223600_LAND_SWAY_START_FRAME  = 14,
        ACTOR_223600_LAND_SWAY_BIAS_FRAME   = 15,
        ACTOR_223600_LAND_SHORT_END_FRAME   = 30,
        ACTOR_223600_LAND_LONG_END_FRAME    = 37,
    };
    _Actor223600Work*            work;
    void**                       reserveCursorSlot;
    void**                       releaseCursorSlot;
    _Actor223600AxisStepScratch* scratchTop;
    SVECTOR*                     axisStep;
    SVECTOR*                     gteStep;
    TmdObject*                   model;
    s32                          placementIndex;
    s16                          fallFrame;
    u16                          fallStepPhase;
    s16                          swayFrame;

    work = task->work;
    // Stage each placement above the floor; placement 2 starts partway through its fall.
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA1E;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0x1590;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x7D0, GRAPHICS_ROTATION_REPLACE);
                work->stateFrame          = -0xA;
                work->fallSpeed           = 0xB4;
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_WALK;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x12C;
                task->extra.tmd->coords->coord.t[1] = -0x4C4;
                task->extra.tmd->coords->coord.t[2] = 0x1194;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x3E8, GRAPHICS_ROTATION_REPLACE);
                work->fallSpeed           = 0xBE;
                work->stateFrame          = 0;
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_WALK;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                break;
            case 2:
                task->extra.tmd->coords->coord.t[0] = 0x104A;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0xFE6;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x400, GRAPHICS_ROTATION_REPLACE);
                _actorMovementStepForward(task->extra.tmd->coords, 0x15E);
                work->stateFrame          = 0x3C;
                work->fallSpeed           = 0x50;
                work->driver.rate         = 4 * ANIMATION_RATE_ONE;
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_FALL;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                do {
                    _animDriverTick(task);
                } while ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) == 0);
                work->driver.rate = ANIMATION_RATE_ONE;
                work->fallSpeed   = 0x46;
                break;
        }
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }

    // The walk is a lead-in; landing restarts this counter for the axis-driven finish.
    if (work->stateFrame == 0) {
        if (work->driver.requestedSet == ACTOR_223600_ANIM_REQUEST_WALK) {
            work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_FALL;
            work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        }
    }
    _animDriverTick(task);

    reserveCursorSlot                                               = SCRATCH_HEAD_ADDR;
    scratchTop                                                      = SCRATCH_HEAD_AT(reserveCursorSlot, _Actor223600AxisStepScratch);
    axisStep                                                        = &scratchTop[-1].step;
    gteStep                                                         = &scratchTop[-1].step;
    SCRATCH_HEAD_AT(reserveCursorSlot, _Actor223600AxisStepScratch) = scratchTop - 1;

    switch (work->driver.requestedSet) {
        case ACTOR_223600_ANIM_REQUEST_FALL:
            work->driver.rate = ANIMATION_RATE_ONE;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) != 2) {
                if (work->stateFrame < ACTOR_223600_DROP_WALK_END_FRAME) {
                    if (work->stateFrame >= ACTOR_223600_DROP_FALL_START_FRAME) {
                        _actorMovementStepForward(task->extra.tmd->coords, 0x16);
                    } else if (work->stateFrame >= ACTOR_223600_DROP_APPROACH_FRAME) {
                        _actorMovementStepForward(task->extra.tmd->coords, 0xA);
                    } else if (work->stateFrame >= ACTOR_223600_DROP_WALK_START_FRAME) {
                        _actorMovementStepForward(task->extra.tmd->coords, 0xA);
                    }
                }
            }
            fallFrame = work->stateFrame;
            if (fallFrame >= ACTOR_223600_DROP_FALL_START_FRAME) {
                fallStepPhase = fallFrame % ACTOR_223600_DROP_STEP_CYCLE_FRAMES;
                if (fallStepPhase < ACTOR_223600_DROP_SLOW_STEP_FRAMES) {
                    task->extra.tmd->coords->coord.t[1] += work->fallSpeed - (fallFrame - ACTOR_223600_DROP_FALL_START_FRAME) / 4;
                } else {
                    task->extra.tmd->coords->coord.t[1] += work->fallSpeed + (fallFrame - ACTOR_223600_DROP_FALL_START_FRAME) / 2;
                }
                if (task->extra.tmd->coords->coord.t[1] >= 2) {
                    task->extra.tmd->coords->coord.t[1] = 1;
                }
            }
            if (task->extra.tmd->coords->coord.t[1] > 0) {
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_LAND;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                work->stateFrame          = 0;
            }
            break;
        case ACTOR_223600_ANIM_REQUEST_LAND:
            // Landing steps use the root axes without the actor-freeze gate.
            work->driver.rate = ANIMATION_RATE_ONE;
            if (work->stateFrame < ACTOR_223600_LAND_FORWARD_END_FRAME) {
                ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixZAxis, 0x23);
            }
            if (work->stateFrame >= ACTOR_223600_LAND_LIFT_START_FRAME && work->stateFrame < ACTOR_223600_LAND_SWAY_START_FRAME) {
                ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixYAxis, -0x14);
                gfxRotMatrixX(&task->extra.tmd->coords[1].coord,
                              -((work->stateFrame - 4) * 0xCC), GRAPHICS_ROTATION_COMPOSE);
            }
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                if (work->stateFrame >= ACTOR_223600_LAND_SWAY_START_FRAME && work->stateFrame < ACTOR_223600_LAND_LONG_END_FRAME) {
                    ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixYAxis, 0xB);
                    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, axisStep);
                    VectorNormalSS(axisStep, axisStep);
                    gte_lddp(-0x1D);
                    gte_ldsv(gteStep);
                    gte_gpf12();
                    gte_stsv(gteStep);
                    swayFrame = work->stateFrame - ACTOR_223600_LAND_SWAY_BIAS_FRAME;
                    switch (swayFrame) {
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                        case 5:
                        case 7:
                        case 9:
                            task->extra.tmd->coords->coord.t[0] -= gteStep->vx;
                            task->extra.tmd->coords->coord.t[1] -= gteStep->vy;
                            task->extra.tmd->coords->coord.t[2] -= gteStep->vz;
                            gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                            gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                          (work->stateFrame - 0xD) * 0x55 - 0x6E, GRAPHICS_ROTATION_COMPOSE);
                            break;
                        default:
                            task->extra.tmd->coords->coord.t[0] += gteStep->vx;
                            task->extra.tmd->coords->coord.t[1] += gteStep->vy;
                            task->extra.tmd->coords->coord.t[2] += gteStep->vz;
                            gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                            gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                          (work->stateFrame - 0xD) * 0x55, GRAPHICS_ROTATION_COMPOSE);
                            break;
                    }
                }
                if (work->stateFrame >= ACTOR_223600_LAND_LONG_END_FRAME) {
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                }
            } else {
                if (work->stateFrame >= ACTOR_223600_LAND_SWAY_START_FRAME && work->stateFrame < ACTOR_223600_LAND_SHORT_END_FRAME) {
                    ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixYAxis, 0xB);
                    ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixXAxis, -0x1D);
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                  (work->stateFrame - 0xD) * 0x78, GRAPHICS_ROTATION_COMPOSE);
                }
                if (work->stateFrame >= ACTOR_223600_LAND_SHORT_END_FRAME) {
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                }
            }
            break;
        case ACTOR_223600_ANIM_REQUEST_WALK:
            work->driver.rate = 2 * ANIMATION_RATE_ONE;
            break;
    }
    // Both invalidations are retained from the original landing-state tail.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    releaseCursorSlot                     = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_AT(releaseCursorSlot, _Actor223600AxisStepScratch);
    work->stateFrame++;
}

/// The three state handlers the tick below picks between by the work block's
/// `state` (`ACTOR_223600_STATE_*`), copied onto the stack before the call. The copy is a three-word
/// block move out of the unit's `.rodata`, which is why the table is a rodata
/// object rather than a local initialiser.
static const EnemyTaskFuncTable3 D_actor_223600_80149E4C = {
    {
        _actor223600Hide,
        _actor223600Walk,
        _actor223600DropIn,
    },
};

/// Advances the enemy state, requests spatial sound cues and refreshes lighting.
///
/// Requires initialized work with state HIDDEN, WALK or DROP_IN and a live
/// model whose composed matrix supplies sound positioning and lighting. Paused
/// and hidden combat-control modes return before state, sound or lighting work.
/// The placement index supplies the sound instance byte. A dirty coordinate or
/// view change schedules lighting from the next tick's cached translation.
static void _actor223600Update(Enemy* enemy, Task* task)
{
    enum { ACTOR_223600_SOUND_INSTANCE_SHIFT = 8 };
    _Actor223600Work*   work;
    EnemyTaskFuncTable3 stateHandlers;
    s32                 soundCue;
    s32                 soundId;
    s32                 panOffset;

    work          = task->work;
    stateHandlers = D_actor_223600_80149E4C;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_223600_STATE_HIDDEN) {
                task->extra.tmd->flags = 0;
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_223600_STATE_HIDDEN) {
                task->extra.tmd->flags = 0;
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    // Publish entry before dispatch so a new state initializes exactly once.
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    stateHandlers.funcs[work->state](enemy, task);

    soundCue = _actor223600PollSoundCue(work);
    if (soundCue != 0) {
        soundId   = soundCue | (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_223600_SOUND_INSTANCE_SHIFT);
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(
            soundId, panOffset,
            (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // Lighting samples the cached translation before this frame's root is composed.
    if (work->relightPending != 0) {
        worldCoordSetModelLighting(task->extra.tmd,
                                   task->extra.tmd->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (task->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
        work->relightPending = 1;
        return;
    }
    work->relightPending = 0;
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `_actor223600Task` runs by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_223600_80149E58 = {
    {
        _actor223600Spawn,
        _actor223600Update,
        enemyDestroy,
    },
};

/// Applies a model-draw message and selects the walk or hidden actor state.
///
/// Requires a live TMD model and initialized work. HIDE and SHOW select WALK
/// and allocate a primitive buffer if missing; their active-draw flag differs.
/// The two SKIP_AUTO_BUFFER modes select HIDDEN, preserving other flags or
/// clearing them first. Other modes change nothing. Ignores the message ID
/// and second argument; returns zero and retains no payload.
static s32 _actor223600SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    TmdObject*        model = task->extra.tmd;
    _Actor223600Work* work  = task->work;

    switch (drawMode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_223600_STATE_WALK;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_223600_STATE_WALK;
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_223600_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = ACTOR_223600_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Records an actor command and applies this room's drop-in or hide selector.
///
/// Requires initialized work and a readable four-byte command through dispatch.
/// Records stage, area and the selector's low byte for every namespace, but
/// changes state only for `ACTOR_223600_COMMAND_CONTEXT` and its known selectors.
/// Ignores the message ID and second argument; returns zero and retains no pointer.
static s32 _actor223600ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor223600Work* work;

    work                   = task->work;
    work->lastCommandStage = command->context.loc.stage;
    work->lastCommandArea  = command->context.loc.area;
    work->lastCommand      = command->command;
    if (command->context.key == ACTOR_223600_COMMAND_CONTEXT) {
        switch (command->command) {
            case ACTOR_223600_COMMAND_HIDE_UNTIL_CUTSCENE:
                work->state = ACTOR_223600_STATE_HIDDEN;
                break;
            case ACTOR_223600_COMMAND_DROP_IN:
                work->state = ACTOR_223600_STATE_DROP_IN;
                break;
            case ACTOR_223600_COMMAND_HIDE:
                work->state = ACTOR_223600_STATE_HIDDEN;
                break;
            case ACTOR_223600_COMMAND_IGNORED:
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Hides the model and disables lock-on on the hidden state's entry tick.
///
/// Requires initialized work, a live enemy and its TMD model; later ticks do nothing.
static void _actor223600Hide(Enemy* enemy, Task* task)
{
    _Actor223600Work* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Dispatches this enemy's spawn, update or teardown task state.
///
/// Requires task state 0..2 and a live enemy in the second spawn argument.
/// Spawn creates task-owned work, update requires it and teardown releases it.
/// The table is copied by value; the actor overlay must remain loaded for the call.
/// Spawn failure and teardown destroy the task and enemy before returning.
static void _actor223600Task(Task* task)
{
    EnemyTaskFuncTable3 taskHandlers;

    taskHandlers = D_actor_223600_80149E58;
    taskHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
