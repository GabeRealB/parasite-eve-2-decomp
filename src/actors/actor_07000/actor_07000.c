#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

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
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
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

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/sucklerceph.h"

/// Work block of the projectile a Slouch spits.
///
/// The projectile's launch state allocates it zeroed and keeps it at
/// `Task::work`; the exit callback unlinks `body` before the task dies. The
/// projectile flies as a capsule whose far end trails one frame's travel
/// behind the model, so the pair and grid passes test the path just covered,
/// and whatever it touches is reported in the single contact.
typedef struct {
    SVECTOR               velocity;    // displacement added to the model's translation each frame, in game-coordinate units; Y gains 10 a frame
    WorldCollisionBody    body;        // capsule body on the model's root coordinate, keyed by the Slouch's second attack row
    WorldCollisionCapsule capsule;     // shape of `body`: radius 150 at both ends, `ends[1]` set to minus `velocity` each frame
    WorldCollisionContact contacts[1]; // contact of `body`; any contact recorded here ends the flight
} _Actor07000SlouchProjectileWork;
STATIC_ASSERT_SIZEOF(_Actor07000SlouchProjectileWork, 0x58);

// Typed callback views for the task message dispatcher.

void ActorsShared801349d8(Task*);

static AnimationSet _gActor07000Actor107000Animation07D1C;
static AnimationSet _gActor07000Actor107000Animation07E64;
static AnimationSet _gActor07000Actor107000Animation08008;
static TmdSource    _gActor07000SucklercephBody;
s32                 Actor07000_Fn05AB8(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
void                Actor07000_Fn05E6C(Task*);
void                Actor07000_Fn06338(Task*);
void                Actor07000_Fn067B4(Task*);

DamageAttack gSucklercephAttack = { 30, 7 };

EnemyParams gSucklercephParams = { &gSucklercephAttack, 70, 6, 12, 3, 100, 20, 100, 0 };

PadScriptCmd Actor07000_D06938[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

PadScriptVibrationSegment Actor07000_D06944[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor07000SucklercephBodySkeleton[3] = {
#include "assets/sucklerceph_body_skeleton.inc"
};

static u32 _gActor07000SucklercephBodyPartVerts[3] = {
#include "assets/sucklerceph_body_partVerts.inc"
};

static SVECTOR _gActor07000SucklercephBodyVerts[68] = {
#include "assets/sucklerceph_body_verts.inc"
};

static SVECTOR _gActor07000SucklercephBodyNormals[68] = {
#include "assets/sucklerceph_body_normals.inc"
};

static u32 _gActor07000SucklercephBodyStream[752] = {
#include "assets/sucklerceph_body_stream.inc"
};

static TmdSource _gActor07000SucklercephBody = {
    0,
    4516,
    584,
    3,
    _gActor07000SucklercephBodyPartVerts,
    _gActor07000SucklercephBodyVerts,
    _gActor07000SucklercephBodyNormals,
    _gActor07000SucklercephBodySkeleton,
    _gActor07000SucklercephBodyStream,
};

static AnimationPackedPose _gActor07000Actor107000Animation07D1CBank1[34] = {
#include "assets/actor_107000_animation_07D1C_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation07D1CBank4[26] = {
#include "assets/actor_107000_animation_07D1C_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation07D1CRecords[74] = {
#include "assets/actor_107000_animation_07D1C_records.inc"
};

static u16 _gActor07000Actor107000Animation07D1CIndices[4] = {
#include "assets/actor_107000_animation_07D1C_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation07D1C = {
    _gActor07000Actor107000Animation07D1CRecords,
    _gActor07000Actor107000Animation07D1CIndices,
    { NULL, _gActor07000Actor107000Animation07D1CBank1, NULL, NULL, _gActor07000Actor107000Animation07D1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation07E64Bank1[11] = {
#include "assets/actor_107000_animation_07E64_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation07E64Bank4[9] = {
#include "assets/actor_107000_animation_07E64_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation07E64Records[28] = {
#include "assets/actor_107000_animation_07E64_records.inc"
};

static u16 _gActor07000Actor107000Animation07E64Indices[4] = {
#include "assets/actor_107000_animation_07E64_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation07E64 = {
    _gActor07000Actor107000Animation07E64Records,
    _gActor07000Actor107000Animation07E64Indices,
    { NULL, _gActor07000Actor107000Animation07E64Bank1, NULL, NULL, _gActor07000Actor107000Animation07E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation08008Bank1[15] = {
#include "assets/actor_107000_animation_08008_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation08008Bank4[12] = {
#include "assets/actor_107000_animation_08008_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation08008Records[36] = {
#include "assets/actor_107000_animation_08008_records.inc"
};

static u16 _gActor07000Actor107000Animation08008Indices[4] = {
#include "assets/actor_107000_animation_08008_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation08008 = {
    _gActor07000Actor107000Animation08008Records,
    _gActor07000Actor107000Animation08008Indices,
    { NULL, _gActor07000Actor107000Animation08008Bank1, NULL, NULL, _gActor07000Actor107000Animation08008Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gSucklercephDropMsgTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, sucklercephMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor07000_D08040 = { { { TASK_BODY_TMD, 96 } }, _sucklercephTask, { .model = &_gActor07000SucklercephBody } };

TaskDesc Actor07000_D0804C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, sucklercephDropTask, { .model = &_gActor07000SucklercephBody } };

AnimationSet* gSucklercephAnimSets[4] = {
    NULL,
    &_gActor07000Actor107000Animation07D1C,
    &_gActor07000Actor107000Animation07E64,
    &_gActor07000Actor107000Animation08008,
};

SVECTOR Actor07000_D08068 = { 0, -10, 0, 0 };

SVECTOR gSucklercephCollapseFxOffset = { 0, -300, 0, 0 };

DamageAttack Actor07000_D08078[2] = {
    { 20, 7 },
    { 12, 3 },
};

EnemyParams Actor07000_D08080 = { Actor07000_D08078, 120, 12, 36, 1, 250, 20, 100, 0 };

static TmdBone _gActor07000Actor107000Model08BB4Skeleton[7] = {
#include "assets/actor_107000_model_08BB4_skeleton.inc"
};

static u32 _gActor07000Actor107000Model08BB4PartVerts[7] = {
#include "assets/actor_107000_model_08BB4_partVerts.inc"
};

static SVECTOR _gActor07000Actor107000Model08BB4Verts[131] = {
#include "assets/actor_107000_model_08BB4_verts.inc"
};

static SVECTOR _gActor07000Actor107000Model08BB4Normals[190] = {
#include "assets/actor_107000_model_08BB4_normals.inc"
};

static u32 _gActor07000Actor107000Model08BB4Stream[1734] = {
#include "assets/actor_107000_model_08BB4_stream.inc"
};

static TmdSource _gActor07000Actor107000Model08BB4 = {
    0,
    9272,
    2448,
    7,
    _gActor07000Actor107000Model08BB4PartVerts,
    _gActor07000Actor107000Model08BB4Verts,
    _gActor07000Actor107000Model08BB4Normals,
    _gActor07000Actor107000Model08BB4Skeleton,
    _gActor07000Actor107000Model08BB4Stream,
};

static TmdBone _gActor07000SlouchBurstLegSkeleton[1] = {
#include "assets/slouch_burst_leg_skeleton.inc"
};

static u32 _gActor07000SlouchBurstLegPartVerts[1] = {
#include "assets/slouch_burst_leg_partVerts.inc"
};

static SVECTOR _gActor07000SlouchBurstLegVerts[17] = {
#include "assets/slouch_burst_leg_verts.inc"
};

static SVECTOR _gActor07000SlouchBurstLegNormals[27] = {
#include "assets/slouch_burst_leg_normals.inc"
};

static u32 _gActor07000SlouchBurstLegStream[179] = {
#include "assets/slouch_burst_leg_stream.inc"
};

static TmdSource _gActor07000SlouchBurstLeg = {
    0,
    1144,
    0,
    1,
    _gActor07000SlouchBurstLegPartVerts,
    _gActor07000SlouchBurstLegVerts,
    _gActor07000SlouchBurstLegNormals,
    _gActor07000SlouchBurstLegSkeleton,
    _gActor07000SlouchBurstLegStream,
};

static TmdBone _gActor07000SlouchBurstArmSkeleton[1] = {
#include "assets/slouch_burst_arm_skeleton.inc"
};

static u32 _gActor07000SlouchBurstArmPartVerts[1] = {
#include "assets/slouch_burst_arm_partVerts.inc"
};

static SVECTOR _gActor07000SlouchBurstArmVerts[27] = {
#include "assets/slouch_burst_arm_verts.inc"
};

static SVECTOR _gActor07000SlouchBurstArmNormals[29] = {
#include "assets/slouch_burst_arm_normals.inc"
};

static u32 _gActor07000SlouchBurstArmStream[274] = {
#include "assets/slouch_burst_arm_stream.inc"
};

static TmdSource _gActor07000SlouchBurstArm = {
    0,
    1804,
    0,
    1,
    _gActor07000SlouchBurstArmPartVerts,
    _gActor07000SlouchBurstArmVerts,
    _gActor07000SlouchBurstArmNormals,
    _gActor07000SlouchBurstArmSkeleton,
    _gActor07000SlouchBurstArmStream,
};

static TmdBone _gActor07000SlouchPoisonSkeleton[1] = {
#include "assets/slouch_poison_skeleton.inc"
};

static u32 _gActor07000SlouchPoisonPartVerts[1] = {
#include "assets/slouch_poison_partVerts.inc"
};

static SVECTOR _gActor07000SlouchPoisonVerts[26] = {
#include "assets/slouch_poison_verts.inc"
};

static SVECTOR _gActor07000SlouchPoisonNormals[28] = {
#include "assets/slouch_poison_normals.inc"
};

static u32 _gActor07000SlouchPoisonStream[232] = {
#include "assets/slouch_poison_stream.inc"
};

static TmdSource _gActor07000SlouchPoison = {
    0,
    1556,
    0,
    1,
    _gActor07000SlouchPoisonPartVerts,
    _gActor07000SlouchPoisonVerts,
    _gActor07000SlouchPoisonNormals,
    _gActor07000SlouchPoisonSkeleton,
    _gActor07000SlouchPoisonStream,
};

static AnimationPackedPose _gActor07000Actor107000Animation0B868Bank1[4] = {
#include "assets/actor_107000_animation_0B868_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0B868Bank4[12] = {
#include "assets/actor_107000_animation_0B868_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0B868Records[41] = {
#include "assets/actor_107000_animation_0B868_records.inc"
};

static u16 _gActor07000Actor107000Animation0B868Indices[8] = {
#include "assets/actor_107000_animation_0B868_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0B868 = {
    _gActor07000Actor107000Animation0B868Records,
    _gActor07000Actor107000Animation0B868Indices,
    { NULL, _gActor07000Actor107000Animation0B868Bank1, NULL, NULL, _gActor07000Actor107000Animation0B868Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0BCF4Bank1[18] = {
#include "assets/actor_107000_animation_0BCF4_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0BCF4Bank4[92] = {
#include "assets/actor_107000_animation_0BCF4_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0BCF4Records[131] = {
#include "assets/actor_107000_animation_0BCF4_records.inc"
};

static u16 _gActor07000Actor107000Animation0BCF4Indices[8] = {
#include "assets/actor_107000_animation_0BCF4_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0BCF4 = {
    _gActor07000Actor107000Animation0BCF4Records,
    _gActor07000Actor107000Animation0BCF4Indices,
    { NULL, _gActor07000Actor107000Animation0BCF4Bank1, NULL, NULL, _gActor07000Actor107000Animation0BCF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0BF18Bank1[6] = {
#include "assets/actor_107000_animation_0BF18_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0BF18Bank4[41] = {
#include "assets/actor_107000_animation_0BF18_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0BF18Records[64] = {
#include "assets/actor_107000_animation_0BF18_records.inc"
};

static u16 _gActor07000Actor107000Animation0BF18Indices[8] = {
#include "assets/actor_107000_animation_0BF18_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0BF18 = {
    _gActor07000Actor107000Animation0BF18Records,
    _gActor07000Actor107000Animation0BF18Indices,
    { NULL, _gActor07000Actor107000Animation0BF18Bank1, NULL, NULL, _gActor07000Actor107000Animation0BF18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0C268Bank1[13] = {
#include "assets/actor_107000_animation_0C268_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0C268Bank4[66] = {
#include "assets/actor_107000_animation_0C268_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0C268Records[93] = {
#include "assets/actor_107000_animation_0C268_records.inc"
};

static u16 _gActor07000Actor107000Animation0C268Indices[8] = {
#include "assets/actor_107000_animation_0C268_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0C268 = {
    _gActor07000Actor107000Animation0C268Records,
    _gActor07000Actor107000Animation0C268Indices,
    { NULL, _gActor07000Actor107000Animation0C268Bank1, NULL, NULL, _gActor07000Actor107000Animation0C268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0C3D4Bank1[5] = {
#include "assets/actor_107000_animation_0C3D4_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0C3D4Bank4[21] = {
#include "assets/actor_107000_animation_0C3D4_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0C3D4Records[41] = {
#include "assets/actor_107000_animation_0C3D4_records.inc"
};

static u16 _gActor07000Actor107000Animation0C3D4Indices[8] = {
#include "assets/actor_107000_animation_0C3D4_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0C3D4 = {
    _gActor07000Actor107000Animation0C3D4Records,
    _gActor07000Actor107000Animation0C3D4Indices,
    { NULL, _gActor07000Actor107000Animation0C3D4Bank1, NULL, NULL, _gActor07000Actor107000Animation0C3D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0CA98Bank1[30] = {
#include "assets/actor_107000_animation_0CA98_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0CA98Bank4[140] = {
#include "assets/actor_107000_animation_0CA98_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0CA98Records[189] = {
#include "assets/actor_107000_animation_0CA98_records.inc"
};

static u16 _gActor07000Actor107000Animation0CA98Indices[8] = {
#include "assets/actor_107000_animation_0CA98_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0CA98 = {
    _gActor07000Actor107000Animation0CA98Records,
    _gActor07000Actor107000Animation0CA98Indices,
    { NULL, _gActor07000Actor107000Animation0CA98Bank1, NULL, NULL, _gActor07000Actor107000Animation0CA98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D270Bank1[32] = {
#include "assets/actor_107000_animation_0D270_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D270Bank4[171] = {
#include "assets/actor_107000_animation_0D270_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D270Records[221] = {
#include "assets/actor_107000_animation_0D270_records.inc"
};

static u16 _gActor07000Actor107000Animation0D270Indices[8] = {
#include "assets/actor_107000_animation_0D270_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D270 = {
    _gActor07000Actor107000Animation0D270Records,
    _gActor07000Actor107000Animation0D270Indices,
    { NULL, _gActor07000Actor107000Animation0D270Bank1, NULL, NULL, _gActor07000Actor107000Animation0D270Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D344Bank1[2] = {
#include "assets/actor_107000_animation_0D344_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D344Bank4[5] = {
#include "assets/actor_107000_animation_0D344_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D344Records[28] = {
#include "assets/actor_107000_animation_0D344_records.inc"
};

static u16 _gActor07000Actor107000Animation0D344Indices[8] = {
#include "assets/actor_107000_animation_0D344_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D344 = {
    _gActor07000Actor107000Animation0D344Records,
    _gActor07000Actor107000Animation0D344Indices,
    { NULL, _gActor07000Actor107000Animation0D344Bank1, NULL, NULL, _gActor07000Actor107000Animation0D344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D458Bank1[4] = {
#include "assets/actor_107000_animation_0D458_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D458Bank4[15] = {
#include "assets/actor_107000_animation_0D458_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D458Records[28] = {
#include "assets/actor_107000_animation_0D458_records.inc"
};

static u16 _gActor07000Actor107000Animation0D458Indices[8] = {
#include "assets/actor_107000_animation_0D458_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D458 = {
    _gActor07000Actor107000Animation0D458Records,
    _gActor07000Actor107000Animation0D458Indices,
    { NULL, _gActor07000Actor107000Animation0D458Bank1, NULL, NULL, _gActor07000Actor107000Animation0D458Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D5A8Bank1[5] = {
#include "assets/actor_107000_animation_0D5A8_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D5A8Bank4[17] = {
#include "assets/actor_107000_animation_0D5A8_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D5A8Records[38] = {
#include "assets/actor_107000_animation_0D5A8_records.inc"
};

static u16 _gActor07000Actor107000Animation0D5A8Indices[8] = {
#include "assets/actor_107000_animation_0D5A8_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D5A8 = {
    _gActor07000Actor107000Animation0D5A8Records,
    _gActor07000Actor107000Animation0D5A8Indices,
    { NULL, _gActor07000Actor107000Animation0D5A8Bank1, NULL, NULL, _gActor07000Actor107000Animation0D5A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D67CBank1[3] = {
#include "assets/actor_107000_animation_0D67C_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D67CBank4[9] = {
#include "assets/actor_107000_animation_0D67C_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D67CRecords[21] = {
#include "assets/actor_107000_animation_0D67C_records.inc"
};

static u16 _gActor07000Actor107000Animation0D67CIndices[8] = {
#include "assets/actor_107000_animation_0D67C_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D67C = {
    _gActor07000Actor107000Animation0D67CRecords,
    _gActor07000Actor107000Animation0D67CIndices,
    { NULL, _gActor07000Actor107000Animation0D67CBank1, NULL, NULL, _gActor07000Actor107000Animation0D67CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor07000Actor107000Animation0D754Bank1[3] = {
#include "assets/actor_107000_animation_0D754_bank1.inc"
};

static AnimationPackedRotation _gActor07000Actor107000Animation0D754Bank4[10] = {
#include "assets/actor_107000_animation_0D754_bank4.inc"
};

static AnimationRecord _gActor07000Actor107000Animation0D754Records[21] = {
#include "assets/actor_107000_animation_0D754_records.inc"
};

static u16 _gActor07000Actor107000Animation0D754Indices[8] = {
#include "assets/actor_107000_animation_0D754_indices.inc"
};

static AnimationSet _gActor07000Actor107000Animation0D754 = {
    _gActor07000Actor107000Animation0D754Records,
    _gActor07000Actor107000Animation0D754Indices,
    { NULL, _gActor07000Actor107000Animation0D754Bank1, NULL, NULL, _gActor07000Actor107000Animation0D754Bank4, NULL, NULL, NULL },
};

AnimationSet* Actor07000_D0D77C[13] = {
    NULL,
    &_gActor07000Actor107000Animation0B868,
    &_gActor07000Actor107000Animation0BCF4,
    &_gActor07000Actor107000Animation0BF18,
    &_gActor07000Actor107000Animation0C268,
    &_gActor07000Actor107000Animation0C3D4,
    &_gActor07000Actor107000Animation0CA98,
    &_gActor07000Actor107000Animation0D270,
    &_gActor07000Actor107000Animation0D344,
    &_gActor07000Actor107000Animation0D458,
    &_gActor07000Actor107000Animation0D5A8,
    &_gActor07000Actor107000Animation0D67C,
    &_gActor07000Actor107000Animation0D754,
};

SVECTOR Actor07000_D0D7B0 = { 0, 0, -120, 0 };

SVECTOR Actor07000_D0D7B8 = { 0, -300, 0, 0 };

TaskMessageEntry Actor07000_D0D7C0[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor07000_Fn05AB8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor07000_D0D7D0[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor07000_Fn05E6C, { .model = &_gActor07000Actor107000Model08BB4 } },
    { { { TASK_BODY_COORD, 96 } }, Actor07000_Fn06338, { .value = 0 } },
};

TaskDesc Actor07000_D0D7E8 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor07000_Fn067B4, { .model = &_gActor07000Actor107000Model08BB4 } };

/// Values of `_Actor07000SlouchWork::state`, the behaviour the per-frame update runs.
enum {
    ACTOR_07000_SLOUCH_STATE_IDLE          = 0, // rests, fidgets and looks around until it notices the player or is touched
    ACTOR_07000_SLOUCH_STATE_ENGAGED       = 1, // runs `_Actor07000SlouchWork::engagedAction`
    ACTOR_07000_SLOUCH_STATE_STATUS_HOLD   = 3, // flinches every 11 frames until the enemy's status buildup runs out, then idle again
    ACTOR_07000_SLOUCH_STATE_PUFFING       = 4, // room command: the same flinching, giving off a puff every 16 frames
    ACTOR_07000_SLOUCH_STATE_PUFFING_DEATH = 5  // the same, dying on the fifth flinch; nothing selects it
};

/// Values of `_Actor07000SlouchWork::engagedAction`.
enum {
    ACTOR_07000_SLOUCH_ENGAGED_WATCH  = 0, // looks around; strikes once the player is in the sense capsule within 5000, else goes back to idle after `idleFrames`
    ACTOR_07000_SLOUCH_ENGAGED_STRIKE = 1, // stretches toward the player with `attackBody` armed, then picks the next action
    ACTOR_07000_SLOUCH_ENGAGED_SPIT   = 2, // launches a projectile on frame 48, then picks the next action
    ACTOR_07000_SLOUCH_ENGAGED_FIDGET = 3, // plays the fidget through, then picks the next action
    ACTOR_07000_SLOUCH_ENGAGED_RECOIL = 4  // thrown back by a heavy hit: launches a projectile on frame 2 and strikes on the way back
};

/// Values of `_Actor07000SlouchWork::deathPhase`.
enum {
    ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN   = 0, // withdraws the target entry and the three bodies; an unburst body turns translucent and takes `deathCoord`
    ACTOR_07000_SLOUCH_DEATH_PHASE_FLATTEN = 1, // 61 frames: an unburst body is squashed along Y, a burst one is hidden
    ACTOR_07000_SLOUCH_DEATH_PHASE_END     = 2  // hides the model and hands the task to its destroy state
};

/// Values of `_Actor07000SlouchWork::animId`: indices into the package's table
/// of the Slouch's animation sets, whose entry 0 is empty. Sets 8, 10 and 11
/// are never requested.
enum {
    ACTOR_07000_SLOUCH_ANIM_IDLE          = 1, // resting loop
    ACTOR_07000_SLOUCH_ANIM_FIDGET        = 2, // 110 frames, with a sound on frames 10 and 105
    ACTOR_07000_SLOUCH_ANIM_SPIT          = 3, // `ENGAGED_SPIT`, 83 frames
    ACTOR_07000_SLOUCH_ANIM_STRIKE        = 4, // `ENGAGED_STRIKE`, 64 frames
    ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH = 5, // restarted every 11 frames by the held states, and played after a tick of damage over time
    ACTOR_07000_SLOUCH_ANIM_FLINCH        = 6, // a hit of 21 to 50 damage; engages after 57 frames
    ACTOR_07000_SLOUCH_ANIM_RECOIL        = 7, // `ENGAGED_RECOIL`, 47 frames
    ACTOR_07000_SLOUCH_ANIM_WATCH         = 9, // looking around; `alert` from frame 18
    ACTOR_07000_SLOUCH_ANIM_DEATH         = 12 // the task's death state
};

/// Work block of a Slouch task, the second enemy of the package.
///
/// Both spawn handlers allocate it zeroed and keep it at `Task::work`. It holds
/// the animation rig of the model's seven parts, the matrices the model is lit
/// through, three collision bodies, each followed by its own contact storage,
/// the coordinate the death flatten splices into the model, and the state
/// machine: `state` picks the behaviour, `engagedAction` the step of the
/// engaged one, and `deathPhase` the step of the task's death state.
///
/// Speeds are game-coordinate units a frame, angles 4096ths of a turn and
/// scales 0x1000 for 1.0.
typedef struct {
    ActorAnimRig7         rig;               // playback of the model's parts; slots 1 to 6 play `animId`, 0 is never started
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    senseBody;         // capsule on the root with no key of its own; a player-body contact is what an alert Slouch engages on
    WorldCollisionCapsule senseCapsule;      // shape of `senseBody`: 3000 along the root's Z, radius 2000 at the root and 4000 at the far end
    WorldCollisionContact senseContacts[1];  // contact of `senseBody`
    WorldCollisionBody    body;              // sphere above the root carrying the key 0x3002A, tested against the room grid, the floor and other bodies
    WorldCollisionContact contacts[4];       // contacts of `body`: wall push-back, the player's touch, hits and other enemies; also the enemy's hit records
    WorldCollisionBody    attackBody;        // sphere on model part 6 carrying the enemy's attack key; enabled only while a strike reaches out
    WorldCollisionContact attackContacts[1]; // contact of `attackBody`; released every frame and never read
    SVECTOR               twist;             // rotation multiplied into model parts 3 and 5; a hit, the drop's first push-back or the landing sets its X angle, which then falls 0x20 a frame
    GfxCoord              deathCoord;        // spliced between the root and model part 1 when an unburst body dies; the flatten scales its Y column
    VECTOR3               prevRootPos;       // root position before the last drop step; restored when the collision step reports a conflict
    byte                  field_348[4];      // never accessed
    SVECTOR               deathScale;        // scale of `deathCoord`, reset to 0x1000 each; only Y is stepped, down 2 a frame
    byte                  field_354[8];      // never accessed
    EffectSpawnArg        hitEffectArg;      // argument record of the effect a hit spawns, hung off model part 1
    s16                   spawnArgHi;        // high half of the hidden spawn's argument, never 1 (that value cancels the spawn) and never read; role unproven
    u16                   spawnArgLo;        // low half of that argument; never read, role unproven
    byte                  field_368[2];      // never accessed
    s16                   state;             // `ACTOR_07000_SLOUCH_STATE_*`
    s16                   deathPhase;        // `ACTOR_07000_SLOUCH_DEATH_PHASE_*`
    s16                   stateFrames;       // frames counted by the current wait: the idle and watch waits, the 11 frames between held flinches, the death flatten
    s16                   animId;            // requested animation, `ACTOR_07000_SLOUCH_ANIM_*`
    s16                   appliedAnim;       // animation last applied to slots 1 to 6; a value other than `animId` applies it again, which is how the held states restart their flinch
    s16                   animFrames;        // frames since `animId` was applied; the actions time their sounds, attacks and ends by it
    byte                  field_376[2];      // never accessed
    s16                   forwardSpeed;      // distance the root moves along its facing each drop step; 200 at the reveal, falling 2 a frame
    s16                   field_37A;         // set to 1 by the first frame that sees another enemy's body in `contacts`, and never cleared; role unproven
    byte                  field_37C[2];      // never accessed
    s16                   field_37E;         // set to 1 together with `field_37A`; role unproven
    s16                   field_380;         // set to 1 by idle frames under the idle and fidget animations and never read; role unproven
    s16                   engagedAction;     // `ACTOR_07000_SLOUCH_ENGAGED_*`
    s16                   stretch;           // amount model part 5 is stretched by, added to 0x1000 along its X column and a quarter of it along the others
    s16                   stretchTarget;     // `stretch` a strike grows toward: 0 within 900 of the player, 0x2000 beyond 2700
    s16                   field_388;         // cleared at spawn and never set; non-zero would send every idle frame to the spit. Role unproven
    s16                   hitCooldown;       // frames before another hit is taken; set from the hit's id parameter 2
    s16                   twistActive;       // 1 while `twist` is turned; never read
    u16                   alert;             // non-zero lets a player contact on `senseBody` engage the idle Slouch; set by a hit and for the length of the watch animation
    u16                   idleFrames;        // length of the idle and watch waits, redrawn from 80..99; the puffing states count the frames between puffs in it
    u16                   watchFrames;       // frames the idle watch animation lasts past its 18th, redrawn from 50..99; the unreached puffing death counts its flinches here
    u16                   hasBurst;          // 1 once a hit has blown a body part off the dying enemy; the death then hides the model instead of flattening it
    u16                   dropArmed;         // 1 once a room command has started the drop into place; cleared when the enemy is hidden again
    s16                   fallSpeed;         // downward speed of the drop into place
    u16                   dropCollided;      // 1 once the drop has been pushed back by the room; the fall then accelerates twice as fast
} _Actor07000SlouchWork;
STATIC_ASSERT_SIZEOF(_Actor07000SlouchWork, 0x39C);

/// Node 3's attack row, packed by `damagePackAttackKey` into `SucklercephWork::attackBody`, and the enemy
/// parameters whose `attacks` point at it; its `hpMax` seeds the enemy's
/// `field_40`.
extern DamageAttack gSucklercephAttack;

extern EnemyParams gSucklercephParams;

extern PadScriptCmd Actor07000_D06938[];

extern PadScriptVibrationSegment Actor07000_D06944[];

extern TaskMessageEntry gSucklercephDropMsgTable[2];

/// Animation-set table bound to the caged specimen's context by `animationInitContext`.
extern AnimationSet* gSucklercephAnimSets[4];

extern SVECTOR Actor07000_D08068;

/// Offset the collapse arms spawn the 0x60080 effect at.
extern SVECTOR gSucklercephCollapseFxOffset;

/// Pair table the second form's node 3 and the projectile's collision body pack
/// into their keys.
extern DamageAttack Actor07000_D08078[2];

/// Enemy parameters of the second form; its `hpMax` seeds the enemy's HP.
extern EnemyParams Actor07000_D08080;

/// Models effect 0x80005 spawns, set in `D_800626EC[5].data.model`, one per
/// random variant the roll selects.
static TmdSource _gActor07000SlouchBurstLeg;

static TmdSource _gActor07000SlouchBurstArm;

static TmdSource _gActor07000SlouchPoison;

/// Animation-set table bound to the second form's context by `animationInitContext`.
extern AnimationSet* Actor07000_D0D77C[13];

extern SVECTOR Actor07000_D0D7B0;

/// Offset the second form's collapse arms spawn the 0x60080 effect at,
/// `{ 0, -0x12C, 0 }`.
extern SVECTOR Actor07000_D0D7B8;

/// Message dispatch table the second form's spawn parks in `Task::msgTable`.
extern TaskMessageEntry Actor07000_D0D7C0[2];

extern TaskDesc Actor07000_D0D7D0[];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void Actor07000_Fn02E0C(Enemy* arg0, Task* arg1);

static void Actor07000_Fn03164(Enemy* arg0, Task* arg1);

static void Actor07000_Fn03460(Task* arg0, TmdObject* arg1, s32 arg2);

static void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2);

static void Actor07000_Fn03E08(Task* arg0);

static void Actor07000_Fn04274(Task* arg0, s32 arg1);

static void Actor07000_Fn04468(Enemy* arg0, Task* arg1);

/// Picks the reaction branch the specimen takes on this hit and stores it in
/// `engagedAction`, then hands back the collision record the caller armed. A
/// countdown of 0xBB8 or more, or a record with no slot matching the 0x10000
/// kind, clears the branch and `stateFrames` instead. Otherwise the branch is 3
/// when the first `gRandomLcgState` draw folds to under 11, 2 when the target is
/// 2500 units or further. Closer than that, a second draw is taken: it lands on
/// 1 when that draw folds to 11 or more, and the branch stays 2 when it does
/// not. Either way `animFrames`/`appliedAnim` are reset, and the record is released.
static void Actor07000_Fn046B8(Task* arg0, s32 arg1);

static s32 Actor07000_Fn047F4(GfxCoord* arg0, u32* arg1);

static void Actor07000_Fn049C0(Task* arg0);

static void Actor07000_Fn04B18(Task* arg0);

static void Actor07000_Fn04E60(Task* arg0);

static void Actor07000_Fn05068(Enemy* arg0, Task* arg1);

static void Actor07000_Fn05400(Enemy* arg0, Task* arg1);

static void Actor07000_Fn0595C(Task* arg0);

static void Actor07000_Fn05ED4(Task* arg0);

static void Actor07000_Fn05F84(Task* task);

static void Actor07000_Fn05FF8(Task* arg0);

static void Actor07000_Fn06088(Task* arg0);

static void Actor07000_Fn060FC(Task* arg0);

static void Actor07000_Fn062A8(Task* arg0);

static void Actor07000_Fn06390(Task* arg0);

static void Actor07000_Fn0662C(Task* arg0);

static void Actor07000_Fn066FC(Task* dst, Task* src);

static void Actor07000_Fn06750(Task* task);

static void Actor07000_Fn06820(Task* arg0);

static void Actor07000_Fn068B4(Task* arg0);

static void Actor07000_Fn068F0(Task* arg0);

static inline s32      _actor07000ClampToZero(s32 value);
static __inline__ void update_animation(Task* task);
static __inline__ void update_color(Enemy* enemy, GfxCoord* coord);
static __inline__ void rotate_parts(Task* arg0);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Message dispatch table the caged specimen's spawn parks in `Task::msgTable`.

#include "../../shared/sucklerceph_spawn_state.inc.c"

/// Task states of the caged specimen as `_sucklercephTask` dispatches
/// them: spawn, per-frame update and teardown.
static const EnemyTaskFuncTable3 gSucklercephTaskStates = {
    { _sucklercephSpawnState, sucklercephUpdateState, sucklercephDeathState },
};

/// Task states of the caged specimen as `sucklercephDropTask` dispatches them:
/// the same update and teardown after a spawn that parks the specimen hidden,
/// and a fourth state for its drop into place.
static const EnemyTaskFuncTable4 gSucklercephDropTaskStates = {
    { sucklercephDropSpawnState, sucklercephUpdateState, sucklercephDeathState, sucklercephDropState },
};

#include "../../shared/sucklerceph_reaction_dispatch.inc.c"

#include "../../shared/sucklerceph_inlines.inc.c"

#include "../../shared/sucklerceph_dormant_tick.inc.c"

#include "../../shared/sucklerceph_awake_tick.inc.c"

/// `value`, or 0 where it is not positive.
static inline s32 _actor07000ClampToZero(s32 value)
{
    if (value <= 0) {
        value = 0;
    }
    return value;
}

#include "../../shared/sucklerceph_contacts.inc.c"

#include "../../shared/sucklerceph_take_damage.inc.c"

#include "../../shared/sucklerceph_turn_to_player.inc.c"

#include "../../shared/sucklerceph_death_state.inc.c"

void sucklercephKill(Task* arg0, u8 arg1)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              soundId;

    obj             = arg0->extra.tmd;
    enemy           = arg0->spawnArg2.pointer;
    work            = arg0->work;
    coord           = obj->coords;
    enemy->hp       = 0;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if (((gRandomLcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->variant != 0) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000B;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0003;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->blastBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        effectSpawn(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 1, NULL);
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords, 0x300, &Actor07000_D08068);
        padScriptSpawn(Actor07000_D06938, Actor07000_D06944);
        work->hasBurst = 1;
    } else {
        if (work->variant != 0) {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000C;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0004;
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->state = SUCKLERCEPH_STATE_SLUMP_DEATH;
    }
}

#include "../../shared/sucklerceph_drop_spawn_state.inc.c"

/// Task states of the specimen's second form as `Actor07000_Fn05E6C`
/// dispatches them: spawn, per-frame update, death and destruction.
static const EnemyTaskFuncTable4 Actor07000_D0003C = {
    { Actor07000_Fn02E0C, Actor07000_Fn03164, Actor07000_Fn04468, enemyDestroy },
};

/// Task states of the specimen's second form as `Actor07000_Fn067B4`
/// dispatches them: the same update, death and destruction after a spawn that
/// parks the model hidden, and a fifth state for its drop into place.
static const EnemyTaskFuncTable5 Actor07000_D0004C = {
    { Actor07000_Fn05068, Actor07000_Fn03164, Actor07000_Fn04468, enemyDestroy, Actor07000_Fn05400 },
};

#include "../../shared/sucklerceph_drop_state.inc.c"

#include "../../shared/sucklerceph_drop_collide.inc.c"

#include "../../shared/sucklerceph_message.inc.c"

#include "../../shared/sucklerceph_task.inc.c"

#include "../../shared/sucklerceph_update_state.inc.c"

#include "../../shared/sucklerceph_reaction_flags.inc.c"

#include "../../shared/sucklerceph_step.inc.c"

#include "../../shared/sucklerceph_animate.inc.c"

#include "../../shared/sucklerceph_colour.inc.c"

#include "../../shared/sucklerceph_draw_shadow.inc.c"

#include "../../shared/sucklerceph_scale_part.inc.c"

#include "../../shared/sucklerceph_flatten.inc.c"

#include "../../shared/sucklerceph_exit.inc.c"

#include "../../shared/sucklerceph_drop_task.inc.c"

#include "../../shared/sucklerceph_fall_step.inc.c"

/// Spawn handler of the specimen's second form, entry 0 of
/// `Actor07000_D0003C`: allocates the 0x39C-byte `_Actor07000SlouchWork`,
/// rebinds the model's light and colour matrices into it, links the enemy
/// node and the three render nodes with their collision tables, seeds the six
/// helper animation slots, draws the two `gRandomLcgState` timers `idleFrames`/
/// `watchFrames`, installs `Actor07000_Fn06750` as the exit callback and moves
/// the task on to its per-frame state.
static void Actor07000_Fn02E0C(Enemy* arg0, Task* arg1)
{
    _Actor07000SlouchWork* work;
    WorldCollisionContact* table;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              coord6;
    u32                    rng;
    u32                    rng2;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor07000SlouchWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    coord6              = &coord[6];
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->bodyPos.vy              = -0x64;
    arg0->coord                   = coord;
    arg0->node.state.parts.flags  = 0;
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &Actor07000_D08080;
    arg0->hp                      = Actor07000_D08080.hpMax;
    arg0->recs                    = work->contacts;
    work->hitEffectArg.coord      = &arg1->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = 0x280;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, Actor07000_D0D77C, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    (sceneAcquireBattleRef)(0);
    work->animId                    = ACTOR_07000_SLOUCH_ANIM_IDLE;
    work->appliedAnim               = ACTOR_07000_SLOUCH_ANIM_IDLE;
    work->field_388                 = 0;
    work->stretch                   = 0;
    work->stretchTarget             = 0;
    work->alert                     = 0;
    work->hitCooldown               = 0;
    work->senseCapsule.ends[0].vz   = 0xBB8;
    work->senseCapsule.end0Radius   = 0xFA0;
    work->senseCapsule.end1Radius   = 0x7D0;
    table                           = work->senseContacts;
    work->senseCapsule.contacts     = table;
    work->senseBody.context.capsule = &work->senseCapsule;
    work->senseBody.coord           = coord;
    work->senseBody.pos.vx          = 0;
    work->senseBody.pos.vy          = 0;
    work->senseBody.pos.vz          = 0;
    work->senseBody.key             = 0;
    work->senseBody.radius          = 0;
    work->senseBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(table, ARRAY_SIZE(work->senseContacts), 0);
    work->body.coord            = coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0x15E;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3002A;
    work->body.radius           = 0x15E;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags                 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.coord            = coord6;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = -0x154;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->attackBody.key              = damagePackAttackKey(Actor07000_D08078, 0);
    work->attackBody.radius           = 0x1F4;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->hasBurst          = 0;
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    rng                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState         = rng;
    work->idleFrames        = (u16)((rng >> 16) % 20U + 0x50);
    rng2                    = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState         = rng2;
    work->watchFrames       = (u16)((rng2 >> 16) % 50U + 0x32);
    arg1->exitCallback      = Actor07000_Fn06750;
    arg1->state            += 1;
}

/// Per-frame mode handler of the specimen. The `gSceneCombatState.actorControl` switch is the same
/// one `sucklercephUpdateState` runs: mode 1 skips to the tail, mode 2 puts
/// the model in its hidden pose and returns, mode 0 clears both flags and falls
/// into the body. The body first dispatches the behaviour `state` - idle and
/// engaged hand the frame to their own handler, the status hold counts
/// `stateFrames` out to 0xB before restarting the flinch animation and drops
/// back to idle once the enemy's buildup runs out, puffing does the count
/// alone, and the puffing death counts those flinches in `watchFrames`, the
/// fifth of which re-cues the impact sound, switches the task to its death
/// state and disables the attack body. The two puffing states share the
/// `idleFrames` count that spawns a 0x60080 effect every 0x10 frames. The
/// tail then twists the model's second
/// coordinate part, runs the five per-frame helpers, and clears the display
/// flags of the model's first two parts before recomputing the second.
static void Actor07000_Fn03164(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    one;
    s32                    soundId;

    obj   = arg1->extra.tmd;
    state = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    one   = 1;
    switch (state) {
        case 0:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case 2:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = one;
            return;
        case 1:
            Actor07000_Fn05F84(arg1);
            return;
        default:
            break;
    }
    switch (work->state) {
        case ACTOR_07000_SLOUCH_STATE_IDLE:
            Actor07000_Fn03460(arg1, obj, one);
            break;
        case ACTOR_07000_SLOUCH_STATE_ENGAGED:
            Actor07000_Fn037EC(arg1, obj, one);
            break;
        case ACTOR_07000_SLOUCH_STATE_STATUS_HOLD:
            work->stateFrames += 1;
            if (work->stateFrames >= 0xB) {
                work->appliedAnim = ACTOR_07000_SLOUCH_ANIM_FIDGET;
                work->animId      = ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH;
                work->animFrames  = 0;
                work->stateFrames = 0;
            }
            if (damageTickEnemyBuildup(arg1->spawnArg2.pointer) != 0) {
                work->state = ACTOR_07000_SLOUCH_STATE_IDLE;
            }
            break;
        case ACTOR_07000_SLOUCH_STATE_PUFFING:
            work->stateFrames += 1;
            if (work->stateFrames >= 0xB) {
                work->appliedAnim = ACTOR_07000_SLOUCH_ANIM_FIDGET;
                work->animId      = ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH;
                work->animFrames  = 0;
                work->stateFrames = 0;
            }
            work->idleFrames += 1;
            if (work->idleFrames >= 0x10) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, arg1->extra.tmd->coords, 0x400, &Actor07000_D0D7B8);
                work->idleFrames = 0;
            }
            break;
        case ACTOR_07000_SLOUCH_STATE_PUFFING_DEATH:
            work->stateFrames += 1;
            if (work->stateFrames >= 0xB) {
                work->appliedAnim  = ACTOR_07000_SLOUCH_ANIM_FIDGET;
                work->animId       = ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH;
                work->animFrames   = 0;
                work->stateFrames  = 0;
                work->watchFrames += 1;
                if (work->watchFrames >= 5) {
                    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), SOUND_SCRIPT_STOP_NO_FADE);
                    soundId = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460005;
                    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    arg1->state             = 2;
                    work->deathPhase        = ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN;
                }
            }
            work->idleFrames += 1;
            if (work->idleFrames >= 0x10) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, arg1->extra.tmd->coords, 0x400, &Actor07000_D0D7B8);
                work->idleFrames = 0;
            }
            break;
    }
    coord->coord.t[1] += 0x80;
    Actor07000_Fn0662C(arg1);
    Actor07000_Fn03E08(arg1);
    Actor07000_Fn05ED4(arg1);
    Actor07000_Fn060FC(arg1);
    Actor07000_Fn06390(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
    Actor07000_Fn05F84(arg1);
}

static void Actor07000_Fn03460(Task* arg0, TmdObject* arg1, s32 arg2)
{
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;
    s32                    soundId;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->animId) {
        case ACTOR_07000_SLOUCH_ANIM_IDLE:
            work->stateFrames++;
            if (work->stateFrames > work->idleFrames) {
                work->stateFrames = 0;
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->idleFrames  = (gRandomLcgState >> 16) % 20 + 80;
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 100) < 31U) {
                    work->animId = ACTOR_07000_SLOUCH_ANIM_FIDGET;
                } else {
                    work->animId = ACTOR_07000_SLOUCH_ANIM_WATCH;
                }
            }
            work->field_380    = 1;
            work->forwardSpeed = 0;
            break;
        case ACTOR_07000_SLOUCH_ANIM_FIDGET:
            work->field_380    = 1;
            work->forwardSpeed = 0;
            if (work->animFrames == 10) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460003;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames == 105) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460006;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames >= 110) {
                work->animId = ACTOR_07000_SLOUCH_ANIM_IDLE;
            }
            break;
        case ACTOR_07000_SLOUCH_ANIM_WATCH:
            work->forwardSpeed = 0;
            if (work->animFrames >= 18) {
                work->alert = 1;
            }
            if (work->animFrames >= work->watchFrames + 18) {
                work->alert       = 0;
                work->animId      = ACTOR_07000_SLOUCH_ANIM_IDLE;
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->watchFrames = (gRandomLcgState >> 16) % 50 + 50;
            }
            break;
        case ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH:
            if (work->animFrames < 22) {
                return;
            }
            work->animId = ACTOR_07000_SLOUCH_ANIM_FIDGET;
            break;
        case ACTOR_07000_SLOUCH_ANIM_FLINCH:
            if (work->animFrames >= 57) {
                work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
                work->stateFrames   = 0;
                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_WATCH;
            }
            break;
    }
    if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0 && work->alert != 0) {
        work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
        work->stateFrames   = 0;
        work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_WATCH;
    }
    if (worldCollisionCountContactsByKind(work->contacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0 || work->field_388 != 0) {
        work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
        work->stateFrames   = 0;
        work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
    }
    worldCollisionClearContacts(work->senseContacts);
}

static void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2)
{
    u32                    distance;
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;
    Enemy*                 enemy;
    s32                    soundId;
    s16                    amount;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    switch (work->engagedAction) {
        case ACTOR_07000_SLOUCH_ENGAGED_WATCH:
            work->animId = ACTOR_07000_SLOUCH_ANIM_WATCH;
            if (work->animFrames >= 18) {
                sceneSetEnemyAlert(1);
                sceneEngageBattle(1);
            }
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &distance);
            if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) == 0 || distance >= 5000U) {
                work->stateFrames++;
                if (work->stateFrames > work->idleFrames) {
                    work->stateFrames = 0;
                    work->state       = ACTOR_07000_SLOUCH_STATE_IDLE;
                    work->animId      = ACTOR_07000_SLOUCH_ANIM_FIDGET;
                    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->idleFrames  = (gRandomLcgState >> 16) % 20 + 80;
                }
            } else {
                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_STRIKE;
            }
            worldCollisionClearContacts(work->senseContacts);
            break;
        case ACTOR_07000_SLOUCH_ENGAGED_STRIKE:
            sceneEngageBattle(1);
            work->animId       = ACTOR_07000_SLOUCH_ANIM_STRIKE;
            work->forwardSpeed = 0;
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &distance);
            if (distance < 900U) {
                work->stretchTarget = 0;
            } else if (distance > 2700U) {
                work->stretchTarget = 0x2000;
            } else {
                work->stretchTarget = ((distance - 900) << 9) / 100;
            }
            if (work->animFrames == 40) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460001;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames >= 32 && work->animFrames < 42) {
                amount = work->stretchTarget;
                if (work->stretch < amount) {
                    work->stretch += amount >> 3;
                }
            } else if (work->animFrames >= 42) {
                if (work->stretch >= 0x200) {
                    work->stretch -= 0x250;
                } else {
                    work->stretch = 0;
                }
            }
            if (work->animFrames >= 32 && work->animFrames < 43) {
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrames >= 64) {
                Actor07000_Fn046B8(arg0, 1);
            }
            break;
        case ACTOR_07000_SLOUCH_ENGAGED_SPIT:
            sceneEngageBattle(1);
            work->animId = ACTOR_07000_SLOUCH_ANIM_SPIT;
            if (work->animFrames == 48) {
                Actor07000_Fn062A8(arg0);
                if (enemy->hp <= 0) {
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    arg0->state             = 2;
                    work->deathPhase        = ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN;
                    return;
                }
            }
            if (work->field_388 != 0 && work->animFrames == 50) {
                Actor07000_Fn046B8(arg0, 0);
            }
            if (work->animFrames >= 83) {
                Actor07000_Fn046B8(arg0, 0);
            }
            if (work->stretch >= 0x200) {
                work->stretch -= 0x250;
            } else {
                work->stretch = 0;
            }
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_07000_SLOUCH_ENGAGED_FIDGET:
            work->animId = ACTOR_07000_SLOUCH_ANIM_FIDGET;
            if (work->animFrames == 105) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460006;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames >= 110) {
                Actor07000_Fn046B8(arg0, 0);
            }
            break;
        case ACTOR_07000_SLOUCH_ENGAGED_RECOIL:
            work->animId = ACTOR_07000_SLOUCH_ANIM_RECOIL;
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &distance);
            if (distance < 900U) {
                work->stretchTarget = 0;
            } else if (distance > 2700U) {
                work->stretchTarget = 0x2000;
            } else {
                work->stretchTarget = ((distance - 900) << 9) / 100;
            }
            if (work->animFrames == 30) {
                soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460001;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrames >= 25 && work->animFrames < 35) {
                amount = work->stretchTarget;
                if (work->stretch < amount) {
                    work->stretch += amount >> 3;
                }
            } else if (work->animFrames >= 35) {
                if (work->stretch >= 0x200) {
                    work->stretch -= 0x250;
                } else {
                    work->stretch = 0;
                }
            } else {
                if (work->stretch >= 0x200) {
                    work->stretch -= 0x250;
                } else {
                    work->stretch = 0;
                }
            }
            if (work->animFrames >= 25 && work->animFrames < 36) {
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            } else {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrames == 2) {
                Actor07000_Fn062A8(arg0);
                if (enemy->hp <= 0) {
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    arg0->state             = 2;
                    work->deathPhase        = ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN;
                    return;
                }
            }
            if (work->animFrames >= 47) {
                Actor07000_Fn046B8(arg0, 0);
            }
            break;
    }
}

static void Actor07000_Fn03E08(Task* arg0)
{
    s32                       movement;
    s32                       dx;
    s32                       dy;
    s32                       dz;
    s32                       reaction;
    s32                       cooldown;
    u32                       random;
    u32                       kind;
    u32                       damage;
    s32                       i;
    _Actor07000SlouchWork*    work;
    GfxCoord*                 coord;
    Enemy*                    enemy;
    void*                     head;
    ActorContactDeltaScratch* scratch;

    work     = arg0->work;
    head     = SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorContactDeltaScratch));
    coord    = arg0->extra.tmd->coords;
    enemy    = arg0->spawnArg2.pointer;
    scratch  = head;
    movement = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0]  = (s32)(coord->coord.t[0] + scratch->delta.fixed.vx.halves.integer);
            coord->coord.t[1]  = (s32)(coord->coord.t[1] + scratch->delta.fixed.vy.halves.integer);
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->contacts); i++) {
        kind = work->contacts[i].key.value & 0xFFFF0000;
        switch (kind) {
            case 0x20000:
                if (work->hitCooldown == 0) {
                    dx                       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                    scratch->delta.vector.vx = dx;
                    dy                       = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                    scratch->delta.vector.vy = dy;
                    dz                       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                    scratch->delta.vector.vz = dz;
                    damage                   = damageComputePlayerAttack(work->contacts[i].key.value, SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
                    if (damageRollCriticalHit(arg0->spawnArg2.pointer, work->contacts[i].key.value, 0) != 0) {
                        effectSpawn(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords, 0, 0);
                        damage *= 4;
                    }
                    damageAccumulateLifeDrainHp(enemy, work->contacts[i].key.value, (s32)damage, 0);
                    Actor07000_Fn04274(arg0, (s32)damage);
                    reaction = damageGetPlayerAttackReaction(work->contacts[i].key.value) & 0xFFFF;
                    switch (reaction) {
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                            if (work->state <= ACTOR_07000_SLOUCH_STATE_ENGAGED) {
                                work->animFrames    = 0;
                                work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
                                work->stateFrames   = 0;
                                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_RECOIL;
                            }
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(enemy, work->contacts[i].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                        case 8:
                        case 9:
                            damageStartEnemyBuildup(enemy, work->contacts[i].key.value, 0);
                            break;
                        case 4:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (enemy->hp < 0) {
                                Actor07000_Fn049C0(arg0);
                                work->hasBurst = 1;
                            }
                            break;
                    }
                    work->alert = 1;
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[i].key.value), (arg0->extra.tmd->coords + 1), &Actor07000_D0D7B0, &work->hitEffectArg);
                    cooldown = damageGetPlayerAttackHitCooldown(work->contacts[i].key.value);
                    if ((cooldown << 0x10) > 0) {
                        work->hitCooldown = cooldown;
                    }
                    work->twistActive = 1;
                    random            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState   = random;
                    work->twist.vx    = ((random >> 0xB) & 0x60) + 0x100;
                }
                break;
            case 0x10000:
                if (work->state == ACTOR_07000_SLOUCH_STATE_IDLE) {
                    work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
                    work->stateFrames   = 0;
                    work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
                }
                break;
            case 0x30000:
                if (work->field_37E == 0) {
                    if ((worldCollisionCountContactsByKind(work->contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) && (work->field_37A == 0)) {
                        work->forwardSpeed = 0;
                        work->field_37A    = 1;
                        work->animId       = ACTOR_07000_SLOUCH_ANIM_IDLE;
                        work->field_37E    = 1;
                    }
                } else if (work->field_37A == 0) {
                    work->field_37E--;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->contacts);
    worldCollisionClearContacts(work->attackContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaScratch);
}

/// Hit reaction of the specimen. `arg1` comes off the context's HP countdown
/// and is pushed through the lock-slot updater by the same amount. A spent
/// countdown switches the task to its death state (2), disables the attack
/// body and resets the death phase; a live one cues the impact sound
/// - bits 12+ of the context's `field_8` pick the sound bank - and then picks
/// the reaction through `state`.
///
/// `state` is only changed while it is idle or engaged: `arg1` at or above
/// 0x33 lands on the engaged recoil and 0x15 or above on the idle flinch
/// animation. Below both, an idle Slouch or an engaged one that is only
/// watching re-measures the coordinate with `Actor07000_Fn047F4` and engages
/// with the spit once the target is 2500 units away, with the strike
/// otherwise.
static void Actor07000_Fn04274(Task* arg0, s32 arg1)
{
    u32                    sp10;
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;
    TmdObject*             obj;
    GfxCoord*              coord;
    s32                    soundId;
    s16                    state;

    enemy      = arg0->spawnArg2.pointer;
    obj        = arg0->extra.tmd;
    coord      = obj->coords;
    work       = arg0->work;
    enemy->hp -= arg1;
    worldTargetAddReadoutAmount(&enemy->node, arg1, 0);
    if (enemy->hp <= 0) {
        sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), SOUND_SCRIPT_STOP_NO_FADE);
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460005;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        arg0->state             = 2;
        work->deathPhase        = ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN;
        return;
    }
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), SOUND_SCRIPT_STOP_NO_FADE);
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460004;
    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    state = work->state;
    if (state <= ACTOR_07000_SLOUCH_STATE_ENGAGED) {
        if ((u32)arg1 >= 0x33) {
            work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
            work->animFrames    = 0;
            work->stateFrames   = 0;
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_RECOIL;
            return;
        }
        if ((u32)arg1 >= 0x15) {
            work->animFrames  = 0;
            work->state       = ACTOR_07000_SLOUCH_STATE_IDLE;
            work->stateFrames = 0;
            work->animId      = ACTOR_07000_SLOUCH_ANIM_FLINCH;
            return;
        }
        if (state == ACTOR_07000_SLOUCH_STATE_IDLE || work->engagedAction == ACTOR_07000_SLOUCH_ENGAGED_WATCH) {
            Actor07000_Fn047F4(arg0->extra.tmd->coords, &sp10);
            work->state       = ACTOR_07000_SLOUCH_STATE_ENGAGED;
            work->stateFrames = 0;
            if (sp10 >= 0x9C4) {
                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
                return;
            }
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_STRIKE;
        }
    }
}

/// Task states of a specimen projectile as `Actor07000_Fn06338` dispatches
/// them: launch, flight and the countdown after impact.
static const TaskFuncTable3 Actor07000_D000E0 = {
    { Actor07000_Fn04B18, Actor07000_Fn04E60, Actor07000_Fn068B4 },
};

static __inline__ void update_animation(Task* task)
{
    _Actor07000SlouchWork* work;
    s32                    i;
    work = task->work;
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++)
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, 8);
    } else {
        work->animFrames++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++)
            animationTickSlot(&work->rig.anim, i);
    }
}

