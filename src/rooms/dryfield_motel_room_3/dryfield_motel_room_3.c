#include "rooms/dryfield_motel_room_3.h"

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
    DRYFIELD_MOTEL_ROOM_3_REVERSE_VIEW_COUNT       = 6,
    DRYFIELD_MOTEL_ROOM_3_ENTRY_VIEW               = 2,
    DRYFIELD_MOTEL_ROOM_3_ENTRY_YAW                = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4,
    DRYFIELD_MOTEL_ROOM_3_DOOR_MAP_FLAG            = 484,
    DRYFIELD_MOTEL_ROOM_3_DEPARTURE_WARP           = 1,
    DRYFIELD_MOTEL_ROOM_3_MAIN_STREET_ARRIVAL_WARP = 5,
    DRYFIELD_MOTEL_ROOM_3_MAIN_STREET_WARP_PAIR =
        (DRYFIELD_MOTEL_ROOM_3_DEPARTURE_WARP << 4) | DRYFIELD_MOTEL_ROOM_3_MAIN_STREET_ARRIVAL_WARP,
    DRYFIELD_MOTEL_ROOM_3_ARRIVAL_SOUND         = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_3, 2),
    DRYFIELD_MOTEL_ROOM_3_DEPARTURE_SOUND       = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_3, 1),
    DRYFIELD_MOTEL_ROOM_3_CLASS_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_MOTEL_ROOM_3_CLASS_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_MOTEL_ROOM_3_CLASS_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 1,
    DRYFIELD_MOTEL_ROOM_3_CLASS_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 3,
};

static s32 _dryfieldMotelRoom3RefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom3AcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom3IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

static WorldCollisionGrid    _gDryfieldMotelRoom3CollisionGrid;
static WorldCollisionTrigger _gDryfieldMotelRoom3ViewBoundaryTriggers[11];
static WorldCollisionTrigger _gDryfieldMotelRoom3ActionTriggers[1];
static WorldCoordRoomLights  _gDryfieldMotelRoom3RoomLights;

/// Room-message handlers accepting transitions and refusing or ignoring other requests.
///
/// Borrowed by the live room task. Four ID/callback pairs precede the end
/// marker, so unsupported messages return zero. Keep the overlay loaded while
/// the task receives messages; transition dispatch borrows a request and reply.
static TaskMessageEntry _gDryfieldMotelRoom3MessageTable[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom3AcceptRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom3RefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom3IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom3IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gDryfieldMotelRoom3RoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldMotelRoom3CollisionGrid,
        .viewBoundaryTriggers = _gDryfieldMotelRoom3ViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldMotelRoom3ActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldMotelRoom3RoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldMotelRoom3RoomViewCounts[1] = { DRYFIELD_MOTEL_ROOM_3_REVERSE_VIEW_COUNT };

WorldCoordRoomLighting gDryfieldMotelRoom3RoomLighting[1] = {
    { .lights = &_gDryfieldMotelRoom3RoomLights, .ambientTable = NULL },
};

DirectionWarpEntry gDryfieldMotelRoom3WarpEntries[1] = {
    {
        .player         = { { .word = DRYFIELD_MOTEL_ROOM_3_ENTRY_YAW }, 4462, 0, 1067 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = DRYFIELD_MOTEL_ROOM_3_ENTRY_YAW }, 4462, 0, 1067 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_MOTEL_ROOM_3_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_MOTEL_ROOM_3_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_MOTEL_ROOM_3_ENTRY_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_MOTEL_ROOM_3_DOOR_MAP_FLAG,
    },
};

/// Collision-grid normals in room axes, with 4096 units per unit length.
static SVECTOR _gDryfieldMotelRoom3CollisionGridNormals[13] = {
#include "assets/dryfield_motel_room_3_collision_007E0_normals.inc"
};

/// Collision-grid vertices in signed room-coordinate units.
static SVECTOR _gDryfieldMotelRoom3CollisionGridVerts[103] = {
#include "assets/dryfield_motel_room_3_collision_007E0_verts.inc"
};

/// Collision-grid quads selecting room-local surface classes 0..2.
static WorldCollisionGridFace _gDryfieldMotelRoom3CollisionGridFaces[45] = {
#include "assets/dryfield_motel_room_3_collision_007E0_faces.inc"
};

/// Collision-grid cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
///
/// The 80 stored halfwords include values outside the four active lists; their
/// role is unproven and the complete asset storage is retained.
static s16 _gDryfieldMotelRoom3CollisionGridCells[80] = {
#include "assets/dryfield_motel_room_3_collision_007E0_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom3CollisionGridCells[i])
/// Collision-grid X-major directory of the four XZ cell face-index lists.
static s16* _gDryfieldMotelRoom3CollisionGridTable[4] = {
#include "assets/dryfield_motel_room_3_collision_007E0_table.inc"
};
#undef GRID_CELL

/// Motel room 3's collision mesh, indexed by a 2x2 XZ cell grid.
///
/// The grid origin is (200, 100) on XZ; cells span 4000 world units per side.
/// Room setup binds the writable descriptor to the current view coordinate.
/// Its 45 quads, 103 vertices, 13 normals and cell lists live with the overlay.
static WorldCollisionGrid _gDryfieldMotelRoom3CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelRoom3CollisionGridNormals,
    .vertices    = _gDryfieldMotelRoom3CollisionGridVerts,
    .faces       = _gDryfieldMotelRoom3CollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelRoom3CollisionGridTable,
    .xBias       = -200,
    .zBias       = -100,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelRoom3CollisionGridFaces),
};

