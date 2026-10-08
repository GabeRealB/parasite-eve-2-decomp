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

/// Packed flame spawn options and the frame counts of the actor's particle tasks.
enum {
    ACTOR_510900_PARTICLE_INITIAL       = 0,
    ACTOR_510900_FLAME_SCALE_MASK       = 0xFFF,
    ACTOR_510900_FLAME_RANDOM_MOTION    = 0x10000,
    ACTOR_510900_FLAME_DEFAULT_SCALE    = 0x200,
    ACTOR_510900_FLAME_MOTION_RANGE     = 48,
    ACTOR_510900_FLAME_45_CELL_COUNT    = 6,
    ACTOR_510900_FLAME_45_LAST_AGE_MASK = 0xF,
    ACTOR_510900_FLAME_FADE_FRAMES      = 8,
    ACTOR_510900_FLAME_FADE_STEP        = 16,
    ACTOR_510900_FLAME_FULL_BRIGHTNESS  = 0x80,
    ACTOR_510900_FLAME_4C_FRAMES        = 8,
    ACTOR_510900_FLAME_52_FRAMES        = 12,
    ACTOR_510900_FLAME_59_FRAMES        = 8,
    ACTOR_510900_DEBRIS_SHORT_TIER      = 1,
    ACTOR_510900_DEBRIS_LONG_TIER       = 2,
    ACTOR_510900_DEBRIS_GRAVITY         = 6,
    ACTOR_510900_DEBRIS_FRAMES_PER_TIER = 16,
};

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

/// Chooses a flame-particle velocity and advances the draw used for its size.
///
/// Standalone block; effect must be a stable pointer lvalue and the numeric
/// arguments side-effect-free: horizontalRange is positive, verticalStep fits
/// s16. Velocity is in parent-coordinate units, X lies in [-range-127,-128].
/// Captures/updates the resident LCG state twice and retains no pointer.
#define ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, horizontalRange, verticalStep) \
    {                                                                                           \
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;     \
        (effect)->move.vx = -((gRandomLcgState >> 16) % (horizontalRange)) - 0x80;              \
        (effect)->move.vy = (verticalStep);                                                     \
        (effect)->move.vz = 0;                                                                  \
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;     \
    }

