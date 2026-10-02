#include "actors/actor_510900.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actor_510900_private.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/sprite_quad.h"

/// 0x24-byte scratch `func_actor_510900_80134284` takes from the scratch stack
/// to draw one frame of the debris trail. `vec0` is the effect coordinate's
/// `workm.t[]` before the per-frame drift is added and `vec1` the same after,
/// so the two `RTPS` projections give the ends of the trail `LINE_F2`.
/// `otz0` / `otz1` receive `gte_stszotz` for each end and their mean picks the
/// OT bucket; `flag` is the shared `gte_stflg` the projections are dropped on.
typedef struct Actor510900TrailScratch {
    /* 0x00 */ SVECTOR vec0;
    /* 0x08 */ SVECTOR vec1;
    /* 0x10 */ s32     otz0;
    /* 0x14 */ s32     otz1;
    /* 0x18 */ s32     flag;
    /* 0x1C */ DVECTOR sxy0;
    /* 0x20 */ DVECTOR sxy1;
} Actor510900TrailScratch;
STATIC_ASSERT_SIZEOF(Actor510900TrailScratch, 0x24);

/// One VRAM CLUT coordinate per frame of the muzzle-flash sprite, packed the
/// way `getClut` takes them. `D_actor_510900_8013C48C` holds twelve, one for
/// each frame `gEffectSpriteAtlasFrames` supplies the texture window for.
typedef struct Actor510900SprClut {
    /* 0x0 */ u16 clutX;
    /* 0x2 */ u16 clutY;
} Actor510900SprClut;
STATIC_ASSERT_SIZEOF(Actor510900SprClut, 4);

/// The twelve muzzle-flash CLUTs `spriteQuadDraw` indexes by frame.
extern Actor510900SprClut D_actor_510900_8013C48C[];

Actor510900SprClut D_actor_510900_8013C48C[12] = {
    { 80, 270 },
    { 96, 270 },
    { 112, 270 },
    { 128, 270 },
    { 144, 270 },
    { 160, 270 },
    { 176, 270 },
    { 192, 270 },
    { 208, 270 },
    { 224, 270 },
    { 240, 270 },
    { 256, 270 },
};

TmdBone D_actor_510900_8013C4BC[19] = {
#include "assets/no9_golem_akropolis_body_skeleton.inc"
};

u32 D_actor_510900_8013C768[19] = {
#include "assets/no9_golem_akropolis_body_partVerts.inc"
};

SVECTOR D_actor_510900_8013C7B4[358] = {
#include "assets/no9_golem_akropolis_body_verts.inc"
};

SVECTOR D_actor_510900_8013D2E4[356] = {
#include "assets/no9_golem_akropolis_body_normals.inc"
};

u32 D_actor_510900_8013DE04[3928] = {
#include "assets/no9_golem_akropolis_body_stream.inc"
};

TmdSource D_actor_510900_80141B64 = {
    0,
    21588,
    5944,
    19,
    D_actor_510900_8013C768,
    D_actor_510900_8013C7B4,
    D_actor_510900_8013D2E4,
    D_actor_510900_8013C4BC,
    D_actor_510900_8013DE04,
};

TmdBone D_actor_510900_80141B88[1] = {
#include "assets/actor_510900_model_0FE60_skeleton.inc"
};

u32 D_actor_510900_80141BAC[1] = {
#include "assets/actor_510900_model_0FE60_partVerts.inc"
};

SVECTOR D_actor_510900_80141BB0[14] = {
#include "assets/actor_510900_model_0FE60_verts.inc"
};

SVECTOR D_actor_510900_80141C20[12] = {
#include "assets/actor_510900_model_0FE60_normals.inc"
};

u32 D_actor_510900_80141C80[98] = {
#include "assets/actor_510900_model_0FE60_stream.inc"
};

TmdSource D_actor_510900_80141E08 = {
    0,
    652,
    0,
    1,
    D_actor_510900_80141BAC,
    D_actor_510900_80141BB0,
    D_actor_510900_80141C20,
    D_actor_510900_80141B88,
    D_actor_510900_80141C80,
};

TmdBone D_actor_510900_80141E2C[1] = {
#include "assets/no9_golem_akropolis_prop_skeleton.inc"
};

u32 D_actor_510900_80141E50[1] = {
#include "assets/no9_golem_akropolis_prop_partVerts.inc"
};

SVECTOR D_actor_510900_80141E54[14] = {
#include "assets/no9_golem_akropolis_prop_verts.inc"
};

SVECTOR D_actor_510900_80141EC4[19] = {
#include "assets/no9_golem_akropolis_prop_normals.inc"
};

u32 D_actor_510900_80141F5C[114] = {
#include "assets/no9_golem_akropolis_prop_stream.inc"
};

TmdSource D_actor_510900_80142124 = {
    0,
    724,
    0,
    1,
    D_actor_510900_80141E50,
    D_actor_510900_80141E54,
    D_actor_510900_80141EC4,
    D_actor_510900_80141E2C,
    D_actor_510900_80141F5C,
};

TmdBone D_actor_510900_80142148[1] = {
#include "assets/actor_510900_model_10468_skeleton.inc"
};

u32 D_actor_510900_8014216C[1] = {
#include "assets/actor_510900_model_10468_partVerts.inc"
};

SVECTOR D_actor_510900_80142170[18] = {
#include "assets/actor_510900_model_10468_verts.inc"
};

SVECTOR D_actor_510900_80142200[17] = {
#include "assets/actor_510900_model_10468_normals.inc"
};

u32 D_actor_510900_80142288[126] = {
#include "assets/actor_510900_model_10468_stream.inc"
};

TmdSource D_actor_510900_80142480 = {
    0,
    860,
    0,
    1,
    D_actor_510900_8014216C,
    D_actor_510900_80142170,
    D_actor_510900_80142200,
    D_actor_510900_80142148,
    D_actor_510900_80142288,
};

TmdBone D_actor_510900_801424A4[1] = {
#include "assets/golem_grenade_skeleton.inc"
};

u32 D_actor_510900_801424C8[1] = {
#include "assets/golem_grenade_partVerts.inc"
};

SVECTOR D_actor_510900_801424CC[12] = {
#include "assets/golem_grenade_verts.inc"
};

SVECTOR D_actor_510900_8014252C[28] = {
#include "assets/golem_grenade_normals.inc"
};

u32 D_actor_510900_8014260C[104] = {
#include "assets/golem_grenade_stream.inc"
};

TmdSource D_actor_510900_801427AC = {
    0,
    720,
    0,
    1,
    D_actor_510900_801424C8,
    D_actor_510900_801424CC,
    D_actor_510900_8014252C,
    D_actor_510900_801424A4,
    D_actor_510900_8014260C,
};

TmdBone D_actor_510900_801427D0[11] = {
#include "assets/actor_510900_model_10C8C_skeleton.inc"
};

u32 D_actor_510900_8014295C[11] = {
#include "assets/actor_510900_model_10C8C_partVerts.inc"
};

SVECTOR D_actor_510900_80142988[33] = {
#include "assets/actor_510900_model_10C8C_verts.inc"
};

SVECTOR D_actor_510900_80142A90[3] = {
#include "assets/actor_510900_model_10C8C_normals.inc"
};

u32 D_actor_510900_80142AA8[421] = {
#include "assets/actor_510900_model_10C8C_stream.inc"
};

TmdSource D_actor_510900_8014313C = {
    0,
    1560,
    1404,
    11,
    D_actor_510900_8014295C,
    D_actor_510900_80142988,
    D_actor_510900_80142A90,
    D_actor_510900_801427D0,
    D_actor_510900_80142AA8,
};

