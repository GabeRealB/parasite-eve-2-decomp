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

extern TaskMessageEntry         gGolemKnightBishopMessages[2];
extern DamageAttack             gGolemKnightBishopAttacks[4];
extern EnemyParams              gGolemKnightBishopParams;
extern GolemKnightBishopSpot    gGolemKnightBishopSpots[];
extern GolemKnightBishopRegion* gGolemKnightBishopRegions[];
extern s16*                     gGolemKnightBishopStageCues[];
extern AnimationSet*            gGolemKnightBishopAnimSets[22];

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

s32 func_actor_403900_801381E4(Task*, s32, s32, s32);

static AnimationSet _gActor403900Animation0C44C;
static AnimationSet _gActor403900Animation0C904;
static AnimationSet _gActor403900Animation0D110;
static AnimationSet _gActor403900Animation0DD48;
static AnimationSet _gActor403900Animation0EDFC;
static AnimationSet _gActor403900Animation0F9A8;
static AnimationSet _gActor403900Animation13A20;
static AnimationSet _gActor403900Animation13F44;
static AnimationSet _gActor403900Animation1498C;
static AnimationSet _gActor403900Animation1567C;
static AnimationSet _gActor403900Animation16FB8;
static AnimationSet _gActor403900Animation17DC8;
static AnimationSet _gActor403900Animation18E40;
static AnimationSet _gActor403900Animation19314;
static AnimationSet _gActor403900Animation19624;
static AnimationSet _gActor403900Animation19800;
static AnimationSet _gActor403900Animation1A748;
static AnimationSet _gActor403900Animation1ACDC;
static AnimationSet _gActor403900Animation1AFA4;
static AnimationSet _gActor403900Animation1B180;
static AnimationSet _gActor403900Animation1E7A8;
static AnimationSet _gActor403900Animation1FB78;
static AnimationSet _gActor403900Animation2010C;
static AnimationSet _gActor403900Animation20B6C;
static AnimationSet _gActor403900Animation21DA8;
extern DamageAttack gGolemKnightBishopAttacks[4];
static TmdSource    _gActor403900GolemBody;
static void         func_actor_403900_80138344(Task*);