ViewCamera gDryfieldMotelRoom3ViewCameras[8] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -2500, 0x34BC, -3500 } }, 316 },
    { { { { 654, 0, -4043 }, { 0, ONE, 0 }, { 4043, 0, 654 } }, { -1650, 1050, -700 } }, 275 },
    { { { { 1425, 0, -3840 }, { -296, 4083, -109 }, { 3828, 315, 1420 } }, { -200, 1250, -900 } }, 235 },
    { { { { 3930, 0, -1152 }, { -94, 4082, -324 }, { 1148, 337, 3917 } }, { -1100, 1450, -200 } }, 230 },
    { { { { -3619, 0, 1916 }, { 173, 4079, 327 }, { -1908, 371, -3605 } }, { -3150, 1200, -3450 } }, 275 },
    { { { { 4066, 0, -488 }, { 0, ONE, 0 }, { 488, 0, 4066 } }, { -3800, 950, -2590 } }, 246 },
    { { { { -768, 0, -4023 }, { 313, 4083, -59 }, { 4011, -319, -765 } }, { -200, 750, -6400 } }, 235 },
    { { { { -419, 0, 4074 }, { 2571, 3176, 264 }, { -3160, 2585, -325 } }, { -3200, 2550, -6240 } }, 235 },
};

/// View 1's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's empty sprite list, selecting decoded background strips before its terminator.
static SpriteBatch _gDryfieldMotelRoom3View8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldMotelRoom3SpriteViews[8] = {
    { { .empty = _gDryfieldMotelRoom3View1SpriteBatches }, _gDryfieldMotelRoom3View1SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View2SpriteBatches }, _gDryfieldMotelRoom3View2SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View3SpriteBatches }, _gDryfieldMotelRoom3View3SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View4SpriteBatches }, _gDryfieldMotelRoom3View4SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View5SpriteBatches }, _gDryfieldMotelRoom3View5SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View6SpriteBatches }, _gDryfieldMotelRoom3View6SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View7SpriteBatches }, _gDryfieldMotelRoom3View7SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelRoom3View8SpriteBatches }, _gDryfieldMotelRoom3View8SpriteBatches, NULL },
};

