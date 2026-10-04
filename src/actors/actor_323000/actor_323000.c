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

/// Animation source `animationInitContext` is handed for both of the work block's
/// contexts.
extern u8 gRigAnimSource[];

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    EffectSpawnArg value;
    u8             retained[88];
} Actor323000Storage3A24;
STATIC_ASSERT_SIZEOF(Actor323000Storage3A24, 96);

extern Actor323000Storage3A24 gRigEffectRec;

/// Message table published as `Task::msgTable` by the spawn handler.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gRigMessages[7];

/// Enemy parameters the spawn handler stores in `Enemy::param`.
extern EnemyParams gRigParams;

/// Whole-unit step `ActorContact_PushContact` last applied to its coordinate.
static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

/// Per-state animation table `desertChaserAnimTick` reads when it
/// re-seeds the slots: 0x2D bytes per `appliedAnim`, indexed by `animId`.
extern s8 gDesertChaserClipStartFrames[];

/// Psy-Q `RotMatrixY`.

static void func_actor_323000_8016409C(Enemy* enemy, Task* task);
static void func_actor_323000_8016420C(Enemy* enemy, Task* task);
static void func_actor_323000_80164C58(Enemy* enemy, Task* task);

/// State handlers `desertChaserFrameState` runs by `DesertChaserWork::state`.
#include "../../shared/actor_contacts.h"

static const EnemyTaskFuncTable4 gDesertChaserStates = {
    desertChaserHideState,
    func_actor_323000_8016409C,
    func_actor_323000_80164C58,
    func_actor_323000_8016420C,
};

/// Task states `desertChaserTask` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `enemyDestroy`.
static const DesertChaserTaskStates gDesertChaserTaskStates = {
    desertChaserSpawn,
    desertChaserFrameState,
    enemyDestroy,
};

static TmdSource _gActor323000DesertChaserBody;
s32              func_actor_323000_80164A54(Task* task, s32 msgId, ActorCommand* msg, s32);
s32              func_actor_323000_8016483C(Task*, s32, s32, s32);