void actor510900FlameJetTask(Task* task)
{
    enum {
        ACTOR_510900_FLAME_JET_LIGHT_SLOT            = 2,
        ACTOR_510900_FLAME_JET_ACTIVE                = 1,
        ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK      = 0xF0,
        ACTOR_510900_FLAME_JET_LIGHT_RED             = ONE,
        ACTOR_510900_FLAME_JET_LIGHT_GREEN           = ONE / 2,
        ACTOR_510900_FLAME_JET_LIGHT_BLUE            = ONE / 4,
        ACTOR_510900_FLAME_JET_LIGHT_CONTRACTION     = 400,
        ACTOR_510900_FLAME_JET_LIGHT_FRAMES          = 16,
        ACTOR_510900_FLAME_JET_INNER_RADIUS          = 8000,
        ACTOR_510900_FLAME_JET_OUTER_RADIUS          = 10000,
        ACTOR_510900_FLAME_JET_FULL_SCALE            = 256,
        ACTOR_510900_FLAME_JET_SCALE_STEP            = 16,
        ACTOR_510900_FLAME_JET_DYING_FLAME_FRAMES    = 30,
        ACTOR_510900_FLAME_JET_DYING_PARTICLE_FRAMES = 60,
    };

    EffectWork*                    effect;
    GfxCoord*                      coord;
    WorldCoordPointLight*          pointLight;
    WorldCoordTransientPointLight* lightSlot;
    EffectWork*                    particle;
    s32                            particleIndex;
    s32                            randomBits;
    s32                            localZ;

    effect     = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    lightSlot  = &gWorldCoordTransientPointLights[ACTOR_510900_FLAME_JET_LIGHT_SLOT];
    pointLight = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
        }
        if (task->spawnArg1.value == ACTOR_510900_FLAME_RELEASED) {
            effectKillTask(effect, task);
        }
        return;
    }
    if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
        coord->parent = effect->parent;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[0]   = effect->pos.vx;
        coord->coord.t[1]   = effect->pos.vy;
        localZ              = effect->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = localZ;
        task->state         = ACTOR_510900_FLAME_JET_ACTIVE;
    }
    actorRenderComposeCoord(coord);
    // The emitter also counts down slot 2 and contracts its full-strength radius.
    if (lightSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
        pointLight->head.color.r = ACTOR_510900_FLAME_JET_LIGHT_RED;
        pointLight->head.color.g = ACTOR_510900_FLAME_JET_LIGHT_GREEN;
        pointLight->head.color.b = ACTOR_510900_FLAME_JET_LIGHT_BLUE;
        if (pointLight->inner > ACTOR_510900_FLAME_JET_LIGHT_CONTRACTION) {
            pointLight->inner -= ACTOR_510900_FLAME_JET_LIGHT_CONTRACTION;
        }
        lightSlot->framesLeft--;
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
        lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
        if (lightSlot->framesLeft == WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            task->spawnArg1.value = ACTOR_510900_FLAME_OFF;
            effect->age           = 0;
            effect->scale         = 0;
        }
    }
    // The actor supplies intensity; each adopted particle follows this task's lifetime.
    switch (task->spawnArg1.value) {
        case ACTOR_510900_FLAME_OFF:
            break;
        case ACTOR_510900_FLAME_BURNING:
            effect->scale = (effect->scale < ACTOR_510900_FLAME_JET_FULL_SCALE) ? effect->scale + ACTOR_510900_FLAME_JET_SCALE_STEP : ACTOR_510900_FLAME_JET_FULL_SCALE;
            for (particleIndex = 0; particleIndex < 2; particleIndex++) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x2C0, 0x40);
                particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + effect->scale, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            lightSlot->framesLeft = ACTOR_510900_FLAME_JET_LIGHT_FRAMES;
            pointLight->inner     = ACTOR_510900_FLAME_JET_INNER_RADIUS;
            pointLight->outer     = ACTOR_510900_FLAME_JET_OUTER_RADIUS;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits            = gRandomLcgState >> 16;
            // Keep scale plus the motion flag grouped before adding random size.
            if (!(randomBits & 3)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x40);
                particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + ({ effect->scale + ACTOR_510900_FLAME_RANDOM_MOTION; }),
                                       &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                particle = effectSpawn(EFFECT_04C, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + ({ effect->scale + ACTOR_510900_FLAME_RANDOM_MOTION; }),
                                       &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + effect->scale, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (randomBits % 3 == 0) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + (ACTOR_510900_FLAME_RANDOM_MOTION | 0x80), &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, -0x80);
                particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + 0x180, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            break;
        case ACTOR_510900_FLAME_BLAST:
            lightSlot->framesLeft = ACTOR_510900_FLAME_JET_LIGHT_FRAMES;
            pointLight->inner     = ACTOR_510900_FLAME_JET_INNER_RADIUS;
            pointLight->outer     = ACTOR_510900_FLAME_JET_OUTER_RADIUS;
            for (particleIndex = 0; particleIndex < 3; particleIndex++) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x40);
                particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) | (ACTOR_510900_FLAME_RANDOM_MOTION | 0x100), &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            if (!(randomBits & 7)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) | 0x100, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 3)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + (ACTOR_510900_FLAME_RANDOM_MOTION | 0x80), &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            randomBits >>= 1;
            if (!(randomBits & 7)) {
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, -0x80);
                particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + 0x180, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            break;
        case ACTOR_510900_FLAME_DYING:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            effect->age++;
            if (effect->age < ACTOR_510900_FLAME_JET_DYING_FLAME_FRAMES) {
                if (!(randomBits & 7)) {
                    ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x2C0, 0x40);
                    particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + 0x80, &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                randomBits >>= 1;
                if (!(randomBits & 3)) {
                    ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x40);
                    particle = effectSpawn(EFFECT_NO9_GOLEM_FLAME, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + (ACTOR_510900_FLAME_RANDOM_MOTION | 0x80), &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                for (particleIndex = 0; particleIndex < 2; particleIndex++) {
                    ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                    particle = effectSpawn(EFFECT_04C, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) | (ACTOR_510900_FLAME_RANDOM_MOTION | 0x100), &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                randomBits >>= 1;
                if (!(randomBits & 7)) {
                    ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0);
                    particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) | 0x100, &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                randomBits >>= 1;
                if (!(randomBits & 7)) {
                    ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                    particle = effectSpawn(EFFECT_NO9_FLAME_SPRITE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + (ACTOR_510900_FLAME_RANDOM_MOTION | 0x80), &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                for (particleIndex = 0; particleIndex < 2; particleIndex++) {
                    ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, -0x80);
                    particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) + 0x180, &effect->move);
                    if (particle != NULL) {
                        taskReparent(task, particle->task);
                    }
                }
                lightSlot->framesLeft = ACTOR_510900_FLAME_JET_LIGHT_FRAMES;
                pointLight->inner     = ACTOR_510900_FLAME_JET_INNER_RADIUS;
                pointLight->outer     = ACTOR_510900_FLAME_JET_OUTER_RADIUS;
            } else if (effect->age < ACTOR_510900_FLAME_JET_DYING_PARTICLE_FRAMES) {
                lightSlot->framesLeft = 2;
                pointLight->inner     = ACTOR_510900_FLAME_JET_LIGHT_CONTRACTION;
                pointLight->outer     = ACTOR_510900_FLAME_JET_LIGHT_CONTRACTION;
                ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY(effect, 0x280, 0x80);
                particle = effectSpawn(EFFECT_NO9_GUNFIRE_PARTICLE, coord, ((gRandomLcgState >> 16) & ACTOR_510900_FLAME_JET_RANDOM_SIZE_MASK) | 0x100, &effect->move);
                if (particle != NULL) {
                    taskReparent(task, particle->task);
                }
            }
            break;
        case ACTOR_510900_FLAME_RELEASED:
            effectKillTask(effect, task);
            break;
    }
}

#undef ACTOR_510900_PREPARE_FLAME_JET_PARTICLE_VELOCITY

