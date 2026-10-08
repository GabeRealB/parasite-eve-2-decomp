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
#include "gameplay/scene_combat.h"

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

/// Whole-unit step `_actorContactApplyGridPushback` last applied to its coordinate.
static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

/// Per-state animation table `_desertChaserAnimTick` reads when it
/// re-seeds the slots: 0x2D bytes per `appliedAnim`, indexed by `animId`.
extern s8 gDesertChaserClipStartFrames[45][45];

/// Animation source `animationInitContext` is handed for both of the work block's
/// contexts.
extern u8 gRigAnimSource[];

/// Message table published as `Task::msgTable` by the spawn handler.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gRigMessages[7];

/// Effect record the spawn handler fills: the model root's coordinate and
/// the two spawn arguments 0x100 and 2.
extern DesertChaserEffectArgStorage gRigEffectRec;

/// Enemy parameters the spawn handler stores in `Enemy::param`.
extern EnemyParams gRigParams;

static void func_actor_323400_801641C4(Enemy* enemy, Task* task);
static void func_actor_323400_80164BD0(Enemy* enemy, Task* task);
static void func_actor_323400_80164C4C(Enemy* enemy, Task* task);

/// State handlers `_desertChaserFrameState` runs by `DesertChaserWork::state`.
#include "../../shared/actor_contacts.h"

static const EnemyTaskFuncTable4 gDesertChaserStates = {
    _desertChaserHideState,
    func_actor_323400_80164BD0,
    func_actor_323400_801641C4,
    func_actor_323400_80164C4C,
};

/// Task states `_desertChaserTask` runs by `Task::state`: the spawn
/// handler, the per-frame driver, then `enemyDestroy`.
static const DesertChaserTaskStates gDesertChaserTaskStates = {
    desertChaserSpawn,
    _desertChaserFrameState,
    enemyDestroy,
};

static TmdSource _gActor323400DesertChaserBody;
static s32       _actor323400ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedArg);
static void      _actor323400IgnoreMessage2015(Task* unusedTask, s32 unusedMessageId, s32 unusedArg, s32 unusedSecondArg);

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

s8 gDesertChaserClipStartFrames[45][45] = {
    /*  0 */ { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  1 */ { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  2 */ { 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  3 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  4 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  5 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  6 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  7 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  8 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  9 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 10 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 11 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 12 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 13 */ { 5, 7, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 14 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 15 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 16 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 17 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 18 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 19 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 20 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 21 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 22 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 23 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 24 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 25 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 26 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 27 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 28 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 29 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 30 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 31 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 32 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 33 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 34 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 35 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 36 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 37 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 38 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 39 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 40 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 41 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 42 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 43 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 44 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

enum { ACTOR_323400_MESSAGE_NO_OP = 2015 };

TaskMessageEntry gRigMessages[7] = {
    { ACTOR_323400_MESSAGE_NO_OP, _actor323400IgnoreMessage2015 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _desertChaserSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor323400ApplyCommand },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _desertChaserMsgPlayAnim },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_323400_8017120C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _desertChaserTask, { .model = &_gActor323400DesertChaserBody } };

static DesertChaserContactPushStepStorage ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &(ActorContact_ScratchPosition.step);
}

DesertChaserEffectArgStorage gRigEffectRec;

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/desert_chaser_blend_tick.inc.c"

/// Emits one-shot animation dust cues and returns an EVT sound script, or zero.
///
/// Walking tests slots 9, 7, 14 and 17; other clips test slot 1. The low ten pose record-index bits select a
/// cue; per-slot history suppresses repeats until a non-cue clears the history.
/// The task and work must belong to the same live chaser, with slots and model
/// parts 0..17 available. Dust offsets use model-part units and are borrowed
/// for placement; dust playback does not follow the retained offset pointer.
/// Sound placement and panning belong to the caller.
static s32 _desertChaserAnimCues(Task* task, DesertChaserWork* work)
{
    SVECTOR effectOffset;
    s32     resetCueHistory;
    resetCueHistory = 1;
    // The low ten record-index bits identify cues, not elapsed animation frames.
    switch (work->animId) {
        case 0: {
            s32 cueIndex = work->rig.slots[9].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x58) {
                previousCueIndex = work->lastCueFrames[9];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[9] = cueIndex;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[9] = previousCueIndex;
                resetCueHistory        = 0;
            }
        }
            {
                s32 cueIndex = work->rig.slots[7].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0x3E) {
                    previousCueIndex = work->lastCueFrames[7];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[7] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                        }
                        return DESERT_CHASER_SOUND_STEP_1;
                    }
                    work->lastCueFrames[7] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0x84) {
                    previousCueIndex = work->lastCueFrames[14];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[14] = cueIndex;
                        effectOffset.vz         = 0;
                        effectOffset.vx         = 0;
                        effectOffset.vy         = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                        }
                        return DESERT_CHASER_SOUND_STEP_2;
                    }
                    work->lastCueFrames[14] = previousCueIndex;
                    resetCueHistory         = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[17].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xA9) {
                    previousCueIndex = work->lastCueFrames[17];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[17] = cueIndex;
                        effectOffset.vz         = 0;
                        effectOffset.vx         = 0;
                        effectOffset.vy         = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                        }
                        return DESERT_CHASER_SOUND_STEP_1;
                    }
                    work->lastCueFrames[17] = previousCueIndex;
                    resetCueHistory         = 0;
                }
            }
            break;

        case 10: {
            s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x9) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[1] = cueIndex;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(0)) {
                        effectSpawn(EFFECT_DUST_PUFF, task->extra.tmd->coords, (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 2560), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_CUE_05;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
        } break;

        case 3: {
            s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x4) {
                resetCueHistory  = 0;
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[1] = cueIndex;
                    return DESERT_CHASER_SOUND_CUE_04;
                }
                work->lastCueFrames[1] = previousCueIndex;
            }
        }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0x8) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        return DESERT_CHASER_SOUND_CUE_03;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            break;

        case 6: {
            s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            s32 previousCueIndex;
            if (cueIndex == 0x6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != cueIndex) {
                    work->lastCueFrames[1] = cueIndex;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return 0;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
        }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xB) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), &effectOffset);
                        }
                        return 0;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xC) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x2BC;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 576), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 768), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 832), &effectOffset);
                        }
                        return 0;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            {
                s32 cueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
                s32 previousCueIndex;
                if (cueIndex == 0xD) {
                    previousCueIndex = work->lastCueFrames[1];
                    if (previousCueIndex != cueIndex) {
                        work->lastCueFrames[1] = cueIndex;
                        effectOffset.vz        = 0;
                        effectOffset.vx        = 0;
                        effectOffset.vy        = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                        }
                        effectOffset.vz = 0;
                        effectOffset.vx = 0;
                        effectOffset.vy = 0x258;
                        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 768), &effectOffset);
                        }
                        return 0;
                    }
                    work->lastCueFrames[1] = previousCueIndex;
                    resetCueHistory        = 0;
                }
            }
            break;
    }

    // An intervening non-cue record rearms the one-shot cue history.
    if (resetCueHistory == 1) {
        memFillBytes(work->lastCueFrames, 0, sizeof(work->lastCueFrames));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

#include "../../shared/desert_chaser_spawn.inc.c"

/// State 2 of `gDesertChaserStates`. On entry it flags the enemy's link
/// node, shows the model (clears its flags) and rebuilds its buffers, resets
/// the slots to clip 0xD and zeroes the frame counter `stateTimer` before the
/// tick. Otherwise it advances `stateTimer` and, on frames 9, 10, 12 and 13,
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

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate       = 0x10;
        work->animId         = 0xD;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        work->stateTimer     = 0;
        _desertChaserAnimTick(task);
        return;
    }
    switch (++work->stateTimer) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000E;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    _desertChaserAnimTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

