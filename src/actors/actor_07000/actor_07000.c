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
#include "gameplay/display.h"
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

static void _sucklercephDropState(Enemy* enemy, Task* task);

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
static void         _actor07000SlouchTask(Task* task);
static void         _actor07000SlouchProjectileTask(Task* task);
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
    { ACTOR_COMMAND_MESSAGE_APPLY, _sucklercephMessage },
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
    { { { TASK_BODY_TMD, 96 } }, _actor07000SlouchTask, { .model = &_gActor07000Actor107000Model08BB4 } },
    { { { TASK_BODY_COORD, 96 } }, _actor07000SlouchProjectileTask, { .value = 0 } },
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

static void _actor07000SlouchSpawn(Enemy* enemy, Task* task);

static void Actor07000_Fn03164(Enemy* arg0, Task* arg1);

static void _actor07000SlouchIdle(Task* task, TmdObject* unusedModel, s32 unusedOne);

static void Actor07000_Fn037EC(Task* arg0, TmdObject* arg1, s32 arg2);

static void _actor07000SlouchContacts(Task* task);

static void _actor07000SlouchTakeDamage(Task* task, s32 damage);

static void _actor07000SlouchDeathState(Enemy* enemy, Task* task);

static void _actor07000SlouchPickAction(Task* task, s32 unusedAfterStrike);

static s32 _actor07000SlouchMeasurePlayer(GfxCoord* reference, u32* rangeOut);

static void _actor07000SlouchBurst(Task* task);

static void _actor07000SlouchProjectileLaunch(Task* task);

static void _actor07000SlouchProjectileFly(Task* task);

static void Actor07000_Fn05068(Enemy* arg0, Task* arg1);

static void Actor07000_Fn05400(Enemy* arg0, Task* arg1);

static void _actor07000SlouchDropCollide(Task* task);

static void _actor07000SlouchAnimate(Task* task);

static void _actor07000SlouchUpdateColor(Task* task);

static void _actor07000SlouchInitDeathCoord(Task* task);

static void _actor07000SlouchFlatten(Task* task);

static void _actor07000SlouchStretch(Task* task);

static void Actor07000_Fn062A8(Task* arg0);

static void _actor07000SlouchApplyTwist(Task* task);

static void _actor07000SlouchTickReactions(Task* task);

static void _actor07000SlouchCopyTexturePlacement(Task* destinationTask, Task* sourceTask);

static void _actor07000SlouchExit(Task* task);

static void _actor07000SlouchFallStep(Task* task);

static void _actor07000SlouchProjectileExpire(Task* task);

static void _actor07000SlouchProjectileExit(Task* task);

static __inline__ void _actor07000SlouchColorAtCoord(Enemy* enemy, GfxCoord* sampleCoord);
static __inline__ void _actor07000SlouchApplyDropTwist(Task* task);
static __inline__ void _actor07000SlouchTwistPart(MATRIX* partMatrix, SVECTOR* twist, MATRIX* twistMatrix);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Message dispatch table the caged specimen's spawn parks in `Task::msgTable`.

#include "../../shared/sucklerceph_spawn_state.inc.c"

/// Task states of the caged specimen as `_sucklercephTask` dispatches
/// them: spawn, per-frame update and teardown.
static const EnemyTaskFuncTable3 gSucklercephTaskStates = {
    { _sucklercephSpawnState, _sucklercephUpdateState, _sucklercephDeathState },
};

/// Task states of the caged specimen as `sucklercephDropTask` dispatches them:
/// the same update and teardown after a spawn that parks the specimen hidden,
/// and a fourth state for its drop into place.
static const EnemyTaskFuncTable4 gSucklercephDropTaskStates = {
    { _sucklercephDropSpawnState, _sucklercephUpdateState, _sucklercephDeathState, _sucklercephDropState },
};

#include "../../shared/sucklerceph_reaction_dispatch.inc.c"

#include "../../shared/sucklerceph_inlines.inc.c"

#include "../../shared/sucklerceph_dormant_tick.inc.c"

#include "../../shared/sucklerceph_awake_tick.inc.c"

#include "../../shared/sucklerceph_contacts.inc.c"

#include "../../shared/sucklerceph_take_damage.inc.c"

#include "../../shared/sucklerceph_turn_to_player.inc.c"

#include "../../shared/sucklerceph_death_state.inc.c"

#define SUCKLERCEPH_BURST_FX_OFFSET          Actor07000_D08068
#define SUCKLERCEPH_BURST_PAD_SCRIPT         Actor07000_D06938
#define SUCKLERCEPH_BURST_VIBRATION_SEGMENTS Actor07000_D06944
#include "../../shared/sucklerceph_kill.inc.c"

#include "../../shared/sucklerceph_drop_spawn_state.inc.c"

/// Task states of the specimen's second form as `_actor07000SlouchTask`
/// dispatches them: spawn, per-frame update, death and destruction.
static const EnemyTaskFuncTable4 Actor07000_D0003C = {
    { _actor07000SlouchSpawn, Actor07000_Fn03164, _actor07000SlouchDeathState, enemyDestroy },
};

/// Task states of the specimen's second form as `Actor07000_Fn067B4`
/// dispatches them: the same update, death and destruction after a spawn that
/// parks the model hidden, and a fifth state for its drop into place.
static const EnemyTaskFuncTable5 Actor07000_D0004C = {
    { Actor07000_Fn05068, Actor07000_Fn03164, _actor07000SlouchDeathState, enemyDestroy, Actor07000_Fn05400 },
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

/// Creates a Slouch's animation, lighting, targeting and collision state.
///
/// Requires the enemy's seven-part model already attached to `task`. Owns a
/// zeroed work allocation until task teardown, borrows the package's resources,
/// and enters the update state; allocation failure destroys the enemy instead.
static void _actor07000SlouchSpawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_07000_SLOUCH_BODY_KEY = 0x2A };
    _Actor07000SlouchWork* work;
    WorldCollisionContact* senseContacts;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              strikeCoord;
    u32                    idleDraw;
    u32                    watchDraw;
    s32                    slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Actor07000SlouchWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Bind work-owned animation and lighting storage before linking collision bodies.
    task->work              = work;
    strikeCoord             = &rootCoord[6];
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->bodyPos.vy             = -0x64;
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &Actor07000_D08080;
    enemy->hp                     = Actor07000_D08080.hpMax;
    enemy->recs                   = work->contacts;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = 0x280;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, Actor07000_D0D77C, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, 1);
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
    senseContacts                   = work->senseContacts;
    work->senseCapsule.contacts     = senseContacts;
    work->senseBody.context.capsule = &work->senseCapsule;
    work->senseBody.coord           = rootCoord;
    work->senseBody.pos.vx          = 0;
    work->senseBody.pos.vy          = 0;
    work->senseBody.pos.vz          = 0;
    work->senseBody.key             = 0;
    work->senseBody.radius          = 0;
    work->senseBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(senseContacts, ARRAY_SIZE(work->senseContacts), 0);
    work->body.coord            = rootCoord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0x15E;
    work->body.pos.vz           = 0;
    work->body.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_07000_SLOUCH_BODY_KEY;
    work->body.radius           = 0x15E;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags                 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.coord            = strikeCoord;
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
    idleDraw                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState         = idleDraw;
    work->idleFrames        = (u16)((idleDraw >> 16) % 20U + 0x50);
    watchDraw               = idleDraw * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState         = watchDraw;
    work->watchFrames       = (u16)((watchDraw >> 16) % 50U + 0x32);
    task->exitCallback      = _actor07000SlouchExit;
    task->state            += 1;
}