void actor510900FlameSpriteTask45(Task* task)
{
    GfxCoord            groundCoord;
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratchTop;
    EffectShapeScratch* projectionScratch;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 screenEdge;
    s16                 riseSpeed;
    u8                  glowBrightness;
    u16                 worldZ;
    s32                 framesLeft;
    s32                 fadeBrightness;

    /// Draws the flame's ground glow in its coordinate's parent frame.
    ///
    /// Arguments must be side-effect-free pointer expressions, a coordinate
    /// lvalue and a brightness value: coordinate arguments are used repeatedly.
    /// Expands as a compound statement; use within braced control flow.
    /// The glow consumes only the composed translation of groundCoord.
#define ACTOR_510900_DRAW_FLAME_GROUND_GLOW(flameCoord, groundCoord, flameEffect, brightness) \
    {                                                                                         \
        (groundCoord).parent       = (flameCoord)->parent;                                    \
        (groundCoord).coord.t[0]   = (flameCoord)->coord.t[0];                                \
        (groundCoord).coord.t[1]   = 0;                                                       \
        (groundCoord).coord.t[2]   = (flameCoord)->coord.t[2];                                \
        (groundCoord).composeStamp = GRAPHICS_COORD_DIRTY;                                    \
        actorRenderComposeCoord(&(groundCoord));                                              \
        effectDrawGroundGlow(&(groundCoord), (flameEffect)->scale >> 1, (brightness));        \
    }

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratchTop                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        projectionScratch                        = scratchTop - 1;
        projectionScratch->worldPoint.vx         = (u16)coord->workm.t[0];
        scratch                                  = projectionScratch;
        scratch->worldPoint.vy                   = (u16)coord->workm.t[1];
        worldZ                                   = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
        scratch->worldPoint.vz                   = worldZ;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(scratchTop - 1)->screenX);
        gte_stflg(&(scratchTop - 1)->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&(scratchTop - 1)->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
                effect->scale   = (u16)task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effect->angle   = (gRandomLcgState >> 16) & ACTOR_510900_FLAME_45_LAST_AGE_MASK;
                if (task->spawnArg1.value & ACTOR_510900_FLAME_RANDOM_MOTION) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effect->period  = (gRandomLcgState >> 16) % ACTOR_510900_FLAME_MOTION_RANGE;
                }
                task->state++;
            }
            // Raw texture colour gives way to modulation for the last eight frames.
            if (effect->angle - ACTOR_510900_FLAME_FADE_FRAMES < effect->age) {
                framesLeft = effect->angle - effect->age + 1;
                // Fitted: the image masks the glow argument with 0xFF, so the
                // product reached glowBrightness through a join of two
                // assignments, and whatever chose between them left no
                // instruction. What was tested, and whether the two sides
                // differed in the source, is unknown; this test is a stand-in.
                if (framesLeft > 0) {
                    fadeBrightness = framesLeft * ACTOR_510900_FLAME_FADE_STEP;
                } else {
                    fadeBrightness = framesLeft * ACTOR_510900_FLAME_FADE_STEP;
                }
                glowBrightness = fadeBrightness;
                sprite->r0     = fadeBrightness;
                sprite->g0     = fadeBrightness;
                sprite->b0     = fadeBrightness;
            } else {
                glowBrightness = ACTOR_510900_FLAME_FULL_BRIGHTNESS;
                setShadeTex(sprite, 1);
            }
            sprite->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            sprite->clut  = getClut(0, 270);
            setSemiTrans(sprite, 1);
            sprite->u0               = (effect->age % ACTOR_510900_FLAME_45_CELL_COUNT) * 32;
            sprite->v0               = 0;
            sprite->u1               = (effect->age % ACTOR_510900_FLAME_45_CELL_COUNT) * 32 + 0x1F;
            sprite->v1               = 0;
            sprite->u2               = (effect->age % ACTOR_510900_FLAME_45_CELL_COUNT) * 32;
            sprite->v2               = 0x27;
            sprite->u3               = (effect->age % ACTOR_510900_FLAME_45_CELL_COUNT) * 32 + 0x1F;
            sprite->v3               = 0x27;
            scratch->extent.corner.x = (effect->scale * 31) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * 39) / scratch->depth;
            screenEdge               = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2               = screenEdge;
            sprite->x0               = screenEdge;
            screenEdge               = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3               = screenEdge;
            sprite->x1               = screenEdge;
            screenEdge               = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1               = screenEdge;
            sprite->y0               = screenEdge;
            screenEdge               = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3               = screenEdge;
            sprite->y2               = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    sprite);
            // Every other frame, put the glow on Y=0 in the same parent frame.
            if (coord->coord.t[1] < 0 && (effect->age & 1)) {
                ACTOR_510900_DRAW_FLAME_GROUND_GLOW(coord, groundCoord, effect, glowBrightness);
#undef ACTOR_510900_DRAW_FLAME_GROUND_GLOW
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        riseSpeed = effect->period;
        if (riseSpeed != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= riseSpeed;
        }
        effect->age++;
        if (effect->angle >= effect->age) {
            return;
        }
    }
    effectKillTask(effect, task);
}

void actor510900FlameSpriteTask4C(Task* task)
{
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 screenEdge;
    s16                 riseSpeed;
    s32                 scale;
    s32                 frameAge;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
                if (task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK) {
                    scale = (u16)task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK;
                } else {
                    scale = ACTOR_510900_FLAME_DEFAULT_SCALE;
                }
                effect->scale = scale;
                if (task->spawnArg1.value & ACTOR_510900_FLAME_RANDOM_MOTION) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effect->period  = (gRandomLcgState >> 16) % ACTOR_510900_FLAME_MOTION_RANGE;
                }
                task->state++;
            }
            sprite->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            setShadeTex(sprite, 1);
            setSemiTrans(sprite, 1);
            frameAge                 = effect->age;
            sprite->clut             = (frameAge & 0x3F) | getClut(0, 271);
            frameAge                 = effect->age;
            sprite->v0               = 0x70;
            sprite->u0               = frameAge * 32;
            frameAge                 = effect->age;
            sprite->v1               = 0x70;
            sprite->u1               = frameAge * 32 + 0x1F;
            frameAge                 = effect->age;
            sprite->v2               = 0x9F;
            sprite->u2               = frameAge * 32;
            frameAge                 = effect->age;
            sprite->v3               = 0x9F;
            sprite->u3               = frameAge * 32 + 0x1F;
            scratch->extent.corner.x = (effect->scale * 31) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * 47) / scratch->depth;
            screenEdge               = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2               = screenEdge;
            sprite->x0               = screenEdge;
            screenEdge               = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3               = screenEdge;
            sprite->x1               = screenEdge;
            screenEdge               = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1               = screenEdge;
            sprite->y0               = screenEdge;
            screenEdge               = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3               = screenEdge;
            sprite->y2               = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        riseSpeed = effect->period;
        if (riseSpeed != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= riseSpeed;
        }
        effect->age++;
        if (effect->age < ACTOR_510900_FLAME_4C_FRAMES) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(effect, task);
}

