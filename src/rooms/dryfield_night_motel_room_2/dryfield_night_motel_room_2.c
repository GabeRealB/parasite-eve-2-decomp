#include "rooms/dryfield_night_motel_room_2.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00700.h"
#include "actors/actor_01600.h"
#include "actors/actor_02500.h"
#include "actors/actor_04000.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"

/// Night motel room 2's mutable cone light and the opaque bytes following it.
///
/// The room light collection exposes only the cone-light array. Its transform
/// and attenuation remain writable while this room overlay is loaded. The
/// following bytes have no established interpretation and stay outside the
/// live light count.
typedef struct {
    WorldCoordSpotLight coneLights[1];    // One cone light enabled in every view; mutable transform and attenuation
    u8                  unknownData[324]; // Opaque bytes outside the live light count; original role unproven
} _DryfieldNightMotelRoom2SpotLightStorage;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelRoom2SpotLightStorage, 432);

static s32 _dryfieldNightMotelRoom2RejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _dryfieldNightMotelRoom2IgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _dryfieldNightMotelRoom2IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_2_TASK_STATE_NEW  = 0,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_TASK_STATE_IDLE = 1,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_TASK_STATE_EXIT = 2,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2, 2),
    DRYFIELD_NIGHT_MOTEL_ROOM_2_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_2, 1),
    DRYFIELD_NIGHT_MOTEL_ROOM_2_MAP_FLAG        = 485,
    // This area's departure slot 1 and main street's arrival slot 4.
    DRYFIELD_NIGHT_MOTEL_ROOM_2_MAIN_STREET_WARP_ENDPOINTS = 0x14,
};

static WorldCollisionGrid     _gDryfieldNightMotelRoom2CollisionGrid;
static WorldCollisionOccluder _gDryfieldNightMotelRoom2SightOccluders[2];
static WorldCollisionTrigger  _gDryfieldNightMotelRoom2ViewBoundaryTriggers[8];
static WorldCollisionTrigger  _gDryfieldNightMotelRoom2ActionTriggers[6];
static WorldCoordRoomLights   _gDryfieldNightMotelRoom2RoomLights;

/// Synchronous room messages: resolve transitions, reject key items and ignore actions and commands.
///
/// The room task borrows this read-only table while the overlay is loaded.
/// The terminal record makes unsupported message IDs return zero. Authored
/// storage remains in the overlay's data section.
static const TaskMessageEntry _gDryfieldNightMotelRoom2RoomMessages[5] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantMainStreetMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelRoom2RejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelRoom2IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelRoom2IgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Two world-space flare centres drawn in logical camera views 2 and 3.
///
/// Signed halfword XYZ values use world-coordinate units; the fourth component
/// is unused. Drawing only reads these overlay-owned points while the room is loaded.
static const SVECTOR _gDryfieldNightMotelRoom2PairedFlarePoints[2] __attribute__((section(".data"))) = {
    { 1660, -970, 470, 0 },
    { 2480, -2160, 2030, 0 },
};

/// World-space flare centre drawn in logical camera views 5 and 6.
///
/// Signed halfword XYZ values use world-coordinate units; the fourth component
/// is unused. Drawing only reads this overlay-owned point while the room is loaded.
static const SVECTOR _gDryfieldNightMotelRoom2SingleFlarePoint __attribute__((section(".data"))) = { 3270, -1840, 4060, 0 };

WorldCoordRoomLighting gDryfieldNightMotelRoom2RoomLighting[1] = {
    { .lights = &_gDryfieldNightMotelRoom2RoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gDryfieldNightMotelRoom2RoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldNightMotelRoom2CollisionGrid,
        .viewBoundaryTriggers = _gDryfieldNightMotelRoom2ViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldNightMotelRoom2ActionTriggers,
        .occluders            = _gDryfieldNightMotelRoom2SightOccluders,
    },
};

u8* gDryfieldNightMotelRoom2RoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightMotelRoom2RoomViewCounts[1] = { DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_COUNT };

DirectionWarpEntry gDryfieldNightMotelRoom2WarpEntries[1] = {
    {
        .player         = { { .word = 3 * ONE / 4 }, 4666, 0, 2610 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3 * ONE / 4 }, 4666, 0, 2610 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_NIGHT_MOTEL_ROOM_2_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_NIGHT_MOTEL_ROOM_2_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_NIGHT_MOTEL_ROOM_2_MAP_FLAG,
    },
};

/// Grid-local normals, with 4096 units per unit length.
static SVECTOR _gDryfieldNightMotelRoom2CollisionGridNormals[13] = {
#include "assets/dryfield_night_motel_room_2_collision_00BC4_normals.inc"
};