/// Per-frame mode handler of the specimen. The `gSceneCombatState.actorControl` switch is the same
/// one `_sucklercephUpdateState` runs: mode 1 skips to the tail, mode 2 puts
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
            _actor07000SlouchUpdateColor(arg1);
            return;
        default:
            break;
    }
    switch (work->state) {
        case ACTOR_07000_SLOUCH_STATE_IDLE:
            _actor07000SlouchIdle(arg1, obj, one);
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
    _actor07000SlouchTickReactions(arg1);
    _actor07000SlouchContacts(arg1);
    _actor07000SlouchAnimate(arg1);
    _actor07000SlouchStretch(arg1);
    _actor07000SlouchApplyTwist(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg1->extra.tmd->coords[1]);
    _actor07000SlouchUpdateColor(arg1);
}

/// Runs the Slouch's idle, fidget, watch and light-hit animations.
///
/// An alert player contact in the sensing capsule engages it; a body contact
/// selects the spit. Consumes sensing contacts after the decision. Animation
/// timing counts driver ticks. The model and constant-one arguments are unused.
static void _actor07000SlouchIdle(Task* task, TmdObject* unusedModel, s32 unusedOne)
{
    _Actor07000SlouchWork* work;
    GfxCoord*              rootCoord;
    s32                    soundId;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
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
                soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3);
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            if (work->animFrames == 105) {
                soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 6);
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
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
            _actor07000SlouchMeasurePlayer(arg0->extra.tmd->coords, &distance);
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
            _actor07000SlouchMeasurePlayer(arg0->extra.tmd->coords, &distance);
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
                _actor07000SlouchPickAction(arg0, 1);
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
                _actor07000SlouchPickAction(arg0, 0);
            }
            if (work->animFrames >= 83) {
                _actor07000SlouchPickAction(arg0, 0);
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
                _actor07000SlouchPickAction(arg0, 0);
            }
            break;
        case ACTOR_07000_SLOUCH_ENGAGED_RECOIL:
            work->animId = ACTOR_07000_SLOUCH_ANIM_RECOIL;
            _actor07000SlouchMeasurePlayer(arg0->extra.tmd->coords, &distance);
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
                _actor07000SlouchPickAction(arg0, 0);
            }
            break;
    }
}

/// Applies the grid correction or restores the saved pre-step position.
///
/// Borrows the live Slouch work/root and the caller's contact scratch block.
/// Corrections use integer halves of signed 16.16 units. Leaves contacts live.
static __inline__ void _actor07000SlouchResolveBodyPushback(_Actor07000SlouchWork* work, GfxCoord* rootCoord, ActorContactDeltaScratch* scratch)
{
    s32 pushbackStatus;
    // Apply the grid correction, or undo the last step for opposed normals.
    pushbackStatus = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (pushbackStatus) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0]  = rootCoord->coord.t[0] + scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1]  = rootCoord->coord.t[1] + scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[1] = work->prevRootPos.vy;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
}

/// Applies the Slouch's room collision, attack hits and body-contact reactions.
///
/// Requires live enemy, seven-part model and initialized Slouch work. Grid
/// pushback adds integer halves of signed 16.16 corrections, or restores the
/// saved position for opposed normals. Each of the four body contacts is tested
/// in order; only a zero hit cooldown admits an attack, and a positive new
/// cooldown blocks later hits in that pass. Damage uses the
/// full XYZ player distance, and critical damage is multiplied by four before
/// HP loss and attribute reactions. Hit cooldowns use the positive signed low
/// halfword of the stored duration. Player-body contact engages an idle Slouch;
/// an enemy-body contact can latch its stopped motion. Consumes both contact
/// tables and releases the scratch block before returning.
static void _actor07000SlouchContacts(Task* task)
{
    // These attributes have this Slouch-specific response; their shared meaning is unproven.
    enum { ACTOR_07000_SLOUCH_REACTION_DETACH_PART  = 4,
           ACTOR_07000_SLOUCH_REACTION_BUILDUP_8    = 8,
           ACTOR_07000_SLOUCH_REACTION_BUILDUP_9    = 9,
           ACTOR_07000_SLOUCH_CRITICAL_FLASH_STYLE  = 0,
           ACTOR_07000_SLOUCH_HIT_TWIST_BASE        = 256,
           ACTOR_07000_SLOUCH_HIT_TWIST_RANDOM_MASK = 0x60 };
    s32                       playerDx;
    s32                       playerDy;
    s32                       playerDz;
    s32                       hitReaction;
    s32                       hitCooldownTicks;
    u32                       twistDraw;
    u32                       contactKind;
    u32                       hitDamage;
    s32                       contactIndex;
    _Actor07000SlouchWork*    work;
    GfxCoord*                 rootCoord;
    Enemy*                    enemy;
    ActorContactDeltaScratch* scratch;

    work      = task->work;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaScratch);
    rootCoord = task->extra.tmd->coords;
    enemy     = task->spawnArg2.pointer;
    _actor07000SlouchResolveBodyPushback(work, rootCoord, scratch);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    // A hit applies HP damage before its attribute reaction and visual response.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        contactKind = work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        switch (contactKind) {
            case WORLD_COLLISION_CONTACT_ATTACK:
                if (work->hitCooldown == 0) {
                    playerDx                 = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                    scratch->delta.vector.vx = playerDx;
                    playerDy                 = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
                    scratch->delta.vector.vy = playerDy;
                    playerDz                 = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                    scratch->delta.vector.vz = playerDz;
                    hitDamage                = damageComputePlayerAttack(work->contacts[contactIndex].key.value, SquareRoot0((playerDx * playerDx) + (playerDy * playerDy) + (playerDz * playerDz)), 0, 0);
                    if (damageRollCriticalHit(task->spawnArg2.pointer, work->contacts[contactIndex].key.value, 0) != 0) {
                        effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords, ACTOR_07000_SLOUCH_CRITICAL_FLASH_STYLE, NULL);
                        hitDamage *= 4;
                    }
                    damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, (s32)hitDamage, 0);
                    _actor07000SlouchTakeDamage(task, (s32)hitDamage);
                    hitReaction = damageGetPlayerAttackReaction(work->contacts[contactIndex].key.value) & 0xFFFF;
                    switch (hitReaction) {
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
                            damageTryStartEnemyDamageOverTime(enemy, work->contacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                        case ACTOR_07000_SLOUCH_REACTION_BUILDUP_8:
                        case ACTOR_07000_SLOUCH_REACTION_BUILDUP_9:
                            damageStartEnemyBuildup(enemy, work->contacts[contactIndex].key.value, 0);
                            break;
                        case ACTOR_07000_SLOUCH_REACTION_DETACH_PART:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                            if (enemy->hp < 0) {
                                _actor07000SlouchBurst(task);
                                work->hasBurst = 1;
                            }
                            break;
                    }
                    work->alert = 1;
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value), (task->extra.tmd->coords + 1), &Actor07000_D0D7B0, &work->hitEffectArg);
                    hitCooldownTicks = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                    if ((hitCooldownTicks << 0x10) > 0) {
                        work->hitCooldown = hitCooldownTicks;
                    }
                    work->twistActive = 1;
                    twistDraw         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState   = twistDraw;
                    work->twist.vx    = ((twistDraw >> 11) & ACTOR_07000_SLOUCH_HIT_TWIST_RANDOM_MASK) + ACTOR_07000_SLOUCH_HIT_TWIST_BASE;
                }
                break;
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (work->state == ACTOR_07000_SLOUCH_STATE_IDLE) {
                    work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
                    work->stateFrames   = 0;
                    work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
                }
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
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