/// Eleven directed boundaries connecting views 2..8 within motel room 3.
///
/// Paired directions join 3/4, 3/5, 4/6, 6/7 and 7/8; the final boundary
/// changes 2 to 3 and carries LAST. Setup binds and links every record to the
/// current view, mutating links, flags and hit latches until the list is retired.
static WorldCollisionTrigger _gDryfieldMotelRoom3ViewBoundaryTriggers[11] = {
    { NULL, NULL, NULL, { 3519, -1136, 2943, 0 }, { { -989, -2160, -373, 0 }, { 990, -2160, 374, 0 }, { -989, 2160, -373, 0 }, { 990, 2160, 374, 0 } }, { 1453, 0, -3857, 0 }, { 0, 0, ONE, 0 }, 2401, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3455, -1104, 2815, 0 }, { { 990, -2128, 374, 0 }, { -989, -2128, -373, 0 }, { 990, 2128, 374, 0 }, { -989, 2128, -373, 0 } }, { -1455, 0, 3849, 0 }, { 0, 0, ONE, 0 }, 2374, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2591, -1104, 862, 0 }, { { 246, -2128, -669, 0 }, { -245, -2128, 670, 0 }, { 246, 2128, -669, 0 }, { -245, 2128, 670, 0 } }, { 3859, 0, 1414, 0 }, { 0, 0, ONE, 0 }, 2231, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2719, -1120, 831, 0 }, { { -278, -2144, 657, 0 }, { 278, -2144, -657, 0 }, { -278, 2144, 657, 0 }, { 278, 2144, -657, 0 } }, { -3775, 0, -1600, 0 }, { 0, 0, ONE, 0 }, 2246, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4094, -1136, 5086, 0 }, { { -829, -2160, -245, 0 }, { 830, -2160, 246, 0 }, { -829, 2160, -245, 0 }, { 830, 2160, 246, 0 } }, { 1160, 0, -3929, 0 }, { 0, 0, ONE, 0 }, 2318, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4063, -1136, 4991, 0 }, { { 830, -2160, 246, 0 }, { -829, -2160, -245, 0 }, { 830, 2160, 246, 0 }, { -829, 2160, -245, 0 } }, { -1163, 0, 3926, 0 }, { 0, 0, ONE, 0 }, 2318, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3199, -1104, 5630, 0 }, { { -6, -2128, -865, 0 }, { 6, -2128, 865, 0 }, { -6, 2128, -865, 0 }, { 6, 2128, 865, 0 } }, { 4095, 0, -30, 0 }, { 0, 0, ONE, 0 }, 2289, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3328, -1152, 5631, 0 }, { { 6, -2176, 865, 0 }, { -6, -2176, -865, 0 }, { 6, 2176, 865, 0 }, { -6, 2176, -865, 0 } }, { -4101, 0, 26, 0 }, { 0, 0, ONE, 0 }, 2332, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2112, -1120, 5840, 0 }, { { -6, -2144, -977, 0 }, { 6, -2144, 977, 0 }, { -6, 2144, -977, 0 }, { 6, 2144, 977, 0 } }, { 4097, 0, -27, 0 }, { 0, 0, ONE, 0 }, 2346, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2272, -1152, 5888, 0 }, { { 6, -2176, 977, 0 }, { -6, -2176, -977, 0 }, { 6, 2176, 977, 0 }, { -6, 2176, -977, 0 } }, { -4122, 0, 23, 0 }, { 0, 0, ONE, 0 }, 2374, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4383, -1120, 1056, 0 }, { { 3, -2160, -885, 0 }, { -2, -2160, 886, 0 }, { 3, 2160, -885, 0 }, { -2, 2160, 886, 0 } }, { 4102, 0, 10, 0 }, { 0, 0, ONE, 0 }, 2332, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// The facing-gated door trigger from motel room 3 to the main street.
///
/// Uses this area's departure endpoint 1 and main-street arrival endpoint 5.
/// Activation requires the interaction button and opposing alignment with -X.
/// LAST includes this sole record in setup's mutable, view-bound action list.
static WorldCollisionTrigger _gDryfieldMotelRoom3ActionTriggers[1] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4528, -48, 1040, 0 },
        .vertices     = { { -368, 0, -816, 0 }, { 368, 0, -816, 0 }, { -368, 0, 816, 0 }, { 368, 0, 816, 0 } },
        .normal       = { 0, 4115, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 893,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_MAIN_STREET,
        .parameter1   = DRYFIELD_MOTEL_ROOM_3_MAIN_STREET_WARP_PAIR,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Four white point lights for motel room 3, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength within `inner` and fades
/// to zero at `outer`. The loaded room overlay owns this mutable array:
/// coordinate updates parent and compose its transforms, and lighting queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldMotelRoom3PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 934, -1735, 6756 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2025, 2025, 2025 },
        },
        .inner = 1140,
        .outer = 5440,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 425, -1236, 2352 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2800, 2800, 2800 },
        },
        .inner = 2444,
        .outer = 7346,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4499, -1500, 5898 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2600, 2600, 2600 },
        },
        .inner = 880,
        .outer = 4011,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -1278, 6899 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2900, 2900, 2900 },
        },
        .inner = 987,
        .outer = 2342,
    },
};

