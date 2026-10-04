#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
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
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
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
#include "../../shared/player_detection.h"
#define GOLEM_PAWN_ROOK_TYPE   GOLEM_ROOK
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_BEAM_SWORD
#include "../../shared/golem_pawn_rook.h"

/// First frame of each animation, indexed by `GolemPawnRookWork::anim`;
/// the state handlers offset it to get the frames their cues fire on.
extern s16 gGolemPawnRookAnimBlendFrames[];
/// The `Gp_PackPair` entry the lunge parks in the work block's `strikeBody`.
extern DamageAttack gGolemPawnRookAttacks[5];
/// Sound id of the cue a sword strike plays as its hit body goes live; the
/// swing and charge states OR the enemy's place index into bits 8 and up
/// before queueing it.
extern s32 gGolemPawnRookSwingCue;

/// The `EnemyParams` the enemy parks in its own `field_50` slot.
extern EnemyParams Actor02300_D159D8;
/// Per-room voice-stream sector tables, indexed by `GameSession::location.loc.stage` then
/// `field_6`; a NULL row means this room has no cue.
extern u16* Actor02300_D15C80[];
/// The overlay's own spawn table: entry 0 is this enemy, 1 and 2 the two
/// companions the setup state spawns.
extern TaskDesc Actor02300_D15C98[];
/// Animation bank `animationInitContext` binds to the work block.
extern AnimationSet* Actor02300_D15CBC[31];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 gGolemPawnRookWeakSpotHits[];
extern s16 gGolemPawnRookWeakSpotHitsFlagged[];

/// Voice-cue sound ids, indexed from `GolemPawnRookWork::soundSet`.
extern s32 gGolemPawnRookVoiceCues[];
/// Sound ids of the burst state's two cues.
extern s32 gGolemPawnRookScreamCue;
extern s32 gGolemPawnRookSilenceCue;

/// Handlers of the `GolemPawnRookWork::behavior` states, one per entry.
extern TaskFunc gGolemPawnRookStates[];

/// Sound id of the part-11 child's cue, ORed with the enemy's id nibble.
extern s32 gGolemPawnRookBurstCue;

static AnimationSet _gActor02300Actor102300Animation0A2F0;
static AnimationSet _gActor02300Actor102300Animation0AC58;
static AnimationSet _gActor02300Actor102300Animation0B280;
static AnimationSet _gActor02300Actor102300Animation0BA80;
static AnimationSet _gActor02300Actor102300Animation0C86C;
static AnimationSet _gActor02300Actor102300Animation0CDE0;
static AnimationSet _gActor02300Actor102300Animation0DCB0;
static AnimationSet _gActor02300Actor102300Animation0E9E8;
static AnimationSet _gActor02300Actor102300Animation0F120;
static AnimationSet _gActor02300Actor102300Animation0F328;
static AnimationSet _gActor02300Actor102300Animation0F934;
static AnimationSet _gActor02300Actor102300Animation0FE58;
static AnimationSet _gActor02300Actor102300Animation10868;
static AnimationSet _gActor02300Actor102300Animation11310;
static AnimationSet _gActor02300Actor102300Animation118F0;
static AnimationSet _gActor02300Actor102300Animation125E4;
static AnimationSet _gActor02300Actor102300Animation1365C;
static AnimationSet _gActor02300Actor102300Animation13B30;
static AnimationSet _gActor02300Actor102300Animation13E40;
static AnimationSet _gActor02300Actor102300Animation1401C;
static AnimationSet _gActor02300Actor102300Animation14F64;
static AnimationSet _gActor02300Actor102300Animation154F8;
static AnimationSet _gActor02300Actor102300Animation157C0;
static AnimationSet _gActor02300Actor102300Animation1599C;
static TmdSource    _gActor02300RookGolemBody;
static TmdSource    _gActor02300GolemBeamSword;
static TmdSource    _gActor02300RookGolemShield;
void                Actor02300_Fn03BA8(Task*);
void                Actor02300_Fn03CE8(Task*);
void                Actor02300_Fn03EE8(Task*);

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

static TmdBone _gActor02300RookGolemBodySkeleton[19] = {
#include "assets/rook_golem_body_skeleton.inc"
};

