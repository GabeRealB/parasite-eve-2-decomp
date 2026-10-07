#include "rooms/dryfield_night_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

#define D_dryfield_night_r08_801805BC (D_dryfield_night_r08_801805AC + 2)
#define D_dryfield_night_r08_801805CC (D_dryfield_night_r08_801805AC + 4)
#define D_dryfield_night_r08_801805DC (D_dryfield_night_r08_801805AC + 6)
#define D_dryfield_night_r08_80180664 (D_dryfield_night_r08_801805AC + 23)

extern SVECTOR D_dryfield_night_r08_8018056C[];

extern WorldCollisionGrid   D_dryfield_night_r08_80181474[1];
extern WorldCoordRoomLights D_dryfield_night_r08_8018189C[1];

SVECTOR D_dryfield_night_r08_8018056C[8] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_dryfield_night_r08_801805AC[24] = {
    { -3246, -2265, -10632, 0 },
    { -3246, -2265, -9370, 0 },
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
    { 3260, -2265, -10632, 0 },
    { 3260, -2265, -9370, 0 },
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -13478, 0 },
    { -1293, -4768, -11488, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -13478, 0 },
    { 1293, -4768, -11488, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting D_dryfield_night_r08_8018067C[1] = {
    { D_dryfield_night_r08_8018189C, NULL },
};

WorldCollisionRoomResources D_dryfield_night_r08_80180684[1] = {
    { D_dryfield_night_r08_80181474, NULL, NULL, NULL },
};

u8* D_dryfield_night_r08_80180694[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_r08_80180698[1] = { 9 };

DirectionWarpEntry D_dryfield_night_r08_8018069C[1] = {
    { { { .word = 0 }, 0, 0, -0x4650 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0, 0, -0x4650 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 1, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldNightR08Collision03EB4Normals[34] = {
#include "assets/dryfield_night_r08_collision_03EB4_normals.inc"
};

static SVECTOR _gDryfieldNightR08Collision03EB4Verts[172] = {
#include "assets/dryfield_night_r08_collision_03EB4_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightR08Collision03EB4Faces[82] = {
#include "assets/dryfield_night_r08_collision_03EB4_faces.inc"
};

static s16 _gDryfieldNightR08Collision03EB4Cells[392] = {
#include "assets/dryfield_night_r08_collision_03EB4_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightR08Collision03EB4Cells[i])
static s16* _gDryfieldNightR08Collision03EB4Table[18] = {
#include "assets/dryfield_night_r08_collision_03EB4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_r08_80181474[1] = {
    { NULL, _gDryfieldNightR08Collision03EB4Normals, _gDryfieldNightR08Collision03EB4Verts, _gDryfieldNightR08Collision03EB4Faces, _gDryfieldNightR08Collision03EB4Table, 5398, 0x4D03, 3, 6, 4000, 82 },
};

ViewCamera D_dryfield_night_r08_80181498[9] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -4086, 0, 279 }, { 19, 4085, 285 }, { -278, 286, -4076 } }, { -92, 1285, 0x2769 } }, 380 },
    { { { { 4017, 0, -798 }, { -146, 4026, -738 }, { 784, 752, 3949 } }, { 853, 1783, 0x391E } }, 148 },
    { { { { 4026, 0, -753 }, { -13, 4095, -74 }, { 753, 75, 4025 } }, { 199, 1212, 0x30DF } }, 282 },
    { { { { -4095, 0, 2 }, { 0, 4095, 4 }, { -2, 4, -4095 } }, { 4, 1285, 8562 } }, 251 },
    { { { { 4071, 0, -448 }, { 66, 4050, 606 }, { 443, -609, 4026 } }, { 1561, 288, 0x38BC } }, 257 },
    { { { { -833, 0, 4010 }, { 882, 3995, 183 }, { -3912, 901, -812 } }, { -3391, 1940, 9236 } }, 246 },
    { { { { 3753, 0, -1640 }, { 59, 4093, 135 }, { 1639, -148, 3750 } }, { 717, 966, 0x3CDD } }, 257 },
    { { { { 3903, 0, -1241 }, { -46, 4093, -147 }, { 1240, 154, 3900 } }, { 530, 1231, 0x2C30 } }, 329 },
};

SpriteBatch D_dryfield_night_r08_801815DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_801815EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_801815FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_8018160C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_8018161C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_r08_8018162C[9] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -96, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -160, 0, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -136, 48, 500, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -80, 48, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -24, 48, 500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 48, 450, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, 56, 412, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, 56, 375, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_r08_801816E0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_801816F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_80181708[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_80181718[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_r08_80181728[9] = {
    { { .empty = D_dryfield_night_r08_801815DC }, D_dryfield_night_r08_801815DC, NULL },
    { { .empty = D_dryfield_night_r08_801815EC }, D_dryfield_night_r08_801815EC, NULL },
    { { .empty = D_dryfield_night_r08_801815FC }, D_dryfield_night_r08_801815FC, NULL },
    { { .empty = D_dryfield_night_r08_8018160C }, D_dryfield_night_r08_8018160C, NULL },
    { { .empty = D_dryfield_night_r08_8018161C }, D_dryfield_night_r08_8018161C, NULL },
    { { .elements = D_dryfield_night_r08_8018162C }, D_dryfield_night_r08_801816E0, NULL },
    { { .empty = D_dryfield_night_r08_801816F8 }, D_dryfield_night_r08_801816F8, NULL },
    { { .empty = D_dryfield_night_r08_80181708 }, D_dryfield_night_r08_80181708, NULL },
    { { .empty = D_dryfield_night_r08_80181718 }, D_dryfield_night_r08_80181718, NULL },
};

WorldCoordLight D_dryfield_night_r08_80181794[3] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2582, 402, 6341 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5324, 5324, 5324 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CC5, -2046, -6920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 8192, 8192, 8192 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2899, 1476, -7757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6144, 6144, 6144 }, { 0, 0 } },
};

