#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

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
#include "gameplay/player_state.h"
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
#include "main/random.h"
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
// Exported instance: rooms spawn from this package's table by name.
#define gGolemPawnRookTasks gActor05700GolemPawnRookTasks
#include "../../shared/player_detection.h"
#define GOLEM_PAWN_ROOK_TYPE   GOLEM_ROOK
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_GRENADE_LAUNCHER
#include "../../shared/golem_pawn_rook.h"

/// Sound ids this actor's cues play, indexed by `GolemPawnRookWork.soundSet`
/// (row `soundSet` starts at the second word, the `- 1` in the body).
extern s32 gGolemPawnRookVoiceCues[];

/// Per-animation frame marks: row `anim` holds the frame the 0x1C, 0x28
/// and 0x7A marks of `golemPawnRookLungeStrikeState` are measured from.
extern s16 gGolemPawnRookAnimBlendFrames[];

/// The body objects' variant flag comes from `gGolemPawnRookAttacks`.
extern DamageAttack gGolemPawnRookAttacks[5];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 gGolemPawnRookWeakPointWeapons[];
extern s16 gGolemPawnRookWeakPointPe[];

static AnimationSet _gActor05700Actor105700Animation0BAAC;
static AnimationSet _gActor05700Actor105700Animation0C414;
static AnimationSet _gActor05700Actor105700Animation0CA3C;
static AnimationSet _gActor05700Actor105700Animation0D23C;
static AnimationSet _gActor05700Actor105700Animation0D444;
static AnimationSet _gActor05700Actor105700Animation0DA50;
static AnimationSet _gActor05700Actor105700Animation0DF80;
static AnimationSet _gActor05700Actor105700Animation0E464;
static AnimationSet _gActor05700Actor105700Animation0E8C4;
static AnimationSet _gActor05700Actor105700Animation0F980;
static AnimationSet _gActor05700Actor105700Animation108D8;
static AnimationSet _gActor05700Actor105700Animation10DFC;
static AnimationSet _gActor05700Actor105700Animation1180C;
static AnimationSet _gActor05700Actor105700Animation122B4;
static AnimationSet _gActor05700Actor105700Animation12894;
static AnimationSet _gActor05700Actor105700Animation13588;
static AnimationSet _gActor05700Actor105700Animation14600;
static AnimationSet _gActor05700Actor105700Animation14AD4;
static AnimationSet _gActor05700Actor105700Animation14DE4;
static AnimationSet _gActor05700Actor105700Animation14FC0;
static AnimationSet _gActor05700Actor105700Animation15F08;
static AnimationSet _gActor05700Actor105700Animation1649C;
static AnimationSet _gActor05700Actor105700Animation16764;
static AnimationSet _gActor05700Actor105700Animation16940;
static AnimationSet _gActor05700Actor105700Animation170CC;
static TmdSource    _gActor05700RookGolemBody;
static TmdSource    _gActor05700GolemGrenadeLauncher;
static TmdSource    _gActor05700RookGolemShield;
static TmdSource    _gActor05700GolemGrenade;
static void         _actor05700GrenadeLauncherTask(Task* launcher);
static void         _actor05700GrenadeTask(Task* grenade);
static void         _actor05700ShieldTask(Task* shield);
static void         _actor05700RookGolemTask(Task* golem);

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

static TmdBone _gActor05700RookGolemBodySkeleton[19] = {
#include "assets/rook_golem_body_skeleton.inc"
};

static u32 _gActor05700RookGolemBodyPartVerts[19] = {
#include "assets/rook_golem_body_partVerts.inc"
};

static SVECTOR _gActor05700RookGolemBodyVerts[328] = {
#include "assets/rook_golem_body_verts.inc"
};

static SVECTOR _gActor05700RookGolemBodyNormals[344] = {
#include "assets/rook_golem_body_normals.inc"
};

static u32 _gActor05700RookGolemBodyStream[3509] = {
#include "assets/rook_golem_body_stream.inc"
};

static TmdSource _gActor05700RookGolemBody = {
    0,
    19580,
    4888,
    19,
    _gActor05700RookGolemBodyPartVerts,
    _gActor05700RookGolemBodyVerts,
    _gActor05700RookGolemBodyNormals,
    _gActor05700RookGolemBodySkeleton,
    _gActor05700RookGolemBodyStream,
};

static TmdBone _gActor05700GolemGrenadeLauncherSkeleton[1] = {
#include "assets/golem_grenade_launcher_skeleton.inc"
};

