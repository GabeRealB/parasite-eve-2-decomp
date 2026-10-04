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
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

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
#define DESERT_CHASER_BUILD DESERT_CHASER_CUTSCENE
#include "../../shared/desert_chaser.h"

/// Psy-Q `RotMatrixY`.

/// Whole-unit step `ActorContact_PushContact` last applied to its coordinate.
static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

/// Per-state animation table `desertChaserAnimTick` reads when it
/// re-seeds the slots: 0x2D bytes per `field_82C`, indexed by `field_82E`.
extern s8 gDesertChaserClipStartFrames[];

/// Animation source `animationInitContext` is handed for both of the work block's
/// contexts.
extern u8 gRigAnimSource[];

/// Message table published as `Task::msgTable` by the spawn handler.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gRigMessages[7];

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    EffectSpawnArg value;
    u8             retained[88];
} Actor323400Storage1228;
STATIC_ASSERT_SIZEOF(Actor323400Storage1228, 96);

extern Actor323400Storage1228 gRigEffectRec;

/// Enemy parameters the spawn handler stores in `Enemy::param`.
extern EnemyParams gRigParams;

static void func_actor_323400_801641C4(Enemy* enemy, Task* task);
static void func_actor_323400_80164BD0(Enemy* enemy, Task* task);
static void func_actor_323400_80164C4C(Enemy* enemy, Task* task);

/// State handlers `desertChaserFrameState` runs by `DesertChaserWork::field_0`.
#include "../../shared/actor_contacts.h"

static const EnemyTaskFuncTable4 gDesertChaserStates = {
    desertChaserHideState,
    func_actor_323400_80164BD0,
    func_actor_323400_801641C4,
    func_actor_323400_80164C4C,
};

/// Task states `desertChaserTask` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `enemyDestroy`.
static const DesertChaserTaskStates gDesertChaserTaskStates = {
    desertChaserSpawn,
    desertChaserFrameState,
    enemyDestroy,
};

static TmdSource _gActor323400DesertChaserBody;
s32              func_actor_323400_80164974(Task* task, s32 msgId, ActorCommand* msg, s32);
s32              func_actor_323400_8016475C(Task*, s32, s32, s32);

