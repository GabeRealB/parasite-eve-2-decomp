#include "rooms/dryfield_motel_room_5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

enum {
    DRYFIELD_MOTEL_ROOM_5_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_5, 2),
    DRYFIELD_MOTEL_ROOM_5_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_5, 1),
    DRYFIELD_MOTEL_ROOM_5_MAP_FLAG        = 464,
    // High nibble selects this room's departure endpoint 1; low nibble the balcony's arrival endpoint 2.
    DRYFIELD_MOTEL_ROOM_5_BALCONY_WARP_ENDPOINTS = 0x12,
    DRYFIELD_MOTEL_ROOM_5_CLASS_1_WALK_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_MOTEL_ROOM_5_CLASS_1_RUN_CUE_BASE   = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_MOTEL_ROOM_5_CLASS_2_WALK_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_MOTEL_ROOM_5_CLASS_2_RUN_CUE_BASE   = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

static s32 _dryfieldMotelRoom5RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom5ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom5IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldMotelRoom5IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid    _gDryfieldMotelRoom5CollisionGrid;
static WorldCollisionTrigger _gDryfieldMotelRoom5ViewBoundaryTriggers[8];
static WorldCollisionTrigger _gDryfieldMotelRoom5ActionTriggers[1];
static WorldCoordRoomLights  _gDryfieldMotelRoom5RoomLights;

/// Motel room 5's synchronous room-message dispatch table.
///
/// Allows transitions unchanged, refuses key items and ignores room actions
/// and commands. The final marker makes unsupported IDs return zero. The live
/// room task borrows this read-only table and its callbacks while the overlay
/// is loaded. Its authored storage remains in the overlay's data section.
static const TaskMessageEntry _gDryfieldMotelRoom5RoomMessages[] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom5ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom5RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom5IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom5IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gDryfieldMotelRoom5RoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldMotelRoom5CollisionGrid,
        .viewBoundaryTriggers = _gDryfieldMotelRoom5ViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldMotelRoom5ActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldMotelRoom5RoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldMotelRoom5RoomViewCounts[1] = { ARRAY_SIZE(gDryfieldMotelRoom5ViewCameras) };

WorldCoordRoomLighting gDryfieldMotelRoom5RoomLighting[1] = {
    { .lights = &_gDryfieldMotelRoom5RoomLights, .ambientTable = NULL },
};

DirectionWarpEntry gDryfieldMotelRoom5WarpEntries[1] = {
    {
        .player         = { { .word = 3 * ONE / 4 }, 4350, 0, 2426 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3 * ONE / 4 }, 4350, 0, 2426 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_MOTEL_ROOM_5_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_MOTEL_ROOM_5_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_MOTEL_ROOM_5_MAP_FLAG,
    },
};

/// Grid-local unit normals, with 4096 units per unit length.
static SVECTOR _gDryfieldMotelRoom5CollisionGridNormals[12] = {
#include "assets/dryfield_motel_room_5_collision_006DC_normals.inc"
};

/// Collision-mesh vertices in signed grid-local world units.
static SVECTOR _gDryfieldMotelRoom5CollisionGridVerts[85] = {
#include "assets/dryfield_motel_room_5_collision_006DC_verts.inc"
};

/// Room triangles/quads, indexing the vertex/normal pools and surface classes 0..2.
static WorldCollisionGridFace _gDryfieldMotelRoom5CollisionGridFaces[38] = {
#include "assets/dryfield_motel_room_5_collision_006DC_faces.inc"
};

/// Four terminated face-index lists and the asset's retained final halfword.
///
/// Lists begin at elements 0, 30, 52 and 60 and end at 29, 50, 59 and 66.
/// Element 51 is outside those scans; element 67 is retained from the asset.
static s16 _gDryfieldMotelRoom5CollisionGridCells[68] = {
#include "assets/dryfield_motel_room_5_collision_006DC_cells.inc"
};

