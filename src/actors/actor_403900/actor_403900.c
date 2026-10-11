#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"
#include "actors/task_tables.h"

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
#include "main/areas.h"
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
#define GOLEM_KNIGHT_BISHOP_KIND GOLEM_BISHOP
#include "../../shared/golem_knight_bishop.h"

/// Per-roll wait lengths the wait state of `_golemKnightBishopIdleSeq`
/// scales by `16 - attackCount`, indexed by a 4-bit `gRandomLcgState` draw.
extern s16 gGolemKnightBishopIdleWaits[];

/// Per-roll state offsets the wait state adds to 2 when `feintBroken` is set.
extern u16 gGolemKnightBishopIdleSteps[];

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 gGolemKnightBishopGrabHpLimits[];

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static s32 _actor403900RecordGrabEscape(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

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
static TmdSource    _gActor403900GolemBody;
static void         _actor403900GolemTask(Task* task);

/// GOLEM message handlers, terminated by `TASK_MESSAGE_TABLE_END`.
///
/// Release-hold records a living player's completed struggle; other IDs return zero.
static TaskMessageEntry _gGolemKnightBishopMessages[2] = {
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor403900RecordGrabEscape },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Blend duration in normal-rate frames for each requested GOLEM clip.
///
/// Indexed by `GOLEM_KNIGHT_BISHOP_ANIM_`; zero restarts without a blend.
static s16 _gGolemKnightBishopAnimBlendFrames[GOLEM_KNIGHT_BISHOP_ANIM_COUNT] = {
    [GOLEM_KNIGHT_BISHOP_ANIM_NONE]                = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_START]          = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_HOLD]           = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_KILL]           = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_STAND]               = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_STRIKE]              = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE]             = 8,
    [GOLEM_KNIGHT_BISHOP_ANIM_CHARGE]              = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIGHT_FLINCH]        = 1,
    [GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_FRONT]  = 1,
    [GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_BEHIND] = 1,
    [GOLEM_KNIGHT_BISHOP_ANIM_RECOVER]             = 8,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE]        = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND]         = 4,
    [GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND]   = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_BEHIND]       = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND]          = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT]          = 4,
    [GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT]    = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_FRONT]        = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT]           = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_EARLY_RELEASE]  = 0,
};