#include "../../shared/desert_chaser_frame.inc.c"

/// Ignores the cutscene chaser's message 2015 without changing any state.
///
/// All arguments are unused. The result register is left unspecified; senders
/// must ignore the dispatch result.
static void _actor323400IgnoreMessage2015(Task* unusedTask, s32 unusedMessageId, s32 unusedArg, s32 unusedSecondArg)
{
}

#include "../../shared/desert_chaser_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw_first.inc.c"

/// Applies the Breezeway cutscene chaser's stage/area command.
///
/// Borrows a complete command for this call and requires initialized work/model.
/// Always records stage, area and the low command byte. Dryfield/Breezeway
/// commands 0/2 hide; 1 places the root at (17200, 1, 2700) parent-coordinate
/// units, dirties composition and starts clip 13's state. Other contexts or
/// selectors leave placement and state intact. Returns 0; the message ID and
/// second payload are unused.
static s32 _actor323400ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_323400_COMMAND_CONTEXT        = (GAME_AREA_DRYFIELD_BREEZEWAY << 8) | GAME_STAGE_DRYFIELD,
        ACTOR_323400_COMMAND_HIDE           = 0,
        ACTOR_323400_COMMAND_PLAY_CLIP_13   = 1,
        ACTOR_323400_COMMAND_HIDE_ALTERNATE = 2,
        ACTOR_323400_STATE_HIDDEN           = 0,
        ACTOR_323400_STATE_PLAY_CLIP_13     = 2,
        ACTOR_323400_SCENE_ROOT_X           = 17200,
        ACTOR_323400_SCENE_ROOT_Z           = 2700
    };
    DesertChaserWork* work;
    u16               commandSelector;

    work = task->work;

    work->commandBytes[0] = command->context.loc.stage;
    work->commandBytes[1] = command->context.loc.area;
    work->commandBytes[2] = (u8)command->command;

    if (command->context.key == ACTOR_323400_COMMAND_CONTEXT) {
        commandSelector = command->command;
        switch (commandSelector) {
            case ACTOR_323400_COMMAND_PLAY_CLIP_13:
                task->extra.tmd->coords->coord.t[0]   = ACTOR_323400_SCENE_ROOT_X;
                task->extra.tmd->coords->coord.t[1]   = commandSelector;
                task->extra.tmd->coords->coord.t[2]   = ACTOR_323400_SCENE_ROOT_Z;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = ACTOR_323400_STATE_PLAY_CLIP_13;
                break;
            case ACTOR_323400_COMMAND_HIDE:
            case ACTOR_323400_COMMAND_HIDE_ALTERNATE:
                work->state = ACTOR_323400_STATE_HIDDEN;
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
/// the slots to the current clip `animId` with both turn targets zeroed.
/// The tick runs every frame.
static void func_actor_323400_80164BD0(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;
    TmdObject*        obj;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate       = 0x10;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        _desertChaserAnimTick(task);
    } else {
        _desertChaserAnimTick(task);
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

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate       = 0x10;
        work->animId         = 2;
        work->animRequest    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        _desertChaserAnimTick(task);
    } else {
        _desertChaserAnimTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

#include "../../shared/desert_chaser_task.inc.c"