static u32 _gActor05700GolemGrenadeLauncherPartVerts[1] = {
#include "assets/golem_grenade_launcher_partVerts.inc"
};

static SVECTOR _gActor05700GolemGrenadeLauncherVerts[24] = {
#include "assets/golem_grenade_launcher_verts.inc"
};

static SVECTOR _gActor05700GolemGrenadeLauncherNormals[24] = {
#include "assets/golem_grenade_launcher_normals.inc"
};

static u32 _gActor05700GolemGrenadeLauncherStream[160] = {
#include "assets/golem_grenade_launcher_stream.inc"
};

static TmdSource _gActor05700GolemGrenadeLauncher = {
    0,
    1144,
    0,
    1,
    _gActor05700GolemGrenadeLauncherPartVerts,
    _gActor05700GolemGrenadeLauncherVerts,
    _gActor05700GolemGrenadeLauncherNormals,
    _gActor05700GolemGrenadeLauncherSkeleton,
    _gActor05700GolemGrenadeLauncherStream,
};

static TmdBone _gActor05700RookGolemShieldSkeleton[1] = {
#include "assets/rook_golem_shield_skeleton.inc"
};

static u32 _gActor05700RookGolemShieldPartVerts[1] = {
#include "assets/rook_golem_shield_partVerts.inc"
};

static SVECTOR _gActor05700RookGolemShieldVerts[16] = {
#include "assets/rook_golem_shield_verts.inc"
};

static SVECTOR _gActor05700RookGolemShieldNormals[20] = {
#include "assets/rook_golem_shield_normals.inc"
};

static u32 _gActor05700RookGolemShieldStream[111] = {
#include "assets/rook_golem_shield_stream.inc"
};

static TmdSource _gActor05700RookGolemShield = {
    0,
    780,
    0,
    1,
    _gActor05700RookGolemShieldPartVerts,
    _gActor05700RookGolemShieldVerts,
    _gActor05700RookGolemShieldNormals,
    _gActor05700RookGolemShieldSkeleton,
    _gActor05700RookGolemShieldStream,
};

static TmdBone _gActor05700GolemGrenadeSkeleton[1] = {
#include "assets/golem_grenade_skeleton.inc"
};

static u32 _gActor05700GolemGrenadePartVerts[1] = {
#include "assets/golem_grenade_partVerts.inc"
};

static SVECTOR _gActor05700GolemGrenadeVerts[12] = {
#include "assets/golem_grenade_verts.inc"
};

static SVECTOR _gActor05700GolemGrenadeNormals[28] = {
#include "assets/golem_grenade_normals.inc"
};

static u32 _gActor05700GolemGrenadeStream[104] = {
#include "assets/golem_grenade_stream.inc"
};

static TmdSource _gActor05700GolemGrenade = {
    0,
    720,
    0,
    1,
    _gActor05700GolemGrenadePartVerts,
    _gActor05700GolemGrenadeVerts,
    _gActor05700GolemGrenadeNormals,
    _gActor05700GolemGrenadeSkeleton,
    _gActor05700GolemGrenadeStream,
};

static AnimationPackedPose _gActor05700Actor105700Animation0BAACBank1[21] = {
#include "assets/actor_105700_animation_0BAAC_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0BAACBank4[317] = {
#include "assets/actor_105700_animation_0BAAC_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0BAACRecords[382] = {
#include "assets/actor_105700_animation_0BAAC_records.inc"
};

