#include "rooms/dryfield_night_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_535700.h"

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
#include "main/sound_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

/// Fixed beam endpoint pairs and two rows of positions for night room 8's glows.
///
/// Positions use signed whole world-coordinate units and are borrowed only
/// during drawing. Every vector includes the fourth halfword read by the GTE.
typedef struct {
    SVECTOR beamEndpoints[5][2]; // Four green beam pairs followed by one red beam pair
    SVECTOR flareRows[2][7];     // Negative/positive-X rows; 3..6 are flares, 0..2 are unused with unproven role
} _DryfieldNightR08GlowPlacements;
STATIC_ASSERT_SIZEOF(_DryfieldNightR08GlowPlacements, 0xC0);

static WorldCollisionGrid   _gDryfieldNightR08CollisionGrid;
static WorldCoordRoomLights _gDryfieldNightR08RoomLights;

/// Four white-beam endpoint pairs used by views 2 and 5, in integer world units.
static SVECTOR _gDryfieldNightR08WhiteBeamEndpoints[4][2] = {
    { { -667, -1910, -0x4A3D, 0 }, { -667, -1910, -0x45ED, 0 } },
    { { -667, -1910, -0x4309, 0 }, { -667, -1910, -0x3EB4, 0 } },
    { { 668, -1910, -0x4A3D, 0 }, { 668, -1910, -0x45ED, 0 } },
    { { 668, -1910, -0x4309, 0 }, { 668, -1910, -0x3EB4, 0 } },
};

/// Fixed green/red beams and flare-row positions selected by views 3, 4 and 6..9.
///
/// Each row retains three leading positions that no current drawer uses; their
/// original role is unproven. All positions use integer world units.
static _DryfieldNightR08GlowPlacements _gDryfieldNightR08GlowPlacements = {
    {
        { { -3246, -2265, -10632, 0 }, { -3246, -2265, -9370, 0 } },
        { { -3246, -2265, -2620, 0 }, { -3246, -2265, -1381, 0 } },
        { { 3260, -2265, -10632, 0 }, { 3260, -2265, -9370, 0 } },
        { { 3260, -2265, -2620, 0 }, { 3260, -2265, -1381, 0 } },
        { { -365, -2226, -180, 0 }, { 365, -2226, -180, 0 } },
    },
    {
        {
            { -1293, -4768, -13478, 0 },
            { -1293, -4768, -11488, 0 },
            { -1293, -4768, -9483, 0 },
            { -1293, -4768, -7483, 0 },
            { -1293, -4768, -5483, 0 },
            { -1293, -4768, -3483, 0 },
            { -1293, -4768, -1483, 0 },
        },
        {
            { 1293, -4768, -13478, 0 },
            { 1293, -4768, -11488, 0 },
            { 1293, -4768, -9483, 0 },
            { 1293, -4768, -7483, 0 },
            { 1293, -4768, -5483, 0 },
            { 1293, -4768, -3483, 0 },
            { 1293, -4768, -1483, 0 },
        },
    },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting gDryfieldNightR08RoomLighting[1] = {
    { &_gDryfieldNightR08RoomLights, NULL },
};

WorldCollisionRoomResources gDryfieldNightR08RoomCollisionResources[1] = {
    { &_gDryfieldNightR08CollisionGrid, NULL, NULL, NULL },
};

u8* gDryfieldNightR08ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightR08ViewCounts[1] = { ARRAY_SIZE(gDryfieldNightR08ViewCameras) };

DirectionWarpEntry gDryfieldNightR08WarpEntries[1] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 0, .y = 0, .z = -0x4650 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = 0 }, .x = 0, .y = 0, .z = -0x4650 },
        .unknown_24     = { 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 1,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Grid-local face normals, with 4096 representing unit length.
static SVECTOR _gDryfieldNightR08CollisionGridNormals[34] = {
#include "assets/dryfield_night_r08_collision_03EB4_normals.inc"
};

/// Grid-local vertices in signed integer world units.
static SVECTOR _gDryfieldNightR08CollisionGridVerts[172] = {
#include "assets/dryfield_night_r08_collision_03EB4_verts.inc"
};

/// Indexed collision faces using surface classes 0, 1 and 2.
static WorldCollisionGridFace _gDryfieldNightR08CollisionGridFaces[82] = {
#include "assets/dryfield_night_r08_collision_03EB4_faces.inc"
};

/// Signed face-index lists ending in -1; retained inter-list halfwords are unused.
static s16 _gDryfieldNightR08CollisionGridCells[392] = {
#include "assets/dryfield_night_r08_collision_03EB4_cells.inc"
};

/// Asset-initializer binding: a halfword index selects a cell list in this grid.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightR08CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..391 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_R08_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightR08CollisionGridCells[(halfwordOffset)])
/// X-major pointers to the 3 by 6 grid's face-index lists.
static s16* _gDryfieldNightR08CollisionGridTable[18] = {
#include "assets/dryfield_night_r08_collision_03EB4_table.inc"
};
#undef DRYFIELD_NIGHT_R08_CELL_FACE_LIST_AT_OFFSET

/// Room 8's collision mesh and 3 by 6 XZ index of 4000-unit square cells.
///
/// The origin is (-5398, -19715) in room XZ; activation supplies the view
/// coordinate. The mesh and cell lists remain writable while the room is loaded.
static WorldCollisionGrid _gDryfieldNightR08CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightR08CollisionGridNormals,
    .vertices    = _gDryfieldNightR08CollisionGridVerts,
    .faces       = _gDryfieldNightR08CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightR08CollisionGridTable,
    .xBias       = 5398,
    .zBias       = 19715,
    .cellCountX  = 3,
    .cellCountZ  = 6,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightR08CollisionGridFaces),
};