static u32 _gActor02300RookGolemBodyPartVerts[19] = {
#include "assets/rook_golem_body_partVerts.inc"
};

static SVECTOR _gActor02300RookGolemBodyVerts[328] = {
#include "assets/rook_golem_body_verts.inc"
};

static SVECTOR _gActor02300RookGolemBodyNormals[344] = {
#include "assets/rook_golem_body_normals.inc"
};

static u32 _gActor02300RookGolemBodyStream[3509] = {
#include "assets/rook_golem_body_stream.inc"
};

static TmdSource _gActor02300RookGolemBody = {
    0,
    19580,
    4888,
    19,
    _gActor02300RookGolemBodyPartVerts,
    _gActor02300RookGolemBodyVerts,
    _gActor02300RookGolemBodyNormals,
    _gActor02300RookGolemBodySkeleton,
    _gActor02300RookGolemBodyStream,
};

static TmdBone _gActor02300GolemBeamSwordSkeleton[1] = {
#include "assets/golem_beam_sword_skeleton.inc"
};

static u32 _gActor02300GolemBeamSwordPartVerts[1] = {
#include "assets/golem_beam_sword_partVerts.inc"
};

static SVECTOR _gActor02300GolemBeamSwordVerts[29] = {
#include "assets/golem_beam_sword_verts.inc"
};

static SVECTOR _gActor02300GolemBeamSwordNormals[24] = {
#include "assets/golem_beam_sword_normals.inc"
};

static u32 _gActor02300GolemBeamSwordStream[212] = {
#include "assets/golem_beam_sword_stream.inc"
};

static TmdSource _gActor02300GolemBeamSword = {
    0,
    1436,
    0,
    1,
    _gActor02300GolemBeamSwordPartVerts,
    _gActor02300GolemBeamSwordVerts,
    _gActor02300GolemBeamSwordNormals,
    _gActor02300GolemBeamSwordSkeleton,
    _gActor02300GolemBeamSwordStream,
};

static TmdBone _gActor02300RookGolemShieldSkeleton[1] = {
#include "assets/rook_golem_shield_skeleton.inc"
};

static u32 _gActor02300RookGolemShieldPartVerts[1] = {
#include "assets/rook_golem_shield_partVerts.inc"
};

static SVECTOR _gActor02300RookGolemShieldVerts[16] = {
#include "assets/rook_golem_shield_verts.inc"
};

static SVECTOR _gActor02300RookGolemShieldNormals[20] = {
#include "assets/rook_golem_shield_normals.inc"
};

static u32 _gActor02300RookGolemShieldStream[111] = {
#include "assets/rook_golem_shield_stream.inc"
};

static TmdSource _gActor02300RookGolemShield = {
    0,
    780,
    0,
    1,
    _gActor02300RookGolemShieldPartVerts,
    _gActor02300RookGolemShieldVerts,
    _gActor02300RookGolemShieldNormals,
    _gActor02300RookGolemShieldSkeleton,
    _gActor02300RookGolemShieldStream,
};

static AnimationPackedPose _gActor02300Actor102300Animation0A2F0Bank1[21] = {
#include "assets/actor_102300_animation_0A2F0_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0A2F0Bank4[317] = {
#include "assets/actor_102300_animation_0A2F0_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0A2F0Records[382] = {
#include "assets/actor_102300_animation_0A2F0_records.inc"
};

