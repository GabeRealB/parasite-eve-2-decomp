#include "actors/task_tables.h"

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
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/actor/player_detection.h"
/// Selects shielded Rook behavior for this Grenade Launcher GOLEM carrier.
#define GOLEM_PAWN_ROOK_KIND GOLEM_PAWN_ROOK_KIND_ROOK
/// Selects the shared Grenade Launcher build for this shielded Rook GOLEM carrier.
///
/// Keep this binding through all GOLEM fragments, with matching weapon tasks,
/// behavior callbacks and animation tables.
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_PAWN_ROOK_WEAPON_GRENADE_LAUNCHER
#include "../../shared/creatures/golem/golem_pawn_rook.h"

/// Character script bank used by this carrier's cue tables.
enum { GOLEM_PAWN_ROOK_SOUND_BANK = 0x39 };

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

/// Blend-in durations indexed by animation request, in whole frames.
///
/// The 32 signed halfwords belong to this carrier. Launcher bursts change
/// entry 13 to 0 or 3 for successive shots; all bodies in the carrier share it.
static s16 _gGolemPawnRookAnimBlendFrames[32] = {
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

/// Five category-4 attack rows, storing base HP damage and player reaction.
///
/// Slots 0/1 are sword swing/charge, 2 is the zero-power Silence row,
/// 3 is the grenade and 4 is the launcher strike. Unused weapon rows remain
/// present so every carrier uses the same attack indices.
static DamageAttack _gGolemPawnRookAttacks[5] = {
    { 30, 5 },
    { 20, 5 },
    { 0, GAME_ACTOR_REACTION_SILENCE },
    { 20, GAME_ACTOR_REACTION_PARALYSIS },
    { 30, 7 },
};

/// Rook GOLEM launcher combat, reaction and release-reward parameters.
///
/// One shared record per loaded carrier, borrowed by newly linked live enemies.
/// Saved corpses bypass combat initialization. Its five attack rows remain live
/// with this record; grenades also pack damage directly from that attack table.
static EnemyParams _gGolemPawnRookParams = {
    .attacks              = _gGolemPawnRookAttacks,
    .hpMax                = 482,
    .exp                  = 250,
    .bp                   = 400,
    .mp                   = 8,
    .critChance           = 0,
    .buildupSteps         = 6,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Raised-shield absorption flags for weapon attack rows.
///
/// Signed halfword values are 0 bypass and 1 absorb. The low seven attack-key
/// bits must select a row 0..45; bit 15 is clear for this family.
/// A raised shield covers frontal hits while standing; absorption spends shield HP
/// instead of body HP. Pawn carriers retain the same table without a shield child.
static s16 _gGolemPawnRookShieldWeaponRows[46] = {
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
};

/// Raised-shield absorption flags for Parasite Energy attack rows.
///
/// Signed halfword values are 0 bypass and 1 absorb. The low seven attack-key
/// bits must select a row 0..55; bit 15 selects this PE family.
/// A raised shield covers frontal hits while standing; absorption spends shield HP
/// instead of body HP. Pawn carriers retain the same table without a shield child.
static s16 _gGolemPawnRookShieldPeRows[56] = {
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
};

/// Character-bank script IDs for animation cues and falls, without instance tags.
///
/// Sound sets 1..4 select cue pairs at 2*set-1/2*set, fall impacts at 8+set,
/// and the start of a backward fall at 12+set. Set 0 skips animation pairs
/// but falls still use entries 8/12; entry 0 is a no-op. Callers insert the
/// placement instance into bits 8..15 and supply spatial pan/depth.
static s32 _gGolemPawnRookMotionCues[17] = {
    SOUND_SCRIPT_REQUEST_NO_OP,
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 1),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 2),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 3),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 4),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 5),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 6),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 12),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 13),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 9),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 10),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 11),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 14),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 15),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 16),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 17),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 18),
};

/// Character-bank script ID played when a grenade detaches from its launcher.
///
/// The stored instance byte is zero; the grenade inserts its placement index
/// in bits 8..15 before requesting the spatialized sound.
static s32 _gGolemPawnRookGrenadeLaunchCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 7);

/// Sound id of the cue a grenade plays where its flight ends; the bullet ORs
/// the firing enemy's place index into bits 8 and up before queueing it.
s32 gGolemPawnRookImpactSound = 0x40390008;

/// Character-bank script ID for the start of the Rook's Silence charge.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookScreamCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 19);

/// Character-bank script ID played when the Rook applies Silence.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookSilenceCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 20);

/// Character-bank script ID for the attached Rook shield breaking.
///
/// The instance byte is zero; the caller inserts the shield placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookShieldBreakCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 21);

