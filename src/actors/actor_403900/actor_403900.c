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

extern Actor402200FrameStep D_actor_403900_801383DC[];

/// Per-roll wait lengths the wait state of `golemKnightBishopIdleSeq`
/// scales by `16 - field_70C`, indexed by a 4-bit `gRandomLcgState` draw.
extern s16 D_actor_403900_80153C3C[];

/// Per-roll state offsets the wait state adds to 2 when `field_6E8` is set.
extern u16 D_actor_403900_80153C5C[];

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

/// Cue-id table: `Actor402200Work::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `flags` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one; the branch sequences read `[field_712 + 8]`.
extern s32 gGolemKnightBishopAnimCues[];

/// Weighted 16-entry roll for `Actor402200Work::field_6E4`: indices 0-10 hold 0
/// and 11-15 hold 1, so the short approach is taken about a third of the time.
extern u16 gGolemKnightBishopApproachRoll[];

/// Per-animation-id value `golemKnightBishopTickAnim` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 gGolemKnightBishopAnimBlend[];

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
} Actor403900MessageEntry;
STATIC_ASSERT_SIZEOF(Actor403900MessageEntry, 8);

extern Actor403900MessageEntry D_actor_403900_801383A0[2];
extern DamageAttack            gGolemKnightBishopAttacks[4];
extern EnemyParams             D_actor_403900_80153C00;
extern Actor402200Spot         D_actor_403900_80153C7C[];
extern Actor402200Region*      D_actor_403900_80153F04[];
extern s16*                    D_actor_403900_8015409C[];
extern AnimationSet*           D_actor_403900_801540EC[22];

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 D_actor_403900_80153C10[];

/// Animation block the grab's 0x3FF messages hand the player.
extern AnimationSet* D_actor_403900_801540B4[5];

/// `func_800FDB18` argument record for the grab's finishing spark.
extern EffectSpawnArg D_actor_403900_801540C8;

/// The two four-vertex index rows the trail's shaded quads take their corners
/// from, into the scratch block's six-entry x / y runs.
extern s16 gGolemKnightBishopBeamQuadCorners[2][4];

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_403900_80135D5C(Task* arg0);
static void func_actor_403900_80137444(Enemy* arg0, Task* arg1);

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