/// Collision-mesh vertices in signed grid-local game units.
static SVECTOR _gDryfieldNightMotelRoom2CollisionGridVerts[105] = {
#include "assets/dryfield_night_motel_room_2_collision_00BC4_verts.inc"
};

/// Indexed room quads using surface classes 0..4.
static WorldCollisionGridFace _gDryfieldNightMotelRoom2CollisionGridFaces[43] = {
#include "assets/dryfield_night_motel_room_2_collision_00BC4_faces.inc"
};

/// Four signed face-index lists terminated by WORLD_COLLISION_GRID_CELL_END.
///
/// Lists end at halfword elements 43, 81, 112 and 132. Elements 113 and 133
/// retain authored zeros outside every cell scan.
static s16 _gDryfieldNightMotelRoom2CollisionGridCells[134] = {
#include "assets/dryfield_night_motel_room_2_collision_00BC4_cells.inc"
};

// Generated initializers select cell lists by offsets in halfword elements.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelRoom2CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..133 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_ROOM_2_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelRoom2CollisionGridCells[(halfwordOffset)])
/// X-major 2-by-2 cell table, with list starts at elements 0, 44, 82 and 114.
static s16* _gDryfieldNightMotelRoom2CollisionGridTable[4] = {
#include "assets/dryfield_night_motel_room_2_collision_00BC4_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_ROOM_2_CELL_FACE_LIST_AT_OFFSET

/// Room 1's collision mesh and 2-by-2 XZ index of 4000-unit cells.
///
/// The room owns this mutable descriptor and its pools. Activation binds the
/// current view coordinate; keep the overlay loaded while the grid is active.
static WorldCollisionGrid _gDryfieldNightMotelRoom2CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelRoom2CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelRoom2CollisionGridVerts,
    .faces       = _gDryfieldNightMotelRoom2CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelRoom2CollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = 2,
    .cellCountZ  = ARRAY_SIZE(_gDryfieldNightMotelRoom2CollisionGridTable) / 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelRoom2CollisionGridFaces),
};

ViewCamera gDryfieldNightMotelRoom2ViewCameras[DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 0x280A, -3000 } }, 257 },
    { { { { 897, 0, -3996 }, { -928, 3983, -208 }, { 3887, 951, 872 } }, { -200, 1800, -1300 } }, 246 },
    { { { { 1464, 0, 3825 }, { 452, 4067, -173 }, { -3798, 484, 1454 } }, { -4800, 1500, -1000 } }, 225 },
    { { { { 4063, 0, -513 }, { -81, 4044, -640 }, { 507, 645, 4012 } }, { -600, 1660, -1300 } }, 240 },
    { { { { -888, 0, 3998 }, { 1562, 3770, 347 }, { -3680, 1600, -817 } }, { -4800, 1770, -5300 } }, 240 },
    { { { { -561, 0, -4057 }, { -1524, 3795, 211 }, { 3760, 1538, -520 } }, { -1750, 1770, -5300 } }, 235 },
};

/// View 1's empty sprite list, retaining decoded background strips.
///
/// The zero-count first record is followed by the terminal record. Its address
/// also fills the descriptor's empty source slot, which supplies no SpriteSource.
static SpriteBatch _gDryfieldNightMotelRoom2View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 92 textured foreground rectangles, borrowed for the room's lifetime.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. Batches cover the full mutable array without a sentinel.
static SpriteSource _gDryfieldNightMotelRoom2View2SpriteSources[92] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 80, 625, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 112, 640, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, 88, 600, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 88, 600, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 88, 600, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 0, 88, 600, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -40, 88, 600, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, 88, 600, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, 64, 612, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, 64, 612, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 64, 612, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 64, 612, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 64, 612, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, 64, 625, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, 48, 650, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 128, 32, 675, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, 40, 650, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 40, 650, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 40, 650, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 32, 675, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 40, 650, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 32, 675, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -56, 48, 650, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 500, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 500, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 80, 500, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -8, 1075, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 16, 1075, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, 16, 1075, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, 48, 1025, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 48, 1025, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, 24, 1025, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, 0, 1025, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 24, 1025, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 0, 1025, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, 0, 1025, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 24, 1025, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 16, 875, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 48, 1000, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 48, 875, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 24, 875, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 24, 1000, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 1000, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 8, 875, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 962, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, -120, 1125, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, -112, 1312, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 0, 1375, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -40, 1375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -80, 1375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -104, 1375, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -80, 887, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -40, 881, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 0, 869, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 40, 898, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 40, 875, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 8, 830, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -32, 829, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -64, 837, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -136, -80, 831, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -80, 799, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -48, 809, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 32, 879, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -8, 808, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, 24, 875, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -16, 840, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -48, 788, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -80, 765, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -16, 825, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -48, 798, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -80, 771, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -80, 948, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, -48, 989, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -16, 960, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, -16, 1005, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -120, 16, 947, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 8, 1004, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, 8, 1087, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -24, 1098, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -104, -56, 1086, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -104, -80, 1044, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 0, 1164, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 0, 1233, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -40, 1168, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -40, 1237, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -8, 1310, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -48, 1275, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -80, 1135, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -80, 1218, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -80, 1230, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 40, 1052, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 40, 925, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
};