/// Stage-1 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..20 index directly; the final zero halfword is unused.
/// Zero disables bank loading and motion cues; 1..4 select a sound file and
/// its footstep/fall cue set. All 22 stored halfwords are retained.
static u16 _gActor05700Stage1AreaSoundSets[22] = {
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

/// Stage-2 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..38 index directly; the final zero halfword is unused.
/// Zero disables loading and motion cues; 1..4 select the sound file and
/// footstep/fall cue set. All 40 stored halfwords are retained.
static u16 _gActor05700Stage2AreaSoundSets[40] = {
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

/// Stage-3 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..38 index directly; the final zero halfword is unused.
/// Zero disables loading and motion cues; 1..4 select the sound file and
/// footstep/fall cue set. All 40 stored halfwords are retained.
static u16 _gActor05700Stage3AreaSoundSets[40] = {
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

/// Stage-4 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..49 index the 50 halfwords directly; 0 disables loading and motion
/// cues, and 1..4 select the sound file and footstep/fall cue set.
static u16 _gActor05700Stage4AreaSoundSets[50] = {
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

/// Stage-5 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..33 index the 34 halfwords directly; 0 disables loading and motion
/// cues, and 1..4 select the sound file and footstep/fall cue set.
static u16 _gActor05700Stage5AreaSoundSets[34] = {
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

/// Launcher GOLEM sound-set selectors indexed first by stage, then by area.
///
/// Stage 0 is NULL and leaves the zero-filled work's soundSet at 0 (no bank or
/// animation cues). Stages 1..5 borrow area-indexed halfwords containing 0..4;
/// nonzero values select the sound file and its footstep/fall cue variant.
/// Stages 1..5 have 22, 40, 40, 50 and 34 area entries, respectively; the spawn
/// has no bounds check. Rows remain borrowed from this carrier until the body
/// and its cue playback end.
static u16* _gGolemPawnRookStageAreaSoundSets[6] = {
    NULL,
    _gActor05700Stage1AreaSoundSets,
    _gActor05700Stage2AreaSoundSets,
    _gActor05700Stage3AreaSoundSets,
    _gActor05700Stage4AreaSoundSets,
    _gActor05700Stage5AreaSoundSets,
};

/// Projected vertex indices of the laser sight's two fading ribbon quads.
///
/// Each row gives GPU corners 0..3. Indices 0/1 are centre-line endpoints;
/// 2/3 and 4/5 are their two edges. Every index addresses a six-element X/Y
/// scratch array; the signed halfword representation and two rows are retained.
static s16 _gGolemPawnRookBeamRibbonCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

enum { ACTOR_05700_TASK_PRIORITY = 96 };

TaskDesc gActor05700RookGolemTasks[4] = {
    { { { TASK_BODY_TMD, ACTOR_05700_TASK_PRIORITY } }, _actor05700RookGolemTask, { .model = &_gActor05700RookGolemBody } },
    { { { TASK_BODY_TMD, ACTOR_05700_TASK_PRIORITY } }, _actor05700GrenadeLauncherTask, { .model = &_gActor05700GolemGrenadeLauncher } },
    { { { TASK_BODY_TMD, ACTOR_05700_TASK_PRIORITY } }, _actor05700GrenadeTask, { .model = &_gActor05700GolemGrenade } },
    { { { TASK_BODY_TMD, ACTOR_05700_TASK_PRIORITY } }, _actor05700ShieldTask, { .model = &_gActor05700RookGolemShield } },
};

/// Launcher GOLEM body clip bank indexed by animation selectors 0..30.
///
/// Slot 0 and unavailable weapon/kind clips are NULL. The nineteen-part body
/// rig borrows the bank while loaded: idle/walk/turn/listen occupy 1..4,
/// launcher attack clips 12..16/30, hit reactions 17..20, and downed poses 22..29.
/// The Rook plays scream clips 10/11 and recovery clip 21; the Pawn retains
/// clip 21 but has no scream behavior.
static AnimationSet* _gGolemPawnRookAnimSets[31] = {
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

/// Combat behavior callbacks indexed by `GOLEM_PAWN_ROOK_BEHAVIOR_*`.
///
/// Indices 0..14 require live body work; unsupported weapon/kind behaviors
/// use the no-op handler. These are separate from the body task lifecycle.
static TaskFunc _gGolemPawnRookBehaviorHandlers[GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_DEATH + 1] = {
    _golemPawnRookIdleState,
    _golemPawnRookPatrolState,
    _golemPawnRookEngageState,
    _golemPawnRookNopState,
    _golemPawnRookNopState,
    _golemPawnRookSilenceScreamState,
    _golemPawnRookGrenadeBurstState,
    _golemPawnRookLauncherStrikeState,
    _golemPawnRookStaggerState,
    _golemPawnRookRecoilState,
    _golemPawnRookBuildupState,
    _golemPawnRookKnockdownState,
    _golemPawnRookDownedHitState,
    _golemPawnRookCollapseState,
    _golemPawnRookDownedDeathState,
};

/// Animation bank the work block's animation context is started on.

/// Enemy parameter record the spawn hands to its `Enemy`.

#include "../../shared/creatures/golem/golem_pawn_rook_take_hits.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_patrol.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_grenade_burst.inc.c"

/// Rook GOLEM launcher lifecycle: 0 attach, 1 mirror flags/fire, 2 destroy.
///
/// The launcher borrows the live body's work, lighting and part-7 coordinate.
/// Dispatch copies all three handlers before selecting an unchecked state.
/// Destruction releases the launcher's Enemy and begins task teardown.
static const EnemyTaskFuncTable3 _gActor05700GrenadeLauncherStates = {
    _golemPawnRookLauncherSpawn,
    _golemPawnRookLauncherTick,
    enemyDestroy,
};

#include "../../shared/creatures/golem/golem_pawn_rook_lunge_strike.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_silence_scream.inc.c"

/// Selects the Rook GOLEM's launcher spawn table for the shared spawn code.
///
/// Binds a writable `TaskDesc[4]`: 0 body, 1 launcher, 2 grenade, 3 shield.
/// The body borrows the array for later grenade spawns; keep the array,
/// callbacks and geometry loaded until all spawned tasks, including detached
/// grenades, have been released. Only the following fragment uses this binding.
#define GOLEM_PAWN_ROOK_TASK_DESCRIPTORS gActor05700RookGolemTasks
#include "../../shared/creatures/golem/golem_pawn_rook_spawn.inc.c"
#undef GOLEM_PAWN_ROOK_TASK_DESCRIPTORS

#include "../../shared/creatures/golem/golem_pawn_rook_inlines.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_frame.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/actor/player_detection_segment.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook.inc.c"

/// Dispatches the Rook's attached grenade-launcher task.
///
/// `launcher` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to attach to body part 7, 1 to mirror the body's
/// visibility and consume a fire request, or 2 to destroy the child. The body
/// must remain alive while its child borrows the body's work and lighting.
/// Destruction may release the task and enemy before returning.
static void _actor05700GrenadeLauncherTask(Task* launcher)
{
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700GrenadeLauncherStates;

    lifecycleHandlers.funcs[launcher->state](launcher->spawnArg2.pointer, launcher);
}

#include "../../shared/creatures/golem/golem_pawn_rook_launcher.inc.c"

/// Grenade lifecycle states: 0 launch/detach, 1 flight, 2 delayed destruction.
///
/// The dispatcher copies all three handlers before indexing by task state.
/// State 2 unlinks collision, then keeps the grenade alive for 61 update ticks.
static const EnemyTaskFuncTable3 _gActor05700GrenadeStates = {
    _golemPawnRookGrenadeSpawn,
    _golemPawnRookGrenadeFly,
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
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700GrenadeStates;

    lifecycleHandlers.funcs[grenade->state](grenade->spawnArg2.pointer, grenade);
}

#include "../../shared/creatures/golem/golem_pawn_rook_grenade_destroy.inc.c"

/// Shield lifecycle states: 0 attach, 1 follow/break, 2 destroy.
///
/// The shield attaches to body part 11 and borrows the live body's work and
/// lighting. The dispatcher copies all three handlers before indexing by task state.
static const EnemyTaskFuncTable3 _gActor05700ShieldStates = {
    _golemPawnRookShieldSpawn,
    _golemPawnRookShieldTick,
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
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700ShieldStates;

    lifecycleHandlers.funcs[shield->state](shield->spawnArg2.pointer, shield);
}

#include "../../shared/creatures/golem/golem_pawn_rook_shield_spawn.inc.c"

/// Launcher Rook body lifecycle states: 0 initialize, 1 combat, 2 persistent corpse.
///
/// The dispatcher copies all three handlers before indexing by task state.
/// The dead state ends combat participation while retaining the body and pose.
static const EnemyTaskFuncTable3 _gActor05700RookGolemStates = {
    _golemPawnRookSpawn,
    _golemPawnRookFrameState,
    _golemPawnRookDeadState,
};

#include "../../shared/creatures/golem/golem_pawn_rook_shield_tick.inc.c"

/// Dispatches the grenade-launcher Rook GOLEM's body task.
///
/// `golem` is a live nineteen-part TMD task with its owned `Enemy` in
/// `spawnArg2.pointer`. Its unchecked `state` is 0 to initialize the body and
/// attached launcher and shield, 1 to run combat behaviour, or 2 to retain and
/// draw its dead body after ending collision and target tracking. Spawn failure
/// may release the task and enemy before returning.
static void _actor05700RookGolemTask(Task* golem)
{
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700RookGolemStates;

    lifecycleHandlers.funcs[golem->state](golem->spawnArg2.pointer, golem);
}
