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
#include "gameplay/object_fields.h"
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

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

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

void Actor02000_Fn02D5C(Task*);

void Actor02000_Fn035E8(Task*);

void Actor02000_Fn03728(Task*);

extern AnimationSet* Actor02000_D15FE8[31];

extern TaskDesc Actor02000_D15FD0[];

extern u16* Actor02000_D15FB8[];

extern EnemyParams Actor02000_D15D10;

extern TaskFunc gGolemPawnRookStates[];

static void Actor02000_Fn0251C(Enemy* ctx, Task* actor);

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
    { { { TASK_BODY_TMD, 96 } }, Actor02000_Fn03728, { .model = &_gActor02000PawnGolemBody } },
    { { { TASK_BODY_TMD, 96 } }, Actor02000_Fn035E8, { .model = &_gActor02000GolemBeamSword } },
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
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    Actor02000_Fn02D5C,
    golemPawnRookChargeState,
    golemPawnRookBeamSwingState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookHitReactionState,
    golemPawnRookRecoilState,
    golemPawnRookFlagWaitState,
    golemPawnRookKnockdownState,
    golemPawnRookDownedShiftState,
    golemPawnRookCollapseState,
    golemPawnRookDownedFinishState,
};

#include "../../shared/golem_pawn_rook_approach.inc.c"

#include "../../shared/golem_pawn_rook_proximity.inc.c"

#include "../../shared/golem_pawn_rook_knockdown.inc.c"

#include "../../shared/golem_pawn_rook_downed_shift.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

#include "../../shared/golem_pawn_rook_charge_state.inc.c"

#include "../../shared/golem_pawn_rook_beam_swing.inc.c"

/// Enemy init. Allocates the `GolemPawnRookWork` block, points the model object at
/// the light / color matrices inside it, runs the animation context over its
/// nineteen slots, and spawns the companion enemy from `Actor02000_D15FD0`,
/// copying that model's texture page and CLUT row out of the current area
/// record. `Enemy.spawnState` then selects the variant: 0 builds the
/// full object set (list node, the four `worldCollisionLinkBody` nodes and their
/// `WorldCollisionContact` tables, and the optional CD prefetch of `soundSet`),
/// while 1 and 2 only prime the animation state and hand the task to state 2.
static void Actor02000_Fn0251C(Enemy* ctx, Task* actor)
{
    GolemPawnRookWork* work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          parts;
    GfxCoord*          partsA;
    GfxCoord*          partsB;
    GfxCoord*          partsC;
    GfxCoord*          effParts;
    Enemy*             eff;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                param;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(GolemPawnRookWork), 0);
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work                   = work;
    obj->flags                    = 0;
    coord->composeStamp           = GRAPHICS_COORD_DIRTY;
    obj->lightMtx                 = &work->lightMtx;
    obj->colorMtx                 = &work->colorMtx;
    work->actorId                 = 0x14;
    work->taskTable               = Actor02000_D15FD0;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, Actor02000_D15FE8, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    eff = Gp_SpawnEnemyFromTable(Actor02000_D15FD0, 1, 0, ctx);
    actorTintTask(eff->task, ctx);

    switch (ctx->spawnState) {
        case 0:
            ctx->field_4  = &coord->coord;
            ctx->field_48 = 0;
            worldTargetLinkNode(&ctx->node);
            parts           = actor->extra.tmd->coords;
            ctx->bodyPos.vx = 0;
            ctx->bodyPos.vy = 0;
            ctx->bodyPos.vz = 0;
            ctx->param      = &Actor02000_D15D10;
            ctx->recs       = work->hurtContacts;
            ctx->coord      = &parts[3];
            ctx->hp         = Actor02000_D15D10.hpMax;
            sceneAcquireBattleRef(0);
            work->patrols = ctx->place->mode & 1;
            if (work->patrols == 0) {
                work->anim     = 1;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = 2;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                param                    = ctx->place->variant;
                work->patrolDistanceLeft = param * 1000;
            }

            tbl = Actor02000_D15FB8[gGameSession->location.loc.stage];
            if (tbl != NULL) {
                work->soundSet = tbl[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                param1[3] = 0;
                param1[2] = 0xA;
                param1[0] = work->soundSet;
                param2[0] = 0x14;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            }

            work->sightCapsule.ends[0].vz   = 0x1F40;
            work->sightCapsule.end0Radius   = 0x3E8;
            work->sightCapsule.ends[0].vx   = 0;
            work->sightCapsule.ends[0].vy   = 0;
            work->sightCapsule.ends[1].vx   = 0;
            work->sightCapsule.ends[1].vy   = 0;
            work->sightCapsule.ends[1].vz   = 0;
            work->sightCapsule.end1Radius   = 0x5DC;
            work->sightCapsule.contacts     = work->sightContacts;
            partsA                          = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &partsA[4];
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody);
            worldCollisionInitContacts(work->sightContacts, ARRAY_SIZE(work->sightContacts), 0);
            work->sightBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            partsB                          = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = 0x30014;
            work->hurtBody.radius           = 0x190;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &partsB[3];
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody);
            worldCollisionInitContacts(work->hurtContacts, ARRAY_SIZE(work->hurtContacts), 0);
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

            partsC                            = actor->extra.tmd->coords;
            work->groundBody.pos.vy           = -0x226;
            work->groundBody.context.contacts = work->groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0;
            work->groundBody.radius           = 0x226;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->groundBody.coord            = partsC;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody);
            worldCollisionInitContacts(work->groundContacts, ARRAY_SIZE(work->groundContacts), 0);
            work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            effParts                          = eff->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = 0x1F4;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = 0x1F4;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = effParts;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody);
            worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            actor->state            = 1;
            break;
        case 1:
            work->anim   = 0x19;
            work->step   = 2;
            actor->state = 2;
            break;
        case 2:
            work->anim   = 0x1D;
            work->step   = 2;
            actor->state = 2;
            break;
    }
}