/// Applies damage to the Slouch's HP/readout and selects its hit reaction.
///
/// Requires live enemy and Slouch work at the task. Damage of at least 51
/// selects engaged recoil, 21..50 selects idle flinch; a smaller hit engages
/// an idle or watching Slouch, spitting at a planar range of at least 2500.
/// Nonpositive remaining HP starts task death and disables the strike body.
/// Comparisons retain unsigned interpretation of the supplied damage.
static void _actor07000SlouchTakeDamage(Task* task, s32 damage)
{
    enum { ACTOR_07000_SLOUCH_TASK_DEATH    = 2,
           ACTOR_07000_SLOUCH_RECOIL_DAMAGE = 51,
           ACTOR_07000_SLOUCH_FLINCH_DAMAGE = 21,
           ACTOR_07000_SLOUCH_SPIT_RANGE    = 2500 };
    u32                    playerRange;
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    s32                    soundId;
    s16                    behavior;

    enemy      = task->spawnArg2.pointer;
    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = task->work;
    enemy->hp -= damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    if (enemy->hp <= 0) {
        sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), SOUND_SCRIPT_STOP_NO_FADE);
        soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 5);
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        task->state             = ACTOR_07000_SLOUCH_TASK_DEATH;
        work->deathPhase        = ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN;
        return;
    }
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 3), SOUND_SCRIPT_STOP_NO_FADE);
    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 4);
    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
    behavior = work->state;
    if (behavior <= ACTOR_07000_SLOUCH_STATE_ENGAGED) {
        if ((u32)damage >= ACTOR_07000_SLOUCH_RECOIL_DAMAGE) {
            work->state         = ACTOR_07000_SLOUCH_STATE_ENGAGED;
            work->animFrames    = 0;
            work->stateFrames   = 0;
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_RECOIL;
            return;
        }
        if ((u32)damage >= ACTOR_07000_SLOUCH_FLINCH_DAMAGE) {
            work->animFrames  = 0;
            work->state       = ACTOR_07000_SLOUCH_STATE_IDLE;
            work->stateFrames = 0;
            work->animId      = ACTOR_07000_SLOUCH_ANIM_FLINCH;
            return;
        }
        if (behavior == ACTOR_07000_SLOUCH_STATE_IDLE || work->engagedAction == ACTOR_07000_SLOUCH_ENGAGED_WATCH) {
            _actor07000SlouchMeasurePlayer(task->extra.tmd->coords, &playerRange);
            work->state       = ACTOR_07000_SLOUCH_STATE_ENGAGED;
            work->stateFrames = 0;
            if (playerRange >= ACTOR_07000_SLOUCH_SPIT_RANGE) {
                work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_SPIT;
                return;
            }
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_STRIKE;
        }
    }
}

/// Task states of a specimen projectile as `_actor07000SlouchProjectileTask` dispatches
/// them: launch, flight and the countdown after impact.
static const TaskFuncTable3 Actor07000_D000E0 = {
    { _actor07000SlouchProjectileLaunch, _actor07000SlouchProjectileFly, _actor07000SlouchProjectileExpire },
};

