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
#include "../../shared/golem_knight_bishop.h"

/// Per-animation-id value `golemKnightBishopTickAnim` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 gGolemKnightBishopAnimBlend[];

extern GolemKnightBishopFrameStep D_actor_402200_801383D8[];

/// Reacts to the damage just taken; see its definition.

/// Runs the one-shot vocal cue armed by `field_718`; see its definition.

/// Aims the actor at the player; see its definition.
static void func_actor_402200_80135D5C(Task* arg0);

/// Parks the actor's target position off the player; see its definition.

/// Reports whether the player stands in one of the kind-1 boxes; see its
/// definition.

/// Draws the red trail between the two projected points; see its definition.

/// Rebuilds the root part's scaled rotation; see its definition.

/// Projects a coordinate and queues the frame-buffer pass at its depth; see
/// its definition.

/// Per-roll wait lengths state 0 of `golemKnightBishopIdleSeq` scales by
/// `16 - field_70C`, indexed by a 4-bit `gRandomLcgState` draw.
extern s16 D_actor_402200_80153C38[];

/// Per-roll state offsets state 0 adds to 2 when `field_6E8` is set.
extern u16 D_actor_402200_80153C58[];

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
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*);
    } handler;
} Actor402200MessageEntry;
STATIC_ASSERT_SIZEOF(Actor402200MessageEntry, 8);

extern Actor402200MessageEntry  D_actor_402200_8013839C[2];
extern DamageAttack             gGolemKnightBishopAttacks[4];
extern EnemyParams              D_actor_402200_80153BFC;
extern GolemKnightBishopSpot    D_actor_402200_80153C78[];
extern GolemKnightBishopRegion* D_actor_402200_80153FA8[];
extern s16*                     D_actor_402200_80154144[];
extern AnimationSet*            D_actor_402200_80154194[22];

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 D_actor_402200_80153C0C[];

/// Weighted 16-entry roll for `GolemKnightBishopWork::field_6E4`: indices 0-4 hold 0
/// and 5-15 hold 1, so the short approach is taken about two thirds of the time.
extern u16 gGolemKnightBishopApproachRoll[];

/// Animation block the grab's 0x3FF messages hand the player.
extern AnimationSet* D_actor_402200_8015415C[5];

/// `func_800FDB18` argument record for the grab's finishing spark.
extern EffectSpawnArg D_actor_402200_80154170;

/// The two four-vertex index rows the trail's shaded quads take their corners
/// from, into the scratch block's six-entry x / y runs.
extern s16 gGolemKnightBishopBeamQuadCorners[2][4];

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_402200_80137444(Enemy* arg0, Task* arg1);

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

