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
#include "gameplay/object_fields.h"
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
// The impact cue symbol carries twelve zero bytes after the id.
#define GOLEM_PAWN_ROOK_IMPACT_SOUND gGolemPawnRookImpactSound.value
#include "../../shared/golem_pawn_rook.h"

/// Placement descriptor for this actor.
extern DamageAttack gGolemPawnRookAttacks[5];

/// Enemy parameters the approach cycle parks at `Enemy::param`; its `hpMax`
/// becomes the enemy's `hp`.
extern EnemyParams Actor05600_D161D0[];

/// Per-stage tables of streaming cue ids, indexed by `GameSession::location.loc.stage`
/// and then `GameSession::location.loc.area`.
extern u16* Actor05600_D16478[];

/// Spawn table the approach cycle starts its companion enemy from, index 1.
extern TaskDesc Actor05600_D164A0[];

/// Animation stream set bound into the work block's animation context.
extern AnimationSet* Actor05600_D164C4[31];

/// Sound id of the burst cue, with the spawn context's room/channel bits packed
/// in.
extern s32 gGolemPawnRookShotSound;

/// Frame counts of the actor's animations, indexed by `GolemPawnRookWork.field_694`.
extern s16 gGolemPawnRookAnimBlendFrames[];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 gGolemPawnRookWeakPointWeapons[];
extern s16 gGolemPawnRookWeakPointPe[];

/// Sound ids of the actor's cues, indexed from `GolemPawnRookWork.field_6D6`.
extern s32 gGolemPawnRookVoiceCues[];

/// The approach cycle's per-state handlers, indexed by `GolemPawnRookWork.field_6A6`.
extern TaskFunc gGolemPawnRookStates[];

extern AnimationSet Actor05600_D0B3CC;
extern AnimationSet Actor05600_D0BD34;
extern AnimationSet Actor05600_D0C35C;
extern AnimationSet Actor05600_D0CB5C;
extern AnimationSet Actor05600_D0D10C;
extern AnimationSet Actor05600_D0D5F0;
extern AnimationSet Actor05600_D0DA50;
extern AnimationSet Actor05600_D0EA28;
extern AnimationSet Actor05600_D0F980;
extern AnimationSet Actor05600_D0FEA4;
extern AnimationSet Actor05600_D108B4;
extern AnimationSet Actor05600_D1135C;
extern AnimationSet Actor05600_D1193C;
extern AnimationSet Actor05600_D12630;
extern AnimationSet Actor05600_D136A8;
extern AnimationSet Actor05600_D13B7C;
extern AnimationSet Actor05600_D13E8C;
extern AnimationSet Actor05600_D14068;
extern AnimationSet Actor05600_D14FB0;
extern AnimationSet Actor05600_D15544;
extern AnimationSet Actor05600_D1580C;
extern AnimationSet Actor05600_D159E8;
extern AnimationSet Actor05600_D16194;
extern TmdSource    Actor05600_D0A020;
extern TmdSource    Actor05600_D0A46C;
extern TmdSource    Actor05600_D0A798;
void                Actor05600_Fn01E1C(Task*);
void                Actor05600_Fn041E4(Task*);
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

TmdBone Actor05600_D04D3C[19] = {
#include "assets/pawn_golem_body_skeleton.inc"
};

u32 Actor05600_D04FE8[19] = {
#include "assets/pawn_golem_body_partVerts.inc"
};

SVECTOR Actor05600_D05034[339] = {
#include "assets/pawn_golem_body_verts.inc"
};

SVECTOR Actor05600_D05ACC[346] = {
#include "assets/pawn_golem_body_normals.inc"
};

u32 Actor05600_D0659C[3745] = {
#include "assets/pawn_golem_body_stream.inc"
};

TmdSource Actor05600_D0A020 = {
    0,
    20476,
    5672,
    19,
    Actor05600_D04FE8,
    Actor05600_D05034,
    Actor05600_D05ACC,
    Actor05600_D04D3C,
    Actor05600_D0659C,
};

TmdBone Actor05600_D0A044[1] = {
#include "assets/golem_pawn_rook_grenade_launcher_skeleton.inc"
};

u32 Actor05600_D0A068[1] = {
#include "assets/golem_pawn_rook_grenade_launcher_partVerts.inc"
};

SVECTOR Actor05600_D0A06C[24] = {
#include "assets/golem_pawn_rook_grenade_launcher_verts.inc"
};

SVECTOR Actor05600_D0A12C[24] = {
#include "assets/golem_pawn_rook_grenade_launcher_normals.inc"
};

u32 Actor05600_D0A1EC[160] = {
#include "assets/golem_pawn_rook_grenade_launcher_stream.inc"
};

TmdSource Actor05600_D0A46C = {
    0,
    1144,
    0,
    1,
    Actor05600_D0A068,
    Actor05600_D0A06C,
    Actor05600_D0A12C,
    Actor05600_D0A044,
    Actor05600_D0A1EC,
};