/// Applies the Slouch's requested animation or advances its six driven parts.
///
/// A changed id restarts the tick counter and seeks slots 1..6 with an eight-
/// frame blend; an unchanged id increments the signed-halfword counter and
/// ticks those slots. Slot 0 stays unstarted. Requires the live initialized
/// seven-slot rig and a valid nonzero index in the package's animation table.
static __inline__ void _actor07000SlouchAnimateInline(Task* task)
{
    enum { ACTOR_07000_SLOUCH_ANIMATION_BLEND_TICKS = 8 };
    _Actor07000SlouchWork* work;
    s32                    slotIndex;
    work = task->work;
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, ACTOR_07000_SLOUCH_ANIMATION_BLEND_TICKS);
        }
    } else {
        work->animFrames++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Runs the Slouch's death phases, then hands the task to destruction.
///
/// Requires live enemy/model/work and a death phase initialized to BEGIN.
/// Paused combat holds the phase; hidden combat hides the model and target.
/// Otherwise withdraws the target, three collision bodies and battle reference,
/// then counts 61 flatten ticks. An intact body gets an inserted scaling
/// coordinate and corpse fire; a burst body is hidden. The end hides the model,
/// restores model part 1's parent and enters task state 3. Animation continues
/// in the weighted color mode on each unpaused, visible death tick.
static void _actor07000SlouchDeathState(Enemy* enemy, Task* task)
{
    enum { ACTOR_07000_SLOUCH_DEATH_FLATTEN_TICKS = 61,
           ACTOR_07000_SLOUCH_TASK_DESTROY        = 3,
           ACTOR_07000_SLOUCH_CORPSE_FIRE_SOUND   = SOUND_COMMON(0x0D),
           ACTOR_07000_SLOUCH_LEGACY_REWARD_ARG   = 0x2A };
    _Actor07000SlouchWork* work;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              bodyCoord;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    bodyCoord = &rootCoord[1];
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->deathPhase) {
                case ACTOR_07000_SLOUCH_DEATH_PHASE_BEGIN:
                    if (work->hasBurst == 0) {
                        sndEvtRequestScriptStart(ACTOR_07000_SLOUCH_CORPSE_FIRE_SOUND, 0, 0);
                        model->flags = TMD_OBJECT_SEMI_TRANS;
                        _actor07000SlouchInitDeathCoord(task);
                    }
                    // Withdraw targeting and collision before flattening or hiding.
                    enemy->recs = NULL;
                    worldTargetUnlinkNode(&enemy->node);
                    worldCollisionUnlinkBody(&work->senseBody);
                    worldCollisionUnlinkBody(&work->body);
                    worldCollisionUnlinkBody(&work->attackBody);
                    sceneReleaseBattleRefWithRewards(task, ACTOR_07000_SLOUCH_LEGACY_REWARD_ARG);
                    work->animId      = ACTOR_07000_SLOUCH_ANIM_DEATH;
                    work->stateFrames = 0;
                    work->deathPhase  = ACTOR_07000_SLOUCH_DEATH_PHASE_FLATTEN;
                    break;
                case ACTOR_07000_SLOUCH_DEATH_PHASE_FLATTEN:
                    if (work->hasBurst == 0) {
                        _actor07000SlouchFlatten(task);
                    } else {
                        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->stateFrames++;
                    if (work->stateFrames >= ACTOR_07000_SLOUCH_DEATH_FLATTEN_TICKS) {
                        work->deathPhase = ACTOR_07000_SLOUCH_DEATH_PHASE_END;
                    }
                    break;
                case ACTOR_07000_SLOUCH_DEATH_PHASE_END:
                    sndEvtRequestScriptStop(ACTOR_07000_SLOUCH_CORPSE_FIRE_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    model->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    bodyCoord->parent = rootCoord;
                    task->state       = ACTOR_07000_SLOUCH_TASK_DESTROY;
                    break;
            }
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            _actor07000SlouchAnimateInline(task);
            break;
    }
}

/// Chooses the Slouch's next engaged action and consumes its sensing contact.
///
/// Watches without a sensed player or at planar range 3000 or greater.
/// Otherwise chooses fidget on 11 of 100 random results, then spits at range
/// 2500 or greater; nearer players get another 11-of-100 spit choice, else a
/// strike. A selected action restarts animation playback. `unusedAfterStrike`
/// is supplied as 1 after a strike and 0 otherwise, but is never read.
static void _actor07000SlouchPickAction(Task* task, s32 unusedAfterStrike)
{
    enum { ACTOR_07000_SLOUCH_ANIMATION_RESTART = 0 };
    u32                    playerRange;
    _Actor07000SlouchWork* work;
    u32                    actionDraw;

    work = task->work;
    _actor07000SlouchMeasurePlayer(task->extra.tmd->coords, &playerRange);
    if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) == 0 || playerRange >= 3000) {
        work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_WATCH;
        work->stateFrames   = 0;
    } else {
        actionDraw      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = actionDraw;
        if ((u16)((actionDraw >> 16) % 100) < 11) {
            work->engagedAction = ACTOR_07000_SLOUCH_ENGAGED_FIDGET;
        } else if (playerRange >= 2500) {
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
        work->appliedAnim = ACTOR_07000_SLOUCH_ANIMATION_RESTART;
    }
    worldCollisionClearContacts(work->senseContacts);
}

/// Returns the player's bearing and writes its planar range from `reference`.
///
/// Bearing uses already-composed caches in the same frame and 4096 units per
/// turn, wrapped to -2048..2048. Range uses local X/Z translations in a common
/// parent frame, narrowing both differences to signed halfwords before squaring;
/// the squared sum must fit the SDK's nonnegative signed-word range. `rangeOut`
/// receives game-coordinate units. Borrows and releases scratch storage and
/// changes GTE state. Requires a live player task, both coordinates and output.
static s32 _actor07000SlouchMeasurePlayer(GfxCoord* reference, u32* rangeOut)
{
    GfxCoord*            playerCoord;
    ActorBearingScratch* scratch;
    s32                  bearing;

    playerCoord       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorBearingScratch);
    bearing           = _actorAngleBearingInFrame(scratch, reference, playerCoord);
    scratch->delta.vx = playerCoord->coord.t[0] - reference->coord.t[0];
    scratch->delta.vz = playerCoord->coord.t[2] - reference->coord.t[2];
    *rangeOut         = SquareRoot0(scratch->delta.vx * scratch->delta.vx + scratch->delta.vz * scratch->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorBearingScratch);
    return bearing;
}

/// Detaches one randomly selected Slouch body-part effect and emits two particles.
///
/// Requires the live seven-part Slouch model. One random draw chooses the centre
/// model on two outcomes, the arm on one and the leg on one, spawned from model
/// parts 1, 5 and 4 respectively. Installs the model in bank 8's slot 5 before
/// spawning and copies texture placement on success. Particle effects on parts
/// 1 and 4 are requested regardless of detached-model allocation success.
static void _actor07000SlouchBurst(Task* task)
{
    enum { ACTOR_07000_SLOUCH_BODY_PART_EFFECT_SLOT = 5,
           ACTOR_07000_SLOUCH_BURST_PARTICLE_SIZE   = 768 };
    /// Spawns a detached model from a part and copies its texture placement.
    ///
    /// Expands multiple statements: use as a full switch arm followed by break,
    /// or inside a braced block.
    /// modelSource and partIndex are evaluated once, sourceTask once, or twice
    /// on success, and effectLocal twice, or three times on success. Supply
    /// stable values and a modifiable EffectWork pointer local without side
    /// effects. The source model and coordinate must stay live for the spawn.
#define ACTOR_07000_SLOUCH_SPAWN_BURST_PART(modelSource, partIndex, sourceTask, effectLocal)                                                                             \
    D_800626EC[ACTOR_07000_SLOUCH_BODY_PART_EFFECT_SLOT].data.model = (modelSource);                                                                                     \
    (effectLocal)                                                   = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, (sourceTask)->extra.tmd->coords + (partIndex), 0, NULL); \
    if ((effectLocal) != NULL) {                                                                                                                                         \
        _actor07000SlouchCopyTexturePlacement((effectLocal)->task, (sourceTask));                                                                                        \
    }
    EffectWork* effect;
    s32         variant;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    variant         = (gRandomLcgState >> 16) & 3;
    switch (variant) {
        case 0:
        case 1:
            ACTOR_07000_SLOUCH_SPAWN_BURST_PART(&_gActor07000SlouchPoison, 1, task, effect);
            break;
        case 2:
            ACTOR_07000_SLOUCH_SPAWN_BURST_PART(&_gActor07000SlouchBurstArm, 5, task, effect);
            break;
        case 3:
            ACTOR_07000_SLOUCH_SPAWN_BURST_PART(&_gActor07000SlouchBurstLeg, 4, task, effect);
            break;
    }
    effectSpawn(EFFECT_030, task->extra.tmd->coords + 1, ACTOR_07000_SLOUCH_BURST_PARTICLE_SIZE, NULL);
    effectSpawn(EFFECT_030, task->extra.tmd->coords + 4, ACTOR_07000_SLOUCH_BURST_PARTICLE_SIZE, NULL);
#undef ACTOR_07000_SLOUCH_SPAWN_BURST_PART
}