Actor402200MessageEntry D_actor_402200_8013839C[2] = {
    { 2014, { .call0 = func_actor_402200_801381E0 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
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

GolemKnightBishopFrameStep D_actor_402200_801383D8[18] = {
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
#include "assets/actor_402200_model_0BDB4_skeleton.inc"
};

u32 D_actor_402200_80138724[19] = {
#include "assets/actor_402200_model_0BDB4_partVerts.inc"
};

SVECTOR D_actor_402200_80138770[363] = {
#include "assets/actor_402200_model_0BDB4_verts.inc"
};

SVECTOR D_actor_402200_801392C8[345] = {
#include "assets/actor_402200_model_0BDB4_normals.inc"
};

u32 D_actor_402200_80139D90[3985] = {
#include "assets/actor_402200_model_0BDB4_stream.inc"
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

EnemyParams D_actor_402200_80153BFC = { gGolemKnightBishopAttacks, 600, 300, 1000, 6, 100, 20, 0, 0 };

s16 D_actor_402200_80153C0C[6] = {
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

s16 D_actor_402200_80153C38[16] = {
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

u16 D_actor_402200_80153C58[16] = {
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

GolemKnightBishopSpot D_actor_402200_80153C78[10] = {
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

GolemKnightBishopRegion* D_actor_402200_80153FA8[10] = {
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

s16* D_actor_402200_80154144[6] = {
    NULL,
    D_actor_402200_80153FD0,
    D_actor_402200_80153FFC,
    D_actor_402200_8015404C,
    D_actor_402200_8015409C,
    D_actor_402200_80154100,
};

AnimationSet* D_actor_402200_8015415C[5] = {
    NULL,
    &D_actor_402200_80151994,
    &D_actor_402200_80151F28,
    &D_actor_402200_80152988,
    &D_actor_402200_80153BC4,
};

EffectSpawnArg D_actor_402200_80154170 = { NULL, 300, 1 };

s16 gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_402200_80154188 = { { { TASK_BODY_TMD, 96 } }, func_actor_402200_80138340, { .model = &D_actor_402200_8013DBD4 } };

AnimationSet* D_actor_402200_80154194[22] = {
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

/// Per-frame hit handler: applies the `func_800E0C10` push-back from the
/// `field_504` and (while `field_49A` enables grid tests) `field_49C`
/// record tables to the root coordinate, ticks the `field_6C6` flinch
/// countdown, and for each kind-2 hit record in `field_49C` computes the
/// damage from the distance to the player, applies it to the `Enemy`,
/// spawns the hit sparks once per distinct id and hands the damage to
/// `golemKnightBishopPickHitReaction` unless the vocal cue is armed.
void golemKnightBishopTakeHits(Task* arg0)
{
    s32                          lastId;
    GolemKnightBishopWork*       work;
    GolemKnightBishopHitScratch* head;
    GolemKnightBishopHitScratch* sc;
    GolemKnightBishopHitScratch* blk;
    Enemy*                       enemy;
    GfxCoord*                    coord;
    s32                          i;
    s32                          damage;
    s32                          kind;
    s32                          wait;
    s16                          t;

    lastId                                            = 0;
    work                                              = arg0->work;
    head                                              = SCRATCH_STACK_CURSOR(GolemKnightBishopHitScratch);
    blk                                               = head - 1;
    SCRATCH_STACK_CURSOR(GolemKnightBishopHitScratch) = blk;
    sc                                                = blk;
    coord                                             = arg0->extra.tmd->coords;
    enemy                                             = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_504, &sc->delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.vx.halves.integer;
            coord->coord.t[1] += sc->delta.vy.halves.integer;
            coord->coord.t[2] += sc->delta.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->field_664;
            coord->coord.t[1] = work->field_668;
            coord->coord.t[2] = work->field_66C;
            break;
    }
    Gp_ClearRec18Occupied(work->field_504);

    if (work->field_49A & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->field_49C, &sc->delta, 3, NULL)) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += sc->delta.vx.halves.integer;
                coord->coord.t[2] += sc->delta.vz.halves.integer;
                break;
            case 2:
                coord->coord.t[0] = work->field_664;
                coord->coord.t[2] = work->field_66C;
                break;
        }
    }

    if (work->field_6C6 != 0) {
        t               = work->field_6C6 - 1;
        work->field_6C6 = t;
        if (t <= 0) {
            work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_6C6  = 0;
            work->field_494  = work->field_716 | 0x30000;
        }
    }

    for (i = 0; i < 3; i++) {
        switch ((u32)work->field_49C[i].key.value >> 16) {
            case 0:
                break;
            case 1:
                if (work->field_6E4 == 1) {
                    work->field_6E8 = 1;
                }
                break;
            case 2:
                if (work->field_6C6 != 0) {
                    break;
                }
                if (work->field_6E4 == 1) {
                    work->field_6E8 = 1;
                    break;
                }
                sc->delta.vx.word = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.word = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.word = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->field_6D2   = (u32) ~(sc->delta.vx.word * coord->coord.m[0][2] +
                                          sc->delta.vy.word * coord->coord.m[1][2] +
                                          sc->delta.vz.word * coord->coord.m[2][2]) >>
                                  31;
                damage = Gp_ComputeDamage(work->field_49C[i].key.value,
                                          SquareRoot0(sc->delta.vx.word * sc->delta.vx.word +
                                                      sc->delta.vy.word * sc->delta.vy.word +
                                                      sc->delta.vz.word * sc->delta.vz.word),
                                          0, 0);
                kind   = Gp_GetIdParam0(work->field_49C[i].key.value);
                if ((u16)kind == 5) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 2, NULL);
                }
                if (Gp_RollEnemyChance(enemy, work->field_49C[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((u16)kind != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->field_49C[i].key.value, damage, 0);
                enemy->hp       -= damage;
                work->field_70A += damage;
                switch ((u16)kind) {
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                        break;
                    case 1:
                    case 2:
                        if (work->field_6EC == 0) {
                            work->field_6EC = 1;
                            work->field_6DA = 7;
                        }
                        break;
                    case 9:
                        work->field_70A += 0xA0;
                        break;
                }
                if (lastId != work->field_49C[i].key.value) {
                    lastId     = work->field_49C[i].key.value;
                    sc->ofs.vx = 0;
                    sc->ofs.vy = 0;
                    t          = -0x96;
                    if (work->field_6D2 == 1) {
                        t = 0xC8;
                    }
                    sc->ofs.vz = t;
                    func_800FDB18((u16)Gp_GetIdParam1(work->field_49C[i].key.value),
                                  &arg0->extra.tmd->coords[3], &sc->ofs,
                                  &work->field_65C);
                }
                wait = Gp_GetIdParam2(work->field_49C[i].key.value);
                if (wait > 0) {
                    work->field_6C6 = wait;
                }
                if (work->field_6DA >= 8) {
                    work->field_6EA = 2;
                }
                if (work->field_718 != 1) {
                    golemKnightBishopPickHitReaction(arg0, damage);
                } else {
                    work->field_6F4 = 2;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_49C);
    if (work->field_584.flags & 1) {
        work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(&work->field_584);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopHitScratch);
}

#include "../../shared/golem_knight_bishop_hit_reaction.inc.c"

#include "../../shared/golem_knight_bishop_box_scan.inc.c"

/// State machine on `field_6CE`: 0 rolls a `field_6D4` wait, 1 counts it
/// down, 2 picks state 3 or 4 from `field_70E` and an LCG draw offset by
/// `field_710` (or 5 when `golemKnightBishopPlayerInBox` reports a box hit), and 3-5
/// settle the result, walking `field_70C` up to 12.
void golemKnightBishopIdleSeq(Task* arg0)
{
    GolemKnightBishopWork* work;

    work = arg0->work;
    switch (work->field_6CE) {
        case 0:
            work->field_6C8 = 0;
            work->field_6EC = 0;
            work->field_6EE = 0;
            if (work->field_6E8 != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6CE = D_actor_402200_80153C58[(gRandomLcgState >> 16) & 0xF] + 2;
                work->field_6EE = 1;
                golemKnightBishopPlaceTarget(arg0);
                work->field_6E4 = 0;
            } else if (work->field_6E4 == 0) {
                work->field_6D4 = (D_actor_402200_80153C38[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF] * (0x10 - work->field_70C)) / 16;
                work->field_6CE = 1;
            } else {
                work->field_6D4 = 0;
                work->field_6CE = 2;
                work->field_6E4 = 0;
            }
            break;
        case 1:
            work->field_6D4--;
            if ((s16)work->field_6D4 <= 0) {
                work->field_6CE = 2;
                work->field_6D4 = 0;
            }
            break;
        case 2:
            if (work->field_70E != 3 && golemKnightBishopPlayerInBox(arg0) != 0) {
                work->field_6CE = 5;
                work->field_710 = 0;
                break;
            }
            if (work->field_70E == 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->field_710 + 10) {
                    work->field_6CE = 4;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 3;
                    work->field_710++;
                }
            } else if (work->field_70E == 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((s32)((gRandomLcgState >> 16) & 0xF) < work->field_710 + 8) {
                    work->field_6CE = 3;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 4;
                    work->field_710++;
                }
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6CE = ((gRandomLcgState >> 16) & 0xF) < 8 ? 3 : 4;
                work->field_710 = 0;
            }
            golemKnightBishopPlaceTarget(arg0);
            break;
        case 3:
            if (work->field_5F4.key.value == 0) {
                work->field_6CC = 1;
                work->field_6CE = 0;
                work->field_70E = 1;
                if (work->field_70C < 12) {
                    work->field_70C++;
                }
            } else {
                work->field_6CE = 2;
                if (work->field_710 != 0) {
                    work->field_710--;
                }
            }
            work->field_5BA &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            work->field_5DA &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
        case 4:
            if (work->field_5F4.key.value == 0) {
                work->field_6CC = 2;
                work->field_6CE = 0;
                work->field_70E = 2;
                if (work->field_70C < 12) {
                    work->field_70C++;
                }
            } else {
                work->field_6CE = 2;
                if (work->field_710 != 0) {
                    work->field_710--;
                }
            }
            work->field_5BA &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
        case 5:
            work->field_6CC = 3;
            work->field_6CE = 0;
            work->field_70E = 3;
            Gp_ClearRec18Occupied(&work->field_644);
            if (work->field_70C < 12) {
                work->field_70C++;
            }
            break;
    }
}

#include "../../shared/golem_knight_bishop_player_in_box.inc.c"

#include "../../shared/golem_knight_bishop_place_target.inc.c"

/// Runs the actor's hold sequence on the player (the same 0x3F8 / 0x3FF
/// message pair `func_actor_103700_80134F50` uses to take a hold). State 0
/// asks the player for range 0x19 while enemies remain; on success it plants
/// the display object at `field_6A4`, places the player 0x5AA in front of it
/// with message 0x3E9 and queues a cue. States 1 and 2 step the player's
/// animation. State 3 waits out `field_6D6`, then every 0x1E frames decides
/// whether the hold ends: always when `gPlayerStatus.hp` is above the
/// per-difficulty `D_actor_402200_80153C0C`, otherwise by an LCG roll whose
/// chance grows with the attempt count `field_6F6`; a raised `field_6F4`
/// ends it early. State 5 either reacts to `field_6F4` or, at frame 0x1A,
/// spawns the spark, sends message 0x400 and clears `gPlayerStatus.hp`; state 7
/// then loads file 9/0x1E and queues cue 0x70010001 once the CD is idle.
void golemKnightBishopGrabSeq(Task* arg0)
{
    GolemKnightBishopWork*        work;
    GfxCoord*                     coord;
    Task*                         player;
    GolemKnightBishopGrabScratch* sc;
    GfxCoord*                     pcoord;
    s32                           flag;
    s32                           snd;
    s32                           chance;
    u32                           random;
    s16                           timer;
    s16                           val;
    s16                           sub;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(sizeof(GolemKnightBishopGrabScratch));
    sc     = SCRATCH_STACK_CURSOR(GolemKnightBishopGrabScratch);
    pcoord = player->extra.tmd->coords;
    flag   = 0;
    switch (work->field_6CE) {
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED && gPlayerStatus.hp > 0) {
                sc->query.field_14 = 0x19;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, 0x3F8, sc, 0) == 0) {
                    work->field_6C0 = 1;
                    work->field_6CE = 1;
                    work->field_6F4 = 0;
                    work->field_6E8 = 0;
                    work->field_718 = 1;
                    sc->in.vx       = 0;
                    sc->in.vy       = work->field_6E6;
                    sc->in.vz       = 0;
                    RotMatrix(&sc->in, &coord->coord);
                    coord->coord.t[0] = work->field_6A4;
                    coord->coord.t[1] = work->field_6A8;
                    coord->coord.t[2] = work->field_6AC;
                    sc->in.vx         = 0;
                    sc->in.vy         = 0;
                    sc->in.vz         = 0x5AA;
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&sc->in);
                    gte_rtv0();
                    gte_stlvnl(&sc->out);
                    sc->place.pos.vx = coord->coord.t[0] + sc->out.vx;
                    sc->place.pos.vy = coord->coord.t[1] + sc->out.vy;
                    sc->place.pos.vz = coord->coord.t[2] + sc->out.vz;
                    sc->place.rot.vx = 0;
                    sc->place.rot.vy = work->field_6E6;
                    sc->place.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &sc->place, 0);
                    Gp_SpawnPadLerp(0xA, 0xFF, 0x80);
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(pcoord), (s8)worldCoordGetOriginAudioDepth(pcoord));
                } else {
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 1:
            sc->anim.source.sets          = D_actor_402200_8015415C;
            sc->anim.animationId          = 1;
            sc->anim.blend                = ANIMATION_BLEND_RESET;
            sc->anim.blendFrames          = 0;
            sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
            work->field_6CE = 2;
            work->field_6DC = 0x3C;
            work->field_6DA = 1;
            work->field_6DE = 0x1E;
            work->field_6B8 = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 2:
            if (work->field_6C4 >= 0x29) {
                work->field_6C0               = 2;
                work->field_6CE               = 3;
                work->field_6D6               = 0x1E;
                work->field_6D4               = 0;
                work->field_6F6               = 0;
                sc->anim.source.sets          = D_actor_402200_8015415C;
                sc->anim.animationId          = 2;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                Gp_ArmStateF0(1);
                work->field_70A = 0;
                if (work->field_6C6 == 0) {
                    work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->field_494  = work->field_716 | 0x30000;
                }
            }
            break;
        case 3:
            if (work->field_6D6 > 0) {
                work->field_6D6--;
            } else {
                if ((s16)work->field_6D4 == 2) {
                    Gp_SpawnPadLerp(5, 0xC0, 0x80);
                }
                timer           = work->field_6D4 - 1;
                work->field_6D4 = timer;
                if (timer <= 0) {
                    if (gPlayerStatus.hp > D_actor_402200_80153C0C[gSceneCombatState.difficulty]) {
                        if (work->field_6F8 == 0) {
                            work->field_6F8 = 1;
                        } else {
                            chance = work->field_6F6 * (0x32 - (gPlayerStatus.hp * 100) / gPlayerStatus.hpMax) / 2;
                            if (chance > 0) {
                                chance          = (chance * 0xFFF) / 100;
                                gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                                if ((s32)((gRandomLcgState >> 16) & 0xFFF) < chance) {
                                    flag = 1;
                                }
                            }
                        }
                    } else {
                        flag = 1;
                    }
                    if (flag != 0) {
                        work->field_6C0               = 3;
                        work->field_6CE               = 5;
                        sc->anim.source.sets          = D_actor_402200_8015415C;
                        sc->anim.animationId          = 3;
                        sc->anim.blend                = ANIMATION_BLEND_RESET;
                        sc->anim.blendFrames          = 0;
                        sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                    } else {
                        work->field_6D4 = 0x1E;
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(gGolemKnightBishopAttacks, 0), 0);
                        work->field_6F6++;
                    }
                }
            }
            if (work->field_6F4 != 0) {
                if (work->field_6F4 == 1) {
                    if (work->field_6D6 > 0 && work->field_6EE == 0) {
                        work->field_6C0 = 0x15;
                        work->field_6CE = 6;
                        work->field_71A = 0;
                        if (work->field_6B8 != 0) {
                            SndEvt_EnqueueType7(work->field_6B8, 1);
                            work->field_6B8 = 0;
                        }
                        work->field_6DA = 7;
                    } else {
                        work->field_6C0 = 0xC;
                        work->field_6CE = 4;
                        work->field_6D4 = 0x69;
                        work->field_6DA = 3;
                        work->field_6DC = 0x4B;
                        work->field_71A = 0;
                        work->field_6DE = 0x1E;
                        work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                        SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                    }
                } else {
                    golemKnightBishopPickHitReaction(arg0, work->field_70A);
                    work->field_71A = 0;
                }
                work->field_718               = 2;
                work->field_6F4               = 0;
                sc->anim.source.sets          = D_actor_402200_8015415C;
                sc->anim.animationId          = 4;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
            }
            break;
        case 4:
            val = 0;
            if (work->field_6C4 < 0x5F) {
                val = -0xA;
            }
            work->field_6C8 = val;
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
            }
            golemKnightBishopHoldCueTimer(arg0);
            break;
        case 5:
            if (work->field_6C4 < 0x1A) {
                if (work->field_6F4 != 0) {
                    work->field_6C0               = 0xC;
                    work->field_6F4               = 0;
                    sc->anim.source.sets          = D_actor_402200_8015415C;
                    sc->anim.animationId          = 4;
                    sc->anim.blend                = ANIMATION_BLEND_RESET;
                    sc->anim.blendFrames          = 0;
                    sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sc->anim, 0);
                    work->field_6D4 = 0x69;
                    work->field_6DA = 3;
                    work->field_6DC = 0x4B;
                    work->field_6CE = 4;
                    work->field_6DE = 0x1E;
                    work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            } else if (work->field_6C4 == 0x1A) {
                ((GameActor*)player->work)->state = 0xA;
                work->field_6CE                   = 7;
                work->field_6D4                   = 0;
                gGameSession->deathRestartDelay   = 0x5A;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                sc->in.vy                         = -0x96;
                sc->in.vx                         = 0;
                sc->in.vz                         = 0xC8;
                func_800FDB18(1, &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[4], &sc->in, &D_actor_402200_80154170);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                Gp_DispatchMsg(player, 0x400, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case 6:
            work->field_6C8 = -0xA;
            golemKnightBishopHoldCueTimer(arg0);
            if (work->field_718 == 0) {
                work->field_6C8 = 0;
                work->field_6CC = 4;
                work->field_6CE = 0;
            }
            break;
        case 7:
            sub = work->field_6D4;
            switch (sub) {
                case 0:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->field_6D4 = 1;
                    break;
                case 1:
                    if ((CdCmd_IsIdle() & 0xFFFF) == 1) {
                        coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                        SndEvt_EnqueueType6(0x70010001, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                        work->field_6D4 = 2;
                    }
                    break;
                case 2:
                    break;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopGrabScratch));
}

#include "../../shared/golem_knight_bishop_strike.inc.c"

/// Runs the actor's approach sequence off the box it last hit. State 0 plants
/// the display object on that box, faces it along the box heading and queues
/// the cue `field_6B8`, parking the state at 1 when the player is within 0xDAC
/// and at 2 otherwise. States 1 and 2 re-aim for `field_6D6` frames; state 2
/// closes in until the player is within 0xA8C (state 3) or turns away by more
/// than 0x180 (state 4). State 3 steps `field_6C8` through the frame table
/// `D_actor_402200_801383D8` and fires its per-frame events; state 4 counts
/// `field_6D4` down back to state 0.
void golemKnightBishopBoxApproachSeq(Task* arg0)
{
    u8*                             head;
    GolemKnightBishopOffsetScratch* sc;
    s32                             state;
    GolemKnightBishopWork*          work;
    GfxCoord*                       coord;
    s32                             pan;
    s32                             snd;
    s32                             i;
    s16                             diff;
    s16                             dist;
    s32                             adiff;
    s32                             val;
    s16                             timer;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopOffsetScratch);
    sc                       = (GolemKnightBishopOffsetScratch*)(head - sizeof(GolemKnightBishopOffsetScratch));
    work                     = arg0->work;
    state                    = work->field_6CE;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            coord->coord.t[0] = work->field_6B4[work->field_708].field_4;
            coord->coord.t[1] = gPlayerStatus.coordMtx->t[1];
            coord->coord.t[2] = work->field_6B4[work->field_708].field_6;
            sc->in.vx         = 0;
            sc->in.vy         = work->field_6B4[work->field_708].field_2;
            sc->in.vz         = 0;
            RotMatrix(&sc->in, &coord->coord);
            sc->out.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->out.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xDAC) {
                work->field_6C0 = 4;
                work->field_6CE = 1;
                work->field_6D4 = 0x1E;
            } else {
                work->field_6C0 = 6;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0x14;
            work->field_6DE = 0xA;
            work->field_6B8 = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_ArmStateF0(1);
            if (work->field_6C6 == 0) {
                work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_494  = work->field_716 | 0x30000;
            }
            work->field_6D6 = 0x14;
            work->field_70A = 0;
            work->field_6F2 = 1;
            break;
        case 1:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6C0 = 6;
                work->field_6CE = 2;
            }
            if (work->field_6D6 > 0) {
                work->field_6D6--;
                func_actor_402200_80135D5C(arg0);
            }
            if (work->field_70A >= 0xA0) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 2:
            val = 0;
            if (work->field_6C4 >= 8) {
                val = 0x78;
            }
            work->field_6C8 = val;
            if (work->field_6D6 > 0) {
                work->field_6D6--;
                func_actor_402200_80135D5C(arg0);
            }
            sc->out.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->out.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xA8C) {
                work->field_6C0  = 7;
                work->field_6CE  = 3;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            } else {
                diff = (ratan2((s16)sc->out.vx, (s16)sc->out.vz) & 0xFFF) - work->field_6B4[work->field_708].field_2;
                dist = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (dist > 0x180) {
                    work->field_6C0  = 4;
                    work->field_6CE  = 4;
                    work->field_6DA  = 3;
                    work->field_6DC  = 0x14;
                    work->field_6DE  = 0xA;
                    work->field_6F2  = 0;
                    work->field_6D4  = work->field_6DC + 0xA;
                    work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->field_6BC  = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            if (work->field_70A >= 0xA0) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 3:
            for (i = 0; work->field_6C4 > D_actor_402200_801383D8[i].frame; i++) {
            }
            work->field_6C8 = D_actor_402200_801383D8[i].value;
            if (work->field_6C4 == 0x12) {
                snd = gGolemKnightBishopStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_6C4 == 0x14) {
                work->field_56C  = arg0->extra.tmd->coords;
                work->field_576  = -0x4B0;
                work->field_578  = 0x1F4;
                work->field_574  = 0;
                work->field_580  = 0x3E8;
                work->field_57C  = Gp_PackPair(gGolemKnightBishopAttacks, 2);
                work->field_582 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->field_6C4 == 0x20) {
                work->field_6F2  = 0;
                work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->field_6C4 == 0x5A) {
                work->field_6DA = 3;
                work->field_6DC = 0x14;
                work->field_6DE = 0xA;
                work->field_6CE = 4;
                work->field_6D4 = work->field_6DC + 0xA;
                work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 4:
            work->field_6C8 = 0;
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}

/// Rolls the actor's cue countdown. State 0 puts the slot set on animation
/// 0xB and drops the state to 1, arming `field_6D4` from the `gRandomLcgState` LCG
/// (0x4B..0x6A); while no flinch is already running it also raises the hit
/// descriptor `field_494`/`field_49A`. State 1 ticks `field_6D4` down and, on
/// the frame it runs out, arms the `field_6DA`/`field_6DC`/`field_6DE`/
/// `field_6E0` timers, clears the state and `field_6CC`, and queues the actor's
/// cue, panned and depth-attenuated from the display object.
void golemKnightBishopRecoverSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    pan;
    u32                    random;
    s16                    timer;

    SCRATCH_STACK_RESERVE_BYTES(8);
    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_6C0 = 0xB;
            work->field_6CE = 1;
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            work->field_6D4 = (u16)(((random >> 16) & 0x1F) + 0x4B);
            if (work->field_6C6 == 0) {
                work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_494  = work->field_716 | 0x30000;
            }
            break;
        case 1:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6DA = 3;
                work->field_6DC = 0xA;
                work->field_6CC = 0;
                work->field_6CE = 0;
                work->field_6DE = 5;
                work->field_6E0 = 0;
                work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan             = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

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
    func_actor_402200_80137444,
    golemKnightBishopFrameState,
    golemKnightBishopDeadState,
};

#include "../../shared/golem_knight_bishop_collapse_death.inc.c"

#include "../../shared/golem_knight_bishop_anim_cues.inc.c"

/// Aims the actor off its fourth part. While `field_6D6` is positive the
/// offset (-0x28, -0x78, 0xDC) through the root-to-part matrix lands in
/// `field_634`..`field_638` with bits 0xC000 of `field_62A` raised. Below 0x13
/// it projects two points into `field_6FC`..`field_704`: the same offset off
/// the part, and a point 0x514 up and the `field_644` target's distance out
/// from the root.
static void func_actor_402200_80135D5C(Task* arg0)
{
    u8*                          head;
    GolemKnightBishopAimScratch* sc;
    GolemKnightBishopWork*       work;
    GfxCoord*                    coord;
    GfxCoord*                    part;
    s32                          i;
    s16                          dist;

    coord                    = arg0->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopAimScratch);
    sc                       = (GolemKnightBishopAimScratch*)(head - sizeof(GolemKnightBishopAimScratch));
    work                     = arg0->work;
    part                     = &coord[3] + 1;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    part->composeStamp       = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(part);
    if (work->field_6D6 > 0) {
        Gp_WorldToLocal(&coord->workm, &part->workm, &sc->m);
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&sc->m);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        work->field_634  = sc->m.t[0] + sc->out.vx;
        work->field_636  = sc->m.t[1] + sc->out.vy;
        work->field_638  = sc->m.t[2] + sc->out.vz;
        work->field_62A |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
    if (work->field_6D6 < 0x13) {
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&part->workm);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        sc->pts[1].vx = part->workm.t[0] + sc->out.vx;
        sc->pts[1].vy = part->workm.t[1] + sc->out.vy;
        sc->pts[1].vz = part->workm.t[2] + sc->out.vz;
        sc->pts[0].vx = 0;
        sc->pts[0].vy = -0x514;
        if (Gp_FindRec18(&work->field_644, 0) != 0) {
            sc->out.vx    = work->field_644.point.vx - sc->pts[1].vx;
            sc->out.vy    = work->field_644.point.vy - sc->pts[1].vy;
            sc->out.vz    = work->field_644.point.vz - sc->pts[1].vz;
            dist          = SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vy * sc->out.vy + sc->out.vz * sc->out.vz);
            sc->pts[0].vz = dist;
            if ((work->field_644.key.value & 0xFFFF0000) == 0x10000) {
                sc->pts[0].vz = dist + 0x12C;
            }
            Gp_ClearRec18Occupied(&work->field_644);
        } else {
            sc->pts[0].vz = 10000;
        }
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&sc->pts[0]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        sc->pts[0].vx = coord->workm.t[0] + sc->out.vx;
        sc->pts[0].vy = coord->workm.t[1] + sc->out.vy;
        sc->pts[0].vz = coord->workm.t[2] + sc->out.vz;
        for (i = 0; i < 2; i++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&sc->pts[i]);
            gte_rtps();
            gte_stsxy(&sc->sxy);
            gte_stszotz(&sc->otz);
            work->field_6FC[i] = sc->sxy;
            work->field_700[i] = sc->sxy >> 16;
            work->field_704[i] = sc->otz;
        }
        golemKnightBishopDrawAimBeam(arg0);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopAimScratch));
}