TmdBone Actor05600_D0A490[1] = {
#include "assets/golem_no9_pawn_rook_grenade_skeleton.inc"
};

u32 Actor05600_D0A4B4[1] = {
#include "assets/golem_no9_pawn_rook_grenade_partVerts.inc"
};

SVECTOR Actor05600_D0A4B8[12] = {
#include "assets/golem_no9_pawn_rook_grenade_verts.inc"
};

SVECTOR Actor05600_D0A518[28] = {
#include "assets/golem_no9_pawn_rook_grenade_normals.inc"
};

u32 Actor05600_D0A5F8[104] = {
#include "assets/golem_no9_pawn_rook_grenade_stream.inc"
};

TmdSource Actor05600_D0A798 = {
    0,
    720,
    0,
    1,
    Actor05600_D0A4B4,
    Actor05600_D0A4B8,
    Actor05600_D0A518,
    Actor05600_D0A490,
    Actor05600_D0A5F8,
};

AnimationPackedPose Actor05600_D0A7BC[21] = {
#include "assets/actor_105600_animation_0B3CC_bank1.inc"
};

AnimationPackedRotation Actor05600_D0A8B8[317] = {
#include "assets/actor_105600_animation_0B3CC_bank4.inc"
};

AnimationRecord Actor05600_D0ADAC[382] = {
#include "assets/actor_105600_animation_0B3CC_records.inc"
};

u16 Actor05600_D0B3A4[20] = {
#include "assets/actor_105600_animation_0B3CC_indices.inc"
};

