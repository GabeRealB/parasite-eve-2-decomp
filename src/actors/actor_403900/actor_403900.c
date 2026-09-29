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
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gamemain.h"
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

extern Actor402200FrameStep D_actor_403900_801383DC[];

/// Per-roll wait lengths the wait state of `func_actor_403900_801329A4`
/// scales by `16 - field_70C`, indexed by a 4-bit `Gp_LcgState` draw.
extern s16 D_actor_403900_80153C3C[];

/// Per-roll state offsets the wait state adds to 2 when `field_6E8` is set.
extern u16 D_actor_403900_80153C5C[];

/// Cue word the countdown's expiry queues, a separate `D_` symbol in the
/// overlay's data.
extern s32 D_actor_403900_8013846C;

/// Cue word the approach's state 0 queues as it plants the actor on its box.
extern s32 D_actor_403900_80138468;

/// Cue word the fade-out in `func_actor_403900_80134968` queues.
extern s32 D_actor_403900_80138470;

/// Cue word the approach's frame 0x12 queues.
extern s32 D_actor_403900_80138474;

/// Base id of the actor's vocal cue: the `GpEnemy` work id's high nibble is
/// OR'd in as bits 8-11 of the cue id.
extern s32 D_actor_403900_80138478;

/// Cue-id table: `Actor402200Work::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `flags` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one; the branch sequences read `[field_712 + 8]`.
extern s32 D_actor_403900_80138424[];

/// Weighted 16-entry roll for `Actor402200Work::field_6E4`: indices 0-10 hold 0
/// and 11-15 hold 1, so the short approach is taken about a third of the time.
extern u16 D_actor_403900_80153C1C[];

/// Per-animation-id value `func_actor_403900_80137EF0` hands `func_800B4114`
/// as its fifth argument when it reseeds animation slots 1..0x12.
extern s16 D_actor_403900_801383B0[];

/// The spawn's tables: the task's next handler record, the `GpU16Pair`
/// `Gp_PackPair` packs into the third collision object, the `GpPairSrcE` whose
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
extern GpU16Pair               D_actor_403900_80153BF0[4];
extern GpPairSrcE              D_actor_403900_80153C00;
extern Actor402200Spot         D_actor_403900_80153C7C[];
extern Actor402200Region*      D_actor_403900_80153F04[];
extern s16*                    D_actor_403900_8015409C[];
extern GpAnimSet*              D_actor_403900_801540EC[22];

/// Per-difficulty HP above which the player always breaks the grab.
extern s16 D_actor_403900_80153C10[];

/// Animation block the grab's 0x3FF messages hand the player.
extern GpAnimSet* D_actor_403900_801540B4[5];

/// `func_800FDB18` argument record for the grab's finishing spark.
extern GpEffArg D_actor_403900_801540C8;

/// The two four-vertex index rows the trail's shaded quads take their corners
/// from, into the scratch block's six-entry x / y runs.
extern s16 D_actor_403900_801540D0[2][4];

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void func_actor_403900_801324E8(Task* arg0, s32 arg1);
static s32  func_actor_403900_80132D78(Task* arg0);
static void func_actor_403900_80132E34(Task* arg0);
static void func_actor_403900_80135D5C(Task* arg0);
static void func_actor_403900_80136184(Task* arg0);
static void func_actor_403900_801368E0(GpEnemy* arg0, Task* arg1);
static void func_actor_403900_80137444(GpEnemy* arg0, Task* arg1);
static void func_actor_403900_80137A20(GpEnemy* arg0, Task* arg1);
static void func_actor_403900_80137B78(Task* arg0);
static void func_actor_403900_80137CA8(Task* arg0);
static void func_actor_403900_80137D7C(Task* arg0);
static void func_actor_403900_80137E4C(Task* arg0);
static void func_actor_403900_80137EF0(Task* arg0);
static void func_actor_403900_80137FB4(Task* arg0);
static void func_actor_403900_80138070(Task* arg0);
static void func_actor_403900_801380DC(Task* arg0);
static void func_actor_403900_8013820C(GfxCoord* arg0, s32 arg1);

s32 func_actor_403900_801381E4(Task*);

extern GpAnimSet D_actor_403900_8013E26C;
extern GpAnimSet D_actor_403900_8013E724;
extern GpAnimSet D_actor_403900_8013EF30;
extern GpAnimSet D_actor_403900_8013FB68;
extern GpAnimSet D_actor_403900_80140C1C;
extern GpAnimSet D_actor_403900_801417C8;
extern GpAnimSet D_actor_403900_80145840;
extern GpAnimSet D_actor_403900_80145D64;
extern GpAnimSet D_actor_403900_801467AC;
extern GpAnimSet D_actor_403900_8014749C;
extern GpAnimSet D_actor_403900_80148DD8;
extern GpAnimSet D_actor_403900_80149BE8;
extern GpAnimSet D_actor_403900_8014AC60;
extern GpAnimSet D_actor_403900_8014B134;
extern GpAnimSet D_actor_403900_8014B444;
extern GpAnimSet D_actor_403900_8014B620;
extern GpAnimSet D_actor_403900_8014C568;
extern GpAnimSet D_actor_403900_8014CAFC;
extern GpAnimSet D_actor_403900_8014CDC4;
extern GpAnimSet D_actor_403900_8014CFA0;
extern GpAnimSet D_actor_403900_801505C8;
extern GpAnimSet D_actor_403900_80151998;
extern GpAnimSet D_actor_403900_80151F2C;
extern GpAnimSet D_actor_403900_8015298C;
extern GpAnimSet D_actor_403900_80153BC8;
extern GpU16Pair D_actor_403900_80153BF0[4];
extern TmdSource D_actor_403900_8013DBD8;
static void      func_actor_403900_80138344(Task*);