TaskMessageEntry gGolemKnightBishopMessages[2] = {
    { 2014, func_actor_403900_801381E4 },
    { TASK_MESSAGE_TABLE_END, NULL },
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

static TmdBone _gActor403900GolemBodySkeleton[19] = {
#include "assets/golem_body_skeleton.inc"
};

static u32 _gActor403900GolemBodyPartVerts[19] = {
#include "assets/golem_body_partVerts.inc"
};

static SVECTOR _gActor403900GolemBodyVerts[363] = {
#include "assets/golem_body_verts.inc"
};

static SVECTOR _gActor403900GolemBodyNormals[345] = {
#include "assets/golem_body_normals.inc"
};

static u32 _gActor403900GolemBodyStream[3985] = {
#include "assets/golem_body_stream.inc"
};

static TmdSource _gActor403900GolemBody = {
    0,
    43904,
    11888,
    19,
    _gActor403900GolemBodyPartVerts,
    _gActor403900GolemBodyVerts,
    _gActor403900GolemBodyNormals,
    _gActor403900GolemBodySkeleton,
    _gActor403900GolemBodyStream,
};

static AnimationPackedPose _gActor403900Animation0C44CBank1[12] = {
#include "assets/actor_403900_animation_0C44C_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation0C44CBank4[165] = {
#include "assets/actor_403900_animation_0C44C_bank4.inc"
};

static AnimationRecord _gActor403900Animation0C44CRecords[201] = {
#include "assets/actor_403900_animation_0C44C_records.inc"
};

static u16 _gActor403900Animation0C44CIndices[20] = {
#include "assets/actor_403900_animation_0C44C_indices.inc"
};

static AnimationSet _gActor403900Animation0C44C = {
    _gActor403900Animation0C44CRecords,
    _gActor403900Animation0C44CIndices,
    { NULL, _gActor403900Animation0C44CBank1, NULL, NULL, _gActor403900Animation0C44CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation0C904Bank1[7] = {
#include "assets/actor_403900_animation_0C904_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation0C904Bank4[87] = {
#include "assets/actor_403900_animation_0C904_bank4.inc"
};

static AnimationRecord _gActor403900Animation0C904Records[174] = {
#include "assets/actor_403900_animation_0C904_records.inc"
};

static u16 _gActor403900Animation0C904Indices[20] = {
#include "assets/actor_403900_animation_0C904_indices.inc"
};

static AnimationSet _gActor403900Animation0C904 = {
    _gActor403900Animation0C904Records,
    _gActor403900Animation0C904Indices,
    { NULL, _gActor403900Animation0C904Bank1, NULL, NULL, _gActor403900Animation0C904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation0D110Bank1[15] = {
#include "assets/actor_403900_animation_0D110_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation0D110Bank4[197] = {
#include "assets/actor_403900_animation_0D110_bank4.inc"
};

static AnimationRecord _gActor403900Animation0D110Records[253] = {
#include "assets/actor_403900_animation_0D110_records.inc"
};

static u16 _gActor403900Animation0D110Indices[20] = {
#include "assets/actor_403900_animation_0D110_indices.inc"
};

static AnimationSet _gActor403900Animation0D110 = {
    _gActor403900Animation0D110Records,
    _gActor403900Animation0D110Indices,
    { NULL, _gActor403900Animation0D110Bank1, NULL, NULL, _gActor403900Animation0D110Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation0DD48Bank1[21] = {
#include "assets/actor_403900_animation_0DD48_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation0DD48Bank4[317] = {
#include "assets/actor_403900_animation_0DD48_bank4.inc"
};

static AnimationRecord _gActor403900Animation0DD48Records[382] = {
#include "assets/actor_403900_animation_0DD48_records.inc"
};

static u16 _gActor403900Animation0DD48Indices[20] = {
#include "assets/actor_403900_animation_0DD48_indices.inc"
};

static AnimationSet _gActor403900Animation0DD48 = {
    _gActor403900Animation0DD48Records,
    _gActor403900Animation0DD48Indices,
    { NULL, _gActor403900Animation0DD48Bank1, NULL, NULL, _gActor403900Animation0DD48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation0EDFCBank1[29] = {
#include "assets/actor_403900_animation_0EDFC_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation0EDFCBank4[451] = {
#include "assets/actor_403900_animation_0EDFC_bank4.inc"
};

static AnimationRecord _gActor403900Animation0EDFCRecords[511] = {
#include "assets/actor_403900_animation_0EDFC_records.inc"
};

static u16 _gActor403900Animation0EDFCIndices[20] = {
#include "assets/actor_403900_animation_0EDFC_indices.inc"
};

static AnimationSet _gActor403900Animation0EDFC = {
    _gActor403900Animation0EDFCRecords,
    _gActor403900Animation0EDFCIndices,
    { NULL, _gActor403900Animation0EDFCBank1, NULL, NULL, _gActor403900Animation0EDFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation0F9A8Bank1[30] = {
#include "assets/actor_403900_animation_0F9A8_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation0F9A8Bank4[277] = {
#include "assets/actor_403900_animation_0F9A8_bank4.inc"
};

static AnimationRecord _gActor403900Animation0F9A8Records[360] = {
#include "assets/actor_403900_animation_0F9A8_records.inc"
};

static u16 _gActor403900Animation0F9A8Indices[20] = {
#include "assets/actor_403900_animation_0F9A8_indices.inc"
};

static AnimationSet _gActor403900Animation0F9A8 = {
    _gActor403900Animation0F9A8Records,
    _gActor403900Animation0F9A8Indices,
    { NULL, _gActor403900Animation0F9A8Bank1, NULL, NULL, _gActor403900Animation0F9A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation13A20Bank1[111] = {
#include "assets/actor_403900_animation_13A20_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation13A20Bank4[1789] = {
#include "assets/actor_403900_animation_13A20_bank4.inc"
};

static AnimationRecord _gActor403900Animation13A20Records[1984] = {
#include "assets/actor_403900_animation_13A20_records.inc"
};

static u16 _gActor403900Animation13A20Indices[20] = {
#include "assets/actor_403900_animation_13A20_indices.inc"
};

static AnimationSet _gActor403900Animation13A20 = {
    _gActor403900Animation13A20Records,
    _gActor403900Animation13A20Indices,
    { NULL, _gActor403900Animation13A20Bank1, NULL, NULL, _gActor403900Animation13A20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation13F44Bank1[8] = {
#include "assets/actor_403900_animation_13F44_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation13F44Bank4[116] = {
#include "assets/actor_403900_animation_13F44_bank4.inc"
};

static AnimationRecord _gActor403900Animation13F44Records[169] = {
#include "assets/actor_403900_animation_13F44_records.inc"
};

static u16 _gActor403900Animation13F44Indices[20] = {
#include "assets/actor_403900_animation_13F44_indices.inc"
};

static AnimationSet _gActor403900Animation13F44 = {
    _gActor403900Animation13F44Records,
    _gActor403900Animation13F44Indices,
    { NULL, _gActor403900Animation13F44Bank1, NULL, NULL, _gActor403900Animation13F44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1498CBank1[19] = {
#include "assets/actor_403900_animation_1498C_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1498CBank4[254] = {
#include "assets/actor_403900_animation_1498C_bank4.inc"
};

static AnimationRecord _gActor403900Animation1498CRecords[327] = {
#include "assets/actor_403900_animation_1498C_records.inc"
};

static u16 _gActor403900Animation1498CIndices[20] = {
#include "assets/actor_403900_animation_1498C_indices.inc"
};

static AnimationSet _gActor403900Animation1498C = {
    _gActor403900Animation1498CRecords,
    _gActor403900Animation1498CIndices,
    { NULL, _gActor403900Animation1498CBank1, NULL, NULL, _gActor403900Animation1498CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1567CBank1[24] = {
#include "assets/actor_403900_animation_1567C_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1567CBank4[343] = {
#include "assets/actor_403900_animation_1567C_bank4.inc"
};

static AnimationRecord _gActor403900Animation1567CRecords[393] = {
#include "assets/actor_403900_animation_1567C_records.inc"
};

static u16 _gActor403900Animation1567CIndices[20] = {
#include "assets/actor_403900_animation_1567C_indices.inc"
};

static AnimationSet _gActor403900Animation1567C = {
    _gActor403900Animation1567CRecords,
    _gActor403900Animation1567CIndices,
    { NULL, _gActor403900Animation1567CBank1, NULL, NULL, _gActor403900Animation1567CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation16FB8Bank1[53] = {
#include "assets/actor_403900_animation_16FB8_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation16FB8Bank4[660] = {
#include "assets/actor_403900_animation_16FB8_bank4.inc"
};

static AnimationRecord _gActor403900Animation16FB8Records[776] = {
#include "assets/actor_403900_animation_16FB8_records.inc"
};

static u16 _gActor403900Animation16FB8Indices[20] = {
#include "assets/actor_403900_animation_16FB8_indices.inc"
};

static AnimationSet _gActor403900Animation16FB8 = {
    _gActor403900Animation16FB8Records,
    _gActor403900Animation16FB8Indices,
    { NULL, _gActor403900Animation16FB8Bank1, NULL, NULL, _gActor403900Animation16FB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation17DC8Bank1[26] = {
#include "assets/actor_403900_animation_17DC8_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation17DC8Bank4[365] = {
#include "assets/actor_403900_animation_17DC8_bank4.inc"
};

static AnimationRecord _gActor403900Animation17DC8Records[437] = {
#include "assets/actor_403900_animation_17DC8_records.inc"
};

static u16 _gActor403900Animation17DC8Indices[20] = {
#include "assets/actor_403900_animation_17DC8_indices.inc"
};

static AnimationSet _gActor403900Animation17DC8 = {
    _gActor403900Animation17DC8Records,
    _gActor403900Animation17DC8Indices,
    { NULL, _gActor403900Animation17DC8Bank1, NULL, NULL, _gActor403900Animation17DC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation18E40Bank1[31] = {
#include "assets/actor_403900_animation_18E40_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation18E40Bank4[429] = {
#include "assets/actor_403900_animation_18E40_bank4.inc"
};

static AnimationRecord _gActor403900Animation18E40Records[512] = {
#include "assets/actor_403900_animation_18E40_records.inc"
};

static u16 _gActor403900Animation18E40Indices[20] = {
#include "assets/actor_403900_animation_18E40_indices.inc"
};

static AnimationSet _gActor403900Animation18E40 = {
    _gActor403900Animation18E40Records,
    _gActor403900Animation18E40Indices,
    { NULL, _gActor403900Animation18E40Bank1, NULL, NULL, _gActor403900Animation18E40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation19314Bank1[9] = {
#include "assets/actor_403900_animation_19314_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation19314Bank4[113] = {
#include "assets/actor_403900_animation_19314_bank4.inc"
};

static AnimationRecord _gActor403900Animation19314Records[149] = {
#include "assets/actor_403900_animation_19314_records.inc"
};

static u16 _gActor403900Animation19314Indices[20] = {
#include "assets/actor_403900_animation_19314_indices.inc"
};

static AnimationSet _gActor403900Animation19314 = {
    _gActor403900Animation19314Records,
    _gActor403900Animation19314Indices,
    { NULL, _gActor403900Animation19314Bank1, NULL, NULL, _gActor403900Animation19314Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation19624Bank1[5] = {
#include "assets/actor_403900_animation_19624_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation19624Bank4[65] = {
#include "assets/actor_403900_animation_19624_bank4.inc"
};

static AnimationRecord _gActor403900Animation19624Records[96] = {
#include "assets/actor_403900_animation_19624_records.inc"
};

static u16 _gActor403900Animation19624Indices[20] = {
#include "assets/actor_403900_animation_19624_indices.inc"
};

static AnimationSet _gActor403900Animation19624 = {
    _gActor403900Animation19624Records,
    _gActor403900Animation19624Indices,
    { NULL, _gActor403900Animation19624Bank1, NULL, NULL, _gActor403900Animation19624Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation19800Bank1[2] = {
#include "assets/actor_403900_animation_19800_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation19800Bank4[17] = {
#include "assets/actor_403900_animation_19800_bank4.inc"
};

static AnimationRecord _gActor403900Animation19800Records[76] = {
#include "assets/actor_403900_animation_19800_records.inc"
};

static u16 _gActor403900Animation19800Indices[20] = {
#include "assets/actor_403900_animation_19800_indices.inc"
};

static AnimationSet _gActor403900Animation19800 = {
    _gActor403900Animation19800Records,
    _gActor403900Animation19800Indices,
    { NULL, _gActor403900Animation19800Bank1, NULL, NULL, _gActor403900Animation19800Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1A748Bank1[28] = {
#include "assets/actor_403900_animation_1A748_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1A748Bank4[394] = {
#include "assets/actor_403900_animation_1A748_bank4.inc"
};

static AnimationRecord _gActor403900Animation1A748Records[480] = {
#include "assets/actor_403900_animation_1A748_records.inc"
};

static u16 _gActor403900Animation1A748Indices[20] = {
#include "assets/actor_403900_animation_1A748_indices.inc"
};

static AnimationSet _gActor403900Animation1A748 = {
    _gActor403900Animation1A748Records,
    _gActor403900Animation1A748Indices,
    { NULL, _gActor403900Animation1A748Bank1, NULL, NULL, _gActor403900Animation1A748Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1ACDCBank1[9] = {
#include "assets/actor_403900_animation_1ACDC_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1ACDCBank4[127] = {
#include "assets/actor_403900_animation_1ACDC_bank4.inc"
};

static AnimationRecord _gActor403900Animation1ACDCRecords[183] = {
#include "assets/actor_403900_animation_1ACDC_records.inc"
};

static u16 _gActor403900Animation1ACDCIndices[20] = {
#include "assets/actor_403900_animation_1ACDC_indices.inc"
};

static AnimationSet _gActor403900Animation1ACDC = {
    _gActor403900Animation1ACDCRecords,
    _gActor403900Animation1ACDCIndices,
    { NULL, _gActor403900Animation1ACDCBank1, NULL, NULL, _gActor403900Animation1ACDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1AFA4Bank1[5] = {
#include "assets/actor_403900_animation_1AFA4_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1AFA4Bank4[56] = {
#include "assets/actor_403900_animation_1AFA4_bank4.inc"
};

static AnimationRecord _gActor403900Animation1AFA4Records[87] = {
#include "assets/actor_403900_animation_1AFA4_records.inc"
};

static u16 _gActor403900Animation1AFA4Indices[20] = {
#include "assets/actor_403900_animation_1AFA4_indices.inc"
};

static AnimationSet _gActor403900Animation1AFA4 = {
    _gActor403900Animation1AFA4Records,
    _gActor403900Animation1AFA4Indices,
    { NULL, _gActor403900Animation1AFA4Bank1, NULL, NULL, _gActor403900Animation1AFA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1B180Bank1[2] = {
#include "assets/actor_403900_animation_1B180_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1B180Bank4[17] = {
#include "assets/actor_403900_animation_1B180_bank4.inc"
};

static AnimationRecord _gActor403900Animation1B180Records[76] = {
#include "assets/actor_403900_animation_1B180_records.inc"
};

static u16 _gActor403900Animation1B180Indices[20] = {
#include "assets/actor_403900_animation_1B180_indices.inc"
};

static AnimationSet _gActor403900Animation1B180 = {
    _gActor403900Animation1B180Records,
    _gActor403900Animation1B180Indices,
    { NULL, _gActor403900Animation1B180Bank1, NULL, NULL, _gActor403900Animation1B180Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1E7A8Bank1[100] = {
#include "assets/actor_403900_animation_1E7A8_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1E7A8Bank4[1463] = {
#include "assets/actor_403900_animation_1E7A8_bank4.inc"
};

static AnimationRecord _gActor403900Animation1E7A8Records[1683] = {
#include "assets/actor_403900_animation_1E7A8_records.inc"
};

static u16 _gActor403900Animation1E7A8Indices[20] = {
#include "assets/actor_403900_animation_1E7A8_indices.inc"
};

static AnimationSet _gActor403900Animation1E7A8 = {
    _gActor403900Animation1E7A8Records,
    _gActor403900Animation1E7A8Indices,
    { NULL, _gActor403900Animation1E7A8Bank1, NULL, NULL, _gActor403900Animation1E7A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation1FB78Bank1[39] = {
#include "assets/actor_403900_animation_1FB78_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation1FB78Bank4[525] = {
#include "assets/actor_403900_animation_1FB78_bank4.inc"
};

static AnimationRecord _gActor403900Animation1FB78Records[606] = {
#include "assets/actor_403900_animation_1FB78_records.inc"
};

static u16 _gActor403900Animation1FB78Indices[20] = {
#include "assets/actor_403900_animation_1FB78_indices.inc"
};

static AnimationSet _gActor403900Animation1FB78 = {
    _gActor403900Animation1FB78Records,
    _gActor403900Animation1FB78Indices,
    { NULL, _gActor403900Animation1FB78Bank1, NULL, NULL, _gActor403900Animation1FB78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation2010CBank1[10] = {
#include "assets/actor_403900_animation_2010C_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation2010CBank4[126] = {
#include "assets/actor_403900_animation_2010C_bank4.inc"
};

static AnimationRecord _gActor403900Animation2010CRecords[181] = {
#include "assets/actor_403900_animation_2010C_records.inc"
};

static u16 _gActor403900Animation2010CIndices[20] = {
#include "assets/actor_403900_animation_2010C_indices.inc"
};

static AnimationSet _gActor403900Animation2010C = {
    _gActor403900Animation2010CRecords,
    _gActor403900Animation2010CIndices,
    { NULL, _gActor403900Animation2010CBank1, NULL, NULL, _gActor403900Animation2010CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation20B6CBank1[21] = {
#include "assets/actor_403900_animation_20B6C_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation20B6CBank4[261] = {
#include "assets/actor_403900_animation_20B6C_bank4.inc"
};

static AnimationRecord _gActor403900Animation20B6CRecords[320] = {
#include "assets/actor_403900_animation_20B6C_records.inc"
};

static u16 _gActor403900Animation20B6CIndices[20] = {
#include "assets/actor_403900_animation_20B6C_indices.inc"
};

static AnimationSet _gActor403900Animation20B6C = {
    _gActor403900Animation20B6CRecords,
    _gActor403900Animation20B6CIndices,
    { NULL, _gActor403900Animation20B6CBank1, NULL, NULL, _gActor403900Animation20B6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403900Animation21DA8Bank1[35] = {
#include "assets/actor_403900_animation_21DA8_bank1.inc"
};

static AnimationPackedRotation _gActor403900Animation21DA8Bank4[485] = {
#include "assets/actor_403900_animation_21DA8_bank4.inc"
};

static AnimationRecord _gActor403900Animation21DA8Records[557] = {
#include "assets/actor_403900_animation_21DA8_records.inc"
};

static u16 _gActor403900Animation21DA8Indices[20] = {
#include "assets/actor_403900_animation_21DA8_indices.inc"
};

static AnimationSet _gActor403900Animation21DA8 = {
    _gActor403900Animation21DA8Records,
    _gActor403900Animation21DA8Indices,
    { NULL, _gActor403900Animation21DA8Bank1, NULL, NULL, _gActor403900Animation21DA8Bank4, NULL, NULL, NULL },
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
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 2500, 4000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 2500, 4300, 10400, 5200, 13000, 3400 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 10500, 5700, 10000, 1500, 11000, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 6450, 1300, 5900, 5400, 7000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 1700, -500, 1000, 5800, 2400, 3200 },
};

GolemKnightBishopRegion D_actor_403900_80153D14[6] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 7000, -3500, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 6000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 800, 1000, 0, 6000, 1550, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 7500, 4500, 1550, 6000, 4000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7000, 5000, 5000, -4000, 9000, -5000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 500, 4500, 6000, 6000, 8000, 4000 },
};

GolemKnightBishopRegion D_actor_403900_80153D74[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 3000 }, 7000, -1600, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -7300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1600, -1600, 1000, -7000, 2500, -8000 },
};

GolemKnightBishopRegion D_actor_403900_80153DA4[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 3000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, -1000, 2750, 1500, 3500, 3500, 2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2500, 500, -3500, 1000, -2000, 0 },
};

GolemKnightBishopRegion D_actor_403900_80153DD4[8] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -1400, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 10000, 4200, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10900, 4200, 1000, 5000, 5000, 3500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 5500, 2500, 1000, 3500, 3500, 1500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 8950, 1600, 8000, 5800, 9500, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 5500, 4000, 11000, 4500, 12400, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1700, 4700, 1000, 0, 2400, -2900 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1800 }, 1400, 4700, 2400, 1200, 3500, 0 },
};

GolemKnightBishopRegion D_actor_403900_80153E54[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 10000, 5300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 7500, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 4000, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 1200, 8000, 6000, 9000, 7500, 7000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10300, 8000, 4000, 9000, 6000, 7000 },
};

GolemKnightBishopRegion D_actor_403900_80153EA4[2] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, -1900, 4800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2000, 4800, -5500, 5500, -1900, 4000 },
};

GolemKnightBishopRegion D_actor_403900_80153EC4[4] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, 3500, -1800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 8500, -1800, 0, -1000, 2000, -2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 7750, -7500, 6500, -1000, 9000, -2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7750, -3000, 6500, -6000, 9000, -9000 },
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
    &_gActor403900Animation1FB78,
    &_gActor403900Animation2010C,
    &_gActor403900Animation20B6C,
    &_gActor403900Animation21DA8,
};

EffectSpawnArg gGolemKnightBishopGrabEffect = { NULL, 300, 1 };

s16 gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_403900_801540E0 = { { { TASK_BODY_TMD, 96 } }, func_actor_403900_80138344, { .model = &_gActor403900GolemBody } };

AnimationSet* gGolemKnightBishopAnimSets[22] = {
    NULL,
    &_gActor403900Animation0C44C,
    &_gActor403900Animation0C904,
    &_gActor403900Animation0D110,
    &_gActor403900Animation0DD48,
    &_gActor403900Animation0EDFC,
    &_gActor403900Animation0F9A8,
    &_gActor403900Animation13A20,
    &_gActor403900Animation13F44,
    &_gActor403900Animation1498C,
    &_gActor403900Animation1567C,
    &_gActor403900Animation16FB8,
    &_gActor403900Animation17DC8,
    &_gActor403900Animation18E40,
    &_gActor403900Animation19314,
    &_gActor403900Animation19624,
    &_gActor403900Animation19800,
    &_gActor403900Animation1A748,
    &_gActor403900Animation1ACDC,
    &_gActor403900Animation1AFA4,
    &_gActor403900Animation1B180,
    &_gActor403900Animation1E7A8,
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
static const EnemyTaskFuncTable3 D_actor_403900_80131F18 = {
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
s32 func_actor_403900_801381E4(Task* task, s32 msgId, s32 arg2, s32 arg3)
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
    EnemyTaskFuncTable3 sp;

    sp = D_actor_403900_80131F18;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