static u16 _gActor02300Actor102300Animation0A2F0Indices[20] = {
#include "assets/actor_102300_animation_0A2F0_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0A2F0 = {
    _gActor02300Actor102300Animation0A2F0Records,
    _gActor02300Actor102300Animation0A2F0Indices,
    { NULL, _gActor02300Actor102300Animation0A2F0Bank1, NULL, NULL, _gActor02300Actor102300Animation0A2F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0AC58Bank1[16] = {
#include "assets/actor_102300_animation_0AC58_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0AC58Bank4[237] = {
#include "assets/actor_102300_animation_0AC58_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0AC58Records[297] = {
#include "assets/actor_102300_animation_0AC58_records.inc"
};

static u16 _gActor02300Actor102300Animation0AC58Indices[20] = {
#include "assets/actor_102300_animation_0AC58_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0AC58 = {
    _gActor02300Actor102300Animation0AC58Records,
    _gActor02300Actor102300Animation0AC58Indices,
    { NULL, _gActor02300Actor102300Animation0AC58Bank1, NULL, NULL, _gActor02300Actor102300Animation0AC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0B280Bank1[12] = {
#include "assets/actor_102300_animation_0B280_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0B280Bank4[142] = {
#include "assets/actor_102300_animation_0B280_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0B280Records[196] = {
#include "assets/actor_102300_animation_0B280_records.inc"
};

static u16 _gActor02300Actor102300Animation0B280Indices[20] = {
#include "assets/actor_102300_animation_0B280_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0B280 = {
    _gActor02300Actor102300Animation0B280Records,
    _gActor02300Actor102300Animation0B280Indices,
    { NULL, _gActor02300Actor102300Animation0B280Bank1, NULL, NULL, _gActor02300Actor102300Animation0B280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0BA80Bank1[14] = {
#include "assets/actor_102300_animation_0BA80_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0BA80Bank4[186] = {
#include "assets/actor_102300_animation_0BA80_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0BA80Records[264] = {
#include "assets/actor_102300_animation_0BA80_records.inc"
};

static u16 _gActor02300Actor102300Animation0BA80Indices[20] = {
#include "assets/actor_102300_animation_0BA80_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0BA80 = {
    _gActor02300Actor102300Animation0BA80Records,
    _gActor02300Actor102300Animation0BA80Indices,
    { NULL, _gActor02300Actor102300Animation0BA80Bank1, NULL, NULL, _gActor02300Actor102300Animation0BA80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0C86CBank1[25] = {
#include "assets/actor_102300_animation_0C86C_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0C86CBank4[368] = {
#include "assets/actor_102300_animation_0C86C_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0C86CRecords[428] = {
#include "assets/actor_102300_animation_0C86C_records.inc"
};

static u16 _gActor02300Actor102300Animation0C86CIndices[20] = {
#include "assets/actor_102300_animation_0C86C_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0C86C = {
    _gActor02300Actor102300Animation0C86CRecords,
    _gActor02300Actor102300Animation0C86CIndices,
    { NULL, _gActor02300Actor102300Animation0C86CBank1, NULL, NULL, _gActor02300Actor102300Animation0C86CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0CDE0Bank1[10] = {
#include "assets/actor_102300_animation_0CDE0_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0CDE0Bank4[120] = {
#include "assets/actor_102300_animation_0CDE0_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0CDE0Records[179] = {
#include "assets/actor_102300_animation_0CDE0_records.inc"
};

static u16 _gActor02300Actor102300Animation0CDE0Indices[20] = {
#include "assets/actor_102300_animation_0CDE0_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0CDE0 = {
    _gActor02300Actor102300Animation0CDE0Records,
    _gActor02300Actor102300Animation0CDE0Indices,
    { NULL, _gActor02300Actor102300Animation0CDE0Bank1, NULL, NULL, _gActor02300Actor102300Animation0CDE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0DCB0Bank1[27] = {
#include "assets/actor_102300_animation_0DCB0_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0DCB0Bank4[393] = {
#include "assets/actor_102300_animation_0DCB0_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0DCB0Records[454] = {
#include "assets/actor_102300_animation_0DCB0_records.inc"
};

static u16 _gActor02300Actor102300Animation0DCB0Indices[20] = {
#include "assets/actor_102300_animation_0DCB0_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0DCB0 = {
    _gActor02300Actor102300Animation0DCB0Records,
    _gActor02300Actor102300Animation0DCB0Indices,
    { NULL, _gActor02300Actor102300Animation0DCB0Bank1, NULL, NULL, _gActor02300Actor102300Animation0DCB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0E9E8Bank1[21] = {
#include "assets/actor_102300_animation_0E9E8_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0E9E8Bank4[354] = {
#include "assets/actor_102300_animation_0E9E8_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0E9E8Records[409] = {
#include "assets/actor_102300_animation_0E9E8_records.inc"
};

static u16 _gActor02300Actor102300Animation0E9E8Indices[20] = {
#include "assets/actor_102300_animation_0E9E8_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0E9E8 = {
    _gActor02300Actor102300Animation0E9E8Records,
    _gActor02300Actor102300Animation0E9E8Indices,
    { NULL, _gActor02300Actor102300Animation0E9E8Bank1, NULL, NULL, _gActor02300Actor102300Animation0E9E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0F120Bank1[16] = {
#include "assets/actor_102300_animation_0F120_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0F120Bank4[177] = {
#include "assets/actor_102300_animation_0F120_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0F120Records[217] = {
#include "assets/actor_102300_animation_0F120_records.inc"
};

static u16 _gActor02300Actor102300Animation0F120Indices[20] = {
#include "assets/actor_102300_animation_0F120_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0F120 = {
    _gActor02300Actor102300Animation0F120Records,
    _gActor02300Actor102300Animation0F120Indices,
    { NULL, _gActor02300Actor102300Animation0F120Bank1, NULL, NULL, _gActor02300Actor102300Animation0F120Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0F328Bank1[3] = {
#include "assets/actor_102300_animation_0F328_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0F328Bank4[25] = {
#include "assets/actor_102300_animation_0F328_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0F328Records[76] = {
#include "assets/actor_102300_animation_0F328_records.inc"
};

static u16 _gActor02300Actor102300Animation0F328Indices[20] = {
#include "assets/actor_102300_animation_0F328_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0F328 = {
    _gActor02300Actor102300Animation0F328Records,
    _gActor02300Actor102300Animation0F328Indices,
    { NULL, _gActor02300Actor102300Animation0F328Bank1, NULL, NULL, _gActor02300Actor102300Animation0F328Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0F934Bank1[9] = {
#include "assets/actor_102300_animation_0F934_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0F934Bank4[151] = {
#include "assets/actor_102300_animation_0F934_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0F934Records[189] = {
#include "assets/actor_102300_animation_0F934_records.inc"
};

static u16 _gActor02300Actor102300Animation0F934Indices[20] = {
#include "assets/actor_102300_animation_0F934_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0F934 = {
    _gActor02300Actor102300Animation0F934Records,
    _gActor02300Actor102300Animation0F934Indices,
    { NULL, _gActor02300Actor102300Animation0F934Bank1, NULL, NULL, _gActor02300Actor102300Animation0F934Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation0FE58Bank1[8] = {
#include "assets/actor_102300_animation_0FE58_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation0FE58Bank4[116] = {
#include "assets/actor_102300_animation_0FE58_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation0FE58Records[169] = {
#include "assets/actor_102300_animation_0FE58_records.inc"
};

static u16 _gActor02300Actor102300Animation0FE58Indices[20] = {
#include "assets/actor_102300_animation_0FE58_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation0FE58 = {
    _gActor02300Actor102300Animation0FE58Records,
    _gActor02300Actor102300Animation0FE58Indices,
    { NULL, _gActor02300Actor102300Animation0FE58Bank1, NULL, NULL, _gActor02300Actor102300Animation0FE58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation10868Bank1[19] = {
#include "assets/actor_102300_animation_10868_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation10868Bank4[255] = {
#include "assets/actor_102300_animation_10868_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation10868Records[312] = {
#include "assets/actor_102300_animation_10868_records.inc"
};

static u16 _gActor02300Actor102300Animation10868Indices[20] = {
#include "assets/actor_102300_animation_10868_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation10868 = {
    _gActor02300Actor102300Animation10868Records,
    _gActor02300Actor102300Animation10868Indices,
    { NULL, _gActor02300Actor102300Animation10868Bank1, NULL, NULL, _gActor02300Actor102300Animation10868Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation11310Bank1[18] = {
#include "assets/actor_102300_animation_11310_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation11310Bank4[281] = {
#include "assets/actor_102300_animation_11310_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation11310Records[327] = {
#include "assets/actor_102300_animation_11310_records.inc"
};

static u16 _gActor02300Actor102300Animation11310Indices[20] = {
#include "assets/actor_102300_animation_11310_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation11310 = {
    _gActor02300Actor102300Animation11310Records,
    _gActor02300Actor102300Animation11310Indices,
    { NULL, _gActor02300Actor102300Animation11310Bank1, NULL, NULL, _gActor02300Actor102300Animation11310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation118F0Bank1[8] = {
#include "assets/actor_102300_animation_118F0_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation118F0Bank4[130] = {
#include "assets/actor_102300_animation_118F0_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation118F0Records[202] = {
#include "assets/actor_102300_animation_118F0_records.inc"
};

static u16 _gActor02300Actor102300Animation118F0Indices[20] = {
#include "assets/actor_102300_animation_118F0_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation118F0 = {
    _gActor02300Actor102300Animation118F0Records,
    _gActor02300Actor102300Animation118F0Indices,
    { NULL, _gActor02300Actor102300Animation118F0Bank1, NULL, NULL, _gActor02300Actor102300Animation118F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation125E4Bank1[23] = {
#include "assets/actor_102300_animation_125E4_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation125E4Bank4[326] = {
#include "assets/actor_102300_animation_125E4_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation125E4Records[414] = {
#include "assets/actor_102300_animation_125E4_records.inc"
};

static u16 _gActor02300Actor102300Animation125E4Indices[20] = {
#include "assets/actor_102300_animation_125E4_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation125E4 = {
    _gActor02300Actor102300Animation125E4Records,
    _gActor02300Actor102300Animation125E4Indices,
    { NULL, _gActor02300Actor102300Animation125E4Bank1, NULL, NULL, _gActor02300Actor102300Animation125E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation1365CBank1[31] = {
#include "assets/actor_102300_animation_1365C_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation1365CBank4[429] = {
#include "assets/actor_102300_animation_1365C_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation1365CRecords[512] = {
#include "assets/actor_102300_animation_1365C_records.inc"
};

static u16 _gActor02300Actor102300Animation1365CIndices[20] = {
#include "assets/actor_102300_animation_1365C_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation1365C = {
    _gActor02300Actor102300Animation1365CRecords,
    _gActor02300Actor102300Animation1365CIndices,
    { NULL, _gActor02300Actor102300Animation1365CBank1, NULL, NULL, _gActor02300Actor102300Animation1365CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation13B30Bank1[9] = {
#include "assets/actor_102300_animation_13B30_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation13B30Bank4[113] = {
#include "assets/actor_102300_animation_13B30_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation13B30Records[149] = {
#include "assets/actor_102300_animation_13B30_records.inc"
};

static u16 _gActor02300Actor102300Animation13B30Indices[20] = {
#include "assets/actor_102300_animation_13B30_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation13B30 = {
    _gActor02300Actor102300Animation13B30Records,
    _gActor02300Actor102300Animation13B30Indices,
    { NULL, _gActor02300Actor102300Animation13B30Bank1, NULL, NULL, _gActor02300Actor102300Animation13B30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation13E40Bank1[5] = {
#include "assets/actor_102300_animation_13E40_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation13E40Bank4[65] = {
#include "assets/actor_102300_animation_13E40_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation13E40Records[96] = {
#include "assets/actor_102300_animation_13E40_records.inc"
};

static u16 _gActor02300Actor102300Animation13E40Indices[20] = {
#include "assets/actor_102300_animation_13E40_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation13E40 = {
    _gActor02300Actor102300Animation13E40Records,
    _gActor02300Actor102300Animation13E40Indices,
    { NULL, _gActor02300Actor102300Animation13E40Bank1, NULL, NULL, _gActor02300Actor102300Animation13E40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation1401CBank1[2] = {
#include "assets/actor_102300_animation_1401C_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation1401CBank4[17] = {
#include "assets/actor_102300_animation_1401C_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation1401CRecords[76] = {
#include "assets/actor_102300_animation_1401C_records.inc"
};

static u16 _gActor02300Actor102300Animation1401CIndices[20] = {
#include "assets/actor_102300_animation_1401C_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation1401C = {
    _gActor02300Actor102300Animation1401CRecords,
    _gActor02300Actor102300Animation1401CIndices,
    { NULL, _gActor02300Actor102300Animation1401CBank1, NULL, NULL, _gActor02300Actor102300Animation1401CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation14F64Bank1[28] = {
#include "assets/actor_102300_animation_14F64_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation14F64Bank4[394] = {
#include "assets/actor_102300_animation_14F64_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation14F64Records[480] = {
#include "assets/actor_102300_animation_14F64_records.inc"
};

static u16 _gActor02300Actor102300Animation14F64Indices[20] = {
#include "assets/actor_102300_animation_14F64_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation14F64 = {
    _gActor02300Actor102300Animation14F64Records,
    _gActor02300Actor102300Animation14F64Indices,
    { NULL, _gActor02300Actor102300Animation14F64Bank1, NULL, NULL, _gActor02300Actor102300Animation14F64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation154F8Bank1[9] = {
#include "assets/actor_102300_animation_154F8_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation154F8Bank4[127] = {
#include "assets/actor_102300_animation_154F8_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation154F8Records[183] = {
#include "assets/actor_102300_animation_154F8_records.inc"
};

static u16 _gActor02300Actor102300Animation154F8Indices[20] = {
#include "assets/actor_102300_animation_154F8_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation154F8 = {
    _gActor02300Actor102300Animation154F8Records,
    _gActor02300Actor102300Animation154F8Indices,
    { NULL, _gActor02300Actor102300Animation154F8Bank1, NULL, NULL, _gActor02300Actor102300Animation154F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation157C0Bank1[5] = {
#include "assets/actor_102300_animation_157C0_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation157C0Bank4[56] = {
#include "assets/actor_102300_animation_157C0_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation157C0Records[87] = {
#include "assets/actor_102300_animation_157C0_records.inc"
};

static u16 _gActor02300Actor102300Animation157C0Indices[20] = {
#include "assets/actor_102300_animation_157C0_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation157C0 = {
    _gActor02300Actor102300Animation157C0Records,
    _gActor02300Actor102300Animation157C0Indices,
    { NULL, _gActor02300Actor102300Animation157C0Bank1, NULL, NULL, _gActor02300Actor102300Animation157C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor02300Actor102300Animation1599CBank1[2] = {
#include "assets/actor_102300_animation_1599C_bank1.inc"
};

static AnimationPackedRotation _gActor02300Actor102300Animation1599CBank4[17] = {
#include "assets/actor_102300_animation_1599C_bank4.inc"
};

static AnimationRecord _gActor02300Actor102300Animation1599CRecords[76] = {
#include "assets/actor_102300_animation_1599C_records.inc"
};

static u16 _gActor02300Actor102300Animation1599CIndices[20] = {
#include "assets/actor_102300_animation_1599C_indices.inc"
};

static AnimationSet _gActor02300Actor102300Animation1599C = {
    _gActor02300Actor102300Animation1599CRecords,
    _gActor02300Actor102300Animation1599CIndices,
    { NULL, _gActor02300Actor102300Animation1599CBank1, NULL, NULL, _gActor02300Actor102300Animation1599CBank4, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, 8 },
    { 15, 2 },
    { 5, 0 },
};

EnemyParams Actor02300_D159D8 = { gGolemPawnRookAttacks, 482, 250, 400, 8, 0, 6, 0, 0 };

s16 gGolemPawnRookWeakSpotHits[46] = {
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

s16 gGolemPawnRookWeakSpotHitsFlagged[56] = {
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
    0x40170001,
    0x40170002,
    0x40170003,
    0x40170004,
    0x40170005,
    0x40170006,
    0x4017000C,
    0x4017000D,
    0x40170009,
    0x4017000A,
    0x4017000B,
    0x4017000E,
    0x4017000F,
    0x40170010,
    0x40170011,
    0x40170012,
};

s32 gGolemPawnRookSwingCue = 0x40170007;

/* The grenade's impact cue, which the Grenade Launcher builds define at this
 * position. The Beam Sword is built without the grenade, so nothing in this
 * package reads it and it holds no id. */
s32 gGolemPawnRookImpactSound = 0;

s32 gGolemPawnRookScreamCue = 0x40170013;

s32 gGolemPawnRookSilenceCue = 0x40170014;

s32 gGolemPawnRookBurstCue = 0x40170015;

u16 Actor02300_D15B0C[22] = {
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

u16 Actor02300_D15B38[40] = {
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

u16 Actor02300_D15B88[40] = {
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

u16 Actor02300_D15BD8[50] = {
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

u16 Actor02300_D15C3C[34] = {
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

u16* Actor02300_D15C80[6] = {
    NULL,
    Actor02300_D15B0C,
    Actor02300_D15B38,
    Actor02300_D15B88,
    Actor02300_D15BD8,
    Actor02300_D15C3C,
};

TaskDesc Actor02300_D15C98[3] = {
    { { { TASK_BODY_TMD, 96 } }, Actor02300_Fn03EE8, { .model = &_gActor02300RookGolemBody } },
    { { { TASK_BODY_TMD, 96 } }, Actor02300_Fn03BA8, { .model = &_gActor02300GolemBeamSword } },
    { { { TASK_BODY_TMD, 96 } }, Actor02300_Fn03CE8, { .model = &_gActor02300RookGolemShield } },
};

AnimationSet* Actor02300_D15CBC[31] = {
    NULL,
    &_gActor02300Actor102300Animation0A2F0,
    &_gActor02300Actor102300Animation0AC58,
    &_gActor02300Actor102300Animation0B280,
    &_gActor02300Actor102300Animation0BA80,
    &_gActor02300Actor102300Animation0C86C,
    &_gActor02300Actor102300Animation0CDE0,
    &_gActor02300Actor102300Animation0DCB0,
    &_gActor02300Actor102300Animation0F120,
    &_gActor02300Actor102300Animation0E9E8,
    &_gActor02300Actor102300Animation0F328,
    &_gActor02300Actor102300Animation0F934,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor02300Actor102300Animation0FE58,
    &_gActor02300Actor102300Animation10868,
    &_gActor02300Actor102300Animation11310,
    &_gActor02300Actor102300Animation118F0,
    &_gActor02300Actor102300Animation125E4,
    &_gActor02300Actor102300Animation1365C,
    &_gActor02300Actor102300Animation13B30,
    &_gActor02300Actor102300Animation13E40,
    &_gActor02300Actor102300Animation1401C,
    &_gActor02300Actor102300Animation14F64,
    &_gActor02300Actor102300Animation154F8,
    &_gActor02300Actor102300Animation157C0,
    &_gActor02300Actor102300Animation1599C,
    NULL,
};

TaskFunc gGolemPawnRookStates[15] = {
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    golemPawnRookLungeCycle,
    golemPawnRookChargeState,
    golemPawnRookBeamSwingState,
    golemPawnRookSilenceScreamState,
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

static void Actor02300_Fn028AC(Enemy* enemy, Task* actor);

#include "../../shared/golem_pawn_rook_inlines.inc.c"

#include "../../shared/golem_pawn_rook_take_hits.inc.c"

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

#include "../../shared/golem_pawn_rook_silence_scream.inc.c"

/// Spawn/setup state for this enemy. Allocates the `GolemPawnRookWork` block, wires the
/// model object to the block's own light/colour matrices, primes the nineteen
/// animation slots, then spawns the two companion enemies from the overlay's
/// table (entries 2 and 1) and points each one's model at the texture page and
/// CLUT row its room's `AreaPlacement` names.
///
/// `Enemy::spawnState` then picks how the enemy starts: 0 builds the full
/// object set -- the four `WorldCollisionBody` nodes with their `WorldCollisionContact` tables, the voice
/// cue looked up per room in `Actor02300_D15C80`, and the coin-flip in
/// `screamCharges` drawn from `gRandomLcgState` -- while 1 and 2 only prime the
/// animation state and hand straight on to the next task state.
static void Actor02300_Fn028AC(Enemy* enemy, Task* actor)
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
    Enemy*             eff2;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                param;
    u32                lcg;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(GolemPawnRookWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work                   = work;
    obj->flags                    = 0;
    coord->composeStamp           = GRAPHICS_COORD_DIRTY;
    obj->lightMtx                 = &work->lightMtx;
    obj->colorMtx                 = &work->colorMtx;
    work->actorId                 = 0x17;
    work->taskTable               = Actor02300_D15C98;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x500;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, Actor02300_D15CBC, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }

    eff = Gp_SpawnEnemyFromTable(Actor02300_D15C98, 2, 0, enemy);
    actorTintModel(eff->task->extra.tmd, enemy);
    eff2 = Gp_SpawnEnemyFromTable(Actor02300_D15C98, 1, 0, enemy);
    actorTintModel(eff2->task->extra.tmd, enemy);

    switch (enemy->spawnState) {
        case 0:
            enemy->field_4  = &coord->coord;
            enemy->field_48 = 0;
            Gp_LinkNode(&enemy->node);
            parts             = actor->extra.tmd->coords;
            enemy->bodyPos.vx = 0;
            enemy->bodyPos.vy = 0;
            enemy->bodyPos.vz = 0;
            enemy->param      = &Actor02300_D159D8;
            enemy->recs       = work->hurtContacts;
            enemy->coord      = &parts[3];
            enemy->hp         = Actor02300_D159D8.hpMax;
            Gp_IncStateF0Ref(0);
            work->patrols = enemy->place->mode & 1;
            if (work->patrols == 0) {
                work->anim     = 1;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = 2;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                param                    = enemy->place->variant;
                work->patrolDistanceLeft = param * 1000;
            }

            tbl = Actor02300_D15C80[gGameSession->location.loc.stage];
            if (tbl != NULL) {
                work->soundSet = tbl[gGameSession->location.loc.area];
            }
            if (work->soundSet != 0) {
                param1[3] = 0;
                param1[2] = 0xA;
                param1[0] = work->soundSet;
                param2[0] = 0x17;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            }

            work->shieldHp                  = 0xFA;
            work->sightCapsule.ends[0].vz   = 0x1F40;
            work->sightCapsule.end0Radius   = 0x3E8;
            work->sightCapsule.end1Radius   = 0x5DC;
            work->sightCapsule.ends[0].vx   = 0;
            work->sightCapsule.ends[0].vy   = 0;
            work->sightCapsule.ends[1].vx   = 0;
            work->sightCapsule.ends[1].vy   = 0;
            work->sightCapsule.ends[1].vz   = 0;
            work->sightCapsule.contacts     = work->sightContacts;
            lcg                             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->screamCharges             = ((lcg >> 16) & 1) + 1;
            gRandomLcgState                 = lcg;
            partsA                          = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &partsA[4];
            Gp_LinkObj(3, &work->sightBody);
            Gp_InitRec18Table(work->sightContacts, ARRAY_SIZE(work->sightContacts), 0);
            work->sightBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            partsB                          = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = 0x30017;
            work->hurtBody.radius           = 0x190;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &partsB[3];
            Gp_LinkObj(2, &work->hurtBody);
            Gp_InitRec18Table(work->hurtContacts, ARRAY_SIZE(work->hurtContacts), 0);
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
            Gp_LinkObj(2, &work->groundBody);
            Gp_InitRec18Table(work->groundContacts, ARRAY_SIZE(work->groundContacts), 0);
            work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            effParts                          = eff2->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = 0x1F4;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = 0x1F4;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = effParts;
            Gp_LinkObj(3, &work->strikeBody);
            Gp_InitRec18Table(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
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

#include "../../shared/golem_pawn_rook_frame.inc.c"

#include "../../shared/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/player_detection_segment.inc.c"

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_hit_reaction.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_flag_wait.inc.c"

#include "../../shared/golem_pawn_rook_downed_finish.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

/// State handlers of the child task hung off part 7 of the enemy's model -
/// spawn/setup, per-frame tick and teardown - dispatched through by
/// `Actor02300_Fn03BA8`.
static const EnemyTaskFuncTable3 Actor02300_D00060 = {
    golemPawnRookDelayedEffectSpawn,
    golemPawnRookDelayedEffectTick,
    enemyDestroy,
};

void Actor02300_Fn03BA8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02300_D00060;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_delayed_effect_spawn.inc.c"

#include "../../shared/golem_pawn_rook_delayed_effect_tick.inc.c"

/// State handlers of the child task hung off part 11 of the enemy's model -
/// spawn/setup, per-frame tick and teardown - dispatched through by
/// `Actor02300_Fn03CE8`.
static const EnemyTaskFuncTable3 Actor02300_D0006C = {
    golemPawnRookBurstPartSpawn,
    golemPawnRookBurstPartTick,
    enemyDestroy,
};

void Actor02300_Fn03CE8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02300_D0006C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_burst_part_spawn.inc.c"

/// The enemy's own state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor02300_Fn03EE8`. Each takes the task
/// as the enemy view it is.
static const EnemyTaskFuncTable3 Actor02300_D00078 = {
    Actor02300_Fn028AC,
    golemPawnRookFrameState,
    golemPawnRookDeadState,
};

#include "../../shared/golem_pawn_rook_burst_part.inc.c"

void Actor02300_Fn03EE8(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02300_D00078;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