#include "../../shared/golem_knight_bishop_aim_beam.inc.c"

#include "../../shared/golem_knight_bishop_inlines.inc.c"

#include "../../shared/golem_knight_bishop_dead.inc.c"

#include "../../shared/frame_capture.inc.c"

/// Spawn handler. Allocates the 0x71C-byte work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `spawnState`. State 0 is the full setup: it links the
/// enemy node, picks the box table and count for the current stage / room out
/// of `D_actor_402200_80153C78`, requests the room's cue bank, and links the
/// work block's five collision objects with their `WorldCollisionContact` tables before
/// moving the task on (`field_30` 1). States 1 and 2 only seed the animation
/// and sequence state.
static void func_actor_402200_80137444(Enemy* arg0, Task* arg1)
{
    u8                     param1[4];
    u8                     param2[4];
    GolemKnightBishopWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    s16*                   cues;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    WorldCollisionContact* records4;
    WorldCollisionContact* records5;
    s32                    i;
    s32                    kind;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x71C, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work                 = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_65C.coord      = &arg1->extra.tmd->coords[3];
    work->field_65C.spawnArgLo = 0x500;
    work->field_65C.spawnArgHi = 2;
    func_800B3F84(&work->rig.anim, D_actor_402200_80154194, obj, work->rig.poses, work->rig.slots);
    work->field_6C0 = 0xB;
    work->field_6C2 = 0xB;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->rig.anim, i, work->field_6C0);
    }
    kind = arg0->spawnState;
    switch (kind) {
        case 0:
            work->field_6D8         = 0xFF;
            obj->shading.colorBlend = 0;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = -1;
            arg0->field_4   = &coord->coord;
            arg0->field_48  = 0;
            Gp_LinkNode(&arg0->node);
            arg0->coord      = &arg1->extra.tmd->coords[3];
            arg0->bodyPos.vx = 0;
            arg0->bodyPos.vy = 0;
            arg0->bodyPos.vz = 0;
            arg0->param      = &D_actor_402200_80153BFC;
            arg0->recs       = work->field_49C;
            arg0->hp         = D_actor_402200_80153BFC.hpMax;
            for (i = 0; D_actor_402200_80153C78[i].field_0 != 0; i++) {
                if (gGameSession->location.loc.stage == D_actor_402200_80153C78[i].field_2 && gGameSession->location.loc.area == D_actor_402200_80153C78[i].field_4) {
                    work->field_6B4 = D_actor_402200_80153FA8[D_actor_402200_80153C78[i].field_0];
                    work->field_6FA = D_actor_402200_80153C78[i].field_6;
                }
            }
            work->field_6CC = 0xB;
            (Gp_IncStateF0Ref)(0);
            work->field_716 = 0x16;
            cues            = D_actor_402200_80154144[gGameSession->location.loc.stage];
            if (cues != NULL) {
                work->field_712 = cues[gGameSession->location.loc.area];
            }
            if (work->field_712 != 0) {
                param1[3] = 0;
                param1[2] = 0x28;
                param1[0] = work->field_712;
                param2[0] = 0x16;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
            }
            work->field_484 = &arg1->extra.tmd->coords[3];
            records1        = work->field_49C;
            work->field_488 = records1;
            work->field_48C = 0;
            work->field_48E = 0;
            work->field_490 = 0;
            work->field_494 = 0x30016;
            work->field_498 = 0x15E;
            work->field_49A = 1;
            Gp_LinkObj(2, (WorldCollisionBody*)work->field_47C);
            Gp_InitRec18Table(records1, 3, 0);
            work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->field_4EC  = arg1->extra.tmd->coords;
            records2         = work->field_504;
            work->field_4F0  = records2;
            work->field_4F4  = 0;
            work->field_4F6  = -0x1F4;
            work->field_4F8  = 0;
            work->field_4FC  = 0x30016;
            work->field_500  = 0x1F4;
            work->field_502  = 1;
            Gp_LinkObj(2, (WorldCollisionBody*)work->field_4E4);
            Gp_InitRec18Table(records2, 4, 0);
            work->field_502 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_56C  = &arg1->extra.tmd->coords[8];
            records3         = &work->field_584;
            work->field_570  = records3;
            work->field_574  = 0;
            work->field_576  = 0;
            work->field_578  = 0;
            work->field_57C  = Gp_PackPair(gGolemKnightBishopAttacks, 1);
            work->field_580  = 0x12C;
            work->field_582  = 1;
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_564);
            Gp_InitRec18Table(records3, 1, 0);
            work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_5DC  = 0;
            work->field_5DE  = -0x3E8;
            work->field_5E0  = -0x7D0;
            work->field_5E4  = 0;
            work->field_5E6  = -0x3E8;
            work->field_5E8  = 0;
            work->field_5EC  = 0x1F4;
            work->field_5EE  = 0x1F4;
            records4         = &work->field_5F4;
            work->field_5F0  = records4;
            work->field_5A4  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->field_5A8  = &work->field_5DC;
            work->field_5AC  = 0;
            work->field_5AE  = 0;
            work->field_5B0  = 0;
            work->field_5B4  = 0;
            work->field_5B8  = 0;
            work->field_5BA  = 3;
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_59C);
            Gp_InitRec18Table(records4, 1, 0);
            work->field_5BA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_5C4  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            work->field_5C8  = records4;
            work->field_5CC  = 0;
            work->field_5CE  = -0x320;
            work->field_5D0  = -0x5AA;
            work->field_5D4  = 0;
            work->field_5D8  = 0x1F4;
            work->field_5DA  = 1;
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_5BC);
            work->field_62C  = 0;
            work->field_62E  = -0x514;
            work->field_630  = 0x2710;
            work->field_634  = 0;
            work->field_636  = 0;
            work->field_638  = 0;
            work->field_63C  = 1;
            work->field_63E  = 1;
            records5         = &work->field_644;
            work->field_640  = records5;
            work->field_614  = coord;
            work->field_618  = &work->field_62C;
            work->field_61C  = 0;
            work->field_61E  = 0;
            work->field_620  = 0;
            work->field_624  = 0;
            work->field_628  = 0;
            work->field_62A  = 3;
            work->field_5DA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_LinkObj(3, (WorldCollisionBody*)work->field_60C);
            Gp_InitRec18Table(records5, 1, 0);
            work->field_62A = (work->field_62A & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | 0xC00;
            arg1->msgTable  = D_actor_402200_8013839C;
            arg1->state     = 1;
            break;
        case 1:
            work->field_6C0         = 0x10;
            work->field_6CE         = 2;
            arg1->state             = 2;
            work->field_6D8         = 0;
            obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
        case 2:
            work->field_6C0         = 0x14;
            work->field_6CE         = kind;
            arg1->state             = kind;
            work->field_6D8         = 0;
            obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
    }
}

#include "../../shared/golem_knight_bishop_frame.inc.c"

#include "../../shared/golem_knight_bishop_run_sequence.inc.c"

#include "../../shared/golem_knight_bishop_apply_scale.inc.c"

#include "../../shared/golem_knight_bishop_kneel_death.inc.c"

#include "../../shared/golem_knight_bishop_step_forward.inc.c"

/// 1BC.h keeps this out of scope on purpose: callers hand it a sign-extended
/// animation id, which a `u16` prototype would zero-extend.
/// Reseeds animation slots 1..0x12 when the actor's animation id changes,
/// handing each slot the blend weight the id selects from
/// `gGolemKnightBishopAnimBlend`; while the id is unchanged it instead ticks every
/// slot one frame and walks the id's frame counter up.
void golemKnightBishopTickAnim(Task* arg0)
{
    golemKnightBishopTickAnimInline(arg0);
}

/// Out-of-line `golemKnightBishopUpdateTintInline`, for the callers after the inline one.
void golemKnightBishopUpdateTint(Task* arg0)
{
    golemKnightBishopUpdateTintInline(arg0);
}

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