Actor403900MessageEntry D_actor_403900_801383A0[2] = {
    { 2014, { .call0 = func_actor_403900_801381E4 } },
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

Actor402200FrameStep D_actor_403900_801383DC[18] = {
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
#include "assets/actor_403900_model_0BDB8_skeleton.inc"
};

u32 D_actor_403900_80138728[19] = {
#include "assets/actor_403900_model_0BDB8_partVerts.inc"
};

SVECTOR D_actor_403900_80138774[363] = {
#include "assets/actor_403900_model_0BDB8_verts.inc"
};

SVECTOR D_actor_403900_801392CC[345] = {
#include "assets/actor_403900_model_0BDB8_normals.inc"
};

u32 D_actor_403900_80139D94[3985] = {
#include "assets/actor_403900_model_0BDB8_stream.inc"
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

EnemyParams D_actor_403900_80153C00 = { gGolemKnightBishopAttacks, 800, 400, 2500, 7, 100, 20, 0, 0 };

s16 D_actor_403900_80153C10[6] = {
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

s16 D_actor_403900_80153C3C[16] = {
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

u16 D_actor_403900_80153C5C[16] = {
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

Actor402200Spot D_actor_403900_80153C7C[9] = {
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

Actor402200Region D_actor_403900_80153CC4[5] = {
    { 0, 1000, 2500, 4000, 0, 0, 0, 0 },
    { 1, 1024, 2500, 4300, 0x28A0, 5200, 0x32C8, 3400 },
    { 1, 2048, 0x2904, 5700, 0x2710, 1500, 0x2AF8, 0 },
    { 1, 0, 6450, 1300, 5900, 5400, 7000, 4000 },
    { 1, 0, 1700, -500, 1000, 5800, 2400, 3200 },
};

Actor402200Region D_actor_403900_80153D14[6] = {
    { 0, 1000, 7000, -3500, 0, 0, 0, 0 },
    { 0, 2000, 7000, 6000, 0, 0, 0, 0 },
    { 1, 0, 800, 1000, 0, 6000, 1550, 3000 },
    { 1, 3072, 7500, 4500, 1550, 6000, 4000, 4000 },
    { 1, 2048, 7000, 5000, 5000, -4000, 9000, -5000 },
    { 1, 1024, 500, 4500, 6000, 6000, 8000, 4000 },
};

Actor402200Region D_actor_403900_80153D74[3] = {
    { 0, 3000, 7000, -1600, 0, 0, 0, 0 },
    { 0, 2000, 1600, -7300, 0, 0, 0, 0 },
    { 1, 2048, 1600, -1600, 1000, -7000, 2500, -8000 },
};

Actor402200Region D_actor_403900_80153DA4[3] = {
    { 0, 2000, 7000, 3000, 0, 0, 0, 0 },
    { 1, 1024, -1000, 2750, 1500, 3500, 3500, 2000 },
    { 1, 3072, 2500, 500, -3500, 1000, -2000, 0 },
};

Actor402200Region D_actor_403900_80153DD4[8] = {
    { 0, 2000, 1600, -1400, 0, 0, 0, 0 },
    { 0, 1000, 0x2710, 4200, 0, 0, 0, 0 },
    { 1, 3072, 0x2A94, 4200, 1000, 5000, 5000, 3500 },
    { 1, 3072, 5500, 2500, 1000, 3500, 3500, 1500 },
    { 1, 0, 8950, 1600, 8000, 5800, 9500, 3000 },
    { 1, 1024, 5500, 4000, 0x2AF8, 4500, 0x3070, 3000 },
    { 1, 2048, 1700, 4700, 1000, 0, 2400, -2900 },
    { 1, 1800, 1400, 4700, 2400, 1200, 3500, 0 },
};

Actor402200Region D_actor_403900_80153E54[5] = {
    { 0, 800, 0x2710, 5300, 0, 0, 0, 0 },
    { 0, 800, 7500, 8000, 0, 0, 0, 0 },
    { 0, 800, 4000, 8000, 0, 0, 0, 0 },
    { 1, 1024, 1200, 8000, 6000, 9000, 7500, 7000 },
    { 1, 3072, 0x283C, 8000, 4000, 9000, 6000, 7000 },
};

Actor402200Region D_actor_403900_80153EA4[2] = {
    { 0, 600, -1900, 4800, 0, 0, 0, 0 },
    { 1, 3072, 2000, 4800, -5500, 5500, -1900, 4000 },
};

Actor402200Region D_actor_403900_80153EC4[4] = {
    { 0, 600, 3500, -1800, 0, 0, 0, 0 },
    { 1, 3072, 8500, -1800, 0, -1000, 2000, -2000 },
    { 1, 0, 7750, -7500, 6500, -1000, 9000, -2000 },
    { 1, 2048, 7750, -3000, 6500, -6000, 9000, -9000 },
};

Actor402200Region* D_actor_403900_80153F04[9] = {
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

s16* D_actor_403900_8015409C[6] = {
    NULL,
    D_actor_403900_80153F28,
    D_actor_403900_80153F54,
    D_actor_403900_80153FA4,
    D_actor_403900_80153FF4,
    D_actor_403900_80154058,
};

AnimationSet* D_actor_403900_801540B4[5] = {
    NULL,
    &D_actor_403900_80151998,
    &D_actor_403900_80151F2C,
    &D_actor_403900_8015298C,
    &D_actor_403900_80153BC8,
};

EffectSpawnArg D_actor_403900_801540C8 = { NULL, 300, 1 };

s16 gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_403900_801540E0 = { { { TASK_BODY_TMD, 96 } }, func_actor_403900_80138344, { .model = &D_actor_403900_8013DBD8 } };

AnimationSet* D_actor_403900_801540EC[22] = {
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

/// Per-frame hit handler: applies the `func_800E0C10` push-back from the
/// `field_504` and (while `field_49A` enables grid tests) `field_49C`
/// record tables to the root coordinate, ticks the `field_6C6` flinch
/// countdown, and for each kind-2 hit record in `field_49C` computes the
/// damage from the distance to the player, applies it to the `Enemy`,
/// spawns the hit sparks once per distinct id and hands the damage to
/// `golemKnightBishopPickHitReaction` unless the vocal cue is armed.
void golemKnightBishopTakeHits(Task* arg0)
{
    s32                    lastId;
    Actor402200Work*       work;
    Actor402200HitScratch* head;
    Actor402200HitScratch* sc;
    Actor402200HitScratch* blk;
    Enemy*                 enemy;
    GfxCoord*              coord;
    s32                    i;
    s32                    damage;
    s32                    kind;
    s32                    wait;
    s16                    t;

    lastId                                      = 0;
    work                                        = arg0->work;
    head                                        = SCRATCH_STACK_CURSOR(Actor402200HitScratch);
    blk                                         = head - 1;
    SCRATCH_STACK_CURSOR(Actor402200HitScratch) = blk;
    sc                                          = blk;
    coord                                       = arg0->extra.tmd->coords;
    enemy                                       = arg0->spawnArg2.pointer;

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
                        work->field_70A += 0xFA;
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
    SCRATCH_STACK_RELEASE_BLOCK(Actor402200HitScratch);
}

#include "../../shared/golem_knight_bishop_hit_reaction.inc.c"

#include "../../shared/golem_knight_bishop_box_scan.inc.c"

/// State machine on `field_6CE`: 0 rolls a `field_6D4` wait, 1 counts it
/// down, 2 picks state 3 or 4 from `field_70E` and an LCG draw offset by
/// `field_710` (or 5 when `golemKnightBishopPlayerInBox` reports a box hit), and 3-5
/// settle the result, walking `field_70C` up to 8.
void golemKnightBishopIdleSeq(Task* arg0)
{
    Actor402200Work* work;

    work = arg0->work;
    switch (work->field_6CE) {
        case 0:
            work->field_6C8 = 0;
            work->field_6EC = 0;
            work->field_6EE = 0;
            if (work->field_6E8 != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6CE = D_actor_403900_80153C5C[(gRandomLcgState >> 16) & 0xF] + 2;
                work->field_6EE = 1;
                golemKnightBishopPlaceTarget(arg0);
                work->field_6E4 = 0;
            } else if (work->field_6E4 == 0) {
                work->field_6D4 = (D_actor_403900_80153C3C[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF] * (0x10 - work->field_70C)) / 16;
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
                if (work->field_70C < 8) {
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
                if (work->field_70C < 8) {
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
            if (work->field_70C < 8) {
                work->field_70C++;
            }
            break;
    }
}

#include "../../shared/golem_knight_bishop_player_in_box.inc.c"

#include "../../shared/golem_knight_bishop_place_target.inc.c"

/// Runs the actor's hold sequence on the player (the same 0x3F8 / 0x3FF
/// message pair `golemKnightBishopGrabSeq` uses to take a hold). State 0
/// asks the player for range 0x19 while enemies remain; on success it plants
/// the display object at `field_6A4`, places the player 0x5AA in front of it
/// with message 0x3E9 and queues a cue. States 1 and 2 step the player's
/// animation. State 3 waits out `field_6D6`, then every 0x14 frames decides
/// whether the hold ends: always when `gPlayerStatus.hp` is above the
/// per-difficulty `D_actor_403900_80153C10`, otherwise by an LCG roll whose
/// chance grows with the attempt count `field_6F6`; a raised `field_6F4`
/// ends it early. State 5 either reacts to `field_6F4` or, at frame 0x1A,
/// spawns the spark, sends message 0x400 and clears `gPlayerStatus.hp`; state 7
/// then loads file 9/0x1E and queues cue 0x70010001 once the CD is idle.
void golemKnightBishopGrabSeq(Task* arg0)
{
    Actor402200Work*        work;
    GfxCoord*               coord;
    Task*                   player;
    Actor402200GrabScratch* sc;
    GfxCoord*               pcoord;
    s32                     flag;
    s32                     snd;
    s32                     chance;
    u32                     random;
    s16                     timer;
    s16                     val;
    s16                     sub;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor402200GrabScratch));
    sc     = SCRATCH_STACK_CURSOR(Actor402200GrabScratch);
    pcoord = player->extra.tmd->coords;
    flag   = 0;
    switch (work->field_6CE) {
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED && gPlayerStatus.hp > 0) {
                sc->query.field_14 = 0x19;
                if (Gp_DispatchMsgPtr(player, 0x3F8, sc, 0) == 0) {
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
                    Gp_DispatchMsgPtr(player, 0x3E9, &sc->place, 0);
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
            sc->anim.source.sets          = D_actor_403900_801540B4;
            sc->anim.animationId          = 1;
            sc->anim.blend                = ANIMATION_BLEND_RESET;
            sc->anim.blendFrames          = 0;
            sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
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
                sc->anim.source.sets          = D_actor_403900_801540B4;
                sc->anim.animationId          = 2;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
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
                    if (gPlayerStatus.hp > D_actor_403900_80153C10[gSceneCombatState.difficulty]) {
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
                        sc->anim.source.sets          = D_actor_403900_801540B4;
                        sc->anim.animationId          = 3;
                        sc->anim.blend                = ANIMATION_BLEND_RESET;
                        sc->anim.blendFrames          = 0;
                        sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
                    } else {
                        work->field_6D4 = 0x14;
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
                sc->anim.source.sets          = D_actor_403900_801540B4;
                sc->anim.animationId          = 4;
                sc->anim.blend                = ANIMATION_BLEND_RESET;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
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
                    sc->anim.source.sets          = D_actor_403900_801540B4;
                    sc->anim.animationId          = 4;
                    sc->anim.blend                = ANIMATION_BLEND_RESET;
                    sc->anim.blendFrames          = 0;
                    sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
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
                func_800FDB18(1, &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[4], &sc->in, &D_actor_403900_801540C8);
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
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor402200GrabScratch));
}

#include "../../shared/golem_knight_bishop_strike.inc.c"

/// Runs the actor's approach sequence off the box it last hit. State 0 plants
/// the display object on that box, faces it along the box heading and queues
/// the cue `field_6B8`, parking the state at 1 when the player is within 0xDAC
/// and at 2 otherwise. States 1 and 2 re-aim for `field_6D6` frames; state 2
/// closes in until the player is within 0xA8C (state 3) or turns away by more
/// than 0x180 (state 4). State 3 steps `field_6C8` through the frame table
/// `D_actor_403900_801383DC` and fires its per-frame events; state 4 counts
/// `field_6D4` down back to state 0.
void golemKnightBishopBoxApproachSeq(Task* arg0)
{
    u8*                       head;
    Actor402200OffsetScratch* sc;
    s32                       state;
    Actor402200Work*          work;
    GfxCoord*                 coord;
    s32                       pan;
    s32                       snd;
    s32                       i;
    s16                       diff;
    s16                       dist;
    s32                       adiff;
    s32                       val;
    s16                       timer;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(Actor402200OffsetScratch);
    sc                       = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
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
            work->field_6D6 = 0xA;
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
                func_actor_403900_80135D5C(arg0);
            }
            if (work->field_70A >= 0xFA) {
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
                func_actor_403900_80135D5C(arg0);
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
            if (work->field_70A >= 0xFA) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 3:
            for (i = 0; work->field_6C4 > D_actor_403900_801383DC[i].frame; i++) {
            }
            work->field_6C8 = D_actor_403900_801383DC[i].value;
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
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor402200OffsetScratch));
}

/// Cue body of the enemy's attack: state 0 arms animation `field_6C0`, sets
/// the cue state and rolls the countdown `field_6D4` from `gRandomLcgState`,
/// raising the hit descriptor `field_494`/`field_49A` while no flinch is
/// already running. State 1 ticks the countdown down and, on the frame it
/// runs out, arms the `field_6DA`/`field_6DC`/`field_6DE`/`field_6E0` timers,
/// clears the cue state and `field_6CC`, and queues the actor's cue, panned
/// and depth-attenuated from the display object.
void golemKnightBishopRecoverSeq(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              pan;
    u32              random;
    s16              timer;

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
            work->field_6D4 = (u16)(((random >> 16) & 0x1F) + 0x2D);
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

/// The enemy task's three state handlers, which `func_actor_403900_80138344`
/// picks by `Task::state`: the spawn setup, the frame handler that runs the
/// sequences, and the frame handler that unlinks the enemy and saves its pose
/// before running its own short sequence.
static const GpEnemyTaskFuncTable3 D_actor_403900_80131F18 = {
    func_actor_403900_80137444,
    golemKnightBishopFrameState,
    golemKnightBishopDeadState,
};

#include "../../shared/golem_knight_bishop_collapse_death.inc.c"

#include "../../shared/golem_knight_bishop_anim_cues.inc.c"

/// Aims the actor: brings the root coordinate local to the fourth part to park
/// the aim point in the work block, then resolves the ground record under the
/// muzzle and projects both world points to screen for the draw step.
static void func_actor_403900_80135D5C(Task* arg0)
{
    u8*                    head;
    Actor402200AimScratch* sc;
    Actor402200Work*       work;
    GfxCoord*              coord;
    GfxCoord*              part;
    s32                    i;
    s16                    dist;

    coord                    = arg0->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(Actor402200AimScratch);
    sc                       = (Actor402200AimScratch*)(head - sizeof(Actor402200AimScratch));
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
    if (work->field_6D6 < 9) {
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
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor402200AimScratch));
}

#include "../../shared/golem_knight_bishop_aim_beam.inc.c"

#include "../../shared/golem_knight_bishop_inlines.inc.c"

#include "../../shared/golem_knight_bishop_dead.inc.c"

#include "../../shared/frame_capture.inc.c"

/// Spawn handler. Allocates the 0x71C-byte work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `spawnState`. State 0 is the full setup: it links the
/// enemy node, picks the box table and count for the current stage / room out
/// of `D_actor_403900_80153C7C`, requests the room's cue bank, and links the
/// work block's five collision objects with their `WorldCollisionContact` tables before
/// moving the task on (`field_30` 1). States 1 and 2 only seed the animation
/// and sequence state.
static void func_actor_403900_80137444(Enemy* arg0, Task* arg1)
{
    u8                     param1[4];
    u8                     param2[4];
    Actor402200Work*       work;
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
    func_800B3F84(&work->rig.anim, D_actor_403900_801540EC, obj, work->rig.poses, work->rig.slots);
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
            arg0->param      = &D_actor_403900_80153C00;
            arg0->recs       = work->field_49C;
            arg0->hp         = D_actor_403900_80153C00.hpMax;
            for (i = 0; D_actor_403900_80153C7C[i].field_0 != 0; i++) {
                if (gGameSession->location.loc.stage == D_actor_403900_80153C7C[i].field_2 && gGameSession->location.loc.area == D_actor_403900_80153C7C[i].field_4) {
                    work->field_6B4 = D_actor_403900_80153F04[D_actor_403900_80153C7C[i].field_0];
                    work->field_6FA = D_actor_403900_80153C7C[i].field_6;
                }
            }
            work->field_6CC = 0xB;
            (Gp_IncStateF0Ref)(0);
            work->field_716 = 0x27;
            cues            = D_actor_403900_8015409C[gGameSession->location.loc.stage];
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
            work->field_494 = 0x30027;
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
            work->field_4FC  = 0x30027;
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
            arg1->msgTable  = D_actor_403900_801383A0;
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

/// Reseeds animation slots 1..0x12 when the actor's animation id changes,
/// handing each slot the blend weight the id selects from
/// `gGolemKnightBishopAnimBlend`; while the id is unchanged it instead ticks every
/// slot one frame and walks the id's frame counter up.
void golemKnightBishopTickAnim(Task* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = gGolemKnightBishopAnimBlend[work->field_6C0];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->field_6C0, 0, value);
        }
    } else {
        work->field_6C4++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
}

/// Out-of-line `actor402200UpdateTint`, for the callers after the inline one.
void golemKnightBishopUpdateTint(Task* arg0)
{
    actor402200UpdateTint(arg0);
}

#include "../../shared/golem_knight_bishop_shadow.inc.c"

#include "../../shared/golem_knight_bishop_hold_cue.inc.c"

/// Raises the actor's phase `field_6F4` to 1 while enemies remain.
s32 func_actor_403900_801381E4(Task* task)
{
    if (gPlayerStatus.hp > 0) {
        ((Actor402200Work*)task->work)->field_6F4 = 1;
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