void actor510900FlameSpriteTask52(Task* task)
{
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 frameAge;
    s16                 screenEdge;
    s32                 scale;
    s32                 paletteAge;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
                if (task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK) {
                    scale = (u16)task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK;
                } else {
                    scale = ACTOR_510900_FLAME_DEFAULT_SCALE;
                }
                effect->scale = scale;
                if (effect->period = (u16)((u32)task->spawnArg1.value >> 16) & (ACTOR_510900_FLAME_RANDOM_MOTION >> 16)) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effect->period  = (gRandomLcgState >> 16) % ACTOR_510900_FLAME_MOTION_RANGE;
                }
                task->state++;
            }
            sprite->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            setShadeTex(sprite, 1);
            setSemiTrans(sprite, 1);
            paletteAge               = effect->age;
            sprite->clut             = ((paletteAge + 8) & 0x3F) | getClut(0, 271);
            frameAge                 = effect->age;
            sprite->u0               = (s16)(frameAge % 8) * 32;
            frameAge                 = effect->age;
            sprite->v0               = (frameAge / 8) * 48 - 0x60;
            frameAge                 = effect->age;
            sprite->u1               = (s16)(frameAge % 8) * 32 + 0x1F;
            frameAge                 = effect->age;
            sprite->v1               = (frameAge / 8) * 48 - 0x60;
            frameAge                 = effect->age;
            sprite->u2               = (s16)(frameAge % 8) * 32;
            frameAge                 = effect->age;
            sprite->v2               = (frameAge / 8) * 48 - 0x31;
            frameAge                 = effect->age;
            sprite->u3               = (s16)(frameAge % 8) * 32 + 0x1F;
            frameAge                 = effect->age;
            sprite->v3               = (frameAge / 8) * 48 - 0x31;
            scratch->extent.corner.x = (effect->scale * 31) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * 47) / scratch->depth;
            // The target shift uses period's low five bits; nonzero period also enables falling.
            scratch->extent.corner.x >>= effect->period;
            screenEdge                 = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2                 = screenEdge;
            sprite->x0                 = screenEdge;
            screenEdge                 = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3                 = screenEdge;
            sprite->x1                 = screenEdge;
            screenEdge                 = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1                 = screenEdge;
            sprite->y0                 = screenEdge;
            screenEdge                 = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3                 = screenEdge;
            sprite->y2                 = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (effect->period != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += 0x38;
        }
        effect->age++;
        if (effect->age < ACTOR_510900_FLAME_52_FRAMES) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(effect, task);
}

void actor510900FlameSpriteTask59(Task* task)
{
    EffectWork*         effect;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           sprite;
    s16                 effectControl;
    s16                 frameAge;
    s16                 screenEdge;
    s16                 riseSpeed;
    s32                 scale;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        // Project the world centre before initializing or drawing the sprite.
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            sprite         = gGpuPrimCursor;
            gGpuPrimCursor = sprite + 1;
            setPolyFT4(sprite);
            if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
                if (task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK) {
                    scale = (u16)task->spawnArg1.value & ACTOR_510900_FLAME_SCALE_MASK;
                } else {
                    scale = ACTOR_510900_FLAME_DEFAULT_SCALE;
                }
                effect->scale   = scale;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effect->period  = (gRandomLcgState >> 16) % ACTOR_510900_FLAME_MOTION_RANGE;
                task->state++;
            }
            sprite->tpage = getTPage(0, GPU_BLEND_SUBTRACT, 704, 0);
            setShadeTex(sprite, 1);
            setSemiTrans(sprite, 1);
            sprite->clut             = getClut(32, 270);
            frameAge                 = effect->age;
            sprite->v0               = 0xD0;
            sprite->u0               = (frameAge / 2 + 4) * 32;
            frameAge                 = effect->age;
            sprite->v1               = 0xD0;
            sprite->u1               = (frameAge / 2 + 4) * 32 + 0x1F;
            frameAge                 = effect->age;
            sprite->v2               = 0xEF;
            sprite->u2               = (frameAge / 2 + 4) * 32;
            frameAge                 = effect->age;
            sprite->v3               = 0xEF;
            sprite->u3               = (frameAge / 2 + 4) * 32 + 0x1F;
            scratch->extent.corner.x = (effect->scale * 31) / scratch->depth;
            scratch->extent.corner.y = (effect->scale * 31) / scratch->depth;
            screenEdge               = scratch->screenX - (u16)scratch->extent.corner.x;
            sprite->x2               = screenEdge;
            sprite->x0               = screenEdge;
            screenEdge               = scratch->screenX + (u16)scratch->extent.corner.x;
            sprite->x3               = screenEdge;
            sprite->x1               = screenEdge;
            screenEdge               = scratch->screenY - (u16)scratch->extent.corner.y;
            sprite->y1               = screenEdge;
            sprite->y0               = screenEdge;
            screenEdge               = scratch->screenY + (u16)scratch->extent.corner.y;
            sprite->y3               = screenEdge;
            sprite->y2               = screenEdge;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        // Paused effects remain visible; only running effects move and age.
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        riseSpeed = effect->period;
        if (riseSpeed != 0) {
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  -= riseSpeed;
        }
        effect->age++;
        if (effect->age < ACTOR_510900_FLAME_59_FRAMES) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(effect, task);
}