DamageAttack D_actor_323400_80164D48[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams gRigParams = { D_actor_323400_80164D48, 200, 75, 50, 4, 100, 10, 100, 0 };

s16 D_actor_323400_80164D6C[16] = {
    60,
    36,
    10,
    150,
    40,
    26,
    10,
    120,
    20,
    18,
    10,
    120,
    60,
    60,
    10,
    150,
};

static TmdBone _gActor323400DesertChaserBodySkeleton[18] = {
#include "assets/desert_chaser_body_skeleton.inc"
};

static u32 _gActor323400DesertChaserBodyPartVerts[18] = {
#include "assets/desert_chaser_body_partVerts.inc"
};

static SVECTOR _gActor323400DesertChaserBodyVerts[266] = {
#include "assets/desert_chaser_body_verts.inc"
};

static SVECTOR _gActor323400DesertChaserBodyNormals[324] = {
#include "assets/desert_chaser_body_normals.inc"
};

static u32 _gActor323400DesertChaserBodyStream[3435] = {
#include "assets/desert_chaser_body_stream.inc"
};

static TmdSource _gActor323400DesertChaserBody = {
    0,
    17616,
    5944,
    18,
    _gActor323400DesertChaserBodyPartVerts,
    _gActor323400DesertChaserBodyVerts,
    _gActor323400DesertChaserBodyNormals,
    _gActor323400DesertChaserBodySkeleton,
    _gActor323400DesertChaserBodyStream,
};

static AnimationPackedPose _gActor323400Animation07F8CBank1[10] = {
#include "assets/actor_323400_animation_07F8C_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation07F8CBank4[110] = {
#include "assets/actor_323400_animation_07F8C_bank4.inc"
};

static AnimationRecord _gActor323400Animation07F8CRecords[175] = {
#include "assets/actor_323400_animation_07F8C_records.inc"
};

static u16 _gActor323400Animation07F8CIndices[18] = {
#include "assets/actor_323400_animation_07F8C_indices.inc"
};

static AnimationSet _gActor323400Animation07F8C = {
    _gActor323400Animation07F8CRecords,
    _gActor323400Animation07F8CIndices,
    { NULL, _gActor323400Animation07F8CBank1, NULL, NULL, _gActor323400Animation07F8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation08248Bank1[5] = {
#include "assets/actor_323400_animation_08248_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation08248Bank4[29] = {
#include "assets/actor_323400_animation_08248_bank4.inc"
};

static AnimationRecord _gActor323400Animation08248Records[112] = {
#include "assets/actor_323400_animation_08248_records.inc"
};

static u16 _gActor323400Animation08248Indices[18] = {
#include "assets/actor_323400_animation_08248_indices.inc"
};

static AnimationSet _gActor323400Animation08248 = {
    _gActor323400Animation08248Records,
    _gActor323400Animation08248Indices,
    { NULL, _gActor323400Animation08248Bank1, NULL, NULL, _gActor323400Animation08248Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0887CBank1[13] = {
#include "assets/actor_323400_animation_0887C_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0887CBank4[129] = {
#include "assets/actor_323400_animation_0887C_bank4.inc"
};

static AnimationRecord _gActor323400Animation0887CRecords[210] = {
#include "assets/actor_323400_animation_0887C_records.inc"
};

static u16 _gActor323400Animation0887CIndices[18] = {
#include "assets/actor_323400_animation_0887C_indices.inc"
};

static AnimationSet _gActor323400Animation0887C = {
    _gActor323400Animation0887CRecords,
    _gActor323400Animation0887CIndices,
    { NULL, _gActor323400Animation0887CBank1, NULL, NULL, _gActor323400Animation0887CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation08E54Bank1[11] = {
#include "assets/actor_323400_animation_08E54_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation08E54Bank4[131] = {
#include "assets/actor_323400_animation_08E54_bank4.inc"
};

static AnimationRecord _gActor323400Animation08E54Records[191] = {
#include "assets/actor_323400_animation_08E54_records.inc"
};

static u16 _gActor323400Animation08E54Indices[18] = {
#include "assets/actor_323400_animation_08E54_indices.inc"
};

static AnimationSet _gActor323400Animation08E54 = {
    _gActor323400Animation08E54Records,
    _gActor323400Animation08E54Indices,
    { NULL, _gActor323400Animation08E54Bank1, NULL, NULL, _gActor323400Animation08E54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation09288Bank1[9] = {
#include "assets/actor_323400_animation_09288_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation09288Bank4[83] = {
#include "assets/actor_323400_animation_09288_bank4.inc"
};

static AnimationRecord _gActor323400Animation09288Records[140] = {
#include "assets/actor_323400_animation_09288_records.inc"
};

static u16 _gActor323400Animation09288Indices[18] = {
#include "assets/actor_323400_animation_09288_indices.inc"
};

static AnimationSet _gActor323400Animation09288 = {
    _gActor323400Animation09288Records,
    _gActor323400Animation09288Indices,
    { NULL, _gActor323400Animation09288Bank1, NULL, NULL, _gActor323400Animation09288Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation09960Bank1[19] = {
#include "assets/actor_323400_animation_09960_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation09960Bank4[138] = {
#include "assets/actor_323400_animation_09960_bank4.inc"
};

static AnimationRecord _gActor323400Animation09960Records[224] = {
#include "assets/actor_323400_animation_09960_records.inc"
};

static u16 _gActor323400Animation09960Indices[18] = {
#include "assets/actor_323400_animation_09960_indices.inc"
};

static AnimationSet _gActor323400Animation09960 = {
    _gActor323400Animation09960Records,
    _gActor323400Animation09960Indices,
    { NULL, _gActor323400Animation09960Bank1, NULL, NULL, _gActor323400Animation09960Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation09F60Bank1[14] = {
#include "assets/actor_323400_animation_09F60_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation09F60Bank4[99] = {
#include "assets/actor_323400_animation_09F60_bank4.inc"
};

static AnimationRecord _gActor323400Animation09F60Records[224] = {
#include "assets/actor_323400_animation_09F60_records.inc"
};

static u16 _gActor323400Animation09F60Indices[18] = {
#include "assets/actor_323400_animation_09F60_indices.inc"
};

static AnimationSet _gActor323400Animation09F60 = {
    _gActor323400Animation09F60Records,
    _gActor323400Animation09F60Indices,
    { NULL, _gActor323400Animation09F60Bank1, NULL, NULL, _gActor323400Animation09F60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0A1BCBank1[4] = {
#include "assets/actor_323400_animation_0A1BC_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0A1BCBank4[34] = {
#include "assets/actor_323400_animation_0A1BC_bank4.inc"
};

static AnimationRecord _gActor323400Animation0A1BCRecords[86] = {
#include "assets/actor_323400_animation_0A1BC_records.inc"
};

static u16 _gActor323400Animation0A1BCIndices[18] = {
#include "assets/actor_323400_animation_0A1BC_indices.inc"
};

static AnimationSet _gActor323400Animation0A1BC = {
    _gActor323400Animation0A1BCRecords,
    _gActor323400Animation0A1BCIndices,
    { NULL, _gActor323400Animation0A1BCBank1, NULL, NULL, _gActor323400Animation0A1BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0A7E0Bank1[12] = {
#include "assets/actor_323400_animation_0A7E0_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0A7E0Bank4[147] = {
#include "assets/actor_323400_animation_0A7E0_bank4.inc"
};

static AnimationRecord _gActor323400Animation0A7E0Records[191] = {
#include "assets/actor_323400_animation_0A7E0_records.inc"
};

static u16 _gActor323400Animation0A7E0Indices[18] = {
#include "assets/actor_323400_animation_0A7E0_indices.inc"
};

static AnimationSet _gActor323400Animation0A7E0 = {
    _gActor323400Animation0A7E0Records,
    _gActor323400Animation0A7E0Indices,
    { NULL, _gActor323400Animation0A7E0Bank1, NULL, NULL, _gActor323400Animation0A7E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0AC30Bank1[9] = {
#include "assets/actor_323400_animation_0AC30_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0AC30Bank4[73] = {
#include "assets/actor_323400_animation_0AC30_bank4.inc"
};

static AnimationRecord _gActor323400Animation0AC30Records[157] = {
#include "assets/actor_323400_animation_0AC30_records.inc"
};

static u16 _gActor323400Animation0AC30Indices[18] = {
#include "assets/actor_323400_animation_0AC30_indices.inc"
};

static AnimationSet _gActor323400Animation0AC30 = {
    _gActor323400Animation0AC30Records,
    _gActor323400Animation0AC30Indices,
    { NULL, _gActor323400Animation0AC30Bank1, NULL, NULL, _gActor323400Animation0AC30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0B214Bank1[12] = {
#include "assets/actor_323400_animation_0B214_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0B214Bank4[126] = {
#include "assets/actor_323400_animation_0B214_bank4.inc"
};

static AnimationRecord _gActor323400Animation0B214Records[196] = {
#include "assets/actor_323400_animation_0B214_records.inc"
};

static u16 _gActor323400Animation0B214Indices[18] = {
#include "assets/actor_323400_animation_0B214_indices.inc"
};

static AnimationSet _gActor323400Animation0B214 = {
    _gActor323400Animation0B214Records,
    _gActor323400Animation0B214Indices,
    { NULL, _gActor323400Animation0B214Bank1, NULL, NULL, _gActor323400Animation0B214Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0B390Bank1[2] = {
#include "assets/actor_323400_animation_0B390_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0B390Bank4[16] = {
#include "assets/actor_323400_animation_0B390_bank4.inc"
};

static AnimationRecord _gActor323400Animation0B390Records[54] = {
#include "assets/actor_323400_animation_0B390_records.inc"
};

static u16 _gActor323400Animation0B390Indices[18] = {
#include "assets/actor_323400_animation_0B390_indices.inc"
};

static AnimationSet _gActor323400Animation0B390 = {
    _gActor323400Animation0B390Records,
    _gActor323400Animation0B390Indices,
    { NULL, _gActor323400Animation0B390Bank1, NULL, NULL, _gActor323400Animation0B390Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0BA34Bank1[13] = {
#include "assets/actor_323400_animation_0BA34_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0BA34Bank4[158] = {
#include "assets/actor_323400_animation_0BA34_bank4.inc"
};

static AnimationRecord _gActor323400Animation0BA34Records[209] = {
#include "assets/actor_323400_animation_0BA34_records.inc"
};

static u16 _gActor323400Animation0BA34Indices[18] = {
#include "assets/actor_323400_animation_0BA34_indices.inc"
};

static AnimationSet _gActor323400Animation0BA34 = {
    _gActor323400Animation0BA34Records,
    _gActor323400Animation0BA34Indices,
    { NULL, _gActor323400Animation0BA34Bank1, NULL, NULL, _gActor323400Animation0BA34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0C27CBank1[21] = {
#include "assets/actor_323400_animation_0C27C_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0C27CBank4[193] = {
#include "assets/actor_323400_animation_0C27C_bank4.inc"
};

static AnimationRecord _gActor323400Animation0C27CRecords[254] = {
#include "assets/actor_323400_animation_0C27C_records.inc"
};

static u16 _gActor323400Animation0C27CIndices[20] = {
#include "assets/actor_323400_animation_0C27C_indices.inc"
};

static AnimationSet _gActor323400Animation0C27C = {
    _gActor323400Animation0C27CRecords,
    _gActor323400Animation0C27CIndices,
    { NULL, _gActor323400Animation0C27CBank1, NULL, NULL, _gActor323400Animation0C27CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0CA18Bank1[20] = {
#include "assets/actor_323400_animation_0CA18_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0CA18Bank4[172] = {
#include "assets/actor_323400_animation_0CA18_bank4.inc"
};

static AnimationRecord _gActor323400Animation0CA18Records[235] = {
#include "assets/actor_323400_animation_0CA18_records.inc"
};

static u16 _gActor323400Animation0CA18Indices[20] = {
#include "assets/actor_323400_animation_0CA18_indices.inc"
};

static AnimationSet _gActor323400Animation0CA18 = {
    _gActor323400Animation0CA18Records,
    _gActor323400Animation0CA18Indices,
    { NULL, _gActor323400Animation0CA18Bank1, NULL, NULL, _gActor323400Animation0CA18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0D224Bank1[15] = {
#include "assets/actor_323400_animation_0D224_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0D224Bank4[206] = {
#include "assets/actor_323400_animation_0D224_bank4.inc"
};

static AnimationRecord _gActor323400Animation0D224Records[244] = {
#include "assets/actor_323400_animation_0D224_records.inc"
};

static u16 _gActor323400Animation0D224Indices[20] = {
#include "assets/actor_323400_animation_0D224_indices.inc"
};

static AnimationSet _gActor323400Animation0D224 = {
    _gActor323400Animation0D224Records,
    _gActor323400Animation0D224Indices,
    { NULL, _gActor323400Animation0D224Bank1, NULL, NULL, _gActor323400Animation0D224Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0DA38Bank1[14] = {
#include "assets/actor_323400_animation_0DA38_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0DA38Bank4[207] = {
#include "assets/actor_323400_animation_0DA38_bank4.inc"
};

static AnimationRecord _gActor323400Animation0DA38Records[248] = {
#include "assets/actor_323400_animation_0DA38_records.inc"
};

static u16 _gActor323400Animation0DA38Indices[20] = {
#include "assets/actor_323400_animation_0DA38_indices.inc"
};

static AnimationSet _gActor323400Animation0DA38 = {
    _gActor323400Animation0DA38Records,
    _gActor323400Animation0DA38Indices,
    { NULL, _gActor323400Animation0DA38Bank1, NULL, NULL, _gActor323400Animation0DA38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0DCECBank1[5] = {
#include "assets/actor_323400_animation_0DCEC_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0DCECBank4[55] = {
#include "assets/actor_323400_animation_0DCEC_bank4.inc"
};

static AnimationRecord _gActor323400Animation0DCECRecords[83] = {
#include "assets/actor_323400_animation_0DCEC_records.inc"
};

static u16 _gActor323400Animation0DCECIndices[20] = {
#include "assets/actor_323400_animation_0DCEC_indices.inc"
};

static AnimationSet _gActor323400Animation0DCEC = {
    _gActor323400Animation0DCECRecords,
    _gActor323400Animation0DCECIndices,
    { NULL, _gActor323400Animation0DCECBank1, NULL, NULL, _gActor323400Animation0DCECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0E00CBank1[6] = {
#include "assets/actor_323400_animation_0E00C_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0E00CBank4[66] = {
#include "assets/actor_323400_animation_0E00C_bank4.inc"
};

static AnimationRecord _gActor323400Animation0E00CRecords[96] = {
#include "assets/actor_323400_animation_0E00C_records.inc"
};

static u16 _gActor323400Animation0E00CIndices[20] = {
#include "assets/actor_323400_animation_0E00C_indices.inc"
};

static AnimationSet _gActor323400Animation0E00C = {
    _gActor323400Animation0E00CRecords,
    _gActor323400Animation0E00CIndices,
    { NULL, _gActor323400Animation0E00CBank1, NULL, NULL, _gActor323400Animation0E00CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323400Animation0EA4CBank1[20] = {
#include "assets/actor_323400_animation_0EA4C_bank1.inc"
};

static AnimationPackedRotation _gActor323400Animation0EA4CBank4[247] = {
#include "assets/actor_323400_animation_0EA4C_bank4.inc"
};

static AnimationRecord _gActor323400Animation0EA4CRecords[330] = {
#include "assets/actor_323400_animation_0EA4C_records.inc"
};

static u16 _gActor323400Animation0EA4CIndices[18] = {
#include "assets/actor_323400_animation_0EA4C_indices.inc"
};

static AnimationSet _gActor323400Animation0EA4C = {
    _gActor323400Animation0EA4CRecords,
    _gActor323400Animation0EA4CIndices,
    { NULL, _gActor323400Animation0EA4CBank1, NULL, NULL, _gActor323400Animation0EA4CBank4, NULL, NULL, NULL },
};

s8 gDesertChaserClipStartFrames[2028] = {
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    5,
    5,
    5,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    7,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

u8 gRigAnimSource[340] = {
    172,
    157,
    22,
    128,
    104,
    160,
    22,
    128,
    156,
    166,
    22,
    128,
    116,
    172,
    22,
    128,
    168,
    176,
    22,
    128,
    128,
    183,
    22,
    128,
    128,
    189,
    22,
    128,
    220,
    191,
    22,
    128,
    0,
    198,
    22,
    128,
    80,
    202,
    22,
    128,
    52,
    208,
    22,
    128,
    176,
    209,
    22,
    128,
    84,
    216,
    22,
    128,
    108,
    8,
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
    156,
    224,
    22,
    128,
    68,
    240,
    22,
    128,
    12,
    251,
    22,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    56,
    232,
    22,
    128,
    88,
    248,
    22,
    128,
    44,
    254,
    22,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    60,
    0,
    244,
    255,
    30,
    0,
    2,
    0,
    206,
    255,
    126,
    255,
    29,
    0,
    2,
    0,
    20,
    0,
    186,
    255,
    25,
    0,
    2,
    0,
    226,
    255,
    191,
    255,
    25,
    0,
    2,
    0,
    60,
    0,
    136,
    255,
    30,
    0,
    2,
    0,
    20,
    0,
    236,
    255,
    251,
    255,
    2,
    0,
    241,
    255,
    206,
    255,
    0,
    0,
    2,
    0,
    2,
    0,
    10,
    0,
    241,
    255,
    2,
    0,
    14,
    0,
    0,
    0,
    0,
    0,
    7,
    0,
    25,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    242,
    255,
    0,
    0,
    0,
    0,
    9,
    0,
    231,
    255,
    0,
    0,
    0,
    0,
    2,
    0,
};

TaskMessageEntry gRigMessages[7] = {
    { 2015, func_actor_323400_8016475C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, desertChaserSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_323400_80164974 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, desertChaserMsgPlayAnim },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_323400_8017120C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, desertChaserTask, { .model = &_gActor323400DesertChaserBody } };

static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &(ActorContact_ScratchPosition.step);
}

Actor323400Storage1228 gRigEffectRec;

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/desert_chaser_blend_tick.inc.c"

/// Per-frame effect dispatch keyed on `field_82E` and the record each animation
/// slot has reached. A recognised record is handled once: `field_848` remembers,
/// per slot, the record last handled, and meeting it again only clears `reset`.
/// A handled record spawns its effects while the room effect mode is 2 and
/// returns a request word; otherwise the result is 0, after wiping `field_848`
/// when no case claimed a record.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 desertChaserAnimCues(Task* task, DesertChaserWork* work)
{
    SVECTOR vec;
    s32     reset;
    s32     steer;
    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002220, &vec);
                    }
                    return 0x40010002;
                }
                work->field_848[9] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[7].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0x3E) {
                    old = work->field_848[7];
                    if (old != clip) {
                        work->field_848[7] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[7] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0x84) {
                    old = work->field_848[14];
                    if (old != clip) {
                        work->field_848[14] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80002220, &vec);
                        }
                        return 0x40010002;
                    }
                    work->field_848[14] = old;
                    reset               = 0;
                }
            }
            {
                s32 clip = work->slots[17].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xA9) {
                    old = work->field_848[17];
                    if (old != clip) {
                        work->field_848[17] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[17] = old;
                    reset               = 0;
                }
            }
            break;

        case 10: {
            s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x9) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, task->extra.tmd->coords, 0x80004A00, &vec);
                    }
                    return 0x40010005;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        } break;

        case 3: {
            s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x4) {
                reset = 0;
                old   = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    return 0x40010004;
                }
                work->field_848[1] = old;
            }
        }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0x8) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        return 0x40010003;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;

        case 6: {
            s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;
            if (clip == 0x6) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80003200, &vec);
                    }
                    vec.vz = 0;
                    vec.vx = 0;
                    vec.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80003200, &vec);
                    }
                    return 0;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xB) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80004480, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80004480, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xC) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002200, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x2BC;
                        if (steer == 0 && gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002240, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (steer == 0 && gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80003300, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80003340, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;
                if (clip == 0xD) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80002200, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80002300, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;
    }

    if (reset == 1) {
        memFillBytes(work->field_848, 0, sizeof(work->field_848));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

#include "../../shared/desert_chaser_spawn.inc.c"

/// State 2 of `gDesertChaserStates`. On entry it flags the enemy's link
/// node, shows the model (clears its flags) and rebuilds its buffers, resets
/// the slots to clip 0xD and zeroes the frame counter `field_6` before the
/// tick. Otherwise it advances `field_6` and, on frames 9, 10, 12 and 13,
/// spawns effect 0x60054 at the matching model part while the room's effect
/// mode is 2 (0x2BC up at parts 9 and 7, 0x258 up at 14 and 17); frame 10
/// also plays a placed sound, and frame 13 always spawns one more at part 1.
/// The tick then runs and the root coordinate is marked for rebuilding.
static void func_actor_323400_801641C4(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    s32               id;
    s32               pan;
    SVECTOR           ofs2;
    SVECTOR           ofs;

    work = (DesertChaserWork*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        desertChaserAnimTick(task);
        return;
    }
    switch (++work->field_6) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000E;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    desertChaserAnimTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/desert_chaser_frame.inc.c"

s32 func_actor_323400_8016475C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

#include "../../shared/desert_chaser_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw_first.inc.c"

/// Handler for message 0x7DB: copies the payload's three leading bytes into
/// the work block and, when its `code` is 0x1602, picks the state from `mode`:
/// 1 moves the root coordinate to (0x4330, 1, 0xA8C), marks it for rebuilding
/// and starts state 2; 0 and 2 restart state 0; any other mode only stores the
/// bytes.
s32 func_actor_323400_80164974(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    DesertChaserWork* work;
    u16               mode;

    work = (DesertChaserWork*)task->work;

    work->field_91C = msg->context.loc.stage;
    work->field_91D = msg->context.loc.area;
    work->field_91E = (u8)msg->command;

    if (msg->context.key == 0x1602) {
        mode = msg->command;
        switch (mode) {
            case 1:
                task->extra.tmd->coords->coord.t[0]   = 0x4330;
                task->extra.tmd->coords->coord.t[1]   = mode;
                task->extra.tmd->coords->coord.t[2]   = 0xA8C;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->field_0                         = 2;
                break;
            case 0:
            case 2:
                work->field_0 = 0;
                break;
        }
    }
    return 0;
}

#include "../../shared/desert_chaser_play_anim.inc.c"

#include "../../shared/desert_chaser_exit.inc.c"

#include "../../shared/desert_chaser_part_effect.inc.c"

#include "../../shared/desert_chaser_hide.inc.c"

/// State 1 of `gDesertChaserStates`: on entry clears the enemy's link-node
/// flags, shows the model (clears its flags), rebuilds its buffers and resets
/// the slots to the current clip `field_82E` with both turn targets zeroed.
/// The tick runs every frame.
static void func_actor_323400_80164BD0(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;

    work = (DesertChaserWork*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        desertChaserAnimTick(task);
    } else {
        desertChaserAnimTick(task);
    }
}

/// State 3 of `gDesertChaserStates`: on entry flags the enemy's link node,
/// shows the model (clears its flags), rebuilds its buffers and re-seeds the
/// slots with clip 2 from the per-state table, with both turn targets zeroed.
/// On later frames the tick runs and the root coordinate is marked for
/// rebuilding.
static void func_actor_323400_80164C4C(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;

    work = (DesertChaserWork*)task->work;
    if (work->field_4 != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 2;
        work->field_828 = 1;
        work->field_83E = 0;
        work->field_840 = 0;
        desertChaserAnimTick(task);
    } else {
        desertChaserAnimTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

#include "../../shared/desert_chaser_task.inc.c"
