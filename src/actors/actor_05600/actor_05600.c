#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

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
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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
#include "../../shared/player_detection.h"
#define GOLEM_PAWN_ROOK_TYPE   GOLEM_PAWN
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_GRENADE_LAUNCHER
#include "../../shared/golem_pawn_rook.h"

/// Placement descriptor for this actor.
extern DamageAttack gGolemPawnRookAttacks[5];

/// Enemy parameters the approach cycle parks at `Enemy::param`; its `hpMax`
/// becomes the enemy's `hp`.
extern EnemyParams gGolemPawnRookParams[];

/// Per-stage tables of streaming cue ids, indexed by `GameSession::location.loc.stage`
/// and then `GameSession::location.loc.area`.
extern u16* gGolemPawnRookAreaParams[];

/// Spawn table the approach cycle starts its companion enemy from, index 1.
extern TaskDesc gGolemPawnRookTasks[];

/// Animation stream set bound into the work block's animation context.
extern AnimationSet* gGolemPawnRookAnimSets[31];

/// Sound id of the burst cue, with the spawn context's room/channel bits packed
/// in.
extern s32 gGolemPawnRookShotSound;

/// Frame counts of the actor's animations, indexed by `GolemPawnRookWork.anim`.
extern s16 gGolemPawnRookAnimBlendFrames[];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 gGolemPawnRookWeakPointWeapons[];
extern s16 gGolemPawnRookWeakPointPe[];

/// Sound ids of the actor's cues, indexed from `GolemPawnRookWork.soundSet`.
extern s32 gGolemPawnRookVoiceCues[];

/// The approach cycle's per-state handlers, indexed by `GolemPawnRookWork.behavior`.
extern TaskFunc gGolemPawnRookStates[];

static AnimationSet _gActor05600Actor105600Animation0B3CC;
static AnimationSet _gActor05600Actor105600Animation0BD34;
static AnimationSet _gActor05600Actor105600Animation0C35C;
static AnimationSet _gActor05600Actor105600Animation0CB5C;
static AnimationSet _gActor05600Actor105600Animation0D10C;
static AnimationSet _gActor05600Actor105600Animation0D5F0;
static AnimationSet _gActor05600Actor105600Animation0DA50;
static AnimationSet _gActor05600Actor105600Animation0EA28;
static AnimationSet _gActor05600Actor105600Animation0F980;
static AnimationSet _gActor05600Actor105600Animation0FEA4;
static AnimationSet _gActor05600Actor105600Animation108B4;
static AnimationSet _gActor05600Actor105600Animation1135C;
static AnimationSet _gActor05600Actor105600Animation1193C;
static AnimationSet _gActor05600Actor105600Animation12630;
static AnimationSet _gActor05600Actor105600Animation136A8;
static AnimationSet _gActor05600Actor105600Animation13B7C;
static AnimationSet _gActor05600Actor105600Animation13E8C;
static AnimationSet _gActor05600Actor105600Animation14068;
static AnimationSet _gActor05600Actor105600Animation14FB0;
static AnimationSet _gActor05600Actor105600Animation15544;
static AnimationSet _gActor05600Actor105600Animation1580C;
static AnimationSet _gActor05600Actor105600Animation159E8;
static AnimationSet _gActor05600Actor105600Animation16194;
static TmdSource    _gActor05600PawnGolemBody;
static TmdSource    _gActor05600GolemGrenadeLauncher;
static TmdSource    _gActor05600GolemGrenade;
void                Actor05600_Fn04A70(Task*);
void                Actor05600_Fn04BAC(Task*);
void                Actor05600_Fn04CA0(Task*);

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

static TmdBone _gActor05600PawnGolemBodySkeleton[19] = {
#include "assets/pawn_golem_body_skeleton.inc"
};

static u32 _gActor05600PawnGolemBodyPartVerts[19] = {
#include "assets/pawn_golem_body_partVerts.inc"
};

static SVECTOR _gActor05600PawnGolemBodyVerts[339] = {
#include "assets/pawn_golem_body_verts.inc"
};

static SVECTOR _gActor05600PawnGolemBodyNormals[346] = {
#include "assets/pawn_golem_body_normals.inc"
};

