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
#include "../../shared/lunging_enemy.h"

/// First frame of each animation, indexed by `Actor105600Work::field_694`;
/// the state handlers offset it to get the frames their cues fire on.
extern s16 gLungerAnimBlendFrames[];
/// The `Gp_PackPair` entry the lunge parks in the work block's 0x5E4 node.
extern DamageAttack Actor02300_D159C4[5];
/// Base sound id of the lunge cue, ORed with the enemy's id nibble.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s32 value;
    u8  retained[4];
} Actor02300Storage7918;
STATIC_ASSERT_SIZEOF(Actor02300Storage7918, 8);

extern Actor02300Storage7918 Actor02300_D15AF8;

/// The `EnemyParams` the enemy parks in its own `field_50` slot.
extern EnemyParams Actor02300_D159D8;
/// Per-room voice-stream sector tables, indexed by `GameSession::location.loc.stage` then
/// `field_6`; a NULL row means this room has no cue.
extern u16* Actor02300_D15C80[];
/// The overlay's own spawn table: entry 0 is this enemy, 1 and 2 the two
/// companions the setup state spawns.
extern TaskDesc Actor02300_D15C98[];
/// Animation bank `func_800B3F84` binds to the work block.
extern AnimationSet* Actor02300_D15CBC[31];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 Actor02300_D159E8[];
extern s16 Actor02300_D15A44[];

/// Voice-cue sound ids, indexed from `Actor105600Work::field_6D6`.
extern s32 gLungerVoiceCues[];
/// Sound ids of the burst state's two cues.
extern s32 gLungerScreamCue;
extern s32 gLungerSilenceCue;

/// Handlers of the `Actor105600Work::field_6A6` states, one per entry.
extern TaskFunc gLungerStates[];

/// Sound id of the part-11 child's cue, ORed with the enemy's id nibble.
extern s32 gLungerBurstCue;

static void Actor02300_Fn03C04(Enemy* arg0, Task* task);
static void Actor02300_Fn03C50(Enemy* arg0, Task* task);
static void Actor02300_Fn03D44(Enemy* arg0, Task* task);

extern AnimationSet Actor02300_D0A2F0;
extern AnimationSet Actor02300_D0AC58;
extern AnimationSet Actor02300_D0B280;
extern AnimationSet Actor02300_D0BA80;
extern AnimationSet Actor02300_D0C86C;
extern AnimationSet Actor02300_D0CDE0;
extern AnimationSet Actor02300_D0DCB0;
extern AnimationSet Actor02300_D0E9E8;
extern AnimationSet Actor02300_D0F120;
extern AnimationSet Actor02300_D0F328;
extern AnimationSet Actor02300_D0F934;
extern AnimationSet Actor02300_D0FE58;
extern AnimationSet Actor02300_D10868;
extern AnimationSet Actor02300_D11310;
extern AnimationSet Actor02300_D118F0;
extern AnimationSet Actor02300_D125E4;
extern AnimationSet Actor02300_D1365C;
extern AnimationSet Actor02300_D13B30;
extern AnimationSet Actor02300_D13E40;
extern AnimationSet Actor02300_D1401C;
extern AnimationSet Actor02300_D14F64;
extern AnimationSet Actor02300_D154F8;
extern AnimationSet Actor02300_D157C0;
extern AnimationSet Actor02300_D1599C;
extern TmdSource    Actor02300_D08E50;
extern TmdSource    Actor02300_D09394;
extern TmdSource    Actor02300_D096BC;
void                Actor02300_Fn00E0C(Task*);
void                Actor02300_Fn01DF0(Task*);
void                Actor02300_Fn02290(Task*);
void                Actor02300_Fn0327C(Task*);
void                Actor02300_Fn03908(Task*);
void                Actor02300_Fn03A5C(Task*);
void                Actor02300_Fn03BA0(Task*);
void                Actor02300_Fn03BA8(Task*);
void                Actor02300_Fn03CE8(Task*);
void                Actor02300_Fn03EE8(Task*);

