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
#define GOLEM_KNIGHT_BISHOP_KIND GOLEM_KNIGHT
#include "../../shared/golem_knight_bishop.h"

/// Per-animation-id value `golemKnightBishopTickAnim` hands `animationSeekSlotWithBlend`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 gGolemKnightBishopAnimBlend[];

extern GolemKnightBishopFrameStep gGolemKnightBishopFrameSteps[];

/// Reacts to the damage just taken; see its definition.

/// Runs the one-shot vocal cue armed by `field_718`; see its definition.

/// Aims the actor at the player; see its definition.

/// Parks the actor's target position off the player; see its definition.

/// Reports whether the player stands in one of the kind-1 boxes; see its
/// definition.

/// Draws the red trail between the two projected points; see its definition.

/// Rebuilds the root part's scaled rotation; see its definition.

/// Projects a coordinate and queues the frame-buffer pass at its depth; see
/// its definition.

/// Per-roll wait lengths state 0 of `golemKnightBishopIdleSeq` scales by
/// `16 - field_70C`, indexed by a 4-bit `gRandomLcgState` draw.
extern s16 gGolemKnightBishopIdleWaits[];

/// Per-roll state offsets state 0 adds to 2 when `field_6E8` is set.
extern u16 gGolemKnightBishopIdleSteps[];

/// Cue word `golemKnightBishopLightFlinchSeq` and `golemKnightBishopHeavyFlinchSeq`
/// queue, a separate `D_` symbol in the overlay's data 0x48 past the cue-id
/// table `gGolemKnightBishopAnimCues`.
extern s32 gGolemKnightBishopPainCue;

/// Cue word the fade-out in `golemKnightBishopTranslucencyFade` queues.
extern s32 gGolemKnightBishopFadeCue;

/// Cue-id table: `GolemKnightBishopWork::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `flags` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one.
extern s32 gGolemKnightBishopAnimCues[];

/// Cue words `golemKnightBishopBoxApproachSeq` queues next to
/// `gGolemKnightBishopPainCue`.
extern s32 gGolemKnightBishopApproachCue;
extern s32 gGolemKnightBishopStrikeCue;

/// Base id of the actor's vocal cues: the `Enemy` work id's high nibble
/// selects one of the four adjacent words here, picked up as bits 8-11 of the
/// cue id.
extern s32 gGolemKnightBishopHoldCue;

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

/// Weighted 16-entry roll for `GolemKnightBishopWork::field_6E4`: indices 0-4 hold 0
/// and 5-15 hold 1, so the short approach is taken about two thirds of the time.
extern u16 gGolemKnightBishopApproachRoll[];

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

s32 func_actor_402200_801381E0(Task*);

extern AnimationSet D_actor_402200_8013E268;
extern AnimationSet D_actor_402200_8013E720;
extern AnimationSet D_actor_402200_8013EF2C;
extern AnimationSet D_actor_402200_8013FB64;
extern AnimationSet D_actor_402200_80140C18;
extern AnimationSet D_actor_402200_801417C4;
extern AnimationSet D_actor_402200_8014583C;
extern AnimationSet D_actor_402200_80145D60;
extern AnimationSet D_actor_402200_801467A8;
extern AnimationSet D_actor_402200_80147498;
extern AnimationSet D_actor_402200_80148DD4;
extern AnimationSet D_actor_402200_80149BE4;
extern AnimationSet D_actor_402200_8014AC5C;
extern AnimationSet D_actor_402200_8014B130;
extern AnimationSet D_actor_402200_8014B440;
extern AnimationSet D_actor_402200_8014B61C;
extern AnimationSet D_actor_402200_8014C564;
extern AnimationSet D_actor_402200_8014CAF8;
extern AnimationSet D_actor_402200_8014CDC0;
extern AnimationSet D_actor_402200_8014CF9C;
extern AnimationSet D_actor_402200_801505C4;
extern AnimationSet D_actor_402200_80151994;
extern AnimationSet D_actor_402200_80151F28;
extern AnimationSet D_actor_402200_80152988;
extern AnimationSet D_actor_402200_80153BC4;
extern DamageAttack gGolemKnightBishopAttacks[4];
extern TmdSource    D_actor_402200_8013DBD4;
static void         func_actor_402200_80138340(Task*);

