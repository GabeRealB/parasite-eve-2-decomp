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
#include "../../shared/coord_math.h"
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

static void func_actor_223600_8014CF3C(Enemy* arg0, Task* arg1);

static TmdSource _gActor223600BloodSucklerBody;
void             func_actor_223600_8014CF6C(Task*);

s32 func_actor_223600_8014CC04(Task*, s32, s32, s32);
s32 func_actor_223600_8014CCD4(Task*, s32, ActorCommand*, s32);

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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_223600_8014CC04 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_223600_8014CCD4 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_223600_80150B48 = { { { TASK_BODY_TMD, 96 } }, func_actor_223600_8014CF6C, { .model = &_gActor223600BloodSucklerBody } };

static SVECTOR ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_223600_80150B5C = { 0 };

static s32             func_actor_223600_8014B464(_Actor223600Work* arg0);
static __inline__ void Actor223600_ScaleForward(SVECTOR* dir, s16 amount);
static __inline__ void Actor223600_MoveForward(GfxCoord* coord, s16 amount);
static void            func_actor_223600_8014B540(Enemy* enemy, Task* task);
static void            func_actor_223600_8014B840(Enemy* enemy, Task* task);
static void            func_actor_223600_8014BBF4(Enemy* enemy, Task* task);
static void            func_actor_223600_8014CA00(Enemy* enemy, Task* task);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// On animations 2 and 3 (`driver.requestedSet`), reports 0x400C0001 the first
/// time rig slot 1's cue index reaches one of that animation's trigger values
/// (latched in `lastSoundCueIndex`); on animation 5, 0x400C0005 while slot 1
/// reports `ANIMATION_SLOT_FOLLOWED_JUMP`.
/// Returns 0 otherwise.
static s32 func_actor_223600_8014B464(_Actor223600Work* arg0)
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
        case 5:
            if (arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

/// Normalises `dir` in place and scales it to `amount`/0x1000 of unit length on
/// the GTE. The pointer stays in one register across `VectorNormalSS` because
/// the GTE loads read it back afterwards.
static __inline__ void Actor223600_ScaleForward(SVECTOR* dir, s16 amount)
{
    VectorNormalSS(dir, dir);
    gte_lddp(amount);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
}

/// Steps the model `amount` units along its facing -- the coordinate matrix's z
/// column, normalised and GTE-scaled in a scratch-pad vector -- and invalidates
/// the coordinate. Skipped entirely while `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` is 1.
static __inline__ void Actor223600_MoveForward(GfxCoord* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        vec                           = head - 1;
        SCRATCH_STACK_CURSOR(SVECTOR) = vec;
        gfxReadMatrixZAxis(&coord->coord, vec);
        Actor223600_ScaleForward(vec, amount);
        coord->coord.t[0]  += head[-1].vx;
        coord->coord.t[1]  += vec->vy;
        coord->coord.t[2]  += vec->vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Spawn state of this enemy: allocates the 0x214 work block, publishes it as
/// `Task::work`, reparents the model to `gGfxViewCoord`, seeds its animation
/// slots from `D_actor_223600_801509C0` and hangs the enemy's display node off
/// part 2 of the model's coordinate array. HP and max HP both come from
/// `D_actor_223600_8014CFCC`, which also picks the opening motion through
/// `_animDriverTick`. The placement index in `Enemy::placeKey` biases the
/// initial values in `driver.rate`, `field_184` and `field_186`: odd indices
/// add the index, even indices subtract half of it. The model's world
/// position is sampled into `spawnPos` and its facing is
/// normalised and scaled on the GTE, and the instance is published as the
/// overlay's anchor `D_actor_223600_80150B5C`.
static void func_actor_223600_8014B540(Enemy* enemy, Task* task)
{
    SVECTOR           dir;
    _Actor223600Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;
    u32               placementIndex;
    u32               placementParity;
    s32               hp;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(_Actor223600Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    coord->parent  = &gGfxViewCoord;
    task->msgTable = D_actor_223600_80150B28;
    obj->flags     = 0;
    animationInitContext(&work->rig.anim, D_actor_223600_801509C0, obj, work->rig.poses, work->rig.slots);

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
    hp                            = D_actor_223600_8014CFCC.hpMax;
    enemy->param                  = &D_actor_223600_8014CFCC;
    enemy->recs                   = 0;
    enemy->hpMax                  = hp;
    enemy->hp                     = hp;

    work->driver.requestedSet = 1;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(task);
    work->field_17E     = 0;
    work->field_8       = 0;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_184     = 5;
    work->field_186     = 0x14;

    placementIndex  = (u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT);
    placementParity = placementIndex & 1;
    if (placementParity == 1) {
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

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    Actor223600_ScaleForward(&dir, 0x3E8);

    work->state     = ACTOR_223600_STATE_HIDDEN;
    work->prevState = -1;

    D_actor_223600_80150B5C.coord      = task->extra.tmd->coords;
    D_actor_223600_80150B5C.spawnArgLo = 0x100;
    D_actor_223600_80150B5C.spawnArgHi = 1;
    task->state++;
}

/// `ACTOR_223600_STATE_WALK`. On the frame it is entered (`stateEntered` set) it
/// allocates the model's draw buffers, sets the target position in
/// `walkTarget`, writes the starting world position for this enemy's
/// placement index -- two start points, any other index leaving the
/// coordinate alone -- faces the model down +Z and restarts animation 2. On
/// every later frame it counts the frame in `stateFrame`, turns the model by up to
/// 0x10 towards the target (the clamped yaw kept in the scratch block) and
/// walks it 5 units forward.
static void func_actor_223600_8014B840(Enemy* enemy, Task* task)
{
    _Actor223600Work* work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    GfxCoord*         coord;
    TmdObject*        obj;
    u32               mode;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->walkTarget.vx = 0x115D;
        work->walkTarget.vy = 1;
        work->walkTarget.vz = 0x12D5;

        mode = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (mode) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA8C;
                task->extra.tmd->coords->coord.t[1] = 1;
                task->extra.tmd->coords->coord.t[2] = 0xA28;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x384;
                task->extra.tmd->coords->coord.t[1] = mode;
                task->extra.tmd->coords->coord.t[2] = 0x960;
                break;
        }
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0, 1);
        work->driver.requestedSet = 2;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }

    work->stateFrame++;
    head                                   = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    head[-1].delta.vx                      = work->walkTarget.vx - task->extra.tmd->coords->coord.t[0];
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1;
    turn                                   = head - 1;
    turn->delta.vy                         = 0;
    turn->delta.vz                         = work->walkTarget.vz - task->extra.tmd->coords->coord.t[2];

    coord       = task->extra.tmd->coords;
    turn->angle = actorNormalizeYaw(ratan2(head[-1].delta.vx, turn->delta.vz) -
                                    ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (turn->angle > 0x10) {
        turn->angle = 0x10;
    }
    if (turn->angle < -0x10) {
        turn->angle = -0x10;
    }
    turn->angle += ratan2(-task->extra.tmd->coords->coord.m[2][0],
                          task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turn->angle, 1);
    Actor223600_MoveForward(task->extra.tmd->coords, 5);
    _animDriverTick(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// `ACTOR_223600_STATE_DROP_IN`. On the frame it is entered (`stateEntered` set)
/// it allocates the model's draw buffers and puts the model above the floor at
/// the point the enemy's placement index selects -- two of them just face the
/// model and start animation 2, while the third steps it forward and
/// starts animation 0xE, advancing it until rig slot 1 reports
/// `ANIMATION_SLOT_REACHED_BOUNDARY`. Every later frame it runs one step of the
/// animation `driver.requestedSet` names: 0xE lowers the model by about `fallSpeed` a frame
/// from frame 0x28 of `stateFrame`, stepping it forward while the counter is inside the walk window, and
/// hands over to 0xF once the model's world Y goes positive; 0xF walks the
/// coordinate along its own axes on the GTE and swings part 1 through the
/// flourish, in a longer form for the nibble-0 context than for the others;
/// animation 2 only doubles `driver.rate` until `stateFrame` reaches 0 and 0xE takes over. The scratch block comes off
/// the scratch stack under three names -- `head`, whose negative index the
/// world-X step reads, `vec`, which the column and normalise calls take, and
/// `gte`, which the GTE round trip reads back -- and the two the scratch stack
/// pointers are the carve and the release, each materialised where it is used.
static void func_actor_223600_8014BBF4(Enemy* enemy, Task* task)
{
    _Actor223600Work*            work;
    void**                       push;
    void**                       pop;
    _Actor223600AxisStepScratch* head;
    SVECTOR*                     vec;
    SVECTOR*                     gte;
    TmdObject*                   obj;
    s32                          mode;
    s16                          frame;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        mode = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (mode) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA1E;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0x1590;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x7D0, 1);
                work->stateFrame          = -0xA;
                work->fallSpeed           = 0xB4;
                work->driver.requestedSet = 2;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x12C;
                task->extra.tmd->coords->coord.t[1] = -0x4C4;
                task->extra.tmd->coords->coord.t[2] = 0x1194;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x3E8, 1);
                work->fallSpeed           = 0xBE;
                work->stateFrame          = 0;
                work->driver.requestedSet = 2;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                break;
            case 2:
                task->extra.tmd->coords->coord.t[0] = 0x104A;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0xFE6;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x400, 1);
                Actor223600_MoveForward(task->extra.tmd->coords, 0x15E);
                work->stateFrame          = 0x3C;
                work->fallSpeed           = 0x50;
                work->driver.rate         = 4 * ANIMATION_RATE_ONE;
                work->driver.requestedSet = 0xE;
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

    if (work->stateFrame == 0) {
        if (work->driver.requestedSet == 2) {
            work->driver.requestedSet = 0xE;
            work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        }
    }
    _animDriverTick(task);

    push                                               = SCRATCH_HEAD_ADDR;
    head                                               = SCRATCH_HEAD_AT(push, _Actor223600AxisStepScratch);
    vec                                                = &head[-1].step;
    gte                                                = &head[-1].step;
    SCRATCH_HEAD_AT(push, _Actor223600AxisStepScratch) = head - 1;

    switch (work->driver.requestedSet) {
        case 0xE:
            work->driver.rate = ANIMATION_RATE_ONE;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) != 2) {
                if (work->stateFrame < 0x32) {
                    if (work->stateFrame >= 0x28) {
                        Actor223600_MoveForward(task->extra.tmd->coords, 0x16);
                    } else if (work->stateFrame >= 0x23) {
                        Actor223600_MoveForward(task->extra.tmd->coords, 0xA);
                    } else if (work->stateFrame >= 0x13) {
                        Actor223600_MoveForward(task->extra.tmd->coords, 0xA);
                    }
                }
            }
            frame = work->stateFrame;
            if (frame >= 0x28) {
                if ((u16)(frame % 5) < 2) {
                    task->extra.tmd->coords->coord.t[1] += work->fallSpeed - (frame - 0x28) / 4;
                } else {
                    task->extra.tmd->coords->coord.t[1] += work->fallSpeed + (frame - 0x28) / 2;
                }
                if (task->extra.tmd->coords->coord.t[1] >= 2) {
                    task->extra.tmd->coords->coord.t[1] = 1;
                }
            }
            if (task->extra.tmd->coords->coord.t[1] > 0) {
                work->driver.requestedSet = 0xF;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                work->stateFrame          = 0;
            }
            break;
        case 0xF:
            work->driver.rate = ANIMATION_RATE_ONE;
            if (work->stateFrame < 0xB) {
                gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(0x23);
                gte_ldsv(gte);
                gte_gpf12();
                gte_stsv(gte);
                task->extra.tmd->coords->coord.t[0] += head[-1].step.vx;
                task->extra.tmd->coords->coord.t[1] += vec->vy;
                task->extra.tmd->coords->coord.t[2] += vec->vz;
            }
            if (work->stateFrame >= 5 && work->stateFrame < 0xE) {
                gfxReadMatrixYAxis(&task->extra.tmd->coords->coord, vec);
                VectorNormalSS(vec, vec);
                gte_lddp(-0x14);
                gte_ldsv(gte);
                gte_gpf12();
                gte_stsv(gte);
                task->extra.tmd->coords->coord.t[0] += head[-1].step.vx;
                task->extra.tmd->coords->coord.t[1] += vec->vy;
                task->extra.tmd->coords->coord.t[2] += vec->vz;
                gfxRotMatrixX(&task->extra.tmd->coords[1].coord,
                              -((work->stateFrame - 4) * 0xCC), GRAPHICS_ROTATION_COMPOSE);
            }
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                if (work->stateFrame >= 0xE && work->stateFrame < 0x25) {
                    gfxReadMatrixYAxis(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    task->extra.tmd->coords->coord.t[0] += head[-1].step.vx;
                    task->extra.tmd->coords->coord.t[1] += vec->vy;
                    task->extra.tmd->coords->coord.t[2] += vec->vz;
                    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    switch ((s16)(work->stateFrame - 0xF)) {
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                        case 5:
                        case 7:
                        case 9:
                            task->extra.tmd->coords->coord.t[0] -= gte->vx;
                            task->extra.tmd->coords->coord.t[1] -= gte->vy;
                            task->extra.tmd->coords->coord.t[2] -= gte->vz;
                            gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, GRAPHICS_ROTATION_COMPOSE);
                            gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                          (work->stateFrame - 0xD) * 0x55 - 0x6E, GRAPHICS_ROTATION_COMPOSE);
                            break;
                        default:
                            task->extra.tmd->coords->coord.t[0] += gte->vx;
                            task->extra.tmd->coords->coord.t[1] += gte->vy;
                            task->extra.tmd->coords->coord.t[2] += gte->vz;
                            gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, GRAPHICS_ROTATION_COMPOSE);
                            gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                          (work->stateFrame - 0xD) * 0x55, GRAPHICS_ROTATION_COMPOSE);
                            break;
                    }
                }
                if (work->stateFrame >= 0x25) {
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord, 0x800, GRAPHICS_ROTATION_COMPOSE);
                }
            } else {
                if (work->stateFrame >= 0xE && work->stateFrame < 0x1E) {
                    gfxReadMatrixYAxis(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(0xB);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    task->extra.tmd->coords->coord.t[0] += head[-1].step.vx;
                    task->extra.tmd->coords->coord.t[1] += vec->vy;
                    task->extra.tmd->coords->coord.t[2] += vec->vz;
                    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, vec);
                    VectorNormalSS(vec, vec);
                    gte_lddp(-0x1D);
                    gte_ldsv(gte);
                    gte_gpf12();
                    gte_stsv(gte);
                    task->extra.tmd->coords->coord.t[0] += head[-1].step.vx;
                    task->extra.tmd->coords->coord.t[1] += vec->vy;
                    task->extra.tmd->coords->coord.t[2] += vec->vz;
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                  (work->stateFrame - 0xD) * 0x78, GRAPHICS_ROTATION_COMPOSE);
                }
                if (work->stateFrame >= 0x1E) {
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -0x800, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord, 0x800, GRAPHICS_ROTATION_COMPOSE);
                }
            }
            break;
        case 2:
            work->driver.rate = 2 * ANIMATION_RATE_ONE;
            break;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    pop                                   = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_AT(pop, _Actor223600AxisStepScratch);
    work->stateFrame++;
}

