#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#define GOLEM_PAWN_ROOK_TYPE   GOLEM_PAWN
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_BEAM_SWORD
#include "../../shared/player_detection.h"
#include "../../shared/golem_pawn_rook.h"

static const EnemyTaskFuncTable3 Actor02000_D00060;
static const EnemyTaskFuncTable3 Actor02000_D0006C;

extern DamageAttack gGolemPawnRookAttacks[];
extern s32          gGolemPawnRookSwingCue;

extern s16 gGolemPawnRookAnimBlendFrames[];
extern s16 gGolemPawnRookWeakSpotHits[];
extern s16 gGolemPawnRookWeakSpotHitsFlagged[];
extern s32 gGolemPawnRookVoiceCues[];

static AnimationSet _gActor02000Actor102000Animation09C20;

static AnimationSet _gActor02000Actor102000Animation0A588;

static AnimationSet _gActor02000Actor102000Animation0ABB0;

static AnimationSet _gActor02000Actor102000Animation0B3B0;

static AnimationSet _gActor02000Actor102000Animation0CAA4;

static AnimationSet _gActor02000Actor102000Animation0D1E8;

static AnimationSet _gActor02000Actor102000Animation0E7FC;

static AnimationSet _gActor02000Actor102000Animation0F534;

static AnimationSet _gActor02000Actor102000Animation0FC6C;

static AnimationSet _gActor02000Actor102000Animation10190;

static AnimationSet _gActor02000Actor102000Animation10BA0;

static AnimationSet _gActor02000Actor102000Animation11648;

static AnimationSet _gActor02000Actor102000Animation11C28;

static AnimationSet _gActor02000Actor102000Animation1291C;

static AnimationSet _gActor02000Actor102000Animation13994;

static AnimationSet _gActor02000Actor102000Animation13E68;

static AnimationSet _gActor02000Actor102000Animation14178;

static AnimationSet _gActor02000Actor102000Animation14354;

static AnimationSet _gActor02000Actor102000Animation1529C;

static AnimationSet _gActor02000Actor102000Animation15830;

static AnimationSet _gActor02000Actor102000Animation15AF8;

static AnimationSet _gActor02000Actor102000Animation15CD4;

static TmdSource _gActor02000PawnGolemBody;

static TmdSource _gActor02000GolemBeamSword;

static void _golemPawnRookEngageState(Task* task);

static void _actor02000SwordTask(Task* sword);

static void _actor02000BodyTask(Task* body);

extern AnimationSet* Actor02000_D15FE8[31];

extern TaskDesc Actor02000_D15FD0[];

extern u16* Actor02000_D15FB8[];

extern EnemyParams Actor02000_D15D10;

extern TaskFunc gGolemPawnRookStates[];

static void _actor02000SpawnBody(Enemy* enemy, Task* actor);

#include "../../shared/golem_pawn_rook_take_hits.inc.c"

s16 gGolemPawnRookAnimBlendFrames[32] = {
    0,
    8,
    8,
    0,
    8,
    8,
    0,
    0,
    8,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    8,
    4,
    4,
    4,
    4,
    4,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
};

static TmdBone _gActor02000PawnGolemBodySkeleton[19] = {
#include "assets/pawn_golem_body_skeleton.inc"
};

static u32 _gActor02000PawnGolemBodyPartVerts[19] = {
#include "assets/pawn_golem_body_partVerts.inc"
};

static SVECTOR _gActor02000PawnGolemBodyVerts[339] = {
#include "assets/pawn_golem_body_verts.inc"
};

static SVECTOR _gActor02000PawnGolemBodyNormals[346] = {
#include "assets/pawn_golem_body_normals.inc"
};

static u32 _gActor02000PawnGolemBodyStream[3745] = {
#include "assets/pawn_golem_body_stream.inc"
};

static TmdSource _gActor02000PawnGolemBody = {
    0,
    20476,
    5672,
    19,
    _gActor02000PawnGolemBodyPartVerts,
    _gActor02000PawnGolemBodyVerts,
    _gActor02000PawnGolemBodyNormals,
    _gActor02000PawnGolemBodySkeleton,
    _gActor02000PawnGolemBodyStream,
};

static TmdBone _gActor02000GolemBeamSwordSkeleton[1] = {
#include "assets/golem_beam_sword_skeleton.inc"
};

static u32 _gActor02000GolemBeamSwordPartVerts[1] = {
#include "assets/golem_beam_sword_partVerts.inc"
};

static SVECTOR _gActor02000GolemBeamSwordVerts[29] = {
#include "assets/golem_beam_sword_verts.inc"
};

static SVECTOR _gActor02000GolemBeamSwordNormals[24] = {
#include "assets/golem_beam_sword_normals.inc"
};

static u32 _gActor02000GolemBeamSwordStream[212] = {
#include "assets/golem_beam_sword_stream.inc"
};

static TmdSource _gActor02000GolemBeamSword = {
    0,
    1436,
    0,
    1,
    _gActor02000GolemBeamSwordPartVerts,
    _gActor02000GolemBeamSwordVerts,
    _gActor02000GolemBeamSwordNormals,
    _gActor02000GolemBeamSwordSkeleton,
    _gActor02000GolemBeamSwordStream,
};

static AnimationPackedPose _gActor02000Actor102000Animation09C20Bank1[21] = {
#include "assets/actor_102000_animation_09C20_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation09C20Bank4[317] = {
#include "assets/actor_102000_animation_09C20_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation09C20Records[382] = {
#include "assets/actor_102000_animation_09C20_records.inc"
};