ViewCamera gDryfieldNightR08ViewCameras[9] = {
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

/// View 1's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// GPU texture and neutral RGB modulation for view 6's opaque 8-bit sprite overlays.
enum {
    DRYFIELD_NIGHT_R08_SPRITE_TPAGE       = getTPage(1, 0, 960, 0),
    DRYFIELD_NIGHT_R08_SPRITE_CLUT        = getClut(0, 255),
    DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB = 128
};

/// Nine view-6 overlay rectangles in screen pixels, with authored OT sorting depths.
///
/// Sources stay writable for sprite setup; their batch covers every element.
static SpriteSource _gDryfieldNightR08View6SpriteSources[9] = {
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 16, 56 } }, -160, -96, 500, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -40, 500, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 24, 80 } }, -160, 0, 500, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 56, 32 } }, -136, 48, 500, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 56, 32 } }, -80, 48, 500, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 56, 32 } }, -24, 48, 500, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 16, 40 } }, 32, 48, 450, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 24, 32 } }, 48, 56, 412, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
    { DRYFIELD_NIGHT_R08_SPRITE_TPAGE, DRYFIELD_NIGHT_R08_SPRITE_CLUT, { .fields = { 40, 40 } }, 72, 56, 375, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_R08_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 6's background-strip selector, complete nine-sprite range and end record.
static SpriteBatch _gDryfieldNightR08View6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightR08View6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list, retaining decoded background strips and its end record.
static SpriteBatch _gDryfieldNightR08View9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightR08SpriteViews[9] = {
    { { .empty = _gDryfieldNightR08View1SpriteBatches }, _gDryfieldNightR08View1SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View2SpriteBatches }, _gDryfieldNightR08View2SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View3SpriteBatches }, _gDryfieldNightR08View3SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View4SpriteBatches }, _gDryfieldNightR08View4SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View5SpriteBatches }, _gDryfieldNightR08View5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightR08View6SpriteSources }, _gDryfieldNightR08View6SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View7SpriteBatches }, _gDryfieldNightR08View7SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View8SpriteBatches }, _gDryfieldNightR08View8SpriteBatches, NULL },
    { { .empty = _gDryfieldNightR08View9SpriteBatches }, _gDryfieldNightR08View9SpriteBatches, NULL },
};

/// Three all-view directional lights for model shading, with Q12 RGB intensities.
///
/// Their translation vectors supply directions in room coordinates; room setup
/// binds the transforms to the view, and lighting queries overwrite attenuation.
static WorldCoordLight _gDryfieldNightR08DirectionalLights[3] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2582, 402, 6341 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5324, 5324, 5324 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CC5, -2046, -6920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 8192, 8192, 8192 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2899, 1476, -7757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6144, 6144, 6144 }, { 0, 0 } },
};

/// Room 8's three directional lights; it has no authored point or cone lights.
static WorldCoordRoomLights _gDryfieldNightR08RoomLights = {
    ARRAY_SIZE(_gDryfieldNightR08DirectionalLights),
    _gDryfieldNightR08DirectionalLights,
    0,
    NULL,
    0,
    NULL,
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    DRYFIELD_NIGHT_R08_RESOURCE_AYA_WALKER = 132,
    DRYFIELD_NIGHT_R08_FILE_AYA_WALKER     = 357,
    DRYFIELD_NIGHT_R08_RESOURCE_PAWN_GOLEM = 20,
    DRYFIELD_NIGHT_R08_FILE_PAWN_GOLEM     = 358,
};

/// Aya Walker and Pawn Golem resource recipes for placement variant 1.
///
/// Keys match `gMapDryfieldFullR08Variant1Placements`; AREA_PLACEMENT_END ends the list.
/// File 535700 supplies both walkers' code/models; 535800 supplies golem textures.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gDryfieldNightR08Variant1Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_R08_RESOURCE_AYA_WALKER, .fileNumber = DRYFIELD_NIGHT_R08_FILE_AYA_WALKER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = 0, .taskTable = &gActor535700EveWalkerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_R08_RESOURCE_PAWN_GOLEM, .fileNumber = DRYFIELD_NIGHT_R08_FILE_PAWN_GOLEM, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_50, .taskIndex = PAIR_WALK_TASK_BODY, .taskTable = gActor535700PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldNightR08AreaVariants[11] = {
    [1] = { .placements = gMapDryfieldFullR08Variant1Placements, .resources = _gDryfieldNightR08Variant1Resources },
};