// Generated cell-table initializers use element offsets into the face-index pool.
#define GRID_CELL(i) (&_gDryfieldMotelRoom5CollisionGridCells[i])
/// X-major 2-by-2 table borrowing the grid's terminated face-index lists.
static s16* _gDryfieldMotelRoom5CollisionGridTable[4] = {
#include "assets/dryfield_motel_room_5_collision_006DC_table.inc"
};
#undef GRID_CELL

/// Motel room 5's collision mesh and 2-by-2 XZ cell index.
///
/// Cells span 4000 world units, starting at X=150 and Z=0. Setup binds the
/// writable descriptor to the current view; its pools remain borrowed until
/// the grid is retired. The complete authored asset contains twelve normals,
/// 85 vertices and 38 faces; cells index only these faces.
static WorldCollisionGrid _gDryfieldMotelRoom5CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelRoom5CollisionGridNormals,
    .vertices    = _gDryfieldMotelRoom5CollisionGridVerts,
    .faces       = _gDryfieldMotelRoom5CollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelRoom5CollisionGridTable,
    .xBias       = -150,
    .zBias       = 0,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelRoom5CollisionGridFaces),
};

ViewCamera gDryfieldMotelRoom5ViewCameras[DRYFIELD_MOTEL_ROOM_5_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 0x2C10, -3000 } }, 289 },
    { { { { 828, 0, -4011 }, { -323, 4082, -66 }, { 3998, 330, 825 } }, { -300, 1280, -900 } }, 235 },
    { { { { 725, 0, 4031 }, { 300, 4084, -54 }, { -4019, 305, 723 } }, { -4800, 1280, -900 } }, 235 },
    { { { { 4029, 0, 732 }, { 75, 4074, -415 }, { -728, 422, 4008 } }, { -4100, 1380, -500 } }, 246 },
    { { { { 4048, 0, -622 }, { -374, 3272, -2434 }, { 497, 2463, 3234 } }, { -800, 2580, -2900 } }, 246 },
    { { { { -4048, 0, 622 }, { 335, 3452, 2178 }, { -524, 2204, -3411 } }, { -1600, 2280, -5800 } }, 246 },
};

/// View 1's empty sprite list: keeps image strips, then terminates without source access.
static SpriteBatch _gDryfieldMotelRoom5View1SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 2's empty sprite list: keeps image strips, then terminates without source access.
static SpriteBatch _gDryfieldMotelRoom5View2SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 3's empty sprite list: keeps image strips, then terminates without source access.
static SpriteBatch _gDryfieldMotelRoom5View3SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 4's empty sprite list: keeps image strips, then terminates without source access.
static SpriteBatch _gDryfieldMotelRoom5View4SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 5's empty sprite list: keeps image strips, then terminates without source access.
static SpriteBatch _gDryfieldMotelRoom5View5SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 6's empty sprite list: keeps image strips, then terminates without source access.
static SpriteBatch _gDryfieldMotelRoom5View6SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldMotelRoom5SpriteViews[DRYFIELD_MOTEL_ROOM_5_VIEW_COUNT] = {
    { { .empty = _gDryfieldMotelRoom5View1SpriteBatches }, _gDryfieldMotelRoom5View1SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom5View2SpriteBatches }, _gDryfieldMotelRoom5View2SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom5View3SpriteBatches }, _gDryfieldMotelRoom5View3SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom5View4SpriteBatches }, _gDryfieldMotelRoom5View4SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom5View5SpriteBatches }, _gDryfieldMotelRoom5View5SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom5View6SpriteBatches }, _gDryfieldMotelRoom5View6SpriteBatches, NULL },
};