void actor510900MuzzleFlashTask44(Task* task)
{
    enum {
        ACTOR_510900_MUZZLE_LIGHT_SLOT         = 3,
        ACTOR_510900_MUZZLE_LIGHT_RED          = 3 * ONE / 4,
        ACTOR_510900_MUZZLE_LIGHT_GREEN        = ONE / 2,
        ACTOR_510900_MUZZLE_LIGHT_BLUE         = ONE / 4,
        ACTOR_510900_MUZZLE_PARTICLE_PAIRS     = 6,
        ACTOR_510900_MUZZLE_LIGHT_FRAMES       = 4,
        ACTOR_510900_MUZZLE_LIGHT_INNER_RADIUS = 4000,
        ACTOR_510900_MUZZLE_LIGHT_OUTER_RADIUS = 4800,
    };

    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    effect;
    GfxCoord*                      coord;
    s32                            particleIndex;

    lightSlot  = &gWorldCoordTransientPointLights[ACTOR_510900_MUZZLE_LIGHT_SLOT];
    lightCoord = &lightSlot->light.head.transform.coord;
    effect     = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    pointLight = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        effectKillTask(effect, task);
        return;
    }
    coord->parent = effect->parent;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]   = effect->pos.vx;
    coord->coord.t[1]   = effect->pos.vy;
    coord->coord.t[2]   = effect->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    // Finish placement before spawning the burst and placing its view-relative light.
    actorRenderComposeCoord(coord);
    effect->move.vx = -0x200;
    effect->move.vy = 0x40;
    effect->move.vz = 0;
    effectSpawn(EFFECT_IMPACT_SPARK, coord, 0x180, &effect->move);
    for (particleIndex = 0; particleIndex < 6; particleIndex++) {
        effectSpawn(EFFECT_NO9_GOLEM_DEBRIS_STREAK, coord, 0, &effect->move);
        effectSpawn(EFFECT_PIXEL_SPARK, coord, 1, NULL);
    }
    lightSlot->framesLeft    = ACTOR_510900_MUZZLE_LIGHT_FRAMES;
    pointLight->inner        = ACTOR_510900_MUZZLE_LIGHT_INNER_RADIUS;
    pointLight->outer        = ACTOR_510900_MUZZLE_LIGHT_OUTER_RADIUS;
    pointLight->head.color.r = ACTOR_510900_MUZZLE_LIGHT_RED;
    pointLight->head.color.g = ACTOR_510900_MUZZLE_LIGHT_GREEN;
    pointLight->head.color.b = ACTOR_510900_MUZZLE_LIGHT_BLUE;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightCoord->coord);
    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    effectKillTask(effect, task);
}

void actor510900DebrisStreakTask(Task* task)
{
    _Actor510900DebrisStreakScratch* scratch;
    EffectWork*                      effect;
    GfxCoord*                        coord;
    LINE_F2*                         line;
    s16                              effectControl;
    s16                              lifetimeTier;
    s32                              randomState;
    s16                              brightness;
    s16                              nextAge;

    // The reservation precedes the control check, including early-return paths.
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_Actor510900DebrisStreakScratch);
    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effect, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effect->move.vx = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effect->move.vy = -((gRandomLcgState >> 16) & 0x3F) - 0x10;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effect->move.vz = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        lifetimeTier    = ACTOR_510900_DEBRIS_LONG_TIER;
        if (((gRandomLcgState >> 16) & 3) != 0) {
            lifetimeTier = ACTOR_510900_DEBRIS_SHORT_TIER;
        }
        randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effect->scale   = lifetimeTier;
        effect->angle   = (((u32)randomState >> 16) & 1) + 1;
        gRandomLcgState = randomState;
        task->state++;
    }
    // Project the segment traced by this frame's local-coordinate drift.
    scratch->endpoints[0].vx = coord->workm.t[0];
    scratch->endpoints[0].vy = coord->workm.t[1];
    scratch->endpoints[0].vz = coord->workm.t[2];
    coord->coord.t[0]       += effect->move.vx;
    coord->coord.t[1]       += effect->move.vy;
    coord->coord.t[2]       += effect->move.vz;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    scratch->endpoints[1].vx = coord->workm.t[0];
    scratch->endpoints[1].vy = coord->workm.t[1];
    scratch->endpoints[1].vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->endpoints[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenEndpoints[0]);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depths[0]);
        gte_ldv0(&scratch->endpoints[1]);
        gte_rtps();
        gte_stsxy(&scratch->screenEndpoints[1]);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depths[1]);
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineF2(line);
            brightness = 0xFF - (effect->age << (5 - effect->scale));
            line->r0   = brightness;
            line->g0   = brightness >> effect->angle;
            line->b0   = brightness >> 3;
            line->x0   = scratch->screenEndpoints[0].vx;
            line->y0   = scratch->screenEndpoints[0].vy;
            line->x1   = scratch->screenEndpoints[1].vx;
            line->y1   = scratch->screenEndpoints[1].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((scratch->depths[0] + scratch->depths[1]) >> 1) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, (scratch->depths[0] + scratch->depths[1]) >> 1);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor510900DebrisStreakScratch);
    effect->move.vy += ACTOR_510900_DEBRIS_GRAVITY;
    nextAge          = effect->age + 1;
    effect->age      = nextAge;
    if (nextAge > effect->scale * ACTOR_510900_DEBRIS_FRAMES_PER_TIER - 1) {
        effectKillTask(effect, task);
    }
}