s16 gLungerAnimBlendFrames[32] = {
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

TmdBone Actor02300_D03F84[19] = {
#include "assets/actor_102300_model_08E50_skeleton.inc"
};

u32 Actor02300_D04230[19] = {
#include "assets/actor_102300_model_08E50_partVerts.inc"
};

SVECTOR Actor02300_D0427C[328] = {
#include "assets/actor_102300_model_08E50_verts.inc"
};

SVECTOR Actor02300_D04CBC[344] = {
#include "assets/actor_102300_model_08E50_normals.inc"
};

u32 Actor02300_D0577C[3509] = {
#include "assets/actor_102300_model_08E50_stream.inc"
};

TmdSource Actor02300_D08E50 = {
    0,
    19580,
    4888,
    19,
    Actor02300_D04230,
    Actor02300_D0427C,
    Actor02300_D04CBC,
    Actor02300_D03F84,
    Actor02300_D0577C,
};

TmdBone Actor02300_D08E74[1] = {
#include "assets/actor_102300_model_09394_skeleton.inc"
};

u32 Actor02300_D08E98[1] = {
#include "assets/actor_102300_model_09394_partVerts.inc"
};

SVECTOR Actor02300_D08E9C[29] = {
#include "assets/actor_102300_model_09394_verts.inc"
};

SVECTOR Actor02300_D08F84[24] = {
#include "assets/actor_102300_model_09394_normals.inc"
};

u32 Actor02300_D09044[212] = {
#include "assets/actor_102300_model_09394_stream.inc"
};

TmdSource Actor02300_D09394 = {
    0,
    1436,
    0,
    1,
    Actor02300_D08E98,
    Actor02300_D08E9C,
    Actor02300_D08F84,
    Actor02300_D08E74,
    Actor02300_D09044,
};

TmdBone Actor02300_D093B8[1] = {
#include "assets/actor_102300_model_096BC_skeleton.inc"
};

u32 Actor02300_D093DC[1] = {
#include "assets/actor_102300_model_096BC_partVerts.inc"
};

SVECTOR Actor02300_D093E0[16] = {
#include "assets/actor_102300_model_096BC_verts.inc"
};

SVECTOR Actor02300_D09460[20] = {
#include "assets/actor_102300_model_096BC_normals.inc"
};

u32 Actor02300_D09500[111] = {
#include "assets/actor_102300_model_096BC_stream.inc"
};

TmdSource Actor02300_D096BC = {
    0,
    780,
    0,
    1,
    Actor02300_D093DC,
    Actor02300_D093E0,
    Actor02300_D09460,
    Actor02300_D093B8,
    Actor02300_D09500,
};

AnimationPackedPose Actor02300_D096E0[21] = {
#include "assets/actor_102300_animation_0A2F0_bank1.inc"
};

AnimationPackedRotation Actor02300_D097DC[317] = {
#include "assets/actor_102300_animation_0A2F0_bank4.inc"
};

AnimationRecord Actor02300_D09CD0[382] = {
#include "assets/actor_102300_animation_0A2F0_records.inc"
};

u16 Actor02300_D0A2C8[20] = {
#include "assets/actor_102300_animation_0A2F0_indices.inc"
};

AnimationSet Actor02300_D0A2F0 = {
    Actor02300_D09CD0,
    Actor02300_D0A2C8,
    { NULL, Actor02300_D096E0, NULL, NULL, Actor02300_D097DC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0A318[16] = {
#include "assets/actor_102300_animation_0AC58_bank1.inc"
};

AnimationPackedRotation Actor02300_D0A3D8[237] = {
#include "assets/actor_102300_animation_0AC58_bank4.inc"
};

AnimationRecord Actor02300_D0A78C[297] = {
#include "assets/actor_102300_animation_0AC58_records.inc"
};

u16 Actor02300_D0AC30[20] = {
#include "assets/actor_102300_animation_0AC58_indices.inc"
};

AnimationSet Actor02300_D0AC58 = {
    Actor02300_D0A78C,
    Actor02300_D0AC30,
    { NULL, Actor02300_D0A318, NULL, NULL, Actor02300_D0A3D8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0AC80[12] = {
#include "assets/actor_102300_animation_0B280_bank1.inc"
};

AnimationPackedRotation Actor02300_D0AD10[142] = {
#include "assets/actor_102300_animation_0B280_bank4.inc"
};

AnimationRecord Actor02300_D0AF48[196] = {
#include "assets/actor_102300_animation_0B280_records.inc"
};

u16 Actor02300_D0B258[20] = {
#include "assets/actor_102300_animation_0B280_indices.inc"
};

AnimationSet Actor02300_D0B280 = {
    Actor02300_D0AF48,
    Actor02300_D0B258,
    { NULL, Actor02300_D0AC80, NULL, NULL, Actor02300_D0AD10, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0B2A8[14] = {
#include "assets/actor_102300_animation_0BA80_bank1.inc"
};

AnimationPackedRotation Actor02300_D0B350[186] = {
#include "assets/actor_102300_animation_0BA80_bank4.inc"
};

AnimationRecord Actor02300_D0B638[264] = {
#include "assets/actor_102300_animation_0BA80_records.inc"
};

u16 Actor02300_D0BA58[20] = {
#include "assets/actor_102300_animation_0BA80_indices.inc"
};

AnimationSet Actor02300_D0BA80 = {
    Actor02300_D0B638,
    Actor02300_D0BA58,
    { NULL, Actor02300_D0B2A8, NULL, NULL, Actor02300_D0B350, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0BAA8[25] = {
#include "assets/actor_102300_animation_0C86C_bank1.inc"
};

AnimationPackedRotation Actor02300_D0BBD4[368] = {
#include "assets/actor_102300_animation_0C86C_bank4.inc"
};

AnimationRecord Actor02300_D0C194[428] = {
#include "assets/actor_102300_animation_0C86C_records.inc"
};

u16 Actor02300_D0C844[20] = {
#include "assets/actor_102300_animation_0C86C_indices.inc"
};

AnimationSet Actor02300_D0C86C = {
    Actor02300_D0C194,
    Actor02300_D0C844,
    { NULL, Actor02300_D0BAA8, NULL, NULL, Actor02300_D0BBD4, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0C894[10] = {
#include "assets/actor_102300_animation_0CDE0_bank1.inc"
};

AnimationPackedRotation Actor02300_D0C90C[120] = {
#include "assets/actor_102300_animation_0CDE0_bank4.inc"
};

AnimationRecord Actor02300_D0CAEC[179] = {
#include "assets/actor_102300_animation_0CDE0_records.inc"
};

u16 Actor02300_D0CDB8[20] = {
#include "assets/actor_102300_animation_0CDE0_indices.inc"
};

AnimationSet Actor02300_D0CDE0 = {
    Actor02300_D0CAEC,
    Actor02300_D0CDB8,
    { NULL, Actor02300_D0C894, NULL, NULL, Actor02300_D0C90C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0CE08[27] = {
#include "assets/actor_102300_animation_0DCB0_bank1.inc"
};

AnimationPackedRotation Actor02300_D0CF4C[393] = {
#include "assets/actor_102300_animation_0DCB0_bank4.inc"
};

AnimationRecord Actor02300_D0D570[454] = {
#include "assets/actor_102300_animation_0DCB0_records.inc"
};

u16 Actor02300_D0DC88[20] = {
#include "assets/actor_102300_animation_0DCB0_indices.inc"
};

AnimationSet Actor02300_D0DCB0 = {
    Actor02300_D0D570,
    Actor02300_D0DC88,
    { NULL, Actor02300_D0CE08, NULL, NULL, Actor02300_D0CF4C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0DCD8[21] = {
#include "assets/actor_102300_animation_0E9E8_bank1.inc"
};

AnimationPackedRotation Actor02300_D0DDD4[354] = {
#include "assets/actor_102300_animation_0E9E8_bank4.inc"
};

AnimationRecord Actor02300_D0E35C[409] = {
#include "assets/actor_102300_animation_0E9E8_records.inc"
};

u16 Actor02300_D0E9C0[20] = {
#include "assets/actor_102300_animation_0E9E8_indices.inc"
};

AnimationSet Actor02300_D0E9E8 = {
    Actor02300_D0E35C,
    Actor02300_D0E9C0,
    { NULL, Actor02300_D0DCD8, NULL, NULL, Actor02300_D0DDD4, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0EA10[16] = {
#include "assets/actor_102300_animation_0F120_bank1.inc"
};

AnimationPackedRotation Actor02300_D0EAD0[177] = {
#include "assets/actor_102300_animation_0F120_bank4.inc"
};

AnimationRecord Actor02300_D0ED94[217] = {
#include "assets/actor_102300_animation_0F120_records.inc"
};

u16 Actor02300_D0F0F8[20] = {
#include "assets/actor_102300_animation_0F120_indices.inc"
};

AnimationSet Actor02300_D0F120 = {
    Actor02300_D0ED94,
    Actor02300_D0F0F8,
    { NULL, Actor02300_D0EA10, NULL, NULL, Actor02300_D0EAD0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0F148[3] = {
#include "assets/actor_102300_animation_0F328_bank1.inc"
};

AnimationPackedRotation Actor02300_D0F16C[25] = {
#include "assets/actor_102300_animation_0F328_bank4.inc"
};

AnimationRecord Actor02300_D0F1D0[76] = {
#include "assets/actor_102300_animation_0F328_records.inc"
};

u16 Actor02300_D0F300[20] = {
#include "assets/actor_102300_animation_0F328_indices.inc"
};

AnimationSet Actor02300_D0F328 = {
    Actor02300_D0F1D0,
    Actor02300_D0F300,
    { NULL, Actor02300_D0F148, NULL, NULL, Actor02300_D0F16C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0F350[9] = {
#include "assets/actor_102300_animation_0F934_bank1.inc"
};

AnimationPackedRotation Actor02300_D0F3BC[151] = {
#include "assets/actor_102300_animation_0F934_bank4.inc"
};

AnimationRecord Actor02300_D0F618[189] = {
#include "assets/actor_102300_animation_0F934_records.inc"
};

u16 Actor02300_D0F90C[20] = {
#include "assets/actor_102300_animation_0F934_indices.inc"
};

AnimationSet Actor02300_D0F934 = {
    Actor02300_D0F618,
    Actor02300_D0F90C,
    { NULL, Actor02300_D0F350, NULL, NULL, Actor02300_D0F3BC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0F95C[8] = {
#include "assets/actor_102300_animation_0FE58_bank1.inc"
};

AnimationPackedRotation Actor02300_D0F9BC[116] = {
#include "assets/actor_102300_animation_0FE58_bank4.inc"
};

AnimationRecord Actor02300_D0FB8C[169] = {
#include "assets/actor_102300_animation_0FE58_records.inc"
};

u16 Actor02300_D0FE30[20] = {
#include "assets/actor_102300_animation_0FE58_indices.inc"
};

AnimationSet Actor02300_D0FE58 = {
    Actor02300_D0FB8C,
    Actor02300_D0FE30,
    { NULL, Actor02300_D0F95C, NULL, NULL, Actor02300_D0F9BC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D0FE80[19] = {
#include "assets/actor_102300_animation_10868_bank1.inc"
};

AnimationPackedRotation Actor02300_D0FF64[255] = {
#include "assets/actor_102300_animation_10868_bank4.inc"
};

AnimationRecord Actor02300_D10360[312] = {
#include "assets/actor_102300_animation_10868_records.inc"
};

u16 Actor02300_D10840[20] = {
#include "assets/actor_102300_animation_10868_indices.inc"
};

AnimationSet Actor02300_D10868 = {
    Actor02300_D10360,
    Actor02300_D10840,
    { NULL, Actor02300_D0FE80, NULL, NULL, Actor02300_D0FF64, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D10890[18] = {
#include "assets/actor_102300_animation_11310_bank1.inc"
};

AnimationPackedRotation Actor02300_D10968[281] = {
#include "assets/actor_102300_animation_11310_bank4.inc"
};

AnimationRecord Actor02300_D10DCC[327] = {
#include "assets/actor_102300_animation_11310_records.inc"
};

u16 Actor02300_D112E8[20] = {
#include "assets/actor_102300_animation_11310_indices.inc"
};

AnimationSet Actor02300_D11310 = {
    Actor02300_D10DCC,
    Actor02300_D112E8,
    { NULL, Actor02300_D10890, NULL, NULL, Actor02300_D10968, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D11338[8] = {
#include "assets/actor_102300_animation_118F0_bank1.inc"
};

AnimationPackedRotation Actor02300_D11398[130] = {
#include "assets/actor_102300_animation_118F0_bank4.inc"
};

AnimationRecord Actor02300_D115A0[202] = {
#include "assets/actor_102300_animation_118F0_records.inc"
};

u16 Actor02300_D118C8[20] = {
#include "assets/actor_102300_animation_118F0_indices.inc"
};

AnimationSet Actor02300_D118F0 = {
    Actor02300_D115A0,
    Actor02300_D118C8,
    { NULL, Actor02300_D11338, NULL, NULL, Actor02300_D11398, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D11918[23] = {
#include "assets/actor_102300_animation_125E4_bank1.inc"
};

AnimationPackedRotation Actor02300_D11A2C[326] = {
#include "assets/actor_102300_animation_125E4_bank4.inc"
};

AnimationRecord Actor02300_D11F44[414] = {
#include "assets/actor_102300_animation_125E4_records.inc"
};

u16 Actor02300_D125BC[20] = {
#include "assets/actor_102300_animation_125E4_indices.inc"
};

AnimationSet Actor02300_D125E4 = {
    Actor02300_D11F44,
    Actor02300_D125BC,
    { NULL, Actor02300_D11918, NULL, NULL, Actor02300_D11A2C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D1260C[31] = {
#include "assets/actor_102300_animation_1365C_bank1.inc"
};

AnimationPackedRotation Actor02300_D12780[429] = {
#include "assets/actor_102300_animation_1365C_bank4.inc"
};

AnimationRecord Actor02300_D12E34[512] = {
#include "assets/actor_102300_animation_1365C_records.inc"
};

u16 Actor02300_D13634[20] = {
#include "assets/actor_102300_animation_1365C_indices.inc"
};

AnimationSet Actor02300_D1365C = {
    Actor02300_D12E34,
    Actor02300_D13634,
    { NULL, Actor02300_D1260C, NULL, NULL, Actor02300_D12780, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D13684[9] = {
#include "assets/actor_102300_animation_13B30_bank1.inc"
};

AnimationPackedRotation Actor02300_D136F0[113] = {
#include "assets/actor_102300_animation_13B30_bank4.inc"
};

AnimationRecord Actor02300_D138B4[149] = {
#include "assets/actor_102300_animation_13B30_records.inc"
};

u16 Actor02300_D13B08[20] = {
#include "assets/actor_102300_animation_13B30_indices.inc"
};

AnimationSet Actor02300_D13B30 = {
    Actor02300_D138B4,
    Actor02300_D13B08,
    { NULL, Actor02300_D13684, NULL, NULL, Actor02300_D136F0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D13B58[5] = {
#include "assets/actor_102300_animation_13E40_bank1.inc"
};

AnimationPackedRotation Actor02300_D13B94[65] = {
#include "assets/actor_102300_animation_13E40_bank4.inc"
};

AnimationRecord Actor02300_D13C98[96] = {
#include "assets/actor_102300_animation_13E40_records.inc"
};

u16 Actor02300_D13E18[20] = {
#include "assets/actor_102300_animation_13E40_indices.inc"
};

AnimationSet Actor02300_D13E40 = {
    Actor02300_D13C98,
    Actor02300_D13E18,
    { NULL, Actor02300_D13B58, NULL, NULL, Actor02300_D13B94, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D13E68[2] = {
#include "assets/actor_102300_animation_1401C_bank1.inc"
};

AnimationPackedRotation Actor02300_D13E80[17] = {
#include "assets/actor_102300_animation_1401C_bank4.inc"
};

AnimationRecord Actor02300_D13EC4[76] = {
#include "assets/actor_102300_animation_1401C_records.inc"
};

u16 Actor02300_D13FF4[20] = {
#include "assets/actor_102300_animation_1401C_indices.inc"
};

AnimationSet Actor02300_D1401C = {
    Actor02300_D13EC4,
    Actor02300_D13FF4,
    { NULL, Actor02300_D13E68, NULL, NULL, Actor02300_D13E80, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D14044[28] = {
#include "assets/actor_102300_animation_14F64_bank1.inc"
};

AnimationPackedRotation Actor02300_D14194[394] = {
#include "assets/actor_102300_animation_14F64_bank4.inc"
};

AnimationRecord Actor02300_D147BC[480] = {
#include "assets/actor_102300_animation_14F64_records.inc"
};

u16 Actor02300_D14F3C[20] = {
#include "assets/actor_102300_animation_14F64_indices.inc"
};

AnimationSet Actor02300_D14F64 = {
    Actor02300_D147BC,
    Actor02300_D14F3C,
    { NULL, Actor02300_D14044, NULL, NULL, Actor02300_D14194, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D14F8C[9] = {
#include "assets/actor_102300_animation_154F8_bank1.inc"
};

AnimationPackedRotation Actor02300_D14FF8[127] = {
#include "assets/actor_102300_animation_154F8_bank4.inc"
};

AnimationRecord Actor02300_D151F4[183] = {
#include "assets/actor_102300_animation_154F8_records.inc"
};

u16 Actor02300_D154D0[20] = {
#include "assets/actor_102300_animation_154F8_indices.inc"
};

AnimationSet Actor02300_D154F8 = {
    Actor02300_D151F4,
    Actor02300_D154D0,
    { NULL, Actor02300_D14F8C, NULL, NULL, Actor02300_D14FF8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D15520[5] = {
#include "assets/actor_102300_animation_157C0_bank1.inc"
};

AnimationPackedRotation Actor02300_D1555C[56] = {
#include "assets/actor_102300_animation_157C0_bank4.inc"
};

AnimationRecord Actor02300_D1563C[87] = {
#include "assets/actor_102300_animation_157C0_records.inc"
};

u16 Actor02300_D15798[20] = {
#include "assets/actor_102300_animation_157C0_indices.inc"
};

AnimationSet Actor02300_D157C0 = {
    Actor02300_D1563C,
    Actor02300_D15798,
    { NULL, Actor02300_D15520, NULL, NULL, Actor02300_D1555C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02300_D157E8[2] = {
#include "assets/actor_102300_animation_1599C_bank1.inc"
};

AnimationPackedRotation Actor02300_D15800[17] = {
#include "assets/actor_102300_animation_1599C_bank4.inc"
};

AnimationRecord Actor02300_D15844[76] = {
#include "assets/actor_102300_animation_1599C_records.inc"
};

u16 Actor02300_D15974[20] = {
#include "assets/actor_102300_animation_1599C_indices.inc"
};

AnimationSet Actor02300_D1599C = {
    Actor02300_D15844,
    Actor02300_D15974,
    { NULL, Actor02300_D157E8, NULL, NULL, Actor02300_D15800, NULL, NULL, NULL },
};

DamageAttack Actor02300_D159C4[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, 8 },
    { 15, 2 },
    { 5, 0 },
};

EnemyParams Actor02300_D159D8 = { Actor02300_D159C4, 482, 250, 400, 8, 0, 6, 0, 0 };

s16 Actor02300_D159E8[46] = {
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

s16 Actor02300_D15A44[56] = {
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

s32 gLungerVoiceCues[17] = {
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

Actor02300Storage7918 Actor02300_D15AF8 = { 0x40170007, { 0 } };

s32 gLungerScreamCue = 0x40170013;

s32 gLungerSilenceCue = 0x40170014;

s32 gLungerBurstCue = 0x40170015;

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
    { { { TASK_BODY_TMD, 96 } }, Actor02300_Fn03EE8, { .model = &Actor02300_D08E50 } },
    { { { TASK_BODY_TMD, 96 } }, Actor02300_Fn03BA8, { .model = &Actor02300_D09394 } },
    { { { TASK_BODY_TMD, 96 } }, Actor02300_Fn03CE8, { .model = &Actor02300_D096BC } },
};

AnimationSet* Actor02300_D15CBC[31] = {
    NULL,
    &Actor02300_D0A2F0,
    &Actor02300_D0AC58,
    &Actor02300_D0B280,
    &Actor02300_D0BA80,
    &Actor02300_D0C86C,
    &Actor02300_D0CDE0,
    &Actor02300_D0DCB0,
    &Actor02300_D0F120,
    &Actor02300_D0E9E8,
    &Actor02300_D0F328,
    &Actor02300_D0F934,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &Actor02300_D0FE58,
    &Actor02300_D10868,
    &Actor02300_D11310,
    &Actor02300_D118F0,
    &Actor02300_D125E4,
    &Actor02300_D1365C,
    &Actor02300_D13B30,
    &Actor02300_D13E40,
    &Actor02300_D1401C,
    &Actor02300_D14F64,
    &Actor02300_D154F8,
    &Actor02300_D157C0,
    &Actor02300_D1599C,
    NULL,
};

TaskFunc gLungerStates[15] = {
    lungerIdleState,
    lungerApproachState,
    Actor02300_Fn0327C,
    Actor02300_Fn01DF0,
    Actor02300_Fn02290,
    lungerSilenceScreamState,
    Actor02300_Fn03BA0,
    Actor02300_Fn03BA0,
    Actor02300_Fn03908,
    lungerRecoilState,
    Actor02300_Fn03A5C,
    Actor02300_Fn00E0C,
    lungerDownedShiftState,
    lungerCollapseState,
    lungerDownedFinishState,
};

static void Actor02300_Fn028AC(Enemy* enemy, Task* actor);

#include "../../shared/lunging_enemy_inlines.inc.c"

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void lungerTakeHits(Task* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor105600Work*       work;
    GpDeltaScratch*        head;
    Actor105600HitScratch* scratch;
    Enemy*                 enemy;
    GfxCoord*              self;
    GfxCoord*              other;
    GfxCoord*              part;
    s32                    i;
    s32                    x, y, z;
    s32                    damage;
    s32                    kind;
    s32                    dz;
    s32                    clamped;
    s32                    val;
    s32                    push;
    s16                    cooldown;
    u32                    rng;
    s32                    tilt;
    s32                    byte1;
    s32                    max;

    result  = 0;
    maxPush = 0;
    hit     = 0;
    lastId  = 0;
    work    = arg0->work;
    head    = SCRATCH_STACK_CURSOR(GpDeltaScratch);
    self    = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(Actor105600HitScratch);
    scratch = SCRATCH_STACK_CURSOR(Actor105600HitScratch);
    enemy   = (Enemy*)arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_584, head - 4, 4, NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-4].vx.halves.integer;
            self->coord.t[1] += scratch->delta.vy.halves.integer;
            self->coord.t[2] += scratch->delta.vz.halves.integer;
            break;
        case 2:
            self->coord.t[0] = work->field_678;
            self->coord.t[1] = work->field_67C;
            self->coord.t[2] = work->field_680;
            break;
    }
    Gp_ClearRec18Occupied(work->field_584);

    if (work->field_4CC.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->field_4EC, &scratch->delta, 5, NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.vx.halves.integer;
                self->coord.t[2] += scratch->delta.vz.halves.integer;
                break;
            case 2:
                self->coord.t[0] = work->field_678;
                self->coord.t[2] = work->field_680;
                break;
        }
    }

    if (work->field_69A != 0) {
        if (--work->field_69A <= 0) {
            work->field_69A = 0;
        }
    }

    for (i = 0; i < 5; i++) {
        switch ((u32)work->field_4EC[i].key.value >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_69A != 0) {
                    break;
                }
                other                  = gPlayerActorTasks[((u32)work->field_4EC[i].key.value >> 7) & 1]->extra.tmd->coords;
                scratch->delta.vx.word = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vy.word = other->coord.t[1] - self->coord.t[1];
                dz                     = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vz.word = dz;
                val                    = (scratch->delta.vx.word * self->coord.m[0][2]) + (scratch->delta.vy.word * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->field_6AA        = val >= 0;
                damage                 = Gp_ComputeDamage(work->field_4EC[i].key.value,
                                                          SquareRoot0((scratch->delta.vx.word * scratch->delta.vx.word) + (scratch->delta.vy.word * scratch->delta.vy.word) + (scratch->delta.vz.word * scratch->delta.vz.word)),
                                                          0, 0);
                kind                   = Gp_GetIdParam0(work->field_4EC[i].key.value);
                if (work->field_6CE != 0 && work->field_6AA == 1 && work->field_6B8 == 0) {
                    if (work->field_4EC[i].key.value & 0x8000) {
                        if (Actor02300_D15A44[work->field_4EC[i].key.value & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (Actor02300_D159E8[work->field_4EC[i].key.value & 0x7F] != 0) {
                        hit              = 1;
                        work->field_6D0 -= damage;
                    }
                    if (hit == 1) {
                        if (work->field_6D0 <= 0) {
                            work->field_6A6        = 9;
                            work->field_6CE        = 0;
                            work->field_6D2        = 1;
                            work->field_6A8        = 0;
                            work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            if (work->field_690 != NULL) {
                                work->field_690->task->state = 3;
                                work->field_690              = NULL;
                            }
                        }
                        func_800DA6E8(&enemy->node, 0, 0);
                        cooldown = Gp_GetIdParam2(work->field_4EC[i].key.value);
                        if (cooldown > 0) {
                            work->field_69A = cooldown;
                        }
                        break;
                    }
                } else {
                    work->field_6CE = 0;
                    if ((kind & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 2, NULL);
                    }
                }
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    if (work->field_6E0 == 0) {
                        result = 1;
                    }
                }
                if (work->field_6C4 != 0 && (work->field_4EC[i].key.value & 0x8000)) {
                    damage >>= 2;
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->field_4EC[i].key.value, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    if (work->field_6B8 == 0) {
                        result = 5;
                    } else {
                        result = 6;
                    }
                } else if (max = enemy->param->hpMax, enemy->hp < max / 4) {
                    if (work->field_6B8 == 0) {
                        result = 3;
                    } else {
                        result = 4;
                    }
                }
                if (work->field_6CC != 0 || work->field_6C2 != 0) {
                    work->field_6B6 += damage;
                }
                switch (kind & 0xFFFF) {
                    case 1:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3 && work->field_6E0 == 0) {
                            result = 2;
                        }
                        break;
                    case 2:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3) {
                            Gp_SetObjFlag2(enemy, work->field_4EC[i].key.value, 0);
                            result = 1;
                        }
                        break;
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                if (lastId != work->field_4EC[i].key.value) {
                    lastId             = work->field_4EC[i].key.value;
                    scratch->effOfs.vx = 0;
                    scratch->effOfs.vy = 0;
                    scratch->effOfs.vz = (work->field_6AA == 1) ? 0x12C : -0x96;
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[3],
                                  &scratch->effOfs, &work->field_670);
                }
                cooldown = Gp_GetIdParam2(work->field_4EC[i].key.value);
                if (cooldown > 0) {
                    work->field_69A = cooldown;
                }
                switch (result) {
                    case 0:
                        if (work->field_6A6 < 2) {
                            work->field_694 = 2;
                            work->field_6A6 = 2;
                            work->field_6A8 = 0;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        rng             = gRandomLcgState >> 16;
                        tilt            = (rng & 0x7F) + 0x40;
                        if (!(rng & 1)) {
                            tilt = -tilt;
                        }
                        work->field_688.vx = tilt;
                        byte1              = (s16)rng >> 8;
                        val                = (byte1 & 0x7F) + 0x40;
                        if (!(byte1 & 1)) {
                            val = -val;
                        }
                        work->field_688.vy = val;
                        work->field_6B4    = 1;
                        break;
                    case 1:
                        work->field_6A6        = 8;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 2:
                        work->field_6A6        = 9;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 3:
                        work->field_6A6        = 0xB;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 4:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xC;
                            work->field_6A8 = 0;
                        }
                        break;
                    case 5:
                        work->field_6A6        = 0xD;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 6:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xE;
                            work->field_6A8 = 0;
                        }
                        break;
                }
                if (result != 0 && work->field_690 != NULL) {
                    work->field_690->task->state = 3;
                    work->field_690              = NULL;
                }
                break;
            case 3:
                part                   = &arg0->extra.tmd->coords[3];
                x                      = part->workm.t[0] - work->field_4EC[i].point.vx;
                scratch->delta.vx.word = x;
                y                      = part->workm.t[1] - work->field_4EC[i].point.vy;
                scratch->delta.vy.word = y;
                z                      = part->workm.t[2] - work->field_4EC[i].point.vz;
                scratch->delta.vz.word = z;
                push                   = work->field_4EC[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped                = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->push);
                }
                break;
        }
    }

    if (maxPush > 0) {
        self->coord.t[0] += (maxPush * scratch->push.vx) >> 12;
        self->coord.t[2] += (maxPush * scratch->push.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_4EC);
    if (work->field_604[0].flags & 1) {
        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->field_604);
    }
    work->field_6B2 = 0;
    if (Gp_CountRec18Hi(work->field_4B4, 0x10000) != 0) {
        part               = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4];
        scratch->effOfs.vx = part->workm.t[0];
        scratch->effOfs.vy = part->workm.t[1];
        scratch->effOfs.vz = part->workm.t[2];
        scratch->target.vx = self->workm.t[0];
        scratch->target.vy = self->workm.t[1];
        scratch->target.vz = self->workm.t[2];
        if (detectSegmentHitsWall(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    SCRATCH_STACK_RELEASE_BYTES(0x40);
}

#include "../../shared/lunging_enemy_approach.inc.c"

#include "../../shared/lunging_enemy_proximity.inc.c"

/// Entry 0xB of the `field_6A6` state table `gLungerStates`. State 0 picks
/// the reaction from `field_6AA` (animation 0x16 into state 1, or 0x1A into
/// state 2), parks the second body node's pose and radius, raises its 0x4000
/// flag, drops the third node's, and clears the enemy's `reactionFlags`.
/// States 1 and 2 play the voice cues at their frame marks and, at the end of
/// the animation, settle on an idle (0x19 / 0x1D) with a fresh 6-bit dwell
/// from `gRandomLcgState`: into state 3 while the enemy has hit points left,
/// otherwise handing the task over to state 2. States 3 and 4 alternate
/// between the two idles until the dwell runs out.
void Actor02300_Fn00E0C(Task* arg0)
{
    s16              state;
    s16              nextAnim;
    s16              nextAnim2;
    s32              snd;
    s32              random3;
    s32              pan;
    s32              pan2;
    s32              pan3;
    u16              timer;
    u16              timer2;
    u32              random;
    u32              random2;
    Actor105600Work* work;
    GfxCoord*        self;

    work  = arg0->work;
    self  = arg0->extra.tmd->coords;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694        = 0x16;
                work->field_6A8        = 1;
                work->field_6B8        = 1;
                work->field_4CC.pos.vz = -0xA7;
            } else {
                work->field_694        = 0x1A;
                work->field_6A8        = 2;
                work->field_6B8        = 2;
                work->field_4CC.pos.vz = 0x109;
            }
            work->field_4CC.radius                           = 0x15E;
            work->field_69C                                  = 0;
            work->field_69E                                  = 0;
            work->field_6DE                                  = 1;
            work->field_4CC.flags                            = (u16)(work->field_4CC.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_564.flags                            = (u16)(work->field_564.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
            ((Enemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->field_6D4                                  = 1;
            break;
        case 1:
            if (work->field_698 == 0x14) {
                snd = gLungerVoiceCues[work->field_6D6 + 0xC] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = gLungerVoiceCues[work->field_6D6 + 8] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan2 = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 >= 0x42) {
                work->field_694 = 0x19;
                work->field_6D4 = 0;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_6AE = (u16)((random >> 0x10) & 0x3F);
                gRandomLcgState = random;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
                    work->field_6A8 = 3;
                } else {
                    arg0->state     = 2;
                    work->field_6A8 = 0;
                }
            }
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
                break;
            }
            break;
        case 2:
            if (work->field_698 == 0x19) {
                snd  = gLungerVoiceCues[work->field_6D6 + 8] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan3 = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 >= 0x31) {
                work->field_694 = 0x1D;
                work->field_6D4 = 0;
                random2         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_6AE = (u16)((random2 >> 0x10) & 0x3F);
                gRandomLcgState = random2;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
                    work->field_6A8 = 3;
                } else {
                    arg0->state     = 2;
                    work->field_6A8 = 0;
                }
            }
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            break;
        case 3:
            timer           = work->field_6AE - 1;
            work->field_6AE = timer;
            if ((s16)timer <= 0) {
                nextAnim = 0x1C;
                if (work->field_6B8 == 1) {
                    nextAnim = 0x18;
                }
                work->field_6AE = 0xAU;
                work->field_694 = nextAnim;
                work->field_6A8 = 4;
                break;
            }
            break;
        case 4:
            timer2          = work->field_6AE - 1;
            work->field_6AE = timer2;
            if ((s16)timer2 <= 0) {
                nextAnim2 = 0x1D;
                if (work->field_6B8 == 1) {
                    nextAnim2 = 0x19;
                }
                work->field_694 = nextAnim2;
                work->field_6A8 = 3;
                random3         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random3;
                work->field_6AE = (u16)(((u32)random3 >> 0x10) & 0x3F);
            }
            break;
    }
}

#include "../../shared/lunging_enemy_downed_shift.inc.c"

#include "../../shared/lunging_enemy_collapse.inc.c"

#include "../../shared/lunging_enemy_turn.inc.c"

#include "../../shared/lunging_enemy_hit_tilt.inc.c"

#include "../../shared/lunging_enemy_anim_cues.inc.c"

#include "../../shared/lunging_enemy_dead.inc.c"

/// Per-frame tick for the enemy's charge, sharing `field_6A8` with the rest of
/// the overlay and measuring the offset to the player through a 0x10-byte
/// scratch stack block. State 0 waits out the wind-up: from frame 0x47 it
/// mirrors `field_6D0` into `field_6CE` and commits to the charge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, zeroes
/// the cycle counter `field_6B6` on frame 0x46, and hands over to state 1 on
/// animation 6 once the animation is past its start frame plus 0x52. State 1
/// drives the charge, aiming `field_6A4` at the player each frame, switching to
/// state 2 on animation 7 within 1500 units and on animation 4 once the heading
/// has drifted more than 0x100 from `field_6A2`. State 2 raises the 0x5E4
/// node's 0x8000 flag and queues the cue on frame 0xD, drops the flag on frame
/// 0x1E, and from frame 0x3B picks animation 8 (back to state 0) within 3000
/// units or animation 4 otherwise. A `field_6B6` of 0x4C at any point aborts
/// the whole cycle back to animation 8.
void Actor02300_Fn01DF0(Task* arg0)
{
    s16              state;
    s16              diff;
    s16              turn;
    void**           scratch;
    s32              dx;
    s32              dz;
    s32              dxAim;
    s32              dzAim;
    s32              dxHold;
    s32              dzHold;
    s32              dist;
    s32              sound;
    s32              pan;
    u8*              head;
    Actor105600Work* work;
    GfxCoord*        self;
    VECTOR*          delta;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    delta                    = (VECTOR*)(head - 0x10);
    work                     = arg0->work;
    state                    = work->field_6A8;
    self                     = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if ((work->field_698 >= 0x47) && (work->field_6CE              = (s16)(work->field_6D0 > 0),
                                              ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]),
                                              dz                           = gPlayerStatus.coordMtx->t[2] - self->coord.t[2],
                                              delta->vz                    = dz,
                                              dx                           = ((VECTOR*)(head - 0x10))->vx,
                                              ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0;
            if (work->field_698 == 0x46) {
                work->field_6B6 = 0;
                work->field_6CC = 1;
            }
            if ((work->field_698 >= 0x47) && (work->field_6B6 >= 0x4C)) {
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
            }
            if (work->field_698 >= (gLungerAnimBlendFrames[work->field_694] + 0x52)) {
                work->field_6A8 = 1;
                work->field_694 = 6;
            }
            break;
        case 1:
            work->field_69C              = 0x84;
            work->field_69E              = 0;
            ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(gPlayerStatus.coordMtx->t[2] - self->coord.t[2]);
            work->field_6A4              = (s16)(ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF);
            dxAim                        = ((VECTOR*)(head - 0x10))->vx;
            dzAim                        = delta->vz;
            dist                         = SquareRoot0((dxAim * dxAim) + (dzAim * dzAim));
            if (work->field_6B6 >= 0x4C) {
                goto reset;
            }
            if (dist < 0x5DC) {
                work->field_6A8 = 2;
                work->field_694 = 7;
                work->field_69C = 0;
            } else {
                diff = (ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF) - work->field_6A2;
                turn = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (turn > 0x100) {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0;
            if ((work->field_698 < 0xD) && (work->field_6B6 >= 0x4C)) {
            reset:
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_6CE       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
            }
            if (work->field_698 == 0xD) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_5E4.key   = Gp_PackPair(Actor02300_D159C4, 1);
                sound                 = Actor02300_D15AF8.value | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan                   = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 == 0x1E) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            if (work->field_698 >= 0x3B) {
                work->field_6CE = 0;
                dxHold          = gPlayerStatus.coordMtx->t[0] - self->coord.t[0];
                delta->vx       = dxHold;
                dzHold          = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
                delta->vz       = dzHold;
                if (SquareRoot0((dxHold * dxHold) + (dzHold * dzHold)) < 0xBB8) {
                    work->field_6A6 = 4;
                    work->field_6A8 = 0;
                    work->field_694 = 8;
                } else {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                }
            }
            break;
    }
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Per-frame tick for the enemy's lunge cycle, sharing the `field_6A8` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte scratch stack block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, aims
/// `field_6A4` at them every frame, raises the 0x5E4 node's 0x8000 flag on
/// frame 0x20 and queues the cue on frame 0x21, then hands over to state 1 on
/// animation 9 once the animation is past frame 0x27. State 1 waits for frame
/// 0x5E and moves on to state 2 on animation 4.
void Actor02300_Fn02290(Task* arg0)
{
    s16              startFrame;
    s16              state;
    s16              frame;
    void**           scratch;
    s32              dz;
    s32              sound;
    s32              dx;
    s32              pan;
    u8*              head;
    Actor105600Work* work;
    GfxCoord*        self;
    VECTOR*          delta;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    delta                    = (VECTOR*)(head - 0x10);
    work                     = arg0->work;
    state                    = work->field_6A8;
    self                     = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
            dz                           = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = gLungerAnimBlendFrames[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (gLungerAnimBlendFrames[work->field_694] + 0x20)) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_5E4.key   = Gp_PackPair(Actor02300_D159C4, 0);
            }
            if (work->field_698 == (gLungerAnimBlendFrames[work->field_694] + 0x21)) {
                sound = Actor02300_D15AF8.value | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan   = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 >= (gLungerAnimBlendFrames[work->field_694] + 0x27)) {
                work->field_6A8       = 1;
                work->field_694       = 9;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x5E) {
                work->field_6A6 = 2;
                work->field_6A8 = 2;
                work->field_694 = 4;
            }
            break;
    }
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

#include "../../shared/lunging_enemy_silence_scream.inc.c"

/// Spawn/setup state for this enemy. Allocates the 0x6E4 work block, wires the
/// model object to the block's own light/colour matrices, primes the nineteen
/// animation slots, then spawns the two companion enemies from the overlay's
/// table (entries 2 and 1) and points each one's model at the texture page and
/// CLUT row its room's `AreaPlacement` names.
///
/// `Enemy::spawnState` then picks how the enemy starts: 0 builds the full
/// object set -- the four `WorldCollisionBody` nodes with their `WorldCollisionContact` tables, the voice
/// cue looked up per room in `Actor02300_D15C80`, and the coin-flip in
/// `field_6C4` drawn from `gRandomLcgState` -- while 1 and 2 only prime the
/// animation state and hand straight on to the next task state.
static void Actor02300_Fn028AC(Enemy* enemy, Task* actor)
{
    Actor105600Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        parts;
    GfxCoord*        partsA;
    GfxCoord*        partsB;
    GfxCoord*        partsC;
    GfxCoord*        effParts;
    Enemy*           eff;
    Enemy*           eff2;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    s32              i;
    s32              param;
    u32              lcg;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x17;
    work->field_66C            = Actor02300_D15C98;
    work->field_670.coord      = &actor->extra.tmd->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->rig.anim, Actor02300_D15CBC, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->rig.anim, i, 1);
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
            enemy->recs       = work->field_4EC;
            enemy->coord      = &parts[3];
            enemy->hp         = Actor02300_D159D8.hpMax;
            Gp_IncStateF0Ref(0);
            work->field_6AC = enemy->place->mode & 1;
            if (work->field_6AC == 0) {
                work->field_694 = 1;
                work->field_6A6 = 0;
            } else {
                work->field_694 = 2;
                work->field_6A6 = 1;
                param           = enemy->place->variant;
                work->field_6DA = param * 1000;
            }

            tbl = Actor02300_D15C80[gGameSession->location.loc.stage];
            if (tbl != NULL) {
                work->field_6D6 = tbl[gGameSession->location.loc.area];
            }
            if (work->field_6D6 != 0) {
                param1[3] = 0;
                param1[2] = 0xA;
                param1[0] = work->field_6D6;
                param2[0] = 0x17;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }

            work->field_6D0                 = 0xFA;
            work->field_49C.ends[0].vz      = 0x1F40;
            work->field_49C.end0Radius      = 0x3E8;
            work->field_49C.end1Radius      = 0x5DC;
            work->field_49C.ends[0].vx      = 0;
            work->field_49C.ends[0].vy      = 0;
            work->field_49C.ends[1].vx      = 0;
            work->field_49C.ends[1].vy      = 0;
            work->field_49C.ends[1].vz      = 0;
            work->field_49C.contacts        = work->field_4B4;
            lcg                             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->field_6C4                 = ((lcg >> 16) & 1) + 1;
            gRandomLcgState                 = lcg;
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
            work->field_4CC.key              = 0x30017;
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

            effParts                         = eff2->task->extra.tmd->coords;
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
            actor->state           = 1;
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

#include "../../shared/lunging_enemy_frame.inc.c"

/// The lunge's own tick, run out of a 0x10-byte scratch stack block. State
/// 0 is the wind-up: it holds `field_69C` at 0 until the animation reaches its
/// start frame, aims `field_6A4` at the player and compares it with the
/// enemy's own facing `field_6A2` - past 0x581 apart it gives up and turns
/// (animation 3, or animation 4 when `field_6DC` says it has already turned
/// once), within 0x80 it raises the body node's 0xC000 flags and commits as
/// soon as `field_6B2` reports contact. State 1 picks what to do next: inside
/// 0x8CA of the player it lunges (animation 8), otherwise it draws from
/// `gRandomLcgState` through a mask that widens by a bit each cycle and either
/// circles (animation 0xA) or walks in (animation 5). State 2 waits out the
/// recovery and state 3 the turn.
void Actor02300_Fn0327C(Task* actor)
{
    s16              yaw;
    s16              yaw2;
    s16              state;
    s16              deltaYaw;
    s16              deltaYaw2;
    s16              speed;
    s32              magnitude;
    s32              magnitude2;
    s16              wrapped;
    s16              wrapped2;
    s16              angle;
    s32              dx;
    s32              dz;
    u32              random;
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor105600Work* work;
    GfxCoord*        coord;

    head                     = SCRATCH_STACK_CURSOR(u8);
    delta                    = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)delta;
    work                     = actor->work;
    state                    = work->field_6A8;
    coord                    = actor->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= gLungerAnimBlendFrames[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = (u16)work->field_6A4 - yaw;
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
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                if (!((random >> 0x10) & ((1 << (work->field_6C0 + 1)) - 1)) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_SILENCE) &&
                    work->field_6C4 != 0) {
                    work->field_6A6 = 5;
                    work->field_6A8 = 0;
                    work->field_694 = 0xA;
                    work->field_6AE = 0;
                    work->field_6C2 = 1;
                    work->field_6B6 = 0;
                    work->field_6C0++;
                } else {
                    work->field_6A6 = 3;
                    work->field_6A8 = 0;
                    work->field_694 = 5;
                    work->field_6AE = 0;
                }
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
                deltaYaw2       = (u16)work->field_6A4 - yaw2;
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

#include "../../shared/lunging_enemy_idle.inc.c"

/// Entry 8 of the `field_6A6` state table `gLungerStates`: step 0 starts
/// animation 0x11 and clears both dwell counters; step 1 waits for frame 0x37,
/// then parks on animation 2 (entry 2) or, with `field_6E0` set, on animation
/// 0x14 (entry 0xA).
void Actor02300_Fn03908(Task* arg0)
{
    Actor105600Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}

#include "../../shared/lunging_enemy_recoil.inc.c"

/// Entry 0xA of the `field_6A6` table: step 0 waits for `Gp_TickObjFlag2` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `field_6E0`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void Actor02300_Fn03A5C(Task* arg0)
{
    Actor105600Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}

#include "../../shared/lunging_enemy_downed_finish.inc.c"

void Actor02300_Fn03BA0(Task* task)
{
}

/// State handlers of the child task hung off part 7 of the enemy's model -
/// spawn/setup, per-frame tick and teardown - dispatched through by
/// `Actor02300_Fn03BA8`.
static const GpEnemyTaskFuncTable3 Actor02300_D00060 = {
    Actor02300_Fn03C04,
    Actor02300_Fn03C50,
    Gp_DestroyEnemy,
};

void Actor02300_Fn03BA8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00060;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Spawn state of the child task driven by `Actor02300_Fn03BA8`: parents the
/// child's root coordinate to part 7 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and seeds the enemy's
/// `field_6D8` countdown the child's tick drains, then advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
static void Actor02300_Fn03C04(Enemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105600Work* work;
    GfxCoord*        coord;
    GfxCoord*        parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor105600Work*)parent->work;

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = &parentCoords[7];
    obj->lightMtx       = &work->field_45C;
    obj->colorMtx       = &work->field_43C;
    obj->flags          = 0;
    task->state         = 1;
    work->field_6D8     = 0xA;
}

/// Per-frame state of the child task `Actor02300_Fn03C04` sets up. It mirrors
/// the enemy's model flags onto its own model and drains the enemy's
/// `field_6D8` countdown; on the frame it reaches zero it spawns a
/// `Gp_SpawnEff` effect at part 7 of the enemy's coordinate array and
/// reparents the effect's task to this child. `arg0` is the spawn context
/// every state handler takes and is unused here.
static void Actor02300_Fn03C50(Enemy* arg0, Task* task)
{
    EffectWork*      effect;
    Task*            parent;
    Actor105600Work* work;
    s16              count;

    parent                 = task->parent;
    work                   = (Actor105600Work*)parent->work;
    task->extra.tmd->flags = (u16)parent->extra.tmd->flags;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &task->parent->extra.tmd->coords[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->task);
            }
        }
    }
}

/// State handlers of the child task hung off part 11 of the enemy's model -
/// spawn/setup, per-frame tick and teardown - dispatched through by
/// `Actor02300_Fn03CE8`.
static const GpEnemyTaskFuncTable3 Actor02300_D0006C = {
    Actor02300_Fn03D44,
    lungerBurstPartTick,
    Gp_DestroyEnemy,
};

void Actor02300_Fn03CE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D0006C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Spawn state of the child task driven by `Actor02300_Fn03CE8`: parents the
/// child's root coordinate to part 11 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
static void Actor02300_Fn03D44(Enemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105600Work* work;
    GfxCoord*        coord;
    GfxCoord*        parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor105600Work*)parent->work;

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = &parentCoords[11];
    obj->lightMtx       = &work->field_45C;
    obj->flags          = 0;
    obj->colorMtx       = &work->field_43C;
    task->state         = 1;
}

/// The enemy's own state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor02300_Fn03EE8`. Each takes the task
/// as the enemy view it is.
static const GpEnemyTaskFuncTable3 Actor02300_D00078 = {
    Actor02300_Fn028AC,
    lungerFrameState,
    lungerDeadState,
};

#include "../../shared/lunging_enemy_burst_part.inc.c"

void Actor02300_Fn03EE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00078;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
