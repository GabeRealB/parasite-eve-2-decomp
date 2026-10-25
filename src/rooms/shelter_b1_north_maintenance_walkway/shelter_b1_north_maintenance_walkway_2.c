#include "rooms/shelter_b1_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_north_maintenance_walkway_private.h"

#include "actors/actor_00300.h"
#include "actors/actor_01100.h"
#include "actors/actor_02100.h"
#include "actors/actor_02400.h"
#include "actors/actor_04600.h"
#include "actors/actor_05700.h"
#include "actors/actor_07000.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/sound_ids.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0x374F,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"
#include "../../shared/room/room_visual_effects_trail_data.inc.c"
#include "../../shared/room/room_visual_effects_disc_data.inc.c"
#include "../../shared/room/room_events.h"
#include "../../shared/effects/glow_draw.h"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

u8* gShelterB1NorthMaintenanceWalkwayViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1NorthMaintenanceWalkwayRoomViewCounts[1] = { 6 };

DirectionWarpEntry gShelterB1NorthMaintenanceWalkwayWarpEntries[2] = {
    {
        .player         = { { .word = 0 }, -2000, 0, 3600 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, -2000, 0, 3600 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1B3,
    },
    {
        .player         = { { .word = 0 }, 1983, 0, -4420 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 1983, 0, -4420 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 3),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_SHELTER_1B0,
    },
};

/// Room-space collision normals, with 4096 for unit length.
static SVECTOR _gShelterB1NorthMaintenanceWalkwayCollisionGridNormals[14] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_normals.inc"
};

/// Floor and wall vertices in signed room-coordinate units.
static SVECTOR _gShelterB1NorthMaintenanceWalkwayCollisionGridVerts[38] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_verts.inc"
};

/// Indexed floor and wall quads; floor faces use surface class 1, the others class 0.
static WorldCollisionGridFace _gShelterB1NorthMaintenanceWalkwayCollisionGridFaces[18] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_faces.inc"
};

/// Packed signed face-index lists, each ending at WORLD_COLLISION_GRID_CELL_END.
///
/// Retains the authored words between and after the six lists; they are not
/// additional cells. All selected indices fit the face array.
static s16 _gShelterB1NorthMaintenanceWalkwayCollisionGridCells[92] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1NorthMaintenanceWalkwayCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..91,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1NorthMaintenanceWalkwayCollisionGridCells[(halfwordOffset)])
/// X-major cell directory, with three Z cells per X row.
static s16* _gShelterB1NorthMaintenanceWalkwayCollisionGridTable[6] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_table.inc"
};
#undef SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1NorthMaintenanceWalkwayCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1NorthMaintenanceWalkwayCollisionGridNormals,
    .vertices    = _gShelterB1NorthMaintenanceWalkwayCollisionGridVerts,
    .faces       = _gShelterB1NorthMaintenanceWalkwayCollisionGridFaces,
    .cellFaceIds = _gShelterB1NorthMaintenanceWalkwayCollisionGridTable,
    .xBias       = 2872,
    .zBias       = 4937,
    .cellCountX  = 2,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB1NorthMaintenanceWalkwayCollisionGridFaces),
};

ViewCamera gShelterB1NorthMaintenanceWalkwayViewCameras[6] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x35D2, 0 } }, 235 },
    { { { { -1505, 0, 3809 }, { 1128, 3912, 446 }, { -3638, 1213, -1437 } }, { -1615, 1990, -4920 } }, 235 },
    { { { { -938, 0, -3987 }, { -839, 4004, 197 }, { 3897, 861, -917 } }, { 2967, 1799, -4953 } }, 235 },
    { { { { -3922, 0, 1178 }, { 358, 3901, 1193 }, { -1122, 1245, -3736 } }, { -2962, 1720, -4953 } }, 225 },
    { { { { -3898, 0, 1255 }, { 321, 3959, 999 }, { -1213, 1049, -3768 } }, { -2910, 1543, -771 } }, 235 },
    { { { { -938, 0, -3987 }, { -839, 4004, 197 }, { 3897, 861, -917 } }, { 2967, 1799, -4953 } }, 235 },
};

/// View 1's empty sprite list; its zero first count enables decoded image strips.
static SpriteBatch _gShelterB1NorthMaintenanceWalkwayView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list; its zero first count enables decoded image strips.
static SpriteBatch _gShelterB1NorthMaintenanceWalkwayView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's encoded 8-bit texture page, two palette rows and neutral RGB modulation.
enum {
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE               = getTPage(1, 0, 960, 0),
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255       = getClut(0, 255),
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256       = getClut(0, 256),
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION = 128,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_VIEW_3_SCENE_SPRITE_INDEX = 31,
};

