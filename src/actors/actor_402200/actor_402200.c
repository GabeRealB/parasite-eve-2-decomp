#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

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
static void _frameCaptureQueue(s32 orderingTableSlot);
#define FRAME_CAPTURE_QUEUE _frameCaptureQueue
#include "../../shared/frame_capture.h"
#define GOLEM_KNIGHT_BISHOP_KIND GOLEM_KNIGHT
#include "../../shared/golem_knight_bishop.h"

/// Per-animation-id value `golemKnightBishopTickAnim` hands `animationSeekSlotWithBlend`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 gGolemKnightBishopAnimBlend[];

extern GolemKnightBishopChargeSpeedSpan gGolemKnightBishopFrameSteps[];

/// Per-roll wait lengths state 0 of `_golemKnightBishopIdleSeq` scales by
/// `16 - attackCount`, indexed by a 4-bit `gRandomLcgState` draw.
extern s16 gGolemKnightBishopIdleWaits[];

/// Per-roll state offsets state 0 adds to 2 when `feintBroken` is set.
extern u16 gGolemKnightBishopIdleSteps[];

/// Cue word `_golemKnightBishopLightFlinchSeq` and `_golemKnightBishopHeavyFlinchSeq`
/// queue, a separate `D_` symbol in the overlay's data 0x48 past the cue-id
/// table `gGolemKnightBishopAnimCues`.
extern s32 gGolemKnightBishopPainCue;

/// Cue word the fade-out in `_golemKnightBishopUpdateAppearance` queues.
extern s32 gGolemKnightBishopFadeCue;

/// Cue-id table: `GolemKnightBishopWork::soundSet` picks two adjacent words,
/// `[soundSet * 2 - 1]` for the `flags` bit 0x20 cue and `[soundSet * 2]`
/// for the 0x10 one.
extern s32 gGolemKnightBishopAnimCues[];

/// Cue words `_golemKnightBishopBoxApproachSeq` queues next to
/// `gGolemKnightBishopPainCue`.
extern s32 gGolemKnightBishopApproachCue;
extern s32 gGolemKnightBishopStrikeCue;

/// Base id of the actor's vocal cues: the `Enemy` work id's high nibble
/// selects one of the four adjacent words here, picked up as bits 8-11 of the
/// cue id.
extern s32 gGolemKnightBishopHoldCue;

/// The spawn's tables: the task's next handler record, the `DamageAttack`
/// `damagePackAttackKey` packs into the third collision object, the `EnemyParams` whose
/// `hpMax` seeds the enemy's HP, the stage / room box-table index run, the
/// box tables it selects, the per-stage cue-bank arrays and the animation data.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry             gGolemKnightBishopMessages[2];
extern DamageAttack                 gGolemKnightBishopAttacks[4];
extern EnemyParams                  gGolemKnightBishopParams;
extern GolemKnightBishopRoomRegions gGolemKnightBishopSpots[];
extern GolemKnightBishopRegion*     gGolemKnightBishopRegions[];
extern s16*                         gGolemKnightBishopStageCues[];
extern AnimationSet*                gGolemKnightBishopAnimSets[22];

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 gGolemKnightBishopGrabHpLimits[];

/// Weighted 16-entry roll for `GolemKnightBishopWork::feinting`: indices 0-4 hold 0
/// and 5-15 hold 1, so the appearance is a feint about two thirds of the time.
extern u16 gGolemKnightBishopApproachRoll[];

/// Animation block the grab's 0x3FF messages hand the player.
extern AnimationSet* gGolemKnightBishopPlayerAnims[5];

/// `effectSpawnHit` argument record for the grab's finishing spark.
extern EffectSpawnArg gGolemKnightBishopGrabEffect;

/// The two four-vertex index rows the trail's shaded quads take their corners
/// from, into the scratch block's six-entry x / y runs.
extern s16 gGolemKnightBishopBeamQuadCorners[2][4];

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