DamageAttack D_actor_323000_80164D40[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams gRigParams = { D_actor_323000_80164D40, 200, 75, 50, 4, 100, 10, 100, 0 };

s16 D_actor_323000_80164D64[16] = {
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

static TmdBone _gActor323000DesertChaserBodySkeleton[18] = {
#include "assets/desert_chaser_body_skeleton.inc"
};

static u32 _gActor323000DesertChaserBodyPartVerts[18] = {
#include "assets/desert_chaser_body_partVerts.inc"
};

static SVECTOR _gActor323000DesertChaserBodyVerts[266] = {
#include "assets/desert_chaser_body_verts.inc"
};

static SVECTOR _gActor323000DesertChaserBodyNormals[324] = {
#include "assets/desert_chaser_body_normals.inc"
};

static u32 _gActor323000DesertChaserBodyStream[3435] = {
#include "assets/desert_chaser_body_stream.inc"
};

static TmdSource _gActor323000DesertChaserBody = {
    0,
    17616,
    5944,
    18,
    _gActor323000DesertChaserBodyPartVerts,
    _gActor323000DesertChaserBodyVerts,
    _gActor323000DesertChaserBodyNormals,
    _gActor323000DesertChaserBodySkeleton,
    _gActor323000DesertChaserBodyStream,
};

static AnimationPackedPose _gActor323000Animation07F84Bank1[10] = {
#include "assets/actor_323000_animation_07F84_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation07F84Bank4[110] = {
#include "assets/actor_323000_animation_07F84_bank4.inc"
};

static AnimationRecord _gActor323000Animation07F84Records[175] = {
#include "assets/actor_323000_animation_07F84_records.inc"
};

static u16 _gActor323000Animation07F84Indices[18] = {
#include "assets/actor_323000_animation_07F84_indices.inc"
};

static AnimationSet _gActor323000Animation07F84 = {
    _gActor323000Animation07F84Records,
    _gActor323000Animation07F84Indices,
    { NULL, _gActor323000Animation07F84Bank1, NULL, NULL, _gActor323000Animation07F84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation08240Bank1[5] = {
#include "assets/actor_323000_animation_08240_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation08240Bank4[29] = {
#include "assets/actor_323000_animation_08240_bank4.inc"
};

static AnimationRecord _gActor323000Animation08240Records[112] = {
#include "assets/actor_323000_animation_08240_records.inc"
};

static u16 _gActor323000Animation08240Indices[18] = {
#include "assets/actor_323000_animation_08240_indices.inc"
};

static AnimationSet _gActor323000Animation08240 = {
    _gActor323000Animation08240Records,
    _gActor323000Animation08240Indices,
    { NULL, _gActor323000Animation08240Bank1, NULL, NULL, _gActor323000Animation08240Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation08874Bank1[13] = {
#include "assets/actor_323000_animation_08874_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation08874Bank4[129] = {
#include "assets/actor_323000_animation_08874_bank4.inc"
};

static AnimationRecord _gActor323000Animation08874Records[210] = {
#include "assets/actor_323000_animation_08874_records.inc"
};

static u16 _gActor323000Animation08874Indices[18] = {
#include "assets/actor_323000_animation_08874_indices.inc"
};

static AnimationSet _gActor323000Animation08874 = {
    _gActor323000Animation08874Records,
    _gActor323000Animation08874Indices,
    { NULL, _gActor323000Animation08874Bank1, NULL, NULL, _gActor323000Animation08874Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation08E4CBank1[11] = {
#include "assets/actor_323000_animation_08E4C_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation08E4CBank4[131] = {
#include "assets/actor_323000_animation_08E4C_bank4.inc"
};

static AnimationRecord _gActor323000Animation08E4CRecords[191] = {
#include "assets/actor_323000_animation_08E4C_records.inc"
};

static u16 _gActor323000Animation08E4CIndices[18] = {
#include "assets/actor_323000_animation_08E4C_indices.inc"
};

static AnimationSet _gActor323000Animation08E4C = {
    _gActor323000Animation08E4CRecords,
    _gActor323000Animation08E4CIndices,
    { NULL, _gActor323000Animation08E4CBank1, NULL, NULL, _gActor323000Animation08E4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation09280Bank1[9] = {
#include "assets/actor_323000_animation_09280_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation09280Bank4[83] = {
#include "assets/actor_323000_animation_09280_bank4.inc"
};

static AnimationRecord _gActor323000Animation09280Records[140] = {
#include "assets/actor_323000_animation_09280_records.inc"
};

static u16 _gActor323000Animation09280Indices[18] = {
#include "assets/actor_323000_animation_09280_indices.inc"
};

static AnimationSet _gActor323000Animation09280 = {
    _gActor323000Animation09280Records,
    _gActor323000Animation09280Indices,
    { NULL, _gActor323000Animation09280Bank1, NULL, NULL, _gActor323000Animation09280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation09958Bank1[19] = {
#include "assets/actor_323000_animation_09958_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation09958Bank4[138] = {
#include "assets/actor_323000_animation_09958_bank4.inc"
};

static AnimationRecord _gActor323000Animation09958Records[224] = {
#include "assets/actor_323000_animation_09958_records.inc"
};

static u16 _gActor323000Animation09958Indices[18] = {
#include "assets/actor_323000_animation_09958_indices.inc"
};

static AnimationSet _gActor323000Animation09958 = {
    _gActor323000Animation09958Records,
    _gActor323000Animation09958Indices,
    { NULL, _gActor323000Animation09958Bank1, NULL, NULL, _gActor323000Animation09958Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation09F58Bank1[14] = {
#include "assets/actor_323000_animation_09F58_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation09F58Bank4[99] = {
#include "assets/actor_323000_animation_09F58_bank4.inc"
};

static AnimationRecord _gActor323000Animation09F58Records[224] = {
#include "assets/actor_323000_animation_09F58_records.inc"
};

static u16 _gActor323000Animation09F58Indices[18] = {
#include "assets/actor_323000_animation_09F58_indices.inc"
};

static AnimationSet _gActor323000Animation09F58 = {
    _gActor323000Animation09F58Records,
    _gActor323000Animation09F58Indices,
    { NULL, _gActor323000Animation09F58Bank1, NULL, NULL, _gActor323000Animation09F58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0A1B4Bank1[4] = {
#include "assets/actor_323000_animation_0A1B4_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0A1B4Bank4[34] = {
#include "assets/actor_323000_animation_0A1B4_bank4.inc"
};

static AnimationRecord _gActor323000Animation0A1B4Records[86] = {
#include "assets/actor_323000_animation_0A1B4_records.inc"
};

static u16 _gActor323000Animation0A1B4Indices[18] = {
#include "assets/actor_323000_animation_0A1B4_indices.inc"
};

static AnimationSet _gActor323000Animation0A1B4 = {
    _gActor323000Animation0A1B4Records,
    _gActor323000Animation0A1B4Indices,
    { NULL, _gActor323000Animation0A1B4Bank1, NULL, NULL, _gActor323000Animation0A1B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0A7D8Bank1[12] = {
#include "assets/actor_323000_animation_0A7D8_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0A7D8Bank4[147] = {
#include "assets/actor_323000_animation_0A7D8_bank4.inc"
};

static AnimationRecord _gActor323000Animation0A7D8Records[191] = {
#include "assets/actor_323000_animation_0A7D8_records.inc"
};

static u16 _gActor323000Animation0A7D8Indices[18] = {
#include "assets/actor_323000_animation_0A7D8_indices.inc"
};

static AnimationSet _gActor323000Animation0A7D8 = {
    _gActor323000Animation0A7D8Records,
    _gActor323000Animation0A7D8Indices,
    { NULL, _gActor323000Animation0A7D8Bank1, NULL, NULL, _gActor323000Animation0A7D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0AC28Bank1[9] = {
#include "assets/actor_323000_animation_0AC28_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0AC28Bank4[73] = {
#include "assets/actor_323000_animation_0AC28_bank4.inc"
};

static AnimationRecord _gActor323000Animation0AC28Records[157] = {
#include "assets/actor_323000_animation_0AC28_records.inc"
};

static u16 _gActor323000Animation0AC28Indices[18] = {
#include "assets/actor_323000_animation_0AC28_indices.inc"
};

static AnimationSet _gActor323000Animation0AC28 = {
    _gActor323000Animation0AC28Records,
    _gActor323000Animation0AC28Indices,
    { NULL, _gActor323000Animation0AC28Bank1, NULL, NULL, _gActor323000Animation0AC28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0B20CBank1[12] = {
#include "assets/actor_323000_animation_0B20C_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0B20CBank4[126] = {
#include "assets/actor_323000_animation_0B20C_bank4.inc"
};

static AnimationRecord _gActor323000Animation0B20CRecords[196] = {
#include "assets/actor_323000_animation_0B20C_records.inc"
};

static u16 _gActor323000Animation0B20CIndices[18] = {
#include "assets/actor_323000_animation_0B20C_indices.inc"
};

static AnimationSet _gActor323000Animation0B20C = {
    _gActor323000Animation0B20CRecords,
    _gActor323000Animation0B20CIndices,
    { NULL, _gActor323000Animation0B20CBank1, NULL, NULL, _gActor323000Animation0B20CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0B388Bank1[2] = {
#include "assets/actor_323000_animation_0B388_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0B388Bank4[16] = {
#include "assets/actor_323000_animation_0B388_bank4.inc"
};

static AnimationRecord _gActor323000Animation0B388Records[54] = {
#include "assets/actor_323000_animation_0B388_records.inc"
};

static u16 _gActor323000Animation0B388Indices[18] = {
#include "assets/actor_323000_animation_0B388_indices.inc"
};

static AnimationSet _gActor323000Animation0B388 = {
    _gActor323000Animation0B388Records,
    _gActor323000Animation0B388Indices,
    { NULL, _gActor323000Animation0B388Bank1, NULL, NULL, _gActor323000Animation0B388Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0BA2CBank1[13] = {
#include "assets/actor_323000_animation_0BA2C_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0BA2CBank4[158] = {
#include "assets/actor_323000_animation_0BA2C_bank4.inc"
};

static AnimationRecord _gActor323000Animation0BA2CRecords[209] = {
#include "assets/actor_323000_animation_0BA2C_records.inc"
};

static u16 _gActor323000Animation0BA2CIndices[18] = {
#include "assets/actor_323000_animation_0BA2C_indices.inc"
};

static AnimationSet _gActor323000Animation0BA2C = {
    _gActor323000Animation0BA2CRecords,
    _gActor323000Animation0BA2CIndices,
    { NULL, _gActor323000Animation0BA2CBank1, NULL, NULL, _gActor323000Animation0BA2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0C274Bank1[21] = {
#include "assets/actor_323000_animation_0C274_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0C274Bank4[193] = {
#include "assets/actor_323000_animation_0C274_bank4.inc"
};

static AnimationRecord _gActor323000Animation0C274Records[254] = {
#include "assets/actor_323000_animation_0C274_records.inc"
};

static u16 _gActor323000Animation0C274Indices[20] = {
#include "assets/actor_323000_animation_0C274_indices.inc"
};

static AnimationSet _gActor323000Animation0C274 = {
    _gActor323000Animation0C274Records,
    _gActor323000Animation0C274Indices,
    { NULL, _gActor323000Animation0C274Bank1, NULL, NULL, _gActor323000Animation0C274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0CA10Bank1[20] = {
#include "assets/actor_323000_animation_0CA10_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0CA10Bank4[172] = {
#include "assets/actor_323000_animation_0CA10_bank4.inc"
};

static AnimationRecord _gActor323000Animation0CA10Records[235] = {
#include "assets/actor_323000_animation_0CA10_records.inc"
};

static u16 _gActor323000Animation0CA10Indices[20] = {
#include "assets/actor_323000_animation_0CA10_indices.inc"
};

static AnimationSet _gActor323000Animation0CA10 = {
    _gActor323000Animation0CA10Records,
    _gActor323000Animation0CA10Indices,
    { NULL, _gActor323000Animation0CA10Bank1, NULL, NULL, _gActor323000Animation0CA10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0D21CBank1[15] = {
#include "assets/actor_323000_animation_0D21C_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0D21CBank4[206] = {
#include "assets/actor_323000_animation_0D21C_bank4.inc"
};

static AnimationRecord _gActor323000Animation0D21CRecords[244] = {
#include "assets/actor_323000_animation_0D21C_records.inc"
};

static u16 _gActor323000Animation0D21CIndices[20] = {
#include "assets/actor_323000_animation_0D21C_indices.inc"
};

static AnimationSet _gActor323000Animation0D21C = {
    _gActor323000Animation0D21CRecords,
    _gActor323000Animation0D21CIndices,
    { NULL, _gActor323000Animation0D21CBank1, NULL, NULL, _gActor323000Animation0D21CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0DA30Bank1[14] = {
#include "assets/actor_323000_animation_0DA30_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0DA30Bank4[207] = {
#include "assets/actor_323000_animation_0DA30_bank4.inc"
};

static AnimationRecord _gActor323000Animation0DA30Records[248] = {
#include "assets/actor_323000_animation_0DA30_records.inc"
};

static u16 _gActor323000Animation0DA30Indices[20] = {
#include "assets/actor_323000_animation_0DA30_indices.inc"
};

static AnimationSet _gActor323000Animation0DA30 = {
    _gActor323000Animation0DA30Records,
    _gActor323000Animation0DA30Indices,
    { NULL, _gActor323000Animation0DA30Bank1, NULL, NULL, _gActor323000Animation0DA30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0DCE4Bank1[5] = {
#include "assets/actor_323000_animation_0DCE4_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0DCE4Bank4[55] = {
#include "assets/actor_323000_animation_0DCE4_bank4.inc"
};

static AnimationRecord _gActor323000Animation0DCE4Records[83] = {
#include "assets/actor_323000_animation_0DCE4_records.inc"
};

static u16 _gActor323000Animation0DCE4Indices[20] = {
#include "assets/actor_323000_animation_0DCE4_indices.inc"
};

static AnimationSet _gActor323000Animation0DCE4 = {
    _gActor323000Animation0DCE4Records,
    _gActor323000Animation0DCE4Indices,
    { NULL, _gActor323000Animation0DCE4Bank1, NULL, NULL, _gActor323000Animation0DCE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0E004Bank1[6] = {
#include "assets/actor_323000_animation_0E004_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0E004Bank4[66] = {
#include "assets/actor_323000_animation_0E004_bank4.inc"
};

static AnimationRecord _gActor323000Animation0E004Records[96] = {
#include "assets/actor_323000_animation_0E004_records.inc"
};

static u16 _gActor323000Animation0E004Indices[20] = {
#include "assets/actor_323000_animation_0E004_indices.inc"
};

static AnimationSet _gActor323000Animation0E004 = {
    _gActor323000Animation0E004Records,
    _gActor323000Animation0E004Indices,
    { NULL, _gActor323000Animation0E004Bank1, NULL, NULL, _gActor323000Animation0E004Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation0EF14Bank1[38] = {
#include "assets/actor_323000_animation_0EF14_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation0EF14Bank4[347] = {
#include "assets/actor_323000_animation_0EF14_bank4.inc"
};

static AnimationRecord _gActor323000Animation0EF14Records[484] = {
#include "assets/actor_323000_animation_0EF14_records.inc"
};

static u16 _gActor323000Animation0EF14Indices[18] = {
#include "assets/actor_323000_animation_0EF14_indices.inc"
};

static AnimationSet _gActor323000Animation0EF14 = {
    _gActor323000Animation0EF14Records,
    _gActor323000Animation0EF14Indices,
    { NULL, _gActor323000Animation0EF14Bank1, NULL, NULL, _gActor323000Animation0EF14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor323000Animation11248Bank1[99] = {
#include "assets/actor_323000_animation_11248_bank1.inc"
};

static AnimationPackedRotation _gActor323000Animation11248Bank4[767] = {
#include "assets/actor_323000_animation_11248_bank4.inc"
};

static AnimationRecord _gActor323000Animation11248Records[1170] = {
#include "assets/actor_323000_animation_11248_records.inc"
};

static u16 _gActor323000Animation11248Indices[18] = {
#include "assets/actor_323000_animation_11248_indices.inc"
};

static AnimationSet _gActor323000Animation11248 = {
    _gActor323000Animation11248Records,
    _gActor323000Animation11248Indices,
    { NULL, _gActor323000Animation11248Bank1, NULL, NULL, _gActor323000Animation11248Bank4, NULL, NULL, NULL },
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
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    164,
    157,
    22,
    128,
    96,
    160,
    22,
    128,
    148,
    166,
    22,
    128,
    108,
    172,
    22,
    128,
    160,
    176,
    22,
    128,
    120,
    183,
    22,
    128,
    120,
    189,
    22,
    128,
    212,
    191,
    22,
    128,
    248,
    197,
    22,
    128,
    72,
    202,
    22,
    128,
    44,
    208,
    22,
    128,
    168,
    209,
    22,
    128,
    76,
    216,
    22,
    128,
    52,
    13,
    23,
    128,
    104,
    48,
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
    148,
    224,
    22,
    128,
    60,
    240,
    22,
    128,
    4,
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
    48,
    232,
    22,
    128,
    80,
    248,
    22,
    128,
    36,
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
    { 2015, func_actor_323000_8016483C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, desertChaserSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_323000_80164A54 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, desertChaserMsgPlayAnim },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_323000_80173A08 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, desertChaserTask, { .model = &_gActor323000DesertChaserBody } };

static DesertChaserContactPushStepStorage ActorContact_ScratchPosition = { 0 };

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &(ActorContact_ScratchPosition.step);
}

Actor323000Storage3A24 gRigEffectRec = { { 0 }, { 0 } };

static void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2);

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/desert_chaser_blend_tick.inc.c"

/// Effect and sound step of the tick: for the clip in `animId`, watches
/// the clip each relevant slot plays, and the first frame one reaches a
/// watched value spawns effect 0x60054 at the matching coordinate and returns
/// the `SndEvt_EnqueueType6` id to play (0 where only effects fire).
/// `lastCueFrames` remembers each slot's last clip so the step fires once; it is
/// cleared when none of the watched clips is playing.
s32 desertChaserAnimCues(Task* task, DesertChaserWork* work)
{
    SVECTOR vec;
    s32     reset;

    reset = 1;
    switch (work->animId) {
        case 0: {
            s32 clip = work->rig.slots[9].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;

            if (clip == 0x58) {
                old = work->lastCueFrames[9];
                if (old != clip) {
                    work->lastCueFrames[9] = clip;
                    vec.vx                 = -500;
                    vec.vz                 = 200;
                    vec.vy                 = 650;
                    Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002220, &vec);
                    return 0x40010002;
                }
                work->lastCueFrames[9] = old;
                reset                  = 0;
            }
        }
            {
                s32 clip = work->rig.slots[7].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0x3E) {
                    old = work->lastCueFrames[7];
                    if (old != clip) {
                        work->lastCueFrames[7] = clip;
                        vec.vx                 = -1000;
                        vec.vz                 = 200;
                        vec.vy                 = 650;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002220, &vec);
                        return 0x40010001;
                    }
                    work->lastCueFrames[7] = old;
                    reset                  = 0;
                }
            }
            {
                s32 clip = work->rig.slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0x84) {
                    old = work->lastCueFrames[14];
                    if (old != clip) {
                        work->lastCueFrames[14] = clip;
                        vec.vz                  = 0;
                        vec.vx                  = 0;
                        vec.vy                  = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80002220, &vec);
                        return 0x40010002;
                    }
                    work->lastCueFrames[14] = old;
                    reset                   = 0;
                }
            }
            {
                s32 clip = work->rig.slots[17].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0xA9) {
                    old = work->lastCueFrames[17];
                    if (old != clip) {
                        work->lastCueFrames[17] = clip;
                        vec.vz                  = 0;
                        vec.vx                  = 0;
                        vec.vy                  = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80002220, &vec);
                        return 0x40010001;
                    }
                    work->lastCueFrames[17] = old;
                    reset                   = 0;
                }
            }
            break;
        case 10: {
            s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;

            if (clip == 0x9) {
                old = work->lastCueFrames[1];
                if (old != clip) {
                    work->lastCueFrames[1] = clip;
                    vec.vz                 = 0;
                    vec.vx                 = 0;
                    vec.vy                 = 0;
                    Gp_SpawnEff(EFFECT_DUST_PUFF, task->extra.tmd->coords, 0x80004A00, &vec);
                    return 0x40010005;
                }
                work->lastCueFrames[1] = old;
                reset                  = 0;
            }
        } break;
        case 3: {
            s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;

            if (clip == 0x4) {
                reset = 0;
                old   = work->lastCueFrames[1];
                if (old != clip) {
                    work->lastCueFrames[1] = clip;
                    return 0x40010004;
                }
                work->lastCueFrames[1] = old;
            }
        }
            {
                s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0x8) {
                    old = work->lastCueFrames[1];
                    if (old != clip) {
                        work->lastCueFrames[1] = clip;
                        return 0x40010003;
                    }
                    work->lastCueFrames[1] = old;
                    reset                  = 0;
                }
            }
            break;
        case 6: {
            s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 old;

            if (clip == 0x6) {
                old = work->lastCueFrames[1];
                if (old != clip) {
                    work->lastCueFrames[1] = clip;
                    vec.vx                 = -500;
                    vec.vz                 = 200;
                    vec.vy                 = 650;
                    Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80003200, &vec);
                    vec.vx = -1000;
                    vec.vz = 200;
                    vec.vy = 650;
                    Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80003200, &vec);
                    return 0;
                }
                work->lastCueFrames[1] = old;
                reset                  = 0;
            }
        }
            {
                s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0xB) {
                    old = work->lastCueFrames[1];
                    if (old != clip) {
                        work->lastCueFrames[1] = clip;
                        vec.vz                 = 0;
                        vec.vx                 = 0;
                        vec.vy                 = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80004480, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80004480, &vec);
                        return 0;
                    }
                    work->lastCueFrames[1] = old;
                    reset                  = 0;
                }
            }
            {
                s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0xC) {
                    old = work->lastCueFrames[1];
                    if (old != clip) {
                        work->lastCueFrames[1] = clip;
                        vec.vx                 = -500;
                        vec.vz                 = 200;
                        vec.vy                 = 650;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002200, &vec);
                        vec.vx = -1000;
                        vec.vz = 200;
                        vec.vy = 650;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002240, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80003300, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80003340, &vec);
                        return 0;
                    }
                    work->lastCueFrames[1] = old;
                    reset                  = 0;
                }
            }
            {
                s32 clip = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 old;

                if (clip == 0xD) {
                    old = work->lastCueFrames[1];
                    if (old != clip) {
                        work->lastCueFrames[1] = clip;
                        vec.vz                 = 0;
                        vec.vx                 = 0;
                        vec.vy                 = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80002200, &vec);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 600;
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80002300, &vec);
                        return 0;
                    }
                    work->lastCueFrames[1] = old;
                    reset                  = 0;
                }
            }
            break;
    }
    if (reset == 1) {
        memFillBytes(work->lastCueFrames, 0, sizeof(work->lastCueFrames));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

#include "../../shared/desert_chaser_spawn.inc.c"

/// State 1: on entry clears the enemy's link flag and the model's flags,
/// rebuilds its buffers and asks the tick to reset the slots. Each frame it
/// ticks; when slot 1 sets flag bit 0 during clip 0xF it moves on to clip
/// 0x10, and during clip 0xE it spawns effect 0x60054 at coordinate 7 with
/// spawn argument 0x80002300 while slot 1 plays clip 7 or 9, 0x80003400 for 8.
static void func_actor_323000_8016409C(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    SVECTOR           sp10;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->animRate       = 0x10;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        desertChaserAnimTick(task);
        return;
    }
    desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (work->animId == 0xF) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            work->animId      = 0x10;
        }
        desertChaserAnimTick(task);
    }
    if (work->animId == 0xE) {
        if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 7 || (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 9) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002300, &sp10);
        }
        if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 8) {
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80003400, &sp10);
        }
    }
}

/// State 3: on entry flags the enemy's link node, clears the model's flags,
/// rebuilds its buffers and starts clip 0xE with the frame counter at 0. Each
/// frame it ticks and, on frames 29, 32 and 33, spawns effect 0x60054 at the
/// limb coordinates; frame 32 also plays a sound chosen by the enemy's
/// `placeKey`.
static void func_actor_323000_8016420C(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    s32               id;
    s32               pan;
    SVECTOR           ofs2;
    SVECTOR           ofs;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->animRate       = 0x10;
        work->animId         = 0xE;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        work->stateTimer     = 0;
        desertChaserAnimTick(task);
        return;
    }
    desertChaserAnimTick(task);
    switch (++work->stateTimer) {
        case 29: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80005600, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80005A00, p);
        } break;
        case 32: {
            SVECTOR* p = &ofs;
            p->vx      = -0x3E8;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80005A80, p);
            p->vx = -0x1F4;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80006800, p);
            id  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000D;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            ofs2.vy = -0x258;
            ofs2.vx = 0;
            ofs2.vz = -0x384;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[1], 0x80005A00, &ofs2);
        } break;
        case 33: {
            SVECTOR* p = &ofs;
            p->vx      = -0x1F4;
            p->vz      = 0xC8;
            p->vy      = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80006800, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80006B00, p);
            p->vx = -0x3E8;
            p->vz = 0xC8;
            p->vy = 0x28A;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80004400, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80003800, p);
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x258;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80004900, p);
            ofs2.vy = -0x2BC;
            ofs2.vx = 0;
            ofs2.vz = -0x258;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &task->extra.tmd->coords[1], 0x80005A00, &ofs2);
        } break;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/desert_chaser_frame.inc.c"

/// Handler for message 0x7DF: does nothing.
s32 func_actor_323000_8016483C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

#include "../../shared/desert_chaser_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw_first.inc.c"

/// Handler for message 0x7DB: copies the payload's three leading bytes into
/// the work block and, when `code` is 0x202, selects the state from `mode`:
/// 1 starts state 2, 0 and 2 state 0, and 3 state 3. Other codes only store
/// the bytes.
s32 func_actor_323000_80164A54(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    DesertChaserWork* work;

    work = task->work;

    work->commandBytes[0] = msg->context.loc.stage;
    work->commandBytes[1] = msg->context.loc.area;
    work->commandBytes[2] = (u8)msg->command;

    if (msg->context.key == 0x202) {
        switch (msg->command) {
            case 1:
                work->state = 2;
                break;
            case 0:
            case 2:
                work->state = 0;
                break;
            case 3:
                work->state = msg->command;
                break;
        }
    }
    return 0;
}

#include "../../shared/desert_chaser_play_anim.inc.c"

#include "../../shared/desert_chaser_exit.inc.c"

/// Spawns effect 0x60054 at coordinate `arg1` with the offset that limb
/// uses; the spawn argument is `arg2` with bit 31 set. Coordinates the
/// switch does not list use whatever the offset holds. Nothing in this
/// package calls it.
static void func_actor_323000_80164B40(Task* task, s16 arg1, s16 arg2)
{
    SVECTOR    sp10;
    TmdObject* obj;

    switch (arg1) {
        case 0:
        case 1:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0;
            break;
        case 9:
            sp10.vx = -0x1F4;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 7:
            sp10.vx = -0x3E8;
            sp10.vz = 0xC8;
            sp10.vy = 0x28A;
            break;
        case 14:
        case 17:
            sp10.vz = 0;
            sp10.vx = 0;
            sp10.vy = 0x258;
            break;
    }

    obj = task->extra.tmd;
    Gp_SpawnEff(EFFECT_DUST_PUFF, &obj->coords[arg1], arg2 | 0x80000000, &sp10);
}

#include "../../shared/desert_chaser_hide.inc.c"

/// State 2: on entry flags the enemy's link node, clears the model's flags,
/// rebuilds its buffers and starts clip 0xD with the frame counter at 0; the
/// tick runs every frame.
static void func_actor_323000_80164C58(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    SVECTOR           unused; // never referenced; only reserves the frame slot the ROM has

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->animRate       = 0x10;
        work->animId         = 0xD;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        work->stateTimer     = 0;
        desertChaserAnimTick(task);
    } else {
        desertChaserAnimTick(task);
    }
}

#include "../../shared/desert_chaser_task.inc.c"
