#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_80131fc8.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_lighting.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/frame_capture.h"
#define GOLEM_KNIGHT_BISHOP_KIND GOLEM_BISHOP
#include "../../shared/golem_knight_bishop.h"

extern GolemKnightBishopFrameStep gGolemKnightBishopFrameSteps[];

/// Per-roll wait lengths the wait state of `golemKnightBishopIdleSeq`
/// scales by `16 - field_70C`, indexed by a 4-bit `gRandomLcgState` draw.
extern s16 gGolemKnightBishopIdleWaits[];

/// Per-roll state offsets the wait state adds to 2 when `field_6E8` is set.
extern u16 gGolemKnightBishopIdleSteps[];

/// Cue word the countdown's expiry queues, a separate `D_` symbol in the
/// overlay's data.
extern s32 gGolemKnightBishopPainCue;

/// Cue word the approach's state 0 queues as it plants the actor on its box.
extern s32 gGolemKnightBishopApproachCue;

/// Cue word the fade-out in `golemKnightBishopTranslucencyFade` queues.
extern s32 gGolemKnightBishopFadeCue;

/// Cue word the approach's frame 0x12 queues.
extern s32 gGolemKnightBishopStrikeCue;

/// Base id of the actor's vocal cue: the `Enemy` work id's high nibble is
/// OR'd in as bits 8-11 of the cue id.
extern s32 gGolemKnightBishopHoldCue;

/// Cue-id table: `GolemKnightBishopWork::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `flags` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one; the branch sequences read `[field_712 + 8]`.
extern s32 gGolemKnightBishopAnimCues[];

/// Weighted 16-entry roll for `GolemKnightBishopWork::field_6E4`: indices 0-10 hold 0
/// and 11-15 hold 1, so the short approach is taken about a third of the time.
extern u16 gGolemKnightBishopApproachRoll[];

/// Per-animation-id value `golemKnightBishopTickAnim` hands `animationSeekSlotWithBlend`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 gGolemKnightBishopAnimBlend[];

/// The spawn's tables: the task's next handler record, the `DamageAttack`
/// `Gp_PackPair` packs into the third collision object, the `EnemyParams` whose
/// `hpMax` seeds the enemy's HP, the stage / room box-table index run, the
/// box tables it selects, the per-stage cue-bank arrays and the animation data.
// Message-table callbacks use the argument views required by this TU.
STATIC_ASSERT_SIZEOF(GolemKnightBishopMessageEntry, 8);

extern GolemKnightBishopMessageEntry gGolemKnightBishopMessages[2];
extern DamageAttack                  gGolemKnightBishopAttacks[4];
extern EnemyParams                   gGolemKnightBishopParams;
extern GolemKnightBishopSpot         gGolemKnightBishopSpots[];
extern GolemKnightBishopRegion*      gGolemKnightBishopRegions[];
extern s16*                          gGolemKnightBishopStageCues[];
extern AnimationSet*                 gGolemKnightBishopAnimSets[22];

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 gGolemKnightBishopGrabHpLimits[];

/// Animation block the grab's 0x3FF messages hand the player.
extern AnimationSet* gGolemKnightBishopPlayerAnims[5];

/// `func_800FDB18` argument record for the grab's finishing spark.
extern EffectSpawnArg gGolemKnightBishopGrabEffect;

/// The two four-vertex index rows the trail's shaded quads take their corners
/// from, into the scratch block's six-entry x / y runs.
extern s16 gGolemKnightBishopBeamQuadCorners[2][4];

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

s32 func_actor_403900_801381E4(Task*);

extern AnimationSet D_actor_403900_8013E26C;
extern AnimationSet D_actor_403900_8013E724;
extern AnimationSet D_actor_403900_8013EF30;
extern AnimationSet D_actor_403900_8013FB68;
extern AnimationSet D_actor_403900_80140C1C;
extern AnimationSet D_actor_403900_801417C8;
extern AnimationSet D_actor_403900_80145840;
extern AnimationSet D_actor_403900_80145D64;
extern AnimationSet D_actor_403900_801467AC;
extern AnimationSet D_actor_403900_8014749C;
extern AnimationSet D_actor_403900_80148DD8;
extern AnimationSet D_actor_403900_80149BE8;
extern AnimationSet D_actor_403900_8014AC60;
extern AnimationSet D_actor_403900_8014B134;
extern AnimationSet D_actor_403900_8014B444;
extern AnimationSet D_actor_403900_8014B620;
extern AnimationSet D_actor_403900_8014C568;
extern AnimationSet D_actor_403900_8014CAFC;
extern AnimationSet D_actor_403900_8014CDC4;
extern AnimationSet D_actor_403900_8014CFA0;
extern AnimationSet D_actor_403900_801505C8;
extern AnimationSet D_actor_403900_80151998;
extern AnimationSet D_actor_403900_80151F2C;
extern AnimationSet D_actor_403900_8015298C;
extern AnimationSet D_actor_403900_80153BC8;
extern DamageAttack gGolemKnightBishopAttacks[4];
extern TmdSource    D_actor_403900_8013DBD8;
static void         func_actor_403900_80138344(Task*);