AnimationSet Actor05600_D0B3CC = {
    Actor05600_D0ADAC,
    Actor05600_D0B3A4,
    { NULL, Actor05600_D0A7BC, NULL, NULL, Actor05600_D0A8B8, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0B3F4[16] = {
#include "assets/actor_105600_animation_0BD34_bank1.inc"
};

AnimationPackedRotation Actor05600_D0B4B4[237] = {
#include "assets/actor_105600_animation_0BD34_bank4.inc"
};

AnimationRecord Actor05600_D0B868[297] = {
#include "assets/actor_105600_animation_0BD34_records.inc"
};

u16 Actor05600_D0BD0C[20] = {
#include "assets/actor_105600_animation_0BD34_indices.inc"
};

AnimationSet Actor05600_D0BD34 = {
    Actor05600_D0B868,
    Actor05600_D0BD0C,
    { NULL, Actor05600_D0B3F4, NULL, NULL, Actor05600_D0B4B4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0BD5C[12] = {
#include "assets/actor_105600_animation_0C35C_bank1.inc"
};

AnimationPackedRotation Actor05600_D0BDEC[142] = {
#include "assets/actor_105600_animation_0C35C_bank4.inc"
};

AnimationRecord Actor05600_D0C024[196] = {
#include "assets/actor_105600_animation_0C35C_records.inc"
};

u16 Actor05600_D0C334[20] = {
#include "assets/actor_105600_animation_0C35C_indices.inc"
};

AnimationSet Actor05600_D0C35C = {
    Actor05600_D0C024,
    Actor05600_D0C334,
    { NULL, Actor05600_D0BD5C, NULL, NULL, Actor05600_D0BDEC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0C384[14] = {
#include "assets/actor_105600_animation_0CB5C_bank1.inc"
};

AnimationPackedRotation Actor05600_D0C42C[186] = {
#include "assets/actor_105600_animation_0CB5C_bank4.inc"
};

AnimationRecord Actor05600_D0C714[264] = {
#include "assets/actor_105600_animation_0CB5C_records.inc"
};

u16 Actor05600_D0CB34[20] = {
#include "assets/actor_105600_animation_0CB5C_indices.inc"
};

AnimationSet Actor05600_D0CB5C = {
    Actor05600_D0C714,
    Actor05600_D0CB34,
    { NULL, Actor05600_D0C384, NULL, NULL, Actor05600_D0C42C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0CB84[9] = {
#include "assets/actor_105600_animation_0D10C_bank1.inc"
};

AnimationPackedRotation Actor05600_D0CBF0[139] = {
#include "assets/actor_105600_animation_0D10C_bank4.inc"
};

AnimationRecord Actor05600_D0CE1C[178] = {
#include "assets/actor_105600_animation_0D10C_records.inc"
};

u16 Actor05600_D0D0E4[20] = {
#include "assets/actor_105600_animation_0D10C_indices.inc"
};

AnimationSet Actor05600_D0D10C = {
    Actor05600_D0CE1C,
    Actor05600_D0D0E4,
    { NULL, Actor05600_D0CB84, NULL, NULL, Actor05600_D0CBF0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0D134[9] = {
#include "assets/actor_105600_animation_0D5F0_bank1.inc"
};

AnimationPackedRotation Actor05600_D0D1A0[107] = {
#include "assets/actor_105600_animation_0D5F0_bank4.inc"
};

AnimationRecord Actor05600_D0D34C[159] = {
#include "assets/actor_105600_animation_0D5F0_records.inc"
};

u16 Actor05600_D0D5C8[20] = {
#include "assets/actor_105600_animation_0D5F0_indices.inc"
};

AnimationSet Actor05600_D0D5F0 = {
    Actor05600_D0D34C,
    Actor05600_D0D5C8,
    { NULL, Actor05600_D0D134, NULL, NULL, Actor05600_D0D1A0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0D618[8] = {
#include "assets/actor_105600_animation_0DA50_bank1.inc"
};

AnimationPackedRotation Actor05600_D0D678[101] = {
#include "assets/actor_105600_animation_0DA50_bank4.inc"
};

AnimationRecord Actor05600_D0D80C[135] = {
#include "assets/actor_105600_animation_0DA50_records.inc"
};

u16 Actor05600_D0DA28[20] = {
#include "assets/actor_105600_animation_0DA50_indices.inc"
};

AnimationSet Actor05600_D0DA50 = {
    Actor05600_D0D80C,
    Actor05600_D0DA28,
    { NULL, Actor05600_D0D618, NULL, NULL, Actor05600_D0D678, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0DA78[29] = {
#include "assets/actor_105600_animation_0EA28_bank1.inc"
};

AnimationPackedRotation Actor05600_D0DBD4[407] = {
#include "assets/actor_105600_animation_0EA28_bank4.inc"
};

AnimationRecord Actor05600_D0E230[500] = {
#include "assets/actor_105600_animation_0EA28_records.inc"
};

u16 Actor05600_D0EA00[20] = {
#include "assets/actor_105600_animation_0EA28_indices.inc"
};

AnimationSet Actor05600_D0EA28 = {
    Actor05600_D0E230,
    Actor05600_D0EA00,
    { NULL, Actor05600_D0DA78, NULL, NULL, Actor05600_D0DBD4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0EA50[27] = {
#include "assets/actor_105600_animation_0F980_bank1.inc"
};

AnimationPackedRotation Actor05600_D0EB94[408] = {
#include "assets/actor_105600_animation_0F980_bank4.inc"
};

AnimationRecord Actor05600_D0F1F4[473] = {
#include "assets/actor_105600_animation_0F980_records.inc"
};

u16 Actor05600_D0F958[20] = {
#include "assets/actor_105600_animation_0F980_indices.inc"
};

AnimationSet Actor05600_D0F980 = {
    Actor05600_D0F1F4,
    Actor05600_D0F958,
    { NULL, Actor05600_D0EA50, NULL, NULL, Actor05600_D0EB94, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0F9A8[8] = {
#include "assets/actor_105600_animation_0FEA4_bank1.inc"
};

AnimationPackedRotation Actor05600_D0FA08[116] = {
#include "assets/actor_105600_animation_0FEA4_bank4.inc"
};

AnimationRecord Actor05600_D0FBD8[169] = {
#include "assets/actor_105600_animation_0FEA4_records.inc"
};

u16 Actor05600_D0FE7C[20] = {
#include "assets/actor_105600_animation_0FEA4_indices.inc"
};

AnimationSet Actor05600_D0FEA4 = {
    Actor05600_D0FBD8,
    Actor05600_D0FE7C,
    { NULL, Actor05600_D0F9A8, NULL, NULL, Actor05600_D0FA08, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D0FECC[19] = {
#include "assets/actor_105600_animation_108B4_bank1.inc"
};

AnimationPackedRotation Actor05600_D0FFB0[255] = {
#include "assets/actor_105600_animation_108B4_bank4.inc"
};

AnimationRecord Actor05600_D103AC[312] = {
#include "assets/actor_105600_animation_108B4_records.inc"
};

u16 Actor05600_D1088C[20] = {
#include "assets/actor_105600_animation_108B4_indices.inc"
};

AnimationSet Actor05600_D108B4 = {
    Actor05600_D103AC,
    Actor05600_D1088C,
    { NULL, Actor05600_D0FECC, NULL, NULL, Actor05600_D0FFB0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D108DC[18] = {
#include "assets/actor_105600_animation_1135C_bank1.inc"
};

AnimationPackedRotation Actor05600_D109B4[281] = {
#include "assets/actor_105600_animation_1135C_bank4.inc"
};

AnimationRecord Actor05600_D10E18[327] = {
#include "assets/actor_105600_animation_1135C_records.inc"
};

u16 Actor05600_D11334[20] = {
#include "assets/actor_105600_animation_1135C_indices.inc"
};

AnimationSet Actor05600_D1135C = {
    Actor05600_D10E18,
    Actor05600_D11334,
    { NULL, Actor05600_D108DC, NULL, NULL, Actor05600_D109B4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D11384[8] = {
#include "assets/actor_105600_animation_1193C_bank1.inc"
};

AnimationPackedRotation Actor05600_D113E4[130] = {
#include "assets/actor_105600_animation_1193C_bank4.inc"
};

AnimationRecord Actor05600_D115EC[202] = {
#include "assets/actor_105600_animation_1193C_records.inc"
};

u16 Actor05600_D11914[20] = {
#include "assets/actor_105600_animation_1193C_indices.inc"
};

AnimationSet Actor05600_D1193C = {
    Actor05600_D115EC,
    Actor05600_D11914,
    { NULL, Actor05600_D11384, NULL, NULL, Actor05600_D113E4, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D11964[23] = {
#include "assets/actor_105600_animation_12630_bank1.inc"
};

AnimationPackedRotation Actor05600_D11A78[326] = {
#include "assets/actor_105600_animation_12630_bank4.inc"
};

AnimationRecord Actor05600_D11F90[414] = {
#include "assets/actor_105600_animation_12630_records.inc"
};

u16 Actor05600_D12608[20] = {
#include "assets/actor_105600_animation_12630_indices.inc"
};

AnimationSet Actor05600_D12630 = {
    Actor05600_D11F90,
    Actor05600_D12608,
    { NULL, Actor05600_D11964, NULL, NULL, Actor05600_D11A78, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D12658[31] = {
#include "assets/actor_105600_animation_136A8_bank1.inc"
};

AnimationPackedRotation Actor05600_D127CC[429] = {
#include "assets/actor_105600_animation_136A8_bank4.inc"
};

AnimationRecord Actor05600_D12E80[512] = {
#include "assets/actor_105600_animation_136A8_records.inc"
};

u16 Actor05600_D13680[20] = {
#include "assets/actor_105600_animation_136A8_indices.inc"
};

AnimationSet Actor05600_D136A8 = {
    Actor05600_D12E80,
    Actor05600_D13680,
    { NULL, Actor05600_D12658, NULL, NULL, Actor05600_D127CC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D136D0[9] = {
#include "assets/actor_105600_animation_13B7C_bank1.inc"
};

AnimationPackedRotation Actor05600_D1373C[113] = {
#include "assets/actor_105600_animation_13B7C_bank4.inc"
};

AnimationRecord Actor05600_D13900[149] = {
#include "assets/actor_105600_animation_13B7C_records.inc"
};

u16 Actor05600_D13B54[20] = {
#include "assets/actor_105600_animation_13B7C_indices.inc"
};

AnimationSet Actor05600_D13B7C = {
    Actor05600_D13900,
    Actor05600_D13B54,
    { NULL, Actor05600_D136D0, NULL, NULL, Actor05600_D1373C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D13BA4[5] = {
#include "assets/actor_105600_animation_13E8C_bank1.inc"
};

AnimationPackedRotation Actor05600_D13BE0[65] = {
#include "assets/actor_105600_animation_13E8C_bank4.inc"
};

AnimationRecord Actor05600_D13CE4[96] = {
#include "assets/actor_105600_animation_13E8C_records.inc"
};

u16 Actor05600_D13E64[20] = {
#include "assets/actor_105600_animation_13E8C_indices.inc"
};

AnimationSet Actor05600_D13E8C = {
    Actor05600_D13CE4,
    Actor05600_D13E64,
    { NULL, Actor05600_D13BA4, NULL, NULL, Actor05600_D13BE0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D13EB4[2] = {
#include "assets/actor_105600_animation_14068_bank1.inc"
};

AnimationPackedRotation Actor05600_D13ECC[17] = {
#include "assets/actor_105600_animation_14068_bank4.inc"
};

AnimationRecord Actor05600_D13F10[76] = {
#include "assets/actor_105600_animation_14068_records.inc"
};

u16 Actor05600_D14040[20] = {
#include "assets/actor_105600_animation_14068_indices.inc"
};

AnimationSet Actor05600_D14068 = {
    Actor05600_D13F10,
    Actor05600_D14040,
    { NULL, Actor05600_D13EB4, NULL, NULL, Actor05600_D13ECC, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D14090[28] = {
#include "assets/actor_105600_animation_14FB0_bank1.inc"
};

AnimationPackedRotation Actor05600_D141E0[394] = {
#include "assets/actor_105600_animation_14FB0_bank4.inc"
};

AnimationRecord Actor05600_D14808[480] = {
#include "assets/actor_105600_animation_14FB0_records.inc"
};

u16 Actor05600_D14F88[20] = {
#include "assets/actor_105600_animation_14FB0_indices.inc"
};

AnimationSet Actor05600_D14FB0 = {
    Actor05600_D14808,
    Actor05600_D14F88,
    { NULL, Actor05600_D14090, NULL, NULL, Actor05600_D141E0, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D14FD8[9] = {
#include "assets/actor_105600_animation_15544_bank1.inc"
};

AnimationPackedRotation Actor05600_D15044[127] = {
#include "assets/actor_105600_animation_15544_bank4.inc"
};

AnimationRecord Actor05600_D15240[183] = {
#include "assets/actor_105600_animation_15544_records.inc"
};

u16 Actor05600_D1551C[20] = {
#include "assets/actor_105600_animation_15544_indices.inc"
};

AnimationSet Actor05600_D15544 = {
    Actor05600_D15240,
    Actor05600_D1551C,
    { NULL, Actor05600_D14FD8, NULL, NULL, Actor05600_D15044, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D1556C[5] = {
#include "assets/actor_105600_animation_1580C_bank1.inc"
};

AnimationPackedRotation Actor05600_D155A8[56] = {
#include "assets/actor_105600_animation_1580C_bank4.inc"
};

AnimationRecord Actor05600_D15688[87] = {
#include "assets/actor_105600_animation_1580C_records.inc"
};

u16 Actor05600_D157E4[20] = {
#include "assets/actor_105600_animation_1580C_indices.inc"
};

AnimationSet Actor05600_D1580C = {
    Actor05600_D15688,
    Actor05600_D157E4,
    { NULL, Actor05600_D1556C, NULL, NULL, Actor05600_D155A8, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D15834[2] = {
#include "assets/actor_105600_animation_159E8_bank1.inc"
};

AnimationPackedRotation Actor05600_D1584C[17] = {
#include "assets/actor_105600_animation_159E8_bank4.inc"
};

AnimationRecord Actor05600_D15890[76] = {
#include "assets/actor_105600_animation_159E8_records.inc"
};

u16 Actor05600_D159C0[20] = {
#include "assets/actor_105600_animation_159E8_indices.inc"
};

AnimationSet Actor05600_D159E8 = {
    Actor05600_D15890,
    Actor05600_D159C0,
    { NULL, Actor05600_D15834, NULL, NULL, Actor05600_D1584C, NULL, NULL, NULL },
};

AnimationPackedPose Actor05600_D15A10[15] = {
#include "assets/actor_105600_animation_16194_bank1.inc"
};

AnimationPackedRotation Actor05600_D15AC4[175] = {
#include "assets/actor_105600_animation_16194_bank4.inc"
};

AnimationRecord Actor05600_D15D80[251] = {
#include "assets/actor_105600_animation_16194_records.inc"
};

u16 Actor05600_D1616C[20] = {
#include "assets/actor_105600_animation_16194_indices.inc"
};

AnimationSet Actor05600_D16194 = {
    Actor05600_D15D80,
    Actor05600_D1616C,
    { NULL, Actor05600_D15A10, NULL, NULL, Actor05600_D15AC4, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 30, 5 },
    { 20, 5 },
    { 0, 8 },
    { 18, 1 },
    { 30, 7 },
};

EnemyParams Actor05600_D161D0[1] = {
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

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s32 value;
    u8  retained[12];
} Actor05600Storage8114;
STATIC_ASSERT_SIZEOF(Actor05600Storage8114, 16);

Actor05600Storage8114 gGolemPawnRookImpactSound = { 0x40380008, { 0 } };

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

u16* Actor05600_D16478[6] = {
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

TaskDesc Actor05600_D164A0[2] = {
    { { { TASK_BODY_TMD, 96 } }, Actor05600_Fn04CA0, { .model = &Actor05600_D0A020 } },
    { { { TASK_BODY_TMD, 96 } }, Actor05600_Fn04A70, { .model = &Actor05600_D0A46C } },
};

TaskDesc Actor05600_D164B8 = { { { TASK_BODY_TMD, 96 } }, Actor05600_Fn04BAC, { .model = &Actor05600_D0A798 } };

AnimationSet* Actor05600_D164C4[31] = {
    NULL,
    &Actor05600_D0B3CC,
    &Actor05600_D0BD34,
    &Actor05600_D0C35C,
    &Actor05600_D0CB5C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &Actor05600_D0D10C,
    &Actor05600_D0D5F0,
    &Actor05600_D0DA50,
    &Actor05600_D0EA28,
    &Actor05600_D0F980,
    &Actor05600_D0FEA4,
    &Actor05600_D108B4,
    &Actor05600_D1135C,
    &Actor05600_D1193C,
    &Actor05600_D12630,
    &Actor05600_D136A8,
    &Actor05600_D13B7C,
    &Actor05600_D13E8C,
    &Actor05600_D14068,
    &Actor05600_D14FB0,
    &Actor05600_D15544,
    &Actor05600_D1580C,
    &Actor05600_D159E8,
    &Actor05600_D16194,
};

TaskFunc gGolemPawnRookStates[15] = {
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    Actor05600_Fn041E4,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    Actor05600_Fn01E1C,
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

/// Sound id of the burst cue, with the spawn context's room/channel bits
/// packed in.
extern Actor05600Storage8114 gGolemPawnRookImpactSound;

static void Actor05600_Fn03924(Enemy* ctx, Task* actor);

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

/// Approach-cycle state machine, entry 6 of `gGolemPawnRookStates` for the
/// second half of the fight. State 0 waits out the opening clip; state 1 backs
/// away while tracking the player and running the aim helper, for 0x1E frames;
/// state 2 measures the distance and yaw error to the companion in slot 3 and
/// either breaks off (too close) or commits to the lunge; state 3 keeps facing
/// the companion, counts the strikes in `field_6BC` / `field_6BE` and picks the
/// follow-up clip from them; state 4 fires the effect burst; states 5 and 6
/// hand back to the other handlers. The delta vector and its normal are carved
/// off the scratch stack and released on the way out.
void Actor05600_Fn01E1C(Task* arg0)
{
    s16                diff;
    s32                mag;
    s16                angle;
    s32                dx;
    s32                dz;
    VECTOR*            delta;
    VECTOR*            normal;
    VECTOR*            normal2;
    GfxCoord*          target;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    delta = (VECTOR*)SCRATCH_STACK_RESERVE_BYTES(0x20);
    work  = (GolemPawnRookWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_6A8) {
        case 0:
            if (work->field_698 >= 0x14) {
                work->field_6A8 = 1;
                work->field_694 = 0x1E;
                work->field_6AE = 0;
            }
            break;
        case 1:
            work->field_69C = -0x16;
            work->field_69E = 0x1E;
            work->field_6CE = work->field_6D0 > 0;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
            golemPawnRookAimLaserSight(arg0);
            work->field_6AE++;
            if (work->field_6AE >= 0x1E) {
                work->field_6A8        = 2;
                work->field_6AE        = 0;
                work->field_61C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 2:
            work->field_6CE = work->field_6D0 > 0;
            target          = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];
            delta->vx       = target->workm.t[0] - coord->workm.t[0];
            normal          = delta + 1;
            delta->vy       = target->workm.t[1] - coord->workm.t[1];
            delta->vz       = target->workm.t[2] - coord->workm.t[2];
            VectorNormal(delta, normal);
            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, delta);
            dx = delta->vx;
            dz = delta->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
                work->field_6CE = 0;
                break;
            }
            diff = (ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF) - work->field_6A2;
            mag  = __builtin_abs(diff);
            if (mag < 0x800) {
                angle = mag;
            } else if (diff > 0) {
                angle = 0x1000 - diff;
            } else {
                angle = diff + 0x1000;
            }
            if (angle >= 0x101) {
                work->field_6A8 = 6;
                work->field_694 = 0xE;
                work->field_6CE = 0;
            } else {
                work->field_6A8 = 3;
                work->field_694 = 0xD;
                work->field_6CC = 1;
                work->field_6BA = 1;
                work->field_6B6 = 0;
                work->field_6BC++;
                work->field_6BE++;
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_6CE = work->field_6D0 > 0;
            if (work->field_698 < 3) {
                work->field_69E = 0;
            } else {
                target    = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];
                delta->vx = target->workm.t[0] - coord->workm.t[0];
                normal2   = delta + 1;
                delta->vy = target->workm.t[1] - coord->workm.t[1];
                delta->vz = target->workm.t[2] - coord->workm.t[2];
                VectorNormal(delta, normal2);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal2, delta);
                work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
                work->field_69E = 7;
            }
            if (work->field_6BE != 0 && work->field_698 == gGolemPawnRookAnimBlendFrames[13] - 1) {
                work->field_6BA = 1;
                work->field_6BC++;
                work->field_6BE++;
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6A6        = 8;
                work->field_6A8        = 0;
                work->field_69C        = 0;
                work->field_69E        = 0;
                work->field_6CC        = 0;
                work->field_6CE        = 0;
                work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (work->field_6BC >= 6) {
                if (work->field_698 >= gGolemPawnRookAnimBlendFrames[13] + 0x16) {
                    work->field_6A8                   = 4;
                    work->field_694                   = 0xF;
                    work->field_6BC                   = 0;
                    work->field_6BE                   = 0;
                    gGolemPawnRookAnimBlendFrames[13] = 0;
                    work->field_6CC                   = 0;
                }
            } else if (work->field_6BE < 6) {
                if (work->field_698 >= gGolemPawnRookAnimBlendFrames[13] + 3) {
                    gGolemPawnRookAnimBlendFrames[13] = 3;
                    work->field_694                   = 0xD;
                    work->field_696                   = 0x1E;
                }
            } else if (work->field_698 >= gGolemPawnRookAnimBlendFrames[13] + 0x16) {
                work->field_6A8                   = 1;
                work->field_6BE                   = 0;
                work->field_694                   = 0x1E;
                gGolemPawnRookAnimBlendFrames[13] = 0;
                work->field_6CC                   = 0;
            }
            break;
        case 4:
            if (work->field_698 == 0x1A) {
                Gp_SpawnEff(0x6006E, &arg0->extra.tmd->coords[7], 0x6000C, NULL);
            }
            work->field_6CE = 0;
            if (work->field_698 >= 0x87) {
                work->field_6A8 = 5;
            }
            break;
        case 5:
            work->field_6A6 = 2;
            work->field_6A8 = 2;
            work->field_694 = 4;
            break;
        case 6:
            if (work->field_698 >= 0x19) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

#include "../../shared/golem_pawn_rook_lunge_strike.inc.c"

#include "../../shared/golem_pawn_rook_laser_sight.inc.c"

#include "../../shared/golem_pawn_rook_laser_beam.inc.c"

#include "../../shared/golem_pawn_rook_bullet_spawn.inc.c"

#include "../../shared/golem_pawn_rook_bullet_fly.inc.c"

/// Spawn handler of the approach cycle: allocates the 0x6E4-byte work block,
/// binds the animation set and reseeds the nineteen slots, then starts the
/// companion enemy whose model takes its texture page and CLUT row from the
/// current room's area record. `Enemy::spawnState` picks how much of that is
/// kept: 0 also links the list node, the five `Gp_LinkObj` collision nodes with
/// their `WorldCollisionContact` tables and the room's streaming cue, while 1 and 2 only
/// prime the animation state. Entry 0 of `Actor05600_D00098`.
static void Actor05600_Fn03924(Enemy* ctx, Task* actor)
{
    GolemPawnRookWork* work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          parts;
    GfxCoord*          partsA;
    GfxCoord*          partsB;
    GfxCoord*          partsC;
    GfxCoord*          partsD;
    GfxCoord*          effParts;
    Enemy*             eff;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                param;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x38;
    work->field_66C            = Actor05600_D164A0;
    work->field_670.coord      = &actor->extra.tmd->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->rig.anim, Actor05600_D164C4, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    eff = Gp_SpawnEnemyFromTable(Actor05600_D164A0, 1, 0, ctx);
    actorTintModel(eff->task->extra.tmd, ctx);

    switch (ctx->spawnState) {
        case 0:
            ctx->field_4  = &coord->coord;
            ctx->field_48 = 0;
            Gp_LinkNode(&ctx->node);
            parts           = actor->extra.tmd->coords;
            ctx->bodyPos.vx = 0;
            ctx->bodyPos.vy = 0;
            ctx->bodyPos.vz = 0;
            ctx->param      = Actor05600_D161D0;
            ctx->recs       = work->field_4EC;
            ctx->coord      = &parts[3];
            ctx->hp         = Actor05600_D161D0->hpMax;
            Gp_IncStateF0Ref(0);
            work->field_6AC = ctx->place->mode & 1;
            if (work->field_6AC == 0) {
                work->field_694 = 1;
                work->field_6A6 = 0;
            } else {
                work->field_694 = 2;
                work->field_6A6 = 1;
                param           = ctx->place->variant;
                work->field_6DA = param * 1000;
            }

            tbl = Actor05600_D16478[gGameSession->location.loc.stage];
            if (tbl != NULL) {
                work->field_6D6 = tbl[gGameSession->location.loc.area];
            }
            if (work->field_6D6 != 0) {
                param1[3] = 0;
                param1[2] = 0xA;
                param1[0] = work->field_6D6;
                param2[0] = 0x38;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }

            work->field_49C.ends[0].vz      = 0x1F40;
            work->field_49C.end0Radius      = 0x3E8;
            work->field_49C.ends[0].vx      = 0;
            work->field_49C.ends[0].vy      = 0;
            work->field_49C.ends[1].vx      = 0;
            work->field_49C.ends[1].vy      = 0;
            work->field_49C.ends[1].vz      = 0;
            work->field_49C.end1Radius      = 0x5DC;
            work->field_49C.contacts        = work->field_4B4;
            partsA                          = actor->extra.tmd->coords;
            work->field_47C.context.capsule = &work->field_49C;
            work->field_47C.pos.vx          = 0;
            work->field_47C.pos.vy          = 0;
            work->field_47C.pos.vz          = 0;
            work->field_47C.key             = 0;
            work->field_47C.radius          = 0;
            work->field_47C.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->field_47C.coord           = &partsA[4];
            Gp_LinkObj(3, &work->field_47C);
            Gp_InitRec18Table(work->field_4B4, 1, 0);
            work->field_47C.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            partsB                           = actor->extra.tmd->coords;
            work->field_4CC.context.contacts = work->field_4EC;
            work->field_4CC.pos.vx           = 0;
            work->field_4CC.pos.vy           = 0;
            work->field_4CC.pos.vz           = 0;
            work->field_4CC.key              = 0x30038;
            work->field_4CC.radius           = 0x190;
            work->field_4CC.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->field_4CC.coord            = &partsB[3];
            Gp_LinkObj(2, &work->field_4CC);
            Gp_InitRec18Table(work->field_4EC, 5, 0);
            work->field_4CC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

            partsC                           = actor->extra.tmd->coords;
            work->field_564.pos.vy           = -0x226;
            work->field_564.context.contacts = work->field_584;
            work->field_564.pos.vx           = 0;
            work->field_564.pos.vz           = 0;
            work->field_564.key              = 0;
            work->field_564.radius           = 0x226;
            work->field_564.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->field_564.coord            = partsC;
            Gp_LinkObj(2, &work->field_564);
            Gp_InitRec18Table(work->field_584, 4, 0);
            work->field_564.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            effParts                         = eff->task->extra.tmd->coords;
            work->field_5E4.context.contacts = work->field_604;
            work->field_5E4.pos.vx           = 0;
            work->field_5E4.pos.vy           = 0x1F4;
            work->field_5E4.pos.vz           = 0;
            work->field_5E4.key              = 0;
            work->field_5E4.radius           = 0x1F4;
            work->field_5E4.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->field_5E4.coord            = effParts;
            Gp_LinkObj(3, &work->field_5E4);
            Gp_InitRec18Table(work->field_604, 1, 0);
            work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

            work->field_63C.ends[0].vx      = 0;
            work->field_63C.ends[0].vy      = 0;
            work->field_63C.ends[0].vz      = 0;
            work->field_63C.ends[1].vx      = 0;
            work->field_63C.ends[1].vy      = 0;
            work->field_63C.ends[1].vz      = 0;
            work->field_63C.end0Radius      = 1;
            work->field_63C.end1Radius      = 1;
            work->field_63C.contacts        = work->field_654;
            partsD                          = actor->extra.tmd->coords;
            work->field_61C.context.capsule = &work->field_63C;
            work->field_61C.pos.vx          = 0;
            work->field_61C.pos.vy          = 0;
            work->field_61C.pos.vz          = 0;
            work->field_61C.key             = 0;
            work->field_61C.radius          = 0;
            work->field_61C.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->field_61C.coord           = partsD;
            Gp_LinkObj(3, &work->field_61C);
            Gp_InitRec18Table(work->field_654, 1, 0);
            work->field_61C.flags = (work->field_61C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            actor->state          = 1;
            break;
        case 1:
            work->field_694 = 0x19;
            work->field_6A8 = 2;
            actor->state    = 2;
            break;
        case 2:
            work->field_694 = 0x1D;
            work->field_6A8 = 2;
            actor->state    = 2;
            break;
    }
}

#include "../../shared/golem_pawn_rook_inlines.inc.c"

/// Saves the root coordinate's translation in `field_678`..`field_680`, then
/// Updates the enemy's colour from `coord`'s world position and draws the
#include "../../shared/golem_pawn_rook_frame_no_dust.inc.c"

/// Approach-cycle state machine, entry 6 of `gGolemPawnRookStates`. State 0
/// turns the actor toward the player, handing over to the charge animation once
/// the clip has run and the yaw error is wide (or to the recovery animation when
/// `field_6DC` is set); state 1 picks the close or far attack from the distance
/// to the player; state 2 waits out its clip before turning again; state 3 arms
/// `field_6DC` and drops back to state 0.
void Actor05600_Fn041E4(Task* arg0)
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
    work                     = (GolemPawnRookWork*)arg0->work;
    state                    = work->field_6A8;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= gGolemPawnRookAnimBlendFrames[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x1E;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
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
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            dz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
            } else {
                work->field_6A6 = 6;
                work->field_6A8 = 0;
                work->field_694 = 0xC;
                work->field_6AE = 0;
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = work->field_6A4 - yaw2;
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
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
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

/// State handlers of the model child hung off the actor's part 7 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05600_Fn04A70`.
static const GpEnemyTaskFuncTable3 Actor05600_D00080 = {
    golemPawnRookGunSpawn,
    golemPawnRookGunTick,
    Gp_DestroyEnemy,
};

void Actor05600_Fn04A70(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D00080;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_gun_spawn.inc.c"

#include "../../shared/golem_pawn_rook_gun_tick.inc.c"

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
static const GpEnemyTaskFuncTable3 Actor05600_D0008C = {
    golemPawnRookBulletSpawn,
    golemPawnRookBulletFly,
    golemPawnRookBulletDestroy,
};

void Actor05600_Fn04BAC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/golem_pawn_rook_bullet_destroy.inc.c"

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
static const GpEnemyTaskFuncTable3 Actor05600_D00098 = {
    Actor05600_Fn03924,
    golemPawnRookFrameStateNoDust,
    golemPawnRookDeadState,
};

void Actor05600_Fn04CA0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