Actor403900MessageEntry D_actor_403900_801383A0[2] = {
    { 2014, { .call0 = func_actor_403900_801381E4 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

s16 D_actor_403900_801383B0[22] = {
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

s32 D_actor_403900_80138424[17] = {
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

s32 D_actor_403900_80138468 = 0x40160007;

s32 D_actor_403900_8013846C = 0x40160008;

s32 D_actor_403900_80138470 = 0x4016000F;

s32 D_actor_403900_80138474 = 0x40160010;

s32 D_actor_403900_80138478 = 0x40160011;

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

GpAnimSet D_actor_403900_8013E26C = {
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

GpAnimSet D_actor_403900_8013E724 = {
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

GpAnimSet D_actor_403900_8013EF30 = {
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

GpAnimSet D_actor_403900_8013FB68 = {
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

GpAnimSet D_actor_403900_80140C1C = {
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

GpAnimSet D_actor_403900_801417C8 = {
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

GpAnimSet D_actor_403900_80145840 = {
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

GpAnimSet D_actor_403900_80145D64 = {
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

GpAnimSet D_actor_403900_801467AC = {
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

GpAnimSet D_actor_403900_8014749C = {
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

GpAnimSet D_actor_403900_80148DD8 = {
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

GpAnimSet D_actor_403900_80149BE8 = {
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

GpAnimSet D_actor_403900_8014AC60 = {
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

GpAnimSet D_actor_403900_8014B134 = {
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

GpAnimSet D_actor_403900_8014B444 = {
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

GpAnimSet D_actor_403900_8014B620 = {
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

GpAnimSet D_actor_403900_8014C568 = {
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

GpAnimSet D_actor_403900_8014CAFC = {
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

GpAnimSet D_actor_403900_8014CDC4 = {
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

GpAnimSet D_actor_403900_8014CFA0 = {
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

GpAnimSet D_actor_403900_801505C8 = {
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

GpAnimSet D_actor_403900_80151998 = {
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

GpAnimSet D_actor_403900_80151F2C = {
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

GpAnimSet D_actor_403900_8015298C = {
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

GpAnimSet D_actor_403900_80153BC8 = {
    D_actor_403900_801532EC,
    D_actor_403900_80153BA0,
    { NULL, D_actor_403900_801529B4, NULL, NULL, D_actor_403900_80152B58, NULL, NULL, NULL },
};

GpU16Pair D_actor_403900_80153BF0[4] = { { 10, 10 }, { 45, 2 }, { 58, 2 }, { 999, 0 } };

GpPairSrcE D_actor_403900_80153C00 = { D_actor_403900_80153BF0, 800, 400, 2500, 7, 100, 20, 0, 0, 0 };

s16 D_actor_403900_80153C10[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

u16 D_actor_403900_80153C1C[16] = {
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

GpAnimSet* D_actor_403900_801540B4[5] = {
    NULL,
    &D_actor_403900_80151998,
    &D_actor_403900_80151F2C,
    &D_actor_403900_8015298C,
    &D_actor_403900_80153BC8,
};

GpEffArg D_actor_403900_801540C8 = { NULL, 300, 1 };

s16 D_actor_403900_801540D0[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc D_actor_403900_801540E0 = { 1, 96, func_actor_403900_80138344, { .model = &D_actor_403900_8013DBD8 } };

GpAnimSet* D_actor_403900_801540EC[22] = {
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

static void        func_actor_403900_80131F54(Task* arg0);
static void        func_actor_403900_80132688(Task* arg0);
static void        func_actor_403900_801329A4(Task* arg0);
static void        func_actor_403900_8013314C(Task* arg0);
static void        func_actor_403900_80133AEC(Task* arg0);
static void        func_actor_403900_80134194(Task* arg0);
static void        func_actor_403900_801347F4(Task* arg0);
static void        func_actor_403900_80134968(Task* arg0);
static void        func_actor_403900_8013539C(Task* arg0);
static void        func_actor_403900_801354B0(Task* arg0);
static void        func_actor_403900_80135630(Task* arg0);
static void        func_actor_403900_8013592C(Task* arg0);
static void        func_actor_403900_80135A24(Task* arg0);
static void        func_actor_403900_80135BE0(Task* arg0);
static inline void Actor403900_ReseedAnim(Task* arg0);
static inline void Actor403900_DrawShadow(Task* arg0);
static void        func_actor_403900_80136D9C(s32 otz);

/// Per-frame hit handler: applies the `func_800E0C10` push-back from the
/// `field_504` and (while bit 0x4000 of `field_49A` is set) `field_49C`
/// record tables to the root coordinate, ticks the `field_6C6` flinch
/// countdown, and for each kind-2 hit record in `field_49C` computes the
/// damage from the distance to the player, applies it to the `GpEnemy`,
/// spawns the hit sparks once per distinct id and hands the damage to
/// `func_actor_403900_801324E8` unless the vocal cue is armed.
static void func_actor_403900_80131F54(Task* arg0)
{
    s32                    lastId;
    Actor402200Work*       work;
    Actor402200HitScratch* head;
    Actor402200HitScratch* sc;
    Actor402200HitScratch* blk;
    GpEnemy*               enemy;
    GfxCoord*              coord;
    s32                    i;
    s32                    damage;
    s32                    kind;
    s32                    wait;
    s16                    t;

    lastId                              = 0;
    work                                = arg0->work;
    head                                = SCRATCH_HEAD(Actor402200HitScratch);
    blk                                 = head - 1;
    SCRATCH_HEAD(Actor402200HitScratch) = blk;
    sc                                  = blk;
    coord                               = arg0->extra.tmd->coords;
    enemy                               = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_504, &sc->delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].delta.vx.h.hi;
            coord->coord.t[1] += sc->delta.vy.h.hi;
            coord->coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_664;
            coord->coord.t[1] = work->field_668;
            coord->coord.t[2] = work->field_66C;
            break;
    }
    Gp_ClearRec18Occupied(work->field_504);

    if (work->field_49A & 0x4000) {
        switch (func_800E0C10(work->field_49C, &sc->delta, 3, NULL)) {
            case 0:
                break;
            case 1:
                coord->coord.t[0] += sc->delta.vx.h.hi;
                coord->coord.t[2] += sc->delta.vz.h.hi;
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
            work->field_49A |= 0x8000;
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
                sc->delta.vx.w  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w  = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                work->field_6D2 = (u32) ~(sc->delta.vx.w * coord->coord.m[0][2] +
                                          sc->delta.vy.w * coord->coord.m[1][2] +
                                          sc->delta.vz.w * coord->coord.m[2][2]) >>
                                  31;
                damage = Gp_ComputeDamage(work->field_49C[i].key.value,
                                          SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                      sc->delta.vy.w * sc->delta.vy.w +
                                                      sc->delta.vz.w * sc->delta.vz.w),
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
                    func_actor_403900_801324E8(arg0, damage);
                } else {
                    work->field_6F4 = 2;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_49C);
    if (work->field_584.flags & 1) {
        work->field_582 &= 0x7FFF;
        Gp_ClearRec18Occupied(&work->field_584);
    }
    SCRATCH_POP(Actor402200HitScratch);
}

/// Picks the damage reaction for the hit just taken, from the enemy's HP and
/// the damage `arg1`: at or below zero HP it silences both queued sound events
/// and enters the death sequence (9, or 10 while `field_6F0` is set); below a
/// tenth of `hpMax` the low-HP sequence (7, or 8 while `field_6F0` is set);
/// otherwise, when `field_6F2` is clear or `field_6EC` set, the flinch sequence
/// 5 for damage below 0x50 and 6 above. A sequence change restarts its state
/// and clears bit 0x8000 of `field_582`; the `field_6F0` variants leave a
/// sequence already held by `field_6F2` running.
static void func_actor_403900_801324E8(Task* arg0, s32 arg1)
{
    GpEnemy*         enemy = arg0->spawnArg2.pointer;
    s16              hp    = enemy->hp;
    Actor402200Work* work  = arg0->work;
    u32              state = 0;
    s32              max;

    if (hp <= 0) {
        state = 6;
        if (work->field_6F0 == 0) {
            state = 5;
        }
        if (work->field_6B8 != 0) {
            SndEvt_EnqueueType7(work->field_6B8, 1);
            work->field_6B8 = 0;
        }
        if (work->field_6BC != 0) {
            SndEvt_EnqueueType7(work->field_6BC, 1);
            work->field_6BC = 0;
        }
    } else if (max = enemy->param->hpMax, hp < max / 10) {
        state = 4;
        if (work->field_6F0 == 0) {
            state = 3;
        }
    } else if (work->field_6F2 == 0 || work->field_6EC != 0) {
        work->field_6F2 = 0;
        state           = 2;
        if (arg1 < 0x50) {
            state = 1;
        }
    }

    switch (state) {
        case 0:
            break;
        case 1:
            work->field_6CC  = 5;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 2:
            work->field_6CC  = 6;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 3:
            work->field_6CC  = 7;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 4:
            if (work->field_6F2 == 0) {
                work->field_6CC = 8;
                work->field_6CE = 0;
            }
            break;
        case 5:
            work->field_6CC  = 9;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 6:
            if (work->field_6F2 == 0) {
                work->field_6CC = 10;
                work->field_6CE = 0;
            }
            break;
    }
}

/// Sequence 0xB, the box scan. In state 0 it walks the `field_6FA` boxes at
/// `field_6B4`: a kind-0 box whose radius `field_2` holds the player's planar
/// offset from its centre (`field_4`, `field_6`) moves to state 1 and parks the
/// target position 0x5AA behind the player, raising bit 0x4000 of `field_5BA`
/// and `field_5DA`; a kind-1 box holding the player starts sequence 3 with
/// `field_70E` at 3 and its index in `field_708`. State 1 enters sequence 1
/// (and `field_70E` 1) unless the first record is occupied, clears the target
/// flags and the record, and drops back to state 0.
static void func_actor_403900_80132688(Task* arg0)
{
    u8*                    head;
    Actor402200BoxScratch* sc;
    Actor402200Work*       work;
    GfxCoord*              coord;
    s32                    i;

    head             = SCRATCH_HEAD(u8);
    work             = arg0->work;
    SCRATCH_HEAD(u8) = head - sizeof(Actor402200BoxScratch);
    sc               = (Actor402200BoxScratch*)(head - sizeof(Actor402200BoxScratch));
    switch (work->field_6CE) {
        case 0:
            for (i = 0; i < work->field_6FA; i++) {
                switch (work->field_6B4[i].field_0) {
                    case 0:
                        sc->out.vx = work->field_6B4[i].field_4 - Player_Status.coordMtx->t[0];
                        sc->out.vz = work->field_6B4[i].field_6 - Player_Status.coordMtx->t[2];
                        if (SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < work->field_6B4[i].field_2) {
                            work->field_6CE = 1;
                            coord           = gameGetPtrSlot(3)->extra.tmd->coords;
                            work->field_6E6 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                            sc->in.vx       = 0;
                            sc->in.vy       = 0;
                            sc->in.vz       = -0x5AA;
                            gte_SetRotMatrix(&coord->coord);
                            gte_ldv0(&sc->in);
                            gte_rtv0();
                            gte_stlvnl(&sc->out);
                            work->field_6A4 = Player_Status.coordMtx->t[0] + sc->out.vx;
                            work->field_6A8 = Player_Status.coordMtx->t[1];
                            SCRATCH_POP_BYTES(sizeof(Actor402200BoxScratch));
                            work->field_6AC  = Player_Status.coordMtx->t[2] + sc->out.vz;
                            work->field_5BA |= 0x4000;
                            work->field_5DA |= 0x4000;
                            return;
                        }
                        break;
                    case 1:
                        if (work->field_6B4[i].field_8 < Player_Status.coordMtx->t[0] &&
                            Player_Status.coordMtx->t[0] < work->field_6B4[i].field_C &&
                            Player_Status.coordMtx->t[2] < work->field_6B4[i].field_A &&
                            work->field_6B4[i].field_E < Player_Status.coordMtx->t[2]) {
                            work->field_6CC = 3;
                            work->field_6CE = 0;
                            work->field_70E = 3;
                            work->field_708 = i;
                            SCRATCH_POP_BYTES(sizeof(Actor402200BoxScratch));
                            return;
                        }
                        break;
                }
            }
            break;
        case 1:
            if (work->field_5F4.key.value == 0) {
                work->field_6CC = 1;
                work->field_70E = 1;
            }
            work->field_6CE  = 0;
            work->field_5BA &= 0xBFFF;
            work->field_5DA &= 0xBFFF;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
    }
    SCRATCH_POP_BYTES(sizeof(Actor402200BoxScratch));
}

/// State machine on `field_6CE`: 0 rolls a `field_6D4` wait, 1 counts it
/// down, 2 picks state 3 or 4 from `field_70E` and an LCG draw offset by
/// `field_710` (or 5 when `func_actor_403900_80132D78` reports a box hit), and 3-5
/// settle the result, walking `field_70C` up to 8.
static void func_actor_403900_801329A4(Task* arg0)
{
    Actor402200Work* work;

    work = arg0->work;
    switch (work->field_6CE) {
        case 0:
            work->field_6C8 = 0;
            work->field_6EC = 0;
            work->field_6EE = 0;
            if (work->field_6E8 != 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6CE = D_actor_403900_80153C5C[(Gp_LcgState >> 16) & 0xF] + 2;
                work->field_6EE = 1;
                func_actor_403900_80132E34(arg0);
                work->field_6E4 = 0;
            } else if (work->field_6E4 == 0) {
                work->field_6D4 = (D_actor_403900_80153C3C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF] * (0x10 - work->field_70C)) / 16;
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
            if (work->field_70E != 3 && func_actor_403900_80132D78(arg0) != 0) {
                work->field_6CE = 5;
                work->field_710 = 0;
                break;
            }
            if (work->field_70E == 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((s32)((Gp_LcgState >> 16) & 0xF) < work->field_710 + 10) {
                    work->field_6CE = 4;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 3;
                    work->field_710++;
                }
            } else if (work->field_70E == 2) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((s32)((Gp_LcgState >> 16) & 0xF) < work->field_710 + 8) {
                    work->field_6CE = 3;
                    work->field_710 = 0;
                } else {
                    work->field_6CE = 4;
                    work->field_710++;
                }
            } else {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6CE = ((Gp_LcgState >> 16) & 0xF) < 8 ? 3 : 4;
                work->field_710 = 0;
            }
            func_actor_403900_80132E34(arg0);
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
            work->field_5BA &= ~0x4000;
            work->field_5DA &= ~0x4000;
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
            work->field_5BA &= ~0x4000;
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

/// Reports whether the player stands inside one of the actor's kind-1 boxes:
/// walks the `field_6FA` entries at `field_6B4` and, on the first kind-1 entry
/// whose box holds the player's world position (x between `field_8` and
/// `field_C`, z between `field_E` and `field_A`), parks its index in
/// `field_708` and answers 1. Otherwise it answers 0.
static s32 func_actor_403900_80132D78(Task* arg0)
{
    Actor402200Work* work;
    s16              count;
    s32              i;

    work  = arg0->work;
    count = work->field_6FA;
    for (i = 0; i < count; i++) {
        if (work->field_6B4[i].field_0 == 1) {
            if ((work->field_6B4[i].field_8 < Player_Status.coordMtx->t[0]) &&
                (Player_Status.coordMtx->t[0] < work->field_6B4[i].field_C)) {
                if ((Player_Status.coordMtx->t[2] < work->field_6B4[i].field_A) &&
                    (work->field_6B4[i].field_E < Player_Status.coordMtx->t[2])) {
                    work->field_708 = i;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/// Parks the actor's target position off the player (`gameGetPtrSlot(3)`).
/// In state 3 it takes `field_6E6` from the player's heading and places the
/// target 0x5AA behind the player, raising bit 0x4000 of `field_5BA` and
/// `field_5DA`; in state 4 it rolls an angle from `Gp_LcgState` (anywhere, or
/// within a quarter turn either side while `field_6E8` is clear), derives
/// `field_5DC` / `field_5E0` from it, adds the player's heading and places the
/// target 0x4B out along the result, raising bit 0x4000 of `field_5BA`.
static void func_actor_403900_80132E34(Task* arg0)
{
    u8*                       head;
    Actor402200OffsetScratch* sc;
    Actor402200Work*          work;
    GfxCoord*                 coord;
    u32                       random;
    s32                       angle;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(Actor402200OffsetScratch);
    sc               = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
    work             = arg0->work;
    if (work->field_6CE == 3) {
        coord           = gameGetPtrSlot(3)->extra.tmd->coords;
        work->field_6E6 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        sc->in.vz       = -0x5AA;
        sc->in.vx       = 0;
        sc->in.vy       = 0;
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&sc->in);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        work->field_6A4  = Player_Status.coordMtx->t[0] + sc->out.vx;
        work->field_6A8  = Player_Status.coordMtx->t[1];
        work->field_6AC  = Player_Status.coordMtx->t[2] + sc->out.vz;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = -0x7D0;
        work->field_5DC  = 0;
        work->field_5BA |= 0x4000;
        work->field_5DA |= 0x4000;
    } else if (work->field_6CE == 4) {
        if (work->field_6E8 != 0) {
            Gp_LcgState     = (Gp_LcgState * 5) + 0x71357911;
            work->field_6E6 = (Gp_LcgState >> 16) & 0xFFF;
        } else {
            random      = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState = random;
            angle       = (random >> 16) & 0x3FF;
            if (!((random >> 16) & 0x400)) {
                angle = -angle;
            }
            work->field_6E6 = angle;
        }
        work->field_5DC  = (u32)(rsin(work->field_6E6) * 0x7D) >> 8;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = (u32)(rcos(work->field_6E6) * 0x7D) >> 8;
        coord            = gameGetPtrSlot(3)->extra.tmd->coords;
        work->field_6E6  = (work->field_6E6 + (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF)) & 0xFFF;
        sc->in.vx        = (u32)(rsin(work->field_6E6) * 0x4B) >> 8;
        sc->in.vz        = (u32)(rcos(work->field_6E6) * 0x4B) >> 8;
        work->field_6A4  = Player_Status.coordMtx->t[0] + sc->in.vx;
        work->field_6A8  = Player_Status.coordMtx->t[1];
        work->field_6AC  = Player_Status.coordMtx->t[2] + sc->in.vz;
        work->field_5BA |= 0x4000;
    }
    SCRATCH_POP_BYTES(sizeof(Actor402200OffsetScratch));
}

/// Runs the actor's hold sequence on the player (the same 0x3F8 / 0x3FF
/// message pair `func_actor_403900_8013314C` uses to take a hold). State 0
/// asks the player for range 0x19 while enemies remain; on success it plants
/// the display object at `field_6A4`, places the player 0x5AA in front of it
/// with message 0x3E9 and queues a cue. States 1 and 2 step the player's
/// animation. State 3 waits out `field_6D6`, then every 0x14 frames decides
/// whether the hold ends: always when `Player_Status.hp` is above the
/// per-difficulty `D_actor_403900_80153C10`, otherwise by an LCG roll whose
/// chance grows with the attempt count `field_6F6`; a raised `field_6F4`
/// ends it early. State 5 either reacts to `field_6F4` or, at frame 0x1A,
/// spawns the spark, sends message 0x400 and clears `Player_Status.hp`; state 7
/// then loads file 9/0x1E and queues cue 0x70010001 once the CD is idle.
static void func_actor_403900_8013314C(Task* arg0)
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
    player = gameGetPtrSlot(3);
    SCRATCH_PUSH_BYTES(sizeof(Actor402200GrabScratch));
    sc     = SCRATCH_HEAD(Actor402200GrabScratch);
    pcoord = player->extra.tmd->coords;
    flag   = 0;
    switch (work->field_6CE) {
        case 0:
            if (((GameActor*)player->work)->field_954 != 2 && Player_Status.hp > 0) {
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
                    snd = (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 6;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(pcoord), (s8)gpGetObjDepth(pcoord));
                } else {
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 1:
            sc->anim.source.sets          = D_actor_403900_801540B4;
            sc->anim.animationId          = 1;
            sc->anim.blend                = 0;
            sc->anim.blendFrames          = 0;
            sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
            work->field_6CE = 2;
            work->field_6DC = 0x3C;
            work->field_6DA = 1;
            work->field_6DE = 0x1E;
            work->field_6B8 = D_actor_403900_80138468 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
                sc->anim.blend                = 0;
                sc->anim.blendFrames          = 0;
                sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
                Gp_ArmStateF0(1);
                work->field_70A = 0;
                if (work->field_6C6 == 0) {
                    work->field_49A |= 0x8000;
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
                    if (Player_Status.hp > D_actor_403900_80153C10[Gp_StateF0.field_2B]) {
                        if (work->field_6F8 == 0) {
                            work->field_6F8 = 1;
                        } else {
                            chance = work->field_6F6 * (0x32 - (Player_Status.hp * 100) / Player_Status.hpMax) / 2;
                            if (chance > 0) {
                                chance      = (chance * 0xFFF) / 100;
                                Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                                if ((s32)((Gp_LcgState >> 16) & 0xFFF) < chance) {
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
                        sc->anim.blend                = 0;
                        sc->anim.blendFrames          = 0;
                        sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
                    } else {
                        work->field_6D4 = 0x14;
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(D_actor_403900_80153BF0, 0), 0);
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
                        work->field_6BC = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                        SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    }
                } else {
                    func_actor_403900_801324E8(arg0, work->field_70A);
                    work->field_71A = 0;
                }
                work->field_718               = 2;
                work->field_6F4               = 0;
                sc->anim.source.sets          = D_actor_403900_801540B4;
                sc->anim.animationId          = 4;
                sc->anim.blend                = 0;
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
            func_actor_403900_801380DC(arg0);
            break;
        case 5:
            if (work->field_6C4 < 0x1A) {
                if (work->field_6F4 != 0) {
                    work->field_6C0               = 0xC;
                    work->field_6F4               = 0;
                    sc->anim.source.sets          = D_actor_403900_801540B4;
                    sc->anim.animationId          = 4;
                    sc->anim.blend                = 0;
                    sc->anim.blendFrames          = 0;
                    sc->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    Gp_DispatchMsgPtr(player, 0x3FF, &sc->anim, 0);
                    work->field_6D4 = 0x69;
                    work->field_6DA = 3;
                    work->field_6DC = 0x4B;
                    work->field_6CE = 4;
                    work->field_6DE = 0x1E;
                    work->field_6BC = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            } else if (work->field_6C4 == 0x1A) {
                ((GameActor*)player->work)->field_956 = 0xA;
                work->field_6CE                       = 7;
                work->field_6D4                       = 0;
                gGameSession->deathRestartDelay       = 0x5A;
                gGameSession->areaBgmCountdown        = 0x7F;
                sc->in.vy                             = -0x96;
                sc->in.vx                             = 0;
                sc->in.vz                             = 0xC8;
                func_800FDB18(1, &gameGetPtrSlot(3)->extra.tmd->coords[4], &sc->in, &D_actor_403900_801540C8);
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                Gp_DispatchMsg(player, 0x400, 0, 0);
                Player_Status.hp = 0;
            }
            break;
        case 6:
            work->field_6C8 = -0xA;
            func_actor_403900_801380DC(arg0);
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
                        coord = gameGetPtrSlot(3)->extra.tmd->coords;
                        SndEvt_EnqueueType6(0x70010001, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        work->field_6D4 = 2;
                    }
                    break;
                case 2:
                    break;
            }
            break;
    }
    SCRATCH_POP_BYTES(sizeof(Actor402200GrabScratch));
}

/// Runs the actor's approach-and-strike sequence. State 0 aims the display
/// object along `field_6E6`, parks it at `field_6A4`..`field_6AC` and rolls
/// `field_6E4`; a 1 with `field_6E8` clear parks the state at 1 with a
/// 0xF..0x1E frame budget, otherwise the state goes to 3 with a 0x1E..0x2D
/// budget, or to 4 when `field_6E8` is set, and the cue `field_6B8` is queued.
/// The budget is split into `field_6DC` (two thirds) and `field_6DE` (the
/// remainder), which the strike states consume in turn. States 1 to 4 and 6
/// count `field_6D4` down: 1 rolls a 0x3C..0x4B wait into state 2, 2 splits a
/// fresh budget into state 6 and releases the link node, 3 and 4 fall through
/// to the next state when the budget runs out and abort back to state 0 while
/// `field_70A` is positive, and 6 returns to state 0. State 5 reacts to the
/// animation's `field_6C4`: 0x14 and 0x1C bind `field_56C` to a body part and
/// queue the strike cue, and 0x23 ends the strike in state 6.
static void func_actor_403900_80133AEC(Task* arg0)
{
    Actor402200OffsetScratch* sc;
    Actor402200Work*          work;
    GfxCoord*                 coord;
    s32                       cue;
    u32                       random;
    u16                       delay;
    s16                       part;
    s16                       timer;

    SCRATCH_PUSH_BYTES(sizeof(Actor402200OffsetScratch));
    sc    = SCRATCH_HEAD(Actor402200OffsetScratch);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_6CE) {
        case 0:
            work->field_6C0 = 4;
            work->field_6E4 = D_actor_403900_80153C1C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            sc->in.vx       = 0;
            sc->in.vy       = (work->field_6E6 + 0x800) & 0xFFF;
            sc->in.vz       = 0;
            RotMatrix(&sc->in, &coord->coord);
            coord->coord.t[0] = work->field_6A4;
            coord->coord.t[1] = work->field_6A8;
            coord->coord.t[2] = work->field_6AC;
            if (work->field_6E4 == 1 && work->field_6E8 == 0) {
                random          = Gp_LcgState * 5 + 0x71357911;
                delay           = ((random >> 16) & 0xF) + 0xF;
                work->field_6CE = 1;
                work->field_6DA = 4;
                Gp_LcgState     = random;
                work->field_6D4 = delay;
                part            = delay * 2 / 3;
                work->field_6DC = part;
                work->field_6DE = delay - part;
            } else {
                work->field_6E4 = 0;
                if (work->field_6E8 == 0) {
                    timer           = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 0x1E;
                    work->field_6CE = 3;
                    work->field_6D4 = timer;
                    part            = timer * 2 / 3;
                    work->field_6DC = part;
                    work->field_6DE = work->field_6D4 - part;
                } else {
                    work->field_6CE = 4;
                    work->field_6DC = 0x14;
                    work->field_6D4 = 0;
                    work->field_6DE = 0xA;
                }
                work->field_6DA = 1;
                work->field_6B8 = D_actor_403900_80138468 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                work->field_70A = 0;
                work->field_6F2 = 1;
            }
            work->field_6D6 = 1;
            work->field_6E8 = 0;
            break;
        case 1:
            if (work->field_6D6 != 0) {
                if (work->field_6C6 == 0) {
                    work->field_494  = 0;
                    work->field_49A |= 0x8000;
                }
                work->field_6D6 = 0;
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0 || work->field_6E8 != 0) {
                work->field_6CE = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((Gp_LcgState >> 16) & 0xF) + 0x3C;
            }
            break;
        case 2:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0 || work->field_6E8 != 0) {
                work->field_6CE = 6;
                work->field_6DA = 6;
                if (work->field_6E8 != 0) {
                    work->field_6DC = 5;
                    work->field_6DE = 3;
                    work->field_6D4 = work->field_6DC + work->field_6DE;
                } else {
                    work->field_6DC = 8;
                    work->field_6DE = 8;
                    work->field_6D4 = work->field_6DC + work->field_6DE;
                }
                Gp_ClearNodeSlots(&((GpEnemy*)arg0->spawnArg2.pointer)->node);
            }
            break;
        case 3:
            if (work->field_6D6 != 0) {
                if (work->field_6C6 == 0) {
                    work->field_49A |= 0x8000;
                    work->field_494  = work->field_716 | 0x30000;
                }
                work->field_6D6 = 0;
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CE = 4;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = (Gp_LcgState >> 16) & 0xF;
            } else if (work->field_70A > 0) {
                work->field_6CC = 4;
                work->field_6CE = 0;
                work->field_6DA = 7;
                if (work->field_6B8 != 0) {
                    SndEvt_EnqueueType7(work->field_6B8, 1);
                    work->field_6B8 = 0;
                }
            }
            break;
        case 4:
            if (work->field_6D6 != 0) {
                if (work->field_6C6 == 0) {
                    work->field_49A |= 0x8000;
                    work->field_494  = work->field_716 | 0x30000;
                }
                work->field_6D6 = 0;
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6C0 = 5;
                work->field_6CE = 5;
                work->field_6D4 = 0;
            } else if (work->field_70A > 0) {
                work->field_6CC = 4;
                work->field_6CE = 0;
                work->field_6DA = 7;
            }
            break;
        case 5:
            if (work->field_6C4 == 0x14) {
                work->field_56C  = &arg0->extra.tmd->coords[8];
                work->field_574  = 0;
                work->field_576  = 0;
                work->field_578  = 0;
                work->field_580  = 0x12C;
                work->field_57C  = Gp_PackPair(D_actor_403900_80153BF0, 1);
                work->field_582 |= 0x8000;
                cue              = D_actor_403900_80138474 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(cue, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else if (work->field_6C4 == 0x1C) {
                work->field_56C = &arg0->extra.tmd->coords[12];
                cue             = D_actor_403900_80138474 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(cue, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 == 0x23) {
                work->field_6CE  = 6;
                work->field_6D4  = 0x1E;
                work->field_6DA  = 3;
                work->field_6DC  = 0x14;
                work->field_6DE  = 0xA;
                work->field_582 &= 0x7FFF;
                work->field_6BC  = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            break;
        case 6:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
                work->field_6F2 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(sizeof(Actor402200OffsetScratch));
}

/// Runs the actor's approach sequence off the box it last hit. State 0 plants
/// the display object on that box, faces it along the box heading and queues
/// the cue `field_6B8`, parking the state at 1 when the player is within 0xDAC
/// and at 2 otherwise. States 1 and 2 re-aim for `field_6D6` frames; state 2
/// closes in until the player is within 0xA8C (state 3) or turns away by more
/// than 0x180 (state 4). State 3 steps `field_6C8` through the frame table
/// `D_actor_403900_801383DC` and fires its per-frame events; state 4 counts
/// `field_6D4` down back to state 0.
static void func_actor_403900_80134194(Task* arg0)
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

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(Actor402200OffsetScratch);
    sc               = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
    work             = arg0->work;
    state            = work->field_6CE;
    coord            = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            coord->coord.t[0] = work->field_6B4[work->field_708].field_4;
            coord->coord.t[1] = Player_Status.coordMtx->t[1];
            coord->coord.t[2] = work->field_6B4[work->field_708].field_6;
            sc->in.vx         = 0;
            sc->in.vy         = work->field_6B4[work->field_708].field_2;
            sc->in.vz         = 0;
            RotMatrix(&sc->in, &coord->coord);
            sc->out.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->out.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
            work->field_6B8 = D_actor_403900_80138468 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            Gp_ArmStateF0(1);
            if (work->field_6C6 == 0) {
                work->field_49A |= 0x8000;
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
                work->field_62A &= 0x3FFF;
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
            sc->out.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            sc->out.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xA8C) {
                work->field_6C0  = 7;
                work->field_6CE  = 3;
                work->field_62A &= 0x3FFF;
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
                    work->field_62A &= 0x3FFF;
                    work->field_6BC  = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            }
            if (work->field_70A >= 0xFA) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= 0x3FFF;
            }
            break;
        case 3:
            for (i = 0; work->field_6C4 > D_actor_403900_801383DC[i].frame; i++) {
            }
            work->field_6C8 = D_actor_403900_801383DC[i].value;
            if (work->field_6C4 == 0x12) {
                snd = D_actor_403900_80138474 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 == 0x14) {
                work->field_56C  = arg0->extra.tmd->coords;
                work->field_576  = -0x4B0;
                work->field_578  = 0x1F4;
                work->field_574  = 0;
                work->field_580  = 0x3E8;
                work->field_57C  = Gp_PackPair(D_actor_403900_80153BF0, 2);
                work->field_582 |= 0x8000;
            }
            if (work->field_6C4 == 0x20) {
                work->field_6F2  = 0;
                work->field_582 &= 0x7FFF;
            }
            if (work->field_6C4 == 0x5A) {
                work->field_6DA = 3;
                work->field_6DC = 0x14;
                work->field_6DE = 0xA;
                work->field_6CE = 4;
                work->field_6D4 = work->field_6DC + 0xA;
                work->field_6BC = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
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
    SCRATCH_POP_BYTES(sizeof(Actor402200OffsetScratch));
}

/// Cue body of the enemy's attack: state 0 arms animation `field_6C0`, sets
/// the cue state and rolls the countdown `field_6D4` from `Gp_LcgState`,
/// raising the hit descriptor `field_494`/`field_49A` while no flinch is
/// already running. State 1 ticks the countdown down and, on the frame it
/// runs out, arms the `field_6DA`/`field_6DC`/`field_6DE`/`field_6E0` timers,
/// clears the cue state and `field_6CC`, and queues the actor's cue, panned
/// and depth-attenuated from the display object.
static void func_actor_403900_801347F4(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              pan;
    u32              random;
    s16              timer;

    SCRATCH_PUSH_BYTES(8);
    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_6C0 = 0xB;
            work->field_6CE = 1;
            random          = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState     = random;
            work->field_6D4 = (u16)(((random >> 16) & 0x1F) + 0x2D);
            if (work->field_6C6 == 0) {
                work->field_49A |= 0x8000;
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
                work->field_6BC = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)gpGetObjDepth(coord));
            }
            break;
    }
    SCRATCH_POP_BYTES(8);
}

/// Runs the actor's fade sequence off `field_6DA`. States 1 / 3 fade the
/// display object's `field_2C` and the `field_6D8` / `field_6E2` shades up and
/// down, releasing the queued cues as they finish; state 4 fades to 0xB00 and
/// snapshots the root matrix into `field_674`, and state 6 winds `scale.vx` /
/// `scale.vy` down before resetting the root matrix to identity. States 7-9
/// flicker between two LCG-rolled timings, spawning effect 0x600E0 at the
/// fourth part on odd animation frames.
static void func_actor_403900_80134968(Task* arg0)
{
    SVECTOR*         sc;
    Actor402200Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GpMtxWords*      m;
    s32              snd;
    s32              pan;
    s32              v;
    s32              w;
    s32              sy;
    s32              y;
    u32              random;
    s16              t;

    sc    = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    coord = obj->coords;
    switch (work->field_6DA) {
        case 0:
            arg0->extra.tmd->flags = 0x80;
            work->field_6E2        = -1;
            work->field_49A       &= 0x7FFF;
            if (work->field_6B8 != 0) {
                SndEvt_EnqueueType7(work->field_6B8, 1);
                work->field_6B8 = 0;
            }
            if (work->field_6BC != 0) {
                SndEvt_EnqueueType7(work->field_6BC, 1);
                work->field_6BC = 0;
            }
            break;
        case 1:
            obj->lightLevel += 0x1000 / work->field_6DE;
            if (obj->lightLevel >= 0x1000) {
                obj->lightLevel = 0x1000;
                t               = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8 = t;
                if (t <= 0) {
                    work->field_6D8 = 0;
                    work->field_6DA = 2;
                    if (work->field_6B8 != 0) {
                        SndEvt_EnqueueType7(work->field_6B8, 1);
                        work->field_6B8 = 0;
                    }
                }
            }
            t               = work->field_6E2 + 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t >= 0x80) {
                work->field_6E2 = 0x80;
            }
            break;
        case 2:
            work->field_6E2 = 0x80;
            if (work->field_6B8 != 0) {
                SndEvt_EnqueueType7(work->field_6B8, 1);
                work->field_6B8 = 0;
            }
            if (work->field_6BC != 0) {
                SndEvt_EnqueueType7(work->field_6BC, 1);
                work->field_6BC = 0;
            }
            break;
        case 3:
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0xFF) {
                work->field_6D8  = 0xFF;
                obj->lightLevel -= 0x1000 / work->field_6DE;
                if (obj->lightLevel <= 0) {
                    obj->lightLevel = 0;
                    work->field_6DA = 0;
                    if (work->field_6BC != 0) {
                        SndEvt_EnqueueType7(work->field_6BC, 1);
                        work->field_6BC = 0;
                    }
                    snd = D_actor_403900_80138470 | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
                }
            }
            t               = work->field_6E2 - 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t < 0) {
                work->field_6E2 = -1;
            }
            break;
        case 4:
            obj->lightLevel += 0xB00 / work->field_6DE;
            if (obj->lightLevel >= 0xB00) {
                obj->lightLevel = 0xB00;
                t               = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8 = t;
                if (t <= 0) {
                    work->field_6DA = 5;
                    work->field_6D8 = 0;
                    work->scale.vx  = 0x1000;
                    work->scale.vy  = 0x1000;
                    work->scale.vz  = 0x1000;
                    work->field_674 = arg0->extra.tmd->coords[0].coord;
                    work->field_6D0 = 0;
                }
            }
            work->field_6E2 = -1;
            break;
        case 5:
            work->field_6E2 = -1;
            break;
        case 6:
            work->field_49A &= 0x7FFF;
            switch (work->field_6D0) {
                case 0:
                    y = work->scale.vy;
                    if (work->field_6E8 != 0) {
                        sy = y - 0x400;
                    } else {
                        sy = y - 0x200;
                    }
                    work->scale.vy = sy;
                    if (sy <= 0x800) {
                        work->field_6D0 = 1;
                    }
                    break;
                case 1:
                    if (work->field_6E8 != 0) {
                        v              = work->scale.vx - 0x200;
                        w              = work->scale.vy + 0x400;
                        work->scale.vx = v;
                        work->scale.vy = w;
                    } else {
                        v              = work->scale.vx - 0x100;
                        w              = work->scale.vy + 0x200;
                        work->scale.vx = v;
                        work->scale.vy = w;
                    }
                    if (work->scale.vx <= 0x800) {
                        work->field_6D0 = 2;
                    }
                    break;
            }
            func_actor_403900_80137CA8(arg0);
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0xFF) {
                work->field_6D8  = 0xFF;
                obj->lightLevel -= 0x1000 / work->field_6DE;
                if (obj->lightLevel <= 0) {
                    obj->lightLevel        = 0;
                    work->field_6DA        = 0;
                    m                      = (GpMtxWords*)&arg0->extra.tmd->coords[0].coord;
                    m->m00_m01             = 0x1000;
                    m->m02_m10             = 0;
                    m->m11_m12             = 0x1000;
                    m->m20_m21             = 0;
                    m->m22                 = 0x1000;
                    arg0->extra.tmd->flags = 0x80;
                }
            }
            work->field_6E2 = -1;
            break;
        case 7:
            work->field_6DA = 8;
            work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
            t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
            work->field_6DC = t;
            work->field_6DE = t;
            break;
        case 8:
            t               = work->field_6D8 + 0xFF / work->field_6DC;
            work->field_6D8 = t;
            if (t >= 0x80) {
                work->field_6D8  = 0x80;
                obj->lightLevel -= 0x1000 / work->field_6DE;
                if (obj->lightLevel <= 0x800) {
                    obj->lightLevel = 0x800;
                }
            }
            t               = work->field_6E2 - 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t < 0) {
                work->field_6E2 = -1;
            }
            t               = work->field_6E0 - 1;
            work->field_6E0 = t;
            if (t <= 0) {
                work->field_6DA = 9;
                work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
                t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
                work->field_6DC = t;
                work->field_6DE = t;
            }
            if (work->field_6EA == 0) {
                work->field_6EA = 1;
            }
            if (work->field_6C4 & 1) {
                sc->vx = 0;
                sc->vy = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF);
                sc->vz = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF;
                Gp_SpawnEff(0x600E0, &arg0->extra.tmd->coords[3], 0x100, sc);
            }
            break;
        case 9:
            obj->lightLevel += 0x1000 / work->field_6DE;
            if (obj->lightLevel >= 0x1000) {
                obj->lightLevel = 0x1000;
                t               = work->field_6D8 - 0xFF / work->field_6DC;
                work->field_6D8 = t;
                if (t <= 0) {
                    work->field_6D8 = 0;
                }
            }
            t               = work->field_6E2 + 0x80 / work->field_6DC;
            work->field_6E2 = t;
            if (t >= 0x80) {
                work->field_6E2 = 0x80;
            }
            t               = work->field_6E0 - 1;
            work->field_6E0 = t;
            if (t <= 0) {
                work->field_6DA = 8;
                work->field_6E0 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF) + 2;
                t               = work->field_6E0 + (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF);
                work->field_6DC = t;
                work->field_6DE = t;
            }
            if (work->field_6C4 & 1) {
                sc->vx = 0;
                sc->vy = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF);
                sc->vz = ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xFF;
                Gp_SpawnEff(0x600E0, &arg0->extra.tmd->coords[3], 0x100, sc);
            }
            break;
    }
    SCRATCH_POP_BYTES(8);
}

/// Runs the actor's animation-reseed sequence. State 0 puts the slot set on
/// animation 8, clears `field_6C8` and drops the state to 1; unless the mode at
/// `field_6EC` is already 1 it also arms the `field_6DA`/`field_6DC`/`field_6DE`
/// timers and queues the actor's cue, panned and depth-attenuated from the
/// display object. State 1 waits for the animation to reach 0x37 frames and
/// then puts the state back to 0, flipping the mode to 2 and raising
/// `field_6CC` if it was 1.
static void func_actor_403900_8013539C(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              pan;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_6C0 = 8;
            work->field_6CE = 1;
            work->field_6C8 = 0;
            if (work->field_6EC != 1) {
                work->field_6DA = 3;
                work->field_6DC = 0x1E;
                work->field_6DE = 0xF;
                work->field_6BC = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)gpGetObjDepth(coord));
                break;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x37) {
                if (work->field_6EC == state) {
                    work->field_6CC = 4;
                    work->field_6EC = 2;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
    }
}

/// Runs the actor's attack sequence. State 0 puts the slot set on animation 9
/// or 0xA, whichever `field_6D2` selects, and parks the state at 1 or 2 to
/// match; unless the mode at `field_6EC` is already 1 it also arms the
/// `field_6DA`/`field_6DC`/`field_6DE` timers and queues the actor's cue,
/// panned and depth-attenuated from the display object. States 1 and 2 wait out
/// their own animation - `field_6C4` at 0x50 and 0x3B frames - and then put the
/// state back to 0, flipping the mode to 2 and raising `field_6CC` when it was
/// still 1.
static void func_actor_403900_801354B0(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              pan;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_6D2 == 1) {
                work->field_6C0 = 9;
                work->field_6CE = 1;
            } else {
                work->field_6C0 = 0xA;
                work->field_6CE = 2;
            }
            work->field_6C8 = 0;
            if (work->field_6EC != 1) {
                work->field_6DA = 3;
                work->field_6DC = 0x1E;
                work->field_6DE = 0xF;
                work->field_6BC = D_actor_403900_8013846C | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)gpGetObjDepth(coord));
                break;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x50) {
                if (work->field_6EC == state) {
                    work->field_6CC = 4;
                    work->field_6EC = 2;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x3B) {
                if (work->field_6EC == 1) {
                    work->field_6CC = 4;
                    work->field_6EC = state;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
    }
}

/// Runs the actor's branch sequence. State 0 puts the slot set on animation
/// 0xD or 0x11, whichever `field_6D2` selects, and parks the state at 1 or 2 to
/// match. States 1 and 2 queue the actor's cue at frame 0x2C / 0x19 and, once
/// `field_6C4` reaches 0x42 / 0x31, move to state 3 with an LCG-rolled
/// `field_6D4` countdown; states 3 and 4 then alternate on that countdown.
static void func_actor_403900_80135630(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              snd;
    s32              anim;
    u32              random;
    s16              timer;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 2;
                work->field_6F0 = 2;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 7;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= 0x4000;
            work->field_502 &= 0xBFFF;
            break;
        case 1:
            if (work->field_6C4 == 0x2C) {
                snd = D_actor_403900_80138424[work->field_712 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 >= 0x42) {
                work->field_6C0 = 0x10;
                work->field_6CE = 3;
                work->field_6F2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            if (work->field_714 == 1) {
                work->field_714 = 2;
            }
            break;
        case 2:
            if (work->field_6C4 == 0x19) {
                snd = D_actor_403900_80138424[work->field_712 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 >= 0x31) {
                work->field_6C0 = 0x14;
                work->field_6CE = 3;
                work->field_6F2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            if (work->field_714 == 1) {
                work->field_714 = 2;
            }
            break;
        case 3:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                anim = 0x13;
                if (work->field_6F0 == 1) {
                    anim = 0xF;
                }
                work->field_6D4 = 0xA;
                work->field_6C0 = anim;
                work->field_6CE = 4;
            }
            break;
        case 4:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                anim = 0x14;
                if (work->field_6F0 == 1) {
                    anim = 0x10;
                }
                work->field_6C0 = anim;
                work->field_6CE = 3;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            break;
    }
}

/// Sequence 8, the low-HP turn: state 0 picks animation 0xE (and state 1) when
/// `field_6F0` is 1, otherwise 0x12 and state 2; states 1 and 2 wait for the
/// frame counter to reach 0x10 or 0x16, then switch to animation 0x10 or 0x14,
/// enter sequence 7 at state 3 and arm the `field_6D4` countdown from the
/// `Gp_LcgState` LCG (0..0x3F).
static void func_actor_403900_8013592C(Task* arg0)
{
    Actor402200Work* work;
    s16              state;
    s32              next;

    work  = arg0->work;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                work->field_6C0 = 0x10;
                work->field_6CC = 7;
                work->field_6CE = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                work->field_6C0 = 0x14;
                work->field_6CC = 7;
                work->field_6CE = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}

/// The enemy task's three state handlers, which `func_actor_403900_80138344`
/// picks by `Task::state`: the spawn setup, the frame handler that runs the
/// sequences, and the frame handler that unlinks the enemy and saves its pose
/// before running its own short sequence.
static const GpEnemyTaskFuncTable3 D_actor_403900_80131F18 = {
    func_actor_403900_80137444,
    func_actor_403900_80137A20,
    func_actor_403900_801368E0,
};

/// Sequence 9, the finishing branch. State 0 puts the slot set on animation
/// 0xD or 0x11, whichever `field_6D2` selects, records the side in `field_6F0`,
/// arms a 0x42 / 0x31 frame countdown and the `field_6DA`..`field_6DE` timers,
/// and raises bit 0x4000 of `field_49A`. State 1 queues the cue
/// `[field_712 + 8]` at frame 0x2C (0x19 for the second side) and, when the
/// countdown runs out, parks 2 in the context's `field_30` and drops back to
/// state 0.
static void func_actor_403900_80135A24(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              snd;
    s32              pan;
    s32              frames;
    s16              timer;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_6D4 = 0x42;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 1;
                work->field_6F0 = 2;
                work->field_6D4 = 0x31;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 1;
            work->field_6DC  = 0x14;
            work->field_6DE  = 0xA;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= 0x4000;
            work->field_502 &= 0xBFFF;
            break;
        case 1:
            if (work->field_714 == state) {
                work->field_714 = 2;
            }
            frames = 0x19;
            if (work->field_6F0 == state) {
                frames = 0x2C;
            }
            if (work->field_6C4 == frames) {
                snd = D_actor_403900_80138424[work->field_712 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                arg0->state     = 2;
                work->field_6CE = 0;
                work->field_6F2 = 0;
            }
            break;
    }
}

/// Fires the cue pair the work block's `field_712` selects: while the second
/// animation slot carries `flags` bit 0x20 or 0x10, a sound is queued on the
/// frame that bit has just dropped from `Actor402200Work::field_6CA`, panned
/// and depth-attenuated from the actor's display object. The cue id is the
/// matching word of `D_actor_403900_80138424` with the `GpEnemy` work id's high
/// nibble in bits 8-11, and a zero `field_712` disarms the body. The record's
/// two bits are latched for the next frame at the end.
static void func_actor_403900_80135BE0(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor402200Work* work;
    GfxCoord*        coord;
    AnimationRecord* rec;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_712 != 0) {
        rec = Gp_AnimGetRec(&work->anim, &work->slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_6CA & ANIMATION_RECORD_CUE_2)) {
                snd = D_actor_403900_80138424[work->field_712 * 2 - 1] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            }
            if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_6CA & ANIMATION_RECORD_CUE_1)) {
                snd  = D_actor_403900_80138424[work->field_712 * 2] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(coord));
            }
            work->field_6CA = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
        }
    }
}

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

    coord               = arg0->extra.tmd->coords;
    head                = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)    = head - sizeof(Actor402200AimScratch);
    sc                  = (Actor402200AimScratch*)(head - sizeof(Actor402200AimScratch));
    work                = arg0->work;
    part                = &coord[3] + 1;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    part->composeStamp  = GRAPHICS_COORD_DIRTY;
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
        work->field_62A |= 0xC000;
    } else {
        work->field_62A &= 0x3FFF;
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
        func_actor_403900_80136184(arg0);
    }
    SCRATCH_POP_BYTES(sizeof(Actor402200AimScratch));
}

/// Draws the red trail between the two points `func_actor_403900_80135D5C`
/// projects into `field_6FC`..`field_704`: eight segments, each skipped while
/// its interpolated depth is below 0x1E, and each drawn as two shaded quads
/// offset along the screen normal, a centre line and a tpage.
static void func_actor_403900_80136184(Task* arg0)
{
    Actor402200TrailScratch* sc;
    Actor402200Work*         work;
    POLY_G4*                 poly;
    LINE_F2*                 line;
    DR_TPAGE*                tp;
    s32                      i;
    s32                      j;

    sc         = SCRATCH_PUSH(Actor402200TrailScratch);
    work       = arg0->work;
    sc->dir.vx = work->field_6FC[1] - work->field_6FC[0];
    sc->dir.vy = work->field_700[1] - work->field_700[0];
    sc->dir.vz = 0;
    VectorNormalS(&sc->dir, &sc->norm);
    sc->norm.vy *= -1;
    sc->dx       = (work->field_6FC[1] - work->field_6FC[0]) / 8;
    sc->dy       = (work->field_700[1] - work->field_700[0]) / 8;
    sc->dz       = (work->field_704[1] - work->field_704[0]) / 8;
    for (i = 0; i < 8; i++) {
        sc->z = sc->dz * (i + 1) + work->field_704[0];
        if (sc->z < 0x1E) {
            continue;
        }
        sc->x[0] = work->field_6FC[0] + sc->dx * i;
        sc->x[1] = work->field_6FC[0] + sc->dx * (i + 1);
        sc->x[2] = sc->x[0] + ((-(sc->norm.vy * 0x600) >> 12) / sc->z);
        sc->x[3] = sc->x[1] + ((-(sc->norm.vy * 0x600) >> 12) / sc->z);
        sc->x[4] = sc->x[0] + (((sc->norm.vy * 3) >> 3) / sc->z);
        sc->x[5] = sc->x[1] + (((sc->norm.vy * 3) >> 3) / sc->z);
        sc->y[0] = work->field_700[0] + sc->dy * i;
        sc->y[1] = work->field_700[0] + sc->dy * (i + 1);
        sc->y[2] = sc->y[0] + ((-(sc->norm.vx * 0x600) >> 12) / sc->z);
        sc->y[3] = sc->y[1] + ((-(sc->norm.vx * 0x600) >> 12) / sc->z);
        sc->y[4] = sc->y[0] + (((sc->norm.vx * 3) >> 3) / sc->z);
        sc->y[5] = sc->y[1] + (((sc->norm.vx * 3) >> 3) / sc->z);
        for (j = 0; j < 2; j++) {
            poly           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(poly + 1);
            setlen(poly, 8);
            poly->code = 0x3A;
            poly->x0   = sc->x[D_actor_403900_801540D0[j][0]];
            poly->y0   = sc->y[D_actor_403900_801540D0[j][0]];
            poly->x1   = sc->x[D_actor_403900_801540D0[j][1]];
            poly->y1   = sc->y[D_actor_403900_801540D0[j][1]];
            poly->x2   = sc->x[D_actor_403900_801540D0[j][2]];
            poly->y2   = sc->y[D_actor_403900_801540D0[j][2]];
            poly->x3   = sc->x[D_actor_403900_801540D0[j][3]];
            poly->y3   = sc->y[D_actor_403900_801540D0[j][3]];
            setRGB0(poly, 0xFF, 0, 0);
            setRGB1(poly, 0xFF, 0, 0);
            setRGB2(poly, 0, 0, 0);
            setRGB3(poly, 0, 0, 0);
            addPrim((&gGpuCurrentOt[((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(line + 1);
        setlen(line, 3);
        line->code = 0x42;
        line->x0   = sc->x[0];
        line->y0   = sc->y[0];
        line->x1   = sc->x[1];
        line->y1   = sc->y[1];
        setRGB0(line, 0xFF, 0, 0);
        addPrim((&gGpuCurrentOt[((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), line);
        tp             = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(tp + 1);
        setlen(tp, 1);
        tp->code[0] = 0xE1000620;
        addPrim((&gGpuCurrentOt[((((u32)(sc->z << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), tp);
    }
    SCRATCH_POP(Actor402200TrailScratch);
}

/// Inlined copy of `func_actor_403900_80137EF0`: reseeds animation slots
/// 1..0x12 when the animation id changes, otherwise ticks them a frame.
static inline void Actor403900_ReseedAnim(Task* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_403900_801383B0[work->field_6C0];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->anim, i, work->field_6C0, 0, value);
        }
    } else {
        work->field_6C4++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Inlined copy of `func_actor_403900_80138070`: draws the ground shadow quad.
static inline void Actor403900_DrawShadow(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    GfxCoord*        sub;
    VECTOR3          vec;

    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[0];
    sub   = &arg0->extra.tmd->coords[3];
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}

/// Frame handler for the scene's `Gp_StateF0.field_4` mode. Mode 1 only refreshes the
/// coordinates, tint and shadow and mode 2 hides the model, both returning
/// without giving back the 8-byte `G_SCRATCH_HEAD` block. Otherwise the
/// `field_6CE` sequence runs: state 0 unlinks the actor and saves its pose,
/// state 1 sprays a randomly angled effect every fourth frame, and state 2
/// projects the actor before moving on to 3.
static void func_actor_403900_801368E0(GpEnemy* arg0, Task* arg1)
{
    u8*              head;
    SVECTOR*         sc;
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              mode;
    u32              random;
    s16              anim;

    work             = arg1->work;
    coord            = &arg1->extra.tmd->coords[0];
    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(SVECTOR);
    sc               = (SVECTOR*)(head - sizeof(SVECTOR));
    mode             = Gp_StateF0.field_4;
    switch (mode) {
        case 0:
            arg1->extra.tmd->flags = 0;
            break;
        case 1:
            coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
            arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            actor402200UpdateTint(arg1);
            Actor403900_DrawShadow(arg1);
            return;
        case 2:
            arg1->extra.tmd->flags = 0x80;
            return;
    }
    switch (work->field_6CE) {
        case 0:
            arg0->recs = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj((GpObj*)work->field_4E4);
            Gp_UnlinkObj((GpObj*)work->field_47C);
            Gp_UnlinkObj((GpObj*)work->field_564);
            Gp_ReleaseStateF0Add(arg1, work->field_716);
            anim = 0x14;
            if (work->field_6F0 == 1) {
                anim = 0x10;
            }
            work->field_6C0  = anim;
            work->field_6CE  = 1;
            arg0->spawnState = work->field_6F0;
            Gp_SaveEnemyPose(arg0);
            break;
        case 1:
            if (!(work->field_6C4 & 3)) {
                sc->vx      = 0;
                sc->vz      = 0;
                random      = Gp_LcgState * 5 + 0x71357911;
                sc->vy      = -((random >> 16) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &arg1->extra.tmd->coords[3], 0x400, sc);
            }
            break;
        case 2:
            func_actor_403900_8013820C(&arg1->extra.tmd->coords[3], 0xC);
            func_actor_403900_80134968(arg1);
            func_8009EA50(work->field_6D8);
            work->field_6CE = 3;
            break;
    }
    func_actor_403900_801380DC(arg1);
    Actor403900_ReseedAnim(arg1);
    coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    actor402200UpdateTint(arg1);
    Actor403900_DrawShadow(arg1);
    SCRATCH_POP_BYTES(sizeof(SVECTOR));
}

/// Queues at depth `otz` a run of primitives that, executed in reverse of
/// the order they are added, point drawing at the 320x240 area at VRAM
/// (0x1C0, 0x100), fill it with a near-black tile with mask-bit setting on,
/// draw two 160x240 raw-texture sprites copied from the current draw buffer
/// over it, and then restore the draw offset, mask setting and draw area for
/// the current buffer. The restored area is the view's sprite rectangle when
/// one is active and nearer than `otz`, full screen otherwise. The 0x14-byte
/// block holding the rectangle and offset is carved off the scratch head and
/// released before returning.
static void func_actor_403900_80136D9C(s32 otz)
{
    ActorsDrawScratch* scratch;
    GpDrawAreaRec*     extra;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;

    extra          = Gp_GetViewSprtExtra();
    scratch        = SCRATCH_PUSH(ActorsDrawScratch);
    scratch->otz   = otz;
    area           = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(area + 1);
    if (extra != NULL && ((extra->depth << gDisplayState.otDepthShift) & 0x3FFF) >> 4 < scratch->otz) {
        scratch->rect    = extra->rect;
        scratch->rect.y += gDisplayState.drawBuffer * 0x110;
    } else {
        scratch->rect.x = 0;
        scratch->rect.y = gDisplayState.drawBuffer * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&gGpuCurrentOt[scratch->otz], tile);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area            = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(area + 1);
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_POP(ActorsDrawScratch);
}

/// Spawn handler. Allocates the 0x71C-byte work block, points the model at its
/// light / colour matrices and loads the animation context, then branches on
/// the enemy's `field_4B` variant. Variant 0 is the full setup: it links the
/// enemy node, picks the box table and count for the current stage / room out
/// of `D_actor_403900_80153C7C`, requests the room's cue bank, and links the
/// work block's five collision objects with their `WorldCollisionContact` tables before
/// moving the task on (`field_30` 1). Variants 1 and 2 only seed the animation
/// and sequence state.
static void func_actor_403900_80137444(GpEnemy* arg0, Task* arg1)
{
    u8                     param1[4];
    u8                     param2[4];
    Actor402200Work*       work;
    TmdObject*             obj;
    GfxCoord*               coord;
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
    func_800B3F84(&work->anim, D_actor_403900_801540EC, obj, work->field_30C, work->slots);
    work->field_6C0 = 0xB;
    work->field_6C2 = 0xB;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->anim, i, work->field_6C0);
    }
    kind = arg0->spawnState;
    switch (kind) {
        case 0:
            work->field_6D8 = 0xFF;
            obj->lightLevel = 0;
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
                if (gGameSession->at4.loc.stage == D_actor_403900_80153C7C[i].field_2 && gGameSession->at4.loc.area == D_actor_403900_80153C7C[i].field_4) {
                    work->field_6B4 = D_actor_403900_80153F04[D_actor_403900_80153C7C[i].field_0];
                    work->field_6FA = D_actor_403900_80153C7C[i].field_6;
                }
            }
            work->field_6CC = 0xB;
            (Gp_IncStateF0Ref)(0);
            work->field_716 = 0x27;
            cues            = D_actor_403900_8015409C[gGameSession->at4.loc.stage];
            if (cues != NULL) {
                work->field_712 = cues[gGameSession->at4.loc.area];
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
            Gp_LinkObj(2, (GpObj*)work->field_47C);
            Gp_InitRec18Table(records1, 3, 0);
            work->field_49A |= 0x8000;
            work->field_4EC  = arg1->extra.tmd->coords;
            records2         = work->field_504;
            work->field_4F0  = records2;
            work->field_4F4  = 0;
            work->field_4F6  = -0x1F4;
            work->field_4F8  = 0;
            work->field_4FC  = 0x30027;
            work->field_500  = 0x1F4;
            work->field_502  = 1;
            Gp_LinkObj(2, (GpObj*)work->field_4E4);
            Gp_InitRec18Table(records2, 4, 0);
            work->field_502 |= 0x4200;
            work->field_56C  = &arg1->extra.tmd->coords[8];
            records3         = &work->field_584;
            work->field_570  = records3;
            work->field_574  = 0;
            work->field_576  = 0;
            work->field_578  = 0;
            work->field_57C  = Gp_PackPair(D_actor_403900_80153BF0, 1);
            work->field_580  = 0x12C;
            work->field_582  = 1;
            Gp_LinkObj(3, (GpObj*)work->field_564);
            Gp_InitRec18Table(records3, 1, 0);
            work->field_582 &= 0x7FFF;
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
            work->field_5A4  = gameGetPtrSlot(3)->extra.tmd->coords;
            work->field_5A8  = &work->field_5DC;
            work->field_5AC  = 0;
            work->field_5AE  = 0;
            work->field_5B0  = 0;
            work->field_5B4  = 0;
            work->field_5B8  = 0;
            work->field_5BA  = 3;
            Gp_LinkObj(3, (GpObj*)work->field_59C);
            Gp_InitRec18Table(records4, 1, 0);
            work->field_5BA &= 0xBFFF;
            work->field_5C4  = gameGetPtrSlot(3)->extra.tmd->coords;
            work->field_5C8  = records4;
            work->field_5CC  = 0;
            work->field_5CE  = -0x320;
            work->field_5D0  = -0x5AA;
            work->field_5D4  = 0;
            work->field_5D8  = 0x1F4;
            work->field_5DA  = 1;
            Gp_LinkObj(3, (GpObj*)work->field_5BC);
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
            work->field_5DA &= 0xBFFF;
            Gp_LinkObj(3, (GpObj*)work->field_60C);
            Gp_InitRec18Table(records5, 1, 0);
            work->field_62A = (work->field_62A & 0x3FFF) | 0xC00;
            arg1->msgTable  = D_actor_403900_801383A0;
            arg1->state     = 1;
            break;
        case 1:
            work->field_6C0 = 0x10;
            work->field_6CE = 2;
            arg1->state     = 2;
            work->field_6D8 = 0;
            obj->lightLevel = 0x1000;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
        case 2:
            work->field_6C0 = 0x14;
            work->field_6CE = kind;
            arg1->state     = kind;
            work->field_6D8 = 0;
            obj->lightLevel = 0x1000;
            func_8009EA50(work->field_6D8);
            work->field_6E2 = 0x80;
            break;
    }
}

/// Frame handler for the scene's `Gp_StateF0.field_4` mode. Mode 1 only refreshes the
/// tint and the ground shadow, and mode 2 hides the model; both return at once.
/// Mode 0 shows the model again while the `field_6DA` timer runs and makes the
/// enemy lockable only while a hit is pending (bit 0x8000 of `field_49A`).
/// Then, once the box table is set, the frame runs: the hit handler, the
/// sequence dispatch, the step forward, the animation reseed, the vocal cue,
/// the coordinate refresh, the tint and shadow, the projection at depth +0xC,
/// the fade and `func_8009EA50`.
static void func_actor_403900_80137A20(GpEnemy* arg0, Task* arg1)
{
    Actor402200Work* temp_s1;
    TmdObject*       temp_a1;
    GfxCoord*        temp_s2;
    s32              state;
    s32              one;

    temp_s1 = arg1->work;
    temp_a1 = arg1->extra.tmd;
    temp_s2 = temp_a1->coords;
    state   = Gp_StateF0.field_4;
    one     = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    if (temp_s1->field_6DA != 0) {
        temp_a1->flags = 0;
    }
    arg0->node.state.b.flags = (temp_s1->field_49A >> 0xF) ^ 1;
    goto default_body;
case1:
    func_actor_403900_80137FB4(arg1);
    func_actor_403900_80138070(arg1);
    return;
case2:
    temp_a1->flags           = 0x80;
    arg0->node.state.b.flags = one;
    return;
default_body:
    if (temp_s1->field_6B4 != 0) {
        func_actor_403900_80131F54(arg1);
        func_actor_403900_80137B78(arg1);
        func_actor_403900_80137E4C(arg1);
        func_actor_403900_80137EF0(arg1);
        func_actor_403900_80135BE0(arg1);
        temp_s2->composeStamp                   = GRAPHICS_COORD_DIRTY;
        arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(temp_s2);
        func_actor_403900_80137FB4(arg1);
        func_actor_403900_80138070(arg1);
        func_actor_403900_8013820C(&arg1->extra.tmd->coords[3], 0xC);
        func_actor_403900_80134968(arg1);
        func_8009EA50(temp_s1->field_6D8);
    }
}

/// Runs the sequence `field_6CC` names (0 to 0xB), then the vocal cue unless
/// the sequence is 1.
static void func_actor_403900_80137B78(Task* arg0)
{
    s16              temp_v1;
    Actor402200Work* temp_s1;

    temp_s1 = arg0->work;
    temp_v1 = temp_s1->field_6CC;
    switch (temp_v1) {
        case 0:
            func_actor_403900_801329A4(arg0);
            break;
        case 1:
            func_actor_403900_8013314C(arg0);
            break;
        case 2:
            func_actor_403900_80133AEC(arg0);
            break;
        case 3:
            func_actor_403900_80134194(arg0);
            break;
        case 4:
            func_actor_403900_801347F4(arg0);
            break;
        case 5:
            func_actor_403900_8013539C(arg0);
            break;
        case 6:
            func_actor_403900_801354B0(arg0);
            break;
        case 7:
            func_actor_403900_80135630(arg0);
            break;
        case 8:
            func_actor_403900_8013592C(arg0);
            break;
        case 9:
            func_actor_403900_80135A24(arg0);
            break;
        case 10:
            func_actor_403900_80137D7C(arg0);
            break;
        case 11:
            func_actor_403900_80132688(arg0);
            break;
    }
    if (temp_s1->field_6CC != 1) {
        func_actor_403900_801380DC(arg0);
    }
}

/// Rebuilds the root part's rotation from the saved attach matrix
/// `field_674`, scaled per axis by `scale`: the saved matrix is
/// copied into the root coordinate, and an identity scaled in a scratchpad
/// matrix is multiplied into it.
static void func_actor_403900_80137CA8(Task* arg0)
{
    void**           scratch;
    OverlayMat*      head;
    OverlayMat*      m;
    GfxCoord*        coord;
    Actor402200Work* work;

    scratch                              = SCRATCH_HEAD_ADDR;
    head                                 = SCRATCH_HEAD_AT(scratch, OverlayMat);
    m                                    = head - 1;
    SCRATCH_HEAD_AT(scratch, OverlayMat) = m;
    coord                                = &arg0->extra.tmd->coords[0];
    work                                 = arg0->work;

    coord->coord     = work->field_674;
    m->ident.m00_m01 = 0x1000;
    m->ident.m02_m10 = 0;
    m->ident.m11_m12 = 0x1000;
    m->ident.m20_m21 = 0;
    m->ident.m22     = 0x1000;
    ScaleMatrix(&m->mat, &work->scale);
    MulMatrix(&coord->coord, &m->mat);
    SCRATCH_POP_AT(scratch, OverlayMat);
}

/// Sequence 0xA, the entrance: state 0 picks animation 0xE (and state 1) when
/// `field_6F0` is 1, otherwise 0x12 and state 2, and arms the timers
/// `field_6DA`..`field_6DE`; states 1 and 2 wait for the frame counter to reach
/// 0x10 or 0x16, then park 2 in the context's `field_30` and drop back to 0.
static void func_actor_403900_80137D7C(Task* arg0)
{
    Actor402200Work* work;
    s16              state;
    s32              next;

    work  = arg0->work;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0xA;
            work->field_6DE = 5;
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                arg0->state     = 2;
                work->field_6CE = 0;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                arg0->state     = state;
                work->field_6CE = 0;
            }
            break;
    }
}

/// Saves the root's translation in `field_664`..`field_66C` and steps it
/// `field_6C8` along the root's facing (its matrix's third column), adding
/// 0x80 to its y while `field_714` is below 2.
static void func_actor_403900_80137E4C(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;

    coord              = &arg0->extra.tmd->coords[0];
    work               = arg0->work;
    work->field_664    = coord->coord.t[0];
    work->field_668    = coord->coord.t[1];
    work->field_66C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_6C8) >> 12;
    if (work->field_714 < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_6C8) >> 12;
}

/// Reseeds animation slots 1..0x12 when the actor's animation id changes,
/// handing each slot the blend weight the id selects from
/// `D_actor_403900_801383B0`; while the id is unchanged it instead ticks every
/// slot one frame and walks the id's frame counter up.
static void func_actor_403900_80137EF0(Task* arg0)
{
    Actor402200Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->field_6C0 != work->field_6C2) {
        work->field_6C2 = work->field_6C0;
        work->field_6C4 = 0;
        value           = D_actor_403900_801383B0[work->field_6C0];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->anim, i, work->field_6C0, 0, value);
        }
    } else {
        work->field_6C4++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Out-of-line `actor402200UpdateTint`, for the callers after the inline one.
static void func_actor_403900_80137FB4(Task* arg0)
{
    actor402200UpdateTint(arg0);
}

/// Draws the ground shadow quad, 0x300 across, under the fourth part's
/// horizontal position at the root's height, shaded by `field_6E2` - which a
/// zero turns into -1 first, so a shadow nothing has raised is not drawn.
static void func_actor_403900_80138070(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    GfxCoord*        sub;
    VECTOR3          vec;

    work  = arg0->work;
    coord = &arg0->extra.tmd->coords[0];
    sub   = &arg0->extra.tmd->coords[3];
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}

/// Runs the one-shot vocal cue armed by `field_718`. While the flag is set the
/// frame counter `field_71A` runs up: at 0x14 the cue is queued, panned and
/// depth-attenuated from the display object, and from 0x5F on the player's
/// slot is asked for message 0x3ED - a refusal sends 0x3F1 instead and clears
/// the flag.
static void func_actor_403900_801380DC(Task* arg0)
{
    Actor402200Work* work;
    s16              timer;
    s32              sound;
    s32              pan;
    Task*            slot;
    GfxCoord*        coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    slot  = gameGetPtrSlot(3);
    if (work->field_718 != 0) {
        if (work->field_71A == 0x14) {
            sound = D_actor_403900_80138478 | ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8);
            pan   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        }
        timer           = (u16)work->field_71A + 1;
        work->field_71A = timer;
        if ((timer >= 0x5F) && (Gp_DispatchMsg(slot, 0x3ED, 0, 0) == 0)) {
            Gp_DispatchMsg(slot, 0x3F1, 0, 0);
            work->field_718 = 0;
        }
    }
}

/// Raises the actor's phase `field_6F4` to 1 while enemies remain.
s32 func_actor_403900_801381E4(Task* task)
{
    if (Player_Status.hp > 0) {
        ((Actor402200Work*)task->work)->field_6F4 = 1;
    }
    return 0;
}

/// Projects the origin of `arg0` to find its ordering-table depth, adds
/// `arg1`, and queues the frame-buffer pass `func_actor_403900_80136D9C` there
/// (at depth `arg1` when the projection fails).
static void func_actor_403900_8013820C(GfxCoord* arg0, s32 arg1)
{
    u8*                  head;
    ActorProjectScratch* block;
    SVECTOR*             vec;

    head                              = SCRATCH_HEAD(u8);
    block                             = (ActorProjectScratch*)(head - sizeof(ActorProjectScratch));
    SCRATCH_HEAD(ActorProjectScratch) = block;
    block->vec.vx                     = 0;
    block->vec.vy                     = 0;
    block->vec.vz                     = 0;
    Gp_UpdateCoord(arg0);
    vec = &block->vec;
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&block->sxy);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + arg1;
    func_actor_403900_80136D9C(block->otz);
    SCRATCH_POP_BYTES(0x18);
}

/// Runs the enemy task's current state handler from
/// `D_actor_403900_80131F18`, copying the table onto the stack first.
static void func_actor_403900_80138344(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403900_80131F18;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