/// The three state handlers the tick below picks between by the work block's
/// `state` (`ACTOR_223600_STATE_*`), copied onto the stack before the call. The copy is a three-word
/// block move out of the unit's `.rodata`, which is why the table is a rodata
/// object rather than a local initialiser.
static const EnemyTaskFuncTable3 D_actor_223600_80149E4C = {
    {
        func_actor_223600_8014CF3C,
        func_actor_223600_8014B840,
        func_actor_223600_8014BBF4,
    },
};

/// Per-frame tick of this enemy, entry 1 of `D_actor_223600_80149E58`. The
/// game mode word selects a one-shot arm first: mode 0 clears the model's
/// `field_C` when the work block's state is not `ACTOR_223600_STATE_HIDDEN` and then carries on, mode 1
/// does the same and returns, and mode 2 forces `field_C` to 0x80 and returns.
/// The common path records the state change in `stateEntered` and the dispatched
/// state in `prevState`, runs the state handler from `D_actor_223600_80149E4C`,
/// turns the animation latch `func_actor_223600_8014B464` raises into a
/// `sndEvtRequestScriptStart` cue -- the top nibble of the enemy's `placeKey` in
/// bits 8-11, with the model's pan and depth -- and finally relights the model
/// through `worldCoordSetModelLighting` while `relightPending` is set, setting
/// `relightPending` again when the session's `viewReady` or the state handler
/// left the root coordinate dirty.
static void func_actor_223600_8014CA00(Enemy* enemy, Task* task)
{
    _Actor223600Work*   work;
    EnemyTaskFuncTable3 fns;
    s32                 reaction;
    s32                 cue;
    s32                 pan;

    work = task->work;
    fns  = D_actor_223600_80149E4C;

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

    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    fns.funcs[work->state](enemy, task);

    reaction = func_actor_223600_8014B464(work);
    if (reaction != 0) {
        cue = reaction | (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(
            cue, pan,
            (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->relightPending != 0) {
        worldCoordSetModelLighting(task->extra.tmd,
                                   (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);
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
/// `func_actor_223600_8014CF6C` runs by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_223600_80149E58 = {
    {
        func_actor_223600_8014B540,
        func_actor_223600_8014CA00,
        enemyDestroy,
    },
};

/// Message handler (id 0x7D5 in `D_actor_223600_80150B28`). Drives the model's
/// `field_C` flag word and the work block's `state` from `arg2`: 0 sets 0x80
/// and rewrites the buffers, 1 clears it and rewrites the buffers, 2 sets bit
/// 2, and 3 clears then sets bit 2. 0 and 1 select `ACTOR_223600_STATE_WALK`,
/// 2 and 3 `ACTOR_223600_STATE_HIDDEN`.
s32 func_actor_223600_8014CC04(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*        obj  = task->extra.tmd;
    _Actor223600Work* work = task->work;

    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_223600_STATE_WALK;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_223600_STATE_WALK;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_223600_STATE_HIDDEN;
            break;
        case 3:
            obj->flags  = 0;
            work->state = ACTOR_223600_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// `ACTOR_COMMAND_MESSAGE_APPLY` handler in `D_actor_223600_80150B28`. Records
/// the command's stage, area and the low byte of its selector in
/// `lastCommandStage`, `lastCommandArea` and `lastCommand`, then, for a
/// command in the `ACTOR_223600_COMMAND_CONTEXT` namespace, sets the work
/// block's `state` from the `ACTOR_223600_COMMAND_*` selector. Commands of any
/// other namespace are recorded and otherwise ignored.
s32 func_actor_223600_8014CCD4(Task* task, s32 arg1, ActorCommand* command, s32 arg3)
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

/// `ACTOR_223600_STATE_HIDDEN` (entry 0 of `D_actor_223600_80149E4C`). On the
/// frame the state is entered (`stateEntered` set) it marks the enemy not lockable
/// and sets the model's flags to 0x80; it does nothing on later frames.
static void func_actor_223600_8014CF3C(Enemy* arg0, Task* arg1)
{
    _Actor223600Work* work;
    TmdObject*        model;

    work = arg1->work;
    if (work->stateEntered != 0) {
        model                        = arg1->extra.tmd;
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Runs the handler of `D_actor_223600_80149E58` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_223600_8014CF6C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_223600_80149E58;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