static u32 _gActor05600PawnGolemBodyStream[3745] = {
#include "assets/pawn_golem_body_stream.inc"
};

static TmdSource _gActor05600PawnGolemBody = {
    0,
    20476,
    5672,
    19,
    _gActor05600PawnGolemBodyPartVerts,
    _gActor05600PawnGolemBodyVerts,
    _gActor05600PawnGolemBodyNormals,
    _gActor05600PawnGolemBodySkeleton,
    _gActor05600PawnGolemBodyStream,
};

static TmdBone _gActor05600GolemGrenadeLauncherSkeleton[1] = {
#include "assets/golem_grenade_launcher_skeleton.inc"
};

static u32 _gActor05600GolemGrenadeLauncherPartVerts[1] = {
#include "assets/golem_grenade_launcher_partVerts.inc"
};

static SVECTOR _gActor05600GolemGrenadeLauncherVerts[24] = {
#include "assets/golem_grenade_launcher_verts.inc"
};

static SVECTOR _gActor05600GolemGrenadeLauncherNormals[24] = {
#include "assets/golem_grenade_launcher_normals.inc"
};

static u32 _gActor05600GolemGrenadeLauncherStream[160] = {
#include "assets/golem_grenade_launcher_stream.inc"
};

static TmdSource _gActor05600GolemGrenadeLauncher = {
    0,
    1144,
    0,
    1,
    _gActor05600GolemGrenadeLauncherPartVerts,
    _gActor05600GolemGrenadeLauncherVerts,
    _gActor05600GolemGrenadeLauncherNormals,
    _gActor05600GolemGrenadeLauncherSkeleton,
    _gActor05600GolemGrenadeLauncherStream,
};

static TmdBone _gActor05600GolemGrenadeSkeleton[1] = {
#include "assets/golem_grenade_skeleton.inc"
};

static u32 _gActor05600GolemGrenadePartVerts[1] = {
#include "assets/golem_grenade_partVerts.inc"
};

static SVECTOR _gActor05600GolemGrenadeVerts[12] = {
#include "assets/golem_grenade_verts.inc"
};

static SVECTOR _gActor05600GolemGrenadeNormals[28] = {
#include "assets/golem_grenade_normals.inc"
};

static u32 _gActor05600GolemGrenadeStream[104] = {
#include "assets/golem_grenade_stream.inc"
};

static TmdSource _gActor05600GolemGrenade = {
    0,
    720,
    0,
    1,
    _gActor05600GolemGrenadePartVerts,
    _gActor05600GolemGrenadeVerts,
    _gActor05600GolemGrenadeNormals,
    _gActor05600GolemGrenadeSkeleton,
    _gActor05600GolemGrenadeStream,
};

static AnimationPackedPose _gActor05600Actor105600Animation0B3CCBank1[21] = {
#include "assets/actor_105600_animation_0B3CC_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0B3CCBank4[317] = {
#include "assets/actor_105600_animation_0B3CC_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0B3CCRecords[382] = {
#include "assets/actor_105600_animation_0B3CC_records.inc"
};