s32 func_actor_402200_801381E0(Task*, s32, s32, s32);

static AnimationSet _gActor402200Animation0C448;
static AnimationSet _gActor402200Animation0C900;
static AnimationSet _gActor402200Animation0D10C;
static AnimationSet _gActor402200Animation0DD44;
static AnimationSet _gActor402200Animation0EDF8;
static AnimationSet _gActor402200Animation0F9A4;
static AnimationSet _gActor402200Animation13A1C;
static AnimationSet _gActor402200Animation13F40;
static AnimationSet _gActor402200Animation14988;
static AnimationSet _gActor402200Animation15678;
static AnimationSet _gActor402200Animation16FB4;
static AnimationSet _gActor402200Animation17DC4;
static AnimationSet _gActor402200Animation18E3C;
static AnimationSet _gActor402200Animation19310;
static AnimationSet _gActor402200Animation19620;
static AnimationSet _gActor402200Animation197FC;
static AnimationSet _gActor402200Animation1A744;
static AnimationSet _gActor402200Animation1ACD8;
static AnimationSet _gActor402200Animation1AFA0;
static AnimationSet _gActor402200Animation1B17C;
static AnimationSet _gActor402200Animation1E7A4;
static AnimationSet _gActor402200Animation1FB74;
static AnimationSet _gActor402200Animation20108;
static AnimationSet _gActor402200Animation20B68;
static AnimationSet _gActor402200Animation21DA4;
extern DamageAttack gGolemKnightBishopAttacks[4];
static TmdSource    _gActor402200GolemBody;
static void         func_actor_402200_80138340(Task*);