static u16 _gActor02000Actor102000Animation09C20Indices[20] = {
#include "assets/actor_102000_animation_09C20_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation09C20 = {
    _gActor02000Actor102000Animation09C20Records,
    _gActor02000Actor102000Animation09C20Indices,
    { NULL, _gActor02000Actor102000Animation09C20Bank1, NULL, NULL, _gActor02000Actor102000Animation09C20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0A588Bank1[16] = {
#include "assets/actor_102000_animation_0A588_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0A588Bank4[237] = {
#include "assets/actor_102000_animation_0A588_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0A588Records[297] = {
#include "assets/actor_102000_animation_0A588_records.inc"
};

static u16 _gActor02000Actor102000Animation0A588Indices[20] = {
#include "assets/actor_102000_animation_0A588_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0A588 = {
    _gActor02000Actor102000Animation0A588Records,
    _gActor02000Actor102000Animation0A588Indices,
    { NULL, _gActor02000Actor102000Animation0A588Bank1, NULL, NULL, _gActor02000Actor102000Animation0A588Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0ABB0Bank1[12] = {
#include "assets/actor_102000_animation_0ABB0_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0ABB0Bank4[142] = {
#include "assets/actor_102000_animation_0ABB0_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0ABB0Records[196] = {
#include "assets/actor_102000_animation_0ABB0_records.inc"
};

static u16 _gActor02000Actor102000Animation0ABB0Indices[20] = {
#include "assets/actor_102000_animation_0ABB0_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0ABB0 = {
    _gActor02000Actor102000Animation0ABB0Records,
    _gActor02000Actor102000Animation0ABB0Indices,
    { NULL, _gActor02000Actor102000Animation0ABB0Bank1, NULL, NULL, _gActor02000Actor102000Animation0ABB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0B3B0Bank1[14] = {
#include "assets/actor_102000_animation_0B3B0_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0B3B0Bank4[186] = {
#include "assets/actor_102000_animation_0B3B0_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0B3B0Records[264] = {
#include "assets/actor_102000_animation_0B3B0_records.inc"
};

static u16 _gActor02000Actor102000Animation0B3B0Indices[20] = {
#include "assets/actor_102000_animation_0B3B0_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0B3B0 = {
    _gActor02000Actor102000Animation0B3B0Records,
    _gActor02000Actor102000Animation0B3B0Indices,
    { NULL, _gActor02000Actor102000Animation0B3B0Bank1, NULL, NULL, _gActor02000Actor102000Animation0B3B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0CAA4Bank1[47] = {
#include "assets/actor_102000_animation_0CAA4_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0CAA4Bank4[613] = {
#include "assets/actor_102000_animation_0CAA4_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0CAA4Records[695] = {
#include "assets/actor_102000_animation_0CAA4_records.inc"
};

static u16 _gActor02000Actor102000Animation0CAA4Indices[20] = {
#include "assets/actor_102000_animation_0CAA4_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0CAA4 = {
    _gActor02000Actor102000Animation0CAA4Records,
    _gActor02000Actor102000Animation0CAA4Indices,
    { NULL, _gActor02000Actor102000Animation0CAA4Bank1, NULL, NULL, _gActor02000Actor102000Animation0CAA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0D1E8Bank1[19] = {
#include "assets/actor_102000_animation_0D1E8_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0D1E8Bank4[160] = {
#include "assets/actor_102000_animation_0D1E8_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0D1E8Records[228] = {
#include "assets/actor_102000_animation_0D1E8_records.inc"
};

static u16 _gActor02000Actor102000Animation0D1E8Indices[20] = {
#include "assets/actor_102000_animation_0D1E8_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0D1E8 = {
    _gActor02000Actor102000Animation0D1E8Records,
    _gActor02000Actor102000Animation0D1E8Indices,
    { NULL, _gActor02000Actor102000Animation0D1E8Bank1, NULL, NULL, _gActor02000Actor102000Animation0D1E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0E7FCBank1[66] = {
#include "assets/actor_102000_animation_0E7FC_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0E7FCBank4[549] = {
#include "assets/actor_102000_animation_0E7FC_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0E7FCRecords[646] = {
#include "assets/actor_102000_animation_0E7FC_records.inc"
};

static u16 _gActor02000Actor102000Animation0E7FCIndices[20] = {
#include "assets/actor_102000_animation_0E7FC_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0E7FC = {
    _gActor02000Actor102000Animation0E7FCRecords,
    _gActor02000Actor102000Animation0E7FCIndices,
    { NULL, _gActor02000Actor102000Animation0E7FCBank1, NULL, NULL, _gActor02000Actor102000Animation0E7FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0F534Bank1[21] = {
#include "assets/actor_102000_animation_0F534_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0F534Bank4[354] = {
#include "assets/actor_102000_animation_0F534_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0F534Records[409] = {
#include "assets/actor_102000_animation_0F534_records.inc"
};

static u16 _gActor02000Actor102000Animation0F534Indices[20] = {
#include "assets/actor_102000_animation_0F534_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0F534 = {
    _gActor02000Actor102000Animation0F534Records,
    _gActor02000Actor102000Animation0F534Indices,
    { NULL, _gActor02000Actor102000Animation0F534Bank1, NULL, NULL, _gActor02000Actor102000Animation0F534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation0FC6CBank1[16] = {
#include "assets/actor_102000_animation_0FC6C_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation0FC6CBank4[177] = {
#include "assets/actor_102000_animation_0FC6C_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation0FC6CRecords[217] = {
#include "assets/actor_102000_animation_0FC6C_records.inc"
};

static u16 _gActor02000Actor102000Animation0FC6CIndices[20] = {
#include "assets/actor_102000_animation_0FC6C_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation0FC6C = {
    _gActor02000Actor102000Animation0FC6CRecords,
    _gActor02000Actor102000Animation0FC6CIndices,
    { NULL, _gActor02000Actor102000Animation0FC6CBank1, NULL, NULL, _gActor02000Actor102000Animation0FC6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation10190Bank1[8] = {
#include "assets/actor_102000_animation_10190_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation10190Bank4[116] = {
#include "assets/actor_102000_animation_10190_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation10190Records[169] = {
#include "assets/actor_102000_animation_10190_records.inc"
};

static u16 _gActor02000Actor102000Animation10190Indices[20] = {
#include "assets/actor_102000_animation_10190_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation10190 = {
    _gActor02000Actor102000Animation10190Records,
    _gActor02000Actor102000Animation10190Indices,
    { NULL, _gActor02000Actor102000Animation10190Bank1, NULL, NULL, _gActor02000Actor102000Animation10190Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation10BA0Bank1[19] = {
#include "assets/actor_102000_animation_10BA0_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation10BA0Bank4[255] = {
#include "assets/actor_102000_animation_10BA0_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation10BA0Records[312] = {
#include "assets/actor_102000_animation_10BA0_records.inc"
};

static u16 _gActor02000Actor102000Animation10BA0Indices[20] = {
#include "assets/actor_102000_animation_10BA0_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation10BA0 = {
    _gActor02000Actor102000Animation10BA0Records,
    _gActor02000Actor102000Animation10BA0Indices,
    { NULL, _gActor02000Actor102000Animation10BA0Bank1, NULL, NULL, _gActor02000Actor102000Animation10BA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation11648Bank1[18] = {
#include "assets/actor_102000_animation_11648_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation11648Bank4[281] = {
#include "assets/actor_102000_animation_11648_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation11648Records[327] = {
#include "assets/actor_102000_animation_11648_records.inc"
};

static u16 _gActor02000Actor102000Animation11648Indices[20] = {
#include "assets/actor_102000_animation_11648_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation11648 = {
    _gActor02000Actor102000Animation11648Records,
    _gActor02000Actor102000Animation11648Indices,
    { NULL, _gActor02000Actor102000Animation11648Bank1, NULL, NULL, _gActor02000Actor102000Animation11648Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation11C28Bank1[8] = {
#include "assets/actor_102000_animation_11C28_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation11C28Bank4[130] = {
#include "assets/actor_102000_animation_11C28_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation11C28Records[202] = {
#include "assets/actor_102000_animation_11C28_records.inc"
};

static u16 _gActor02000Actor102000Animation11C28Indices[20] = {
#include "assets/actor_102000_animation_11C28_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation11C28 = {
    _gActor02000Actor102000Animation11C28Records,
    _gActor02000Actor102000Animation11C28Indices,
    { NULL, _gActor02000Actor102000Animation11C28Bank1, NULL, NULL, _gActor02000Actor102000Animation11C28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation1291CBank1[23] = {
#include "assets/actor_102000_animation_1291C_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation1291CBank4[326] = {
#include "assets/actor_102000_animation_1291C_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation1291CRecords[414] = {
#include "assets/actor_102000_animation_1291C_records.inc"
};

static u16 _gActor02000Actor102000Animation1291CIndices[20] = {
#include "assets/actor_102000_animation_1291C_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation1291C = {
    _gActor02000Actor102000Animation1291CRecords,
    _gActor02000Actor102000Animation1291CIndices,
    { NULL, _gActor02000Actor102000Animation1291CBank1, NULL, NULL, _gActor02000Actor102000Animation1291CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation13994Bank1[31] = {
#include "assets/actor_102000_animation_13994_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation13994Bank4[429] = {
#include "assets/actor_102000_animation_13994_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation13994Records[512] = {
#include "assets/actor_102000_animation_13994_records.inc"
};

static u16 _gActor02000Actor102000Animation13994Indices[20] = {
#include "assets/actor_102000_animation_13994_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation13994 = {
    _gActor02000Actor102000Animation13994Records,
    _gActor02000Actor102000Animation13994Indices,
    { NULL, _gActor02000Actor102000Animation13994Bank1, NULL, NULL, _gActor02000Actor102000Animation13994Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation13E68Bank1[9] = {
#include "assets/actor_102000_animation_13E68_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation13E68Bank4[113] = {
#include "assets/actor_102000_animation_13E68_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation13E68Records[149] = {
#include "assets/actor_102000_animation_13E68_records.inc"
};

static u16 _gActor02000Actor102000Animation13E68Indices[20] = {
#include "assets/actor_102000_animation_13E68_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation13E68 = {
    _gActor02000Actor102000Animation13E68Records,
    _gActor02000Actor102000Animation13E68Indices,
    { NULL, _gActor02000Actor102000Animation13E68Bank1, NULL, NULL, _gActor02000Actor102000Animation13E68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation14178Bank1[5] = {
#include "assets/actor_102000_animation_14178_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation14178Bank4[65] = {
#include "assets/actor_102000_animation_14178_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation14178Records[96] = {
#include "assets/actor_102000_animation_14178_records.inc"
};

static u16 _gActor02000Actor102000Animation14178Indices[20] = {
#include "assets/actor_102000_animation_14178_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation14178 = {
    _gActor02000Actor102000Animation14178Records,
    _gActor02000Actor102000Animation14178Indices,
    { NULL, _gActor02000Actor102000Animation14178Bank1, NULL, NULL, _gActor02000Actor102000Animation14178Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation14354Bank1[2] = {
#include "assets/actor_102000_animation_14354_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation14354Bank4[17] = {
#include "assets/actor_102000_animation_14354_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation14354Records[76] = {
#include "assets/actor_102000_animation_14354_records.inc"
};

static u16 _gActor02000Actor102000Animation14354Indices[20] = {
#include "assets/actor_102000_animation_14354_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation14354 = {
    _gActor02000Actor102000Animation14354Records,
    _gActor02000Actor102000Animation14354Indices,
    { NULL, _gActor02000Actor102000Animation14354Bank1, NULL, NULL, _gActor02000Actor102000Animation14354Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation1529CBank1[28] = {
#include "assets/actor_102000_animation_1529C_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation1529CBank4[394] = {
#include "assets/actor_102000_animation_1529C_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation1529CRecords[480] = {
#include "assets/actor_102000_animation_1529C_records.inc"
};

static u16 _gActor02000Actor102000Animation1529CIndices[20] = {
#include "assets/actor_102000_animation_1529C_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation1529C = {
    _gActor02000Actor102000Animation1529CRecords,
    _gActor02000Actor102000Animation1529CIndices,
    { NULL, _gActor02000Actor102000Animation1529CBank1, NULL, NULL, _gActor02000Actor102000Animation1529CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation15830Bank1[9] = {
#include "assets/actor_102000_animation_15830_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation15830Bank4[127] = {
#include "assets/actor_102000_animation_15830_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation15830Records[183] = {
#include "assets/actor_102000_animation_15830_records.inc"
};

static u16 _gActor02000Actor102000Animation15830Indices[20] = {
#include "assets/actor_102000_animation_15830_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation15830 = {
    _gActor02000Actor102000Animation15830Records,
    _gActor02000Actor102000Animation15830Indices,
    { NULL, _gActor02000Actor102000Animation15830Bank1, NULL, NULL, _gActor02000Actor102000Animation15830Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation15AF8Bank1[5] = {
#include "assets/actor_102000_animation_15AF8_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation15AF8Bank4[56] = {
#include "assets/actor_102000_animation_15AF8_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation15AF8Records[87] = {
#include "assets/actor_102000_animation_15AF8_records.inc"
};

static u16 _gActor02000Actor102000Animation15AF8Indices[20] = {
#include "assets/actor_102000_animation_15AF8_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation15AF8 = {
    _gActor02000Actor102000Animation15AF8Records,
    _gActor02000Actor102000Animation15AF8Indices,
    { NULL, _gActor02000Actor102000Animation15AF8Bank1, NULL, NULL, _gActor02000Actor102000Animation15AF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02000Actor102000Animation15CD4Bank1[2] = {
#include "assets/actor_102000_animation_15CD4_bank1.inc"
};

static AnimationPackedRotation _gActor02000Actor102000Animation15CD4Bank4[17] = {
#include "assets/actor_102000_animation_15CD4_bank4.inc"
};

static AnimationRecord _gActor02000Actor102000Animation15CD4Records[76] = {
#include "assets/actor_102000_animation_15CD4_records.inc"
};

static u16 _gActor02000Actor102000Animation15CD4Indices[20] = {
#include "assets/actor_102000_animation_15CD4_indices.inc"
};

static AnimationSet _gActor02000Actor102000Animation15CD4 = {
    _gActor02000Actor102000Animation15CD4Records,
    _gActor02000Actor102000Animation15CD4Indices,
    { NULL, _gActor02000Actor102000Animation15CD4Bank1, NULL, NULL, _gActor02000Actor102000Animation15CD4Bank4, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, 8 },
    { 15, 2 },
    { 5, 0 },
};

EnemyParams Actor02000_D15D10 = { gGolemPawnRookAttacks, 425, 125, 100, 5, 50, 6, 0, 0 };

s16 gGolemPawnRookWeakSpotHits[46] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
};

s16 gGolemPawnRookWeakSpotHitsFlagged[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

s32 gGolemPawnRookVoiceCues[17] = {
    0,
    0x40140001,
    0x40140002,
    0x40140003,
    0x40140004,
    0x40140005,
    0x40140006,
    0x4014000C,
    0x4014000D,
    0x40140009,
    0x4014000A,
    0x4014000B,
    0x4014000E,
    0x4014000F,
    0x40140010,
    0x40140011,
    0x40140012,
};

s32 gGolemPawnRookSwingCue = 0x40140007;

/* The four cue ids the other builds define at this position: the grenade's
 * impact, and the Rook's scream, silence and burst. The Pawn's Beam Sword
 * build has none of the code that plays them, so nothing in this package
 * reads these and each holds no id. */
s32 gGolemPawnRookImpactSound = 0;

s32 gGolemPawnRookScreamCue = 0;

s32 gGolemPawnRookSilenceCue = 0;

s32 gGolemPawnRookBurstCue = 0;

u16 Actor02000_D15E44[22] = {
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    0,
    2,
    0,
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

u16 Actor02000_D15E70[40] = {
    0,
    0,
    4,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
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

u16 Actor02000_D15EC0[40] = {
    0,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
};

u16 Actor02000_D15F10[50] = {
    0,
    4,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    1,
    2,
    1,
    0,
    3,
    2,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    3,
    1,
    0,
    1,
    0,
    2,
    2,
    2,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
};

u16 Actor02000_D15F74[34] = {
    0,
    0,
    2,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    4,
    0,
    2,
    2,
    0,
    2,
    0,
    4,
    2,
    0,
    2,
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
    4,
    0,
};

u16* Actor02000_D15FB8[6] = {
    NULL,
    Actor02000_D15E44,
    Actor02000_D15E70,
    Actor02000_D15EC0,
    Actor02000_D15F10,
    Actor02000_D15F74,
};

TaskDesc Actor02000_D15FD0[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor02000BodyTask, { .model = &_gActor02000PawnGolemBody } },
    { { { TASK_BODY_TMD, 96 } }, _actor02000SwordTask, { .model = &_gActor02000GolemBeamSword } },
};

AnimationSet* Actor02000_D15FE8[31] = {
    NULL,
    &_gActor02000Actor102000Animation09C20,
    &_gActor02000Actor102000Animation0A588,
    &_gActor02000Actor102000Animation0ABB0,
    &_gActor02000Actor102000Animation0B3B0,
    &_gActor02000Actor102000Animation0CAA4,
    &_gActor02000Actor102000Animation0D1E8,
    &_gActor02000Actor102000Animation0E7FC,
    &_gActor02000Actor102000Animation0FC6C,
    &_gActor02000Actor102000Animation0F534,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor02000Actor102000Animation10190,
    &_gActor02000Actor102000Animation10BA0,
    &_gActor02000Actor102000Animation11648,
    &_gActor02000Actor102000Animation11C28,
    &_gActor02000Actor102000Animation1291C,
    &_gActor02000Actor102000Animation13994,
    &_gActor02000Actor102000Animation13E68,
    &_gActor02000Actor102000Animation14178,
    &_gActor02000Actor102000Animation14354,
    &_gActor02000Actor102000Animation1529C,
    &_gActor02000Actor102000Animation15830,
    &_gActor02000Actor102000Animation15AF8,
    &_gActor02000Actor102000Animation15CD4,
    NULL,
};

TaskFunc gGolemPawnRookStates[15] = {
    _golemPawnRookIdleState,
    _golemPawnRookPatrolState,
    _golemPawnRookEngageState,
    _golemPawnRookSwordChargeState,
    _golemPawnRookSwordSwingState,
    _golemPawnRookNopState,
    _golemPawnRookNopState,
    _golemPawnRookNopState,
    _golemPawnRookStaggerState,
    _golemPawnRookRecoilState,
    _golemPawnRookBuildupState,
    _golemPawnRookKnockdownState,
    _golemPawnRookDownedHitState,
    _golemPawnRookCollapseState,
    _golemPawnRookDownedDeathState,
};

#include "../../shared/golem_pawn_rook_patrol.inc.c"

#include "../../shared/golem_pawn_rook_player_noise.inc.c"

#include "../../shared/golem_pawn_rook_knockdown.inc.c"

#include "../../shared/golem_pawn_rook_downed_hit.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

#include "../../shared/golem_pawn_rook_sword_charge.inc.c"

#include "../../shared/golem_pawn_rook_sword_swing.inc.c"

/// Initializes the Beam Sword Pawn GOLEM body and its attached sword.
///
/// Requires a live nineteen-part model, placement and valid stage/area sound
/// indices. Allocates body-owned work and lighting and initializes slots 1..18.
/// Sword spawning must succeed; its texture placement is applied immediately.
/// A fresh placement links targeting and four combat bodies and acquires a
/// battle reference. Saved behind/front downed poses enter the persistent corpse
/// state without relinking combat. Work allocation failure destroys the pair.
static void _actor02000SpawnBody(Enemy* enemy, Task* actor)
{
    /// Links an initialized body, clears its contacts and enables collision tests.
    ///
    /// body is a side-effect-free pointer expression evaluated twice; other
    /// arguments are evaluated once. contactCount counts writable contact elements.
    /// The body already borrows this table, directly or through its capsule;
    /// all storage stays live until unlinking. Expands to a braced statement block.
#define ACTOR_02000_LINK_BODY_CONTACTS(listIndex, body, contacts, contactCount, enabledTests) \
    {                                                                                         \
        worldCollisionLinkBody((listIndex), (body));                                          \
        worldCollisionInitContacts((contacts), (contactCount), 0);                            \
        (body)->flags |= (enabledTests);                                                      \
    }
    enum {
        ACTOR_02000_ID                       = 20,
        ACTOR_02000_BODY_PART                = 3,
        ACTOR_02000_SIGHT_PART               = 4,
        ACTOR_02000_SWORD_TASK               = 1,
        ACTOR_02000_HIT_EFFECT_SIZE          = 1280,
        ACTOR_02000_HIT_EFFECT_HIGH_ARGUMENT = 2,
        ACTOR_02000_PATROL_UNITS_PER_VARIANT = 1000,
        ACTOR_02000_SOUND_FILE_GROUP         = 10,
        ACTOR_02000_SIGHT_LENGTH             = 8000,
        ACTOR_02000_SIGHT_FAR_RADIUS         = 1000,
        ACTOR_02000_SIGHT_NEAR_RADIUS        = 1500,
        ACTOR_02000_HURT_RADIUS              = 400,
        ACTOR_02000_GROUND_RADIUS            = 550,
        ACTOR_02000_STRIKE_OFFSET_Y          = 500,
        ACTOR_02000_STRIKE_RADIUS            = 500,
        ACTOR_02000_RESTORED_CORPSE_STEP     = 2,
        ACTOR_02000_CORPSE_BEHIND_ANIM       = 25,
        ACTOR_02000_CORPSE_FRONT_ANIM        = 29,
        ACTOR_02000_IDLE_ANIM                = 1,
        ACTOR_02000_FRESH_PLACEMENT          = 0,
        ACTOR_02000_PLACEMENT_PATROLS        = 1,
    };
    GolemPawnRookWork* work;
    TmdObject*         model;
    GfxCoord*          root;
    GfxCoord*          bodyCoords;
    GfxCoord*          sightCoords;
    GfxCoord*          hurtCoords;
    GfxCoord*          groundCoords;
    GfxCoord*          swordCoords;
    Enemy*             swordEnemy;
    u16*               areaSoundSets;
    u8                 soundFileKey[8];
    u8                 soundFileArgs[8];
    s32                slotIndex;
    s32                patrolLengthThousands;

    // Body-owned work supplies lighting and animation storage to the attached sword.
    model = actor->extra.tmd;
    root  = model->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work                   = work;
    model->flags                  = 0;
    root->composeStamp            = GRAPHICS_COORD_DIRTY;
    model->lightMtx               = &work->lightMtx;
    model->colorMtx               = &work->colorMtx;
    work->actorId                 = ACTOR_02000_ID;
    work->taskTable               = Actor02000_D15FD0;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[ACTOR_02000_BODY_PART];
    work->hitEffectArg.spawnArgLo = ACTOR_02000_HIT_EFFECT_SIZE;
    work->hitEffectArg.spawnArgHi = ACTOR_02000_HIT_EFFECT_HIGH_ARGUMENT;
    animationInitContext(&work->rig.anim, Actor02000_D15FE8, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_02000_IDLE_ANIM);
    }
    swordEnemy = enemySpawnFromTable(Actor02000_D15FD0, ACTOR_02000_SWORD_TASK, 0, enemy);
    _actorRenderApplyTaskPlacementTextureOffsets(swordEnemy->task, enemy);

    // Restored corpses retain the sword but do not rejoin combat.
    switch (enemy->spawnState) {
        case ACTOR_02000_FRESH_PLACEMENT:
            enemy->field_4  = &root->coord;
            enemy->field_48 = 0;
            worldTargetLinkNode(&enemy->node);
            bodyCoords        = actor->extra.tmd->coords;
            enemy->bodyPos.vx = 0;
            enemy->bodyPos.vy = 0;
            enemy->bodyPos.vz = 0;
            enemy->param      = &Actor02000_D15D10;
            enemy->recs       = work->hurtContacts;
            enemy->coord      = &bodyCoords[ACTOR_02000_BODY_PART];
            enemy->hp         = Actor02000_D15D10.hpMax;
            sceneAcquireBattleRef(0);
            work->patrols = enemy->place->mode & ACTOR_02000_PLACEMENT_PATROLS;
            if (work->patrols == 0) {
                work->anim     = ACTOR_02000_IDLE_ANIM;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                patrolLengthThousands    = enemy->place->variant;
                work->patrolDistanceLeft = patrolLengthThousands * ACTOR_02000_PATROL_UNITS_PER_VARIANT;
            }

            // The CD request reads key bytes 3/2/0 and only four argument bytes.
            areaSoundSets = Actor02000_D15FB8[gGameSession->location.loc.stage];
            if (areaSoundSets != NULL) {
                work->soundSet = areaSoundSets[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                soundFileKey[3]  = 0;
                soundFileKey[2]  = ACTOR_02000_SOUND_FILE_GROUP;
                soundFileKey[0]  = work->soundSet;
                soundFileArgs[0] = ACTOR_02000_ID;
                soundFileArgs[3] = 0;
                soundFileArgs[2] = 0;
                soundFileArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, soundFileKey, soundFileArgs);
            }

            // Four collision bodies borrow contact storage from this work block.
            work->sightCapsule.ends[0].vz   = ACTOR_02000_SIGHT_LENGTH;
            work->sightCapsule.end0Radius   = ACTOR_02000_SIGHT_FAR_RADIUS;
            work->sightCapsule.ends[0].vx   = 0;
            work->sightCapsule.ends[0].vy   = 0;
            work->sightCapsule.ends[1].vx   = 0;
            work->sightCapsule.ends[1].vy   = 0;
            work->sightCapsule.ends[1].vz   = 0;
            work->sightCapsule.end1Radius   = ACTOR_02000_SIGHT_NEAR_RADIUS;
            work->sightCapsule.contacts     = work->sightContacts;
            sightCoords                     = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &sightCoords[ACTOR_02000_SIGHT_PART];
            ACTOR_02000_LINK_BODY_CONTACTS(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody, work->sightContacts,
                                           ARRAY_SIZE(work->sightContacts), (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));

            hurtCoords                      = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02000_ID;
            work->hurtBody.radius           = ACTOR_02000_HURT_RADIUS;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &hurtCoords[ACTOR_02000_BODY_PART];
            ACTOR_02000_LINK_BODY_CONTACTS(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody, work->hurtContacts,
                                           ARRAY_SIZE(work->hurtContacts), WORLD_COLLISION_BODY_PAIR_ENABLED);

            groundCoords                      = actor->extra.tmd->coords;
            work->groundBody.pos.vy           = -ACTOR_02000_GROUND_RADIUS;
            work->groundBody.context.contacts = work->groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0;
            work->groundBody.radius           = ACTOR_02000_GROUND_RADIUS;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->groundBody.coord            = groundCoords;
            ACTOR_02000_LINK_BODY_CONTACTS(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody, work->groundContacts,
                                           ARRAY_SIZE(work->groundContacts), (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED));

            swordCoords                       = swordEnemy->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = ACTOR_02000_STRIKE_OFFSET_Y;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = ACTOR_02000_STRIKE_RADIUS;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = swordCoords;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody);
            worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            actor->state            = GOLEM_PAWN_ROOK_TASK_RUNNING;
            break;
        case GOLEM_PAWN_ROOK_DOWNED_BEHIND:
            work->anim   = ACTOR_02000_CORPSE_BEHIND_ANIM;
            work->step   = ACTOR_02000_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
        case GOLEM_PAWN_ROOK_DOWNED_FRONT:
            work->anim   = ACTOR_02000_CORPSE_FRONT_ANIM;
            work->step   = ACTOR_02000_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
    }
#undef ACTOR_02000_LINK_BODY_CONTACTS
}
#include "../../shared/golem_pawn_rook_inlines.inc.c"

#include "../../shared/golem_pawn_rook_frame_no_sparks.inc.c"

/// Faces and approaches the player, then selects a Beam Sword swing or charge.
///
/// Requires initialized GOLEM Pawn body work, a live model root and the player's
/// matrix in the same coordinate frame. Steps 0..3 track, choose an attack,
/// listen and turn; other steps do nothing. Bearing narrows player offsets to
/// signed halfwords and uses 4096 units per turn; attack range uses full-word
/// X/Z offsets whose squared sum must fit a signed word. Updates movement and
/// animation requests for the enclosing tick rather than moving the root here.
/// Borrows one VECTOR-sized scratch-stack reservation, leaving its Y unused,
/// and restores the cursor on every path. All pointed-to state stays borrowed.
static void _golemPawnRookEngageState(Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_ENGAGE_TRACK           = 0,
        GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK     = 1,
        GOLEM_PAWN_ROOK_ENGAGE_LISTEN          = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP,
        GOLEM_PAWN_ROOK_ENGAGE_TURN            = 3,
        GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN       = 3,
        GOLEM_PAWN_ROOK_ENGAGE_SPEED           = 20,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD  = 1409,
        GOLEM_PAWN_ROOK_ENGAGE_SIGHT_YAW_LIMIT = 128,
        GOLEM_PAWN_ROOK_ENGAGE_LISTEN_FRAMES   = 96,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_FRAMES     = 35,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_RATE       = 59,
    };
    s16                currentYaw;
    s16                listenYaw;
    s16                step;
    s16                yawDelta;
    s16                listenYawDelta;
    s16                forwardSpeed;
    s32                absoluteYawDelta;
    s32                absoluteListenYawDelta;
    s16                wrappedYawDelta;
    s16                wrappedListenYawDelta;
    s16                yawError;
    s32                playerOffsetX;
    s32                playerOffsetZ;
    u16                sightFlags;
    u16                listenSightFlags;
    VECTOR*            scratchTop;
    VECTOR*            toPlayer;
    GolemPawnRookWork* work;
    GfxCoord*          rootCoord;

    /// Updates player/current yaw and measures the shortest 4096-unit gap.
    ///
    /// All arguments must be side-effect-free lvalues or pointers; expressions
    /// are evaluated repeatedly. Inputs are live work, root and X/Z offset;
    /// yawValue, deltaValue, wrappedValue and errorValue are s16; magnitudeValue
    /// is s32. Captures no identifiers. Expands to a compound statement and is
    /// undefined below.
#define GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, rootCoord, toPlayer, yawValue, deltaValue, magnitudeValue, wrappedValue, errorValue) \
    {                                                                                                                                     \
        (work)->targetYaw = ratan2((s16)(toPlayer)->vx, (s16)(toPlayer)->vz) & ACTOR_TRANSFORM_ANGLE_MASK;                                \
        (yawValue)        = ratan2((rootCoord)->coord.m[0][2], (rootCoord)->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;                  \
        (work)->yaw       = (yawValue);                                                                                                   \
        (deltaValue)      = (work)->targetYaw - (yawValue);                                                                               \
        (magnitudeValue)  = __builtin_abs((deltaValue));                                                                                  \
        if ((magnitudeValue) < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {                                                                         \
            (errorValue) = (magnitudeValue);                                                                                              \
        } else {                                                                                                                          \
            if ((deltaValue) > 0) {                                                                                                       \
                (wrappedValue) = ACTOR_TRANSFORM_ANGLE_TURN - (deltaValue);                                                               \
            } else {                                                                                                                      \
                (wrappedValue) = (deltaValue) + ACTOR_TRANSFORM_ANGLE_TURN;                                                               \
            }                                                                                                                             \
            (errorValue) = (wrappedValue);                                                                                                \
        }                                                                                                                                 \
    }

    scratchTop                   = SCRATCH_STACK_CURSOR(VECTOR);
    toPlayer                     = scratchTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = toPlayer;
    work                         = task->work;
    step                         = work->step;
    rootCoord                    = task->extra.tmd->coords;
    switch (step) {
        // Approach after the blend, opening sight only inside the narrow yaw cone.
        case GOLEM_PAWN_ROOK_ENGAGE_TRACK:
            forwardSpeed = 0;
            if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim]) {
                forwardSpeed = GOLEM_PAWN_ROOK_ENGAGE_SPEED;
            }
            work->forwardSpeed = forwardSpeed;
            work->turnRate     = GOLEM_PAWN_ROOK_WIND_UP_TURN;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            toPlayer->vz       = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, rootCoord, toPlayer, currentYaw, yawDelta, absoluteYawDelta, wrappedYawDelta, yawError);
            if (yawError >= GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD) {
                if (work->turnedAround == 0) {
                    work->anim = GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TURN;
                } else {
                    work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                    work->step         = GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK;
                    work->turnedAround = 0;
                }
            }
            if (yawError < GOLEM_PAWN_ROOK_ENGAGE_SIGHT_YAW_LIMIT) {
                sightFlags            = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sightBody.flags = sightFlags;
                if (work->playerSpotted != 0) {
                    work->sightBody.flags = sightFlags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->step            = GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK;
                    work->turnedAround    = 0;
                }
            }
            break;
        // Commit to a close swing or a running charge using the full X/Z distance.
        case GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            playerOffsetZ      = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            toPlayer->vz       = playerOffsetZ;
            playerOffsetX      = toPlayer->vx;
            if (SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < GOLEM_PAWN_ROOK_LUNGE_RANGE) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_LUNGE_ANIM;
            } else {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_CHARGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_WALK_ANIM;
                work->timer    = 0;
            }
            break;
        // A missed approach listens for contact, then retries facing after the hold.
        case GOLEM_PAWN_ROOK_ENGAGE_LISTEN:
            work->forwardSpeed    = 0;
            work->turnRate        = 0;
            listenSightFlags      = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sightBody.flags = listenSightFlags;
            if (work->playerSpotted != 0) {
                work->sightBody.flags = listenSightFlags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                work->anim            = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step            = GOLEM_PAWN_ROOK_ENGAGE_TRACK;
            } else if (work->animFrame >= GOLEM_PAWN_ROOK_ENGAGE_LISTEN_FRAMES) {
                toPlayer->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                toPlayer->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, rootCoord, toPlayer, listenYaw, listenYawDelta, absoluteListenYawDelta, wrappedListenYawDelta, yawError);
                if (yawError >= GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD) {
                    work->anim = GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TURN;
                } else {
                    work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TRACK;
                }
            }
            break;
        case GOLEM_PAWN_ROOK_ENGAGE_TURN:
            work->forwardSpeed = 0;
            work->turnRate     = GOLEM_PAWN_ROOK_ENGAGE_TURN_RATE;
            if (work->animFrame >= GOLEM_PAWN_ROOK_ENGAGE_TURN_FRAMES) {
                work->anim         = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step         = GOLEM_PAWN_ROOK_ENGAGE_TRACK;
                work->turnedAround = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);

#undef GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP
}

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_stagger.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_buildup.inc.c"

#include "../../shared/golem_pawn_rook_downed_death.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

/// Runs the Pawn GOLEM's attached Beam Sword lifecycle.
///
/// Requires a live sword Enemy in `spawnArg2.pointer`, its TMD model and parent
/// body work. The unchecked state is 0 attachment at body part 7, 1 visibility
/// mirroring and delayed trail spawn, or 2 destruction. Lighting is borrowed
/// from the body; sword teardown also releases its successfully spawned trail.
static void _actor02000SwordTask(Task* sword)
{
    const EnemyTaskFuncTable3 stateHandlers = Actor02000_D00060;

    stateHandlers.funcs[sword->state](sword->spawnArg2.pointer, sword);
}

#include "../../shared/golem_pawn_rook_sword_spawn.inc.c"

#include "../../shared/golem_pawn_rook_sword_tick.inc.c"

/// Runs the Beam Sword Pawn GOLEM body's lifecycle.
///
/// Requires a live body Enemy in `spawnArg2.pointer` and a nineteen-part model.
/// The unchecked state is 0 allocation/setup, 1 combat update, or 2 corpse and
/// teardown handling. Spawn owns the work shared by attached children; a handler
/// may destroy the Enemy/task pair, so nothing is read after dispatch.
static void _actor02000BodyTask(Task* body)
{
    const EnemyTaskFuncTable3 stateHandlers = Actor02000_D0006C;

    stateHandlers.funcs[body->state](body->spawnArg2.pointer, body);
}

static const EnemyTaskFuncTable3 Actor02000_D00060 = { {
    _golemPawnRookSwordSpawn,
    _golemPawnRookSwordTick,
    enemyDestroy,
} };

static const EnemyTaskFuncTable3 Actor02000_D0006C = { {
    _actor02000SpawnBody,
    _golemPawnRookFrameStateNoSparks,
    _golemPawnRookDeadState,
} };