WorldCoordRoomLights D_dryfield_night_r08_8018189C[1] = {
    { ARRAY_SIZE(D_dryfield_night_r08_80181794), D_dryfield_night_r08_80181794, 0, NULL, 0, NULL },
};

AreaResource D_dryfield_night_r08_801818B4[3] = {
    { 132, 357, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, &D_actor_535700_8013DADC },
    { 20, 358, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, gActor535700PairWalkTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_r08_801818D8[11] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B508, D_dryfield_night_r08_801818B4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_dryfield_night_r08_80181930 = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_8018193C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_80181944[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_8018194C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_80181954[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_r08_80181930 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_r08_8018195C[8] = {
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_80181944,
    D_dryfield_night_r08_8018194C,
    D_dryfield_night_r08_80181954,
    D_dryfield_night_r08_8018193C,
};

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Texture column and perspective half-extent numerator for the room's fixed flares.
///
/// Pixel half-extent is the radius scale times 39 / (camera Z / 4).
enum {
    DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE   = 0x300,
    DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN = 1
};

/// Draws the room's flickering flare row at four consecutive world positions.
///
/// Borrows four word-aligned `worldPoints` entries in integer world units,
/// including their readable pad halfwords, only for this call. Each flare uses
/// texture column 1, pixel half-extent `0x300 * 39 / (camera Z / 4)` and RGB
/// intensity 32 or 48 on alternating animation frames. Points below
/// `GLOW_MIN_DEPTH` are rejected independently.
///
/// Requires a composed view, an initialized scratch stack with room for one
/// `GlowCentreRadiusScratch`, a current ordering table and four `POLY_FT4`
/// packets in the frame arena, including packets for rejected points. Reserves
/// packets in array order; queued storage must remain live until GPU completion.
static inline void _dryfieldNightR08DrawFlareRow(const SVECTOR worldPoints[4])
{
    glowDrawFlareClipped(&worldPoints[0], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
    glowDrawFlareClipped(&worldPoints[1], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
    glowDrawFlareClipped(&worldPoints[2], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
    glowDrawFlareClipped(&worldPoints[3], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
}

/// Submits the two four-flare rows shared by night room 8's views 3, 6 and 8.
///
/// Uses the current composed view and frame animation to draw texture column 1
/// with pixel half-extent `0x300 * 39 / (camera Z / 4)` at depths of at least 17.
/// Requires an initialized scratch stack with room for `GlowCentreRadiusScratch`,
/// a current ordering table and eight `POLY_FT4` packets in the frame arena,
/// including packets for points rejected by depth clipping. Queued packets
/// remain in that arena until GPU completion.
static inline void _dryfieldNightR08DrawMainFlares(void)
{
    enum {
        DRYFIELD_NIGHT_R08_MAIN_FLARE_NEGATIVE_X_START = 13,
        DRYFIELD_NIGHT_R08_MAIN_FLARE_POSITIVE_X_START = 20
    };

    // Submit the negative-X row before the positive-X row to retain packet order.
    _dryfieldNightR08DrawFlareRow(&D_dryfield_night_r08_801805AC[DRYFIELD_NIGHT_R08_MAIN_FLARE_NEGATIVE_X_START]);
    _dryfieldNightR08DrawFlareRow(&D_dryfield_night_r08_801805AC[DRYFIELD_NIGHT_R08_MAIN_FLARE_POSITIVE_X_START]);
}

void dryfieldNightR08DrawGlowsTask(Task* task)
{
    // Beam radii scale by 64 / depth; RGB factors occupy bits 8..15, 4 and 0.
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE = 0x200,
           DRYFIELD_NIGHT_R08_BEAM_GREEN        = 1 << 4,
           DRYFIELD_NIGHT_R08_BEAM_RED          = 1 << 8,
           DRYFIELD_NIGHT_R08_BEAM_WHITE        = (1 << 8) | (1 << 4) | 1 };

    // Publish the room-local callbacks before enemies can spawn their effects.
    if (task->state == GLOWS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_NIGHT_R08_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_NIGHT_R08_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_NIGHT_R08_SPARK_BURST;
        task->state             = GLOWS_DRAW;
    }

    // Each beam borrows a consecutive endpoint pair; flare points are single entries.
    switch (gGameSession->location.loc.view) {
        case 3:
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[6], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[8], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            _dryfieldNightR08DrawMainFlares();
            break;
        case 2:
        case 5:
            _glowDrawBeam(&D_dryfield_night_r08_8018056C[0], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            _glowDrawBeam(&D_dryfield_night_r08_8018056C[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            _glowDrawBeam(&D_dryfield_night_r08_8018056C[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            _glowDrawBeam(&D_dryfield_night_r08_8018056C[6], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            break;
        case 4:
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[6], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[8], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[16], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[22], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[23], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            break;
        case 6:
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[0], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[6], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[8], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            _dryfieldNightR08DrawMainFlares();
            break;
        case 7:
            _glowDrawBeam(D_dryfield_night_r08_801805AC, DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            break;
        case 8:
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[6], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[8], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            _dryfieldNightR08DrawMainFlares();
            break;
        case 9:
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[6], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(&D_dryfield_night_r08_801805AC[8], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[16], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[23], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            break;
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_flare_clipped.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void dryfieldNightR08RoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void dryfieldNightR08RoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_night_r08_8017F8FC(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