#include "../../shared/golem_pawn_rook_inlines.inc.c"

/// Saves the root coordinate's translation in `prevRootPos`, then
/// Updates the enemy's colour from `coord`'s world position and draws the
#include "../../shared/golem_pawn_rook_frame_no_dust.inc.c"

void Actor02000_Fn02D5C(Task* arg0)
{
    s16                yaw;
    s16                yaw2;
    s16                state;
    s16                deltaYaw;
    s16                deltaYaw2;
    s16                speed;
    s32                magnitude;
    s32                magnitude2;
    s16                wrapped;
    s16                wrapped2;
    s16                angle;
    s32                dx;
    s32                dz;
    u16                flags;
    u16                flags2;
    u8*                head;
    VECTOR*            delta;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    head                     = SCRATCH_STACK_CURSOR(u8);
    delta                    = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)delta;
    work                     = arg0->work;
    state                    = work->step;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim]) {
                speed = 0x14;
            }
            work->forwardSpeed = speed;
            work->turnRate     = 0x3C;
            delta->vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw    = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw                = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->yaw          = yaw;
            deltaYaw           = work->targetYaw - yaw;
            magnitude          = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->turnedAround == 0) {
                    work->anim = 3;
                    work->step = 3;
                } else {
                    work->anim         = 4;
                    work->step         = 1;
                    work->turnedAround = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sightBody.flags = flags;
                if (work->playerSpotted != 0) {
                    work->sightBody.flags = (u16)(flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->step            = 1;
                    work->turnedAround    = 0;
                }
            }
            break;
        case 1:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            delta->vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            dz                 = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz          = dz;
            dx                 = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING;
                work->step     = 0;
                work->anim     = 8;
            } else {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_CHARGE;
                work->step     = 0;
                work->anim     = 5;
                work->timer    = 0;
            }
            break;
        case 2:
            work->forwardSpeed    = 0;
            work->turnRate        = 0;
            flags2                = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sightBody.flags = flags2;
            if (work->playerSpotted != 0) {
                work->sightBody.flags = (u16)(flags2 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->anim            = 2;
                work->step            = 0;
            } else if (work->animFrame >= 0x60) {
                delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->targetYaw = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->yaw       = yaw2;
                deltaYaw2       = work->targetYaw - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->anim = 3;
                    work->step = 3;
                } else {
                    work->anim = 2;
                    work->step = 0;
                }
            }
            break;
        case 3:
            work->forwardSpeed = 0;
            work->turnRate     = 0x3B;
            if (work->animFrame >= 0x23) {
                work->anim         = 2;
                work->step         = 0;
                work->turnedAround = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_hit_reaction.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_flag_wait.inc.c"

#include "../../shared/golem_pawn_rook_downed_finish.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

void Actor02000_Fn035E8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02000_D00060;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

#include "../../shared/golem_pawn_rook_delayed_effect_spawn.inc.c"

#include "../../shared/golem_pawn_rook_delayed_effect_tick.inc.c"

void Actor02000_Fn03728(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02000_D0006C;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static const EnemyTaskFuncTable3 Actor02000_D00060 = { {
    golemPawnRookDelayedEffectSpawn,
    golemPawnRookDelayedEffectTick,
    enemyDestroy,
} };

static const EnemyTaskFuncTable3 Actor02000_D0006C = { {
    Actor02000_Fn0251C,
    golemPawnRookFrameStateNoDust,
    golemPawnRookDeadState,
} };