/// Death handler of the specimen's second form, entry 2 of
/// `Actor07000_D0003C`. `gSceneCombatState.actorControl` mode 1 does nothing and mode 2 hides the
/// model. Otherwise `deathPhase` steps the death: phase 0 (unless the body
/// `hasBurst`) cues the death sound, sets the model's flag
/// word to 2 and splices a scaling coordinate into the model through
/// `Actor07000_Fn05FF8`, then unlinks the enemy node and the three
/// render nodes, releases the global state and switches the animation to 0xC;
/// phase 1 flattens that coordinate through `Actor07000_Fn06088` for 0x3D
/// frames; phase 2 cues the closing sound,
/// hides the model, re-parents its second coordinate onto the root and moves
/// the task to state 3. The six helper animation slots are then rebound or
/// ticked with the lighting mode set to 1.
static void Actor07000_Fn04468(Enemy* arg0, Task* arg1)
{
    _Actor07000SlouchWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    part  = &coord[1];
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->deathPhase) {
                case ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN:
                    if (work->hasBurst == 0) {
                        sndEvtRequestScriptStart(SOUND_COMMON(0x0D), 0, 0);
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                        Actor07000_Fn05FF8(arg1);
                    }
                    arg0->recs = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    worldCollisionUnlinkBody(&work->senseBody);
                    worldCollisionUnlinkBody(&work->body);
                    worldCollisionUnlinkBody(&work->attackBody);
                    sceneReleaseBattleRefWithRewards(arg1, 0x2A);
                    work->animId      = ACTOR_07000_SLOUCH_ANIM_DEATH;
                    work->stateFrames = 0;
                    work->deathPhase  = ACTOR_07000_SLOUCH_DEATH_PHASE_FLATTEN;
                    break;
                case ACTOR_07000_SLOUCH_DEATH_PHASE_FLATTEN:
                    if (work->hasBurst == 0) {
                        Actor07000_Fn06088(arg1);
                    } else {
                        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->stateFrames++;
                    if (work->stateFrames >= 0x3D) {
                        work->deathPhase = ACTOR_07000_SLOUCH_DEATH_PHASE_END;
                    }
                    break;
                case ACTOR_07000_SLOUCH_DEATH_PHASE_END:
                    sndEvtRequestScriptStop(SOUND_COMMON(0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    obj->flags   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    part->parent = coord;
                    arg1->state  = 3;
                    break;
            }
            worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
            update_animation(arg1);
            break;
    }
}

/// Picks the reaction branch in `engagedAction` from the collision record at
/// `senseContacts` and the distance to the model in pointer slot 3. With no occupant
/// of kind 0x10000, or at 3000 or more, the branch is cleared. Otherwise a
/// `gRandomLcgState` draw under 11 of 100 selects branch 3; failing that, 2500 or
/// more selects 2, and closer in a second draw picks 2 on the same odds or 1.
/// A chosen branch restarts the animation bookkeeping. The record is released
/// either way.
static void Actor07000_Fn046B8(Task* arg0, s32 arg1)
{
    u32                    dist;
    _Actor07000SlouchWork* work;
    u32                    rng;

    work = arg0->work;
    Actor07000_Fn047F4(arg0->extra.tmd->coords, &dist);
    if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) == 0 || dist >= 3000) {
        work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_WATCH;
        work->stateFrames   = 0;
    } else {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        if ((u16)((rng >> 16) % 100) < 11) {
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_FIDGET;
        } else if (dist >= 2500) {
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 100) < 11) {
                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
            } else {
                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_STRIKE;
            }
        }
        work->animFrames  = 0;
        work->appliedAnim = 0;
    }
    worldCollisionClearContacts(work->senseContacts);
}

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// its bearing in `arg0`'s own frame, folded into -0x800..0x800, and stores
/// in `*arg1` the planar x/z distance between the two coordinates' local
/// translations. The work is staged in a block of the scratch stack.
static s32 Actor07000_Fn047F4(GfxCoord* arg0, u32* arg1)
{
    GfxCoord*            other;
    ActorBearingScratch* blk;
    s32                  angle;

    other         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    blk           = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    angle         = _actorAngleBearingInFrame(blk, arg0, other);
    blk->delta.vx = other->coord.t[0] - arg0->coord.t[0];
    blk->delta.vz = other->coord.t[2] - arg0->coord.t[2];
    *arg1         = SquareRoot0(blk->delta.vx * blk->delta.vx + blk->delta.vz * blk->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return angle;
}

/// Ground-burst tick of the specimen. The `gRandomLcgState` draw is folded to a
/// variant and each of the three effect-setup records, with its own part of the
/// model, is spawned as effect 0x80005: variant 2 uses part 5, variant 3 part 4,
/// and variants 0 and 1 share part 1. The spawned effect is re-coloured from the
/// specimen's own palette before the tick ends by arming effect 0x60030 on parts
/// 1 and 4.
static void Actor07000_Fn049C0(Task* arg0)
{
    EffectWork* effect;
    s32         r;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    r               = (gRandomLcgState >> 16) & 3;
    switch (r) {
        case 0:
        case 1:
            D_800626EC[5].data.model = &_gActor07000SlouchPoison;
            effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 1, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
        case 2:
            D_800626EC[5].data.model = &_gActor07000SlouchBurstArm;
            effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 5, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
        case 3:
            D_800626EC[5].data.model = &_gActor07000SlouchBurstLeg;
            effect                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, arg0->extra.tmd->coords + 4, 0, NULL);
            if (effect != NULL) {
                Actor07000_Fn066FC(effect->task, arg0);
            }
            break;
    }
    effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x300, NULL);
    effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 4, 0x300, NULL);
}