static u16 _gActor05600Actor105600Animation0B3CCIndices[20] = {
#include "assets/actor_105600_animation_0B3CC_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0B3CC = {
    _gActor05600Actor105600Animation0B3CCRecords,
    _gActor05600Actor105600Animation0B3CCIndices,
    { NULL, _gActor05600Actor105600Animation0B3CCBank1, NULL, NULL, _gActor05600Actor105600Animation0B3CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0BD34Bank1[16] = {
#include "assets/actor_105600_animation_0BD34_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0BD34Bank4[237] = {
#include "assets/actor_105600_animation_0BD34_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0BD34Records[297] = {
#include "assets/actor_105600_animation_0BD34_records.inc"
};

static u16 _gActor05600Actor105600Animation0BD34Indices[20] = {
#include "assets/actor_105600_animation_0BD34_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0BD34 = {
    _gActor05600Actor105600Animation0BD34Records,
    _gActor05600Actor105600Animation0BD34Indices,
    { NULL, _gActor05600Actor105600Animation0BD34Bank1, NULL, NULL, _gActor05600Actor105600Animation0BD34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0C35CBank1[12] = {
#include "assets/actor_105600_animation_0C35C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0C35CBank4[142] = {
#include "assets/actor_105600_animation_0C35C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0C35CRecords[196] = {
#include "assets/actor_105600_animation_0C35C_records.inc"
};

static u16 _gActor05600Actor105600Animation0C35CIndices[20] = {
#include "assets/actor_105600_animation_0C35C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0C35C = {
    _gActor05600Actor105600Animation0C35CRecords,
    _gActor05600Actor105600Animation0C35CIndices,
    { NULL, _gActor05600Actor105600Animation0C35CBank1, NULL, NULL, _gActor05600Actor105600Animation0C35CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0CB5CBank1[14] = {
#include "assets/actor_105600_animation_0CB5C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0CB5CBank4[186] = {
#include "assets/actor_105600_animation_0CB5C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0CB5CRecords[264] = {
#include "assets/actor_105600_animation_0CB5C_records.inc"
};

static u16 _gActor05600Actor105600Animation0CB5CIndices[20] = {
#include "assets/actor_105600_animation_0CB5C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0CB5C = {
    _gActor05600Actor105600Animation0CB5CRecords,
    _gActor05600Actor105600Animation0CB5CIndices,
    { NULL, _gActor05600Actor105600Animation0CB5CBank1, NULL, NULL, _gActor05600Actor105600Animation0CB5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0D10CBank1[9] = {
#include "assets/actor_105600_animation_0D10C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0D10CBank4[139] = {
#include "assets/actor_105600_animation_0D10C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0D10CRecords[178] = {
#include "assets/actor_105600_animation_0D10C_records.inc"
};

static u16 _gActor05600Actor105600Animation0D10CIndices[20] = {
#include "assets/actor_105600_animation_0D10C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0D10C = {
    _gActor05600Actor105600Animation0D10CRecords,
    _gActor05600Actor105600Animation0D10CIndices,
    { NULL, _gActor05600Actor105600Animation0D10CBank1, NULL, NULL, _gActor05600Actor105600Animation0D10CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0D5F0Bank1[9] = {
#include "assets/actor_105600_animation_0D5F0_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0D5F0Bank4[107] = {
#include "assets/actor_105600_animation_0D5F0_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0D5F0Records[159] = {
#include "assets/actor_105600_animation_0D5F0_records.inc"
};

static u16 _gActor05600Actor105600Animation0D5F0Indices[20] = {
#include "assets/actor_105600_animation_0D5F0_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0D5F0 = {
    _gActor05600Actor105600Animation0D5F0Records,
    _gActor05600Actor105600Animation0D5F0Indices,
    { NULL, _gActor05600Actor105600Animation0D5F0Bank1, NULL, NULL, _gActor05600Actor105600Animation0D5F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0DA50Bank1[8] = {
#include "assets/actor_105600_animation_0DA50_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0DA50Bank4[101] = {
#include "assets/actor_105600_animation_0DA50_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0DA50Records[135] = {
#include "assets/actor_105600_animation_0DA50_records.inc"
};

static u16 _gActor05600Actor105600Animation0DA50Indices[20] = {
#include "assets/actor_105600_animation_0DA50_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0DA50 = {
    _gActor05600Actor105600Animation0DA50Records,
    _gActor05600Actor105600Animation0DA50Indices,
    { NULL, _gActor05600Actor105600Animation0DA50Bank1, NULL, NULL, _gActor05600Actor105600Animation0DA50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0EA28Bank1[29] = {
#include "assets/actor_105600_animation_0EA28_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0EA28Bank4[407] = {
#include "assets/actor_105600_animation_0EA28_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0EA28Records[500] = {
#include "assets/actor_105600_animation_0EA28_records.inc"
};

static u16 _gActor05600Actor105600Animation0EA28Indices[20] = {
#include "assets/actor_105600_animation_0EA28_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0EA28 = {
    _gActor05600Actor105600Animation0EA28Records,
    _gActor05600Actor105600Animation0EA28Indices,
    { NULL, _gActor05600Actor105600Animation0EA28Bank1, NULL, NULL, _gActor05600Actor105600Animation0EA28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0F980Bank1[27] = {
#include "assets/actor_105600_animation_0F980_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0F980Bank4[408] = {
#include "assets/actor_105600_animation_0F980_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0F980Records[473] = {
#include "assets/actor_105600_animation_0F980_records.inc"
};

static u16 _gActor05600Actor105600Animation0F980Indices[20] = {
#include "assets/actor_105600_animation_0F980_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0F980 = {
    _gActor05600Actor105600Animation0F980Records,
    _gActor05600Actor105600Animation0F980Indices,
    { NULL, _gActor05600Actor105600Animation0F980Bank1, NULL, NULL, _gActor05600Actor105600Animation0F980Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation0FEA4Bank1[8] = {
#include "assets/actor_105600_animation_0FEA4_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation0FEA4Bank4[116] = {
#include "assets/actor_105600_animation_0FEA4_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation0FEA4Records[169] = {
#include "assets/actor_105600_animation_0FEA4_records.inc"
};

static u16 _gActor05600Actor105600Animation0FEA4Indices[20] = {
#include "assets/actor_105600_animation_0FEA4_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation0FEA4 = {
    _gActor05600Actor105600Animation0FEA4Records,
    _gActor05600Actor105600Animation0FEA4Indices,
    { NULL, _gActor05600Actor105600Animation0FEA4Bank1, NULL, NULL, _gActor05600Actor105600Animation0FEA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation108B4Bank1[19] = {
#include "assets/actor_105600_animation_108B4_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation108B4Bank4[255] = {
#include "assets/actor_105600_animation_108B4_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation108B4Records[312] = {
#include "assets/actor_105600_animation_108B4_records.inc"
};

static u16 _gActor05600Actor105600Animation108B4Indices[20] = {
#include "assets/actor_105600_animation_108B4_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation108B4 = {
    _gActor05600Actor105600Animation108B4Records,
    _gActor05600Actor105600Animation108B4Indices,
    { NULL, _gActor05600Actor105600Animation108B4Bank1, NULL, NULL, _gActor05600Actor105600Animation108B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation1135CBank1[18] = {
#include "assets/actor_105600_animation_1135C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation1135CBank4[281] = {
#include "assets/actor_105600_animation_1135C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation1135CRecords[327] = {
#include "assets/actor_105600_animation_1135C_records.inc"
};

static u16 _gActor05600Actor105600Animation1135CIndices[20] = {
#include "assets/actor_105600_animation_1135C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation1135C = {
    _gActor05600Actor105600Animation1135CRecords,
    _gActor05600Actor105600Animation1135CIndices,
    { NULL, _gActor05600Actor105600Animation1135CBank1, NULL, NULL, _gActor05600Actor105600Animation1135CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation1193CBank1[8] = {
#include "assets/actor_105600_animation_1193C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation1193CBank4[130] = {
#include "assets/actor_105600_animation_1193C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation1193CRecords[202] = {
#include "assets/actor_105600_animation_1193C_records.inc"
};

static u16 _gActor05600Actor105600Animation1193CIndices[20] = {
#include "assets/actor_105600_animation_1193C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation1193C = {
    _gActor05600Actor105600Animation1193CRecords,
    _gActor05600Actor105600Animation1193CIndices,
    { NULL, _gActor05600Actor105600Animation1193CBank1, NULL, NULL, _gActor05600Actor105600Animation1193CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation12630Bank1[23] = {
#include "assets/actor_105600_animation_12630_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation12630Bank4[326] = {
#include "assets/actor_105600_animation_12630_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation12630Records[414] = {
#include "assets/actor_105600_animation_12630_records.inc"
};

static u16 _gActor05600Actor105600Animation12630Indices[20] = {
#include "assets/actor_105600_animation_12630_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation12630 = {
    _gActor05600Actor105600Animation12630Records,
    _gActor05600Actor105600Animation12630Indices,
    { NULL, _gActor05600Actor105600Animation12630Bank1, NULL, NULL, _gActor05600Actor105600Animation12630Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation136A8Bank1[31] = {
#include "assets/actor_105600_animation_136A8_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation136A8Bank4[429] = {
#include "assets/actor_105600_animation_136A8_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation136A8Records[512] = {
#include "assets/actor_105600_animation_136A8_records.inc"
};

static u16 _gActor05600Actor105600Animation136A8Indices[20] = {
#include "assets/actor_105600_animation_136A8_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation136A8 = {
    _gActor05600Actor105600Animation136A8Records,
    _gActor05600Actor105600Animation136A8Indices,
    { NULL, _gActor05600Actor105600Animation136A8Bank1, NULL, NULL, _gActor05600Actor105600Animation136A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation13B7CBank1[9] = {
#include "assets/actor_105600_animation_13B7C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation13B7CBank4[113] = {
#include "assets/actor_105600_animation_13B7C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation13B7CRecords[149] = {
#include "assets/actor_105600_animation_13B7C_records.inc"
};

static u16 _gActor05600Actor105600Animation13B7CIndices[20] = {
#include "assets/actor_105600_animation_13B7C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation13B7C = {
    _gActor05600Actor105600Animation13B7CRecords,
    _gActor05600Actor105600Animation13B7CIndices,
    { NULL, _gActor05600Actor105600Animation13B7CBank1, NULL, NULL, _gActor05600Actor105600Animation13B7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation13E8CBank1[5] = {
#include "assets/actor_105600_animation_13E8C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation13E8CBank4[65] = {
#include "assets/actor_105600_animation_13E8C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation13E8CRecords[96] = {
#include "assets/actor_105600_animation_13E8C_records.inc"
};

static u16 _gActor05600Actor105600Animation13E8CIndices[20] = {
#include "assets/actor_105600_animation_13E8C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation13E8C = {
    _gActor05600Actor105600Animation13E8CRecords,
    _gActor05600Actor105600Animation13E8CIndices,
    { NULL, _gActor05600Actor105600Animation13E8CBank1, NULL, NULL, _gActor05600Actor105600Animation13E8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation14068Bank1[2] = {
#include "assets/actor_105600_animation_14068_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation14068Bank4[17] = {
#include "assets/actor_105600_animation_14068_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation14068Records[76] = {
#include "assets/actor_105600_animation_14068_records.inc"
};

static u16 _gActor05600Actor105600Animation14068Indices[20] = {
#include "assets/actor_105600_animation_14068_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation14068 = {
    _gActor05600Actor105600Animation14068Records,
    _gActor05600Actor105600Animation14068Indices,
    { NULL, _gActor05600Actor105600Animation14068Bank1, NULL, NULL, _gActor05600Actor105600Animation14068Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation14FB0Bank1[28] = {
#include "assets/actor_105600_animation_14FB0_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation14FB0Bank4[394] = {
#include "assets/actor_105600_animation_14FB0_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation14FB0Records[480] = {
#include "assets/actor_105600_animation_14FB0_records.inc"
};

static u16 _gActor05600Actor105600Animation14FB0Indices[20] = {
#include "assets/actor_105600_animation_14FB0_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation14FB0 = {
    _gActor05600Actor105600Animation14FB0Records,
    _gActor05600Actor105600Animation14FB0Indices,
    { NULL, _gActor05600Actor105600Animation14FB0Bank1, NULL, NULL, _gActor05600Actor105600Animation14FB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation15544Bank1[9] = {
#include "assets/actor_105600_animation_15544_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation15544Bank4[127] = {
#include "assets/actor_105600_animation_15544_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation15544Records[183] = {
#include "assets/actor_105600_animation_15544_records.inc"
};

static u16 _gActor05600Actor105600Animation15544Indices[20] = {
#include "assets/actor_105600_animation_15544_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation15544 = {
    _gActor05600Actor105600Animation15544Records,
    _gActor05600Actor105600Animation15544Indices,
    { NULL, _gActor05600Actor105600Animation15544Bank1, NULL, NULL, _gActor05600Actor105600Animation15544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation1580CBank1[5] = {
#include "assets/actor_105600_animation_1580C_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation1580CBank4[56] = {
#include "assets/actor_105600_animation_1580C_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation1580CRecords[87] = {
#include "assets/actor_105600_animation_1580C_records.inc"
};

static u16 _gActor05600Actor105600Animation1580CIndices[20] = {
#include "assets/actor_105600_animation_1580C_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation1580C = {
    _gActor05600Actor105600Animation1580CRecords,
    _gActor05600Actor105600Animation1580CIndices,
    { NULL, _gActor05600Actor105600Animation1580CBank1, NULL, NULL, _gActor05600Actor105600Animation1580CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation159E8Bank1[2] = {
#include "assets/actor_105600_animation_159E8_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation159E8Bank4[17] = {
#include "assets/actor_105600_animation_159E8_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation159E8Records[76] = {
#include "assets/actor_105600_animation_159E8_records.inc"
};

static u16 _gActor05600Actor105600Animation159E8Indices[20] = {
#include "assets/actor_105600_animation_159E8_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation159E8 = {
    _gActor05600Actor105600Animation159E8Records,
    _gActor05600Actor105600Animation159E8Indices,
    { NULL, _gActor05600Actor105600Animation159E8Bank1, NULL, NULL, _gActor05600Actor105600Animation159E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05600Actor105600Animation16194Bank1[15] = {
#include "assets/actor_105600_animation_16194_bank1.inc"
};

static AnimationPackedRotation _gActor05600Actor105600Animation16194Bank4[175] = {
#include "assets/actor_105600_animation_16194_bank4.inc"
};

static AnimationRecord _gActor05600Actor105600Animation16194Records[251] = {
#include "assets/actor_105600_animation_16194_records.inc"
};

static u16 _gActor05600Actor105600Animation16194Indices[20] = {
#include "assets/actor_105600_animation_16194_indices.inc"
};

static AnimationSet _gActor05600Actor105600Animation16194 = {
    _gActor05600Actor105600Animation16194Records,
    _gActor05600Actor105600Animation16194Indices,
    { NULL, _gActor05600Actor105600Animation16194Bank1, NULL, NULL, _gActor05600Actor105600Animation16194Bank4, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 30, 5 },
    { 20, 5 },
    { 0, 8 },
    { 18, 1 },
    { 30, 7 },
};

EnemyParams gGolemPawnRookParams[1] = {
    { gGolemPawnRookAttacks, 425, 125, 100, 5, 50, 6, 0, 0 },
};

s16 gGolemPawnRookWeakPointWeapons[46] = {
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

s16 gGolemPawnRookWeakPointPe[56] = {
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
    0x40380001,
    0x40380002,
    0x40380003,
    0x40380004,
    0x40380005,
    0x40380006,
    0x4038000C,
    0x4038000D,
    0x40380009,
    0x4038000A,
    0x4038000B,
    0x4038000E,
    0x4038000F,
    0x40380010,
    0x40380011,
    0x40380012,
};

s32 gGolemPawnRookShotSound = 0x40380007;

/// Sound id of the cue a grenade plays where its flight ends; the bullet ORs
/// the firing enemy's place index into bits 8 and up before queueing it.
s32 gGolemPawnRookImpactSound = 0x40380008;

/* The three cue ids every Rook build defines at this position: its scream,
 * silence and burst. The Pawn is built without the code that plays them, so
 * nothing in this package reads these and each holds no id. */
s32 gGolemPawnRookScreamCue = 0;

s32 gGolemPawnRookSilenceCue = 0;

s32 gGolemPawnRookBurstCue = 0;

u16 Actor05600_D16304[22] = {
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

u16 Actor05600_D16330[40] = {
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

u16 Actor05600_D16380[40] = {
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

u16 Actor05600_D163D0[50] = {
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

u16 Actor05600_D16434[34] = {
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

u16* gGolemPawnRookAreaParams[6] = {
    NULL,
    Actor05600_D16304,
    Actor05600_D16330,
    Actor05600_D16380,
    Actor05600_D163D0,
    Actor05600_D16434,
};

s16 gGolemPawnRookBeamRibbonCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc gGolemPawnRookTasks[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor05600_Fn04CA0, { .model = &_gActor05600PawnGolemBody } },
    { { { TASK_BODY_TMD, 96 } }, Actor05600_Fn04A70, { .model = &_gActor05600GolemGrenadeLauncher } },
};

TaskDesc Actor05600_D164B8 = { { { TASK_BODY_TMD, 96 } }, Actor05600_Fn04BAC, { .model = &_gActor05600GolemGrenade } };

AnimationSet* gGolemPawnRookAnimSets[31] = {
    NULL,
    &_gActor05600Actor105600Animation0B3CC,
    &_gActor05600Actor105600Animation0BD34,
    &_gActor05600Actor105600Animation0C35C,
    &_gActor05600Actor105600Animation0CB5C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor05600Actor105600Animation0D10C,
    &_gActor05600Actor105600Animation0D5F0,
    &_gActor05600Actor105600Animation0DA50,
    &_gActor05600Actor105600Animation0EA28,
    &_gActor05600Actor105600Animation0F980,
    &_gActor05600Actor105600Animation0FEA4,
    &_gActor05600Actor105600Animation108B4,
    &_gActor05600Actor105600Animation1135C,
    &_gActor05600Actor105600Animation1193C,
    &_gActor05600Actor105600Animation12630,
    &_gActor05600Actor105600Animation136A8,
    &_gActor05600Actor105600Animation13B7C,
    &_gActor05600Actor105600Animation13E8C,
    &_gActor05600Actor105600Animation14068,
    &_gActor05600Actor105600Animation14FB0,
    &_gActor05600Actor105600Animation15544,
    &_gActor05600Actor105600Animation1580C,
    &_gActor05600Actor105600Animation159E8,
    &_gActor05600Actor105600Animation16194,
};

TaskFunc gGolemPawnRookStates[15] = {
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    golemPawnRookLungeCycle,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookCompanionCycle,
    golemPawnRookLungeStrikeState,
    golemPawnRookHitReactionState,
    golemPawnRookRecoilState,
    golemPawnRookFlagWaitState,
    golemPawnRookKnockdownState,
    golemPawnRookDownedShiftState,
    golemPawnRookCollapseState,
    golemPawnRookDownedFinishState,
};

/// Corner indices of the two ribbon polygons in the beam scratch's
/// projected-point arrays.
extern s16 gGolemPawnRookBeamRibbonCorners[][4];

#include "../../shared/golem_pawn_rook_hit_tick.inc.c"

#include "../../shared/golem_pawn_rook_approach.inc.c"

#include "../../shared/golem_pawn_rook_proximity.inc.c"

#include "../../shared/golem_pawn_rook_knockdown.inc.c"

#include "../../shared/golem_pawn_rook_downed_shift.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

#include "../../shared/golem_pawn_rook_companion_cycle.inc.c"

#include "../../shared/golem_pawn_rook_lunge_strike.inc.c"

#include "../../shared/golem_pawn_rook_laser_sight.inc.c"

#include "../../shared/golem_pawn_rook_laser_beam.inc.c"

#include "../../shared/golem_pawn_rook_bullet_spawn.inc.c"

#include "../../shared/golem_pawn_rook_bullet_fly.inc.c"

#include "../../shared/golem_pawn_rook_spawn.inc.c"

#include "../../shared/golem_pawn_rook_inlines.inc.c"

/// Saves the root coordinate's translation in `prevRootPos`, then
/// Updates the enemy's colour from `coord`'s world position and draws the
#include "../../shared/golem_pawn_rook_frame_no_dust.inc.c"

#include "../../shared/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_hit_reaction.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_flag_wait.inc.c"

#include "../../shared/golem_pawn_rook_downed_finish.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

/// State handlers of the model child hung off the actor's part 7 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05600_Fn04A70`.
static const EnemyTaskFuncTable3 Actor05600_D00080 = {
    golemPawnRookGunSpawn,
    golemPawnRookGunTick,
    enemyDestroy,
};

void Actor05600_Fn04A70(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor05600_D00080;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_gun_spawn.inc.c"

#include "../../shared/golem_pawn_rook_gun_tick.inc.c"

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
static const EnemyTaskFuncTable3 Actor05600_D0008C = {
    golemPawnRookBulletSpawn,
    golemPawnRookBulletFly,
    golemPawnRookBulletDestroy,
};

void Actor05600_Fn04BAC(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor05600_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_bullet_destroy.inc.c"

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
static const EnemyTaskFuncTable3 Actor05600_D00098 = {
    golemPawnRookSpawn,
    golemPawnRookFrameStateNoDust,
    golemPawnRookDeadState,
};

void Actor05600_Fn04CA0(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor05600_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
