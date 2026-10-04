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

/// Scratch-stack block one frame's segment of a debris streak is drawn from.
///
/// A streak, `EFFECT_NO9_GOLEM_DEBRIS_STREAK`, is a point that drifts a little
/// every frame. `endpoints` holds its world position before and after the
/// frame's drift, narrowed to signed 16-bit coordinate units, and the line
/// between the two is that frame's segment. Endpoints, depths and screen
/// positions share indices 0..1, the order of the line packet's vertices.
///
/// Each end is projected by an RTPS of its own. `projectionFlags` holds the
/// most recently stored GTE FLAG word, and a negative word after either
/// projection drops the segment. Unlike `EffectLineScratch`, the block keeps
/// a depth per end: the segment is sorted and blended at their mean.
///
/// Reserve one complete, word-aligned block and release it in scratch-stack
/// order after drawing; no pointer into the block survives release.
typedef struct {
    SVECTOR endpoints[2];       // World positions of the segment's two ends: before the frame's drift, then after it
    s32     depths[2];          // SZ3 / 4 of each end, stored once that end's projection is accepted; their mean is the ordering and blend depth
    s32     projectionFlags;    // Latest GTE FLAG word; bit 31 makes it negative and drops the segment
    DVECTOR screenEndpoints[2]; // Signed screen X/Y pixels, written together as one GTE word per end
} _Actor510900DebrisStreakScratch;
STATIC_ASSERT_SIZEOF(_Actor510900DebrisStreakScratch, 0x24);

/// VRAM coordinates of one palette for actor 510900's explosion fireball.
///
/// Each entry is the unencoded input of `getClut`: X in words, Y in scanlines.
/// The fireball drawer pairs entry `frame` with the same index of
/// `gEffectSpriteAtlasFrames`.
typedef struct {
    u16 clutX; // Palette X in VRAM words, aligned to 16 words.
    u16 clutY; // Palette Y in VRAM scanlines.
} _Actor510900SpritePalette;
STATIC_ASSERT_SIZEOF(_Actor510900SpritePalette, 4);