/// Motel room 3's four all-view point lights, with no directional or cone sources.
///
/// Borrows the overlay's mutable point-light array for coordinate composition
/// and attenuation queries. The collection and sources must remain loaded.
static WorldCoordRoomLights _gDryfieldMotelRoom3RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelRoom3PointLights),
    .pointLights           = _gDryfieldMotelRoom3PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Surface class 1's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps use loaded type-1 bank entries 0x35/0x36;
/// running uses 0x37/0x38. Companion requests add 100 to the selected entry.
static WorldCollisionFootstepSounds _gDryfieldMotelRoom3Class1FootstepSounds = {
    .walk         = DRYFIELD_MOTEL_ROOM_3_CLASS_1_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_3_CLASS_1_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_3_CLASS_1_WALK_CUE_BASE,
};

/// Surface class 2's walking, running and scripted-jump footstep cue bases.
///
/// Walking and scripted jumps use loaded type-1 bank entries 1/2; running
/// uses 3/4. Companion requests add 100 to the selected entry.
static WorldCollisionFootstepSounds _gDryfieldMotelRoom3Class2FootstepSounds = {
    .walk         = DRYFIELD_MOTEL_ROOM_3_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_MOTEL_ROOM_3_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_3_CLASS_2_WALK_CUE_BASE,
};

/// Shared silent, blocking, impact-enabled properties for surface classes 0 and 4..7.
///
/// Pushback remains enabled. Five directory entries borrow this single record.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom3SolidSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1's blocking and impact-enabled properties with its footstep cues.
///
/// Applies pushback and borrows cue storage for the loaded overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom3Class1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom3Class1FootstepSounds,
};

/// Surface class 2's blocking and impact-enabled properties with its footstep cues.
///
/// Applies pushback and borrows cue storage for the loaded overlay's lifetime.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom3Class2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom3Class2FootstepSounds,
};

/// Surface class 3's silent properties, passing probes and suppressing weapon impacts.
///
/// Pushback remains enabled. The mesh does not use this retained directory entry.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom3Class3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldMotelRoom3SurfacePropertiesByClass[8] = {
    &_gDryfieldMotelRoom3SolidSurfaceProperties,
    &_gDryfieldMotelRoom3Class1SurfaceProperties,
    &_gDryfieldMotelRoom3Class2SurfaceProperties,
    &_gDryfieldMotelRoom3Class3SurfaceProperties,
    &_gDryfieldMotelRoom3SolidSurfaceProperties,
    &_gDryfieldMotelRoom3SolidSurfaceProperties,
    &_gDryfieldMotelRoom3SolidSurfaceProperties,
    &_gDryfieldMotelRoom3SolidSurfaceProperties,
};

static void _dryfieldMotelRoom3InitRoomTask(Task* task);
static void _dryfieldMotelRoom3IdleRoomTask(Task* unusedTask);

/// Refuses every key-item-use request in the motel room 3.
///
/// Ignores the collected item ID and other arguments. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, leaving the room and inventory unchanged.
static s32 _dryfieldMotelRoom3RefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition from the motel room 3 with its destination unchanged.
///
/// Borrows a readable eight-byte request and writable reply for synchronous
/// dispatch; they may be the same record. Copies the complete request for both
/// query and execution calls, retains neither pointer, and returns 1.
static s32 _dryfieldMotelRoom3AcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_3_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_3_TRANSITION_ALLOWED;
}

/// Ignores room commands in the motel room 3 and returns zero.
///
/// The command word and other arguments are unused; no room state changes.
static s32 _dryfieldMotelRoom3IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores directed room actions in the motel room 3 and returns zero.
///
/// The borrowed action request is neither read nor retained; all arguments
/// are unused and no room state changes.
static s32 _dryfieldMotelRoom3IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Registers motel room 3's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldMotelRoom3InitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldMotelRoom3MessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps motel room 3's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldMotelRoom3IdleRoomTask(Task* unusedTask)
{
}

/// Room-task states 0 initialize, 1 remain available for messages, and 2 release the task.
///
/// Copied whole before unchecked state dispatch in 0..2; all three callback
/// addresses are borrowed from loaded code. There is no terminator.
static const TaskFuncTable3 _gDryfieldMotelRoom3RoomTaskStates = {
    { _dryfieldMotelRoom3InitRoomTask, _dryfieldMotelRoom3IdleRoomTask, taskKill },
};

void dryfieldMotelRoom3Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMotelRoom3RoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