GolemKnightBishopMessageEntry gGolemKnightBishopMessages[2] = {
    { 2014, { .call0 = func_actor_402200_801381E0 } },
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

TmdBone D_actor_402200_80138478[19] = {
#include "assets/golem_body_skeleton.inc"
};

u32 D_actor_402200_80138724[19] = {
#include "assets/golem_body_partVerts.inc"
};

SVECTOR D_actor_402200_80138770[363] = {
#include "assets/golem_body_verts.inc"
};

SVECTOR D_actor_402200_801392C8[345] = {
#include "assets/golem_body_normals.inc"
};

u32 D_actor_402200_80139D90[3985] = {
#include "assets/golem_body_stream.inc"
};

TmdSource D_actor_402200_8013DBD4 = {
    0,
    43904,
    11888,
    19,
    D_actor_402200_80138724,
    D_actor_402200_80138770,
    D_actor_402200_801392C8,
    D_actor_402200_80138478,
    D_actor_402200_80139D90,
};

AnimationPackedPose D_actor_402200_8013DBF8[12] = {
#include "assets/actor_402200_animation_0C448_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8013DC88[165] = {
#include "assets/actor_402200_animation_0C448_bank4.inc"
};

AnimationRecord D_actor_402200_8013DF1C[201] = {
#include "assets/actor_402200_animation_0C448_records.inc"
};

u16 D_actor_402200_8013E240[20] = {
#include "assets/actor_402200_animation_0C448_indices.inc"
};

AnimationSet D_actor_402200_8013E268 = {
    D_actor_402200_8013DF1C,
    D_actor_402200_8013E240,
    { NULL, D_actor_402200_8013DBF8, NULL, NULL, D_actor_402200_8013DC88, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8013E290[7] = {
#include "assets/actor_402200_animation_0C900_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8013E2E4[87] = {
#include "assets/actor_402200_animation_0C900_bank4.inc"
};

AnimationRecord D_actor_402200_8013E440[174] = {
#include "assets/actor_402200_animation_0C900_records.inc"
};

u16 D_actor_402200_8013E6F8[20] = {
#include "assets/actor_402200_animation_0C900_indices.inc"
};

AnimationSet D_actor_402200_8013E720 = {
    D_actor_402200_8013E440,
    D_actor_402200_8013E6F8,
    { NULL, D_actor_402200_8013E290, NULL, NULL, D_actor_402200_8013E2E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8013E748[15] = {
#include "assets/actor_402200_animation_0D10C_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8013E7FC[197] = {
#include "assets/actor_402200_animation_0D10C_bank4.inc"
};

AnimationRecord D_actor_402200_8013EB10[253] = {
#include "assets/actor_402200_animation_0D10C_records.inc"
};

u16 D_actor_402200_8013EF04[20] = {
#include "assets/actor_402200_animation_0D10C_indices.inc"
};

AnimationSet D_actor_402200_8013EF2C = {
    D_actor_402200_8013EB10,
    D_actor_402200_8013EF04,
    { NULL, D_actor_402200_8013E748, NULL, NULL, D_actor_402200_8013E7FC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8013EF54[21] = {
#include "assets/actor_402200_animation_0DD44_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8013F050[317] = {
#include "assets/actor_402200_animation_0DD44_bank4.inc"
};

AnimationRecord D_actor_402200_8013F544[382] = {
#include "assets/actor_402200_animation_0DD44_records.inc"
};

u16 D_actor_402200_8013FB3C[20] = {
#include "assets/actor_402200_animation_0DD44_indices.inc"
};

AnimationSet D_actor_402200_8013FB64 = {
    D_actor_402200_8013F544,
    D_actor_402200_8013FB3C,
    { NULL, D_actor_402200_8013EF54, NULL, NULL, D_actor_402200_8013F050, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8013FB8C[29] = {
#include "assets/actor_402200_animation_0EDF8_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8013FCE8[451] = {
#include "assets/actor_402200_animation_0EDF8_bank4.inc"
};

AnimationRecord D_actor_402200_801403F4[511] = {
#include "assets/actor_402200_animation_0EDF8_records.inc"
};

u16 D_actor_402200_80140BF0[20] = {
#include "assets/actor_402200_animation_0EDF8_indices.inc"
};

AnimationSet D_actor_402200_80140C18 = {
    D_actor_402200_801403F4,
    D_actor_402200_80140BF0,
    { NULL, D_actor_402200_8013FB8C, NULL, NULL, D_actor_402200_8013FCE8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_80140C40[30] = {
#include "assets/actor_402200_animation_0F9A4_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80140DA8[277] = {
#include "assets/actor_402200_animation_0F9A4_bank4.inc"
};

AnimationRecord D_actor_402200_801411FC[360] = {
#include "assets/actor_402200_animation_0F9A4_records.inc"
};

u16 D_actor_402200_8014179C[20] = {
#include "assets/actor_402200_animation_0F9A4_indices.inc"
};

AnimationSet D_actor_402200_801417C4 = {
    D_actor_402200_801411FC,
    D_actor_402200_8014179C,
    { NULL, D_actor_402200_80140C40, NULL, NULL, D_actor_402200_80140DA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_801417EC[111] = {
#include "assets/actor_402200_animation_13A1C_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80141D20[1789] = {
#include "assets/actor_402200_animation_13A1C_bank4.inc"
};

AnimationRecord D_actor_402200_80143914[1984] = {
#include "assets/actor_402200_animation_13A1C_records.inc"
};

u16 D_actor_402200_80145814[20] = {
#include "assets/actor_402200_animation_13A1C_indices.inc"
};

AnimationSet D_actor_402200_8014583C = {
    D_actor_402200_80143914,
    D_actor_402200_80145814,
    { NULL, D_actor_402200_801417EC, NULL, NULL, D_actor_402200_80141D20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_80145864[8] = {
#include "assets/actor_402200_animation_13F40_bank1.inc"
};

AnimationPackedRotation D_actor_402200_801458C4[116] = {
#include "assets/actor_402200_animation_13F40_bank4.inc"
};

AnimationRecord D_actor_402200_80145A94[169] = {
#include "assets/actor_402200_animation_13F40_records.inc"
};

u16 D_actor_402200_80145D38[20] = {
#include "assets/actor_402200_animation_13F40_indices.inc"
};

AnimationSet D_actor_402200_80145D60 = {
    D_actor_402200_80145A94,
    D_actor_402200_80145D38,
    { NULL, D_actor_402200_80145864, NULL, NULL, D_actor_402200_801458C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_80145D88[19] = {
#include "assets/actor_402200_animation_14988_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80145E6C[254] = {
#include "assets/actor_402200_animation_14988_bank4.inc"
};

AnimationRecord D_actor_402200_80146264[327] = {
#include "assets/actor_402200_animation_14988_records.inc"
};

u16 D_actor_402200_80146780[20] = {
#include "assets/actor_402200_animation_14988_indices.inc"
};

AnimationSet D_actor_402200_801467A8 = {
    D_actor_402200_80146264,
    D_actor_402200_80146780,
    { NULL, D_actor_402200_80145D88, NULL, NULL, D_actor_402200_80145E6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_801467D0[24] = {
#include "assets/actor_402200_animation_15678_bank1.inc"
};

AnimationPackedRotation D_actor_402200_801468F0[343] = {
#include "assets/actor_402200_animation_15678_bank4.inc"
};

AnimationRecord D_actor_402200_80146E4C[393] = {
#include "assets/actor_402200_animation_15678_records.inc"
};

u16 D_actor_402200_80147470[20] = {
#include "assets/actor_402200_animation_15678_indices.inc"
};

AnimationSet D_actor_402200_80147498 = {
    D_actor_402200_80146E4C,
    D_actor_402200_80147470,
    { NULL, D_actor_402200_801467D0, NULL, NULL, D_actor_402200_801468F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_801474C0[53] = {
#include "assets/actor_402200_animation_16FB4_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014773C[660] = {
#include "assets/actor_402200_animation_16FB4_bank4.inc"
};

AnimationRecord D_actor_402200_8014818C[776] = {
#include "assets/actor_402200_animation_16FB4_records.inc"
};

u16 D_actor_402200_80148DAC[20] = {
#include "assets/actor_402200_animation_16FB4_indices.inc"
};

AnimationSet D_actor_402200_80148DD4 = {
    D_actor_402200_8014818C,
    D_actor_402200_80148DAC,
    { NULL, D_actor_402200_801474C0, NULL, NULL, D_actor_402200_8014773C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_80148DFC[26] = {
#include "assets/actor_402200_animation_17DC4_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80148F34[365] = {
#include "assets/actor_402200_animation_17DC4_bank4.inc"
};

AnimationRecord D_actor_402200_801494E8[437] = {
#include "assets/actor_402200_animation_17DC4_records.inc"
};

u16 D_actor_402200_80149BBC[20] = {
#include "assets/actor_402200_animation_17DC4_indices.inc"
};

AnimationSet D_actor_402200_80149BE4 = {
    D_actor_402200_801494E8,
    D_actor_402200_80149BBC,
    { NULL, D_actor_402200_80148DFC, NULL, NULL, D_actor_402200_80148F34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_80149C0C[31] = {
#include "assets/actor_402200_animation_18E3C_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80149D80[429] = {
#include "assets/actor_402200_animation_18E3C_bank4.inc"
};

AnimationRecord D_actor_402200_8014A434[512] = {
#include "assets/actor_402200_animation_18E3C_records.inc"
};

u16 D_actor_402200_8014AC34[20] = {
#include "assets/actor_402200_animation_18E3C_indices.inc"
};

AnimationSet D_actor_402200_8014AC5C = {
    D_actor_402200_8014A434,
    D_actor_402200_8014AC34,
    { NULL, D_actor_402200_80149C0C, NULL, NULL, D_actor_402200_80149D80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014AC84[9] = {
#include "assets/actor_402200_animation_19310_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014ACF0[113] = {
#include "assets/actor_402200_animation_19310_bank4.inc"
};

AnimationRecord D_actor_402200_8014AEB4[149] = {
#include "assets/actor_402200_animation_19310_records.inc"
};

u16 D_actor_402200_8014B108[20] = {
#include "assets/actor_402200_animation_19310_indices.inc"
};

AnimationSet D_actor_402200_8014B130 = {
    D_actor_402200_8014AEB4,
    D_actor_402200_8014B108,
    { NULL, D_actor_402200_8014AC84, NULL, NULL, D_actor_402200_8014ACF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014B158[5] = {
#include "assets/actor_402200_animation_19620_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014B194[65] = {
#include "assets/actor_402200_animation_19620_bank4.inc"
};

AnimationRecord D_actor_402200_8014B298[96] = {
#include "assets/actor_402200_animation_19620_records.inc"
};

u16 D_actor_402200_8014B418[20] = {
#include "assets/actor_402200_animation_19620_indices.inc"
};

AnimationSet D_actor_402200_8014B440 = {
    D_actor_402200_8014B298,
    D_actor_402200_8014B418,
    { NULL, D_actor_402200_8014B158, NULL, NULL, D_actor_402200_8014B194, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014B468[2] = {
#include "assets/actor_402200_animation_197FC_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014B480[17] = {
#include "assets/actor_402200_animation_197FC_bank4.inc"
};

AnimationRecord D_actor_402200_8014B4C4[76] = {
#include "assets/actor_402200_animation_197FC_records.inc"
};

u16 D_actor_402200_8014B5F4[20] = {
#include "assets/actor_402200_animation_197FC_indices.inc"
};

AnimationSet D_actor_402200_8014B61C = {
    D_actor_402200_8014B4C4,
    D_actor_402200_8014B5F4,
    { NULL, D_actor_402200_8014B468, NULL, NULL, D_actor_402200_8014B480, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014B644[28] = {
#include "assets/actor_402200_animation_1A744_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014B794[394] = {
#include "assets/actor_402200_animation_1A744_bank4.inc"
};

AnimationRecord D_actor_402200_8014BDBC[480] = {
#include "assets/actor_402200_animation_1A744_records.inc"
};

u16 D_actor_402200_8014C53C[20] = {
#include "assets/actor_402200_animation_1A744_indices.inc"
};

AnimationSet D_actor_402200_8014C564 = {
    D_actor_402200_8014BDBC,
    D_actor_402200_8014C53C,
    { NULL, D_actor_402200_8014B644, NULL, NULL, D_actor_402200_8014B794, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014C58C[9] = {
#include "assets/actor_402200_animation_1ACD8_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014C5F8[127] = {
#include "assets/actor_402200_animation_1ACD8_bank4.inc"
};

AnimationRecord D_actor_402200_8014C7F4[183] = {
#include "assets/actor_402200_animation_1ACD8_records.inc"
};

u16 D_actor_402200_8014CAD0[20] = {
#include "assets/actor_402200_animation_1ACD8_indices.inc"
};

AnimationSet D_actor_402200_8014CAF8 = {
    D_actor_402200_8014C7F4,
    D_actor_402200_8014CAD0,
    { NULL, D_actor_402200_8014C58C, NULL, NULL, D_actor_402200_8014C5F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014CB20[5] = {
#include "assets/actor_402200_animation_1AFA0_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014CB5C[56] = {
#include "assets/actor_402200_animation_1AFA0_bank4.inc"
};

AnimationRecord D_actor_402200_8014CC3C[87] = {
#include "assets/actor_402200_animation_1AFA0_records.inc"
};

u16 D_actor_402200_8014CD98[20] = {
#include "assets/actor_402200_animation_1AFA0_indices.inc"
};

AnimationSet D_actor_402200_8014CDC0 = {
    D_actor_402200_8014CC3C,
    D_actor_402200_8014CD98,
    { NULL, D_actor_402200_8014CB20, NULL, NULL, D_actor_402200_8014CB5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014CDE8[2] = {
#include "assets/actor_402200_animation_1B17C_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014CE00[17] = {
#include "assets/actor_402200_animation_1B17C_bank4.inc"
};

AnimationRecord D_actor_402200_8014CE44[76] = {
#include "assets/actor_402200_animation_1B17C_records.inc"
};

u16 D_actor_402200_8014CF74[20] = {
#include "assets/actor_402200_animation_1B17C_indices.inc"
};

AnimationSet D_actor_402200_8014CF9C = {
    D_actor_402200_8014CE44,
    D_actor_402200_8014CF74,
    { NULL, D_actor_402200_8014CDE8, NULL, NULL, D_actor_402200_8014CE00, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_8014CFC4[100] = {
#include "assets/actor_402200_animation_1E7A4_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8014D474[1463] = {
#include "assets/actor_402200_animation_1E7A4_bank4.inc"
};

AnimationRecord D_actor_402200_8014EB50[1683] = {
#include "assets/actor_402200_animation_1E7A4_records.inc"
};

u16 D_actor_402200_8015059C[20] = {
#include "assets/actor_402200_animation_1E7A4_indices.inc"
};

AnimationSet D_actor_402200_801505C4 = {
    D_actor_402200_8014EB50,
    D_actor_402200_8015059C,
    { NULL, D_actor_402200_8014CFC4, NULL, NULL, D_actor_402200_8014D474, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_801505EC[39] = {
#include "assets/actor_402200_animation_1FB74_bank1.inc"
};

AnimationPackedRotation D_actor_402200_801507C0[525] = {
#include "assets/actor_402200_animation_1FB74_bank4.inc"
};

AnimationRecord D_actor_402200_80150FF4[606] = {
#include "assets/actor_402200_animation_1FB74_records.inc"
};

u16 D_actor_402200_8015196C[20] = {
#include "assets/actor_402200_animation_1FB74_indices.inc"
};

AnimationSet D_actor_402200_80151994 = {
    D_actor_402200_80150FF4,
    D_actor_402200_8015196C,
    { NULL, D_actor_402200_801505EC, NULL, NULL, D_actor_402200_801507C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_801519BC[10] = {
#include "assets/actor_402200_animation_20108_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80151A34[126] = {
#include "assets/actor_402200_animation_20108_bank4.inc"
};

AnimationRecord D_actor_402200_80151C2C[181] = {
#include "assets/actor_402200_animation_20108_records.inc"
};

u16 D_actor_402200_80151F00[20] = {
#include "assets/actor_402200_animation_20108_indices.inc"
};

AnimationSet D_actor_402200_80151F28 = {
    D_actor_402200_80151C2C,
    D_actor_402200_80151F00,
    { NULL, D_actor_402200_801519BC, NULL, NULL, D_actor_402200_80151A34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_80151F50[21] = {
#include "assets/actor_402200_animation_20B68_bank1.inc"
};

AnimationPackedRotation D_actor_402200_8015204C[261] = {
#include "assets/actor_402200_animation_20B68_bank4.inc"
};

AnimationRecord D_actor_402200_80152460[320] = {
#include "assets/actor_402200_animation_20B68_records.inc"
};

u16 D_actor_402200_80152960[20] = {
#include "assets/actor_402200_animation_20B68_indices.inc"
};

AnimationSet D_actor_402200_80152988 = {
    D_actor_402200_80152460,
    D_actor_402200_80152960,
    { NULL, D_actor_402200_80151F50, NULL, NULL, D_actor_402200_8015204C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_402200_801529B0[35] = {
#include "assets/actor_402200_animation_21DA4_bank1.inc"
};

AnimationPackedRotation D_actor_402200_80152B54[485] = {
#include "assets/actor_402200_animation_21DA4_bank4.inc"
};

AnimationRecord D_actor_402200_801532E8[557] = {
#include "assets/actor_402200_animation_21DA4_records.inc"
};

u16 D_actor_402200_80153B9C[20] = {
#include "assets/actor_402200_animation_21DA4_indices.inc"
};

AnimationSet D_actor_402200_80153BC4 = {
    D_actor_402200_801532E8,
    D_actor_402200_80153B9C,
    { NULL, D_actor_402200_801529B0, NULL, NULL, D_actor_402200_80152B54, NULL, NULL, NULL },
};

DamageAttack gGolemKnightBishopAttacks[4] = { { 10, 3 }, { 36, 3 }, { 50, 3 }, { 999, 0 } };

EnemyParams gGolemKnightBishopParams = { gGolemKnightBishopAttacks, 600, 300, 1000, 6, 100, 20, 0, 0 };

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
};

s16 gGolemKnightBishopIdleWaits[16] = {
    50,
    60,
    70,
    80,
    90,
    90,
    95,
    95,
    100,
    100,
    105,
    105,
    110,
    110,
    120,
    130,
};

u16 gGolemKnightBishopIdleSteps[16] = {
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
    2,
    2,
    2,
    2,
};

GolemKnightBishopSpot gGolemKnightBishopSpots[10] = {
    { 1, 4, 14, 8 },
    { 2, 4, 19, 5 },
    { 3, 5, 5, 2 },
    { 4, 1, 11, 5 },
    { 5, 2, 15, 3 },
    { 6, 3, 38, 8 },
    { 7, 4, 15, 4 },
    { 8, 4, 32, 5 },
    { 9, 5, 21, 6 },
    { 0, 0, 0, 0 },
};

GolemKnightBishopRegion D_actor_402200_80153CC8[8] = {
    { 0, 2000, 1600, -1400, 0, 0, 0, 0 },
    { 0, 1000, 0x2710, 4200, 0, 0, 0, 0 },
    { 1, 3072, 0x2A94, 4200, 1000, 5000, 5000, 3500 },
    { 1, 3072, 5500, 2500, 1000, 3500, 3500, 1500 },
    { 1, 0, 8950, 1600, 8000, 5800, 9500, 3000 },
    { 1, 1024, 5500, 4000, 0x2AF8, 4500, 0x3070, 3000 },
    { 1, 2048, 1700, 4700, 1000, 0, 2400, -2900 },
    { 1, 1800, 1400, 4700, 2400, 1200, 3500, 0 },
};

GolemKnightBishopRegion D_actor_402200_80153D48[5] = {
    { 0, 800, 0x2710, 5300, 0, 0, 0, 0 },
    { 0, 800, 7500, 8000, 0, 0, 0, 0 },
    { 0, 800, 4000, 8000, 0, 0, 0, 0 },
    { 1, 1024, 1200, 8000, 6000, 9000, 7500, 7000 },
    { 1, 3072, 0x283C, 8000, 4000, 9000, 6000, 7000 },
};

GolemKnightBishopRegion D_actor_402200_80153D98[2] = {
    { 0, 600, -1900, 4800, 0, 0, 0, 0 },
    { 1, 3072, 2000, 4800, -5500, 5500, -1900, 4000 },
};

GolemKnightBishopRegion D_actor_402200_80153DB8[5] = {
    { 0, 2000, -3000, -2000, 0, 0, 0, 0 },
    { 0, 1000, -1500, -7500, 0, 0, 0, 0 },
    { 0, 2000, -100, 2600, 0, 0, 0, 0 },
    { 1, 0, -1000, -7500, -2500, 4500, 500, 2000 },
    { 1, 2048, -300, 9500, -500, 6500, 500, 5000 },
};

GolemKnightBishopRegion D_actor_402200_80153E08[3] = {
    { 0, 2000, 7000, 3000, 0, 0, 0, 0 },
    { 1, 1024, -1000, 2750, 1500, 3500, 3500, 2000 },
    { 1, 3072, 2500, 500, -3500, 1000, -2000, 0 },
};

GolemKnightBishopRegion D_actor_402200_80153E38[8] = {
    { 0, 3000, 5500, -8500, 0, 0, 0, 0 },
    { 0, 2000, 0x4074, -7000, 0, 0, 0, 0 },
    { 1, 0, 0x4074, -7000, 0x3A98, -500, 0x4650, -2500 },
    { 1, 1024, 7500, -8500, 0x32C8, -7000, 0x3C8C, -9800 },
    { 1, 2048, 0x4074, -4000, 0x3C8C, -6600, 0x4650, -9800 },
    { 1, 3072, 0x2904, -8500, 4000, -7000, 6500, -9800 },
    { 1, 3072, 0x4268, -8500, 9200, -7000, 0x2EE0, -9800 },
    { 1, 3072, 0x2AF8, -8500, 6500, -7000, 8500, -9800 },
};

GolemKnightBishopRegion D_actor_402200_80153EB8[4] = {
    { 0, 2000, 0, -3000, 0, 0, 0, 0 },
    { 1, 2048, 0, -2000, -500, -7000, 500, -0x2AF8 },
    { 1, 0, 0, -0x4650, -500, -0x2AF8, 500, -0x32C8 },
    { 1, 0, 0, -0x2710, -500, -5000, 500, -7000 },
};

GolemKnightBishopRegion D_actor_402200_80153EF8[5] = {
    { 0, 1000, 2500, 4000, 0, 0, 0, 0 },
    { 1, 1024, 2500, 4300, 0x28A0, 5200, 0x32C8, 3400 },
    { 1, 2048, 0x2904, 5700, 0x2710, 1500, 0x2AF8, 0 },
    { 1, 0, 6450, 1300, 5900, 5400, 7000, 4000 },
    { 1, 0, 1700, -500, 1000, 5800, 2400, 3200 },
};

GolemKnightBishopRegion D_actor_402200_80153F48[6] = {
    { 0, 1000, 7000, -3500, 0, 0, 0, 0 },
    { 0, 2000, 7000, 6000, 0, 0, 0, 0 },
    { 1, 0, 800, 1000, 0, 6000, 1550, 3000 },
    { 1, 3072, 7500, 4500, 1550, 6000, 4000, 4000 },
    { 1, 2048, 7000, 5000, 5000, -4000, 9000, -5000 },
    { 1, 1024, 500, 4500, 6000, 6000, 8000, 4000 },
};

GolemKnightBishopRegion* gGolemKnightBishopRegions[10] = {
    NULL,
    D_actor_402200_80153CC8,
    D_actor_402200_80153D48,
    D_actor_402200_80153D98,
    D_actor_402200_80153DB8,
    D_actor_402200_80153E08,
    D_actor_402200_80153E38,
    D_actor_402200_80153EB8,
    D_actor_402200_80153EF8,
    D_actor_402200_80153F48,
};

s16 D_actor_402200_80153FD0[21] = {
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

s16 D_actor_402200_80153FFC[39] = {
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

s16 D_actor_402200_8015404C[39] = {
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

s16 D_actor_402200_8015409C[50] = {
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

s16 D_actor_402200_80154100[34] = {
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
    D_actor_402200_80153FD0,
    D_actor_402200_80153FFC,
    D_actor_402200_8015404C,
    D_actor_402200_8015409C,
    D_actor_402200_80154100,
};

AnimationSet* gGolemKnightBishopPlayerAnims[5] = {
    NULL,
    &D_actor_402200_80151994,
    &D_actor_402200_80151F28,
    &D_actor_402200_80152988,
    &D_actor_402200_80153BC4,
};

EffectSpawnArg gGolemKnightBishopGrabEffect = { NULL, 300, 1 };

s16 gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_402200_80154188 = { { { TASK_BODY_TMD, 96 } }, func_actor_402200_80138340, { .model = &D_actor_402200_8013DBD4 } };

AnimationSet* gGolemKnightBishopAnimSets[22] = {
    NULL,
    &D_actor_402200_8013E268,
    &D_actor_402200_8013E720,
    &D_actor_402200_8013EF2C,
    &D_actor_402200_8013FB64,
    &D_actor_402200_80140C18,
    &D_actor_402200_801417C4,
    &D_actor_402200_8014583C,
    &D_actor_402200_80145D60,
    &D_actor_402200_801467A8,
    &D_actor_402200_80147498,
    &D_actor_402200_80148DD4,
    &D_actor_402200_80149BE4,
    &D_actor_402200_8014AC5C,
    &D_actor_402200_8014B130,
    &D_actor_402200_8014B440,
    &D_actor_402200_8014B61C,
    &D_actor_402200_8014C564,
    &D_actor_402200_8014CAF8,
    &D_actor_402200_8014CDC0,
    &D_actor_402200_8014CF9C,
    &D_actor_402200_801505C4,
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

/// The enemy task's three state handlers, which `func_actor_402200_80138340`
/// picks by `Task::state`: the spawn setup, the frame handler that runs the
/// sequences, and the frame handler that unlinks the enemy and saves its pose
/// before running its own short sequence.
static const GpEnemyTaskFuncTable3 D_actor_402200_80131F18 = {
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
s32 func_actor_402200_801381E0(Task* task)
{
    if (gPlayerStatus.hp > 0) {
        ((GolemKnightBishopWork*)task->work)->field_6F4 = 1;
    }
    return 0;
}

#include "../../shared/golem_knight_bishop_frame_capture.inc.c"
/// Runs the enemy task's current state handler from
/// `D_actor_402200_80131F18`, copying the table onto the stack first.
static void func_actor_402200_80138340(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_402200_80131F18;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