/// Spawn handler of a specimen projectile, entry 0 of `Actor07000_D000E0`.
/// Allocates the `_Actor07000SlouchProjectileWork`, spawns effect 0x60081 on
/// the parent's coordinate and re-parents the task under it, and derives a launch velocity
/// from the spawn angle in `spawnArg1` and two `gRandomLcgState` draws, rotated
/// into the coordinate's frame and scaled on the GTE. The coordinate's rotation
/// is reset to identity and nudged by that velocity, and the work's capsule
/// body is linked on the coordinate, keyed by `Actor07000_D08078`. The
/// task takes `Actor07000_Fn068F0` as its exit callback, cues the launch sound
/// and runs its first frame through `Actor07000_Fn04E60`.
static void Actor07000_Fn04B18(Task* arg0)
{
    _Actor07000SlouchProjectileWork* work;
    GfxCoord*                        coord;
    EffectWork*                      eff;
    WorldCollisionBody*              body;
    WorldCollisionCapsule*           capsule;
    SVECTOR*                         vec;
    s32                              angle;
    s32                              pan;

    coord = arg0->extra.tmd->coords;
    work  = memCalloc(sizeof(_Actor07000SlouchProjectileWork), false);
    if (work == NULL) {
        taskCallExit(arg0);
        return;
    }
    body                    = &work->body;
    capsule                 = &work->capsule;
    vec                     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    arg0->work              = work;
    eff                     = effectSpawn(EFFECT_PROJECTILE_GLOW_SPRITE, coord, 0, NULL);
    arg0->spawnArg2.pointer = eff->task;
    taskReparent(arg0, eff->task);
    angle           = arg0->spawnArg1.value;
    vec->vy         = -rcos(angle);
    vec->vx         = rsin(angle);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    vec->vz         = 0xE000 - ((gRandomLcgState >> 16) & 0x1FFF);
    _gfxRotateSv(&coord->coord, vec);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gte_lddp(((gRandomLcgState >> 16) & 0x1F) + 0x1E);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(&work->velocity);
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]    += work->velocity.vx;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    coord->coord.t[1]    += (gRandomLcgState >> 16) & 0x7F;
    coord->coord.t[2]    += work->velocity.vz;
    coord->composeStamp   = GRAPHICS_COORD_DIRTY;
    body->coord           = coord;
    body->context.capsule = capsule;
    body->pos.vx          = 0;
    body->pos.vy          = 0;
    body->pos.vz          = 0;
    body->radius          = 0;
    body->key             = damagePackAttackKey(Actor07000_D08078, 1);
    body->flags           = WORLD_COLLISION_BODY_CAPSULE;
    capsule->contacts     = work->contacts;
    capsule->ends[1].vx   = 0;
    capsule->ends[1].vy   = 0;
    capsule->ends[1].vz   = 0;
    capsule->ends[0].vx   = 0;
    capsule->ends[0].vy   = 0;
    capsule->ends[0].vz   = 0;
    capsule->end0Radius   = 0x96;
    capsule->end1Radius   = 0x96;
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
    body->flags       |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg0->exitCallback = Actor07000_Fn068F0;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    arg0->state += 1;
    pan          = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_SUCKLERCEPH_PROJECTILE_LAUNCH, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    Actor07000_Fn04E60(arg0);
}