void actor510900ExplosionTask185(Task* task)
{
    enum {
        ACTOR_510900_EXPLOSION_FIREBALL           = 0,
        ACTOR_510900_EXPLOSION_WAIT_SMOKE         = 1,
        ACTOR_510900_EXPLOSION_SMOKE              = 2,
        ACTOR_510900_EXPLOSION_TAIL_SMOKE         = 3,
        ACTOR_510900_EXPLOSION_FINISHED           = 4,
        ACTOR_510900_EXPLOSION_SMOKE_START        = 9,
        ACTOR_510900_EXPLOSION_TAIL_START         = 51,
        ACTOR_510900_EXPLOSION_END                = 61,
        ACTOR_510900_EXPLOSION_MAIN_SMOKE_OPTIONS = 0x82004400,
        ACTOR_510900_EXPLOSION_TAIL_SMOKE_OPTIONS = 0xD2004400,
    };

    EffectWork* effect;
    GfxCoord*   coord;
    s16         effectControl;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN || task->state == ACTOR_510900_EXPLOSION_FINISHED) {
            effectKillTask(effect, task);
        }
        return;
    }
    effect->age++;
    switch (task->state) {
        case ACTOR_510900_EXPLOSION_FIREBALL:
            // Adopt the fireball; later calls reload the controller work.
            effect = effectSpawn(EFFECT_NO9_EXPLOSION_FIREBALL, coord, 0x480, NULL);
            if (effect != NULL) {
                taskReparent(task, effect->task);
            }
            task->state++;
            break;
        case ACTOR_510900_EXPLOSION_WAIT_SMOKE:
            if (effect->age >= ACTOR_510900_EXPLOSION_SMOKE_START) {
                task->state++;
            }
            break;
        case ACTOR_510900_EXPLOSION_SMOKE:
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_510900_EXPLOSION_MAIN_SMOKE_OPTIONS, NULL);
            if (effect->age >= ACTOR_510900_EXPLOSION_TAIL_START) {
                task->state++;
            }
            break;
        case ACTOR_510900_EXPLOSION_TAIL_SMOKE:
            effectSpawn(EFFECT_SMOKE_PUFF, coord, ACTOR_510900_EXPLOSION_TAIL_SMOKE_OPTIONS, NULL);
            if (effect->age >= ACTOR_510900_EXPLOSION_END) {
                task->state++;
            }
            break;
        case ACTOR_510900_EXPLOSION_FINISHED:
            effectKillTask(effect, task);
            break;
    }
}

/// Builds the fireball's random drift, scaled by size and motion in its parent basis.
///
/// Standalone block; effect must be a stable pointer lvalue. Captures the LCG
/// state, GTE and the enclosing task's two FIREBALL_*SHIFT/FRACTION_BITS
/// constants. The borrowed parent matrix must remain live; each GTE scale
/// saturates to signed halfwords before the parent rotation.
#define ACTOR_510900_MAKE_FIREBALL_DRIFT(effect)                                            \
    {                                                                                       \
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
        (effect)->move.vx = 0x10 - ((gRandomLcgState >> 16) & 0x1F);                        \
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
        (effect)->move.vy = 0x10 - ((gRandomLcgState >> 16) & 0x1F);                        \
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
        (effect)->move.vz = 0x10 - ((gRandomLcgState >> 16) & 0x1F);                        \
        gte_lddp((effect)->scale << ACTOR_510900_FIREBALL_DRIFT_SCALE_SHIFT);               \
        gte_ldsv(&(effect)->move);                                                          \
        gte_gpf12();                                                                        \
        gte_stsv(&(effect)->move);                                                          \
        gte_lddp((effect)->index << ACTOR_510900_FIREBALL_ROTATION_FRACTION_BITS);          \
        gte_ldsv(&(effect)->move);                                                          \
        gte_gpf12();                                                                        \
        gte_stsv(&(effect)->move);                                                          \
        gte_SetRotMatrix(&(effect)->parent->coord);                                         \
        gte_ldv0(&(effect)->move);                                                          \
        gte_rtv0();                                                                         \
        gte_stsv(&(effect)->move);                                                          \
    }