/// Forward-speed curve of the box approach's charge, in world units per frame.
///
/// The first span ending at or after the animation frame supplies the raw
/// halfword speed. The final frame 109 covers the charge through its end.
static GolemKnightBishopChargeSpeedSpan _gGolemKnightBishopChargeSpeedSpans[18] = {
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

/// Base sound-script IDs for the room's GOLEM sound set.
///
/// Sets 1..4 use [set * 2 - 1] for cue 2, [set * 2] for cue 1, and
/// [set + 8] for the fall impact. Set 0 disables animation cues; its impact
/// lookup still selects entry 8. Entries 13..16 have no observed consumers.
/// Callers OR the Enemy placement index into bits 8..11 before starting a script.
static s32 _gGolemKnightBishopAnimSounds[17] = {
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

/// Base sound-script ID played during a real appearance.
///
/// The placement index is ORed into bits 8..11; the live instance ID is kept
/// until the appearance completes or is interrupted, then stopped.
static s32 _gGolemKnightBishopAppearSound = 0x40160007;

/// Base sound-script ID played while the GOLEM vanishes.
///
/// The placement index is ORed into bits 8..11; the live instance ID is kept
/// until the model is hidden, then stopped.
static s32 _gGolemKnightBishopVanishSound = 0x40160008;

/// Base sound-script ID played once when a normal vanish finishes.
///
/// Callers OR the placement index into bits 8..11; feint shrinking does not play it.
static s32 _gGolemKnightBishopVanishEndSound = 0x4016000F;

/// Base sound-script ID for each strike blow and the charge's attack cue.
///
/// Callers OR the placement index into bits 8..11 before starting the script.
static s32 _gGolemKnightBishopStrikeSound = 0x40160010;

/// Base sound-script ID played twenty ticks into the grab release countdown.
///
/// Callers OR the placement index into bits 8..11 before starting the script.
static s32 _gGolemKnightBishopGrabReleaseSound = 0x40160011;

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

/// Damage and reaction rows for the grab, strike and charge.
///
/// The fourth row is retained but is not selected by the included sequences.
static DamageAttack _gGolemKnightBishopAttacks[4] = {
    [GOLEM_KNIGHT_BISHOP_ATTACK_GRAB]   = { 10, 10 },
    [GOLEM_KNIGHT_BISHOP_ATTACK_STRIKE] = { 45, 2 },
    [GOLEM_KNIGHT_BISHOP_ATTACK_CHARGE] = { 58, 2 },
    [3]                                 = { 999, 0 },
};

/// Bishop GOLEM combat parameters and release rewards.
///
/// Shared by live enemies of this carrier; `hpMax` supplies their starting HP.
static EnemyParams _gGolemKnightBishopParams = { _gGolemKnightBishopAttacks, 800, 400, 2500, 7, 100, 20, 0, 0 };

s16 gGolemKnightBishopGrabHpLimits[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

/// Weighted four-bit roll selecting whether a strike appearance is a feint.
///
/// Eleven rolls select a real appearance and five a feint. Values are 0 real, 1 feint.
static u16 _gGolemKnightBishopFeintRoll[16] = {
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
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

static GolemKnightBishopRegion _gActor403900ShelterB2BreedingRoomRegions[5];
static GolemKnightBishopRegion _gActor403900NeoArkShrineRegions[6];
static GolemKnightBishopRegion _gActor403900ShelterB2PodAccessTunnelRegions[3];
static GolemKnightBishopRegion _gActor403900DryfieldNightParkingLotRegions[3];
static GolemKnightBishopRegion _gActor403900ShelterB1SleepingQuartersRegions[8];
static GolemKnightBishopRegion _gActor403900ShelterB1AccessTunnelRegions[5];
static GolemKnightBishopRegion _gActor403900Shelter1fAirlockRegions[2];
static GolemKnightBishopRegion _gActor403900NeoArkPowerPlant2Regions[4];

/// Room selection for the GOLEM's activation regions.
///
/// `regionTable` indexes `_gGolemKnightBishopRegionTables`; zero ends the list.
/// Matching rows supply both circle and box regions, with the last match winning.
static GolemKnightBishopRoomRegions _gGolemKnightBishopRoomRegions[9] = {
    { 1, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, ARRAY_SIZE(_gActor403900ShelterB2BreedingRoomRegions) },
    { 2, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, ARRAY_SIZE(_gActor403900NeoArkShrineRegions) },
    { 3, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, ARRAY_SIZE(_gActor403900ShelterB2PodAccessTunnelRegions) },
    { 4, GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, ARRAY_SIZE(_gActor403900DryfieldNightParkingLotRegions) },
    { 5, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, ARRAY_SIZE(_gActor403900ShelterB1SleepingQuartersRegions) },
    { 6, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, ARRAY_SIZE(_gActor403900ShelterB1AccessTunnelRegions) },
    { 7, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, ARRAY_SIZE(_gActor403900Shelter1fAirlockRegions) },
    { 8, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, ARRAY_SIZE(_gActor403900NeoArkPowerPlant2Regions) },
    { GOLEM_KNIGHT_BISHOP_REGION_TABLE_END, GAME_STAGE_NONE, 0, 0 },
};

/// Bishop activation regions in the Shelter B2 breeding room.
///
/// One circle requests a grab behind the player; four boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB2BreedingRoomRegions[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 2500, 4000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 2500, 4300, 10400, 5200, 13000, 3400 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 10500, 5700, 10000, 1500, 11000, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 6450, 1300, 5900, 5400, 7000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 1700, -500, 1000, 5800, 2400, 3200 },
};

/// Bishop activation regions in the Neo Ark shrine.
///
/// Two circles request grabs behind the player; four boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900NeoArkShrineRegions[6] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 7000, -3500, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 6000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 800, 1000, 0, 6000, 1550, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 7500, 4500, 1550, 6000, 4000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7000, 5000, 5000, -4000, 9000, -5000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 500, 4500, 6000, 6000, 8000, 4000 },
};

/// Bishop activation regions in the Shelter B2 pod access tunnel.
///
/// Two circles request grabs behind the player; one box selects an attack post.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB2PodAccessTunnelRegions[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 3000 }, 7000, -1600, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -7300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1600, -1600, 1000, -7000, 2500, -8000 },
};

/// Bishop activation regions in the Dryfield night parking lot.
///
/// One circle requests a grab behind the player; two boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900DryfieldNightParkingLotRegions[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 3000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, -1000, 2750, 1500, 3500, 3500, 2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2500, 500, -3500, 1000, -2000, 0 },
};

/// Bishop activation regions in the Shelter B1 sleeping quarters.
///
/// Two circles request grabs behind the player; six boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB1SleepingQuartersRegions[8] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -1400, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 10000, 4200, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10900, 4200, 1000, 5000, 5000, 3500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 5500, 2500, 1000, 3500, 3500, 1500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 8950, 1600, 8000, 5800, 9500, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 5500, 4000, 11000, 4500, 12400, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1700, 4700, 1000, 0, 2400, -2900 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1800 }, 1400, 4700, 2400, 1200, 3500, 0 },
};