/// Type-1 cue-2 bases; cue 1 adds one and companion playback adds 100.
enum {
    DRYFIELD_NIGHT_R08_FOOTSTEP_WALK_SCRIPT = SOUND_SCRIPT_REQUEST_TYPE_1 | 69,
    DRYFIELD_NIGHT_R08_FOOTSTEP_RUN_SCRIPT  = SOUND_SCRIPT_REQUEST_TYPE_1 | 71
};

/// Surface-class-6 footstep requests; scripted jumps share the walking cue pair.
static WorldCollisionFootstepSounds _gDryfieldNightR08FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_R08_FOOTSTEP_WALK_SCRIPT,
    .run          = DRYFIELD_NIGHT_R08_FOOTSTEP_RUN_SCRIPT,
    .scriptedJump = DRYFIELD_NIGHT_R08_FOOTSTEP_WALK_SCRIPT,
};

/// Blocking, weapon-impact-enabled policy for classes 0..3 and 7, with no footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightR08BlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 4's separate blocking policy, with weapon impacts and no footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightR08Class4BlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 5 passes probes and disables weapon impacts, while retaining contact pushback.
static WorldCollisionSurfaceProperties _gDryfieldNightR08ProbePassSurface = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 6 blocks probes, permits weapon impacts and supplies paired footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightR08BlockingFootstepSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldNightR08FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightR08SurfacePropertiesByClass[8] = {
    &_gDryfieldNightR08BlockingSurface,
    &_gDryfieldNightR08BlockingSurface,
    &_gDryfieldNightR08BlockingSurface,
    &_gDryfieldNightR08BlockingSurface,
    &_gDryfieldNightR08Class4BlockingSurface,
    &_gDryfieldNightR08ProbePassSurface,
    &_gDryfieldNightR08BlockingFootstepSurface,
    &_gDryfieldNightR08BlockingSurface,
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
        DRYFIELD_NIGHT_R08_MAIN_FLARE_NEGATIVE_X_START = 3,
        DRYFIELD_NIGHT_R08_MAIN_FLARE_POSITIVE_X_START = 3
    };

    // Submit the negative-X row before the positive-X row to retain packet order.
    _dryfieldNightR08DrawFlareRow(&_gDryfieldNightR08GlowPlacements.flareRows[0][DRYFIELD_NIGHT_R08_MAIN_FLARE_NEGATIVE_X_START]);
    _dryfieldNightR08DrawFlareRow(&_gDryfieldNightR08GlowPlacements.flareRows[1][DRYFIELD_NIGHT_R08_MAIN_FLARE_POSITIVE_X_START]);
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
    switch (gGameSession->location.view) {
        case 3:
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[1], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[3], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            _dryfieldNightR08DrawMainFlares();
            break;
        case 2:
        case 5:
            _glowDrawBeam(_gDryfieldNightR08WhiteBeamEndpoints[0], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            _glowDrawBeam(_gDryfieldNightR08WhiteBeamEndpoints[1], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            _glowDrawBeam(_gDryfieldNightR08WhiteBeamEndpoints[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            _glowDrawBeam(_gDryfieldNightR08WhiteBeamEndpoints[3], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_WHITE);
            break;
        case 4:
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[1], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[3], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            glowDrawFlareClipped(&_gDryfieldNightR08GlowPlacements.flareRows[0][6], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&_gDryfieldNightR08GlowPlacements.flareRows[1][5], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&_gDryfieldNightR08GlowPlacements.flareRows[1][6], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            break;
        case 6:
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[0], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[1], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[3], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            _dryfieldNightR08DrawMainFlares();
            break;
        case 7:
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[0], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            break;
        case 8:
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[2], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[3], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            _dryfieldNightR08DrawMainFlares();
            break;
        case 9:
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[3], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, 0, DRYFIELD_NIGHT_R08_BEAM_GREEN);
            _glowDrawBeam(_gDryfieldNightR08GlowPlacements.beamEndpoints[4], DRYFIELD_NIGHT_R08_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, DRYFIELD_NIGHT_R08_BEAM_RED);
            glowDrawFlareClipped(&_gDryfieldNightR08GlowPlacements.flareRows[0][6], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&_gDryfieldNightR08GlowPlacements.flareRows[1][6], DRYFIELD_NIGHT_R08_FLARE_TEXTURE_COLUMN, DRYFIELD_NIGHT_R08_FLARE_RADIUS_SCALE);
            break;
    }
}

#include "../../shared/effects/glow_draw_beam.inc.c"

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void dryfieldNightR08RoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void dryfieldNightR08RoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void dryfieldNightR08RoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