/// View 2's six contiguous sprite ranges plus background and terminal records.
///
/// The ranges cover all 92 sources. The initial zero count retains decoded
/// background strips; visibility and cached-packet policies remain writable.
static SpriteBatch _gDryfieldNightMotelRoom2View2SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 3, 0 } },
    { 23, 3, 0, 0, { 0, 0 } },
    { 26, 3, 0, 0, { 5, 0 } },
    { 29, 8, 0, 0, { 1, 0 } },
    { 37, 7, 0, 0, { 4, 0 } },
    { 44, ARRAY_SIZE(_gDryfieldNightMotelRoom2View2SpriteSources) - 44, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 87 textured foreground rectangles, borrowed for the room's lifetime.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. Batches cover the full mutable array without a sentinel.
static SpriteSource _gDryfieldNightMotelRoom2View3SpriteSources[87] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 48, 537, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 525, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 80, 475, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 450, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -120, 88, 450, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -80, 88, 450, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -40, 88, 450, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 0, 88, 450, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 88, 450, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, 88, 450, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 72, 475, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 72, 475, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 72, 475, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 72, 475, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 72, 475, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 40, 72, 475, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 64, 525, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 56, 525, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, 56, 525, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 56, 525, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, 56, 525, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 0, 48, 537, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, 96, 550, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -104, 96, 425, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 64, 550, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -128, 64, 550, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 550, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -128, 32, 550, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -128, 0, 550, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -144, 0, 550, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -136, 16, 1075, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 1031, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -152, 56, 1025, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, 56, 1025, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 56, 1025, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, 56, 1025, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -152, 24, 1055, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, 24, 1050, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, 24, 1055, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -56, 24, 1037, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1075, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 16, 1075, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -24, 24, 1037, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -8, 24, 1037, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 1031, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 56, 1031, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 8, 40, 1037, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 670, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 136, 64, 661, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 136, 32, 649, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, 32, 616, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 152, 56, 671, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 64, 644, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 152, 80, 645, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, 88, 645, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 937, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 72, 32, 1000, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 72, 0, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 80, -32, 1000, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, -56, 1000, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, -96, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -104, 1000, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 112, -96, 962, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 112, -112, 962, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 136, -120, 962, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -96, 937, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 152, -56, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 152, -112, 937, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 16, 670, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 56, 625, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -24, 692, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -96, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -96, 1000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 1016, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 1016, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -16, 1000, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -16, 1016, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 24, 1000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, 24, 1000, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 24, 1000, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, 24, 987, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, -16, 1003, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, -56, 1003, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 987, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -56, 1003, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -16, 1003, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 136, 24, 987, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
};

/// View 3's seven contiguous sprite ranges plus background and terminal records.
///
/// The ranges cover all 87 sources. The initial zero count retains decoded
/// background strips; visibility and cached-packet policies remain writable.
static SpriteBatch _gDryfieldNightMotelRoom2View3SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 8, 0, 0, { 4, 0 } },
    { 30, 17, 0, 0, { 1, 0 } },
    { 47, 8, 0, 0, { 5, 0 } },
    { 55, 16, 0, 0, { 0, 0 } },
    { 71, 9, 0, 0, { 6, 0 } },
    { 80, ARRAY_SIZE(_gDryfieldNightMotelRoom2View3SpriteSources) - 80, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 47 textured foreground rectangles, borrowed for the room's lifetime.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. Batches cover the full mutable array without a sentinel.
static SpriteSource _gDryfieldNightMotelRoom2View4SpriteSources[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -88, 1250, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 80, 950, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -120, 725, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -80, 725, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -40, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 0, 725, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 40, 725, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 80, 725, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 80, 800, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 40, 800, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, 0, 800, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 800, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 800, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 950, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -80, 950, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -40, 950, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 0, 950, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, 40, 950, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 1125, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 0, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 1125, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -80, 1125, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -96, 1125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -80, 1250, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, -40, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 0, 1250, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, -120, 725, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 96, -120, 725, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -120, 725, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -80, 725, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 96, -80, 725, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, -80, 725, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, -40, 725, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 56, 0, 725, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, 40, 725, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 56, 80, 725, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 80, 725, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 725, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 0, 725, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -40, 725, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -64, 725, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -64, 725, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -40, 725, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 0, 725, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 40, 725, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 80, 725, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
};

/// View 4's two contiguous sprite ranges plus background and terminal records.
///
/// The ranges cover all 47 sources. The initial zero count retains decoded
/// background strips; visibility and cached-packet policies remain writable.
static SpriteBatch _gDryfieldNightMotelRoom2View4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 1, 0 } },
    { 27, ARRAY_SIZE(_gDryfieldNightMotelRoom2View4SpriteSources) - 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 70 textured foreground rectangles, borrowed for the room's lifetime.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. Batches cover the full mutable array without a sentinel.
static SpriteSource _gDryfieldNightMotelRoom2View5SpriteSources[70] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 500, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 500, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 500, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 500, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 500, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 537, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 537, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -96, -72, 587, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 537, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 537, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -40, 587, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 0, 587, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 40, 537, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 80, 537, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 80, 587, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 40, 587, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, 40, 625, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -64, 0, 912, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -72, -40, 912, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -72, -80, 912, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 912, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -24, 846, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -72, 846, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 846, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -120, 862, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -32, 24, 846, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -8, -24, 835, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 24, 835, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -8, -72, 835, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -8, -120, 835, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 40, 56, 675, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 24, 32, 687, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 40, 32, 687, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 72, 32, 687, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, 16, 700, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 700, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 24, 700, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 300, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 312, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 325, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 120, 0, 350, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 300, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 312, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -40, 325, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 0, 350, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 120, 40, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 120, 80, 375, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 40, 375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 80, 375, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 325, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 0, 35, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 56, 40, 375, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 48, 80, 375, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 48, 562, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, 24, 575, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -112, 24, 575, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -112, 16, 575, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 96, 525, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -96, 96, 525, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -120, 72, 525, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 525, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -64, -120, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -24, -120, 862, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -64, -80, 875, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -24, -80, 862, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -64, -40, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -56, 0, 875, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -24, -40, 862, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -24, 0, 862, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
};