/// Bishop activation regions in the Shelter B1 access tunnel.
///
/// Three circles request grabs behind the player; two boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB1AccessTunnelRegions[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 10000, 5300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 7500, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 4000, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 1200, 8000, 6000, 9000, 7500, 7000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10300, 8000, 4000, 9000, 6000, 7000 },
};

/// Bishop activation regions in the Shelter 1F airlock.
///
/// One circle requests a grab behind the player; one box selects an attack post.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900Shelter1fAirlockRegions[2] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, -1900, 4800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2000, 4800, -5500, 5500, -1900, 4000 },
};

/// Bishop activation regions in the Neo Ark power plant 2.
///
/// One circle requests a grab behind the player; three boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900NeoArkPowerPlant2Regions[4] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, 3500, -1800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 8500, -1800, 0, -1000, 2000, -2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 7750, -7500, 6500, -1000, 9000, -2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7750, -3000, 6500, -6000, 9000, -9000 },
};

/// Bishop room activation-region tables, selected by the room directory's 1..8 keys.
///
/// Slot 0 is NULL. Each selected table is borrowed with the corresponding
/// directory row's complete regionCount for the actor overlay's lifetime.
static GolemKnightBishopRegion* _gGolemKnightBishopRegionTables[9] = {
    NULL,
    _gActor403900ShelterB2BreedingRoomRegions,
    _gActor403900NeoArkShrineRegions,
    _gActor403900ShelterB2PodAccessTunnelRegions,
    _gActor403900DryfieldNightParkingLotRegions,
    _gActor403900ShelterB1SleepingQuartersRegions,
    _gActor403900ShelterB1AccessTunnelRegions,
    _gActor403900Shelter1fAirlockRegions,
    _gActor403900NeoArkPowerPlant2Regions,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_ACROPOLIS`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 21.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900AcropolisSoundSets[21] = {
    [GAME_AREA_ACROPOLIS_CAFETERIA]   = 2,
    [GAME_AREA_ACROPOLIS_FORKED_ROAD] = 2,
    [GAME_AREA_ACROPOLIS_PROMENADE]   = 2,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_DRYFIELD`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 39.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900DryfieldSoundSets[39] = {
    [GAME_AREA_DRYFIELD_MAIN_STREET]   = 4,
    [GAME_AREA_DRYFIELD_BACK_STREET]   = 4,
    [GAME_AREA_DRYFIELD_PARKING_LOT]   = 4,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY] = 2,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_DRYFIELD_NIGHT`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 39.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900DryfieldNightSoundSets[39] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION] = 4,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET] = 4,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT] = 4,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]   = 4,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]   = 4,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_MINE_SHELTER`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 50.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900MineShelterSoundSets[50] = {
    [GAME_AREA_MINE_MESA]                             = 4,
    [GAME_AREA_MINE_CAVERN]                           = 4,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = 3,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = 2,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = 3,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = 2,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = 2,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = 3,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = 1,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = 1,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = 3,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = 2,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = 2,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = 2,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = 1,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = 2,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = 2,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_SHELTER_NEO_ARK`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 34.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900ShelterNeoArkSoundSets[34] = {
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = 2,
    [GAME_AREA_SHELTER_1F_BULWARK]           = 2,
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = 2,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = 2,
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = 4,
    [GAME_AREA_NEO_ARK_PAVILION]             = 2,
    [GAME_AREA_NEO_ARK_ISLAND]               = 2,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = 2,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = 4,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = 2,
    [GAME_AREA_NEO_ARK_SHRINE]               = 2,
    [GAME_AREA_NEO_ARK_BRIDGE]               = 1,
    [GAME_AREA_NEO_ARK_PYRAMID]              = 4,
};

/// Per-area sound-set selector tables indexed by `GAME_STAGE_*` (0..5).
///
/// Stage 0 has no table. Area bounds for stages 1..5 are 21, 39, 39, 50 and
/// 34 entries; selectors 0..4 control bank loading and motion/impact scripts.
/// Spawn copies the selector, while these carrier-owned tables remain loaded.
static s16* _gGolemKnightBishopStageSoundSets[6] = {
    [GAME_STAGE_NONE]            = NULL,
    [GAME_STAGE_ACROPOLIS]       = _gActor403900AcropolisSoundSets,
    [GAME_STAGE_DRYFIELD]        = _gActor403900DryfieldSoundSets,
    [GAME_STAGE_DRYFIELD_NIGHT]  = _gActor403900DryfieldNightSoundSets,
    [GAME_STAGE_MINE_SHELTER]    = _gActor403900MineShelterSoundSets,
    [GAME_STAGE_SHELTER_NEO_ARK] = _gActor403900ShelterNeoArkSoundSets,
};

/// Player clips borrowed for the GOLEM grab: start, hold, kill and release.
///
/// Index 0 is unused. The player retains the bank and clip data through playback,
/// so the carrier must remain loaded until scripted control is returned.
static AnimationSet* _gGolemKnightBishopPlayerGrabAnimSets[GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_ANIM_COUNT] = {
    [GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_NONE]    = NULL,
    [GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_START]   = &_gActor403900Animation1FB78,
    [GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_HOLD]    = &_gActor403900Animation2010C,
    [GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_KILL]    = &_gActor403900Animation20B6C,
    [GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_RELEASE] = &_gActor403900Animation21DA8,
};

/// Writable puff-spawn record used at the fatal grab's killing frame.
///
/// The first call caches the player's part-4 coordinate in `coord`; it must remain
/// live for later calls. `spawnArgHi` requests one repeated puff; this effect kind
/// ignores the retained low-halfword value 300.
static EffectSpawnArg _gGolemKnightBishopGrabKillEffectArg = { NULL, 300, 1 };

/// Vertex indices of the two fading ribbons around one aim-beam segment.
///
/// Each row selects four corners from the six screen-space vertices: 0/1 are
/// the beam centreline, 2/3 and 4/5 the two outer edges.
static s16 _gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

enum { ACTOR_403900_TASK_PRIORITY = 96 };

TaskDesc gActor403900GolemTaskDesc = { { { TASK_BODY_TMD, ACTOR_403900_TASK_PRIORITY } }, _actor403900GolemTask, { .model = &_gActor403900GolemBody } };

/// GOLEM animation bank indexed by `GOLEM_KNIGHT_BISHOP_ANIM_`.
///
/// Index 0 is unused; clips 1..21 drive the nineteen-part rig. The bank and
/// its clip data are borrowed by live slots for the carrier's loaded lifetime.
static AnimationSet* _gGolemKnightBishopAnimSets[GOLEM_KNIGHT_BISHOP_ANIM_COUNT] = {
    [GOLEM_KNIGHT_BISHOP_ANIM_NONE]                = NULL,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_START]          = &_gActor403900Animation0C44C,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_HOLD]           = &_gActor403900Animation0C904,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_KILL]           = &_gActor403900Animation0D110,
    [GOLEM_KNIGHT_BISHOP_ANIM_STAND]               = &_gActor403900Animation0DD48,
    [GOLEM_KNIGHT_BISHOP_ANIM_STRIKE]              = &_gActor403900Animation0EDFC,
    [GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE]             = &_gActor403900Animation0F9A8,
    [GOLEM_KNIGHT_BISHOP_ANIM_CHARGE]              = &_gActor403900Animation13A20,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIGHT_FLINCH]        = &_gActor403900Animation13F44,
    [GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_FRONT]  = &_gActor403900Animation1498C,
    [GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_BEHIND] = &_gActor403900Animation1567C,
    [GOLEM_KNIGHT_BISHOP_ANIM_RECOVER]             = &_gActor403900Animation16FB8,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE]        = &_gActor403900Animation17DC8,
    [GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND]         = &_gActor403900Animation18E40,
    [GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND]   = &_gActor403900Animation19314,
    [GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_BEHIND]       = &_gActor403900Animation19624,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND]          = &_gActor403900Animation19800,
    [GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT]          = &_gActor403900Animation1A748,
    [GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT]    = &_gActor403900Animation1ACDC,
    [GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_FRONT]        = &_gActor403900Animation1AFA4,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT]           = &_gActor403900Animation1B180,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_EARLY_RELEASE]  = &_gActor403900Animation1E7A8,
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

/// The enemy task's three state handlers, which `_actor403900GolemTask`
/// picks by `Task::state`: the spawn setup, the frame handler that runs the
/// sequences, and the frame handler that unlinks the enemy and saves its pose
/// before running its own short sequence.
static const EnemyTaskFuncTable3 D_actor_403900_80131F18 = {
    _golemKnightBishopSpawn,
    _golemKnightBishopFrameState,
    _golemKnightBishopDeadState,
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

/// Records the living player's completed struggle as a GOLEM grab escape.
///
/// The release-hold message has no payload; messageId and both arguments are
/// ignored. Requires initialized Knight/Bishop work. A dead player leaves the
/// grab latch intact. Returns zero; the grab sequence performs the release.
static s32 _actor403900RecordGrabEscape(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    if (gPlayerStatus.hp > 0) {
        ((GolemKnightBishopWork*)task->work)->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_STRUGGLE;
    }
    return 0;
}

#include "../../shared/golem_knight_bishop_frame_capture.inc.c"

/// Dispatches the Bishop GOLEM's spawn, living frame or death frame.
///
/// Task state must be 0, 1 or 2, respectively; dispatch is unchecked. The second
/// spawn argument borrows a live Enemy. Spawn establishes task-owned work;
/// later frames require its model, animation and enemy storage until teardown.
static void _actor403900GolemTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_403900_80131F18;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