TaskMessageEntry gGolemKnightBishopMessages[2] = {
    { 2014, func_actor_402200_801381E0 },
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

GolemKnightBishopChargeSpeedSpan gGolemKnightBishopFrameSteps[18] = {
    { 3, -10 },
    { 5, 350 },
    { 6, 210 },
    { 9, 117 },
    { 11, 130 },
    { 15, 85 },
    { 17, 90 },
    { 21, 35 },
    { 22, -50 },
    { 23, -80 },
    { 31, 0 },
    { 37, 2 },
    { 43, 5 },
    { 62, 8 },
    { 64, -5 },
    { 69, -6 },
    { 84, -17 },
    { 109, -6 },
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

static TmdBone _gActor402200GolemBodySkeleton[19] = {
#include "assets/golem_body_skeleton.inc"
};

static u32 _gActor402200GolemBodyPartVerts[19] = {
#include "assets/golem_body_partVerts.inc"
};

static SVECTOR _gActor402200GolemBodyVerts[363] = {
#include "assets/golem_body_verts.inc"
};

static SVECTOR _gActor402200GolemBodyNormals[345] = {
#include "assets/golem_body_normals.inc"
};

static u32 _gActor402200GolemBodyStream[3985] = {
#include "assets/golem_body_stream.inc"
};

static TmdSource _gActor402200GolemBody = {
    0,
    43904,
    11888,
    19,
    _gActor402200GolemBodyPartVerts,
    _gActor402200GolemBodyVerts,
    _gActor402200GolemBodyNormals,
    _gActor402200GolemBodySkeleton,
    _gActor402200GolemBodyStream,
};

static AnimationPackedPose _gActor402200Animation0C448Bank1[12] = {
#include "assets/actor_402200_animation_0C448_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation0C448Bank4[165] = {
#include "assets/actor_402200_animation_0C448_bank4.inc"
};

static AnimationRecord _gActor402200Animation0C448Records[201] = {
#include "assets/actor_402200_animation_0C448_records.inc"
};

static u16 _gActor402200Animation0C448Indices[20] = {
#include "assets/actor_402200_animation_0C448_indices.inc"
};

static AnimationSet _gActor402200Animation0C448 = {
    _gActor402200Animation0C448Records,
    _gActor402200Animation0C448Indices,
    { NULL, _gActor402200Animation0C448Bank1, NULL, NULL, _gActor402200Animation0C448Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation0C900Bank1[7] = {
#include "assets/actor_402200_animation_0C900_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation0C900Bank4[87] = {
#include "assets/actor_402200_animation_0C900_bank4.inc"
};

static AnimationRecord _gActor402200Animation0C900Records[174] = {
#include "assets/actor_402200_animation_0C900_records.inc"
};

static u16 _gActor402200Animation0C900Indices[20] = {
#include "assets/actor_402200_animation_0C900_indices.inc"
};

static AnimationSet _gActor402200Animation0C900 = {
    _gActor402200Animation0C900Records,
    _gActor402200Animation0C900Indices,
    { NULL, _gActor402200Animation0C900Bank1, NULL, NULL, _gActor402200Animation0C900Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation0D10CBank1[15] = {
#include "assets/actor_402200_animation_0D10C_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation0D10CBank4[197] = {
#include "assets/actor_402200_animation_0D10C_bank4.inc"
};

static AnimationRecord _gActor402200Animation0D10CRecords[253] = {
#include "assets/actor_402200_animation_0D10C_records.inc"
};

static u16 _gActor402200Animation0D10CIndices[20] = {
#include "assets/actor_402200_animation_0D10C_indices.inc"
};

static AnimationSet _gActor402200Animation0D10C = {
    _gActor402200Animation0D10CRecords,
    _gActor402200Animation0D10CIndices,
    { NULL, _gActor402200Animation0D10CBank1, NULL, NULL, _gActor402200Animation0D10CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation0DD44Bank1[21] = {
#include "assets/actor_402200_animation_0DD44_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation0DD44Bank4[317] = {
#include "assets/actor_402200_animation_0DD44_bank4.inc"
};

static AnimationRecord _gActor402200Animation0DD44Records[382] = {
#include "assets/actor_402200_animation_0DD44_records.inc"
};

static u16 _gActor402200Animation0DD44Indices[20] = {
#include "assets/actor_402200_animation_0DD44_indices.inc"
};

static AnimationSet _gActor402200Animation0DD44 = {
    _gActor402200Animation0DD44Records,
    _gActor402200Animation0DD44Indices,
    { NULL, _gActor402200Animation0DD44Bank1, NULL, NULL, _gActor402200Animation0DD44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation0EDF8Bank1[29] = {
#include "assets/actor_402200_animation_0EDF8_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation0EDF8Bank4[451] = {
#include "assets/actor_402200_animation_0EDF8_bank4.inc"
};

static AnimationRecord _gActor402200Animation0EDF8Records[511] = {
#include "assets/actor_402200_animation_0EDF8_records.inc"
};

static u16 _gActor402200Animation0EDF8Indices[20] = {
#include "assets/actor_402200_animation_0EDF8_indices.inc"
};

static AnimationSet _gActor402200Animation0EDF8 = {
    _gActor402200Animation0EDF8Records,
    _gActor402200Animation0EDF8Indices,
    { NULL, _gActor402200Animation0EDF8Bank1, NULL, NULL, _gActor402200Animation0EDF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation0F9A4Bank1[30] = {
#include "assets/actor_402200_animation_0F9A4_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation0F9A4Bank4[277] = {
#include "assets/actor_402200_animation_0F9A4_bank4.inc"
};

static AnimationRecord _gActor402200Animation0F9A4Records[360] = {
#include "assets/actor_402200_animation_0F9A4_records.inc"
};

static u16 _gActor402200Animation0F9A4Indices[20] = {
#include "assets/actor_402200_animation_0F9A4_indices.inc"
};

static AnimationSet _gActor402200Animation0F9A4 = {
    _gActor402200Animation0F9A4Records,
    _gActor402200Animation0F9A4Indices,
    { NULL, _gActor402200Animation0F9A4Bank1, NULL, NULL, _gActor402200Animation0F9A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation13A1CBank1[111] = {
#include "assets/actor_402200_animation_13A1C_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation13A1CBank4[1789] = {
#include "assets/actor_402200_animation_13A1C_bank4.inc"
};

static AnimationRecord _gActor402200Animation13A1CRecords[1984] = {
#include "assets/actor_402200_animation_13A1C_records.inc"
};

static u16 _gActor402200Animation13A1CIndices[20] = {
#include "assets/actor_402200_animation_13A1C_indices.inc"
};

static AnimationSet _gActor402200Animation13A1C = {
    _gActor402200Animation13A1CRecords,
    _gActor402200Animation13A1CIndices,
    { NULL, _gActor402200Animation13A1CBank1, NULL, NULL, _gActor402200Animation13A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation13F40Bank1[8] = {
#include "assets/actor_402200_animation_13F40_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation13F40Bank4[116] = {
#include "assets/actor_402200_animation_13F40_bank4.inc"
};

static AnimationRecord _gActor402200Animation13F40Records[169] = {
#include "assets/actor_402200_animation_13F40_records.inc"
};

static u16 _gActor402200Animation13F40Indices[20] = {
#include "assets/actor_402200_animation_13F40_indices.inc"
};

static AnimationSet _gActor402200Animation13F40 = {
    _gActor402200Animation13F40Records,
    _gActor402200Animation13F40Indices,
    { NULL, _gActor402200Animation13F40Bank1, NULL, NULL, _gActor402200Animation13F40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation14988Bank1[19] = {
#include "assets/actor_402200_animation_14988_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation14988Bank4[254] = {
#include "assets/actor_402200_animation_14988_bank4.inc"
};

static AnimationRecord _gActor402200Animation14988Records[327] = {
#include "assets/actor_402200_animation_14988_records.inc"
};

static u16 _gActor402200Animation14988Indices[20] = {
#include "assets/actor_402200_animation_14988_indices.inc"
};

static AnimationSet _gActor402200Animation14988 = {
    _gActor402200Animation14988Records,
    _gActor402200Animation14988Indices,
    { NULL, _gActor402200Animation14988Bank1, NULL, NULL, _gActor402200Animation14988Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation15678Bank1[24] = {
#include "assets/actor_402200_animation_15678_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation15678Bank4[343] = {
#include "assets/actor_402200_animation_15678_bank4.inc"
};

static AnimationRecord _gActor402200Animation15678Records[393] = {
#include "assets/actor_402200_animation_15678_records.inc"
};

static u16 _gActor402200Animation15678Indices[20] = {
#include "assets/actor_402200_animation_15678_indices.inc"
};

static AnimationSet _gActor402200Animation15678 = {
    _gActor402200Animation15678Records,
    _gActor402200Animation15678Indices,
    { NULL, _gActor402200Animation15678Bank1, NULL, NULL, _gActor402200Animation15678Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation16FB4Bank1[53] = {
#include "assets/actor_402200_animation_16FB4_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation16FB4Bank4[660] = {
#include "assets/actor_402200_animation_16FB4_bank4.inc"
};

static AnimationRecord _gActor402200Animation16FB4Records[776] = {
#include "assets/actor_402200_animation_16FB4_records.inc"
};

static u16 _gActor402200Animation16FB4Indices[20] = {
#include "assets/actor_402200_animation_16FB4_indices.inc"
};

static AnimationSet _gActor402200Animation16FB4 = {
    _gActor402200Animation16FB4Records,
    _gActor402200Animation16FB4Indices,
    { NULL, _gActor402200Animation16FB4Bank1, NULL, NULL, _gActor402200Animation16FB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation17DC4Bank1[26] = {
#include "assets/actor_402200_animation_17DC4_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation17DC4Bank4[365] = {
#include "assets/actor_402200_animation_17DC4_bank4.inc"
};

static AnimationRecord _gActor402200Animation17DC4Records[437] = {
#include "assets/actor_402200_animation_17DC4_records.inc"
};

static u16 _gActor402200Animation17DC4Indices[20] = {
#include "assets/actor_402200_animation_17DC4_indices.inc"
};

static AnimationSet _gActor402200Animation17DC4 = {
    _gActor402200Animation17DC4Records,
    _gActor402200Animation17DC4Indices,
    { NULL, _gActor402200Animation17DC4Bank1, NULL, NULL, _gActor402200Animation17DC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation18E3CBank1[31] = {
#include "assets/actor_402200_animation_18E3C_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation18E3CBank4[429] = {
#include "assets/actor_402200_animation_18E3C_bank4.inc"
};

static AnimationRecord _gActor402200Animation18E3CRecords[512] = {
#include "assets/actor_402200_animation_18E3C_records.inc"
};

static u16 _gActor402200Animation18E3CIndices[20] = {
#include "assets/actor_402200_animation_18E3C_indices.inc"
};

static AnimationSet _gActor402200Animation18E3C = {
    _gActor402200Animation18E3CRecords,
    _gActor402200Animation18E3CIndices,
    { NULL, _gActor402200Animation18E3CBank1, NULL, NULL, _gActor402200Animation18E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation19310Bank1[9] = {
#include "assets/actor_402200_animation_19310_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation19310Bank4[113] = {
#include "assets/actor_402200_animation_19310_bank4.inc"
};

static AnimationRecord _gActor402200Animation19310Records[149] = {
#include "assets/actor_402200_animation_19310_records.inc"
};

static u16 _gActor402200Animation19310Indices[20] = {
#include "assets/actor_402200_animation_19310_indices.inc"
};

static AnimationSet _gActor402200Animation19310 = {
    _gActor402200Animation19310Records,
    _gActor402200Animation19310Indices,
    { NULL, _gActor402200Animation19310Bank1, NULL, NULL, _gActor402200Animation19310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation19620Bank1[5] = {
#include "assets/actor_402200_animation_19620_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation19620Bank4[65] = {
#include "assets/actor_402200_animation_19620_bank4.inc"
};

static AnimationRecord _gActor402200Animation19620Records[96] = {
#include "assets/actor_402200_animation_19620_records.inc"
};

static u16 _gActor402200Animation19620Indices[20] = {
#include "assets/actor_402200_animation_19620_indices.inc"
};

static AnimationSet _gActor402200Animation19620 = {
    _gActor402200Animation19620Records,
    _gActor402200Animation19620Indices,
    { NULL, _gActor402200Animation19620Bank1, NULL, NULL, _gActor402200Animation19620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation197FCBank1[2] = {
#include "assets/actor_402200_animation_197FC_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation197FCBank4[17] = {
#include "assets/actor_402200_animation_197FC_bank4.inc"
};

static AnimationRecord _gActor402200Animation197FCRecords[76] = {
#include "assets/actor_402200_animation_197FC_records.inc"
};

static u16 _gActor402200Animation197FCIndices[20] = {
#include "assets/actor_402200_animation_197FC_indices.inc"
};

static AnimationSet _gActor402200Animation197FC = {
    _gActor402200Animation197FCRecords,
    _gActor402200Animation197FCIndices,
    { NULL, _gActor402200Animation197FCBank1, NULL, NULL, _gActor402200Animation197FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation1A744Bank1[28] = {
#include "assets/actor_402200_animation_1A744_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation1A744Bank4[394] = {
#include "assets/actor_402200_animation_1A744_bank4.inc"
};

static AnimationRecord _gActor402200Animation1A744Records[480] = {
#include "assets/actor_402200_animation_1A744_records.inc"
};

static u16 _gActor402200Animation1A744Indices[20] = {
#include "assets/actor_402200_animation_1A744_indices.inc"
};

static AnimationSet _gActor402200Animation1A744 = {
    _gActor402200Animation1A744Records,
    _gActor402200Animation1A744Indices,
    { NULL, _gActor402200Animation1A744Bank1, NULL, NULL, _gActor402200Animation1A744Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation1ACD8Bank1[9] = {
#include "assets/actor_402200_animation_1ACD8_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation1ACD8Bank4[127] = {
#include "assets/actor_402200_animation_1ACD8_bank4.inc"
};

static AnimationRecord _gActor402200Animation1ACD8Records[183] = {
#include "assets/actor_402200_animation_1ACD8_records.inc"
};

static u16 _gActor402200Animation1ACD8Indices[20] = {
#include "assets/actor_402200_animation_1ACD8_indices.inc"
};

static AnimationSet _gActor402200Animation1ACD8 = {
    _gActor402200Animation1ACD8Records,
    _gActor402200Animation1ACD8Indices,
    { NULL, _gActor402200Animation1ACD8Bank1, NULL, NULL, _gActor402200Animation1ACD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation1AFA0Bank1[5] = {
#include "assets/actor_402200_animation_1AFA0_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation1AFA0Bank4[56] = {
#include "assets/actor_402200_animation_1AFA0_bank4.inc"
};

static AnimationRecord _gActor402200Animation1AFA0Records[87] = {
#include "assets/actor_402200_animation_1AFA0_records.inc"
};

static u16 _gActor402200Animation1AFA0Indices[20] = {
#include "assets/actor_402200_animation_1AFA0_indices.inc"
};

static AnimationSet _gActor402200Animation1AFA0 = {
    _gActor402200Animation1AFA0Records,
    _gActor402200Animation1AFA0Indices,
    { NULL, _gActor402200Animation1AFA0Bank1, NULL, NULL, _gActor402200Animation1AFA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation1B17CBank1[2] = {
#include "assets/actor_402200_animation_1B17C_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation1B17CBank4[17] = {
#include "assets/actor_402200_animation_1B17C_bank4.inc"
};

static AnimationRecord _gActor402200Animation1B17CRecords[76] = {
#include "assets/actor_402200_animation_1B17C_records.inc"
};

static u16 _gActor402200Animation1B17CIndices[20] = {
#include "assets/actor_402200_animation_1B17C_indices.inc"
};

static AnimationSet _gActor402200Animation1B17C = {
    _gActor402200Animation1B17CRecords,
    _gActor402200Animation1B17CIndices,
    { NULL, _gActor402200Animation1B17CBank1, NULL, NULL, _gActor402200Animation1B17CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation1E7A4Bank1[100] = {
#include "assets/actor_402200_animation_1E7A4_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation1E7A4Bank4[1463] = {
#include "assets/actor_402200_animation_1E7A4_bank4.inc"
};

static AnimationRecord _gActor402200Animation1E7A4Records[1683] = {
#include "assets/actor_402200_animation_1E7A4_records.inc"
};

static u16 _gActor402200Animation1E7A4Indices[20] = {
#include "assets/actor_402200_animation_1E7A4_indices.inc"
};

static AnimationSet _gActor402200Animation1E7A4 = {
    _gActor402200Animation1E7A4Records,
    _gActor402200Animation1E7A4Indices,
    { NULL, _gActor402200Animation1E7A4Bank1, NULL, NULL, _gActor402200Animation1E7A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation1FB74Bank1[39] = {
#include "assets/actor_402200_animation_1FB74_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation1FB74Bank4[525] = {
#include "assets/actor_402200_animation_1FB74_bank4.inc"
};

static AnimationRecord _gActor402200Animation1FB74Records[606] = {
#include "assets/actor_402200_animation_1FB74_records.inc"
};

static u16 _gActor402200Animation1FB74Indices[20] = {
#include "assets/actor_402200_animation_1FB74_indices.inc"
};

static AnimationSet _gActor402200Animation1FB74 = {
    _gActor402200Animation1FB74Records,
    _gActor402200Animation1FB74Indices,
    { NULL, _gActor402200Animation1FB74Bank1, NULL, NULL, _gActor402200Animation1FB74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation20108Bank1[10] = {
#include "assets/actor_402200_animation_20108_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation20108Bank4[126] = {
#include "assets/actor_402200_animation_20108_bank4.inc"
};

static AnimationRecord _gActor402200Animation20108Records[181] = {
#include "assets/actor_402200_animation_20108_records.inc"
};

static u16 _gActor402200Animation20108Indices[20] = {
#include "assets/actor_402200_animation_20108_indices.inc"
};

static AnimationSet _gActor402200Animation20108 = {
    _gActor402200Animation20108Records,
    _gActor402200Animation20108Indices,
    { NULL, _gActor402200Animation20108Bank1, NULL, NULL, _gActor402200Animation20108Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation20B68Bank1[21] = {
#include "assets/actor_402200_animation_20B68_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation20B68Bank4[261] = {
#include "assets/actor_402200_animation_20B68_bank4.inc"
};

static AnimationRecord _gActor402200Animation20B68Records[320] = {
#include "assets/actor_402200_animation_20B68_records.inc"
};

static u16 _gActor402200Animation20B68Indices[20] = {
#include "assets/actor_402200_animation_20B68_indices.inc"
};

static AnimationSet _gActor402200Animation20B68 = {
    _gActor402200Animation20B68Records,
    _gActor402200Animation20B68Indices,
    { NULL, _gActor402200Animation20B68Bank1, NULL, NULL, _gActor402200Animation20B68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor402200Animation21DA4Bank1[35] = {
#include "assets/actor_402200_animation_21DA4_bank1.inc"
};

static AnimationPackedRotation _gActor402200Animation21DA4Bank4[485] = {
#include "assets/actor_402200_animation_21DA4_bank4.inc"
};

static AnimationRecord _gActor402200Animation21DA4Records[557] = {
#include "assets/actor_402200_animation_21DA4_records.inc"
};

static u16 _gActor402200Animation21DA4Indices[20] = {
#include "assets/actor_402200_animation_21DA4_indices.inc"
};

static AnimationSet _gActor402200Animation21DA4 = {
    _gActor402200Animation21DA4Records,
    _gActor402200Animation21DA4Indices,
    { NULL, _gActor402200Animation21DA4Bank1, NULL, NULL, _gActor402200Animation21DA4Bank4, NULL, NULL, NULL },
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

GolemKnightBishopRoomRegions gGolemKnightBishopSpots[10] = {
    { 1, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 8 },
    { 2, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 5 },
    { 3, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, 2 },
    { 4, GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 5 },
    { 5, GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 3 },
    { 6, GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS, 8 },
    { 7, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 4 },
    { 8, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 5 },
    { 9, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, 6 },
    { 0, 0, 0, 0 },
};

GolemKnightBishopRegion D_actor_402200_80153CC8[8] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -1400, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 10000, 4200, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10900, 4200, 1000, 5000, 5000, 3500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 5500, 2500, 1000, 3500, 3500, 1500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 8950, 1600, 8000, 5800, 9500, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 5500, 4000, 11000, 4500, 12400, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1700, 4700, 1000, 0, 2400, -2900 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1800 }, 1400, 4700, 2400, 1200, 3500, 0 },
};

GolemKnightBishopRegion D_actor_402200_80153D48[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 10000, 5300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 7500, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 4000, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 1200, 8000, 6000, 9000, 7500, 7000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10300, 8000, 4000, 9000, 6000, 7000 },
};

GolemKnightBishopRegion D_actor_402200_80153D98[2] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, -1900, 4800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2000, 4800, -5500, 5500, -1900, 4000 },
};

GolemKnightBishopRegion D_actor_402200_80153DB8[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, -3000, -2000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, -1500, -7500, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, -100, 2600, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, -1000, -7500, -2500, 4500, 500, 2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, -300, 9500, -500, 6500, 500, 5000 },
};

GolemKnightBishopRegion D_actor_402200_80153E08[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 3000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, -1000, 2750, 1500, 3500, 3500, 2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2500, 500, -3500, 1000, -2000, 0 },
};

GolemKnightBishopRegion D_actor_402200_80153E38[8] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 3000 }, 5500, -8500, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 16500, -7000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 16500, -7000, 15000, -500, 18000, -2500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 7500, -8500, 13000, -7000, 15500, -9800 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 16500, -4000, 15500, -6600, 18000, -9800 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10500, -8500, 4000, -7000, 6500, -9800 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 17000, -8500, 9200, -7000, 12000, -9800 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 11000, -8500, 6500, -7000, 8500, -9800 },
};

GolemKnightBishopRegion D_actor_402200_80153EB8[4] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 0, -3000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 0, -2000, -500, -7000, 500, -11000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 0, -18000, -500, -11000, 500, -13000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 0, -10000, -500, -5000, 500, -7000 },
};

GolemKnightBishopRegion D_actor_402200_80153EF8[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 2500, 4000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 2500, 4300, 10400, 5200, 13000, 3400 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 10500, 5700, 10000, 1500, 11000, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 6450, 1300, 5900, 5400, 7000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 1700, -500, 1000, 5800, 2400, 3200 },
};

GolemKnightBishopRegion D_actor_402200_80153F48[6] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 7000, -3500, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 6000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 800, 1000, 0, 6000, 1550, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 7500, 4500, 1550, 6000, 4000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7000, 5000, 5000, -4000, 9000, -5000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 500, 4500, 6000, 6000, 8000, 4000 },
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
    &_gActor402200Animation1FB74,
    &_gActor402200Animation20108,
    &_gActor402200Animation20B68,
    &_gActor402200Animation21DA4,
};

EffectSpawnArg gGolemKnightBishopGrabEffect = { NULL, 300, 1 };

s16 gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_402200_80154188 = { { { TASK_BODY_TMD, 96 } }, func_actor_402200_80138340, { .model = &_gActor402200GolemBody } };

AnimationSet* gGolemKnightBishopAnimSets[22] = {
    NULL,
    &_gActor402200Animation0C448,
    &_gActor402200Animation0C900,
    &_gActor402200Animation0D10C,
    &_gActor402200Animation0DD44,
    &_gActor402200Animation0EDF8,
    &_gActor402200Animation0F9A4,
    &_gActor402200Animation13A1C,
    &_gActor402200Animation13F40,
    &_gActor402200Animation14988,
    &_gActor402200Animation15678,
    &_gActor402200Animation16FB4,
    &_gActor402200Animation17DC4,
    &_gActor402200Animation18E3C,
    &_gActor402200Animation19310,
    &_gActor402200Animation19620,
    &_gActor402200Animation197FC,
    &_gActor402200Animation1A744,
    &_gActor402200Animation1ACD8,
    &_gActor402200Animation1AFA0,
    &_gActor402200Animation1B17C,
    &_gActor402200Animation1E7A4,
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
static const EnemyTaskFuncTable3 D_actor_402200_80131F18 = {
    _golemKnightBishopSpawn,
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

/// Records that the player has struggled free of the hold (`grabBreak` 1),
/// provided they are still alive.
s32 func_actor_402200_801381E0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (gPlayerStatus.hp > 0) {
        ((GolemKnightBishopWork*)task->work)->grabBreak = 1;
    }
    return 0;
}

#include "../../shared/golem_knight_bishop_frame_capture.inc.c"
/// Runs the enemy task's current state handler from
/// `D_actor_402200_80131F18`, copying the table onto the stack first.
static void func_actor_402200_80138340(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_402200_80131F18;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