/// Creates and launches the Slouch's coordinate-only swept projectile.
///
/// Requires a TASK_BODY_COORD task with its placement transform already copied
/// from model part 1 and spawnArg1 holding aim in 4096 units per turn. Allocates
/// zeroed work, attaches a glow child and stores that child in spawnArg2. Glow
/// allocation is assumed to succeed by the original code. Three random draws
/// choose the signed-halfword launch vector's Z component, its GTE 12-bit scale
/// (30..61) and a 0..127 height offset, in that order. Links a radius-150 capsule
/// with attack row 1, installs the exit callback, advances to flight and runs its
/// first tick immediately. Work allocation failure dispatches the existing exit.
static void _actor07000SlouchProjectileLaunch(Task* task)
{
    enum { ACTOR_07000_SLOUCH_PROJECTILE_RADIUS         = 150,
           ACTOR_07000_SLOUCH_PROJECTILE_ATTACK_ROW     = 1,
           ACTOR_07000_SLOUCH_LAUNCH_Z_BASE             = 0xE000,
           ACTOR_07000_SLOUCH_LAUNCH_Z_RANDOM_MASK      = 0x1FFF,
           ACTOR_07000_SLOUCH_LAUNCH_SCALE_MIN          = 30,
           ACTOR_07000_SLOUCH_LAUNCH_SCALE_RANDOM_MASK  = 0x1F,
           ACTOR_07000_SLOUCH_LAUNCH_HEIGHT_RANDOM_MASK = 0x7F };
    _Actor07000SlouchProjectileWork* work;
    GfxCoord*                        rootCoord;
    EffectWork*                      glowEffect;
    WorldCollisionBody*              body;
    WorldCollisionCapsule*           capsule;
    SVECTOR*                         launchVector;
    s32                              aimAngle;
    s32                              audioPan;

    rootCoord = task->extra.coordBody->coord;
    work      = memCalloc(sizeof(_Actor07000SlouchProjectileWork), false);
    if (work == NULL) {
        taskCallExit(task);
        return;
    }
    body                    = &work->body;
    capsule                 = &work->capsule;
    launchVector            = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    task->work              = work;
    glowEffect              = effectSpawn(EFFECT_PROJECTILE_GLOW_SPRITE, rootCoord, 0, NULL);
    task->spawnArg2.pointer = glowEffect->task;
    // The glow is the projectile's child in the teardown tree.
    taskReparent(task, glowEffect->task);
    aimAngle         = task->spawnArg1.value;
    launchVector->vy = -rcos(aimAngle);
    launchVector->vx = rsin(aimAngle);
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    launchVector->vz = ACTOR_07000_SLOUCH_LAUNCH_Z_BASE - ((gRandomLcgState >> 16) & ACTOR_07000_SLOUCH_LAUNCH_Z_RANDOM_MASK);
    _gfxRotateSv(&rootCoord->coord, launchVector);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gte_lddp(((gRandomLcgState >> 16) & ACTOR_07000_SLOUCH_LAUNCH_SCALE_RANDOM_MASK) + ACTOR_07000_SLOUCH_LAUNCH_SCALE_MIN);
    gte_ldsv(launchVector);
    gte_gpf12();
    gte_stsv(&work->velocity);
    // Velocity keeps the launch orientation; the live coordinate becomes unrotated.
    gfxSetRotIdentity(&rootCoord->coord);
    rootCoord->coord.t[0]  += work->velocity.vx;
    gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    rootCoord->coord.t[1]  += (gRandomLcgState >> 16) & ACTOR_07000_SLOUCH_LAUNCH_HEIGHT_RANDOM_MASK;
    rootCoord->coord.t[2]  += work->velocity.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    body->coord             = rootCoord;
    body->context.capsule   = capsule;
    body->pos.vx            = 0;
    body->pos.vy            = 0;
    body->pos.vz            = 0;
    body->radius            = 0;
    body->key               = damagePackAttackKey(Actor07000_D08078, ACTOR_07000_SLOUCH_PROJECTILE_ATTACK_ROW);
    body->flags             = WORLD_COLLISION_BODY_CAPSULE;
    capsule->contacts       = work->contacts;
    capsule->ends[1].vx     = 0;
    capsule->ends[1].vy     = 0;
    capsule->ends[1].vz     = 0;
    capsule->ends[0].vx     = 0;
    capsule->ends[0].vy     = 0;
    capsule->ends[0].vz     = 0;
    capsule->end0Radius     = ACTOR_07000_SLOUCH_PROJECTILE_RADIUS;
    capsule->end1Radius     = ACTOR_07000_SLOUCH_PROJECTILE_RADIUS;
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
    body->flags       |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = _actor07000SlouchProjectileExit;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    task->state += 1;
    audioPan     = (s8)worldCoordGetOriginAudioPan(rootCoord);
    sndEvtRequestScriptStart(SOUND_SUCKLERCEPH_PROJECTILE_LAUNCH, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    _actor07000SlouchProjectileFly(task);
}

/// Disables projectile collision and advances to its post-impact expiry state.
///
/// Requires the flight task and its live work. Clears grid and body-pair tests,
/// resets the signed-halfword countdown to 30 ticks and advances to state 2.
/// Leaves the body linked and storage live for the later exit callback.
static __inline__ void _actor07000SlouchProjectileStartExpiry(Task* task, _Actor07000SlouchProjectileWork* work)
{
    enum { ACTOR_07000_SLOUCH_PROJECTILE_IMPACT_TICKS = 30 };
    work->body.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->killCountdown = ACTOR_07000_SLOUCH_PROJECTILE_IMPACT_TICKS;
    task->state        += 1;
}

/// Moves the Slouch's swept projectile capsule and handles its first impact.
///
/// Requires coordinate-body storage and live projectile work. Paused and hidden
/// combat modes skip movement; other modes add velocity in parent-coordinate
/// units, then add 10 to its signed-halfword Y velocity. Any occupied contact
/// ends flight and starts a 30-tick expiry. A floor-like contact requests the
/// glow child's animated quad burst (2); other impacts its particle burst (3).
/// The contact is consumed after each moving tick.
static void _actor07000SlouchProjectileFly(Task* task)
{
    enum { ACTOR_07000_SLOUCH_PROJECTILE_GRAVITY           = 10,
           ACTOR_07000_SLOUCH_PROJECTILE_FLOOR_NORMAL_Y    = -3072,
           ACTOR_07000_SLOUCH_GLOW_FLOOR_BURST             = 2,
           ACTOR_07000_SLOUCH_GLOW_IMPACT_BURST            = 3,
           ACTOR_07000_SLOUCH_PROJECTILE_HIDDEN_BODY_VALUE = 0x80 };
    _Actor07000SlouchProjectileWork* work;
    ModelObjectCoordBody*            coordBody;
    Task*                            glowTask;
    GfxCoord*                        rootCoord;
    WorldCollisionCapsule*           capsule;
    WorldCollisionContact*           impactContact;
    WorldCollisionContact*           contacts;
    s32                              actorControl;

    work          = task->work;
    coordBody     = task->extra.coordBody;
    actorControl  = gSceneCombatState.actorControl;
    glowTask      = task->firstChild;
    capsule       = &work->capsule;
    rootCoord     = coordBody->coord;
    impactContact = work->contacts;

    // Preserve the low-halfword body-control writes; its full role is unproven.
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            *(u16*)&coordBody->field_C = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            *(u16*)&coordBody->field_C = ACTOR_07000_SLOUCH_PROJECTILE_HIDDEN_BODY_VALUE;
            return;
        default:
            break;
    }
    // Sweep the capsule back over this frame's step, then take the step.
    capsule->ends[1].vx     = -work->velocity.vx;
    capsule->ends[1].vy     = -work->velocity.vy;
    capsule->ends[1].vz     = -work->velocity.vz;
    rootCoord->coord.t[0]   = rootCoord->coord.t[0] + work->velocity.vx;
    contacts                = work->contacts;
    rootCoord->coord.t[1]   = rootCoord->coord.t[1] + work->velocity.vy;
    rootCoord->coord.t[2]   = rootCoord->coord.t[2] + work->velocity.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->velocity.vy       = work->velocity.vy + ACTOR_07000_SLOUCH_PROJECTILE_GRAVITY;
    if (worldCollisionCountContactsByKind(contacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        sndEvtRequestScriptStart(SOUND_SUCKLERCEPH_PROJECTILE_IMPACT, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                 (s8)worldCoordGetOriginAudioDepth(rootCoord));
        if (glowTask != NULL) {
            glowTask->spawnArg1.value = ACTOR_07000_SLOUCH_GLOW_IMPACT_BURST;
        }
        _actor07000SlouchProjectileStartExpiry(task, work);
    } else if (worldCollisionFindContactIndex(contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        sndEvtRequestScriptStart(SOUND_SUCKLERCEPH_PROJECTILE_IMPACT, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                 (s8)worldCoordGetOriginAudioDepth(rootCoord));
        if (glowTask != NULL) {
            if (impactContact->response.direction.vy >= ACTOR_07000_SLOUCH_PROJECTILE_FLOOR_NORMAL_Y) {
                glowTask->spawnArg1.value = ACTOR_07000_SLOUCH_GLOW_IMPACT_BURST;
            } else {
                glowTask->spawnArg1.value = ACTOR_07000_SLOUCH_GLOW_FLOOR_BURST;
            }
        }
        _actor07000SlouchProjectileStartExpiry(task, work);
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

/// Right-multiplies one part's rotation by the Euler twist using borrowed scratch.
///
/// Angles use 4096 units per turn. Translations and dirty stamps are unchanged;
/// the three rotation columns retain GTE saturation and halfword writes.
/// partMatrix and twistMatrix must be distinct live matrices; twist is borrowed.
static __inline__ void _actor07000SlouchTwistPart(MATRIX* partMatrix, SVECTOR* twist, MATRIX* twistMatrix)
{
    RotMatrix(twist, twistMatrix);
    gte_SetRotMatrix(partMatrix);
    gte_ldclmv(twistMatrix);
    gte_rtir();
    gte_stclmv(partMatrix);
    gte_ldclmv(&twistMatrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&partMatrix->m[0][1]);
    gte_ldclmv(&twistMatrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&partMatrix->m[0][2]);
}

/// Updates the Slouch enemy's lighting and color at a cached coordinate position.
///
/// Borrows the live enemy and sampleCoord; cached workm XYZ must already be
/// current in the lighting query's world frame. Reserves one VECTOR before
/// sampling signed 32-bit XYZ, leaves its fourth word intact, and releases it
/// after the nested query. Requires an initialized aligned scratch stack and
/// retains no pointer. Does not compose the coordinate.
static __inline__ void _actor07000SlouchColorAtCoord(Enemy* enemy, GfxCoord* sampleCoord)
{
    VECTOR* samplePosition;
    samplePosition     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    samplePosition->vx = sampleCoord->workm.t[0];
    samplePosition->vy = sampleCoord->workm.t[1];
    samplePosition->vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, samplePosition, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
/// Applies and decays the Slouch's drop reaction twist, then dirties the root.
///
/// Requires the initialized seven-part model and Slouch work. Right-multiplies
/// parts 3 and 5 by the Euler twist (4096 units per turn), retaining GTE
/// saturation and halfword writes. A nonzero signed X twist loses 32, or clears
/// with its active latch when below 33. Marks the model root dirty for the drop handler's following composition. Releases
/// its one MATRIX scratch reservation before returning.
static __inline__ void _actor07000SlouchApplyDropTwist(Task* task)
{
    enum { ACTOR_07000_SLOUCH_DROP_TWIST_DECAY = 32 };
    _Actor07000SlouchWork* work;
    GfxCoord*              coords;
    MATRIX*                twistMatrix;
    s16                    twistX;

    work        = task->work;
    twistMatrix = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    coords      = task->extra.tmd->coords;
    _actor07000SlouchTwistPart(&coords[3].coord, &work->twist, twistMatrix);
    _actor07000SlouchTwistPart(&coords[5].coord, &work->twist, twistMatrix);
    twistX = work->twist.vx;
    if (twistX != 0) {
        if (twistX < ACTOR_07000_SLOUCH_DROP_TWIST_DECAY + 1) {
            work->twist.vx    = 0;
            work->twistActive = 0;
        } else {
            work->twist.vx -= ACTOR_07000_SLOUCH_DROP_TWIST_DECAY;
        }
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
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
            _actor07000SlouchColorAtCoord(arg1->spawnArg2.pointer, &arg1->extra.tmd->coords[1]);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->dropArmed != 0) {
                _actor07000SlouchFallStep(arg1);
                _actor07000SlouchDropCollide(arg1);
                _actor07000SlouchAnimateInline(arg1);
                _actor07000SlouchApplyDropTwist(arg1);
                actorRenderComposeCoord(arg1->extra.tmd->coords);
                _actor07000SlouchColorAtCoord(arg1->spawnArg2.pointer, &arg1->extra.tmd->coords[1]);
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

/// Applies room pushback while the Slouch drops into place.
///
/// Adds the integer halves of the signed 16.16 correction to root X/Z. The
/// first grid response also corrects Y, arms a quarter-turn twist, rebounds
/// upward at 80 units per tick and reduces forward speed by a quarter. Opposed
/// normals restore the position saved before the step. Consumes both body and
/// strike contacts, with scratch storage released before return.
static void _actor07000SlouchDropCollide(Task* task)
{
    enum { ACTOR_07000_SLOUCH_DROP_TWIST = ONE / 4 };
    ActorContactDeltaScratch* scratch;
    _Actor07000SlouchWork*    work;
    GfxCoord*                 coord;
    s32                       pushbackStatus;

    work           = task->work;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaScratch);
    coord          = task->extra.tmd->coords;
    pushbackStatus = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (pushbackStatus) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            if (work->dropCollided == 0) {
                work->twistActive  = pushbackStatus;
                work->twist.vx     = ACTOR_07000_SLOUCH_DROP_TWIST;
                coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->fallSpeed    = -80;
                work->forwardSpeed = work->forwardSpeed - work->forwardSpeed / 4;
                work->dropCollided = pushbackStatus;
            }
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
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

/// Dispatches the standing Slouch task's spawn, update, death or destroy state.
///
/// Requires a live task with a state in 0..3 and its Enemy in spawnArg2.pointer.
/// Copies all four callbacks to the stack before dispatch; the callback can
/// release the task, so neither argument is accessed afterwards.
static void _actor07000SlouchTask(Task* task)
{
    EnemyTaskFuncTable4 states;

    states = Actor07000_D0003C;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Advances the Slouch's animation through its shared inline driver.
static void _actor07000SlouchAnimate(Task* task)
{
    _actor07000SlouchAnimateInline(task);
}

/// Updates the Slouch's lighting and color from model part 1's cached translation.
///
/// Requires a live enemy, its TMD model with at least two coordinates, and an
/// initialized scratch stack. Samples signed 32-bit XYZ without composing the
/// coordinate, then publishes a VECTOR reservation for the nested world-color
/// query. Retains the local cursor slot across the query and releases the sample;
/// its fourth word stays uninitialized.
static void _actor07000SlouchUpdateColor(Task* task)
{
    GfxCoord* sampleCoord;
    void**    cursorSlot;
    VECTOR*   samplePosition;
    Enemy*    enemy;

    enemy          = task->spawnArg2.pointer;
    sampleCoord    = &task->extra.tmd->coords[1];
    cursorSlot     = SCRATCH_STACK_CURSOR_SLOT;
    samplePosition = SCRATCH_HEAD_AT(cursorSlot, VECTOR) - 1;
    // Publish the reservation after sampling, before the nested lighting query.
    samplePosition->vx                  = sampleCoord->workm.t[0];
    samplePosition->vy                  = sampleCoord->workm.t[1];
    samplePosition->vz                  = sampleCoord->workm.t[2];
    SCRATCH_HEAD_AT(cursorSlot, VECTOR) = samplePosition;
    worldCoordUpdateActorColor(enemy, samplePosition, 0, 0);
    SCRATCH_POP_BYTES_AT(cursorSlot, sizeof(*samplePosition));
}

/// Inserts the Slouch's death-scaling coordinate between root and model part 1.
///
/// Requires initialized Slouch work and the live model. Starts the inserted
/// rotation and three scale components at 4096 = unity, invalidates it and part
/// 1, and clears the phase counter. Requests two corpse-fire bursts unless the
/// behavior is puffing death. The death handler later restores part 1's parent;
/// this coordinate remains owned by the task's work throughout.
static void _actor07000SlouchInitDeathCoord(Task* task)
{
    enum { ACTOR_07000_SLOUCH_CORPSE_FIRE_BURSTS = 2 };
    GfxCoord*              modelCoords;
    GfxCoord*              deathCoord;
    _Actor07000SlouchWork* work;

    work                  = task->work;
    modelCoords           = task->extra.tmd->coords;
    deathCoord            = &work->deathCoord;
    deathCoord->parent    = modelCoords;
    modelCoords[1].parent = deathCoord;
    gfxSetRotIdentity(&deathCoord->coord);
    deathCoord->composeStamp    = GRAPHICS_COORD_DIRTY;
    modelCoords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    work->deathScale.vx         = ONE;
    work->deathScale.vy         = ONE;
    work->deathScale.vz         = ONE;
    work->stateFrames           = 0;
    if (work->state != ACTOR_07000_SLOUCH_STATE_PUFFING_DEATH) {
        effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords, ACTOR_07000_SLOUCH_CORPSE_FIRE_BURSTS, NULL);
    }
}

/// Compounds the dying Slouch's Y flattening on its inserted death coordinate.
///
/// Requires the initialized death coordinate between root and model part 1.
/// Subtracts two from the signed 12-fractional-bit Y scale, narrows to a
/// halfword, then multiplies the existing Y column by that scale and marks
/// composition dirty. Each tick scales the column left by the preceding tick.
static void _actor07000SlouchFlatten(Task* task)
{
    enum { ACTOR_07000_SLOUCH_SCALE_FRACTION_BITS = 12 };
    u16                    nextYScale;
    GfxCoord*              deathCoord;
    _Actor07000SlouchWork* work;

    work                          = task->work;
    deathCoord                    = &work->deathCoord;
    nextYScale                    = work->deathScale.vy - 2;
    work->deathScale.vy           = nextYScale;
    deathCoord->coord.m[0][1]     = (s16)((s32)(deathCoord->coord.m[0][1] * (s16)nextYScale) >> ACTOR_07000_SLOUCH_SCALE_FRACTION_BITS);
    deathCoord->coord.m[1][1]     = (s16)((s32)(deathCoord->coord.m[1][1] * work->deathScale.vy) >> ACTOR_07000_SLOUCH_SCALE_FRACTION_BITS);
    deathCoord->coord.m[2][1]     = (s16)((s32)(deathCoord->coord.m[2][1] * work->deathScale.vy) >> ACTOR_07000_SLOUCH_SCALE_FRACTION_BITS);
    work->deathCoord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies the Slouch's strike stretch to model part 5.
///
/// Requires its seven-part animated model. A nonzero stretch adds to the X
/// column's scale and a quarter of it to Y/Z, with 4096 representing unity;
/// transforms retain GTE saturation and halfword stores. Marks that part dirty
/// and leaves its translation intact.
static void _actor07000SlouchStretch(Task* task)
{
    /// Scales one local rotation column by a signed 12-fractional-bit stretch.
    ///
    /// `column` must be a compile-time constant in 0..2. `matrix` is evaluated
    /// twice and `columnVector` four times: supply stable live pointers with no
    /// side effects. `scaleDelta` is evaluated once after reading the column;
    /// 4096 is unity. Translation and composition stamps stay unchanged.
#define ACTOR_07000_SLOUCH_SCALE_PART_COLUMN(matrix, column, scaleDelta, columnVector) \
    do {                                                                               \
        gte_ReadMatrixColumn((matrix), (column), (columnVector));                      \
        gte_lddp((scaleDelta) + ONE);                                                  \
        gte_ldsv((columnVector));                                                      \
        gte_gpf12();                                                                   \
        gte_stsv((columnVector));                                                      \
        gte_WriteMatrixColumn((columnVector), (matrix), (column));                     \
    } while (0)
    SVECTOR                columnVector;
    MATRIX*                partMatrix;
    _Actor07000SlouchWork* work;
    GfxCoord*              coords;

    work   = task->work;
    coords = task->extra.tmd->coords;
    if (work->stretch != 0) {
        partMatrix = &coords[5].coord;
        ACTOR_07000_SLOUCH_SCALE_PART_COLUMN(partMatrix, 0, work->stretch, &columnVector);

        ACTOR_07000_SLOUCH_SCALE_PART_COLUMN(partMatrix, 1, work->stretch >> 2, &columnVector);

        ACTOR_07000_SLOUCH_SCALE_PART_COLUMN(partMatrix, 2, work->stretch >> 2, &columnVector);

        coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    }
#undef ACTOR_07000_SLOUCH_SCALE_PART_COLUMN
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
    angle     = _actor07000SlouchMeasurePlayer(coords, &dist);
    offset.vz = 0;
    offset.vy = 0;
    offset.vx = 0;
    task      = taskSpawnFromTable(Actor07000_D0D7D0, 1, angle, 0);
    if (task != NULL) {
        actorRenderCopyCoordBodyTransform(task, child, &offset);
        taskReparent(arg0, task);
    }
}

/// Dispatches the Slouch projectile's launch, flight or impact-expiry state.
///
/// Requires a live coordinate-body task with state in 0..2. Copies all three
/// callbacks to the stack before dispatch. Launch supplies its work; expiry or
/// allocation failure can release the task during the call.
static void _actor07000SlouchProjectileTask(Task* task)
{
    TaskFuncTable3 states;

    states = Actor07000_D000E0;
    states.funcs[task->state](task);
}

/// Composes the Slouch's reaction twist onto model parts 3 and 5.
///
/// Angles use 4096 units per turn. Each tick right-multiplies both local
/// rotations by the twist, then decreases a nonzero X twist by 32 or clears
/// it and its active latch when below 33. Translation and composition stamps
/// are left to the caller. Requires live work/model and initialized scratch
/// storage, released before return; changes GTE state.
static void _actor07000SlouchApplyTwist(Task* task)
{
    enum { ACTOR_07000_SLOUCH_TWIST_DECAY = 32 };
    _Actor07000SlouchWork* work;
    GfxCoord*              coords;
    MATRIX*                twistMatrix;
    s16                    twistX;

    work        = task->work;
    twistMatrix = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    coords      = task->extra.tmd->coords;
    _actor07000SlouchTwistPart(&coords[3].coord, &work->twist, twistMatrix);
    _actor07000SlouchTwistPart(&coords[5].coord, &work->twist, twistMatrix);
    twistX = work->twist.vx;
    if (twistX != 0) {
        if (twistX < ACTOR_07000_SLOUCH_TWIST_DECAY + 1) {
            work->twist.vx    = 0;
            work->twistActive = 0;
        } else {
            work->twist.vx -= ACTOR_07000_SLOUCH_TWIST_DECAY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Consumes Slouch status flags and applies a pending damage-over-time tick.
///
/// Clears stagger, starts the buildup hold, and steps damage over time in that
/// order. A damage tick selects idle status flinch even if it also starts task
/// death; expiry clears the damage-over-time flags. Requires live enemy/work.
static void _actor07000SlouchTickReactions(Task* task)
{
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;
    s32                    damage;
    u8                     reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags != 0) {
        if (reactionFlags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->state           = ACTOR_07000_SLOUCH_STATE_STATUS_HOLD;
            work->stateFrames     = 0;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            damage = damageTickEnemyDamageOverTime(enemy);
            if (damage != 0) {
                _actor07000SlouchTakeDamage(task, damage);
                work->state  = ACTOR_07000_SLOUCH_STATE_IDLE;
                work->animId = ACTOR_07000_SLOUCH_ANIM_STATUS_FLINCH;
            }
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}

/// Gives a detached Slouch body-part model its source model's texture placement.
///
/// Borrows live TMD bodies from both tasks, copying page and CLUT-row offsets.
/// Rebuilds both primitive-buffer halves when a buffer exists; their capacity
/// and geometry remain unchanged. GPU use of the writable buffer must be over.
static void _actor07000SlouchCopyTexturePlacement(Task* destinationTask, Task* sourceTask)
{
    TmdObject* destinationModel;
    TmdObject* sourceModel;

    sourceModel                         = sourceTask->extra.tmd;
    destinationModel                    = destinationTask->extra.tmd;
    destinationModel->texturePageOffset = sourceModel->texturePageOffset;
    destinationModel->clutRowOffset     = sourceModel->clutRowOffset;
    if (destinationModel->buffer != NULL) {
        tmdBuildBufferHalf(destinationModel);
        tmdBuildBufferHalf(destinationModel);
    }
}

/// Unlinks the Slouch's target and three collision bodies before enemy teardown.
///
/// Requires live enemy and initialized work, including during death after the
/// same nodes have already been unlinked. Marks the target unavailable and
/// clears its borrowed contact pointer before common teardown releases storage.
static void _actor07000SlouchExit(Task* task)
{
    _Actor07000SlouchWork* work;
    Enemy*                 enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = NULL;
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

/// Saves the Slouch's root position and takes one forward/downward drop step.
///
/// Requires live model/work. Adds facing-axis X/Z times signed forwardSpeed,
/// with 12 fractional bits in the rotation column, and signed fallSpeed to Y;
/// speeds are game units per tick. Keeps the full preceding XYZ for collision
/// rollback. The caller controls freezing, acceleration and composition stamps.
static void _actor07000SlouchFallStep(Task* task)
{
    enum { ACTOR_07000_SLOUCH_FALL_AXIS_FRACTION_BITS = 12 };
    GfxCoord*              rootCoord;
    _Actor07000SlouchWork* work;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    work->prevRootPos.vx = rootCoord->coord.t[0];
    work->prevRootPos.vy = rootCoord->coord.t[1];
    work->prevRootPos.vz = rootCoord->coord.t[2];

    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> ACTOR_07000_SLOUCH_FALL_AXIS_FRACTION_BITS;
    rootCoord->coord.t[1] += work->fallSpeed;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> ACTOR_07000_SLOUCH_FALL_AXIS_FRACTION_BITS;
}

/// Exits an impacted Slouch projectile when its signed-halfword countdown ends.
///
/// Subtracts one and narrows before testing for zero or negative remaining
/// ticks. This state advances independently of the actor-control freeze modes;
/// the exit callback unlinks the projectile body and releases the task.
static void _actor07000SlouchProjectileExpire(Task* task)
{
    s16 remainingFrames;

    remainingFrames     = task->killCountdown - 1;
    task->killCountdown = remainingFrames;
    if (remainingFrames <= 0) {
        taskCallExit(task);
    }
}

/// Unlinks the Slouch projectile's collision body before task teardown.
///
/// Requires its allocated live work and linked or already unlinked body. Common
/// task teardown dispatches the glow child's exit and releases coordinate/work
/// storage; the caller must not access the task after this callback.
static void _actor07000SlouchProjectileExit(Task* task)
{
    _Actor07000SlouchProjectileWork* work;

    work = task->work;
    worldCollisionUnlinkBody(&work->body);
    taskKill(task);
}