static u16 _gActor05700Actor105700Animation0BAACIndices[20] = {
#include "assets/actor_105700_animation_0BAAC_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0BAAC = {
    _gActor05700Actor105700Animation0BAACRecords,
    _gActor05700Actor105700Animation0BAACIndices,
    { NULL, _gActor05700Actor105700Animation0BAACBank1, NULL, NULL, _gActor05700Actor105700Animation0BAACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0C414Bank1[16] = {
#include "assets/actor_105700_animation_0C414_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0C414Bank4[237] = {
#include "assets/actor_105700_animation_0C414_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0C414Records[297] = {
#include "assets/actor_105700_animation_0C414_records.inc"
};

static u16 _gActor05700Actor105700Animation0C414Indices[20] = {
#include "assets/actor_105700_animation_0C414_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0C414 = {
    _gActor05700Actor105700Animation0C414Records,
    _gActor05700Actor105700Animation0C414Indices,
    { NULL, _gActor05700Actor105700Animation0C414Bank1, NULL, NULL, _gActor05700Actor105700Animation0C414Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0CA3CBank1[12] = {
#include "assets/actor_105700_animation_0CA3C_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0CA3CBank4[142] = {
#include "assets/actor_105700_animation_0CA3C_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0CA3CRecords[196] = {
#include "assets/actor_105700_animation_0CA3C_records.inc"
};

static u16 _gActor05700Actor105700Animation0CA3CIndices[20] = {
#include "assets/actor_105700_animation_0CA3C_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0CA3C = {
    _gActor05700Actor105700Animation0CA3CRecords,
    _gActor05700Actor105700Animation0CA3CIndices,
    { NULL, _gActor05700Actor105700Animation0CA3CBank1, NULL, NULL, _gActor05700Actor105700Animation0CA3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0D23CBank1[14] = {
#include "assets/actor_105700_animation_0D23C_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0D23CBank4[186] = {
#include "assets/actor_105700_animation_0D23C_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0D23CRecords[264] = {
#include "assets/actor_105700_animation_0D23C_records.inc"
};

static u16 _gActor05700Actor105700Animation0D23CIndices[20] = {
#include "assets/actor_105700_animation_0D23C_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0D23C = {
    _gActor05700Actor105700Animation0D23CRecords,
    _gActor05700Actor105700Animation0D23CIndices,
    { NULL, _gActor05700Actor105700Animation0D23CBank1, NULL, NULL, _gActor05700Actor105700Animation0D23CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0D444Bank1[3] = {
#include "assets/actor_105700_animation_0D444_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0D444Bank4[25] = {
#include "assets/actor_105700_animation_0D444_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0D444Records[76] = {
#include "assets/actor_105700_animation_0D444_records.inc"
};

static u16 _gActor05700Actor105700Animation0D444Indices[20] = {
#include "assets/actor_105700_animation_0D444_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0D444 = {
    _gActor05700Actor105700Animation0D444Records,
    _gActor05700Actor105700Animation0D444Indices,
    { NULL, _gActor05700Actor105700Animation0D444Bank1, NULL, NULL, _gActor05700Actor105700Animation0D444Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0DA50Bank1[9] = {
#include "assets/actor_105700_animation_0DA50_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0DA50Bank4[151] = {
#include "assets/actor_105700_animation_0DA50_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0DA50Records[189] = {
#include "assets/actor_105700_animation_0DA50_records.inc"
};

static u16 _gActor05700Actor105700Animation0DA50Indices[20] = {
#include "assets/actor_105700_animation_0DA50_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0DA50 = {
    _gActor05700Actor105700Animation0DA50Records,
    _gActor05700Actor105700Animation0DA50Indices,
    { NULL, _gActor05700Actor105700Animation0DA50Bank1, NULL, NULL, _gActor05700Actor105700Animation0DA50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0DF80Bank1[9] = {
#include "assets/actor_105700_animation_0DF80_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0DF80Bank4[125] = {
#include "assets/actor_105700_animation_0DF80_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0DF80Records[160] = {
#include "assets/actor_105700_animation_0DF80_records.inc"
};

static u16 _gActor05700Actor105700Animation0DF80Indices[20] = {
#include "assets/actor_105700_animation_0DF80_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0DF80 = {
    _gActor05700Actor105700Animation0DF80Records,
    _gActor05700Actor105700Animation0DF80Indices,
    { NULL, _gActor05700Actor105700Animation0DF80Bank1, NULL, NULL, _gActor05700Actor105700Animation0DF80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0E464Bank1[9] = {
#include "assets/actor_105700_animation_0E464_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0E464Bank4[107] = {
#include "assets/actor_105700_animation_0E464_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0E464Records[159] = {
#include "assets/actor_105700_animation_0E464_records.inc"
};

static u16 _gActor05700Actor105700Animation0E464Indices[20] = {
#include "assets/actor_105700_animation_0E464_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0E464 = {
    _gActor05700Actor105700Animation0E464Records,
    _gActor05700Actor105700Animation0E464Indices,
    { NULL, _gActor05700Actor105700Animation0E464Bank1, NULL, NULL, _gActor05700Actor105700Animation0E464Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0E8C4Bank1[8] = {
#include "assets/actor_105700_animation_0E8C4_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0E8C4Bank4[101] = {
#include "assets/actor_105700_animation_0E8C4_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0E8C4Records[135] = {
#include "assets/actor_105700_animation_0E8C4_records.inc"
};

static u16 _gActor05700Actor105700Animation0E8C4Indices[20] = {
#include "assets/actor_105700_animation_0E8C4_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0E8C4 = {
    _gActor05700Actor105700Animation0E8C4Records,
    _gActor05700Actor105700Animation0E8C4Indices,
    { NULL, _gActor05700Actor105700Animation0E8C4Bank1, NULL, NULL, _gActor05700Actor105700Animation0E8C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation0F980Bank1[30] = {
#include "assets/actor_105700_animation_0F980_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation0F980Bank4[430] = {
#include "assets/actor_105700_animation_0F980_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation0F980Records[531] = {
#include "assets/actor_105700_animation_0F980_records.inc"
};

static u16 _gActor05700Actor105700Animation0F980Indices[20] = {
#include "assets/actor_105700_animation_0F980_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation0F980 = {
    _gActor05700Actor105700Animation0F980Records,
    _gActor05700Actor105700Animation0F980Indices,
    { NULL, _gActor05700Actor105700Animation0F980Bank1, NULL, NULL, _gActor05700Actor105700Animation0F980Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation108D8Bank1[27] = {
#include "assets/actor_105700_animation_108D8_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation108D8Bank4[408] = {
#include "assets/actor_105700_animation_108D8_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation108D8Records[473] = {
#include "assets/actor_105700_animation_108D8_records.inc"
};

static u16 _gActor05700Actor105700Animation108D8Indices[20] = {
#include "assets/actor_105700_animation_108D8_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation108D8 = {
    _gActor05700Actor105700Animation108D8Records,
    _gActor05700Actor105700Animation108D8Indices,
    { NULL, _gActor05700Actor105700Animation108D8Bank1, NULL, NULL, _gActor05700Actor105700Animation108D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation10DFCBank1[8] = {
#include "assets/actor_105700_animation_10DFC_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation10DFCBank4[116] = {
#include "assets/actor_105700_animation_10DFC_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation10DFCRecords[169] = {
#include "assets/actor_105700_animation_10DFC_records.inc"
};

static u16 _gActor05700Actor105700Animation10DFCIndices[20] = {
#include "assets/actor_105700_animation_10DFC_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation10DFC = {
    _gActor05700Actor105700Animation10DFCRecords,
    _gActor05700Actor105700Animation10DFCIndices,
    { NULL, _gActor05700Actor105700Animation10DFCBank1, NULL, NULL, _gActor05700Actor105700Animation10DFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation1180CBank1[19] = {
#include "assets/actor_105700_animation_1180C_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation1180CBank4[255] = {
#include "assets/actor_105700_animation_1180C_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation1180CRecords[312] = {
#include "assets/actor_105700_animation_1180C_records.inc"
};

static u16 _gActor05700Actor105700Animation1180CIndices[20] = {
#include "assets/actor_105700_animation_1180C_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation1180C = {
    _gActor05700Actor105700Animation1180CRecords,
    _gActor05700Actor105700Animation1180CIndices,
    { NULL, _gActor05700Actor105700Animation1180CBank1, NULL, NULL, _gActor05700Actor105700Animation1180CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation122B4Bank1[18] = {
#include "assets/actor_105700_animation_122B4_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation122B4Bank4[281] = {
#include "assets/actor_105700_animation_122B4_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation122B4Records[327] = {
#include "assets/actor_105700_animation_122B4_records.inc"
};

static u16 _gActor05700Actor105700Animation122B4Indices[20] = {
#include "assets/actor_105700_animation_122B4_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation122B4 = {
    _gActor05700Actor105700Animation122B4Records,
    _gActor05700Actor105700Animation122B4Indices,
    { NULL, _gActor05700Actor105700Animation122B4Bank1, NULL, NULL, _gActor05700Actor105700Animation122B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation12894Bank1[8] = {
#include "assets/actor_105700_animation_12894_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation12894Bank4[130] = {
#include "assets/actor_105700_animation_12894_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation12894Records[202] = {
#include "assets/actor_105700_animation_12894_records.inc"
};

static u16 _gActor05700Actor105700Animation12894Indices[20] = {
#include "assets/actor_105700_animation_12894_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation12894 = {
    _gActor05700Actor105700Animation12894Records,
    _gActor05700Actor105700Animation12894Indices,
    { NULL, _gActor05700Actor105700Animation12894Bank1, NULL, NULL, _gActor05700Actor105700Animation12894Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation13588Bank1[23] = {
#include "assets/actor_105700_animation_13588_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation13588Bank4[326] = {
#include "assets/actor_105700_animation_13588_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation13588Records[414] = {
#include "assets/actor_105700_animation_13588_records.inc"
};

static u16 _gActor05700Actor105700Animation13588Indices[20] = {
#include "assets/actor_105700_animation_13588_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation13588 = {
    _gActor05700Actor105700Animation13588Records,
    _gActor05700Actor105700Animation13588Indices,
    { NULL, _gActor05700Actor105700Animation13588Bank1, NULL, NULL, _gActor05700Actor105700Animation13588Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation14600Bank1[31] = {
#include "assets/actor_105700_animation_14600_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation14600Bank4[429] = {
#include "assets/actor_105700_animation_14600_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation14600Records[512] = {
#include "assets/actor_105700_animation_14600_records.inc"
};

static u16 _gActor05700Actor105700Animation14600Indices[20] = {
#include "assets/actor_105700_animation_14600_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation14600 = {
    _gActor05700Actor105700Animation14600Records,
    _gActor05700Actor105700Animation14600Indices,
    { NULL, _gActor05700Actor105700Animation14600Bank1, NULL, NULL, _gActor05700Actor105700Animation14600Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation14AD4Bank1[9] = {
#include "assets/actor_105700_animation_14AD4_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation14AD4Bank4[113] = {
#include "assets/actor_105700_animation_14AD4_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation14AD4Records[149] = {
#include "assets/actor_105700_animation_14AD4_records.inc"
};

static u16 _gActor05700Actor105700Animation14AD4Indices[20] = {
#include "assets/actor_105700_animation_14AD4_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation14AD4 = {
    _gActor05700Actor105700Animation14AD4Records,
    _gActor05700Actor105700Animation14AD4Indices,
    { NULL, _gActor05700Actor105700Animation14AD4Bank1, NULL, NULL, _gActor05700Actor105700Animation14AD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation14DE4Bank1[5] = {
#include "assets/actor_105700_animation_14DE4_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation14DE4Bank4[65] = {
#include "assets/actor_105700_animation_14DE4_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation14DE4Records[96] = {
#include "assets/actor_105700_animation_14DE4_records.inc"
};

static u16 _gActor05700Actor105700Animation14DE4Indices[20] = {
#include "assets/actor_105700_animation_14DE4_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation14DE4 = {
    _gActor05700Actor105700Animation14DE4Records,
    _gActor05700Actor105700Animation14DE4Indices,
    { NULL, _gActor05700Actor105700Animation14DE4Bank1, NULL, NULL, _gActor05700Actor105700Animation14DE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation14FC0Bank1[2] = {
#include "assets/actor_105700_animation_14FC0_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation14FC0Bank4[17] = {
#include "assets/actor_105700_animation_14FC0_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation14FC0Records[76] = {
#include "assets/actor_105700_animation_14FC0_records.inc"
};

static u16 _gActor05700Actor105700Animation14FC0Indices[20] = {
#include "assets/actor_105700_animation_14FC0_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation14FC0 = {
    _gActor05700Actor105700Animation14FC0Records,
    _gActor05700Actor105700Animation14FC0Indices,
    { NULL, _gActor05700Actor105700Animation14FC0Bank1, NULL, NULL, _gActor05700Actor105700Animation14FC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation15F08Bank1[28] = {
#include "assets/actor_105700_animation_15F08_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation15F08Bank4[394] = {
#include "assets/actor_105700_animation_15F08_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation15F08Records[480] = {
#include "assets/actor_105700_animation_15F08_records.inc"
};

static u16 _gActor05700Actor105700Animation15F08Indices[20] = {
#include "assets/actor_105700_animation_15F08_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation15F08 = {
    _gActor05700Actor105700Animation15F08Records,
    _gActor05700Actor105700Animation15F08Indices,
    { NULL, _gActor05700Actor105700Animation15F08Bank1, NULL, NULL, _gActor05700Actor105700Animation15F08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation1649CBank1[9] = {
#include "assets/actor_105700_animation_1649C_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation1649CBank4[127] = {
#include "assets/actor_105700_animation_1649C_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation1649CRecords[183] = {
#include "assets/actor_105700_animation_1649C_records.inc"
};

static u16 _gActor05700Actor105700Animation1649CIndices[20] = {
#include "assets/actor_105700_animation_1649C_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation1649C = {
    _gActor05700Actor105700Animation1649CRecords,
    _gActor05700Actor105700Animation1649CIndices,
    { NULL, _gActor05700Actor105700Animation1649CBank1, NULL, NULL, _gActor05700Actor105700Animation1649CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation16764Bank1[5] = {
#include "assets/actor_105700_animation_16764_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation16764Bank4[56] = {
#include "assets/actor_105700_animation_16764_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation16764Records[87] = {
#include "assets/actor_105700_animation_16764_records.inc"
};

static u16 _gActor05700Actor105700Animation16764Indices[20] = {
#include "assets/actor_105700_animation_16764_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation16764 = {
    _gActor05700Actor105700Animation16764Records,
    _gActor05700Actor105700Animation16764Indices,
    { NULL, _gActor05700Actor105700Animation16764Bank1, NULL, NULL, _gActor05700Actor105700Animation16764Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation16940Bank1[2] = {
#include "assets/actor_105700_animation_16940_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation16940Bank4[17] = {
#include "assets/actor_105700_animation_16940_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation16940Records[76] = {
#include "assets/actor_105700_animation_16940_records.inc"
};

static u16 _gActor05700Actor105700Animation16940Indices[20] = {
#include "assets/actor_105700_animation_16940_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation16940 = {
    _gActor05700Actor105700Animation16940Records,
    _gActor05700Actor105700Animation16940Indices,
    { NULL, _gActor05700Actor105700Animation16940Bank1, NULL, NULL, _gActor05700Actor105700Animation16940Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor05700Actor105700Animation170CCBank1[15] = {
#include "assets/actor_105700_animation_170CC_bank1.inc"
};

static AnimationPackedRotation _gActor05700Actor105700Animation170CCBank4[171] = {
#include "assets/actor_105700_animation_170CC_bank4.inc"
};

static AnimationRecord _gActor05700Actor105700Animation170CCRecords[247] = {
#include "assets/actor_105700_animation_170CC_records.inc"
};

static u16 _gActor05700Actor105700Animation170CCIndices[20] = {
#include "assets/actor_105700_animation_170CC_indices.inc"
};

static AnimationSet _gActor05700Actor105700Animation170CC = {
    _gActor05700Actor105700Animation170CCRecords,
    _gActor05700Actor105700Animation170CCIndices,
    { NULL, _gActor05700Actor105700Animation170CCBank1, NULL, NULL, _gActor05700Actor105700Animation170CCBank4, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 30, 5 },
    { 20, 5 },
    { 0, 8 },
    { 20, 2 },
    { 30, 7 },
};

EnemyParams gGolemPawnRookParams[1] = {
    { gGolemPawnRookAttacks, 482, 250, 400, 8, 0, 6, 0, 0 },
};

s16 gGolemPawnRookWeakPointWeapons[46] = {
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
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
    1,
    1,
    1,
    1,
    1,
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
    1,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
};

s32 gGolemPawnRookVoiceCues[17] = {
    0,
    0x40390001,
    0x40390002,
    0x40390003,
    0x40390004,
    0x40390005,
    0x40390006,
    0x4039000C,
    0x4039000D,
    0x40390009,
    0x4039000A,
    0x4039000B,
    0x4039000E,
    0x4039000F,
    0x40390010,
    0x40390011,
    0x40390012,
};

s32 gGolemPawnRookShotSound = 0x40390007;

/// Sound id of the cue a grenade plays where its flight ends; the bullet ORs
/// the firing enemy's place index into bits 8 and up before queueing it.
s32 gGolemPawnRookImpactSound = 0x40390008;

s32 gGolemPawnRookScreamCue = 0x40390013;

s32 gGolemPawnRookSilenceCue = 0x40390014;

s32 gGolemPawnRookBurstCue = 0x40390015;

u16 Actor05700_D1723C[22] = {
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

u16 Actor05700_D17268[40] = {
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

u16 Actor05700_D172B8[40] = {
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

u16 Actor05700_D17308[50] = {
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

u16 Actor05700_D1736C[34] = {
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
    Actor05700_D1723C,
    Actor05700_D17268,
    Actor05700_D172B8,
    Actor05700_D17308,
    Actor05700_D1736C,
};

s16 gGolemPawnRookBeamRibbonCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc gGolemPawnRookTasks[4] = {
    { { { TASK_BODY_TMD, 96 } }, _actor05700RookGolemTask, { .model = &_gActor05700RookGolemBody } },
    { { { TASK_BODY_TMD, 96 } }, _actor05700GrenadeLauncherTask, { .model = &_gActor05700GolemGrenadeLauncher } },
    { { { TASK_BODY_TMD, 96 } }, _actor05700GrenadeTask, { .model = &_gActor05700GolemGrenade } },
    { { { TASK_BODY_TMD, 96 } }, _actor05700ShieldTask, { .model = &_gActor05700RookGolemShield } },
};

AnimationSet* gGolemPawnRookAnimSets[31] = {
    NULL,
    &_gActor05700Actor105700Animation0BAAC,
    &_gActor05700Actor105700Animation0C414,
    &_gActor05700Actor105700Animation0CA3C,
    &_gActor05700Actor105700Animation0D23C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor05700Actor105700Animation0D444,
    &_gActor05700Actor105700Animation0DA50,
    &_gActor05700Actor105700Animation0DF80,
    &_gActor05700Actor105700Animation0E464,
    &_gActor05700Actor105700Animation0E8C4,
    &_gActor05700Actor105700Animation0F980,
    &_gActor05700Actor105700Animation108D8,
    &_gActor05700Actor105700Animation10DFC,
    &_gActor05700Actor105700Animation1180C,
    &_gActor05700Actor105700Animation122B4,
    &_gActor05700Actor105700Animation12894,
    &_gActor05700Actor105700Animation13588,
    &_gActor05700Actor105700Animation14600,
    &_gActor05700Actor105700Animation14AD4,
    &_gActor05700Actor105700Animation14DE4,
    &_gActor05700Actor105700Animation14FC0,
    &_gActor05700Actor105700Animation15F08,
    &_gActor05700Actor105700Animation1649C,
    &_gActor05700Actor105700Animation16764,
    &_gActor05700Actor105700Animation16940,
    &_gActor05700Actor105700Animation170CC,
};

TaskFunc gGolemPawnRookStates[15] = {
    _golemPawnRookIdleState,
    _golemPawnRookPatrolState,
    golemPawnRookLungeCycle,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookSilenceScreamState,
    golemPawnRookCompanionCycle,
    golemPawnRookLungeStrikeState,
    _golemPawnRookStaggerState,
    golemPawnRookRecoilState,
    _golemPawnRookBuildupState,
    golemPawnRookKnockdownState,
    _golemPawnRookDownedHitState,
    _golemPawnRookCollapseState,
    _golemPawnRookDownedDeathState,
};

extern s16 gGolemPawnRookBeamRibbonCorners[][4];

extern s32 gGolemPawnRookShotSound;

extern s32 gGolemPawnRookScreamCue;

extern s32 gGolemPawnRookSilenceCue;

/// Animation bank the work block's animation context is started on.
extern AnimationSet* gGolemPawnRookAnimSets[31];

/// The actor's spawn table: entry 3 is the model child re-skinned with the
/// placement's texture page, entry 1 the effect child, and the whole table
/// is kept in `taskTable` for later spawns.
extern TaskDesc gGolemPawnRookTasks[];

/// Per-stage tables of per-area CD cue ids; a NULL stage has no cue.
extern u16* gGolemPawnRookAreaParams[];

/// Enemy parameter record the spawn hands to its `Enemy`.
extern EnemyParams gGolemPawnRookParams[];

/// Per-state handlers of the approach cycle, indexed by `behavior`.
extern TaskFunc gGolemPawnRookStates[];

/// Sound id the spawn cue is played against; the low byte comes from the
/// context block's room/channel bits.
extern s32 gGolemPawnRookBurstCue;

#include "../../shared/golem_pawn_rook_hit_tick.inc.c"

#include "../../shared/golem_pawn_rook_patrol.inc.c"

#include "../../shared/golem_pawn_rook_player_noise.inc.c"

#include "../../shared/golem_pawn_rook_knockdown.inc.c"

#include "../../shared/golem_pawn_rook_downed_hit.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

#include "../../shared/golem_pawn_rook_companion_cycle.inc.c"

/// State handlers of the model child hung off the actor's part 7 - spawn,
/// per-frame tick and teardown - dispatched through by `_actor05700GrenadeLauncherTask`.
static const EnemyTaskFuncTable3 Actor05700_D00080 = {
    _golemPawnRookLauncherSpawn,
    _golemPawnRookLauncherTick,
    enemyDestroy,
};

#include "../../shared/golem_pawn_rook_lunge_strike.inc.c"

#include "../../shared/golem_pawn_rook_laser_sight.inc.c"

#include "../../shared/golem_pawn_rook_laser_beam.inc.c"

#include "../../shared/golem_pawn_rook_grenade_spawn.inc.c"

#include "../../shared/golem_pawn_rook_bullet_fly.inc.c"

#include "../../shared/golem_pawn_rook_silence_scream.inc.c"

/// `_actorRenderApplyPlacementTextureOffsets` for a spawned enemy's model.
#include "../../shared/golem_pawn_rook_spawn.inc.c"

#include "../../shared/golem_pawn_rook_inlines.inc.c"

#include "../../shared/golem_pawn_rook_frame.inc.c"

#include "../../shared/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_stagger.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_buildup.inc.c"

#include "../../shared/golem_pawn_rook_downed_death.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

/// Dispatches the Rook's attached grenade-launcher task.
///
/// `launcher` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to attach to body part 7, 1 to mirror the body's
/// visibility and consume a fire request, or 2 to destroy the child. The body
/// must remain alive while its child borrows the body's work and lighting.
/// Destruction may release the task and enemy before returning.
static void _actor05700GrenadeLauncherTask(Task* launcher)
{
    EnemyTaskFuncTable3 lifecycleHandlers = Actor05700_D00080;

    lifecycleHandlers.funcs[launcher->state](launcher->spawnArg2.pointer, launcher);
}

#include "../../shared/golem_pawn_rook_launcher_spawn.inc.c"

#include "../../shared/golem_pawn_rook_launcher_tick.inc.c"

/// State handlers of the effect child - spawn/setup, per-frame tick and
/// teardown - dispatched through by `_actor05700GrenadeTask`.
static const EnemyTaskFuncTable3 Actor05700_D0008C = {
    _golemPawnRookGrenadeSpawn,
    golemPawnRookBulletFly,
    _golemPawnRookGrenadeDestroy,
};

/// Dispatches a grenade fired by the Rook's launcher.
///
/// `grenade` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to launch and detach from the launcher, 1 to fly
/// until impact or timeout, or 2 to unlink collision and wait 61 frames before
/// destruction. State 0 requires a live launcher parent; later states use the
/// grenade's own work and lighting. Destruction may release the task and enemy.
static void _actor05700GrenadeTask(Task* grenade)
{
    EnemyTaskFuncTable3 lifecycleHandlers = Actor05700_D0008C;

    lifecycleHandlers.funcs[grenade->state](grenade->spawnArg2.pointer, grenade);
}

#include "../../shared/golem_pawn_rook_grenade_destroy.inc.c"

/// State handlers of the burst child parented to the actor's part 11 - spawn,
/// per-frame tick and teardown - dispatched through by `_actor05700ShieldTask`.
static const EnemyTaskFuncTable3 Actor05700_D00098 = {
    _golemPawnRookShieldSpawn,
    golemPawnRookBurstPartTick,
    enemyDestroy,
};

/// Dispatches the Rook's attached shield task through its break and destruction.
///
/// `shield` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to attach to body part 11, 1 to mirror the body's
/// visibility and service its shield-break request, or 2 to destroy the child.
/// The body must remain alive while the shield borrows its work and lighting.
/// Destruction may release the task and enemy before returning.
static void _actor05700ShieldTask(Task* shield)
{
    EnemyTaskFuncTable3 lifecycleHandlers = Actor05700_D00098;

    lifecycleHandlers.funcs[shield->state](shield->spawnArg2.pointer, shield);
}

#include "../../shared/golem_pawn_rook_shield_spawn.inc.c"

/// The actor's own state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by `_actor05700RookGolemTask`. The tick and
/// teardown take the task as the actor view it is.
static const EnemyTaskFuncTable3 Actor05700_D000A4 = {
    golemPawnRookSpawn,
    golemPawnRookFrameState,
    golemPawnRookDeadState,
};

#include "../../shared/golem_pawn_rook_burst_part.inc.c"

/// Dispatches the grenade-launcher Rook GOLEM's body task.
///
/// `golem` is a live nineteen-part TMD task with its owned `Enemy` in
/// `spawnArg2.pointer`. Its unchecked `state` is 0 to initialize the body and
/// attached launcher and shield, 1 to run combat behaviour, or 2 to retain and
/// draw its dead body after ending collision and target tracking. Spawn failure
/// may release the task and enemy before returning.
static void _actor05700RookGolemTask(Task* golem)
{
    EnemyTaskFuncTable3 lifecycleHandlers = Actor05700_D000A4;

    lifecycleHandlers.funcs[golem->state](golem->spawnArg2.pointer, golem);
}