/// View 5's seven contiguous sprite ranges plus background and terminal records.
///
/// The ranges cover all 70 sources. The initial zero count retains decoded
/// background strips; visibility and cached-packet policies remain writable.
static SpriteBatch _gDryfieldNightMotelRoom2View5SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 9, 0, 0, { 4, 0 } },
    { 31, 7, 0, 0, { 1, 0 } },
    { 38, 16, 0, 0, { 5, 0 } },
    { 54, 4, 0, 0, { 0, 0 } },
    { 58, 4, 0, 0, { 6, 0 } },
    { 62, ARRAY_SIZE(_gDryfieldNightMotelRoom2View5SpriteSources) - 62, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 182x238 draw-buffer clip, restored to full screen at sorting depth 835.
///
/// Includes one terminal record. Pixel coordinates start at the draw buffer's
/// top-left; the renderer adds its VRAM Y origin. Borrowed while the room is loaded.
static SpriteDrawArea _gDryfieldNightMotelRoom2View5DrawAreas[2] = {
    { .clipRect = { 0, 0, 182, 238 }, .restoreDepth = 835 },
    { .clipRect = { 0, 0, 0, 0 }, .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// View 6's 79 textured foreground rectangles, borrowed for the room's lifetime.
///
/// Positions and sizes are pixels relative to the view centre; depths are
/// unscaled sorting values. Batches cover the full mutable array without a sentinel.
static SpriteSource _gDryfieldNightMotelRoom2View6SpriteSources[79] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, 80, 907, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, -120, 941, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, 0, 937, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -40, 937, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -80, 937, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 937, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 829, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 883, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 0, 916, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 8, 853, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 24, 786, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -16, 782, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -56, 752, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, -40, 834, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -80, 816, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, -80, 757, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 80, 40, 637, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 40, 639, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 40, 622, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 40, 589, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 720, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 0, 674, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 0, 651, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, 0, 583, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -40, 707, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -40, 663, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 625, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -40, 575, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 695, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -80, 653, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -80, 616, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -80, 567, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 664, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 610, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, -120, 534, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 443, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 514, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 136, -80, 457, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -40, 521, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 462, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 527, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 0, 471, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 40, 531, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 40, 476, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 541, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 482, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -120, 375, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -120, 375, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -112, -120, 375, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -80, 375, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -80, 375, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -80, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -40, 375, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -40, 375, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 375, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 0, 375, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -88, 0, 375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 0, 375, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 0, 375, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, -32, 375, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -88, 40, 375, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, 80, 375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 40, 375, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -64, 80, 375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, 40, 375, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, 80, 375, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -40, 40, 375, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 72, 72, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 72, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 72, 48, 500, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 500, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 487, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 500, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 104, 24, 500, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 487, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 40, 487, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 136, 40, 487, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 104, 56, 450, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

/// View 6's four contiguous sprite ranges plus background and terminal records.
///
/// The ranges cover all 79 sources. The initial zero count retains decoded
/// background strips; visibility and cached-packet policies remain writable.
static SpriteBatch _gDryfieldNightMotelRoom2View6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 46, 0, 0, { 3, 0 } },
    { 46, 22, 0, 0, { 0, 0 } },
    { 68, 4, 0, 0, { 2, 0 } },
    { 72, ARRAY_SIZE(_gDryfieldNightMotelRoom2View6SpriteSources) - 72, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightMotelRoom2SpriteViews[DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_COUNT] = {
    { { .empty = _gDryfieldNightMotelRoom2View1SpriteBatches }, _gDryfieldNightMotelRoom2View1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom2View2SpriteSources }, _gDryfieldNightMotelRoom2View2SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom2View3SpriteSources }, _gDryfieldNightMotelRoom2View3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom2View4SpriteSources }, _gDryfieldNightMotelRoom2View4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelRoom2View5SpriteSources }, _gDryfieldNightMotelRoom2View5SpriteBatches, _gDryfieldNightMotelRoom2View5DrawAreas },
    { { .elements = _gDryfieldNightMotelRoom2View6SpriteSources }, _gDryfieldNightMotelRoom2View6SpriteBatches, NULL },
};