void actor510900ExplosionFireballTask184(Task* task)
{
    enum {
        ACTOR_510900_FIREBALL_DEFAULT_SCALE          = 768,
        ACTOR_510900_FIREBALL_SIZE_MASK              = 0xFFF,
        ACTOR_510900_FIREBALL_ACTIVE                 = 1,
        ACTOR_510900_FIREBALL_DEFAULT_PERIOD         = 2,
        ACTOR_510900_FIREBALL_PERIOD_MASK            = 0xF000,
        ACTOR_510900_FIREBALL_PERIOD_SHIFT           = 12,
        ACTOR_510900_FIREBALL_MAX_PERIOD             = 15,
        ACTOR_510900_FIREBALL_MOTION_MASK            = 15,
        ACTOR_510900_FIREBALL_SUPPRESS_CHILDREN      = 0xF0000000,
        ACTOR_510900_FIREBALL_FAST_CHILD_OPTIONS     = (2 << 24) | (1 << 12),
        ACTOR_510900_FIREBALL_SLOW_CHILD_OPTIONS     = (1 << 24) | (2 << 12),
        ACTOR_510900_FIREBALL_ROTATION_FRACTION_BITS = 12,
        ACTOR_510900_FIREBALL_DRIFT_SCALE_SHIFT      = 3,
    };

    EffectWork* effect;
    EffectWork* childEffect;
    GfxCoord*   coord;
    s16         effectControl;
    s16         scale;
    s16         framePeriod;
    s32         optionsByte;
    s32         childIndex;
    s32         childCount;

    effect        = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(effect, task);
        }
        return;
    }
    effect->age++;
    if (task->state == ACTOR_510900_PARTICLE_INITIAL) {
        scale = ACTOR_510900_FIREBALL_DEFAULT_SCALE;
        if (task->spawnArg1.value & ACTOR_510900_FIREBALL_SIZE_MASK) {
            scale = task->spawnArg1.halves.low & ACTOR_510900_FIREBALL_SIZE_MASK;
        }
        effect->scale   = scale;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effect->angle   = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
        if (task->spawnArg1.value & ACTOR_510900_FIREBALL_PERIOD_MASK) {
            framePeriod = (task->spawnArg1.value >> ACTOR_510900_FIREBALL_PERIOD_SHIFT) & ACTOR_510900_FIREBALL_MAX_PERIOD;
        } else {
            framePeriod = ACTOR_510900_FIREBALL_DEFAULT_PERIOD;
        }
        effect->period = framePeriod;
        effect->step   = (s32)((u16)effect->scale << 16) >> 23;
        optionsByte    = task->spawnArg1.signedBytes[3];
        effect->index  = optionsByte & ACTOR_510900_FIREBALL_MOTION_MASK;
        // Nonzero motion selects drift in the borrowed parent's local basis.
        if (effect->index != 0) {
            ACTOR_510900_MAKE_FIREBALL_DRIFT(effect);
        } else if (!(task->spawnArg1.value & ACTOR_510900_FIREBALL_SUPPRESS_CHILDREN)) {
            childCount = gDisplayState.animFrame & 3;
            childIndex = 0;
            if (childCount != 0) {
                do {
                    childEffect = effectSpawn(EFFECT_NO9_EXPLOSION_FIREBALL, coord, ((s32)((u16)effect->scale << 16) >> 17) | ACTOR_510900_FIREBALL_FAST_CHILD_OPTIONS, NULL);
                    if (childEffect != NULL) {
                        taskReparent(task, childEffect->task);
                    }
                    childIndex += 1;
                } while (childIndex < childCount);
            }
            childCount = gDisplayState.animFrame & 1;
            childIndex = 0;
            if (childIndex < childCount) {
                do {
                    childEffect = effectSpawn(EFFECT_NO9_EXPLOSION_FIREBALL, coord, ((s32)((u16)effect->scale << 16) >> 17) | ACTOR_510900_FIREBALL_SLOW_CHILD_OPTIONS, NULL);
                    if (childEffect != NULL) {
                        taskReparent(task, childEffect->task);
                    }
                    childIndex += 1;
                } while (childIndex < childCount);
            }
        }
        effect->age--;
        task->state = ACTOR_510900_FIREBALL_ACTIVE;
    }
    // Draw before advancing translation and the signed-halfword size.
    spriteQuadDraw(coord, effect->age / effect->period, effect->scale, effect->angle);
    coord->coord.t[0]  += effect->move.vx;
    coord->coord.t[1]  += effect->move.vy;
    coord->coord.t[2]  += effect->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    effect->scale      += effect->step;
    if (effect->age > effect->period * (ARRAY_SIZE(gEffectSpriteAtlasFrames) - 1) - 1) {
        effectKillTask(effect, task);
    }
}

#undef ACTOR_510900_MAKE_FIREBALL_DRIFT

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