AnimationPackedPose D_actor_510900_80143160[18] = {
#include "assets/actor_510900_animation_11CC0_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80143238[231] = {
#include "assets/actor_510900_animation_11CC0_bank4.inc"
};

AnimationRecord D_actor_510900_801435D4[313] = {
#include "assets/actor_510900_animation_11CC0_records.inc"
};

u16 D_actor_510900_80143AB8[20] = {
#include "assets/actor_510900_animation_11CC0_indices.inc"
};

AnimationSet D_actor_510900_80143AE0 = {
    D_actor_510900_801435D4,
    D_actor_510900_80143AB8,
    { NULL, D_actor_510900_80143160, NULL, NULL, D_actor_510900_80143238, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80143B08[8] = {
#include "assets/actor_510900_animation_11FB8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80143B68[57] = {
#include "assets/actor_510900_animation_11FB8_bank4.inc"
};

AnimationRecord D_actor_510900_80143C4C[89] = {
#include "assets/actor_510900_animation_11FB8_records.inc"
};

u16 D_actor_510900_80143DB0[20] = {
#include "assets/actor_510900_animation_11FB8_indices.inc"
};

AnimationSet D_actor_510900_80143DD8 = {
    D_actor_510900_80143C4C,
    D_actor_510900_80143DB0,
    { NULL, D_actor_510900_80143B08, NULL, NULL, D_actor_510900_80143B68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80143E00[29] = {
#include "assets/actor_510900_animation_129B0_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80143F5C[221] = {
#include "assets/actor_510900_animation_129B0_bank4.inc"
};

AnimationRecord D_actor_510900_801442D0[310] = {
#include "assets/actor_510900_animation_129B0_records.inc"
};

u16 D_actor_510900_801447A8[20] = {
#include "assets/actor_510900_animation_129B0_indices.inc"
};

AnimationSet D_actor_510900_801447D0 = {
    D_actor_510900_801442D0,
    D_actor_510900_801447A8,
    { NULL, D_actor_510900_80143E00, NULL, NULL, D_actor_510900_80143F5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801447F8[45] = {
#include "assets/actor_510900_animation_13884_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80144A14[341] = {
#include "assets/actor_510900_animation_13884_bank4.inc"
};

AnimationRecord D_actor_510900_80144F68[453] = {
#include "assets/actor_510900_animation_13884_records.inc"
};

u16 D_actor_510900_8014567C[20] = {
#include "assets/actor_510900_animation_13884_indices.inc"
};

AnimationSet D_actor_510900_801456A4 = {
    D_actor_510900_80144F68,
    D_actor_510900_8014567C,
    { NULL, D_actor_510900_801447F8, NULL, NULL, D_actor_510900_80144A14, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801456CC[20] = {
#include "assets/actor_510900_animation_142A0_bank1.inc"
};

AnimationPackedRotation D_actor_510900_801457BC[253] = {
#include "assets/actor_510900_animation_142A0_bank4.inc"
};

AnimationRecord D_actor_510900_80145BB0[314] = {
#include "assets/actor_510900_animation_142A0_records.inc"
};

u16 D_actor_510900_80146098[20] = {
#include "assets/actor_510900_animation_142A0_indices.inc"
};

AnimationSet D_actor_510900_801460C0 = {
    D_actor_510900_80145BB0,
    D_actor_510900_80146098,
    { NULL, D_actor_510900_801456CC, NULL, NULL, D_actor_510900_801457BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801460E8[30] = {
#include "assets/actor_510900_animation_14E4C_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80146250[277] = {
#include "assets/actor_510900_animation_14E4C_bank4.inc"
};

AnimationRecord D_actor_510900_801466A4[360] = {
#include "assets/actor_510900_animation_14E4C_records.inc"
};

u16 D_actor_510900_80146C44[20] = {
#include "assets/actor_510900_animation_14E4C_indices.inc"
};

AnimationSet D_actor_510900_80146C6C = {
    D_actor_510900_801466A4,
    D_actor_510900_80146C44,
    { NULL, D_actor_510900_801460E8, NULL, NULL, D_actor_510900_80146250, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80146C94[78] = {
#include "assets/actor_510900_animation_172DC_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014703C[974] = {
#include "assets/actor_510900_animation_172DC_bank4.inc"
};

AnimationRecord D_actor_510900_80147F74[1112] = {
#include "assets/actor_510900_animation_172DC_records.inc"
};

u16 D_actor_510900_801490D4[20] = {
#include "assets/actor_510900_animation_172DC_indices.inc"
};

AnimationSet D_actor_510900_801490FC = {
    D_actor_510900_80147F74,
    D_actor_510900_801490D4,
    { NULL, D_actor_510900_80146C94, NULL, NULL, D_actor_510900_8014703C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80149124[27] = {
#include "assets/actor_510900_animation_17CB8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80149268[238] = {
#include "assets/actor_510900_animation_17CB8_bank4.inc"
};

AnimationRecord D_actor_510900_80149620[292] = {
#include "assets/actor_510900_animation_17CB8_records.inc"
};

u16 D_actor_510900_80149AB0[20] = {
#include "assets/actor_510900_animation_17CB8_indices.inc"
};

AnimationSet D_actor_510900_80149AD8 = {
    D_actor_510900_80149620,
    D_actor_510900_80149AB0,
    { NULL, D_actor_510900_80149124, NULL, NULL, D_actor_510900_80149268, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80149B00[29] = {
#include "assets/actor_510900_animation_186E0_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80149C5C[244] = {
#include "assets/actor_510900_animation_186E0_bank4.inc"
};

AnimationRecord D_actor_510900_8014A02C[299] = {
#include "assets/actor_510900_animation_186E0_records.inc"
};

u16 D_actor_510900_8014A4D8[20] = {
#include "assets/actor_510900_animation_186E0_indices.inc"
};

AnimationSet D_actor_510900_8014A500 = {
    D_actor_510900_8014A02C,
    D_actor_510900_8014A4D8,
    { NULL, D_actor_510900_80149B00, NULL, NULL, D_actor_510900_80149C5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014A528[8] = {
#include "assets/actor_510900_animation_18B5C_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014A588[106] = {
#include "assets/actor_510900_animation_18B5C_bank4.inc"
};

AnimationRecord D_actor_510900_8014A730[137] = {
#include "assets/actor_510900_animation_18B5C_records.inc"
};

u16 D_actor_510900_8014A954[20] = {
#include "assets/actor_510900_animation_18B5C_indices.inc"
};

AnimationSet D_actor_510900_8014A97C = {
    D_actor_510900_8014A730,
    D_actor_510900_8014A954,
    { NULL, D_actor_510900_8014A528, NULL, NULL, D_actor_510900_8014A588, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014A9A4[47] = {
#include "assets/actor_510900_animation_1A250_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014ABD8[613] = {
#include "assets/actor_510900_animation_1A250_bank4.inc"
};

AnimationRecord D_actor_510900_8014B56C[695] = {
#include "assets/actor_510900_animation_1A250_records.inc"
};

u16 D_actor_510900_8014C048[20] = {
#include "assets/actor_510900_animation_1A250_indices.inc"
};

AnimationSet D_actor_510900_8014C070 = {
    D_actor_510900_8014B56C,
    D_actor_510900_8014C048,
    { NULL, D_actor_510900_8014A9A4, NULL, NULL, D_actor_510900_8014ABD8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014C098[19] = {
#include "assets/actor_510900_animation_1A994_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014C17C[160] = {
#include "assets/actor_510900_animation_1A994_bank4.inc"
};

AnimationRecord D_actor_510900_8014C3FC[228] = {
#include "assets/actor_510900_animation_1A994_records.inc"
};

u16 D_actor_510900_8014C78C[20] = {
#include "assets/actor_510900_animation_1A994_indices.inc"
};

AnimationSet D_actor_510900_8014C7B4 = {
    D_actor_510900_8014C3FC,
    D_actor_510900_8014C78C,
    { NULL, D_actor_510900_8014C098, NULL, NULL, D_actor_510900_8014C17C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014C7DC[66] = {
#include "assets/actor_510900_animation_1BFA8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014CAF4[549] = {
#include "assets/actor_510900_animation_1BFA8_bank4.inc"
};

AnimationRecord D_actor_510900_8014D388[646] = {
#include "assets/actor_510900_animation_1BFA8_records.inc"
};

u16 D_actor_510900_8014DDA0[20] = {
#include "assets/actor_510900_animation_1BFA8_indices.inc"
};

AnimationSet D_actor_510900_8014DDC8 = {
    D_actor_510900_8014D388,
    D_actor_510900_8014DDA0,
    { NULL, D_actor_510900_8014C7DC, NULL, NULL, D_actor_510900_8014CAF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014DDF0[19] = {
#include "assets/actor_510900_animation_1CBA8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014DED4[310] = {
#include "assets/actor_510900_animation_1CBA8_bank4.inc"
};

AnimationRecord D_actor_510900_8014E3AC[381] = {
#include "assets/actor_510900_animation_1CBA8_records.inc"
};

u16 D_actor_510900_8014E9A0[20] = {
#include "assets/actor_510900_animation_1CBA8_indices.inc"
};

AnimationSet D_actor_510900_8014E9C8 = {
    D_actor_510900_8014E3AC,
    D_actor_510900_8014E9A0,
    { NULL, D_actor_510900_8014DDF0, NULL, NULL, D_actor_510900_8014DED4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014E9F0[28] = {
#include "assets/actor_510900_animation_1D868_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014EB40[322] = {
#include "assets/actor_510900_animation_1D868_bank4.inc"
};

AnimationRecord D_actor_510900_8014F048[390] = {
#include "assets/actor_510900_animation_1D868_records.inc"
};

u16 D_actor_510900_8014F660[20] = {
#include "assets/actor_510900_animation_1D868_indices.inc"
};

AnimationSet D_actor_510900_8014F688 = {
    D_actor_510900_8014F048,
    D_actor_510900_8014F660,
    { NULL, D_actor_510900_8014E9F0, NULL, NULL, D_actor_510900_8014EB40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8014F6B0[28] = {
#include "assets/actor_510900_animation_1E56C_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8014F800[328] = {
#include "assets/actor_510900_animation_1E56C_bank4.inc"
};

AnimationRecord D_actor_510900_8014FD20[401] = {
#include "assets/actor_510900_animation_1E56C_records.inc"
};

u16 D_actor_510900_80150364[20] = {
#include "assets/actor_510900_animation_1E56C_indices.inc"
};

AnimationSet D_actor_510900_8015038C = {
    D_actor_510900_8014FD20,
    D_actor_510900_80150364,
    { NULL, D_actor_510900_8014F6B0, NULL, NULL, D_actor_510900_8014F800, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801503B4[19] = {
#include "assets/actor_510900_animation_1EF7C_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80150498[255] = {
#include "assets/actor_510900_animation_1EF7C_bank4.inc"
};

AnimationRecord D_actor_510900_80150894[312] = {
#include "assets/actor_510900_animation_1EF7C_records.inc"
};

u16 D_actor_510900_80150D74[20] = {
#include "assets/actor_510900_animation_1EF7C_indices.inc"
};

AnimationSet D_actor_510900_80150D9C = {
    D_actor_510900_80150894,
    D_actor_510900_80150D74,
    { NULL, D_actor_510900_801503B4, NULL, NULL, D_actor_510900_80150498, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80150DC4[19] = {
#include "assets/actor_510900_animation_1F668_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80150EA8[153] = {
#include "assets/actor_510900_animation_1F668_bank4.inc"
};

AnimationRecord D_actor_510900_8015110C[213] = {
#include "assets/actor_510900_animation_1F668_records.inc"
};

u16 D_actor_510900_80151460[20] = {
#include "assets/actor_510900_animation_1F668_indices.inc"
};

AnimationSet D_actor_510900_80151488 = {
    D_actor_510900_8015110C,
    D_actor_510900_80151460,
    { NULL, D_actor_510900_80150DC4, NULL, NULL, D_actor_510900_80150EA8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801514B0[23] = {
#include "assets/actor_510900_animation_2035C_bank1.inc"
};

AnimationPackedRotation D_actor_510900_801515C4[326] = {
#include "assets/actor_510900_animation_2035C_bank4.inc"
};

AnimationRecord D_actor_510900_80151ADC[414] = {
#include "assets/actor_510900_animation_2035C_records.inc"
};

u16 D_actor_510900_80152154[20] = {
#include "assets/actor_510900_animation_2035C_indices.inc"
};

AnimationSet D_actor_510900_8015217C = {
    D_actor_510900_80151ADC,
    D_actor_510900_80152154,
    { NULL, D_actor_510900_801514B0, NULL, NULL, D_actor_510900_801515C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801521A4[8] = {
#include "assets/actor_510900_animation_2093C_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80152204[130] = {
#include "assets/actor_510900_animation_2093C_bank4.inc"
};

AnimationRecord D_actor_510900_8015240C[202] = {
#include "assets/actor_510900_animation_2093C_records.inc"
};

u16 D_actor_510900_80152734[20] = {
#include "assets/actor_510900_animation_2093C_indices.inc"
};

AnimationSet D_actor_510900_8015275C = {
    D_actor_510900_8015240C,
    D_actor_510900_80152734,
    { NULL, D_actor_510900_801521A4, NULL, NULL, D_actor_510900_80152204, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80152784[18] = {
#include "assets/actor_510900_animation_213E4_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015285C[281] = {
#include "assets/actor_510900_animation_213E4_bank4.inc"
};

AnimationRecord D_actor_510900_80152CC0[327] = {
#include "assets/actor_510900_animation_213E4_records.inc"
};

u16 D_actor_510900_801531DC[20] = {
#include "assets/actor_510900_animation_213E4_indices.inc"
};

AnimationSet D_actor_510900_80153204 = {
    D_actor_510900_80152CC0,
    D_actor_510900_801531DC,
    { NULL, D_actor_510900_80152784, NULL, NULL, D_actor_510900_8015285C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8015322C[10] = {
#include "assets/actor_510900_animation_217A4_bank1.inc"
};

AnimationPackedRotation D_actor_510900_801532A4[79] = {
#include "assets/actor_510900_animation_217A4_bank4.inc"
};

AnimationRecord D_actor_510900_801533E0[111] = {
#include "assets/actor_510900_animation_217A4_records.inc"
};

u16 D_actor_510900_8015359C[20] = {
#include "assets/actor_510900_animation_217A4_indices.inc"
};

AnimationSet D_actor_510900_801535C4 = {
    D_actor_510900_801533E0,
    D_actor_510900_8015359C,
    { NULL, D_actor_510900_8015322C, NULL, NULL, D_actor_510900_801532A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801535EC[25] = {
#include "assets/actor_510900_animation_22074_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80153718[195] = {
#include "assets/actor_510900_animation_22074_bank4.inc"
};

AnimationRecord D_actor_510900_80153A24[274] = {
#include "assets/actor_510900_animation_22074_records.inc"
};

u16 D_actor_510900_80153E6C[20] = {
#include "assets/actor_510900_animation_22074_indices.inc"
};

AnimationSet D_actor_510900_80153E94 = {
    D_actor_510900_80153A24,
    D_actor_510900_80153E6C,
    { NULL, D_actor_510900_801535EC, NULL, NULL, D_actor_510900_80153718, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80153EBC[6] = {
#include "assets/actor_510900_animation_223F8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80153F04[79] = {
#include "assets/actor_510900_animation_223F8_bank4.inc"
};

AnimationRecord D_actor_510900_80154040[108] = {
#include "assets/actor_510900_animation_223F8_records.inc"
};

u16 D_actor_510900_801541F0[20] = {
#include "assets/actor_510900_animation_223F8_indices.inc"
};

AnimationSet D_actor_510900_80154218 = {
    D_actor_510900_80154040,
    D_actor_510900_801541F0,
    { NULL, D_actor_510900_80153EBC, NULL, NULL, D_actor_510900_80153F04, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80154240[55] = {
#include "assets/actor_510900_animation_24128_bank1.inc"
};

AnimationPackedRotation D_actor_510900_801544D4[785] = {
#include "assets/actor_510900_animation_24128_bank4.inc"
};

AnimationRecord D_actor_510900_80155118[898] = {
#include "assets/actor_510900_animation_24128_records.inc"
};

u16 D_actor_510900_80155F20[20] = {
#include "assets/actor_510900_animation_24128_indices.inc"
};

AnimationSet D_actor_510900_80155F48 = {
    D_actor_510900_80155118,
    D_actor_510900_80155F20,
    { NULL, D_actor_510900_80154240, NULL, NULL, D_actor_510900_801544D4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80155F70[27] = {
#include "assets/actor_510900_animation_24FE8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_801560B4[386] = {
#include "assets/actor_510900_animation_24FE8_bank4.inc"
};

AnimationRecord D_actor_510900_801566BC[457] = {
#include "assets/actor_510900_animation_24FE8_records.inc"
};

u16 D_actor_510900_80156DE0[20] = {
#include "assets/actor_510900_animation_24FE8_indices.inc"
};

AnimationSet D_actor_510900_80156E08 = {
    D_actor_510900_801566BC,
    D_actor_510900_80156DE0,
    { NULL, D_actor_510900_80155F70, NULL, NULL, D_actor_510900_801560B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80156E30[41] = {
#include "assets/actor_510900_animation_267A8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015701C[623] = {
#include "assets/actor_510900_animation_267A8_bank4.inc"
};

AnimationRecord D_actor_510900_801579D8[754] = {
#include "assets/actor_510900_animation_267A8_records.inc"
};

u16 D_actor_510900_801585A0[20] = {
#include "assets/actor_510900_animation_267A8_indices.inc"
};

AnimationSet D_actor_510900_801585C8 = {
    D_actor_510900_801579D8,
    D_actor_510900_801585A0,
    { NULL, D_actor_510900_80156E30, NULL, NULL, D_actor_510900_8015701C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801585F0[23] = {
#include "assets/actor_510900_animation_273B8_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80158704[303] = {
#include "assets/actor_510900_animation_273B8_bank4.inc"
};

AnimationRecord D_actor_510900_80158BC0[380] = {
#include "assets/actor_510900_animation_273B8_records.inc"
};

u16 D_actor_510900_801591B0[20] = {
#include "assets/actor_510900_animation_273B8_indices.inc"
};

AnimationSet D_actor_510900_801591D8 = {
    D_actor_510900_80158BC0,
    D_actor_510900_801591B0,
    { NULL, D_actor_510900_801585F0, NULL, NULL, D_actor_510900_80158704, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80159200[13] = {
#include "assets/actor_510900_animation_27994_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015929C[119] = {
#include "assets/actor_510900_animation_27994_bank4.inc"
};

AnimationRecord D_actor_510900_80159478[197] = {
#include "assets/actor_510900_animation_27994_records.inc"
};

u16 D_actor_510900_8015978C[20] = {
#include "assets/actor_510900_animation_27994_indices.inc"
};

AnimationSet D_actor_510900_801597B4 = {
    D_actor_510900_80159478,
    D_actor_510900_8015978C,
    { NULL, D_actor_510900_80159200, NULL, NULL, D_actor_510900_8015929C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801597DC[14] = {
#include "assets/actor_510900_animation_27FDC_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80159884[153] = {
#include "assets/actor_510900_animation_27FDC_bank4.inc"
};

AnimationRecord D_actor_510900_80159AE8[187] = {
#include "assets/actor_510900_animation_27FDC_records.inc"
};

u16 D_actor_510900_80159DD4[20] = {
#include "assets/actor_510900_animation_27FDC_indices.inc"
};

AnimationSet D_actor_510900_80159DFC = {
    D_actor_510900_80159AE8,
    D_actor_510900_80159DD4,
    { NULL, D_actor_510900_801597DC, NULL, NULL, D_actor_510900_80159884, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80159E24[85] = {
#include "assets/actor_510900_animation_2A068_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015A220[821] = {
#include "assets/actor_510900_animation_2A068_bank4.inc"
};

AnimationRecord D_actor_510900_8015AEF4[987] = {
#include "assets/actor_510900_animation_2A068_records.inc"
};

u16 D_actor_510900_8015BE60[20] = {
#include "assets/actor_510900_animation_2A068_indices.inc"
};

AnimationSet D_actor_510900_8015BE88 = {
    D_actor_510900_8015AEF4,
    D_actor_510900_8015BE60,
    { NULL, D_actor_510900_80159E24, NULL, NULL, D_actor_510900_8015A220, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8015BEB0[47] = {
#include "assets/actor_510900_animation_2B800_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015C0E4[610] = {
#include "assets/actor_510900_animation_2B800_bank4.inc"
};

AnimationRecord D_actor_510900_8015CA6C[739] = {
#include "assets/actor_510900_animation_2B800_records.inc"
};

u16 D_actor_510900_8015D5F8[20] = {
#include "assets/actor_510900_animation_2B800_indices.inc"
};

AnimationSet D_actor_510900_8015D620 = {
    D_actor_510900_8015CA6C,
    D_actor_510900_8015D5F8,
    { NULL, D_actor_510900_8015BEB0, NULL, NULL, D_actor_510900_8015C0E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8015D648[2] = {
#include "assets/actor_510900_animation_2BC04_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015D660[82] = {
#include "assets/actor_510900_animation_2BC04_bank4.inc"
};

AnimationRecord D_actor_510900_8015D7A8[149] = {
#include "assets/actor_510900_animation_2BC04_records.inc"
};

u16 D_actor_510900_8015D9FC[20] = {
#include "assets/actor_510900_animation_2BC04_indices.inc"
};

AnimationSet D_actor_510900_8015DA24 = {
    D_actor_510900_8015D7A8,
    D_actor_510900_8015D9FC,
    { NULL, D_actor_510900_8015D648, NULL, NULL, D_actor_510900_8015D660, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_8015DA4C[89] = {
#include "assets/actor_510900_animation_2E9DC_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8015DE78[1150] = {
#include "assets/actor_510900_animation_2E9DC_bank4.inc"
};

AnimationRecord D_actor_510900_8015F070[1497] = {
#include "assets/actor_510900_animation_2E9DC_records.inc"
};

u16 D_actor_510900_801607D4[20] = {
#include "assets/actor_510900_animation_2E9DC_indices.inc"
};

AnimationSet D_actor_510900_801607FC = {
    D_actor_510900_8015F070,
    D_actor_510900_801607D4,
    { NULL, D_actor_510900_8015DA4C, NULL, NULL, D_actor_510900_8015DE78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80160824[78] = {
#include "assets/actor_510900_animation_324C0_bank1.inc"
};

AnimationPackedRotation D_actor_510900_80160BCC[1574] = {
#include "assets/actor_510900_animation_324C0_bank4.inc"
};

AnimationRecord D_actor_510900_80162464[1941] = {
#include "assets/actor_510900_animation_324C0_records.inc"
};

u16 D_actor_510900_801642B8[20] = {
#include "assets/actor_510900_animation_324C0_indices.inc"
};

AnimationSet D_actor_510900_801642E0 = {
    D_actor_510900_80162464,
    D_actor_510900_801642B8,
    { NULL, D_actor_510900_80160824, NULL, NULL, D_actor_510900_80160BCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_80164308[131] = {
#include "assets/actor_510900_animation_353AC_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8016492C[1141] = {
#include "assets/actor_510900_animation_353AC_bank4.inc"
};

AnimationRecord D_actor_510900_80165B00[1449] = {
#include "assets/actor_510900_animation_353AC_records.inc"
};

u16 D_actor_510900_801671A4[20] = {
#include "assets/actor_510900_animation_353AC_indices.inc"
};

AnimationSet D_actor_510900_801671CC = {
    D_actor_510900_80165B00,
    D_actor_510900_801671A4,
    { NULL, D_actor_510900_80164308, NULL, NULL, D_actor_510900_8016492C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801671F4[2] = {
#include "assets/actor_510900_animation_35474_bank1.inc"
};

AnimationPackedRotation D_actor_510900_8016720C[6] = {
#include "assets/actor_510900_animation_35474_bank4.inc"
};

AnimationRecord D_actor_510900_80167224[22] = {
#include "assets/actor_510900_animation_35474_records.inc"
};

u16 D_actor_510900_8016727C[12] = {
#include "assets/actor_510900_animation_35474_indices.inc"
};

AnimationSet D_actor_510900_80167294 = {
    D_actor_510900_80167224,
    D_actor_510900_8016727C,
    { NULL, D_actor_510900_801671F4, NULL, NULL, D_actor_510900_8016720C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_510900_801672BC[20] = {
#include "assets/actor_510900_animation_35B20_bank1.inc"
};

AnimationPackedRotation D_actor_510900_801673AC[143] = {
#include "assets/actor_510900_animation_35B20_bank4.inc"
};

AnimationRecord D_actor_510900_801675E8[208] = {
#include "assets/actor_510900_animation_35B20_records.inc"
};

u16 D_actor_510900_80167928[12] = {
#include "assets/actor_510900_animation_35B20_indices.inc"
};

AnimationSet D_actor_510900_80167940 = {
    D_actor_510900_801675E8,
    D_actor_510900_80167928,
    { NULL, D_actor_510900_801672BC, NULL, NULL, D_actor_510900_801673AC, NULL, NULL, NULL },
};

DamageAttack D_actor_510900_80167968 = { 14, 7 };

void func_actor_510900_80131F24(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordPointLight*          slot;
    WorldCoordTransientPointLight* lightSlot;
    EffectWork*                    eff;
    GfxRotationWords*              mat;
    s32                            i;
    s32                            bits;
    s32                            z;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[2];
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
        }
        if (arg0->spawnArg1.value == 4) {
            effectKillTask(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        mat                 = (GfxRotationWords*)&coord->coord;
        coord->parent       = mem->parent;
        mat->m00M01         = ONE;
        mat->m02M10         = 0;
        mat->m11M12         = ONE;
        mat->m20M21         = 0;
        mat->m22            = ONE;
        coord->coord.t[0]   = mem->pos.vx;
        coord->coord.t[1]   = mem->pos.vy;
        z                   = mem->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = z;
        arg0->state         = 1;
    }
    Gp_UpdateCoord(coord);
    if (lightSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
        slot->head.color.r = 0x1000;
        slot->head.color.g = 0x800;
        slot->head.color.b = 0x400;
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        lightSlot->framesLeft--;
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
        lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
        if (lightSlot->framesLeft == WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            arg0->spawnArg1.value = 0;
            mem->age              = 0;
            mem->scale            = 0;
        }
    }
    switch (arg0->spawnArg1.value) {
        case 0:
            break;
        case 1:
            mem->scale = (mem->scale < 0x100) ? mem->scale + 0x10 : 0x100;
            for (i = 0; i < 2; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x2C0) - 0x80;
                mem->move.vy    = 0x40;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60045, coord, ((gRandomLcgState >> 16) & 0xF0) + mem->scale, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            lightSlot->framesLeft = 0x10;
            slot->inner           = 0x1F40;
            slot->outer           = 0x2710;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            bits                  = gRandomLcgState >> 16;
            /* The `field_24 + 0x10000` sums below are evaluated as their own
             * operand. Written plainly, `fold` reassociates the constant onto
             * the draw; held in a local, sched1 moves the load ahead of it. */
            if (!(bits & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0x40;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60045, coord, ((gRandomLcgState >> 16) & 0xF0) + ({ mem->scale + 0x10000; }),
                                              &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x6004C, coord, ((gRandomLcgState >> 16) & 0xF0) + ({ mem->scale + 0x10000; }),
                                              &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60052, coord, ((gRandomLcgState >> 16) & 0xF0) + mem->scale, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (bits % 3 == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0x80;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60052, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = -0x80;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60059, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x180, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            break;
        case 2:
            lightSlot->framesLeft = 0x10;
            slot->inner           = 0x1F40;
            slot->outer           = 0x2710;
            for (i = 0; i < 3; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0x40;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60045, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x10100, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            bits            = gRandomLcgState >> 16;
            if (!(bits & 7)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60052, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x100, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = 0x80;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60052, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            bits >>= 1;
            if (!(bits & 7)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy    = -0x80;
                mem->move.vz    = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff             = Gp_SpawnEff(0x60059, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x180, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            break;
        case 3:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            bits            = gRandomLcgState >> 16;
            mem->age++;
            if (mem->age < 0x1E) {
                if (!(bits & 7)) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = -((gRandomLcgState >> 16) % 0x2C0) - 0x80;
                    mem->move.vy    = 0x40;
                    mem->move.vz    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    eff             = Gp_SpawnEff(0x60045, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x80, &mem->move);
                    if (eff != NULL) {
                        taskReparent(arg0, eff->task);
                    }
                }
                bits >>= 1;
                if (!(bits & 3)) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy    = 0x40;
                    mem->move.vz    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    eff             = Gp_SpawnEff(0x60045, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                    if (eff != NULL) {
                        taskReparent(arg0, eff->task);
                    }
                }
                for (i = 0; i < 2; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy    = 0;
                    mem->move.vz    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    eff             = Gp_SpawnEff(0x6004C, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x10100, &mem->move);
                    if (eff != NULL) {
                        taskReparent(arg0, eff->task);
                    }
                }
                bits >>= 1;
                if (!(bits & 7)) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy    = 0;
                    mem->move.vz    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    eff             = Gp_SpawnEff(0x60052, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x100, &mem->move);
                    if (eff != NULL) {
                        taskReparent(arg0, eff->task);
                    }
                }
                bits >>= 1;
                if (!(bits & 7)) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy    = 0x80;
                    mem->move.vz    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    eff             = Gp_SpawnEff(0x60052, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
                    if (eff != NULL) {
                        taskReparent(arg0, eff->task);
                    }
                }
                for (i = 0; i < 2; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                    mem->move.vy    = -0x80;
                    mem->move.vz    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    eff             = Gp_SpawnEff(0x60059, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x180, &mem->move);
                    if (eff != NULL) {
                        taskReparent(arg0, eff->task);
                    }
                }
                lightSlot->framesLeft = 0x10;
                slot->inner           = 0x1F40;
                slot->outer           = 0x2710;
            } else if (mem->age < 0x3C) {
                lightSlot->framesLeft = 2;
                slot->inner           = 0x190;
                slot->outer           = 0x190;
                gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx          = -((gRandomLcgState >> 16) % 0x280) - 0x80;
                mem->move.vy          = 0x80;
                mem->move.vz          = 0;
                gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                eff                   = Gp_SpawnEff(0x60059, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x100, &mem->move);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
            }
            break;
        case 4:
            effectKillTask(mem, arg0);
            break;
    }
}

void func_actor_510900_80132D4C(Task* arg0)
{
    GfxCoord            hit;
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* projectionScratch;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 x;
    u8                  col;
    u16                 vz;
    s32                 x2;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        projectionScratch                        = head - 1;
        projectionScratch->worldPoint.vx         = (u16)coord->workm.t[0];
        block                                    = projectionScratch;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                mem->scale      = (u16)arg0->spawnArg1.value & 0xFFF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->angle      = (gRandomLcgState >> 16) & 0xF;
                if (arg0->spawnArg1.value & 0x10000) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->period     = (gRandomLcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            if (mem->angle - 8 < mem->age) {
                x2 = (mem->angle - mem->age + 1) * 16;
                __asm__ volatile("" : "=r"(col) : "0"(x2));
                prim->r0 = x2;
                prim->g0 = x2;
                prim->b0 = x2;
            } else {
                col         = 0x80;
                prim->code |= 1;
            }
            prim->tpage            = 0x2B;
            prim->clut             = 0x4380;
            prim->code            |= 2;
            prim->u0               = (mem->age % 6) * 32;
            prim->v0               = 0;
            prim->u1               = (mem->age % 6) * 32 + 0x1F;
            prim->v1               = 0;
            prim->u2               = (mem->age % 6) * 32;
            prim->v2               = 0x27;
            prim->u3               = (mem->age % 6) * 32 + 0x1F;
            prim->v3               = 0x27;
            block->extent.corner.x = (mem->scale * 31) / block->depth;
            block->extent.corner.y = (mem->scale * 39) / block->depth;
            x                      = block->screenX - (u16)block->extent.corner.x;
            prim->x2               = x;
            prim->x0               = x;
            x                      = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = x;
            prim->x1               = x;
            x                      = block->screenY - (u16)block->extent.corner.y;
            prim->y1               = x;
            prim->y0               = x;
            x                      = block->screenY + (u16)block->extent.corner.y;
            prim->y3               = x;
            prim->y2               = x;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            if (coord->coord.t[1] < 0 && (mem->age & 1)) {
                hit.parent       = coord->parent;
                hit.coord.t[0]   = coord->coord.t[0];
                hit.coord.t[1]   = 0;
                hit.coord.t[2]   = coord->coord.t[2];
                hit.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&hit);
                Gp_DrawEffSprite7C(&hit, mem->scale >> 1, col);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        x = mem->period;
        if (x != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= x;
        }
        mem->age++;
        if (mem->angle >= mem->age) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void func_actor_510900_801332EC(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 x;
    s32                 amt;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        Gp_UpdateCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1.value & 0xFFF) {
                    amt = (u16)arg0->spawnArg1.value & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->scale = amt;
                if (arg0->spawnArg1.value & 0x10000) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->period     = (gRandomLcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            prim->tpage            = 0x2B;
            prim->code            |= 3;
            amt                    = mem->age;
            prim->clut             = (amt & 0x3F) | 0x43C0;
            amt                    = mem->age;
            prim->v0               = 0x70;
            prim->u0               = amt * 32;
            amt                    = mem->age;
            prim->v1               = 0x70;
            prim->u1               = amt * 32 + 0x1F;
            amt                    = mem->age;
            prim->v2               = 0x9F;
            prim->u2               = amt * 32;
            amt                    = mem->age;
            prim->v3               = 0x9F;
            prim->u3               = amt * 32 + 0x1F;
            block->extent.corner.x = (mem->scale * 31) / block->depth;
            block->extent.corner.y = (mem->scale * 47) / block->depth;
            x                      = block->screenX - (u16)block->extent.corner.x;
            prim->x2               = x;
            prim->x0               = x;
            x                      = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = x;
            prim->x1               = x;
            x                      = block->screenY - (u16)block->extent.corner.y;
            prim->y1               = x;
            prim->y0               = x;
            x                      = block->screenY + (u16)block->extent.corner.y;
            prim->y3               = x;
            prim->y2               = x;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        x = mem->period;
        if (x != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= x;
        }
        mem->age++;
        if (mem->age < 8) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

void func_actor_510900_8013371C(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 x;
    s32                 amt;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        Gp_UpdateCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1.value & 0xFFF) {
                    amt = (u16)arg0->spawnArg1.value & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->scale = amt;
                if (mem->period = (u16)((u32)arg0->spawnArg1.value >> 16) & 1) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->period     = (gRandomLcgState >> 16) % 0x30;
                }
                arg0->state++;
            }
            prim->tpage              = 0x2B;
            prim->code              |= 3;
            amt                      = mem->age;
            prim->clut               = ((amt + 8) & 0x3F) | 0x43C0;
            x                        = mem->age;
            prim->u0                 = (s16)(x % 8) * 32;
            x                        = mem->age;
            prim->v0                 = (x / 8) * 48 - 0x60;
            x                        = mem->age;
            prim->u1                 = (s16)(x % 8) * 32 + 0x1F;
            x                        = mem->age;
            prim->v1                 = (x / 8) * 48 - 0x60;
            x                        = mem->age;
            prim->u2                 = (s16)(x % 8) * 32;
            x                        = mem->age;
            prim->v2                 = (x / 8) * 48 - 0x31;
            x                        = mem->age;
            prim->u3                 = (s16)(x % 8) * 32 + 0x1F;
            x                        = mem->age;
            prim->v3                 = (x / 8) * 48 - 0x31;
            block->extent.corner.x   = (mem->scale * 31) / block->depth;
            block->extent.corner.y   = (mem->scale * 47) / block->depth;
            block->extent.corner.x >>= mem->period;
            x                        = block->screenX - (u16)block->extent.corner.x;
            prim->x2                 = x;
            prim->x0                 = x;
            x                        = block->screenX + (u16)block->extent.corner.x;
            prim->x3                 = x;
            prim->x1                 = x;
            x                        = block->screenY - (u16)block->extent.corner.y;
            prim->y1                 = x;
            prim->y0                 = x;
            x                        = block->screenY + (u16)block->extent.corner.y;
            prim->y3                 = x;
            prim->y2                 = x;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (mem->period != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += 0x38;
        }
        mem->age++;
        if (mem->age < 12) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

void func_actor_510900_80133C84(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 x;
    s32                 amt;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        Gp_UpdateCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                if (arg0->spawnArg1.value & 0xFFF) {
                    amt = (u16)arg0->spawnArg1.value & 0xFFF;
                } else {
                    amt = 0x200;
                }
                mem->scale      = amt;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->period     = (gRandomLcgState >> 16) % 0x30;
                arg0->state++;
            }
            prim->tpage            = 0x4B;
            prim->code            |= 3;
            prim->clut             = 0x4382;
            x                      = mem->age;
            prim->v0               = 0xD0;
            prim->u0               = (x / 2 + 4) * 32;
            x                      = mem->age;
            prim->v1               = 0xD0;
            prim->u1               = (x / 2 + 4) * 32 + 0x1F;
            x                      = mem->age;
            prim->v2               = 0xEF;
            prim->u2               = (x / 2 + 4) * 32;
            x                      = mem->age;
            prim->v3               = 0xEF;
            prim->u3               = (x / 2 + 4) * 32 + 0x1F;
            block->extent.corner.x = (mem->scale * 31) / block->depth;
            block->extent.corner.y = (mem->scale * 31) / block->depth;
            x                      = block->screenX - (u16)block->extent.corner.x;
            prim->x2               = x;
            prim->x0               = x;
            x                      = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = x;
            prim->x1               = x;
            x                      = block->screenY - (u16)block->extent.corner.y;
            prim->y1               = x;
            prim->y0               = x;
            x                      = block->screenY + (u16)block->extent.corner.y;
            prim->y3               = x;
            prim->y2               = x;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        x = mem->period;
        if (x != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= x;
        }
        mem->age++;
        if (mem->age < 8) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

void func_actor_510900_801340E8(Task* arg0)
{
    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    eff;
    GfxCoord*                      coord;
    GfxRotationWords*              mat;
    s32                            i;

    lightSlot  = &gWorldCoordTransientPointLights[3];
    lightCoord = &lightSlot->light.head.transform.coord;
    eff        = arg0->spawnArg2.pointer;
    coord      = arg0->extra.coordBody->coord;
    pointLight = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        effectKillTask(eff, arg0);
        return;
    }
    mat                 = (GfxRotationWords*)&coord->coord;
    coord->parent       = eff->parent;
    mat->m00M01         = ONE;
    mat->m02M10         = 0;
    mat->m11M12         = ONE;
    mat->m20M21         = 0;
    mat->m22            = ONE;
    coord->coord.t[0]   = eff->pos.vx;
    coord->coord.t[1]   = eff->pos.vy;
    coord->coord.t[2]   = eff->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    eff->move.vx = -0x200;
    eff->move.vy = 0x40;
    eff->move.vz = 0;
    Gp_SpawnEff(0x6003B, coord, 0x180, &eff->move);
    for (i = 0; i < 6; i++) {
        Gp_SpawnEff(0x60065, coord, 0, &eff->move);
        Gp_SpawnEff(0x600A4, coord, 1, NULL);
    }
    lightSlot->framesLeft    = 4;
    pointLight->inner        = 0xFA0;
    pointLight->outer        = 0x12C0;
    pointLight->head.color.r = 0xC00;
    pointLight->head.color.g = 0x800;
    pointLight->head.color.b = 0x400;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    effectKillTask(eff, arg0);
}

/// One frame of the trail effect: the coordinate drifts by a per-effect random
/// step, and the segment between last frame's position and this one is drawn as
/// a `LINE_F2` that fades out over `field_24 * 16` frames.
void func_actor_510900_80134284(Task* arg0)
{
    Actor510900TrailScratch* block;
    EffectWork*              eff;
    GfxCoord*                coord;
    LINE_F2*                 prim;
    s16                      mode;
    s16                      step;
    s32                      rng;
    s16                      val;
    s16                      count;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor510900TrailScratch));
    block = SCRATCH_STACK_CURSOR(Actor510900TrailScratch);
    eff   = arg0->spawnArg2.pointer;
    mode  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (mode != ROOM_EFFECT_CONTROL_RUNNING) {
        if (mode >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(eff, arg0);
        }
        return;
    }
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        eff->move.vx    = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        eff->move.vy    = -((gRandomLcgState >> 16) & 0x3F) - 0x10;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        eff->move.vz    = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        step            = 2;
        if (((gRandomLcgState >> 16) & 3) != 0) {
            step = 1;
        }
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        eff->scale      = step;
        eff->angle      = (((u32)rng >> 16) & 1) + 1;
        gRandomLcgState = rng;
        arg0->state++;
    }
    block->vec0.vx      = (u16)coord->workm.t[0];
    block->vec0.vy      = (u16)coord->workm.t[1];
    block->vec0.vz      = (u16)coord->workm.t[2];
    coord->coord.t[0]  += eff->move.vx;
    coord->coord.t[1]  += eff->move.vy;
    coord->coord.t[2]  += eff->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    block->vec1.vx = (u16)coord->workm.t[0];
    block->vec1.vy = (u16)coord->workm.t[1];
    block->vec1.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec0);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->vec1);
        gte_rtps();
        gte_stsxy(&block->sxy1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 3);
            setcode(prim, 0x40);
            val      = 0xFF - (eff->age << (5 - eff->scale));
            prim->r0 = val;
            prim->g0 = val >> eff->angle;
            prim->b0 = val >> 3;
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz0 + block->otz1) >> 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, (block->otz0 + block->otz1) >> 1);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor510900TrailScratch));
    eff->move.vy += 6;
    count         = eff->age + 1;
    eff->age      = count;
    if (count > eff->scale * 16 - 1) {
        effectKillTask(eff, arg0);
    }
}

void func_actor_510900_801346D4(Task* arg0)
{
    EffectWork* eff;
    GfxCoord*   coord;
    s16         mode;

    eff   = arg0->spawnArg2.pointer;
    mode  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (mode != ROOM_EFFECT_CONTROL_RUNNING) {
        if (mode >= ROOM_EFFECT_CONTROL_CANCEL_MIN || arg0->state == 4) {
            effectKillTask(eff, arg0);
        }
        return;
    }
    eff->age++;
    switch (arg0->state) {
        case 0:
            eff = Gp_SpawnEff(0x60184, coord, 0x480, NULL);
            if (eff != NULL) {
                taskReparent(arg0, eff->task);
            }
            arg0->state++;
            break;
        case 1:
            if (eff->age >= 9) {
                arg0->state++;
            }
            break;
        case 2:
            Gp_SpawnEff(0x60070, coord, 0x82004400, NULL);
            if (eff->age >= 0x33) {
                arg0->state++;
            }
            break;
        case 3:
            Gp_SpawnEff(0x60070, coord, 0xD2004400, NULL);
            if (eff->age >= 0x3D) {
                arg0->state++;
            }
            break;
        case 4:
            effectKillTask(eff, arg0);
            break;
    }
}

void func_actor_510900_8013482C(Task* arg0)
{
    EffectWork* eff;
    EffectWork* spawned;
    GfxCoord*   coord;
    s16         mode;
    s16         scale;
    s16         step;
    s32         tmp;
    s32         i;
    s32         n;

    eff   = arg0->spawnArg2.pointer;
    mode  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (mode != ROOM_EFFECT_CONTROL_RUNNING) {
        if (mode >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(eff, arg0);
        }
        return;
    }
    eff->age++;
    if (arg0->state == 0) {
        scale = 0x300;
        if (arg0->spawnArg1.value & 0xFFF) {
            scale = arg0->spawnArg1.halves.low & 0xFFF;
        }
        eff->scale      = scale;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        eff->angle      = (gRandomLcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1.value & 0xF000) {
            step = (arg0->spawnArg1.value >> 12) & 0xF;
        } else {
            step = 2;
        }
        eff->period = step;
        eff->step   = (s32)((u16)eff->scale << 16) >> 23;
        tmp         = arg0->spawnArg1.signedBytes[3];
        eff->index  = tmp & 0xF;
        if (eff->index != 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            eff->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            eff->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            eff->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gte_lddp(eff->scale << 3);
            gte_ldsv(&eff->move);
            gte_gpf12();
            gte_stsv(&eff->move);
            gte_lddp(eff->index << 12);
            gte_ldsv(&eff->move);
            gte_gpf12();
            gte_stsv(&eff->move);
            gte_SetRotMatrix(&eff->parent->coord);
            gte_ldv0(&eff->move);
            gte_rtv0();
            gte_stsv(&eff->move);
        } else if (!(arg0->spawnArg1.value & 0xF0000000)) {
            n = gDisplayState.animFrame & 3;
            i = 0;
            if (n != 0) {
                do {
                    spawned = Gp_SpawnEff(0x60184, coord, ((s32)((u16)eff->scale << 16) >> 17) | 0x02001000, NULL);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                    i += 1;
                } while (i < n);
            }
            n = gDisplayState.animFrame & 1;
            i = 0;
            if (i < n) {
                do {
                    spawned = Gp_SpawnEff(0x60184, coord, ((s32)((u16)eff->scale << 16) >> 17) | 0x01002000, NULL);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                    i += 1;
                } while (i < n);
            }
        }
        eff->age--;
        arg0->state = 1;
    }
    spriteQuadDraw(coord, eff->age / eff->period, eff->scale, eff->angle);
    coord->coord.t[0]  += eff->move.vx;
    coord->coord.t[1]  += eff->move.vy;
    coord->coord.t[2]  += eff->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    eff->scale         += eff->step;
    if (eff->age > eff->period * (ARRAY_SIZE(gEffectSpriteAtlasFrames) - 1) - 1) {
        effectKillTask(eff, arg0);
    }
}

/// Packed additive texture page for the shared effect atlas, with this actor's palettes.
#define SPRITE_QUAD_TEXTURE_PAGE EFFECT_SPRITE_ATLAS_TEXTURE_PAGE
#define SPRITE_QUAD_CLUT         ((D_actor_510900_8013C48C[frame].clutY << 6) | ((D_actor_510900_8013C48C[frame].clutX >> 4) & 0x3F))
#define SPRITE_QUAD_UV_TABLE     gEffectSpriteAtlasFrames
/// Texel width and height of the shared atlas cells used with this actor's palettes.
#define SPRITE_QUAD_CELL_WIDTH EFFECT_SPRITE_ATLAS_CELL_SIZE
/// Perspective-sizing multiplier for the shared atlas sprite.
///
/// Uses the atlas's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE EFFECT_SPRITE_ATLAS_UV_SPAN
#include "../../shared/sprite_quad_draw.inc.c"

/// Spawn/setup handler. It allocates the 0x5C8-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate, pair
/// source and HP (`field_40`, seeded from the record's `hpMax`).
///
/// The block's rig is bound with `animationInitContext` over its nineteen slots, and
/// slots 1..18 are reset. Six enemies are spawned from `D_actor_510900_80167A18`; entries 2
/// and 3 are the two whose models get the current room's texture page and CLUT
/// row (`Gp_GetNestedAreaRec`, indexed by the context id's top nibble) and whose
/// tasks are kept in `field_568` / `field_56C`. Entry 2 also gets an effect
/// reparented onto this task.
///
/// The three list nodes at 0x47C / 0x4E4 / 0x504 are linked into the global
/// object lists with their collision tables (`Gp_InitRec18Table`), which also
/// sets each node's 0x8000 "last element" flag -- kept for the first node and
/// cleared again for the other two.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
void func_actor_510900_801350F8(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    GfxCoord*              coord;
    Actor510900Work*       work;
    Enemy*                 spawned;
    EffectWork*            eff;
    u32                    raw1;
    u32                    raw2;
    u32                    index1;
    u32                    index2;
    TmdObject*             model1;
    TmdObject*             model2;
    AreaPlacement*         entry1;
    AreaPlacement*         entry2;
    GameLocationKey        key;
    GameLocationKey*       sessionKey1;
    GameLocationKey*       sessionKey2;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    u8                     areaByte0;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor510900Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_45C;
    obj->colorMtx       = &work->field_43C;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = &arg1->extra.tmd->coords[3];
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &D_actor_510900_80167980;
    arg0->recs                 = work->rec49C;
    arg0->hp                   = D_actor_510900_80167980.hpMax;
    work->field_53C.coord      = &arg1->extra.tmd->coords[3];
    work->field_53C.spawnArgLo = 0x400;
    work->field_53C.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_510900_80167AA4, obj,
                         work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    work->field_592 = 1;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 1, 0, arg0);
    spawned     = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 2, 0, arg0);
    raw1        = arg0->placeKey;
    model1      = spawned->task->extra.tmd;
    sessionKey1 = &gGameSession->location.loc;
    key.stage   = sessionKey1->stage;
    key.area    = sessionKey1->area;
    index1      = raw1 >> 12;
    key.room    = sessionKey1->room;
    areaByte0   = gGameSession->location.loc.view;
    key.view    = areaByte0;
    areaSyncLocationVariant(&key);
    entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, index1);
    model1->texturePageOffset = entry1->texturePageOffset;
    model1->clutRowOffset     = entry1->clutRowOffset;
    if (model1->buffer != NULL) {
        tmdProcessStream(model1);
        tmdProcessStream(model1);
    }
    work->field_568 = spawned->task;
    eff             = Gp_SpawnEff(0x80060043, spawned->task->extra.tmd->coords, 0, NULL);
    if (eff != NULL) {
        work->field_564 = (s32*)eff->task;
        taskReparent(arg1, eff->task);
    }
    if (work->field_564 != NULL) {
        work->field_564[0xD] = 0;
    }
    spawned     = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 3, 0, arg0);
    raw2        = arg0->placeKey;
    model2      = spawned->task->extra.tmd;
    sessionKey2 = &gGameSession->location.loc;
    key.stage   = sessionKey2->stage;
    key.area    = sessionKey2->area;
    index2      = raw2 >> 12;
    key.room    = sessionKey2->room;
    areaByte0   = gGameSession->location.loc.view;
    key.view    = areaByte0;
    areaSyncLocationVariant(&key);
    entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->field_0, index2);
    model2->texturePageOffset = entry2->texturePageOffset;
    model2->clutRowOffset     = entry2->clutRowOffset;
    if (model2->buffer != NULL) {
        tmdProcessStream(model2);
        tmdProcessStream(model2);
    }
    work->field_56C = spawned->task;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 0, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 1, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 2, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 6, 0, arg0);
    work->obj47C.coord            = &arg1->extra.tmd->coords[3];
    records1                      = work->rec49C;
    work->obj47C.context.contacts = records1;
    work->obj47C.pos.vx           = 0;
    work->obj47C.pos.vy           = 0;
    work->obj47C.pos.vz           = 0;
    work->obj47C.key              = 0x3001B;
    work->obj47C.radius           = 0x1C2;
    work->obj47C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj47C);
    Gp_InitRec18Table(records1, 3, 0);
    records2                      = work->rec524;
    work->obj47C.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->obj4E4.coord            = work->field_568->extra.tmd->coords;
    work->obj4E4.context.contacts = records2;
    work->obj4E4.pos.vx           = -0x140;
    work->obj4E4.pos.vy           = 0x80;
    work->obj4E4.pos.vz           = 0;
    work->obj4E4.key              = 0;
    work->obj4E4.radius           = 0x190;
    work->obj4E4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj4E4);
    Gp_InitRec18Table(records2, 1, 0);
    work->obj4E4.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj504.coord            = &arg1->extra.tmd->coords[7];
    work->obj504.context.contacts = records2;
    work->obj504.pos.vx           = 0;
    work->obj504.pos.vy           = 0;
    work->obj504.pos.vz           = 0;
    work->obj504.key              = 0;
    work->obj504.radius           = 0x190;
    work->obj504.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj504);
    work->obj504.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->msgTable      = D_actor_510900_80167A6C;
    arg1->exitCallback  = func_actor_510900_8013B608;
    func_actor_510900_8013B524(arg1);
    func_actor_510900_8013B424(1);
    arg1->state = 1;
}

void func_actor_510900_801355B4(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    Actor510900Work* work;
    s32              snd;
    s32              pan;
    s32              pan2;
    s32              i;

    work                         = arg1->work;
    coord                        = arg1->extra.tmd->coords;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->field_586 == 0x20 && work->field_58A == 0xD2) {
        work->field_594 = 1;
        work->field_598 = 0xFF;
        snd             = (((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
        pan             = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        work->field_580 = (((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780011;
        pan2            = (s8)worldCoordGetOriginAudioPan(coord);
        SndEvt_EnqueueType6(work->field_580, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    work->field_58A++;
    for (i = 1; i < 0x13; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_actor_510900_8013BC38(arg1, coord);
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}