/// Directed camera boundaries joining views 2/3, 3/4, 3/5 and 5/6.
///
/// Origins, relative corners and radii use room-local game units; normals
/// use 4096 per unit. Setup binds and links all eight mutable records, including
/// the LAST-marked final boundary. Keep the room loaded until they are unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelRoom2ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 2590, -1168, 2061, 0 }, { { 783, -2192, 2008, 0 }, { -789, -2192, -2014, 0 }, { 783, 2192, 2008, 0 }, { -789, 2192, -2014, 0 } }, { -3826, 0, 1494, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2495, -1280, 2079, 0 }, { { -791, -2192, -2016, 0 }, { 781, -2192, 2006, 0 }, { -791, 2192, -2016, 0 }, { 781, 2192, 2006, 0 } }, { 3824, 0, -1496, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 847, -1184, 3966, 0 }, { { -838, -2192, 18, 0 }, { 839, -2192, -17, 0 }, { -838, 2192, 18, 0 }, { 839, 2192, -17, 0 } }, { -87, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 895, -1152, 3887, 0 }, { { 846, -2192, -36, 0 }, { -846, -2192, 36, 0 }, { 846, 2192, -36, 0 }, { -846, 2192, 36, 0 } }, { 174, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2464, -1184, 4000, 0 }, { { -838, -2192, 18, 0 }, { 839, -2192, -17, 0 }, { -838, 2192, 18, 0 }, { 839, 2192, -17, 0 } }, { -87, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2431, -1120, 3871, 0 }, { { 839, -2192, -17, 0 }, { -838, -2192, 18, 0 }, { 839, 2192, -17, 0 }, { -838, 2192, 18, 0 } }, { 84, 0, 4099, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3568, -1312, 4975, 0 }, { { -267, -2192, 1058, 0 }, { 267, -2192, -1057, 0 }, { -267, 2192, 1058, 0 }, { 267, 2192, -1057, 0 } }, { -3976, 0, -1005, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3424, -1248, 4927, 0 }, { { 267, -2192, -1057, 0 }, { -267, -2192, 1058, 0 }, { 267, 2192, -1057, 0 }, { -267, 2192, 1058, 0 } }, { 3973, 0, 1002, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room interaction quads: one main-street warp and five CAP command regions.
///
/// CAP indices are 5, 1, 8, 10 and 11; presentation flags are zero. The final
/// region accepts proximity or facing and also carries LAST. Positions and
/// radii use room-local game units, normals use 4096 per unit. Setup binds,
/// links and enables all six writable records for the loaded room's lifetime.
static WorldCollisionTrigger _gDryfieldNightMotelRoom2ActionTriggers[6] = {
    { NULL, NULL, NULL, { 4416, -74, 2576, 0 }, { { -416, 0, -944, 0 }, { 416, 0, -944, 0 }, { -416, 0, 944, 0 }, { 416, 0, 944, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1024, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, DRYFIELD_NIGHT_MOTEL_ROOM_2_MAIN_STREET_WARP_ENDPOINTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 400, -64, 2928, 0 }, { { -336, 0, -480, 0 }, { 336, 0, -480, 0 }, { -336, 0, 480, 0 }, { 336, 0, 480, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 583, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, 5152, 0 }, { { -560, 0, -256, 0 }, { 560, 0, -256, 0 }, { -560, 0, 256, 0 }, { 560, 0, 256, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4097, 0 }, 613, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2272, -64, 4896, 0 }, { { -464, 0, -320, 0 }, { 464, 0, -320, 0 }, { -464, 0, 320, 0 }, { 464, 0, 320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, 4880, 0 }, { { -304, 0, -464, 0 }, { 304, 0, -464, 0 }, { -304, 0, 464, 0 }, { 304, 0, 464, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 554, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3392, -64, 3904, 0 }, { { -1168, 0, 160, 0 }, { 880, 0, 160, 0 }, { -912, 0, 928, 0 }, { 432, 0, 928, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space quads blocking line of sight, with LAST on the second record.
///
/// Origins, relative corners and radii use game units; normals use 4096 per
/// unit. Setup links and enables both mutable records. Retain their storage
/// until unlinked and the room overlay is released. Authored flag bit 0 has
/// no established meaning and is preserved.
static WorldCollisionOccluder _gDryfieldNightMotelRoom2SightOccluders[2] = {
    { NULL, NULL, { 4000, -1280, 3904, 0 }, { { -1024, -1952, 0, 0 }, { 1024, -1952, 0, 0 }, { -1024, 1952, 0, 0 }, { 1024, 1952, 0, 0 } }, { 0, 0, -4101, 0 }, 2202, 1, 0 },
    { NULL, NULL, { 1600, -1312, 4896, 0 }, { { 0, -1952, 1024, 0 }, { 0, -1952, -1024, 0 }, { 0, 1952, 1024, 0 }, { 0, 1952, -1024, 0 } }, { -4101, 0, 0, 0 }, 2202, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Night motel room 2's four point lights, shared by every view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (ONE is full intensity). The loaded room overlay owns
/// this writable array: coordinate updates set its parent and composed matrices,
/// and lighting queries overwrite attenuation. Borrowed light pointers remain
/// valid only while the overlay is loaded.
static WorldCoordPointLight _gDryfieldNightMotelRoom2PointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2346, -1185, 535 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3690, 3360, 3150 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 561,
        .outer = 1501,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3813, -1893, 4098 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2900, 3200, 3450 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1161,
        .outer = 2993,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2482, -2113, 2033 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1064, 983, 901 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 700,
        .outer = 3100,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 612, -1385, 3787 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2100, 2100, 2100 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 550,
        .outer = 3199,
    },
};

/// Night motel room 2's live cone light and retained bytes outside its light count.
///
/// Position and falloff radii use world units; RGB and axis use 12 fractional
/// bits, and the opening angle uses 4096 units per turn. Coordinate updates and
/// light queries mutate the live light. The following 324 bytes have no established
/// role or consumer. Borrow the light only while this overlay is loaded.
static _DryfieldNightMotelRoom2SpotLightStorage _gDryfieldNightMotelRoom2SpotLightStorage = {
    .coneLights = {
        {
            .head = {
                .transform  = { .lighting = {
                                    .composeStamp = GRAPHICS_COORD_DIRTY,
                                    .local        = { { { -4102, 0, -2 }, { -2, 0, 4102 }, { 0, 4101, 0 } }, { 2482, -2788, 2033 } },
                                    .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                    .unknown_46   = { 0, 0, 0, 0 },
                                    .attenuation  = 0,
                                    .parent       = NULL,
                               } },
                .color      = { 2550, 2600, 2700 },
                .unknown_56 = { 0, 0 },
            },
            .axis  = { -1, 4095, 0, 0 },
            .inner = 3800,
            .outer = 4200,
            .angle = 1251,
        },
    },
    .unknownData = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x04,
        0x00,
        0x00,
        0x00,
        0xE0,
        0x1D,
        0x18,
        0x80,
        0x01,
        0x00,
        0x00,
        0x00,
        0x60,
        0x1F,
        0x00,
        0x00,
        0xF0,
        0xF7,
        0xD0,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x08,
        0xD0,
        0x10,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC0,
        0x1F,
        0x18,
        0x80,
        0x70,
        0xB7,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0E,
        0x00,
        0x00,
        0x80,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0xF7,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        0x08,
        0xD0,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x13,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xC2,
        0x12,
        0x00,
        0x00,
        0x04,
        0x05,
        0x61,
        0x00,
        0xA4,
        0x20,
        0x18,
        0x80,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0xF9,
        0xD0,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xFA,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xE3,
        0x11,
        0x00,
        0x00,
        0x06,
        0x05,
        0x61,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x58,
        0x20,
        0x18,
        0x80,
        0x70,
        0xB7,
        0x06,
        0x80,
        0x90,
        0xF4,
        0x00,
        0x00,
        0xA0,
        0x00,
        0x00,
        0x00,
        0x70,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x90,
        0xF7,
        0x30,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC2,
        0x12,
        0x00,
        0x00,
        0x05,
        0x06,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA4,
        0x20,
        0x18,
        0x80,
        0x70,
        0xB7,
        0x00,
        0x00,
        0xF0,
        0xF4,
        0x00,
        0x00,
        0x80,
        0xF3,
        0x00,
        0x00,
        0x10,
        0xF8,
        0x30,
        0xEF,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF0,
        0x07,
        0x30,
        0xEF,
    },
};

/// Room 1's four point lights and single cone light, with no directional lights.
///
/// Counts cover only the live mutable arrays. The cone storage's trailing bytes
/// are excluded. This collection and its borrowed lights require the loaded overlay.
static WorldCoordRoomLights _gDryfieldNightMotelRoom2RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelRoom2PointLights),
    .pointLights           = _gDryfieldNightMotelRoom2PointLights,
    .coneLightCount        = ARRAY_SIZE(_gDryfieldNightMotelRoom2SpotLightStorage.coneLights),
    .coneLights            = _gDryfieldNightMotelRoom2SpotLightStorage.coneLights,
};

/// Placement-entry keys shared by this room's encounter resource recipes.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_2_SCORPION_ENTRY      = 25,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_BLOOD_SUCKLER_ENTRY = 40,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_SCAVENGER_ENTRY     = 16,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_MOTH_ENTRY          = 8,
};

/// Variant 2's Scorpion and Blood Suckler loading and spawn recipes.
///
/// Entries 25/40 select files 102500/204000 and descriptor 0 of each actor,
/// serving three Scorpion and four Blood Suckler placements. AREA_PLACEMENT_END
/// terminates the list; keep the recipes and actor descriptors loaded through spawning.
static AreaResource _gDryfieldNightMotelRoom2Variant2Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_2_SCORPION_ENTRY, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor102500Tasks },
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_2_BLOOD_SUCKLER_ENTRY, .fileNumber = 40, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor204000BloodSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Blood Suckler loading and spawn recipe for placement entry 40.
///
/// File group base 10 and number 40 select file 104000; descriptor 0 serves
/// six placements. The sentinel terminates this overlay-owned borrowed list.
static AreaResource _gDryfieldNightMotelRoom2Variant3Resources[2] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_2_BLOOD_SUCKLER_ENTRY, .fileNumber = 40, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor04000BloodSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 21's Scavenger and Moth loading and spawn recipes.
///
/// Entries 16/8 select files 101600/200700 and descriptor 0 of each actor,
/// serving two Scavenger and four Moth placements. The Moth's placement key
/// differs from its file number. The sentinel terminates the borrowed list.
static AreaResource _gDryfieldNightMotelRoom2Variant21Resources[3] = {
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_2_SCAVENGER_ENTRY, .fileNumber = 16, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor101600ScavengerTaskDesc },
    { .entryId = DRYFIELD_NIGHT_MOTEL_ROOM_2_MOTH_ENTRY, .fileNumber = 7, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightMotelRoom2AreaVariants[22] = {
    [2]  = { .placements = gMapDryfieldFullMotelRoom2Variant2Placements, .resources = _gDryfieldNightMotelRoom2Variant2Resources },
    [3]  = { .placements = gMapDryfieldFullMotelRoom2Variant3Placements, .resources = _gDryfieldNightMotelRoom2Variant3Resources },
    [21] = { .placements = gMapDryfieldFullMotelRoom2Variant21Placements, .resources = _gDryfieldNightMotelRoom2Variant21Resources },
};

/// Cue-2 bases in the loaded type-1 sound bank; cue 1 selects the next entry.
enum {
    DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Class 1's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps share entries 0x35/0x36; running uses 0x37/0x38
/// in the loaded type-1 bank. Paired animation cues select base/base + 1.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom2Class1FootstepSounds __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_1_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_1_WALK_CUE_BASE,
};

/// Classes 2 and 4's shared walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps share entries 1/2; running uses 3/4 in the loaded
/// type-1 bank. Both classes occur in this room's collision grid.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelRoom2Classes2And4FootstepSounds __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_MOTEL_ROOM_2_CLASS_2_WALK_CUE_BASE,
};

/// Silent, probe-blocking properties shared by surface classes 0, 5, 6 and 7.
///
/// Weapon impacts and pushback remain enabled. All four directory entries
/// borrow this single record from the loaded room overlay.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom2SolidSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking, impact-enabled surface with its own footstep cues.
///
/// Applies pushback and borrows the cue record for the room overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom2Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom2Class1FootstepSounds,
};

/// Class 2's blocking, impact-enabled surface with cues shared by class 4.
///
/// Applies pushback and borrows the cue record for the room overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom2Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom2Classes2And4FootstepSounds,
};

/// Class 3's silent surface, passing probes and ignoring weapon impacts.
///
/// Pushback remains enabled independently of probe and impact policy.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom2Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 4's probe-passing, impact-enabled surface with class 2's footstep cues.
///
/// Pushback remains enabled. The grid assigns this class to two wall faces;
/// their contacts may select these cues even when the floor is another class.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelRoom2Class4SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelRoom2Classes2And4FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom2SurfacePropertiesByClass[8] = {
    &_gDryfieldNightMotelRoom2SolidSurfaceProperties,
    &_gDryfieldNightMotelRoom2Class1SurfaceProperties,
    &_gDryfieldNightMotelRoom2Class2SurfaceProperties,
    &_gDryfieldNightMotelRoom2Class3SurfaceProperties,
    &_gDryfieldNightMotelRoom2Class4SurfaceProperties,
    &_gDryfieldNightMotelRoom2SolidSurfaceProperties,
    &_gDryfieldNightMotelRoom2SolidSurfaceProperties,
    &_gDryfieldNightMotelRoom2SolidSurfaceProperties,
};