/// Directed camera boundaries joining views 2/3, 2/4, 4/5 and 5/6 in both directions.
///
/// Views use 1-based IDs; view 1 has no boundary. Setup binds coordinates and
/// links/enables all eight records, including the LAST-marked final entry.
/// Keep this mutable array loaded until its runtime links are cleared.
static WorldCollisionTrigger _gDryfieldMotelRoom5ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 2384, -1296, 1392, 0 }, { { 112, -1936, -1360, 0 }, { -112, -1936, 1360, 0 }, { 112, 1936, -1360, 0 }, { -112, 1936, 1360, 0 } }, { 4092, 0, 335, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2496, -1120, 1408, 0 }, { { -112, -2144, 1360, 0 }, { 112, -2144, -1360, 0 }, { -112, 2144, 1360, 0 }, { 112, 2144, -1360, 0 } }, { -4092, 0, -338, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3663, -1120, 3199, 0 }, { { 1449, -2144, 275, 0 }, { -1451, -2144, -277, 0 }, { 1449, 2144, 275, 0 }, { -1451, 2144, -277, 0 } }, { -768, 0, 4032, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3711, -1104, 3295, 0 }, { { -1568, -2128, -307, 0 }, { 1566, -2128, 305, 0 }, { -1568, 2128, -307, 0 }, { 1566, 2128, 305, 0 } }, { 783, 0, -4021, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2206, -1104, 4927, 0 }, { { -43, -2128, -1006, 0 }, { 43, -2128, 1006, 0 }, { -43, 2128, -1006, 0 }, { 43, 2128, 1006, 0 } }, { 4115, 0, -178, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2302, -1072, 5023, 0 }, { { 43, -2096, 1006, 0 }, { -43, -2096, -1006, 0 }, { 43, 2096, 1006, 0 }, { -43, 2096, -1006, 0 } }, { -4120, 0, 176, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1183, -1072, 4446, 0 }, { { -1093, -2096, 109, 0 }, { 1093, -2096, -109, 0 }, { -1093, 2096, 109, 0 }, { 1093, 2096, -109, 0 } }, { -409, 0, -4081, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1184, -1072, 4320, 0 }, { { 1093, -2096, -109, 0 }, { -1093, -2096, 109, 0 }, { 1093, 2096, -109, 0 }, { -1093, 2096, 109, 0 } }, { 406, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Motel room 5's facing-tested warp to the balcony in Dryfield area 29.
///
/// Departure uses endpoint 1; destination is balcony endpoint 2. The sole
/// LAST-marked record is linked and enabled by room setup, which also binds
/// its coordinate. Keep the mutable list loaded until unlinked.
static WorldCollisionTrigger _gDryfieldMotelRoom5ActionTriggers[1] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4368, 0, 2272, 0 },
        .vertices     = { { -432, 0, -800, 0 }, { 432, 0, -800, 0 }, { -432, 0, 800, 0 }, { 432, 0, 800, 0 } },
        .normal       = { 0, 4112, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 907,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_MOTEL_BALCONY,
        .parameter1   = DRYFIELD_MOTEL_ROOM_5_BALCONY_WARP_ENDPOINTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Twelve point lights for motel room 5, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength through `inner` and fades
/// with squared distance to zero at `outer`. The loaded room overlay owns the
/// mutable array: coordinate updates parent and compose its transforms, and
/// lighting queries overwrite attenuation. Borrowed pointers must not outlive
/// the overlay.
static WorldCoordPointLight _gDryfieldMotelRoom5PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10247, -936, -1738 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3280, 3076, 2870 },
        },
        .inner = 1587,
        .outer = 2185,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8192, -2555, -1604 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2460, 2460, 2460 },
        },
        .inner = 1120,
        .outer = 1672,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6380, -2555, -1625 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2460, 2460, 2460 },
        },
        .inner = 1057,
        .outer = 1718,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10564, -1003, -1164 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 1839,
        .outer = 2505,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 490, -1975, -479 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, 3072 },
        },
        .inner = 1107,
        .outer = 1294,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 169, -2006, -1625 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, 3076 },
        },
        .inner = 1112,
        .outer = 1374,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -911, -2035, -2681 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, 3072 },
        },
        .inner = 1288,
        .outer = 1572,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1618, -1020, -130 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2870, 2870, 2870 },
        },
        .inner = 2591,
        .outer = 3272,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3750, -1440, -3 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1229, 1229, 1229 },
        },
        .inner = 2029,
        .outer = 2673,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6865, -1440, 10 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1229, 1229, 1229 },
        },
        .inner = 1935,
        .outer = 2601,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6885, -1440, -2420 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2666, 2666, 2663 },
        },
        .inner = 2646,
        .outer = 3283,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3665, -1440, -2318 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3280, 3280, 3280 },
        },
        .inner = 2164,
        .outer = 2832,
    },
};