/// View 3's 38 textured rectangles for depth-sorted drawing over its image strips.
///
/// Positions and sizes use drawing pixels, UVs use texels and depth is unscaled.
/// Source 31 is the scene patch; its batch visibility follows the B2 scene flag.
static SpriteSource _gShelterB1NorthMaintenanceWalkwayView3SpriteSources[38] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, 128, -96, 730, { .fields = { 16, 64 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 80, -96, 764, { .fields = { 8, 112 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 40, -80, 955, { .fields = { 16, 0 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 80, -80, 792, { .fields = { 16, 16 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, -80, 778, { .fields = { 8, 80 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 120, -64, 875, { .fields = { 48, 136 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 120, -40, 875, { .fields = { 48, 160 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 120, -16, 875, { .fields = { 48, 232 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 120, 8, 875, { .fields = { 56, 112 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 120, 32, 875, { .fields = { 48, 64 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 120, 56, 875, { .fields = { 8, 144 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 80, 48, 1075, { .fields = { 48, 88 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 80, 24, 1050, { .fields = { 40, 208 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 80, 0, 975, { .fields = { 40, 184 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 80, -48, 925, { .fields = { 80, 208 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 80, -64, 925, { .fields = { 8, 128 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 40, -64, 955, { .fields = { 8, 176 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 56, -56, 950, { .fields = { 24, 232 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, 64, -48, 950, { .fields = { 32, 160 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 72, -40, 975, { .fields = { 112, 232 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 72, -16, 950, { .fields = { 120, 216 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 72, 8, 1075, { .fields = { 112, 184 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 72, 32, 1075, { .fields = { 40, 96 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 48, 48, 1210, { .fields = { 80, 184 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 40, 32, 1100, { .fields = { 88, 64 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 56, 24, 1075, { .fields = { 88, 88 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 48, 32, 1109, { .fields = { 120, 240 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 64, 16, 1075, { .fields = { 120, 184 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 80, -24, 922, { .fields = { 88, 144 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 96, -24, 950, { .fields = { 88, 232 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 32, -96, 945, { .fields = { 8, 32 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_255, { .fields = { 72, 64 } }, -40, -24, 5000, { .fields = { 56, 0 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256, { .fields = { 32, 40 } }, 128, -120, 875, { .fields = { 96, 64 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256, { .fields = { 24, 40 } }, 104, -120, 900, { .fields = { 104, 144 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256, { .fields = { 32, 40 } }, 72, -120, 912, { .fields = { 96, 104 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 56, -112, 925, { .fields = { 88, 168 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, 40, -104, 925, { .fields = { 32, 168 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_PAGE, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_CLUT_ROW_256, { .fields = { 48, 16 } }, 24, -96, 925, { .fields = { 8, 48 } }, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 3's image-strip marker, two scenery ranges and single-sprite scene range.
///
/// The scene range remains writable through `gShelterB1NorthMaintenanceWalkwaySpriteViews`.
/// The two serialized bytes at the end of each batch retain unproven meaning.
static SpriteBatch _gShelterB1NorthMaintenanceWalkwayView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_VIEW_3_SCENE_SPRITE_INDEX, 0, 0, { 2, 0 } },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_VIEW_3_SCENE_SPRITE_INDEX, 1, 0, 0, { 1, 0 } },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_VIEW_3_SCENE_SPRITE_INDEX + 1, ARRAY_SIZE(_gShelterB1NorthMaintenanceWalkwayView3SpriteSources) - SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_VIEW_3_SCENE_SPRITE_INDEX - 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list; its zero first count enables decoded image strips.
static SpriteBatch _gShelterB1NorthMaintenanceWalkwayView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list; its zero first count enables decoded image strips.
static SpriteBatch _gShelterB1NorthMaintenanceWalkwayView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list; its zero first count enables decoded image strips.
static SpriteBatch _gShelterB1NorthMaintenanceWalkwayView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1NorthMaintenanceWalkwaySpriteViews[6] = {
    { { .empty = _gShelterB1NorthMaintenanceWalkwayView1SpriteBatches }, _gShelterB1NorthMaintenanceWalkwayView1SpriteBatches, NULL },
    { { .empty = _gShelterB1NorthMaintenanceWalkwayView2SpriteBatches }, _gShelterB1NorthMaintenanceWalkwayView2SpriteBatches, NULL },
    { { .elements = _gShelterB1NorthMaintenanceWalkwayView3SpriteSources }, _gShelterB1NorthMaintenanceWalkwayView3SpriteBatches, NULL },
    { { .empty = _gShelterB1NorthMaintenanceWalkwayView4SpriteBatches }, _gShelterB1NorthMaintenanceWalkwayView4SpriteBatches, NULL },
    { { .empty = _gShelterB1NorthMaintenanceWalkwayView5SpriteBatches }, _gShelterB1NorthMaintenanceWalkwayView5SpriteBatches, NULL },
    { { .empty = _gShelterB1NorthMaintenanceWalkwayView6SpriteBatches }, _gShelterB1NorthMaintenanceWalkwayView6SpriteBatches, NULL },
};

/// Five mutable world-space point lights contributing in every room view.
///
/// RGB uses 12 fractional bits; inner and outer radii use world-coordinate units.
/// Room lighting binds their parents and updates their composition and attenuation.
static WorldCoordPointLight _gShelterB1NorthMaintenanceWalkwayPointLights[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1863, -221, 4731 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2758, 2836, 2918 }, { 0, 0 } }, 948, 2672 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 68, -223, 4315 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2698, 2860, 2900 }, { 0, 0 } }, 1359, 3223 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2002, -504, -2007 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2839, 2881 }, { 0, 0 } }, 1799, 3522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1916, -2, 445 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2839, 2860, 2999 }, { 0, 0 } }, 1721, 3243 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1988, -223, 2836 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2659, 2760, 2850 }, { 0, 0 } }, 1741, 3222 },
};

WorldCoordRoomLights gShelterB1NorthMaintenanceWalkwayRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1NorthMaintenanceWalkwayPointLights),
    .pointLights           = _gShelterB1NorthMaintenanceWalkwayPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

WorldCollisionTrigger gShelterB1NorthMaintenanceWalkwayViewBoundaryTriggers[6] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2080, -1584, -576, 0 }, .vertices = { { -1376, -1904, 192, 0 }, { 1376, -1904, -192, 0 }, { -1376, 1904, 192, 0 }, { 1376, 1904, -192, 0 } }, .normal = { -569, 0, -4074, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2346, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2048, -1600, -672, 0 }, .vertices = { { 1408, -1904, -192, 0 }, { -1408, -1904, 192, 0 }, { 1408, 1904, -192, 0 }, { -1408, 1904, 192, 0 } }, .normal = { 554, 0, 4067, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2374, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2240, -1504, 3200, 0 }, .vertices = { { 1536, -1904, 256, 0 }, { -1536, -1904, -256, 0 }, { 1536, 1904, 256, 0 }, { -1536, 1904, -256, 0 } }, .normal = { -676, 0, 4053, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2455, .control = 0, .parameter0 = 3, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 2176, -1505, 3280, 0 }, .vertices = { { -1536, -1904, -272, 0 }, { 1536, -1904, 272, 0 }, { -1536, 1904, -272, 0 }, { 1536, 1904, 272, 0 } }, .normal = { 714, 0, -4039, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2455, .control = 0, .parameter0 = 4, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { -608, -1568, 4016, 0 }, .vertices = { { 96, -1904, -1312, 0 }, { -96, -1904, 1312, 0 }, { 96, 1904, -1312, 0 }, { -96, 1904, 1312, 0 } }, .normal = { 4090, 0, 298, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2304, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { -496, -1600, 3984, 0 }, .vertices = { { -80, -1904, 1328, 0 }, { 80, -1904, -1328, 0 }, { -80, 1904, 1328, 0 }, { 80, 1904, -1328, 0 } }, .normal = { -4097, 0, -248, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 2318, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

WorldCollisionOccluder gShelterB1NorthMaintenanceWalkwaySightOccluders[1] = {
    { .next = NULL, .prevLink = NULL, .origin = { -1280, -1104, 1776, 0 }, .vertices = { { -1920, 2128, -848, 0 }, { 1920, 2128, 848, 0 }, { -1920, -2128, -848, 0 }, { 1920, -2128, 848, 0 } }, .normal = { -1662, 0, 3761, 0 }, .radius = 2985, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST, .unknown_3B = 0 },
};

/// Resource-entry keys shared by the walkway's actor recipes and placements.
enum {
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_BRAIN_STINGER = 3,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_MOSSBACK      = 11,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER       = 21,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA        = 24,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SUCKLERCEPH   = 46,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER = 47,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_ROOK_GOLEM    = 57,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SLOUCH        = 70,
};

/// Variant 1's Watcher and Amoeba file selections and spawn recipes.
///
/// Matches placement entry IDs and ends at AREA_PLACEMENT_END. The room and
/// selected actor overlays must remain loaded while recipes are used.
static AreaResource _gShelterB1NorthMaintenanceWalkwayVariant1Resources[3] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's Brain Stinger file selection and spawn recipe, then the end marker.
static AreaResource _gShelterB1NorthMaintenanceWalkwayVariant2Resources[2] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_BRAIN_STINGER, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor100300Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Slouch, Sucklerceph and Skull Stalker file selections and recipes.
///
/// Entry 47 retains file number 47 in group 20, although its task descriptor
/// belongs to the same actor overlay as entry 46. Ends at AREA_PLACEMENT_END.
static AreaResource _gShelterB1NorthMaintenanceWalkwayVariant3Resources[4] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SLOUCH, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor107000SlouchTaskDescs },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SUCKLERCEPH, 46, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor204600SucklercephTaskDesc },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 47, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor204600SkullStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4's Mossback file selection and spawn recipe, then the end marker.
static AreaResource _gShelterB1NorthMaintenanceWalkwayVariant4Resources[2] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_MOSSBACK, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor101100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's Watcher and Rook GOLEM file selections and spawn recipes.
///
/// Matches placement entry IDs and ends at AREA_PLACEMENT_END; recipes borrow
/// the selected actor overlays' task descriptors.
static AreaResource _gShelterB1NorthMaintenanceWalkwayVariant11Resources[3] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_ROOK_GOLEM, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 1's two Watchers and three Amoebas, followed by the end marker.
///
/// Positions use world units and yaw uses 4096 units per turn. Spawned actors
/// borrow these records until teardown, so this room overlay must stay loaded.
static AreaPlacement _gShelterB1NorthMaintenanceWalkwayVariant1Placements[6] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 1, 0, -2900, -2000, 4900, 1024, 0, 0, 2, 3 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 1, 0, -2900, -2000, 4000, 1024, 0, 0, 2, 3 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, 0, 1500, 0, 2900, 800, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, 0, 2500, 0, 2100, 2300, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_AMOEBA, 0, 0, 1500, 0, -1200, 1900, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 2's single Brain Stinger at (2000, 0, 4000), followed by the end marker.
///
/// Coordinates use world units and yaw uses 4096 units per turn. The actor
/// borrows this placement while alive; the room overlay must stay loaded.
static AreaPlacement _gShelterB1NorthMaintenanceWalkwayVariant2Placements[2] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_BRAIN_STINGER, 0, 0, 2000, 0, 4000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 3's two Slouches, three Sucklercephs and four Skull Stalkers.
///
/// Ends at AREA_PLACEMENT_END. Coordinates use world units and yaw uses 4096
/// units per turn; spawned actors borrow the records until teardown.
static AreaPlacement _gShelterB1NorthMaintenanceWalkwayVariant3Placements[10] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SLOUCH, 0, 0, 1750, 0, -1050, 2400, 0, 0, 2, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SLOUCH, 0, 0, 2300, 0, 3600, 3600, 0, 0, 2, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SUCKLERCEPH, 0, 0, 2550, 0, 4500, 500, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SUCKLERCEPH, 0, 0, 500, 0, 3600, 3300, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SUCKLERCEPH, 0, 0, 1850, 0, 2050, 1500, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 0, 0, -1800, 0, 4500, 2100, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 0, 0, -350, 0, 3900, 3050, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 0, 0, 1550, 0, -2000, 1800, 0, 2, 4, 0 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_SKULL_STALKER, 0, 0, 2550, 0, -2400, 2500, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 4's single Mossback at (2000, 0, 1600), followed by the end marker.
///
/// Coordinates use world units and yaw uses 4096 units per turn; the actor
/// borrows this placement while alive.
static AreaPlacement _gShelterB1NorthMaintenanceWalkwayVariant4Placements[2] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_MOSSBACK, 0, 0, 2000, 0, 1600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 11's four Watchers and one Rook GOLEM, followed by the end marker.
///
/// Coordinates use world units and yaw uses 4096 units per turn; spawned actors
/// borrow the records until teardown. All resource keys resolve in this variant.
static AreaPlacement _gShelterB1NorthMaintenanceWalkwayVariant11Placements[6] = {
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 1, 0, -2900, -2000, 4900, 1024, 0, 0, 2, 3 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 1, 0, -2900, -2000, 4000, 1024, 0, 0, 2, 3 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 1, 0, 2000, -1900, 5100, 2048, 0, 0, 2, 3 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_WATCHER, 1, 0, 3200, -1900, 4000, 3072, 0, 0, 2, 3 },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ENTRY_ROOK_GOLEM, 0, 0, -500, 0, 4000, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB1NorthMaintenanceWalkwayAreaVariants[SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = _gShelterB1NorthMaintenanceWalkwayVariant1Placements, .resources = _gShelterB1NorthMaintenanceWalkwayVariant1Resources },
    [2]                    = { .placements = _gShelterB1NorthMaintenanceWalkwayVariant2Placements, .resources = _gShelterB1NorthMaintenanceWalkwayVariant2Resources },
    [3]                    = { .placements = _gShelterB1NorthMaintenanceWalkwayVariant3Placements, .resources = _gShelterB1NorthMaintenanceWalkwayVariant3Resources },
    [4]                    = { .placements = _gShelterB1NorthMaintenanceWalkwayVariant4Placements, .resources = _gShelterB1NorthMaintenanceWalkwayVariant4Resources },
    [11]                   = { .placements = _gShelterB1NorthMaintenanceWalkwayVariant11Placements, .resources = _gShelterB1NorthMaintenanceWalkwayVariant11Resources },
};

/// Action payloads: packed departure/arrival slots and room-local selectors.
enum {
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B1_SLEEPING_QUARTERS = (1 << 4) | 2,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B1_STOREROOM         = (2 << 4) | 3,
};

WorldCollisionTrigger gShelterB1NorthMaintenanceWalkwayActionTriggers[2] = {
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { -2128, -48, 3344, 0 }, .vertices = { { -720, 0, -400, 0 }, { 720, 0, -400, 0 }, { -720, 0, 400, 0 }, { 720, 0, 400, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 822, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, .parameter1 = SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B1_SLEEPING_QUARTERS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .next = NULL, .prevLink = NULL, .coord = NULL, .origin = { 1984, -48, -4560, 0 }, .vertices = { { -1024, 0, -448, 0 }, { 1024, 0, -448, 0 }, { -1024, 0, 448, 0 }, { 1024, 0, 448, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 1115, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B1_STOREROOM, .parameter1 = SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TRIGGER_WARP_TO_B1_STOREROOM, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Type-1 footstep requests whose bank is resolved to the loaded room sound bank.
enum {
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_WALK_FOOTSTEP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_RUN_FOOTSTEP_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Surface class 1's walking, running and scripted-jump footstep cue bases.
///
/// Walk and jump use entry 0x5D; running uses 0x5F. Cue 1 adds one, and
/// companions add 100. Playback requires the current type-1 sound bank loaded.
static WorldCollisionFootstepSounds _gShelterB1NorthMaintenanceWalkwaySurfaceClass1FootstepSounds = {
    .walk         = SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_WALK_FOOTSTEP_SOUND,
    .run          = SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_RUN_FOOTSTEP_SOUND,
    .scriptedJump = SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_WALK_FOOTSTEP_SOUND,
};

/// Classes 0 and 2..7: blocked probes, enabled weapon impacts, pushback and no footsteps.
///
/// The first byte retains its authored zero with unproven meaning.
static WorldCollisionSurfaceProperties _gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1: blocked probes, enabled weapon impacts, pushback and floor footstep cues.
///
/// The first byte retains its authored zero with unproven meaning; the sound
/// table is borrowed from this overlay.
static WorldCollisionSurfaceProperties _gShelterB1NorthMaintenanceWalkwaySurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1NorthMaintenanceWalkwaySurfaceClass1FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1NorthMaintenanceWalkwaySurfacePropertiesByClass[8] = {
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
    &_gShelterB1NorthMaintenanceWalkwaySurfaceClass1Properties,
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
    &_gShelterB1NorthMaintenanceWalkwaySilentBlockingSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedDestination = { 0 };

s8 gShelterB1NorthMaintenanceWalkwayStagedEventStarted = 0;

/// Retained, unreferenced bytes after the event-start byte; their role and grouping are unproven.
static u8 D_shelter_b1_north_maintenance_walkway_80185B7D = 89;

static u8 D_shelter_b1_north_maintenance_walkway_80185B7E = 61;

static u8 D_shelter_b1_north_maintenance_walkway_80185B7F = 49;

RoomLatchedEvent gRoomEventLatched;

#include "../../shared/effects/glow_draw_cone.inc.c"

#include "../../shared/effects/glow_draw_red_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

    void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task * task)
    {
        _roomVisualEffectsGlowDiscTask(task);
    }

    void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task * task)
    {
        _roomVisualEffectsFlyingSparkTask(task);
    }

#include "../../shared/room/room_visual_effects_burst.inc.c"

    void shelterB1NorthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task * task)
    {
        _roomVisualEffectsFlyingOrangeBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