/// Twelve fireball palettes, one per frame of `gEffectSpriteAtlasFrames`.
///
/// Every entry sits on scanline 270. X runs from 80 through 256 in steps of
/// 16 words, one 16-colour palette per atlas frame. The fireball drawer in
/// this file is the only reader. The table stays in initialized data so that
/// drawer can address it at its image offset.
static const _Actor510900SpritePalette _gActor510900FireballFramePalettes[12] __attribute__((section(".data"))) = {
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

static TmdBone _gActor510900No9GolemAkropolisBodySkeleton[19] = {
#include "assets/no9_golem_akropolis_body_skeleton.inc"
};

static u32 _gActor510900No9GolemAkropolisBodyPartVerts[19] = {
#include "assets/no9_golem_akropolis_body_partVerts.inc"
};

static SVECTOR _gActor510900No9GolemAkropolisBodyVerts[358] = {
#include "assets/no9_golem_akropolis_body_verts.inc"
};

static SVECTOR _gActor510900No9GolemAkropolisBodyNormals[356] = {
#include "assets/no9_golem_akropolis_body_normals.inc"
};

static u32 _gActor510900No9GolemAkropolisBodyStream[3928] = {
#include "assets/no9_golem_akropolis_body_stream.inc"
};

TmdSource gActor510900No9GolemAkropolisBody = {
    0,
    21588,
    5944,
    19,
    _gActor510900No9GolemAkropolisBodyPartVerts,
    _gActor510900No9GolemAkropolisBodyVerts,
    _gActor510900No9GolemAkropolisBodyNormals,
    _gActor510900No9GolemAkropolisBodySkeleton,
    _gActor510900No9GolemAkropolisBodyStream,
};

static TmdBone _gActor510900Model0FE60Skeleton[1] = {
#include "assets/actor_510900_model_0FE60_skeleton.inc"
};

static u32 _gActor510900Model0FE60PartVerts[1] = {
#include "assets/actor_510900_model_0FE60_partVerts.inc"
};

static SVECTOR _gActor510900Model0FE60Verts[14] = {
#include "assets/actor_510900_model_0FE60_verts.inc"
};

static SVECTOR _gActor510900Model0FE60Normals[12] = {
#include "assets/actor_510900_model_0FE60_normals.inc"
};

static u32 _gActor510900Model0FE60Stream[98] = {
#include "assets/actor_510900_model_0FE60_stream.inc"
};

TmdSource gActor510900Model0FE60 = {
    0,
    652,
    0,
    1,
    _gActor510900Model0FE60PartVerts,
    _gActor510900Model0FE60Verts,
    _gActor510900Model0FE60Normals,
    _gActor510900Model0FE60Skeleton,
    _gActor510900Model0FE60Stream,
};

static TmdBone _gActor510900No9GolemAkropolisPropSkeleton[1] = {
#include "assets/no9_golem_akropolis_prop_skeleton.inc"
};

static u32 _gActor510900No9GolemAkropolisPropPartVerts[1] = {
#include "assets/no9_golem_akropolis_prop_partVerts.inc"
};

static SVECTOR _gActor510900No9GolemAkropolisPropVerts[14] = {
#include "assets/no9_golem_akropolis_prop_verts.inc"
};

static SVECTOR _gActor510900No9GolemAkropolisPropNormals[19] = {
#include "assets/no9_golem_akropolis_prop_normals.inc"
};

static u32 _gActor510900No9GolemAkropolisPropStream[114] = {
#include "assets/no9_golem_akropolis_prop_stream.inc"
};

TmdSource gActor510900No9GolemAkropolisProp = {
    0,
    724,
    0,
    1,
    _gActor510900No9GolemAkropolisPropPartVerts,
    _gActor510900No9GolemAkropolisPropVerts,
    _gActor510900No9GolemAkropolisPropNormals,
    _gActor510900No9GolemAkropolisPropSkeleton,
    _gActor510900No9GolemAkropolisPropStream,
};

static TmdBone _gActor510900Model10468Skeleton[1] = {
#include "assets/actor_510900_model_10468_skeleton.inc"
};

static u32 _gActor510900Model10468PartVerts[1] = {
#include "assets/actor_510900_model_10468_partVerts.inc"
};

static SVECTOR _gActor510900Model10468Verts[18] = {
#include "assets/actor_510900_model_10468_verts.inc"
};

static SVECTOR _gActor510900Model10468Normals[17] = {
#include "assets/actor_510900_model_10468_normals.inc"
};

static u32 _gActor510900Model10468Stream[126] = {
#include "assets/actor_510900_model_10468_stream.inc"
};

TmdSource gActor510900Model10468 = {
    0,
    860,
    0,
    1,
    _gActor510900Model10468PartVerts,
    _gActor510900Model10468Verts,
    _gActor510900Model10468Normals,
    _gActor510900Model10468Skeleton,
    _gActor510900Model10468Stream,
};

static TmdBone _gActor510900GolemGrenadeSkeleton[1] = {
#include "assets/golem_grenade_skeleton.inc"
};

static u32 _gActor510900GolemGrenadePartVerts[1] = {
#include "assets/golem_grenade_partVerts.inc"
};

static SVECTOR _gActor510900GolemGrenadeVerts[12] = {
#include "assets/golem_grenade_verts.inc"
};

static SVECTOR _gActor510900GolemGrenadeNormals[28] = {
#include "assets/golem_grenade_normals.inc"
};

static u32 _gActor510900GolemGrenadeStream[104] = {
#include "assets/golem_grenade_stream.inc"
};

TmdSource gActor510900GolemGrenade = {
    0,
    720,
    0,
    1,
    _gActor510900GolemGrenadePartVerts,
    _gActor510900GolemGrenadeVerts,
    _gActor510900GolemGrenadeNormals,
    _gActor510900GolemGrenadeSkeleton,
    _gActor510900GolemGrenadeStream,
};

static TmdBone _gActor510900Model10C8CSkeleton[11] = {
#include "assets/actor_510900_model_10C8C_skeleton.inc"
};

static u32 _gActor510900Model10C8CPartVerts[11] = {
#include "assets/actor_510900_model_10C8C_partVerts.inc"
};

static SVECTOR _gActor510900Model10C8CVerts[33] = {
#include "assets/actor_510900_model_10C8C_verts.inc"
};

static SVECTOR _gActor510900Model10C8CNormals[3] = {
#include "assets/actor_510900_model_10C8C_normals.inc"
};

static u32 _gActor510900Model10C8CStream[421] = {
#include "assets/actor_510900_model_10C8C_stream.inc"
};

TmdSource gActor510900Model10C8C = {
    0,
    1560,
    1404,
    11,
    _gActor510900Model10C8CPartVerts,
    _gActor510900Model10C8CVerts,
    _gActor510900Model10C8CNormals,
    _gActor510900Model10C8CSkeleton,
    _gActor510900Model10C8CStream,
};

static AnimationPackedPose _gActor510900Animation11CC0Bank1[18] = {
#include "assets/actor_510900_animation_11CC0_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation11CC0Bank4[231] = {
#include "assets/actor_510900_animation_11CC0_bank4.inc"
};

static AnimationRecord _gActor510900Animation11CC0Records[313] = {
#include "assets/actor_510900_animation_11CC0_records.inc"
};

static u16 _gActor510900Animation11CC0Indices[20] = {
#include "assets/actor_510900_animation_11CC0_indices.inc"
};

static AnimationSet _gActor510900Animation11CC0 = {
    _gActor510900Animation11CC0Records,
    _gActor510900Animation11CC0Indices,
    { NULL, _gActor510900Animation11CC0Bank1, NULL, NULL, _gActor510900Animation11CC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation11FB8Bank1[8] = {
#include "assets/actor_510900_animation_11FB8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation11FB8Bank4[57] = {
#include "assets/actor_510900_animation_11FB8_bank4.inc"
};

static AnimationRecord _gActor510900Animation11FB8Records[89] = {
#include "assets/actor_510900_animation_11FB8_records.inc"
};

static u16 _gActor510900Animation11FB8Indices[20] = {
#include "assets/actor_510900_animation_11FB8_indices.inc"
};

static AnimationSet _gActor510900Animation11FB8 = {
    _gActor510900Animation11FB8Records,
    _gActor510900Animation11FB8Indices,
    { NULL, _gActor510900Animation11FB8Bank1, NULL, NULL, _gActor510900Animation11FB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation129B0Bank1[29] = {
#include "assets/actor_510900_animation_129B0_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation129B0Bank4[221] = {
#include "assets/actor_510900_animation_129B0_bank4.inc"
};

static AnimationRecord _gActor510900Animation129B0Records[310] = {
#include "assets/actor_510900_animation_129B0_records.inc"
};

static u16 _gActor510900Animation129B0Indices[20] = {
#include "assets/actor_510900_animation_129B0_indices.inc"
};

static AnimationSet _gActor510900Animation129B0 = {
    _gActor510900Animation129B0Records,
    _gActor510900Animation129B0Indices,
    { NULL, _gActor510900Animation129B0Bank1, NULL, NULL, _gActor510900Animation129B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation13884Bank1[45] = {
#include "assets/actor_510900_animation_13884_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation13884Bank4[341] = {
#include "assets/actor_510900_animation_13884_bank4.inc"
};

static AnimationRecord _gActor510900Animation13884Records[453] = {
#include "assets/actor_510900_animation_13884_records.inc"
};

static u16 _gActor510900Animation13884Indices[20] = {
#include "assets/actor_510900_animation_13884_indices.inc"
};

static AnimationSet _gActor510900Animation13884 = {
    _gActor510900Animation13884Records,
    _gActor510900Animation13884Indices,
    { NULL, _gActor510900Animation13884Bank1, NULL, NULL, _gActor510900Animation13884Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation142A0Bank1[20] = {
#include "assets/actor_510900_animation_142A0_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation142A0Bank4[253] = {
#include "assets/actor_510900_animation_142A0_bank4.inc"
};

static AnimationRecord _gActor510900Animation142A0Records[314] = {
#include "assets/actor_510900_animation_142A0_records.inc"
};

static u16 _gActor510900Animation142A0Indices[20] = {
#include "assets/actor_510900_animation_142A0_indices.inc"
};

static AnimationSet _gActor510900Animation142A0 = {
    _gActor510900Animation142A0Records,
    _gActor510900Animation142A0Indices,
    { NULL, _gActor510900Animation142A0Bank1, NULL, NULL, _gActor510900Animation142A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation14E4CBank1[30] = {
#include "assets/actor_510900_animation_14E4C_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation14E4CBank4[277] = {
#include "assets/actor_510900_animation_14E4C_bank4.inc"
};

static AnimationRecord _gActor510900Animation14E4CRecords[360] = {
#include "assets/actor_510900_animation_14E4C_records.inc"
};

static u16 _gActor510900Animation14E4CIndices[20] = {
#include "assets/actor_510900_animation_14E4C_indices.inc"
};

static AnimationSet _gActor510900Animation14E4C = {
    _gActor510900Animation14E4CRecords,
    _gActor510900Animation14E4CIndices,
    { NULL, _gActor510900Animation14E4CBank1, NULL, NULL, _gActor510900Animation14E4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation172DCBank1[78] = {
#include "assets/actor_510900_animation_172DC_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation172DCBank4[974] = {
#include "assets/actor_510900_animation_172DC_bank4.inc"
};

static AnimationRecord _gActor510900Animation172DCRecords[1112] = {
#include "assets/actor_510900_animation_172DC_records.inc"
};

static u16 _gActor510900Animation172DCIndices[20] = {
#include "assets/actor_510900_animation_172DC_indices.inc"
};

static AnimationSet _gActor510900Animation172DC = {
    _gActor510900Animation172DCRecords,
    _gActor510900Animation172DCIndices,
    { NULL, _gActor510900Animation172DCBank1, NULL, NULL, _gActor510900Animation172DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation17CB8Bank1[27] = {
#include "assets/actor_510900_animation_17CB8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation17CB8Bank4[238] = {
#include "assets/actor_510900_animation_17CB8_bank4.inc"
};

static AnimationRecord _gActor510900Animation17CB8Records[292] = {
#include "assets/actor_510900_animation_17CB8_records.inc"
};

static u16 _gActor510900Animation17CB8Indices[20] = {
#include "assets/actor_510900_animation_17CB8_indices.inc"
};

static AnimationSet _gActor510900Animation17CB8 = {
    _gActor510900Animation17CB8Records,
    _gActor510900Animation17CB8Indices,
    { NULL, _gActor510900Animation17CB8Bank1, NULL, NULL, _gActor510900Animation17CB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation186E0Bank1[29] = {
#include "assets/actor_510900_animation_186E0_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation186E0Bank4[244] = {
#include "assets/actor_510900_animation_186E0_bank4.inc"
};

static AnimationRecord _gActor510900Animation186E0Records[299] = {
#include "assets/actor_510900_animation_186E0_records.inc"
};

static u16 _gActor510900Animation186E0Indices[20] = {
#include "assets/actor_510900_animation_186E0_indices.inc"
};

static AnimationSet _gActor510900Animation186E0 = {
    _gActor510900Animation186E0Records,
    _gActor510900Animation186E0Indices,
    { NULL, _gActor510900Animation186E0Bank1, NULL, NULL, _gActor510900Animation186E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation18B5CBank1[8] = {
#include "assets/actor_510900_animation_18B5C_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation18B5CBank4[106] = {
#include "assets/actor_510900_animation_18B5C_bank4.inc"
};

static AnimationRecord _gActor510900Animation18B5CRecords[137] = {
#include "assets/actor_510900_animation_18B5C_records.inc"
};

static u16 _gActor510900Animation18B5CIndices[20] = {
#include "assets/actor_510900_animation_18B5C_indices.inc"
};

static AnimationSet _gActor510900Animation18B5C = {
    _gActor510900Animation18B5CRecords,
    _gActor510900Animation18B5CIndices,
    { NULL, _gActor510900Animation18B5CBank1, NULL, NULL, _gActor510900Animation18B5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1A250Bank1[47] = {
#include "assets/actor_510900_animation_1A250_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1A250Bank4[613] = {
#include "assets/actor_510900_animation_1A250_bank4.inc"
};

static AnimationRecord _gActor510900Animation1A250Records[695] = {
#include "assets/actor_510900_animation_1A250_records.inc"
};

static u16 _gActor510900Animation1A250Indices[20] = {
#include "assets/actor_510900_animation_1A250_indices.inc"
};

static AnimationSet _gActor510900Animation1A250 = {
    _gActor510900Animation1A250Records,
    _gActor510900Animation1A250Indices,
    { NULL, _gActor510900Animation1A250Bank1, NULL, NULL, _gActor510900Animation1A250Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1A994Bank1[19] = {
#include "assets/actor_510900_animation_1A994_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1A994Bank4[160] = {
#include "assets/actor_510900_animation_1A994_bank4.inc"
};

static AnimationRecord _gActor510900Animation1A994Records[228] = {
#include "assets/actor_510900_animation_1A994_records.inc"
};

static u16 _gActor510900Animation1A994Indices[20] = {
#include "assets/actor_510900_animation_1A994_indices.inc"
};

static AnimationSet _gActor510900Animation1A994 = {
    _gActor510900Animation1A994Records,
    _gActor510900Animation1A994Indices,
    { NULL, _gActor510900Animation1A994Bank1, NULL, NULL, _gActor510900Animation1A994Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1BFA8Bank1[66] = {
#include "assets/actor_510900_animation_1BFA8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1BFA8Bank4[549] = {
#include "assets/actor_510900_animation_1BFA8_bank4.inc"
};

static AnimationRecord _gActor510900Animation1BFA8Records[646] = {
#include "assets/actor_510900_animation_1BFA8_records.inc"
};

static u16 _gActor510900Animation1BFA8Indices[20] = {
#include "assets/actor_510900_animation_1BFA8_indices.inc"
};

static AnimationSet _gActor510900Animation1BFA8 = {
    _gActor510900Animation1BFA8Records,
    _gActor510900Animation1BFA8Indices,
    { NULL, _gActor510900Animation1BFA8Bank1, NULL, NULL, _gActor510900Animation1BFA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1CBA8Bank1[19] = {
#include "assets/actor_510900_animation_1CBA8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1CBA8Bank4[310] = {
#include "assets/actor_510900_animation_1CBA8_bank4.inc"
};

static AnimationRecord _gActor510900Animation1CBA8Records[381] = {
#include "assets/actor_510900_animation_1CBA8_records.inc"
};

static u16 _gActor510900Animation1CBA8Indices[20] = {
#include "assets/actor_510900_animation_1CBA8_indices.inc"
};

static AnimationSet _gActor510900Animation1CBA8 = {
    _gActor510900Animation1CBA8Records,
    _gActor510900Animation1CBA8Indices,
    { NULL, _gActor510900Animation1CBA8Bank1, NULL, NULL, _gActor510900Animation1CBA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1D868Bank1[28] = {
#include "assets/actor_510900_animation_1D868_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1D868Bank4[322] = {
#include "assets/actor_510900_animation_1D868_bank4.inc"
};

static AnimationRecord _gActor510900Animation1D868Records[390] = {
#include "assets/actor_510900_animation_1D868_records.inc"
};

static u16 _gActor510900Animation1D868Indices[20] = {
#include "assets/actor_510900_animation_1D868_indices.inc"
};

static AnimationSet _gActor510900Animation1D868 = {
    _gActor510900Animation1D868Records,
    _gActor510900Animation1D868Indices,
    { NULL, _gActor510900Animation1D868Bank1, NULL, NULL, _gActor510900Animation1D868Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1E56CBank1[28] = {
#include "assets/actor_510900_animation_1E56C_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1E56CBank4[328] = {
#include "assets/actor_510900_animation_1E56C_bank4.inc"
};

static AnimationRecord _gActor510900Animation1E56CRecords[401] = {
#include "assets/actor_510900_animation_1E56C_records.inc"
};

static u16 _gActor510900Animation1E56CIndices[20] = {
#include "assets/actor_510900_animation_1E56C_indices.inc"
};

static AnimationSet _gActor510900Animation1E56C = {
    _gActor510900Animation1E56CRecords,
    _gActor510900Animation1E56CIndices,
    { NULL, _gActor510900Animation1E56CBank1, NULL, NULL, _gActor510900Animation1E56CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1EF7CBank1[19] = {
#include "assets/actor_510900_animation_1EF7C_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1EF7CBank4[255] = {
#include "assets/actor_510900_animation_1EF7C_bank4.inc"
};

static AnimationRecord _gActor510900Animation1EF7CRecords[312] = {
#include "assets/actor_510900_animation_1EF7C_records.inc"
};

static u16 _gActor510900Animation1EF7CIndices[20] = {
#include "assets/actor_510900_animation_1EF7C_indices.inc"
};

static AnimationSet _gActor510900Animation1EF7C = {
    _gActor510900Animation1EF7CRecords,
    _gActor510900Animation1EF7CIndices,
    { NULL, _gActor510900Animation1EF7CBank1, NULL, NULL, _gActor510900Animation1EF7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation1F668Bank1[19] = {
#include "assets/actor_510900_animation_1F668_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation1F668Bank4[153] = {
#include "assets/actor_510900_animation_1F668_bank4.inc"
};

static AnimationRecord _gActor510900Animation1F668Records[213] = {
#include "assets/actor_510900_animation_1F668_records.inc"
};

static u16 _gActor510900Animation1F668Indices[20] = {
#include "assets/actor_510900_animation_1F668_indices.inc"
};

static AnimationSet _gActor510900Animation1F668 = {
    _gActor510900Animation1F668Records,
    _gActor510900Animation1F668Indices,
    { NULL, _gActor510900Animation1F668Bank1, NULL, NULL, _gActor510900Animation1F668Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation2035CBank1[23] = {
#include "assets/actor_510900_animation_2035C_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation2035CBank4[326] = {
#include "assets/actor_510900_animation_2035C_bank4.inc"
};

static AnimationRecord _gActor510900Animation2035CRecords[414] = {
#include "assets/actor_510900_animation_2035C_records.inc"
};

static u16 _gActor510900Animation2035CIndices[20] = {
#include "assets/actor_510900_animation_2035C_indices.inc"
};

static AnimationSet _gActor510900Animation2035C = {
    _gActor510900Animation2035CRecords,
    _gActor510900Animation2035CIndices,
    { NULL, _gActor510900Animation2035CBank1, NULL, NULL, _gActor510900Animation2035CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation2093CBank1[8] = {
#include "assets/actor_510900_animation_2093C_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation2093CBank4[130] = {
#include "assets/actor_510900_animation_2093C_bank4.inc"
};

static AnimationRecord _gActor510900Animation2093CRecords[202] = {
#include "assets/actor_510900_animation_2093C_records.inc"
};

static u16 _gActor510900Animation2093CIndices[20] = {
#include "assets/actor_510900_animation_2093C_indices.inc"
};

static AnimationSet _gActor510900Animation2093C = {
    _gActor510900Animation2093CRecords,
    _gActor510900Animation2093CIndices,
    { NULL, _gActor510900Animation2093CBank1, NULL, NULL, _gActor510900Animation2093CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation213E4Bank1[18] = {
#include "assets/actor_510900_animation_213E4_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation213E4Bank4[281] = {
#include "assets/actor_510900_animation_213E4_bank4.inc"
};

static AnimationRecord _gActor510900Animation213E4Records[327] = {
#include "assets/actor_510900_animation_213E4_records.inc"
};

static u16 _gActor510900Animation213E4Indices[20] = {
#include "assets/actor_510900_animation_213E4_indices.inc"
};

static AnimationSet _gActor510900Animation213E4 = {
    _gActor510900Animation213E4Records,
    _gActor510900Animation213E4Indices,
    { NULL, _gActor510900Animation213E4Bank1, NULL, NULL, _gActor510900Animation213E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation217A4Bank1[10] = {
#include "assets/actor_510900_animation_217A4_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation217A4Bank4[79] = {
#include "assets/actor_510900_animation_217A4_bank4.inc"
};

static AnimationRecord _gActor510900Animation217A4Records[111] = {
#include "assets/actor_510900_animation_217A4_records.inc"
};

static u16 _gActor510900Animation217A4Indices[20] = {
#include "assets/actor_510900_animation_217A4_indices.inc"
};

static AnimationSet _gActor510900Animation217A4 = {
    _gActor510900Animation217A4Records,
    _gActor510900Animation217A4Indices,
    { NULL, _gActor510900Animation217A4Bank1, NULL, NULL, _gActor510900Animation217A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation22074Bank1[25] = {
#include "assets/actor_510900_animation_22074_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation22074Bank4[195] = {
#include "assets/actor_510900_animation_22074_bank4.inc"
};

static AnimationRecord _gActor510900Animation22074Records[274] = {
#include "assets/actor_510900_animation_22074_records.inc"
};

static u16 _gActor510900Animation22074Indices[20] = {
#include "assets/actor_510900_animation_22074_indices.inc"
};

static AnimationSet _gActor510900Animation22074 = {
    _gActor510900Animation22074Records,
    _gActor510900Animation22074Indices,
    { NULL, _gActor510900Animation22074Bank1, NULL, NULL, _gActor510900Animation22074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation223F8Bank1[6] = {
#include "assets/actor_510900_animation_223F8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation223F8Bank4[79] = {
#include "assets/actor_510900_animation_223F8_bank4.inc"
};

static AnimationRecord _gActor510900Animation223F8Records[108] = {
#include "assets/actor_510900_animation_223F8_records.inc"
};

static u16 _gActor510900Animation223F8Indices[20] = {
#include "assets/actor_510900_animation_223F8_indices.inc"
};

static AnimationSet _gActor510900Animation223F8 = {
    _gActor510900Animation223F8Records,
    _gActor510900Animation223F8Indices,
    { NULL, _gActor510900Animation223F8Bank1, NULL, NULL, _gActor510900Animation223F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation24128Bank1[55] = {
#include "assets/actor_510900_animation_24128_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation24128Bank4[785] = {
#include "assets/actor_510900_animation_24128_bank4.inc"
};

static AnimationRecord _gActor510900Animation24128Records[898] = {
#include "assets/actor_510900_animation_24128_records.inc"
};

static u16 _gActor510900Animation24128Indices[20] = {
#include "assets/actor_510900_animation_24128_indices.inc"
};

static AnimationSet _gActor510900Animation24128 = {
    _gActor510900Animation24128Records,
    _gActor510900Animation24128Indices,
    { NULL, _gActor510900Animation24128Bank1, NULL, NULL, _gActor510900Animation24128Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation24FE8Bank1[27] = {
#include "assets/actor_510900_animation_24FE8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation24FE8Bank4[386] = {
#include "assets/actor_510900_animation_24FE8_bank4.inc"
};

static AnimationRecord _gActor510900Animation24FE8Records[457] = {
#include "assets/actor_510900_animation_24FE8_records.inc"
};

static u16 _gActor510900Animation24FE8Indices[20] = {
#include "assets/actor_510900_animation_24FE8_indices.inc"
};

static AnimationSet _gActor510900Animation24FE8 = {
    _gActor510900Animation24FE8Records,
    _gActor510900Animation24FE8Indices,
    { NULL, _gActor510900Animation24FE8Bank1, NULL, NULL, _gActor510900Animation24FE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation267A8Bank1[41] = {
#include "assets/actor_510900_animation_267A8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation267A8Bank4[623] = {
#include "assets/actor_510900_animation_267A8_bank4.inc"
};

static AnimationRecord _gActor510900Animation267A8Records[754] = {
#include "assets/actor_510900_animation_267A8_records.inc"
};

static u16 _gActor510900Animation267A8Indices[20] = {
#include "assets/actor_510900_animation_267A8_indices.inc"
};

static AnimationSet _gActor510900Animation267A8 = {
    _gActor510900Animation267A8Records,
    _gActor510900Animation267A8Indices,
    { NULL, _gActor510900Animation267A8Bank1, NULL, NULL, _gActor510900Animation267A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation273B8Bank1[23] = {
#include "assets/actor_510900_animation_273B8_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation273B8Bank4[303] = {
#include "assets/actor_510900_animation_273B8_bank4.inc"
};

static AnimationRecord _gActor510900Animation273B8Records[380] = {
#include "assets/actor_510900_animation_273B8_records.inc"
};

static u16 _gActor510900Animation273B8Indices[20] = {
#include "assets/actor_510900_animation_273B8_indices.inc"
};

static AnimationSet _gActor510900Animation273B8 = {
    _gActor510900Animation273B8Records,
    _gActor510900Animation273B8Indices,
    { NULL, _gActor510900Animation273B8Bank1, NULL, NULL, _gActor510900Animation273B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation27994Bank1[13] = {
#include "assets/actor_510900_animation_27994_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation27994Bank4[119] = {
#include "assets/actor_510900_animation_27994_bank4.inc"
};

static AnimationRecord _gActor510900Animation27994Records[197] = {
#include "assets/actor_510900_animation_27994_records.inc"
};

static u16 _gActor510900Animation27994Indices[20] = {
#include "assets/actor_510900_animation_27994_indices.inc"
};

AnimationSet gActor510900Animation27994 = {
    _gActor510900Animation27994Records,
    _gActor510900Animation27994Indices,
    { NULL, _gActor510900Animation27994Bank1, NULL, NULL, _gActor510900Animation27994Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation27FDCBank1[14] = {
#include "assets/actor_510900_animation_27FDC_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation27FDCBank4[153] = {
#include "assets/actor_510900_animation_27FDC_bank4.inc"
};

static AnimationRecord _gActor510900Animation27FDCRecords[187] = {
#include "assets/actor_510900_animation_27FDC_records.inc"
};

static u16 _gActor510900Animation27FDCIndices[20] = {
#include "assets/actor_510900_animation_27FDC_indices.inc"
};

AnimationSet gActor510900Animation27FDC = {
    _gActor510900Animation27FDCRecords,
    _gActor510900Animation27FDCIndices,
    { NULL, _gActor510900Animation27FDCBank1, NULL, NULL, _gActor510900Animation27FDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation2A068Bank1[85] = {
#include "assets/actor_510900_animation_2A068_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation2A068Bank4[821] = {
#include "assets/actor_510900_animation_2A068_bank4.inc"
};

static AnimationRecord _gActor510900Animation2A068Records[987] = {
#include "assets/actor_510900_animation_2A068_records.inc"
};

static u16 _gActor510900Animation2A068Indices[20] = {
#include "assets/actor_510900_animation_2A068_indices.inc"
};

static AnimationSet _gActor510900Animation2A068 = {
    _gActor510900Animation2A068Records,
    _gActor510900Animation2A068Indices,
    { NULL, _gActor510900Animation2A068Bank1, NULL, NULL, _gActor510900Animation2A068Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation2B800Bank1[47] = {
#include "assets/actor_510900_animation_2B800_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation2B800Bank4[610] = {
#include "assets/actor_510900_animation_2B800_bank4.inc"
};

static AnimationRecord _gActor510900Animation2B800Records[739] = {
#include "assets/actor_510900_animation_2B800_records.inc"
};

static u16 _gActor510900Animation2B800Indices[20] = {
#include "assets/actor_510900_animation_2B800_indices.inc"
};

static AnimationSet _gActor510900Animation2B800 = {
    _gActor510900Animation2B800Records,
    _gActor510900Animation2B800Indices,
    { NULL, _gActor510900Animation2B800Bank1, NULL, NULL, _gActor510900Animation2B800Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation2BC04Bank1[2] = {
#include "assets/actor_510900_animation_2BC04_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation2BC04Bank4[82] = {
#include "assets/actor_510900_animation_2BC04_bank4.inc"
};

static AnimationRecord _gActor510900Animation2BC04Records[149] = {
#include "assets/actor_510900_animation_2BC04_records.inc"
};

static u16 _gActor510900Animation2BC04Indices[20] = {
#include "assets/actor_510900_animation_2BC04_indices.inc"
};

static AnimationSet _gActor510900Animation2BC04 = {
    _gActor510900Animation2BC04Records,
    _gActor510900Animation2BC04Indices,
    { NULL, _gActor510900Animation2BC04Bank1, NULL, NULL, _gActor510900Animation2BC04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation2E9DCBank1[89] = {
#include "assets/actor_510900_animation_2E9DC_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation2E9DCBank4[1150] = {
#include "assets/actor_510900_animation_2E9DC_bank4.inc"
};

static AnimationRecord _gActor510900Animation2E9DCRecords[1497] = {
#include "assets/actor_510900_animation_2E9DC_records.inc"
};

static u16 _gActor510900Animation2E9DCIndices[20] = {
#include "assets/actor_510900_animation_2E9DC_indices.inc"
};

static AnimationSet _gActor510900Animation2E9DC = {
    _gActor510900Animation2E9DCRecords,
    _gActor510900Animation2E9DCIndices,
    { NULL, _gActor510900Animation2E9DCBank1, NULL, NULL, _gActor510900Animation2E9DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation324C0Bank1[78] = {
#include "assets/actor_510900_animation_324C0_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation324C0Bank4[1574] = {
#include "assets/actor_510900_animation_324C0_bank4.inc"
};

static AnimationRecord _gActor510900Animation324C0Records[1941] = {
#include "assets/actor_510900_animation_324C0_records.inc"
};

static u16 _gActor510900Animation324C0Indices[20] = {
#include "assets/actor_510900_animation_324C0_indices.inc"
};

static AnimationSet _gActor510900Animation324C0 = {
    _gActor510900Animation324C0Records,
    _gActor510900Animation324C0Indices,
    { NULL, _gActor510900Animation324C0Bank1, NULL, NULL, _gActor510900Animation324C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation353ACBank1[131] = {
#include "assets/actor_510900_animation_353AC_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation353ACBank4[1141] = {
#include "assets/actor_510900_animation_353AC_bank4.inc"
};

static AnimationRecord _gActor510900Animation353ACRecords[1449] = {
#include "assets/actor_510900_animation_353AC_records.inc"
};

static u16 _gActor510900Animation353ACIndices[20] = {
#include "assets/actor_510900_animation_353AC_indices.inc"
};

static AnimationSet _gActor510900Animation353AC = {
    _gActor510900Animation353ACRecords,
    _gActor510900Animation353ACIndices,
    { NULL, _gActor510900Animation353ACBank1, NULL, NULL, _gActor510900Animation353ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation35474Bank1[2] = {
#include "assets/actor_510900_animation_35474_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation35474Bank4[6] = {
#include "assets/actor_510900_animation_35474_bank4.inc"
};

static AnimationRecord _gActor510900Animation35474Records[22] = {
#include "assets/actor_510900_animation_35474_records.inc"
};

static u16 _gActor510900Animation35474Indices[12] = {
#include "assets/actor_510900_animation_35474_indices.inc"
};

AnimationSet gActor510900Animation35474 = {
    _gActor510900Animation35474Records,
    _gActor510900Animation35474Indices,
    { NULL, _gActor510900Animation35474Bank1, NULL, NULL, _gActor510900Animation35474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor510900Animation35B20Bank1[20] = {
#include "assets/actor_510900_animation_35B20_bank1.inc"
};

static AnimationPackedRotation _gActor510900Animation35B20Bank4[143] = {
#include "assets/actor_510900_animation_35B20_bank4.inc"
};

static AnimationRecord _gActor510900Animation35B20Records[208] = {
#include "assets/actor_510900_animation_35B20_records.inc"
};

static u16 _gActor510900Animation35B20Indices[12] = {
#include "assets/actor_510900_animation_35B20_indices.inc"
};

AnimationSet gActor510900Animation35B20 = {
    _gActor510900Animation35B20Records,
    _gActor510900Animation35B20Indices,
    { NULL, _gActor510900Animation35B20Bank1, NULL, NULL, _gActor510900Animation35B20Bank4, NULL, NULL, NULL },
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
    actorRenderComposeCoord(coord);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & 0xF0) + mem->scale, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & 0xF0) + ({ mem->scale + 0x10000; }),
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
                eff             = Gp_SpawnEff(EFFECT_04C, coord, ((gRandomLcgState >> 16) & 0xF0) + ({ mem->scale + 0x10000; }),
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
                eff             = Gp_SpawnEff(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & 0xF0) + mem->scale, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x180, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x10100, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x100, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
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
                eff             = Gp_SpawnEff(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x180, &mem->move);
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
                    eff             = Gp_SpawnEff(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x80, &mem->move);
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
                    eff             = Gp_SpawnEff(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
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
                    eff             = Gp_SpawnEff(EFFECT_04C, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x10100, &mem->move);
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
                    eff             = Gp_SpawnEff(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x100, &mem->move);
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
                    eff             = Gp_SpawnEff(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x10080, &mem->move);
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
                    eff             = Gp_SpawnEff(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xF0) + 0x180, &mem->move);
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
                eff                   = Gp_SpawnEff(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xF0) | 0x100, &mem->move);
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
        actorRenderComposeCoord(coord);
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
                actorRenderComposeCoord(&hit);
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
        actorRenderComposeCoord(coord);
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
        actorRenderComposeCoord(coord);
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
        actorRenderComposeCoord(coord);
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
    actorRenderComposeCoord(coord);
    eff->move.vx = -0x200;
    eff->move.vy = 0x40;
    eff->move.vz = 0;
    Gp_SpawnEff(EFFECT_IMPACT_SPARK, coord, 0x180, &eff->move);
    for (i = 0; i < 6; i++) {
        Gp_SpawnEff(EFFECT_NO9_GOLEM_DEBRIS_STREAK, coord, 0, &eff->move);
        Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 1, NULL);
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
    _Actor510900DebrisStreakScratch* block;
    EffectWork*                      eff;
    GfxCoord*                        coord;
    LINE_F2*                         prim;
    s16                              mode;
    s16                              step;
    s32                              rng;
    s16                              val;
    s16                              count;

    block = SCRATCH_STACK_RESERVE_BLOCK(_Actor510900DebrisStreakScratch);
    eff   = arg0->spawnArg2.pointer;
    mode  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (mode != ROOM_EFFECT_CONTROL_RUNNING) {
        if (mode >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(eff, arg0);
        }
        return;
    }
    actorRenderComposeCoord(coord);
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
    block->endpoints[0].vx = coord->workm.t[0];
    block->endpoints[0].vy = coord->workm.t[1];
    block->endpoints[0].vz = coord->workm.t[2];
    coord->coord.t[0]     += eff->move.vx;
    coord->coord.t[1]     += eff->move.vy;
    coord->coord.t[2]     += eff->move.vz;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    block->endpoints[1].vx = coord->workm.t[0];
    block->endpoints[1].vy = coord->workm.t[1];
    block->endpoints[1].vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->endpoints[0]);
    gte_rtps();
    gte_stsxy(&block->screenEndpoints[0]);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depths[0]);
        gte_ldv0(&block->endpoints[1]);
        gte_rtps();
        gte_stsxy(&block->screenEndpoints[1]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depths[1]);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 3);
            setcode(prim, 0x40);
            val      = 0xFF - (eff->age << (5 - eff->scale));
            prim->r0 = val;
            prim->g0 = val >> eff->angle;
            prim->b0 = val >> 3;
            prim->x0 = block->screenEndpoints[0].vx;
            prim->y0 = block->screenEndpoints[0].vy;
            prim->x1 = block->screenEndpoints[1].vx;
            prim->y1 = block->screenEndpoints[1].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->depths[0] + block->depths[1]) >> 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, (block->depths[0] + block->depths[1]) >> 1);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor510900DebrisStreakScratch);
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
            eff = Gp_SpawnEff(EFFECT_NO9_EXPLOSION_FIREBALL, coord, 0x480, NULL);
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
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x82004400, NULL);
            if (eff->age >= 0x33) {
                arg0->state++;
            }
            break;
        case 3:
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xD2004400, NULL);
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
                    spawned = Gp_SpawnEff(EFFECT_NO9_EXPLOSION_FIREBALL, coord, ((s32)((u16)eff->scale << 16) >> 17) | 0x02001000, NULL);
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
                    spawned = Gp_SpawnEff(EFFECT_NO9_EXPLOSION_FIREBALL, coord, ((s32)((u16)eff->scale << 16) >> 17) | 0x01002000, NULL);
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
/// Fireball palette for atlas frame `frame`, packed as a GPU CLUT word.
///
/// X is in VRAM words and Y in scanlines. The drawer assigns the word once per
/// emitted quad. `frame` must be a nonnegative index into the twelve-entry
/// table; the subscript is evaluated for each coordinate.
#define SPRITE_QUAD_CLUT getClut(_gActor510900FireballFramePalettes[frame].clutX, _gActor510900FireballFramePalettes[frame].clutY)
/// UV-origin table for the next included sprite-quad drawer.
///
/// Bind an array or side-effect-free pointer expression with
/// `EffectSpriteTextureFrame` elements before `sprite_quad_draw.inc.c`.
/// The drawer borrows the table for the call and reads only `u` and `v`, in
/// texels relative to `SPRITE_QUAD_TEXTURE_PAGE`; palettes come from
/// `SPRITE_QUAD_CLUT`. Each cell is square with `SPRITE_QUAD_CELL_WIDTH`
/// texels per side, and its inclusive endpoints narrow to GPU bytes.
///
/// Defining this binding replaces arithmetic UV selection. The drawer's
/// `frame` parameter indexes it directly, without wrapping or clamping, and
/// must be nonnegative and in range for both this table and any frame-indexed
/// CLUT table. The table expression is evaluated once for an emitted quad;
/// no pointer is retained. The fragment undefines the binding after inclusion.
///
/// Actor 510900 is the sole table-mode carrier: it borrows the gameplay
/// image's twelve atlas origins and selects twelve actor-specific palettes.
#define SPRITE_QUAD_UV_TABLE gEffectSpriteAtlasFrames
STATIC_ASSERT(ARRAY_SIZE(SPRITE_QUAD_UV_TABLE) == ARRAY_SIZE(_gActor510900FireballFramePalettes), sprite_quad_uv_palette_frame_counts_match);

/// Texel width and height of the shared atlas cells used with this actor's palettes.
#define SPRITE_QUAD_CELL_WIDTH EFFECT_SPRITE_ATLAS_CELL_SIZE
/// Perspective-sizing multiplier for the shared atlas sprite.
///
/// Uses the atlas's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE EFFECT_SPRITE_ATLAS_UV_SPAN
#include "../../shared/sprite_quad_draw.inc.c"

/// Spawn/setup handler. It allocates the 0x5C8-byte work block and hangs it off
/// the task, points the model object at the block's `light` and `color`
/// matrices and fills the context's coordinate, pair
/// source and HP (`field_40`, seeded from the record's `hpMax`).
///
/// The block's rig is bound with `animationInitContext` over its nineteen slots, and
/// slots 1..18 are reset. Six enemies are spawned from `D_actor_510900_80167A18`; entries 2
/// and 3 are the two whose models get the current room's texture page and CLUT
/// row (`Gp_GetNestedAreaRec`, indexed by the context id's top nibble) and whose
/// tasks are kept in `weaponTask` / `chestModelTask`. Entry 2 also gets the
/// flame-jet effect, kept in `flameJetTask` and reparented onto this task.
///
/// `body`, `weaponAttack` and `forearmAttack` are linked into the global
/// object lists with their contact tables (`Gp_InitRec18Table`); pair tests
/// are then enabled for `body` and left disabled for the two attack spheres.
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
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                   = &arg1->extra.tmd->coords[3];
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vy              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &D_actor_510900_80167980;
    arg0->recs                    = work->bodyContacts;
    arg0->hp                      = D_actor_510900_80167980.hpMax;
    work->hitEffectArg.coord      = &arg1->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x400;
    work->hitEffectArg.spawnArgHi = 2;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_510900_80167AA4, obj,
                         work->rig.poses, work->rig.slots);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    work->present = 1;
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
    entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index1);
    model1->texturePageOffset = entry1->texturePageOffset;
    model1->clutRowOffset     = entry1->clutRowOffset;
    if (model1->buffer != NULL) {
        tmdProcessStream(model1);
        tmdProcessStream(model1);
    }
    work->weaponTask = spawned->task;
    eff              = Gp_SpawnEff((EFFECT_ACTOR_510900_FLAME_JET | EFFECT_SPAWN_UNLIMITED), spawned->task->extra.tmd->coords, 0, NULL);
    if (eff != NULL) {
        work->flameJetTask = eff->task;
        taskReparent(arg1, eff->task);
    }
    if (work->flameJetTask != NULL) {
        work->flameJetTask->spawnArg1.value = ACTOR_510900_FLAME_OFF;
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
    entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
    model2->texturePageOffset = entry2->texturePageOffset;
    model2->clutRowOffset     = entry2->clutRowOffset;
    if (model2->buffer != NULL) {
        tmdProcessStream(model2);
        tmdProcessStream(model2);
    }
    work->chestModelTask = spawned->task;
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 0, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 1, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 5, 2, arg0);
    Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 6, 0, arg0);
    work->body.coord            = &arg1->extra.tmd->coords[3];
    records1                    = work->bodyContacts;
    work->body.context.contacts = records1;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3001B;
    work->body.radius           = 0x1C2;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(records1, ARRAY_SIZE(work->bodyContacts), 0);
    records2                            = work->attackContacts;
    work->body.flags                   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->weaponAttack.coord            = work->weaponTask->extra.tmd->coords;
    work->weaponAttack.context.contacts = records2;
    work->weaponAttack.pos.vx           = -0x140;
    work->weaponAttack.pos.vy           = 0x80;
    work->weaponAttack.pos.vz           = 0;
    work->weaponAttack.key              = 0;
    work->weaponAttack.radius           = 0x190;
    work->weaponAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->weaponAttack);
    Gp_InitRec18Table(records2, ARRAY_SIZE(work->attackContacts), 0);
    work->weaponAttack.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->forearmAttack.coord            = &arg1->extra.tmd->coords[7];
    work->forearmAttack.context.contacts = records2;
    work->forearmAttack.pos.vx           = 0;
    work->forearmAttack.pos.vy           = 0;
    work->forearmAttack.pos.vz           = 0;
    work->forearmAttack.key              = 0;
    work->forearmAttack.radius           = 0x190;
    work->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->forearmAttack);
    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    arg1->msgTable             = D_actor_510900_80167A6C;
    arg1->exitCallback         = func_actor_510900_8013B608;
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
    if (work->animationId == 0x20 && work->animationFrame == 0xD2) {
        work->flameMode   = ACTOR_510900_FLAME_BURNING;
        work->flameFrames = 0xFF;
        snd               = (((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
        pan               = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        work->eventFlameSound = (((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780011;
        pan2                  = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(work->eventFlameSound, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    work->animationFrame++;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    func_actor_510900_8013BC38(arg1, coord);
    if (work->flameMode != work->sentFlameMode) {
        if (work->flameJetTask != NULL) {
            work->flameJetTask->spawnArg1.value = work->flameMode;
        }
        work->sentFlameMode = work->flameMode;
    }
}