/// Room light collection containing twelve point lights and no directional or cone lights.
///
/// The borrowed point-light array remains writable for composition and shading.
/// Collection and lights belong to this overlay and must stay loaded while used.
static WorldCoordRoomLights _gDryfieldMotelRoom5RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelRoom5PointLights),
    .pointLights           = _gDryfieldMotelRoom5PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Surface class 1's paired walking, running and scripted-jump cue bases.
///
/// Requests the loaded type-1 bank: walking/jumping use entries 0x35/0x36,
/// running 0x37/0x38. Read-only authored storage remains in the data section.
static const WorldCollisionFootstepSounds _gDryfieldMotelRoom5Class1FootstepSounds __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_MOTEL_ROOM_5_CLASS_1_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_5_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_5_CLASS_1_WALK_CUE_BASE,
};

/// Surface class 2's configured walking, running and scripted-jump cue bases.
///
/// Requests the loaded type-1 bank: walking/jumping use entries 1/2, running
/// 3/4. These entries are absent from this daytime room's bank, so the sound
/// driver rejects the requests. Read-only authored storage stays in the data section.
static const WorldCollisionFootstepSounds _gDryfieldMotelRoom5Class2FootstepSounds __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_MOTEL_ROOM_5_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_5_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_5_CLASS_2_WALK_CUE_BASE,
};

/// Classes 0 and 4..7 block probes, allow weapon impacts and apply pushback without footsteps.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom5DefaultSurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1 blocks probes, allows weapon impacts and applies pushback with its footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom5Class1SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom5Class1FootstepSounds,
};

/// Class 2 blocks probes, allows weapon impacts and applies pushback with configured footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom5Class2SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom5Class2FootstepSounds,
};

/// Class 3 passes probes and ignores weapon impacts, but applies pushback without footsteps.
///
/// Retained surface-directory entry; the authored mesh has no class-3 face.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom5Class3SurfaceProperties = {
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldMotelRoom5SurfacePropertiesByClass[DRYFIELD_MOTEL_ROOM_5_SURFACE_CLASS_COUNT] = {
    &_gDryfieldMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldMotelRoom5Class1SurfaceProperties,
    &_gDryfieldMotelRoom5Class2SurfaceProperties,
    &_gDryfieldMotelRoom5Class3SurfaceProperties,
    &_gDryfieldMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldMotelRoom5DefaultSurfaceProperties,
    &_gDryfieldMotelRoom5DefaultSurfaceProperties,
};

static void _dryfieldMotelRoom5InitRoomTask(Task* task);
static void _dryfieldMotelRoom5IdleRoomTask(Task* unusedTask);

/// Refuses every key-item use request in Dryfield motel room 5.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldMotelRoom5RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with its requested destination unchanged.
///
/// Copies the complete eight-byte record and returns 1 for query and execution
/// requests alike, with no transition side effects. The non-null records are
/// borrowed through synchronous dispatch; `reply` must be writable and may be
/// the same object as `request`. Neither pointer is retained.
static s32 _dryfieldMotelRoom5ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_5_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_5_TRANSITION_ALLOWED;
}

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldMotelRoom5IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores room actions requested by direction triggers.
///
/// The borrowed request is neither read nor retained. The sender supplies zero
/// for the second payload and ignores the result, which is always zero.
static s32 _dryfieldMotelRoom5IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers motel room 5's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldMotelRoom5InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldMotelRoom5RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps motel room 5's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldMotelRoom5IdleRoomTask(Task* unusedTask)
{
}

/// Motel room 5 receiver states: 0 installs messages, 1 idles, 2 releases the task.
///
/// The dispatcher copies the full three-callback value before unchecked state
/// indexing. Both the borrowed task and this overlay must remain live through dispatch.
static const TaskFuncTable3 _gDryfieldMotelRoom5RoomStateHandlers = {
    { _dryfieldMotelRoom5InitRoomTask, _dryfieldMotelRoom5IdleRoomTask, taskKill },
};

void dryfieldMotelRoom5Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMotelRoom5RoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