GolemKnightBishopMessageEntry gGolemKnightBishopMessages[2] = {
    { 2014, { .call0 = func_actor_403900_801381E4 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

s16 gGolemKnightBishopAnimBlend[22] = {
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    0,
    1,
    1,
    1,
    8,
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
};

GolemKnightBishopFrameStep gGolemKnightBishopFrameSteps[18] = {
    { 3, 0xFFF6 },
    { 5, 350 },
    { 6, 210 },
    { 9, 117 },
    { 11, 130 },
    { 15, 85 },
    { 17, 90 },
    { 21, 35 },
    { 22, 0xFFCE },
    { 23, 0xFFB0 },
    { 31, 0 },
    { 37, 2 },
    { 43, 5 },
    { 62, 8 },
    { 64, 0xFFFB },
    { 69, 0xFFFA },
    { 84, 0xFFEF },
    { 109, 0xFFFA },
};

s32 gGolemKnightBishopAnimCues[17] = {
    0,
    0x40160001,
    0x40160002,
    0x40160003,
    0x40160004,
    0x40160005,
    0x40160006,
    0x40160012,
    0x40160013,
    0x40160009,
    0x4016000A,
    0x4016000B,
    0x40160014,
    0,
    0,
    0,
    0,
};

s32 gGolemKnightBishopApproachCue = 0x40160007;

s32 gGolemKnightBishopPainCue = 0x40160008;

s32 gGolemKnightBishopFadeCue = 0x4016000F;

s32 gGolemKnightBishopStrikeCue = 0x40160010;

s32 gGolemKnightBishopHoldCue = 0x40160011;

TmdBone D_actor_403900_8013847C[19] = {
#include "assets/golem_body_skeleton.inc"
};

u32 D_actor_403900_80138728[19] = {
#include "assets/golem_body_partVerts.inc"
};

SVECTOR D_actor_403900_80138774[363] = {
#include "assets/golem_body_verts.inc"
};

SVECTOR D_actor_403900_801392CC[345] = {
#include "assets/golem_body_normals.inc"
};

u32 D_actor_403900_80139D94[3985] = {
#include "assets/golem_body_stream.inc"
};

TmdSource D_actor_403900_8013DBD8 = {
    0,
    43904,
    11888,
    19,
    D_actor_403900_80138728,
    D_actor_403900_80138774,
    D_actor_403900_801392CC,
    D_actor_403900_8013847C,
    D_actor_403900_80139D94,
};

AnimationPackedPose D_actor_403900_8013DBFC[12] = {
#include "assets/actor_403900_animation_0C44C_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8013DC8C[165] = {
#include "assets/actor_403900_animation_0C44C_bank4.inc"
};

AnimationRecord D_actor_403900_8013DF20[201] = {
#include "assets/actor_403900_animation_0C44C_records.inc"
};

u16 D_actor_403900_8013E244[20] = {
#include "assets/actor_403900_animation_0C44C_indices.inc"
};

AnimationSet D_actor_403900_8013E26C = {
    D_actor_403900_8013DF20,
    D_actor_403900_8013E244,
    { NULL, D_actor_403900_8013DBFC, NULL, NULL, D_actor_403900_8013DC8C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8013E294[7] = {
#include "assets/actor_403900_animation_0C904_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8013E2E8[87] = {
#include "assets/actor_403900_animation_0C904_bank4.inc"
};

AnimationRecord D_actor_403900_8013E444[174] = {
#include "assets/actor_403900_animation_0C904_records.inc"
};

u16 D_actor_403900_8013E6FC[20] = {
#include "assets/actor_403900_animation_0C904_indices.inc"
};

AnimationSet D_actor_403900_8013E724 = {
    D_actor_403900_8013E444,
    D_actor_403900_8013E6FC,
    { NULL, D_actor_403900_8013E294, NULL, NULL, D_actor_403900_8013E2E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8013E74C[15] = {
#include "assets/actor_403900_animation_0D110_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8013E800[197] = {
#include "assets/actor_403900_animation_0D110_bank4.inc"
};

AnimationRecord D_actor_403900_8013EB14[253] = {
#include "assets/actor_403900_animation_0D110_records.inc"
};

u16 D_actor_403900_8013EF08[20] = {
#include "assets/actor_403900_animation_0D110_indices.inc"
};

AnimationSet D_actor_403900_8013EF30 = {
    D_actor_403900_8013EB14,
    D_actor_403900_8013EF08,
    { NULL, D_actor_403900_8013E74C, NULL, NULL, D_actor_403900_8013E800, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8013EF58[21] = {
#include "assets/actor_403900_animation_0DD48_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8013F054[317] = {
#include "assets/actor_403900_animation_0DD48_bank4.inc"
};

AnimationRecord D_actor_403900_8013F548[382] = {
#include "assets/actor_403900_animation_0DD48_records.inc"
};

u16 D_actor_403900_8013FB40[20] = {
#include "assets/actor_403900_animation_0DD48_indices.inc"
};

AnimationSet D_actor_403900_8013FB68 = {
    D_actor_403900_8013F548,
    D_actor_403900_8013FB40,
    { NULL, D_actor_403900_8013EF58, NULL, NULL, D_actor_403900_8013F054, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8013FB90[29] = {
#include "assets/actor_403900_animation_0EDFC_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8013FCEC[451] = {
#include "assets/actor_403900_animation_0EDFC_bank4.inc"
};

AnimationRecord D_actor_403900_801403F8[511] = {
#include "assets/actor_403900_animation_0EDFC_records.inc"
};

u16 D_actor_403900_80140BF4[20] = {
#include "assets/actor_403900_animation_0EDFC_indices.inc"
};

AnimationSet D_actor_403900_80140C1C = {
    D_actor_403900_801403F8,
    D_actor_403900_80140BF4,
    { NULL, D_actor_403900_8013FB90, NULL, NULL, D_actor_403900_8013FCEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_80140C44[30] = {
#include "assets/actor_403900_animation_0F9A8_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80140DAC[277] = {
#include "assets/actor_403900_animation_0F9A8_bank4.inc"
};

AnimationRecord D_actor_403900_80141200[360] = {
#include "assets/actor_403900_animation_0F9A8_records.inc"
};

u16 D_actor_403900_801417A0[20] = {
#include "assets/actor_403900_animation_0F9A8_indices.inc"
};

AnimationSet D_actor_403900_801417C8 = {
    D_actor_403900_80141200,
    D_actor_403900_801417A0,
    { NULL, D_actor_403900_80140C44, NULL, NULL, D_actor_403900_80140DAC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_801417F0[111] = {
#include "assets/actor_403900_animation_13A20_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80141D24[1789] = {
#include "assets/actor_403900_animation_13A20_bank4.inc"
};

AnimationRecord D_actor_403900_80143918[1984] = {
#include "assets/actor_403900_animation_13A20_records.inc"
};

u16 D_actor_403900_80145818[20] = {
#include "assets/actor_403900_animation_13A20_indices.inc"
};

AnimationSet D_actor_403900_80145840 = {
    D_actor_403900_80143918,
    D_actor_403900_80145818,
    { NULL, D_actor_403900_801417F0, NULL, NULL, D_actor_403900_80141D24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_80145868[8] = {
#include "assets/actor_403900_animation_13F44_bank1.inc"
};

AnimationPackedRotation D_actor_403900_801458C8[116] = {
#include "assets/actor_403900_animation_13F44_bank4.inc"
};

AnimationRecord D_actor_403900_80145A98[169] = {
#include "assets/actor_403900_animation_13F44_records.inc"
};

u16 D_actor_403900_80145D3C[20] = {
#include "assets/actor_403900_animation_13F44_indices.inc"
};

AnimationSet D_actor_403900_80145D64 = {
    D_actor_403900_80145A98,
    D_actor_403900_80145D3C,
    { NULL, D_actor_403900_80145868, NULL, NULL, D_actor_403900_801458C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_80145D8C[19] = {
#include "assets/actor_403900_animation_1498C_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80145E70[254] = {
#include "assets/actor_403900_animation_1498C_bank4.inc"
};

AnimationRecord D_actor_403900_80146268[327] = {
#include "assets/actor_403900_animation_1498C_records.inc"
};

u16 D_actor_403900_80146784[20] = {
#include "assets/actor_403900_animation_1498C_indices.inc"
};

AnimationSet D_actor_403900_801467AC = {
    D_actor_403900_80146268,
    D_actor_403900_80146784,
    { NULL, D_actor_403900_80145D8C, NULL, NULL, D_actor_403900_80145E70, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_801467D4[24] = {
#include "assets/actor_403900_animation_1567C_bank1.inc"
};

AnimationPackedRotation D_actor_403900_801468F4[343] = {
#include "assets/actor_403900_animation_1567C_bank4.inc"
};

AnimationRecord D_actor_403900_80146E50[393] = {
#include "assets/actor_403900_animation_1567C_records.inc"
};

u16 D_actor_403900_80147474[20] = {
#include "assets/actor_403900_animation_1567C_indices.inc"
};

AnimationSet D_actor_403900_8014749C = {
    D_actor_403900_80146E50,
    D_actor_403900_80147474,
    { NULL, D_actor_403900_801467D4, NULL, NULL, D_actor_403900_801468F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_801474C4[53] = {
#include "assets/actor_403900_animation_16FB8_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80147740[660] = {
#include "assets/actor_403900_animation_16FB8_bank4.inc"
};

AnimationRecord D_actor_403900_80148190[776] = {
#include "assets/actor_403900_animation_16FB8_records.inc"
};

u16 D_actor_403900_80148DB0[20] = {
#include "assets/actor_403900_animation_16FB8_indices.inc"
};

AnimationSet D_actor_403900_80148DD8 = {
    D_actor_403900_80148190,
    D_actor_403900_80148DB0,
    { NULL, D_actor_403900_801474C4, NULL, NULL, D_actor_403900_80147740, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_80148E00[26] = {
#include "assets/actor_403900_animation_17DC8_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80148F38[365] = {
#include "assets/actor_403900_animation_17DC8_bank4.inc"
};

AnimationRecord D_actor_403900_801494EC[437] = {
#include "assets/actor_403900_animation_17DC8_records.inc"
};

u16 D_actor_403900_80149BC0[20] = {
#include "assets/actor_403900_animation_17DC8_indices.inc"
};

AnimationSet D_actor_403900_80149BE8 = {
    D_actor_403900_801494EC,
    D_actor_403900_80149BC0,
    { NULL, D_actor_403900_80148E00, NULL, NULL, D_actor_403900_80148F38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_80149C10[31] = {
#include "assets/actor_403900_animation_18E40_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80149D84[429] = {
#include "assets/actor_403900_animation_18E40_bank4.inc"
};

AnimationRecord D_actor_403900_8014A438[512] = {
#include "assets/actor_403900_animation_18E40_records.inc"
};

u16 D_actor_403900_8014AC38[20] = {
#include "assets/actor_403900_animation_18E40_indices.inc"
};

AnimationSet D_actor_403900_8014AC60 = {
    D_actor_403900_8014A438,
    D_actor_403900_8014AC38,
    { NULL, D_actor_403900_80149C10, NULL, NULL, D_actor_403900_80149D84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014AC88[9] = {
#include "assets/actor_403900_animation_19314_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014ACF4[113] = {
#include "assets/actor_403900_animation_19314_bank4.inc"
};

AnimationRecord D_actor_403900_8014AEB8[149] = {
#include "assets/actor_403900_animation_19314_records.inc"
};

u16 D_actor_403900_8014B10C[20] = {
#include "assets/actor_403900_animation_19314_indices.inc"
};

AnimationSet D_actor_403900_8014B134 = {
    D_actor_403900_8014AEB8,
    D_actor_403900_8014B10C,
    { NULL, D_actor_403900_8014AC88, NULL, NULL, D_actor_403900_8014ACF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014B15C[5] = {
#include "assets/actor_403900_animation_19624_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014B198[65] = {
#include "assets/actor_403900_animation_19624_bank4.inc"
};

AnimationRecord D_actor_403900_8014B29C[96] = {
#include "assets/actor_403900_animation_19624_records.inc"
};

u16 D_actor_403900_8014B41C[20] = {
#include "assets/actor_403900_animation_19624_indices.inc"
};

AnimationSet D_actor_403900_8014B444 = {
    D_actor_403900_8014B29C,
    D_actor_403900_8014B41C,
    { NULL, D_actor_403900_8014B15C, NULL, NULL, D_actor_403900_8014B198, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014B46C[2] = {
#include "assets/actor_403900_animation_19800_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014B484[17] = {
#include "assets/actor_403900_animation_19800_bank4.inc"
};

AnimationRecord D_actor_403900_8014B4C8[76] = {
#include "assets/actor_403900_animation_19800_records.inc"
};

u16 D_actor_403900_8014B5F8[20] = {
#include "assets/actor_403900_animation_19800_indices.inc"
};

AnimationSet D_actor_403900_8014B620 = {
    D_actor_403900_8014B4C8,
    D_actor_403900_8014B5F8,
    { NULL, D_actor_403900_8014B46C, NULL, NULL, D_actor_403900_8014B484, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014B648[28] = {
#include "assets/actor_403900_animation_1A748_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014B798[394] = {
#include "assets/actor_403900_animation_1A748_bank4.inc"
};

AnimationRecord D_actor_403900_8014BDC0[480] = {
#include "assets/actor_403900_animation_1A748_records.inc"
};

u16 D_actor_403900_8014C540[20] = {
#include "assets/actor_403900_animation_1A748_indices.inc"
};

AnimationSet D_actor_403900_8014C568 = {
    D_actor_403900_8014BDC0,
    D_actor_403900_8014C540,
    { NULL, D_actor_403900_8014B648, NULL, NULL, D_actor_403900_8014B798, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014C590[9] = {
#include "assets/actor_403900_animation_1ACDC_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014C5FC[127] = {
#include "assets/actor_403900_animation_1ACDC_bank4.inc"
};

AnimationRecord D_actor_403900_8014C7F8[183] = {
#include "assets/actor_403900_animation_1ACDC_records.inc"
};

u16 D_actor_403900_8014CAD4[20] = {
#include "assets/actor_403900_animation_1ACDC_indices.inc"
};

AnimationSet D_actor_403900_8014CAFC = {
    D_actor_403900_8014C7F8,
    D_actor_403900_8014CAD4,
    { NULL, D_actor_403900_8014C590, NULL, NULL, D_actor_403900_8014C5FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014CB24[5] = {
#include "assets/actor_403900_animation_1AFA4_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014CB60[56] = {
#include "assets/actor_403900_animation_1AFA4_bank4.inc"
};

AnimationRecord D_actor_403900_8014CC40[87] = {
#include "assets/actor_403900_animation_1AFA4_records.inc"
};

u16 D_actor_403900_8014CD9C[20] = {
#include "assets/actor_403900_animation_1AFA4_indices.inc"
};

AnimationSet D_actor_403900_8014CDC4 = {
    D_actor_403900_8014CC40,
    D_actor_403900_8014CD9C,
    { NULL, D_actor_403900_8014CB24, NULL, NULL, D_actor_403900_8014CB60, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014CDEC[2] = {
#include "assets/actor_403900_animation_1B180_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014CE04[17] = {
#include "assets/actor_403900_animation_1B180_bank4.inc"
};

AnimationRecord D_actor_403900_8014CE48[76] = {
#include "assets/actor_403900_animation_1B180_records.inc"
};

u16 D_actor_403900_8014CF78[20] = {
#include "assets/actor_403900_animation_1B180_indices.inc"
};

AnimationSet D_actor_403900_8014CFA0 = {
    D_actor_403900_8014CE48,
    D_actor_403900_8014CF78,
    { NULL, D_actor_403900_8014CDEC, NULL, NULL, D_actor_403900_8014CE04, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_8014CFC8[100] = {
#include "assets/actor_403900_animation_1E7A8_bank1.inc"
};

AnimationPackedRotation D_actor_403900_8014D478[1463] = {
#include "assets/actor_403900_animation_1E7A8_bank4.inc"
};

AnimationRecord D_actor_403900_8014EB54[1683] = {
#include "assets/actor_403900_animation_1E7A8_records.inc"
};

u16 D_actor_403900_801505A0[20] = {
#include "assets/actor_403900_animation_1E7A8_indices.inc"
};

AnimationSet D_actor_403900_801505C8 = {
    D_actor_403900_8014EB54,
    D_actor_403900_801505A0,
    { NULL, D_actor_403900_8014CFC8, NULL, NULL, D_actor_403900_8014D478, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_801505F0[39] = {
#include "assets/actor_403900_animation_1FB78_bank1.inc"
};

AnimationPackedRotation D_actor_403900_801507C4[525] = {
#include "assets/actor_403900_animation_1FB78_bank4.inc"
};

AnimationRecord D_actor_403900_80150FF8[606] = {
#include "assets/actor_403900_animation_1FB78_records.inc"
};

u16 D_actor_403900_80151970[20] = {
#include "assets/actor_403900_animation_1FB78_indices.inc"
};

AnimationSet D_actor_403900_80151998 = {
    D_actor_403900_80150FF8,
    D_actor_403900_80151970,
    { NULL, D_actor_403900_801505F0, NULL, NULL, D_actor_403900_801507C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_801519C0[10] = {
#include "assets/actor_403900_animation_2010C_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80151A38[126] = {
#include "assets/actor_403900_animation_2010C_bank4.inc"
};

AnimationRecord D_actor_403900_80151C30[181] = {
#include "assets/actor_403900_animation_2010C_records.inc"
};

u16 D_actor_403900_80151F04[20] = {
#include "assets/actor_403900_animation_2010C_indices.inc"
};

AnimationSet D_actor_403900_80151F2C = {
    D_actor_403900_80151C30,
    D_actor_403900_80151F04,
    { NULL, D_actor_403900_801519C0, NULL, NULL, D_actor_403900_80151A38, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_80151F54[21] = {
#include "assets/actor_403900_animation_20B6C_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80152050[261] = {
#include "assets/actor_403900_animation_20B6C_bank4.inc"
};

AnimationRecord D_actor_403900_80152464[320] = {
#include "assets/actor_403900_animation_20B6C_records.inc"
};

u16 D_actor_403900_80152964[20] = {
#include "assets/actor_403900_animation_20B6C_indices.inc"
};

AnimationSet D_actor_403900_8015298C = {
    D_actor_403900_80152464,
    D_actor_403900_80152964,
    { NULL, D_actor_403900_80151F54, NULL, NULL, D_actor_403900_80152050, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403900_801529B4[35] = {
#include "assets/actor_403900_animation_21DA8_bank1.inc"
};

AnimationPackedRotation D_actor_403900_80152B58[485] = {
#include "assets/actor_403900_animation_21DA8_bank4.inc"
};

AnimationRecord D_actor_403900_801532EC[557] = {
#include "assets/actor_403900_animation_21DA8_records.inc"
};

u16 D_actor_403900_80153BA0[20] = {
#include "assets/actor_403900_animation_21DA8_indices.inc"
};

AnimationSet D_actor_403900_80153BC8 = {
    D_actor_403900_801532EC,
    D_actor_403900_80153BA0,
    { NULL, D_actor_403900_801529B4, NULL, NULL, D_actor_403900_80152B58, NULL, NULL, NULL },
};

DamageAttack gGolemKnightBishopAttacks[4] = { { 10, 10 }, { 45, 2 }, { 58, 2 }, { 999, 0 } };

EnemyParams gGolemKnightBishopParams = { gGolemKnightBishopAttacks, 800, 400, 2500, 7, 100, 20, 0, 0 };

s16 gGolemKnightBishopGrabHpLimits[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

u16 gGolemKnightBishopApproachRoll[16] = {
    0,
    0,
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
    1,
    1,
};

s16 gGolemKnightBishopIdleWaits[16] = {
    10,
    15,
    20,
    20,
    25,
    30,
    40,
    50,
    25,
    30,
    40,
    50,
    25,
    30,
    40,
    50,
};

u16 gGolemKnightBishopIdleSteps[16] = {
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

GolemKnightBishopSpot gGolemKnightBishopSpots[9] = {
    { 1, 4, 32, 5 },
    { 2, 5, 21, 6 },
    { 3, 4, 35, 3 },
    { 4, 3, 15, 3 },
    { 5, 4, 14, 8 },
    { 6, 4, 19, 5 },
    { 7, 5, 5, 2 },
    { 8, 5, 16, 4 },
    { 0, 0, 0, 0 },
};

GolemKnightBishopRegion D_actor_403900_80153CC4[5] = {
    { 0, 1000, 2500, 4000, 0, 0, 0, 0 },
    { 1, 1024, 2500, 4300, 0x28A0, 5200, 0x32C8, 3400 },
    { 1, 2048, 0x2904, 5700, 0x2710, 1500, 0x2AF8, 0 },
    { 1, 0, 6450, 1300, 5900, 5400, 7000, 4000 },
    { 1, 0, 1700, -500, 1000, 5800, 2400, 3200 },
};

GolemKnightBishopRegion D_actor_403900_80153D14[6] = {
    { 0, 1000, 7000, -3500, 0, 0, 0, 0 },
    { 0, 2000, 7000, 6000, 0, 0, 0, 0 },
    { 1, 0, 800, 1000, 0, 6000, 1550, 3000 },
    { 1, 3072, 7500, 4500, 1550, 6000, 4000, 4000 },
    { 1, 2048, 7000, 5000, 5000, -4000, 9000, -5000 },
    { 1, 1024, 500, 4500, 6000, 6000, 8000, 4000 },
};

GolemKnightBishopRegion D_actor_403900_80153D74[3] = {
    { 0, 3000, 7000, -1600, 0, 0, 0, 0 },
    { 0, 2000, 1600, -7300, 0, 0, 0, 0 },
    { 1, 2048, 1600, -1600, 1000, -7000, 2500, -8000 },
};

GolemKnightBishopRegion D_actor_403900_80153DA4[3] = {
    { 0, 2000, 7000, 3000, 0, 0, 0, 0 },
    { 1, 1024, -1000, 2750, 1500, 3500, 3500, 2000 },
    { 1, 3072, 2500, 500, -3500, 1000, -2000, 0 },
};

GolemKnightBishopRegion D_actor_403900_80153DD4[8] = {
    { 0, 2000, 1600, -1400, 0, 0, 0, 0 },
    { 0, 1000, 0x2710, 4200, 0, 0, 0, 0 },
    { 1, 3072, 0x2A94, 4200, 1000, 5000, 5000, 3500 },
    { 1, 3072, 5500, 2500, 1000, 3500, 3500, 1500 },
    { 1, 0, 8950, 1600, 8000, 5800, 9500, 3000 },
    { 1, 1024, 5500, 4000, 0x2AF8, 4500, 0x3070, 3000 },
    { 1, 2048, 1700, 4700, 1000, 0, 2400, -2900 },
    { 1, 1800, 1400, 4700, 2400, 1200, 3500, 0 },
};

GolemKnightBishopRegion D_actor_403900_80153E54[5] = {
    { 0, 800, 0x2710, 5300, 0, 0, 0, 0 },
    { 0, 800, 7500, 8000, 0, 0, 0, 0 },
    { 0, 800, 4000, 8000, 0, 0, 0, 0 },
    { 1, 1024, 1200, 8000, 6000, 9000, 7500, 7000 },
    { 1, 3072, 0x283C, 8000, 4000, 9000, 6000, 7000 },
};

GolemKnightBishopRegion D_actor_403900_80153EA4[2] = {
    { 0, 600, -1900, 4800, 0, 0, 0, 0 },
    { 1, 3072, 2000, 4800, -5500, 5500, -1900, 4000 },
};

GolemKnightBishopRegion D_actor_403900_80153EC4[4] = {
    { 0, 600, 3500, -1800, 0, 0, 0, 0 },
    { 1, 3072, 8500, -1800, 0, -1000, 2000, -2000 },
    { 1, 0, 7750, -7500, 6500, -1000, 9000, -2000 },
    { 1, 2048, 7750, -3000, 6500, -6000, 9000, -9000 },
};

GolemKnightBishopRegion* gGolemKnightBishopRegions[9] = {
    NULL,
    D_actor_403900_80153CC4,
    D_actor_403900_80153D14,
    D_actor_403900_80153D74,
    D_actor_403900_80153DA4,
    D_actor_403900_80153DD4,
    D_actor_403900_80153E54,
    D_actor_403900_80153EA4,
    D_actor_403900_80153EC4,
};

s16 D_actor_403900_80153F28[21] = {
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
};

s16 D_actor_403900_80153F54[39] = {
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
};

s16 D_actor_403900_80153FA4[39] = {
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
};

s16 D_actor_403900_80153FF4[50] = {
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

s16 D_actor_403900_80154058[34] = {
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

s16* gGolemKnightBishopStageCues[6] = {
    NULL,
    D_actor_403900_80153F28,
    D_actor_403900_80153F54,
    D_actor_403900_80153FA4,
    D_actor_403900_80153FF4,
    D_actor_403900_80154058,
};

AnimationSet* gGolemKnightBishopPlayerAnims[5] = {
    NULL,
    &D_actor_403900_80151998,
    &D_actor_403900_80151F2C,
    &D_actor_403900_8015298C,
    &D_actor_403900_80153BC8,
};

EffectSpawnArg gGolemKnightBishopGrabEffect = { NULL, 300, 1 };

s16 gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_403900_801540E0 = { { { TASK_BODY_TMD, 96 } }, func_actor_403900_80138344, { .model = &D_actor_403900_8013DBD8 } };

AnimationSet* gGolemKnightBishopAnimSets[22] = {
    NULL,
    &D_actor_403900_8013E26C,
    &D_actor_403900_8013E724,
    &D_actor_403900_8013EF30,
    &D_actor_403900_8013FB68,
    &D_actor_403900_80140C1C,
    &D_actor_403900_801417C8,
    &D_actor_403900_80145840,
    &D_actor_403900_80145D64,
    &D_actor_403900_801467AC,
    &D_actor_403900_8014749C,
    &D_actor_403900_80148DD8,
    &D_actor_403900_80149BE8,
    &D_actor_403900_8014AC60,
    &D_actor_403900_8014B134,
    &D_actor_403900_8014B444,
    &D_actor_403900_8014B620,
    &D_actor_403900_8014C568,
    &D_actor_403900_8014CAFC,
    &D_actor_403900_8014CDC4,
    &D_actor_403900_8014CFA0,
    &D_actor_403900_801505C8,
};

#include "../../shared/golem_knight_bishop_take_hits.inc.c"

#include "../../shared/golem_knight_bishop_hit_reaction.inc.c"

#include "../../shared/golem_knight_bishop_box_scan.inc.c"

#include "../../shared/golem_knight_bishop_idle_seq.inc.c"

#include "../../shared/golem_knight_bishop_player_in_box.inc.c"

#include "../../shared/golem_knight_bishop_place_target.inc.c"

#include "../../shared/golem_knight_bishop_grab_seq.inc.c"

#include "../../shared/golem_knight_bishop_strike.inc.c"

#include "../../shared/golem_knight_bishop_box_approach_seq.inc.c"

#include "../../shared/golem_knight_bishop_recover_seq.inc.c"

#include "../../shared/golem_knight_bishop_translucency_fade.inc.c"

#include "../../shared/golem_knight_bishop_light_flinch.inc.c"

#include "../../shared/golem_knight_bishop_heavy_flinch.inc.c"

#include "../../shared/golem_knight_bishop_kneel.inc.c"

#include "../../shared/golem_knight_bishop_kneel_hit.inc.c"

/// The enemy task's three state handlers, which `func_actor_403900_80138344`
/// picks by `Task::state`: the spawn setup, the frame handler that runs the
/// sequences, and the frame handler that unlinks the enemy and saves its pose
/// before running its own short sequence.
static const GpEnemyTaskFuncTable3 D_actor_403900_80131F18 = {
    golemKnightBishopSpawn,
    golemKnightBishopFrameState,
    golemKnightBishopDeadState,
};

#include "../../shared/golem_knight_bishop_collapse_death.inc.c"

#include "../../shared/golem_knight_bishop_anim_cues.inc.c"

#include "../../shared/golem_knight_bishop_aim_from_part.inc.c"

#include "../../shared/golem_knight_bishop_aim_beam.inc.c"

#include "../../shared/golem_knight_bishop_inlines.inc.c"

#include "../../shared/golem_knight_bishop_dead.inc.c"

#include "../../shared/frame_capture.inc.c"

#include "../../shared/golem_knight_bishop_spawn.inc.c"

#include "../../shared/golem_knight_bishop_frame.inc.c"

#include "../../shared/golem_knight_bishop_run_sequence.inc.c"

#include "../../shared/golem_knight_bishop_apply_scale.inc.c"

#include "../../shared/golem_knight_bishop_kneel_death.inc.c"

#include "../../shared/golem_knight_bishop_step_forward.inc.c"

#include "../../shared/golem_knight_bishop_tick_anim.inc.c"

#include "../../shared/golem_knight_bishop_update_tint.inc.c"

#include "../../shared/golem_knight_bishop_shadow.inc.c"

#include "../../shared/golem_knight_bishop_hold_cue.inc.c"

/// Raises the actor's phase `field_6F4` to 1 while enemies remain.
s32 func_actor_403900_801381E4(Task* task)
{
    if (gPlayerStatus.hp > 0) {
        ((GolemKnightBishopWork*)task->work)->field_6F4 = 1;
    }
    return 0;
}

#include "../../shared/golem_knight_bishop_frame_capture.inc.c"

/// Runs the enemy task's current state handler from
/// `D_actor_403900_80131F18`, copying the table onto the stack first.
static void func_actor_403900_80138344(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403900_80131F18;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