static void _dryfieldNightMotelRoom2InitRoomTask(Task* task);

/// Rejects key-item use in night motel room 2 without changing the item or room.
///
/// `itemId` is the collected item ID; zero is returned for every request, so
/// the item menu reports that the item cannot be used here. No arguments are
/// read or retained; the second payload word is unused.
static s32 _dryfieldNightMotelRoom2RejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

#include "../../shared/room/room_variants_main_street.inc.c"

/// Ignores room commands in night motel room 2 and returns zero.
///
/// Neither command payload word is read, and no task or scene state changes.
static s32 _dryfieldNightMotelRoom2IgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction-trigger room actions in night motel room 2.
///
/// The borrowed action request is never accessed or retained. The second
/// payload word is unused, and zero is returned without changing room state.
static s32 _dryfieldNightMotelRoom2IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers night motel room 2's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldNightMotelRoom2InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldNightMotelRoom2RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Leaves the initialized room task idle with its message table active.
///
/// State 1 does no per-frame work and never reads `task` or advances its state;
/// the separate state-2 callback handles teardown.
static void _dryfieldNightMotelRoom2IdleRoomTask(Task* task)
{
}

/// Room-task callbacks for states 0 initialize, 1 idle and 2 release.
///
/// The dispatcher copies all three function pointers and indexes the copy
/// without a bounds check. Callbacks remain valid while this overlay is loaded.
static const TaskFuncTable3 _gDryfieldNightMotelRoom2RoomTaskStates = {
    .funcs = {
        [DRYFIELD_NIGHT_MOTEL_ROOM_2_TASK_STATE_NEW]  = _dryfieldNightMotelRoom2InitRoomTask,
        [DRYFIELD_NIGHT_MOTEL_ROOM_2_TASK_STATE_IDLE] = _dryfieldNightMotelRoom2IdleRoomTask,
        [DRYFIELD_NIGHT_MOTEL_ROOM_2_TASK_STATE_EXIT] = taskKill,
    },
};