/// Per-frame handler of a specimen projectile, entry 1 of
/// `Actor07000_D000E0`. `gSceneCombatState.actorControl` mode 1 returns at once and mode 2 hides
/// the model; mode 0 shows it again before the update. The update moves the
/// coordinate by the work's velocity (its negation becoming the capsule's far
/// end, so the shape covers the step just taken), lets the vertical speed grow
/// by 0xA a frame, and tests the work's contact: a hit on an object of the
/// 0x10000 kind or any other occupied contact cues the impact sound, tells the
/// child task how it landed through `spawnArg1` (3, or 2 for a contact whose
/// direction's Y is below -0xC00), takes the body out of the grid and pair
/// passes, arms a 0x1E-frame kill countdown and moves on to
/// `Actor07000_Fn068B4`. The contact is released either way.
///
/// The 2/3 pair is written into each arm rather than through a temp: the shared
/// store m2c reads as one variable is `jump.c` cross-jumping the two arms, and
/// a named temp puts the value's live range in front of the comparison that
/// picks it, where it can no longer share `$v0` with the `slti` result.
static void Actor07000_Fn04E60(Task* arg0)
{
    _Actor07000SlouchProjectileWork* work;
    TmdObject*                       part;
    Task*                            child;
    GfxCoord*                        coord;
    WorldCollisionCapsule*           capsule;
    WorldCollisionContact*           hit;
    WorldCollisionContact*           contacts;
    s32                              state;

    work    = arg0->work;
    part    = arg0->extra.tmd;
    state   = gSceneCombatState.actorControl;
    child   = arg0->firstChild;
    capsule = &work->capsule;
    coord   = part->coords;
    hit     = work->contacts;

    switch (state) {
        case 0:
            part->flags = 0;
            break;
        case 1:
            return;
        case 2:
            part->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        default:
            break;
    }
    // Sweep the capsule back over this frame's step, then take the step.
    capsule->ends[1].vx = -work->velocity.vx;
    capsule->ends[1].vy = -work->velocity.vy;
    capsule->ends[1].vz = -work->velocity.vz;
    coord->coord.t[0]   = coord->coord.t[0] + work->velocity.vx;
    contacts            = work->contacts;
    coord->coord.t[1]   = coord->coord.t[1] + work->velocity.vy;
    coord->coord.t[2]   = coord->coord.t[2] + work->velocity.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->velocity.vy   = work->velocity.vy + 0xA;
    if (worldCollisionCountContactsByKind(contacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        sndEvtRequestScriptStart(SOUND_SUCKLERCEPH_PROJECTILE_IMPACT, (s8)worldCoordGetOriginAudioPan(coord),
                                 (s8)worldCoordGetOriginAudioDepth(coord));
        if (child != NULL) {
            child->spawnArg1.value = 3;
        }
        // Landed: stop colliding and leave the task to its kill countdown.
        work->body.flags    = work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        arg0->killCountdown = 0x1E;
        arg0->state         = arg0->state + 1;
    } else if (worldCollisionFindContactIndex(contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        sndEvtRequestScriptStart(SOUND_SUCKLERCEPH_PROJECTILE_IMPACT, (s8)worldCoordGetOriginAudioPan(coord),
                                 (s8)worldCoordGetOriginAudioDepth(coord));
        if (child != NULL) {
            if (hit->response.direction.vy >= -0xC00) {
                child->spawnArg1.value = 3;
            } else {
                child->spawnArg1.value = 2;
            }
        }
        // Landed: stop colliding and leave the task to its kill countdown.
        work->body.flags    = work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        arg0->killCountdown = 0x1E;
        arg0->state         = arg0->state + 1;
    }
    worldCollisionClearContacts(work->contacts);
}

/// The variant's spawn handler: allocate the `_Actor07000SlouchWork` block,
/// rebind the model's light and colour matrices into it, link its three `WorldCollisionBody`
/// render nodes and their collision tables, and hand the task over to the state
/// table in `Task::msgTable`.
///
/// Node 1 is the odd one: it points its context at the `WorldCollisionCapsule` at 0x1FC
/// rather than at a record table, and the record's own `contacts` names the one
/// `WorldCollisionContact` beside it - the pair `CompanionWork` keeps, and the three constants it
/// carries are that record's fields rather than an object's. Node 3's `field_8`
/// is the model's seventh coordinate (`&coord[6]`), which is the value the
/// sibling `_sucklercephSpawnState` computes for its `bodyCoord`.
///
/// The spawn arg seeds `spawnArgHi`/`spawnArgLo` the same way it does there, and
/// a high halfword of 1 kills the specimen instead. The tail draws two numbers
/// off `gRandomLcgState` for `idleFrames`/`watchFrames`, the lengths of the
/// idle wait and of the watch animation.
static void Actor07000_Fn05068(Enemy* arg0, Task* arg1)
{
    _Actor07000SlouchWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    u32                    draw;
    s32                    one;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[6];
    one   = 1;
    if ((s16)(arg1->spawnArg1.value >> 16) == one) {
        enemyDestroy(arg0, arg1);
        return;
    }
    work = memCalloc(sizeof(_Actor07000SlouchWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    work->spawnArgLo    = arg1->spawnArg1.value;
    work->spawnArgHi    = arg1->spawnArg1.value >> 16;
    obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->coord                   = coord;
    arg0->node.state.parts.flags  = one;
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vy              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &Actor07000_D08080;
    arg0->hp                      = Actor07000_D08080.hpMax;
    arg0->recs                    = &work->contacts[0];
    work->hitEffectArg.coord      = &arg1->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = one;
    animationInitContext(&work->rig.anim, Actor07000_D0D77C, obj, work->rig.poses, &work->rig.slots[0]);
    i = 1;
    do {
        animationResetSlot(&work->rig.anim, i, 1);
        i += 1;
    } while (i < ARRAY_SIZE(work->rig.slots));
    (sceneAcquireBattleRef)(0);
    work->animId                    = ACTOR_07000_SLOUCH_ANIM_IDLE;
    work->appliedAnim               = ACTOR_07000_SLOUCH_ANIM_IDLE;
    work->field_388                 = 0;
    work->stretch                   = 0;
    work->stretchTarget             = 0;
    work->alert                     = 0;
    work->hitCooldown               = 0;
    work->senseCapsule.ends[0].vz   = 0xBB8;
    work->senseCapsule.end0Radius   = 0xFA0;
    work->senseCapsule.end1Radius   = 0x7D0;
    work->senseCapsule.contacts     = work->senseContacts;
    work->senseBody.context.capsule = &work->senseCapsule;
    work->senseBody.coord           = coord;
    work->senseBody.pos.vx          = 0;
    work->senseBody.pos.vy          = 0;
    work->senseBody.pos.vz          = 0;
    work->senseBody.key             = 0;
    work->senseBody.radius          = 0;
    work->senseBody.flags           = (u32)WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(work->senseContacts, ARRAY_SIZE(work->senseContacts), 0);
    work->body.coord            = coord;
    work->body.context.contacts = &work->contacts[0];
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0x258;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3002A;
    work->body.radius           = 0x258;
    work->body.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags       = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(&work->contacts[0], ARRAY_SIZE(work->contacts), 0);
    work->attackBody.coord            = part;
    work->attackBody.context.contacts = &work->attackContacts[0];
    work->attackBody.pos.vx           = -0x154;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->body.flags                  = (u16)(work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
    work->attackBody.key              = damagePackAttackKey(Actor07000_D08078, 0);
    work->attackBody.radius           = 0x12C;
    work->attackBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(&work->attackContacts[0], ARRAY_SIZE(work->attackContacts), 0);
    work->hasBurst         = 0;
    work->dropArmed        = 0;
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->idleFrames       = (u16)(((u32)draw >> 16) % 20U + 0x50);
    draw = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->watchFrames      = (u16)(((u32)draw >> 16) % 50U + 0x32);
    arg1->msgTable         = Actor07000_D0D7C0;
    arg1->state            = 4;
}

static __inline__ void update_color(Enemy* enemy, GfxCoord* coord)
{
    VECTOR* block;
    block                        = (VECTOR*)(SCRATCH_STACK_CURSOR(u8) - 0x10);
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    block->vx                    = coord->workm.t[0];
    block->vy                    = coord->workm.t[1];
    block->vz                    = coord->workm.t[2];
    worldCoordUpdateActorColor(enemy, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
static __inline__ void rotate_parts(Task* arg0)
{
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                         = arg0->work;
    head                         = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(MATRIX) = (MATRIX*)(head - 0x20);
    scratch                      = (MATRIX*)(head - 0x20);
    coord                        = arg0->extra.tmd->coords;
    RotMatrix(&work->twist, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->twist, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->twist.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->twist.vx    = 0;
            work->twistActive = 0;
        } else {
            work->twist.vx -= 0x20;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Per-frame handler of the specimen's second form while it drops into place,
/// entry 4 of `Actor07000_D0004C`. `gSceneCombatState.actorControl` mode 1 only re-colours the
/// actor and mode 2 hides the model; otherwise, once `dropArmed` has armed the
/// drop, the root is stepped along its facing and by the fall speed
/// `fallSpeed`, the collision response is applied, the six helper animation
/// slots tick, the reaction twist is applied to coordinates 3 and 5, and the
/// root is recomputed, with the step length `forwardSpeed` decaying by 2 a frame.
/// Once the root is below the floor (Y above 0) the landing sound is cued, the
/// twist is armed at 0x400, the root is pinned at 0 and the task moves to
/// state 1; until then the fall speed grows by 10 a frame, or 20 once the
/// collision response has latched `dropCollided`.
static void Actor07000_Fn05400(Enemy* arg0, Task* arg1)
{
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;
    s32                    sound;
    s32                    pan;
    work = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            update_color(arg1->spawnArg2.pointer, &arg1->extra.tmd->coords[1]);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->dropArmed != 0) {
                Actor07000_Fn06820(arg1);
                Actor07000_Fn0595C(arg1);
                update_animation(arg1);
                rotate_parts(arg1);
                actorRenderComposeCoord(arg1->extra.tmd->coords);
                update_color(arg1->spawnArg2.pointer, &arg1->extra.tmd->coords[1]);
                work->forwardSpeed -= 2;
                if (work->forwardSpeed < 0)
                    work->forwardSpeed = 0;
                coord = arg1->extra.tmd->coords;
                if (coord->coord.t[1] > 0) {
                    sound = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                    work->twistActive                   = 1;
                    work->twist.vx                      = 0x400;
                    work->fallSpeed                     = 0;
                    work->forwardSpeed                  = 0;
                    arg1->extra.tmd->coords->coord.t[1] = 0;
                    arg1->state                         = 1;
                    return;
                }
                if (work->dropCollided == 0)
                    work->fallSpeed += 10;
                else
                    work->fallSpeed += 20;
            }
            break;
    }
}

/// Collision response of the specimen's second form: node 2's collision table
/// is run through `worldCollisionResolvePushback` with a 0x38-byte scratch. Response 1 adds
/// the returned X and Z offsets to the root; only the first one (while
/// `dropCollided` is clear) also adds Y, latches the response in `twistActive` and
/// `dropCollided`, arms `twist.vx` to 0x400, sets the fall speed `fallSpeed` to
/// -0x50 and takes a quarter off the step length `forwardSpeed`. Response 2 puts
/// the root back at the translation saved in `prevRootPos`. Both collision
/// tables are released either way.
static void Actor07000_Fn0595C(Task* arg0)
{
    ActorContactDeltaScratch* scratch;
    _Actor07000SlouchWork*    work;
    GfxCoord*                 coord;
    s32                       movement;

    work     = arg0->work;
    scratch  = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaScratch);
    coord    = arg0->extra.tmd->coords;
    movement = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->dropCollided == 0) {
                work->twistActive  = movement;
                work->twist.vx     = 0x400;
                coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->fallSpeed    = -0x50;
                work->forwardSpeed = work->forwardSpeed - work->forwardSpeed / 4;
                work->dropCollided = movement;
            }
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->contacts);
    worldCollisionClearContacts(work->attackContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaScratch);
}

/// Message 0x7DB handler of the second form's table (`Actor07000_D0D7C0`,
/// parked in `Task::msgTable` by `Actor07000_Fn05068`). The payload's
/// halfword at 0x2 is a command word.
///
/// 4 and 5 are the puffing arms: both spawn the 0x60080 effect on the model's
/// root coordinate, restart the puff count in `idleFrames` and set `state` to
/// puffing; 5 clears `watchFrames` as well.
///
/// Low byte 1 is the reveal. Unless the task already runs one of the two live
/// states, the model is placed at the spawn point bits 8..11 select from the
/// current map's table - `D_shelter_b3_dumping_hole_8018B74C` on map 0x27, where the appearance sound
/// is cued through `sndEvtRequestScriptStart` as well, `D_shelter_b3_garbage_incinerator_801874C4` on 0x28 - the
/// buffers are re-armed, the 0x80 and 4 bits are cleared from the model's flag
/// word, the enemy's `node.state.parts.flags` is zeroed, both render nodes are revealed, and
/// the model is turned to the spawn point's heading. Low byte 3 is the hide:
/// the two bits and the pose flag go the other way, both nodes are hidden, the
/// model's translation and rotation are zeroed, and the task moves to state 4.
s32 Actor07000_Fn05AB8(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;
    TmdObject*             obj;
    GfxCoord*              coord;
    SVECTOR                rot;
    u16                    word;
    s32                    mode;
    s32                    sound;
    s32                    pan;

    word  = request->command;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    mode  = word & 0xFFFF;
    coord = obj->coords;
    if (mode == 4) {
        effectSpawn(EFFECT_ADDITIVE_PUFF, coord, 0x400, &Actor07000_D0D7B8);
        work->idleFrames = 0;
        work->state      = ACTOR_07000_SLOUCH_STATE_PUFFING;
        return 0;
    }
    if (mode == 5) {
        effectSpawn(EFFECT_ADDITIVE_PUFF, coord, 0x400, &Actor07000_D0D7B8);
        work->idleFrames  = 0;
        work->watchFrames = 0;
        work->state       = ACTOR_07000_SLOUCH_STATE_PUFFING;
        return 0;
    }
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->location.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[request->command >> 8].z;
                sound             = (((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54270006);
                pan               = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (gGameSession->location.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].x;
                coord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].y;
                coord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> 8].z;
            }
            tmdAllocPrimitiveBuffer(arg0->extra.tmd);
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = 0;
            work->senseBody.flags        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->body.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            RotMatrix(&rot, &coord->coord);
            work->forwardSpeed                    = 0xC8;
            work->dropArmed                       = 1;
            work->fallSpeed                       = 0x64;
            work->dropCollided                    = 0;
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->senseBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        rot.vz                        = 0;
        rot.vy                        = 0;
        rot.vx                        = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                     = 0;
        coord->coord.t[1]                     = 0;
        coord->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg0->extra.tmd->coords);
        arg0->state     = 4;
        work->dropArmed = 0;
    }
    return 0;
}

void Actor07000_Fn05E6C(Task* arg0)
{
    EnemyTaskFuncTable4 sp;

    sp = Actor07000_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Rebinds the second form's animation id `animId` to its six helper
/// slots. When the id has changed since the last frame `appliedAnim` follows it,
/// the frame count `animFrames` restarts and every slot is pointed at the new
/// id with a blend of 8; otherwise the count ticks and the slots advance by
/// one frame.
static void Actor07000_Fn05ED4(Task* arg0)
{
    _Actor07000SlouchWork* work;
    s32                    i;

    work = arg0->work;
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, 8);
        }
    } else {
        work->animFrames++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Colours the specimen's second form from the world position of the model's
/// second coordinate, staged in a `VECTOR` taken off the scratch stack; the
/// colour target is the task's enemy.
static void Actor07000_Fn05F84(Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;
    void*     obj;

    obj                            = task->spawnArg2.pointer;
    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    worldCoordUpdateActorColor(obj, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Splices the work's own coordinate between the model's root and its second
/// part, with an identity rotation, marks both dirty and resets the scale
/// `SVECTOR` beside it to one. `stateFrames` is cleared, and unless `state`
/// is the puffing death the 0x600A5 effect is spawned on the root.
static void Actor07000_Fn05FF8(Task* arg0)
{
    GfxCoord*              parts;
    GfxCoord*              coord;
    _Actor07000SlouchWork* work;

    work            = arg0->work;
    parts           = arg0->extra.tmd->coords;
    coord           = &work->deathCoord;
    coord->parent   = parts;
    parts[1].parent = coord;
    gfxSetRotIdentity(&coord->coord);
    coord->composeStamp   = GRAPHICS_COORD_DIRTY;
    parts[1].composeStamp = GRAPHICS_COORD_DIRTY;
    work->deathScale.vx   = 0x1000;
    work->deathScale.vy   = 0x1000;
    work->deathScale.vz   = 0x1000;
    work->stateFrames     = 0;
    if (work->state != ACTOR_07000_SLOUCH_STATE_PUFFING_DEATH) {
        effectSpawn(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords, 2, NULL);
    }
}

/// Flattens the coordinate `Actor07000_Fn05FF8` spliced in: the Y scale
/// `deathScale.vy` loses 2 and the matrix's second column is scaled by it, then
/// the coordinate is marked dirty.
static void Actor07000_Fn06088(Task* arg0)
{
    u16                    scale;
    GfxCoord*              coord;
    _Actor07000SlouchWork* work;

    work                          = arg0->work;
    coord                         = &work->deathCoord;
    scale                         = work->deathScale.vy - 2;
    work->deathScale.vy           = scale;
    coord->coord.m[0][1]          = (s16)((s32)(coord->coord.m[0][1] * (s16)scale) >> 0xC);
    coord->coord.m[1][1]          = (s16)((s32)(coord->coord.m[1][1] * work->deathScale.vy) >> 0xC);
    coord->coord.m[2][1]          = (s16)((s32)(coord->coord.m[2][1] * work->deathScale.vy) >> 0xC);
    work->deathCoord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Stretches the model's sixth coordinate by the scale delta `stretch`
/// while it is non-zero: the matrix's first column is scaled by
/// `0x1000 + stretch`, the other two by a quarter of that delta, each column
/// through an `SVECTOR` on the GTE, and the coordinate is marked dirty.
static void Actor07000_Fn060FC(Task* arg0)
{
    SVECTOR                vec;
    MATRIX*                m;
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->stretch != 0) {
        m = &coord[5].coord;
        gte_ReadMatrixColumn(m, 0, &vec);
        gte_lddp(work->stretch + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 0);

        gte_ReadMatrixColumn(m, 1, &vec);
        gte_lddp((work->stretch >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 1);

        gte_ReadMatrixColumn(m, 2, &vec);
        gte_lddp((work->stretch >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12();
        gte_stsv(&vec);
        gte_WriteMatrixColumn(&vec, m, 2);

        coord[5].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

static void Actor07000_Fn062A8(Task* arg0)
{
    SVECTOR   offset;
    u32       dist;
    GfxCoord* coords;
    GfxCoord* child;
    Task*     task;
    s32       angle;

    coords    = arg0->extra.tmd->coords;
    child     = &coords[1];
    angle     = Actor07000_Fn047F4(coords, &dist);
    offset.vz = 0;
    offset.vy = 0;
    offset.vx = 0;
    task      = taskSpawnFromTable(Actor07000_D0D7D0, 1, angle, 0);
    if (task != NULL) {
        actorRenderCopyCoordBodyTransform(task, child, &offset);
        taskReparent(arg0, task);
    }
}

/// Task handler of the specimen's contact effect: runs the entry of
/// `Actor07000_D000E0` for the task's state. The table is copied onto the
/// stack before the call.
void Actor07000_Fn06338(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor07000_D000E0;
    sp.funcs[task->state](task);
}

/// Applies the second form's reaction twist: the rotation `twist` is
/// turned into a matrix on the scratch stack and multiplied into the rotations
/// of coordinates 3 and 5 on the GTE. The twist's X angle then decays by 0x20
/// a frame; once it would drop to 0x20 or below it is cleared together with
/// `twistActive`.
static void Actor07000_Fn06390(Task* arg0)
{
    _Actor07000SlouchWork* work;
    GfxCoord*              coord;
    MATRIX*                scratch;
    u8*                    head;
    s16                    value;

    work                         = arg0->work;
    head                         = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(MATRIX) = (MATRIX*)(head - 0x20);
    scratch                      = (MATRIX*)(head - 0x20);
    coord                        = arg0->extra.tmd->coords;
    RotMatrix(&work->twist, scratch);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);
    RotMatrix(&work->twist, scratch);
    gte_SetRotMatrix(&coord[5].coord);
    gte_ldclmv(scratch);
    gte_rtir();
    gte_stclmv(&coord[5].coord);
    gte_ldclmv(&scratch->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][1]);
    gte_ldclmv(&scratch->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[5].coord.m[0][2]);
    value = work->twist.vx;
    if (value != 0) {
        if (value < 0x21) {
            work->twist.vx    = 0;
            work->twistActive = 0;
        } else {
            work->twist.vx -= 0x20;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Per-frame reaction handler: folds the generic hit flags into the enemy's
/// `reactionFlags`, applies a pending hit, and drops the work to its death pose when
/// the hit lands.
static void Actor07000_Fn0662C(Task* arg0)
{
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;
    s32                    tick;
    u8                     flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags != 0) {
        if (flags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->state           = ACTOR_07000_SLOUCH_STATE_STATUS_HOLD;
            work->stateFrames     = 0;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            tick = damageTickEnemyDamageOverTime(enemy);
            if (tick != 0) {
                Actor07000_Fn04274(arg0, tick);
                work->state  = ACTOR_07000_SLOUCH_STATE_IDLE;
                work->animId = ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH;
            }
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}

/// Gives `dst`'s model the texture page and CLUT of `src`'s, re-streaming it
/// twice when it has a buffer.
static void Actor07000_Fn066FC(Task* dst, Task* src)
{
    TmdObject* to;
    TmdObject* from;

    from                  = src->extra.tmd;
    to                    = dst->extra.tmd;
    to->texturePageOffset = from->texturePageOffset;
    to->clutRowOffset     = from->clutRowOffset;
    if (to->buffer != NULL) {
        tmdBuildBufferHalf(to);
        tmdBuildBufferHalf(to);
    }
}

/// Exit callback of the specimen's second form: flags the enemy's node, takes
/// it and the three render nodes back off their lists and runs the common
/// enemy task exit.
static void Actor07000_Fn06750(Task* task)
{
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = 0;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->attackBody);
    enemyTaskExit(task);
}

/// Task handler of the specimen's second form: runs the entry of
/// `Actor07000_D0004C` for the task's state with the enemy and the task. The
/// table is copied onto the stack before the call.
void Actor07000_Fn067B4(Task* task)
{
    EnemyTaskFuncTable5 sp;

    sp = Actor07000_D0004C;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Steps the second form's root one frame: saves the current translation in
/// `prevRootPos`, advances X and Z along the rotation's Z column scaled by the
/// step length `forwardSpeed`, and Y by the fall speed `fallSpeed`.
static void Actor07000_Fn06820(Task* arg0)
{
    GfxCoord*              coord;
    _Actor07000SlouchWork* work;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    coord->coord.t[1] += work->fallSpeed;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
}

/// Counts the task's kill countdown down and runs its exit callback once it
/// runs out.
static void Actor07000_Fn068B4(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        taskCallExit(arg0);
    }
}

/// Exit callback of the Slouch's projectile: takes its collision body back off
/// the object list and kills the task.
static void Actor07000_Fn068F0(Task* arg0)
{
    _Actor07000SlouchProjectileWork* work;

    work = arg0->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(arg0);
}