void actor510900InitBody(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_510900_BODY_CHEST_PART          = 3,
        ACTOR_510900_BODY_FOREARM_PART        = 7,
        ACTOR_510900_PROP_DESCRIPTOR          = 1,
        ACTOR_510900_WEAPON_DESCRIPTOR        = 2,
        ACTOR_510900_CHEST_DESCRIPTOR         = 3,
        ACTOR_510900_LIGHT_DESCRIPTOR         = 5,
        ACTOR_510900_BLAST_SOURCE_DESCRIPTOR  = 6,
        ACTOR_510900_BODY_INITIAL_ANIMATION   = 1,
        ACTOR_510900_BODY_TASK_RUNNING        = 1,
        ACTOR_510900_HIT_EFFECT_ARGUMENT_LOW  = 1024,
        ACTOR_510900_HIT_EFFECT_ARGUMENT_HIGH = 2,
        ACTOR_510900_BODY_CONTACT_KEY         = WORLD_COLLISION_CONTACT_ENEMY_BODY | 27,
        ACTOR_510900_BODY_RADIUS              = 450,
        ACTOR_510900_WEAPON_ATTACK_OFFSET_X   = -320,
        ACTOR_510900_WEAPON_ATTACK_OFFSET_Y   = 128,
        ACTOR_510900_ATTACK_RADIUS            = 400,
    };
    TmdObject*             bodyModel;
    GfxCoord*              rootCoord;
    Actor510900Work*       work;
    Enemy*                 childEnemy;
    EffectWork*            flameEffect;
    WorldCollisionContact* bodyContacts;
    WorldCollisionContact* attackContacts;
    s32                    slotIndex;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    work      = memCalloc(sizeof(Actor510900Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    bodyModel->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    bodyModel->lightMtx     = &work->light;
    bodyModel->colorMtx     = &work->color;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[ACTOR_510900_BODY_CHEST_PART];
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &D_actor_510900_80167980;
    enemy->recs                   = work->bodyContacts;
    enemy->hp                     = D_actor_510900_80167980.hpMax;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[ACTOR_510900_BODY_CHEST_PART];
    work->hitEffectArg.spawnArgLo = ACTOR_510900_HIT_EFFECT_ARGUMENT_LOW;
    work->hitEffectArg.spawnArgHi = ACTOR_510900_HIT_EFFECT_ARGUMENT_HIGH;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_510900_80167AA4, bodyModel,
                         work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_510900_BODY_INITIAL_ANIMATION);
    }
    work->present = 1;
    enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_PROP_DESCRIPTOR, 0, enemy);
    childEnemy = enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_WEAPON_DESCRIPTOR, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(childEnemy->task->extra.tmd, enemy);
    work->weaponTask = childEnemy->task;
    flameEffect      = effectSpawn((EFFECT_ACTOR_510900_FLAME_JET | EFFECT_SPAWN_UNLIMITED), childEnemy->task->extra.tmd->coords, 0, NULL);
    if (flameEffect != NULL) {
        work->flameJetTask = flameEffect->task;
        taskReparent(task, flameEffect->task);
    }
    if (work->flameJetTask != NULL) {
        work->flameJetTask->spawnArg1.value = ACTOR_510900_FLAME_OFF;
    }
    childEnemy = enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_CHEST_DESCRIPTOR, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(childEnemy->task->extra.tmd, enemy);
    work->chestModelTask = childEnemy->task;
    enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_LIGHT_DESCRIPTOR, 0, enemy);
    enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_LIGHT_DESCRIPTOR, 1, enemy);
    enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_LIGHT_DESCRIPTOR, 2, enemy);
    enemySpawnFromTable(D_actor_510900_80167A18, ACTOR_510900_BLAST_SOURCE_DESCRIPTOR, 0, enemy);
    work->body.coord            = &task->extra.tmd->coords[ACTOR_510900_BODY_CHEST_PART];
    bodyContacts                = work->bodyContacts;
    work->body.context.contacts = bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = ACTOR_510900_BODY_CONTACT_KEY;
    work->body.radius           = ACTOR_510900_BODY_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    attackContacts                      = work->attackContacts;
    work->body.flags                   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->weaponAttack.coord            = work->weaponTask->extra.tmd->coords;
    work->weaponAttack.context.contacts = attackContacts;
    work->weaponAttack.pos.vx           = ACTOR_510900_WEAPON_ATTACK_OFFSET_X;
    work->weaponAttack.pos.vy           = ACTOR_510900_WEAPON_ATTACK_OFFSET_Y;
    work->weaponAttack.pos.vz           = 0;
    work->weaponAttack.key              = 0;
    work->weaponAttack.radius           = ACTOR_510900_ATTACK_RADIUS;
    work->weaponAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->weaponAttack);
    worldCollisionInitContacts(attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->weaponAttack.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->forearmAttack.coord            = &task->extra.tmd->coords[ACTOR_510900_BODY_FOREARM_PART];
    work->forearmAttack.context.contacts = attackContacts;
    work->forearmAttack.pos.vx           = 0;
    work->forearmAttack.pos.vy           = 0;
    work->forearmAttack.pos.vz           = 0;
    work->forearmAttack.key              = 0;
    work->forearmAttack.radius           = ACTOR_510900_ATTACK_RADIUS;
    work->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->forearmAttack);
    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->msgTable             = D_actor_510900_80167A6C;
    task->exitCallback         = actor510900ExitBody;
    actor510900RestoreGridFaces(task);
    actor510900SetExtraGridFace(true);
    task->state = ACTOR_510900_BODY_TASK_RUNNING;
}

void actor510900TickEvent(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_510900_EVENT_FLAME_ANIMATION   = 32,
        ACTOR_510900_EVENT_FLAME_FRAME       = 210,
        ACTOR_510900_EVENT_FLAME_FRAMES      = 255,
        ACTOR_510900_EVENT_FLAME_START_SOUND = SOUND_CHARACTER(0x78, 14),
        ACTOR_510900_EVENT_FLAME_LOOP_SOUND  = SOUND_CHARACTER(0x78, 17),
    };
    GfxCoord*        rootCoord;
    Actor510900Work* work;
    s32              startSound;
    s32              startPan;
    s32              loopPan;
    s32              slotIndex;

    work                          = task->work;
    rootCoord                     = task->extra.tmd->coords;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->animationId == ACTOR_510900_EVENT_FLAME_ANIMATION && work->animationFrame == ACTOR_510900_EVENT_FLAME_FRAME) {
        work->flameMode   = ACTOR_510900_FLAME_BURNING;
        work->flameFrames = ACTOR_510900_EVENT_FLAME_FRAMES;
        startSound        = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_510900_EVENT_FLAME_START_SOUND;
        startPan          = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(startSound, startPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        work->eventFlameSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_510900_EVENT_FLAME_LOOP_SOUND;
        loopPan               = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(work->eventFlameSound, loopPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
    work->animationFrame++;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    actor510900UpdateLighting(task, rootCoord);
    // Publish changed flame modes to the adopted jet after updating the body pose.
    if (work->flameMode != work->sentFlameMode) {
        if (work->flameJetTask != NULL) {
            work->flameJetTask->spawnArg1.value = work->flameMode;
        }
        work->sentFlameMode = work->flameMode;
    }
}