void dryfieldNightMotelRoom2RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gDryfieldNightMotelRoom2RoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_flare_clipped.inc.c"

void dryfieldNightMotelRoom2DrawFlaresTask(Task* unusedTask)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_FLARE_PAIR_A    = 2,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_FLARE_PAIR_B    = 3,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_SINGLE_FLARE_A  = 5,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_SINGLE_FLARE_B  = 6,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_PAIR_FLARE_TEXTURE   = 1,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_SINGLE_FLARE_TEXTURE = 2,
        // Screen half-extent is radius scale * 39 / (camera Z / 4), in pixels.
        DRYFIELD_NIGHT_MOTEL_ROOM_2_FIRST_FLARE_RADIUS_SCALE  = 0x200,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_SECOND_FLARE_RADIUS_SCALE = 0x240,
        DRYFIELD_NIGHT_MOTEL_ROOM_2_SINGLE_FLARE_RADIUS_SCALE = 0x180,
    };

    switch (gGameSession->location.view) {
        case DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_FLARE_PAIR_A:
        case DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_FLARE_PAIR_B: {
            const SVECTOR* flarePoints = _gDryfieldNightMotelRoom2PairedFlarePoints;
            glowDrawFlareClipped(&flarePoints[0], DRYFIELD_NIGHT_MOTEL_ROOM_2_PAIR_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_ROOM_2_FIRST_FLARE_RADIUS_SCALE);
            glowDrawFlareClipped(&flarePoints[1], DRYFIELD_NIGHT_MOTEL_ROOM_2_PAIR_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_ROOM_2_SECOND_FLARE_RADIUS_SCALE);
            break;
        }
        case DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_SINGLE_FLARE_A:
        case DRYFIELD_NIGHT_MOTEL_ROOM_2_VIEW_SINGLE_FLARE_B: {
            const SVECTOR* flarePoints = &_gDryfieldNightMotelRoom2SingleFlarePoint;
            glowDrawFlareClipped(&flarePoints[0], DRYFIELD_NIGHT_MOTEL_ROOM_2_SINGLE_FLARE_TEXTURE, DRYFIELD_NIGHT_MOTEL_ROOM_2_SINGLE_FLARE_RADIUS_SCALE);
            break;
        }
    }
}
