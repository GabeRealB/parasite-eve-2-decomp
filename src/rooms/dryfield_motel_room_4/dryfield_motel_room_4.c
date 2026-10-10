#include "rooms/dryfield_motel_room_4.h"

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

static s32 _dryfieldMotelRoom4RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldMotelRoom4ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelRoom4IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument);
static s32 _dryfieldMotelRoom4IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid    _gDryfieldMotelRoom4CollisionGrid;
static WorldCollisionTrigger _gDryfieldMotelRoom4ViewBoundaryTriggers[8];
static WorldCollisionTrigger _gDryfieldMotelRoom4ExitTriggers[1];
static WorldCoordRoomLights  _gDryfieldMotelRoom4RoomLights;

/// Endpoint settings and the packed departure/arrival slots of the Main Street exit.
enum {
    DRYFIELD_MOTEL_ROOM_4_INITIAL_VIEW           = 2,
    DRYFIELD_MOTEL_ROOM_4_MAP_FLAG_ID            = 483,
    DRYFIELD_MOTEL_ROOM_4_MAIN_STREET_WARP_SLOTS = (1 << 4) | 6,
    DRYFIELD_MOTEL_ROOM_4_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_4, 2),
    DRYFIELD_MOTEL_ROOM_4_DEPARTURE_SOUND        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_ROOM_4, 1),
};

/// Room messages allowing transitions, refusing key items and ignoring commands/actions.
///
/// Four handlers followed by an end marker; unsupported IDs return zero.
/// The live room task borrows this immutable table until it is released, so
/// the overlay must stay loaded. The section preserves its stored data placement.
static const TaskMessageEntry _gDryfieldMotelRoom4Messages[5] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelRoom4ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelRoom4RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelRoom4IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelRoom4IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gDryfieldMotelRoom4RoomCollisionResources[1] = {
    { &_gDryfieldMotelRoom4CollisionGrid, _gDryfieldMotelRoom4ViewBoundaryTriggers, _gDryfieldMotelRoom4ExitTriggers, NULL },
};

u8* gDryfieldMotelRoom4RoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldMotelRoom4RoomViewCounts[1] = { ARRAY_SIZE(gDryfieldMotelRoom4ViewCameras) };

WorldCoordRoomLighting gDryfieldMotelRoom4RoomLighting[1] = {
    { &_gDryfieldMotelRoom4RoomLights, NULL },
};

DirectionWarpEntry gDryfieldMotelRoom4WarpEntries[1] = {
    {
        .player         = { { .word = 0 }, 3400, 0, 675 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 3400, 0, 675 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_MOTEL_ROOM_4_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_MOTEL_ROOM_4_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_MOTEL_ROOM_4_INITIAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_MOTEL_ROOM_4_MAP_FLAG_ID,
    },
};

/// Collision grid's fourteen room-space normals, with 4096 representing unit length.
static SVECTOR _gDryfieldMotelRoom4CollisionGridNormals[] = {
#include "assets/dryfield_motel_room_4_collision_00830_normals.inc"
};

/// Collision grid's 103 vertices in signed room-coordinate units.
static SVECTOR _gDryfieldMotelRoom4CollisionGridVerts[] = {
#include "assets/dryfield_motel_room_4_collision_00830_verts.inc"
};

/// Collision grid's 47 indexed quads, selecting room surface classes 0..2.
static WorldCollisionGridFace _gDryfieldMotelRoom4CollisionGridFaces[] = {
#include "assets/dryfield_motel_room_4_collision_00830_faces.inc"
};

/// Collision grid's packed face-index lists, each terminated by -1.
///
/// The 104 stored halfwords retain unused values between lists. Cell-table
/// offsets select the four list starts; all live indices address the face array.
static s16 _gDryfieldMotelRoom4CollisionGridCells[] = {
#include "assets/dryfield_motel_room_4_collision_00830_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom4CollisionGridCells[i])
/// Collision grid's X-major 2-by-2 directory of borrowed face-index lists.
static s16* _gDryfieldMotelRoom4CollisionGridTable[] = {
#include "assets/dryfield_motel_room_4_collision_00830_table.inc"
};
#undef GRID_CELL

/// Motel room 4's collision mesh and four-cell spatial index.
///
/// The X/Z origin is (200, 200), with square cells of 4000 game units.
/// Binding writes the view coordinate; the record and its mesh/list arrays
/// remain borrowed and writable while this overlay is loaded.
static WorldCollisionGrid _gDryfieldMotelRoom4CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelRoom4CollisionGridNormals,
    .vertices    = _gDryfieldMotelRoom4CollisionGridVerts,
    .faces       = _gDryfieldMotelRoom4CollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelRoom4CollisionGridTable,
    .xBias       = -200,
    .zBias       = -200,
    .cellCountX  = 2,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelRoom4CollisionGridFaces),
};

ViewCamera gDryfieldMotelRoom4ViewCameras[6] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3500, 10500, -2500 } }, 289 },
    { { { { 652, 0, 4043 }, { 208, 4090, -33 }, { -4038, 211, 651 } }, { -6800, 1250, -950 } }, 275 },
    { { { { 3984, 0, -951 }, { -39, 4092, -166 }, { 950, 171, 3980 } }, { -1200, 1450, -200 } }, 240 },
    { { { { 660, 0, -4042 }, { -57, 4095, -9 }, { 4042, 58, 660 } }, { -1400, 1250, -1050 } }, 275 },
    { { { { 209, 0, 4090 }, { 1972, 3588, -100 }, { -3583, 1975, 183 } }, { -6800, 2170, -3550 } }, 263 },
    { { { { 866, 0, -4003 }, { -172, 4092, -37 }, { 3999, 176, 865 } }, { -2200, 1160, -3120 } }, 269 },
};

/// View 1's zero-count batch and end marker, selecting decoded background strips.
static SpriteBatch _gDryfieldMotelRoom4View1Batches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's zero-count batch and end marker, selecting decoded background strips.
static SpriteBatch _gDryfieldMotelRoom4View2Batches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's zero-count batch and end marker, selecting decoded background strips.
static SpriteBatch _gDryfieldMotelRoom4View3Batches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's zero-count batch and end marker, selecting decoded background strips.
static SpriteBatch _gDryfieldMotelRoom4View4Batches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's zero-count batch and end marker, selecting decoded background strips.
static SpriteBatch _gDryfieldMotelRoom4View5Batches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's zero-count batch and end marker, selecting decoded background strips.
static SpriteBatch _gDryfieldMotelRoom4View6Batches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldMotelRoom4SpriteViews[6] = {
    { { .empty = _gDryfieldMotelRoom4View1Batches }, _gDryfieldMotelRoom4View1Batches, NULL },
    { { .empty = _gDryfieldMotelRoom4View2Batches }, _gDryfieldMotelRoom4View2Batches, NULL },
    { { .empty = _gDryfieldMotelRoom4View3Batches }, _gDryfieldMotelRoom4View3Batches, NULL },
    { { .empty = _gDryfieldMotelRoom4View4Batches }, _gDryfieldMotelRoom4View4Batches, NULL },
    { { .empty = _gDryfieldMotelRoom4View5Batches }, _gDryfieldMotelRoom4View5Batches, NULL },
    { { .empty = _gDryfieldMotelRoom4View6Batches }, _gDryfieldMotelRoom4View6Batches, NULL },
};

/// Directed camera boundaries connecting views 2/3, 2/4, 3/5 and 5/6 in both directions.
///
/// The LAST-marked eighth element is included. Room setup binds the view
/// coordinate and mutates links, flags and hit latches; keep the array loaded
/// and writable until all its triggers are unlinked.
static WorldCollisionTrigger _gDryfieldMotelRoom4ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 1872, -1136, 2736, 0 }, { { -1808, -2160, 16, 0 }, { 1808, -2160, -16, 0 }, { -1808, 2160, 16, 0 }, { 1808, 2160, -16, 0 } }, { -37, 0, -4105, 0 }, { 0, 0, ONE, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1888, -1088, 2656, 0 }, { { 1808, -2112, -16, 0 }, { -1808, -2112, 16, 0 }, { 1808, 2112, -16, 0 }, { -1808, 2112, 16, 0 } }, { 36, 0, ONE, 0 }, { 0, 0, ONE, 0 }, 2769, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4511, -1152, 1519, 0 }, { { -17, -2176, -1316, 0 }, { 14, -2176, 1313, 0 }, { -17, 2176, -1316, 0 }, { 14, 2176, 1313, 0 } }, { 4095, 0, -49, 0 }, { 0, 0, ONE, 0 }, 2534, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4639, -1120, 1471, 0 }, { { 15, -2144, 1314, 0 }, { -16, -2144, -1315, 0 }, { 15, 2144, 1314, 0 }, { -16, 2144, -1315, 0 } }, { -4107, 0, 47, 0 }, { 0, 0, ONE, 0 }, 2508, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3647, -1120, 3502, 0 }, { { 0, -2144, -738, 0 }, { 0, -2144, 738, 0 }, { 0, 2144, -738, 0 }, { 0, 2144, 738, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2260, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3776, -1136, 3488, 0 }, { { 0, -2160, 738, 0 }, { 0, -2160, -738, 0 }, { 0, 2160, 738, 0 }, { 0, 2160, -738, 0 } }, { -4109, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2275, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5456, -1088, 3840, 0 }, { { -112, -2160, 1026, 0 }, { 112, -2160, -1026, 0 }, { -112, 2160, 1026, 0 }, { 112, 2160, -1026, 0 } }, { -4082, 0, -447, 0 }, { 0, 0, ONE, 0 }, 2387, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5344, -1088, 3808, 0 }, { { 112, -2160, -1026, 0 }, { -112, -2160, 1026, 0 }, { 112, 2160, -1026, 0 }, { -112, 2160, 1026, 0 } }, { 4079, 0, 444, 0 }, { 0, 0, ONE, 0 }, 2387, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Facing-gated exit to Main Street, using departure endpoint 1 and arrival endpoint 6.
///
/// The sole element carries LAST and requires the interaction button. Setup
/// binds its coordinate and list links; collision updates its hit latch.
/// Keep it writable and loaded until it is unlinked.
static WorldCollisionTrigger _gDryfieldMotelRoom4ExitTriggers[1] = {
    { NULL, NULL, NULL, { 3584, -48, 512, 0 }, { { -832, 0, -320, 0 }, { 832, 0, -320, 0 }, { -832, 0, 320, 0 }, { 832, 0, 320, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 891, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MAIN_STREET, DRYFIELD_MOTEL_ROOM_4_MAIN_STREET_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Point lights contributing in every view of Dryfield motel room 4.
///
/// Positions and falloff radii are in integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is 1.0). The room-light collection borrows this
/// writable array for the lifetime of the loaded room overlay. Runtime updates
/// attach the transforms to the view coordinate and compose their matrices;
/// lighting queries overwrite each light's attenuation.
static WorldCoordPointLight _gDryfieldMotelRoom4PointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5369, -1319, 5201 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2700, 2700, 2700 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2100,
        .outer = 4442,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4280, -1466, 1405 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 4500, 4500, 4500 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1020,
        .outer = 2601,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1051, -1619, 2995 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2600, 2600, 2600 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2485,
        .outer = 3743,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1025, -1772, -171 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5440, 5440, 5440 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2140,
        .outer = 3071,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5111, -1523, 416 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3600, 3600, 3600 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 788,
        .outer = 1130,
    },
};

/// The room's five all-view point lights, with no directional or cone lights.
///
/// Borrows the point-light array while the overlay is loaded; coordinate
/// updates and shading queries mutate the lights' transforms and attenuation.
static WorldCoordRoomLights _gDryfieldMotelRoom4RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelRoom4PointLights),
    .pointLights           = _gDryfieldMotelRoom4PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Cue-2 bases in the loaded type-1 footstep bank; cue 1 uses the next entry.
enum {
    DRYFIELD_MOTEL_ROOM_4_SURFACE_1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_MOTEL_ROOM_4_SURFACE_1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_MOTEL_ROOM_4_SURFACE_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 1,
    DRYFIELD_MOTEL_ROOM_4_SURFACE_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 3,
};

/// Surface class 1's footstep requests: walking/stairs use 0x35, running uses 0x37.
///
/// Cue 1 adds one and companion playback adds 100 in the loaded type-1 bank.
/// The surface borrows this immutable record while the room overlay is loaded;
/// its data section preserves the stored placement.
static const WorldCollisionFootstepSounds _gDryfieldMotelRoom4Surface1Footsteps __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_MOTEL_ROOM_4_SURFACE_1_WALK_SOUND,
    .run          = DRYFIELD_MOTEL_ROOM_4_SURFACE_1_RUN_SOUND,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_4_SURFACE_1_WALK_SOUND,
};

/// Surface class 2's footstep requests: walking/stairs use entry 1, running uses 3.
///
/// Cue 1 adds one and companion playback adds 100 in the loaded type-1 bank.
/// The surface borrows this immutable record while the room overlay is loaded;
/// its data section preserves the stored placement.
static const WorldCollisionFootstepSounds _gDryfieldMotelRoom4Surface2Footsteps __attribute__((section(".data"))) = {
    .walk         = DRYFIELD_MOTEL_ROOM_4_SURFACE_2_WALK_SOUND,
    .run          = DRYFIELD_MOTEL_ROOM_4_SURFACE_2_RUN_SOUND,
    .scriptedJump = DRYFIELD_MOTEL_ROOM_4_SURFACE_2_WALK_SOUND,
};

/// Shared policy for classes 0 and 4..7: blocking, weapon impacts, pushback and no footsteps.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom4BlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1's blocking, impact-enabled policy with pushback and its footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom4Surface1 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom4Surface1Footsteps,
};

/// Surface class 2's blocking, impact-enabled policy with pushback and its footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom4Surface2 = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelRoom4Surface2Footsteps,
};

/// Surface class 3's probe-passing policy, with pushback and no weapon impacts or footsteps.
static WorldCollisionSurfaceProperties _gDryfieldMotelRoom4ProbePassSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldMotelRoom4SurfacePropertiesByClass[8] = {
    &_gDryfieldMotelRoom4BlockingSurface,
    &_gDryfieldMotelRoom4Surface1,
    &_gDryfieldMotelRoom4Surface2,
    &_gDryfieldMotelRoom4ProbePassSurface,
    &_gDryfieldMotelRoom4BlockingSurface,
    &_gDryfieldMotelRoom4BlockingSurface,
    &_gDryfieldMotelRoom4BlockingSurface,
    &_gDryfieldMotelRoom4BlockingSurface,
};

/// Refuses every key-item use request in Dryfield motel room 4.
///
/// Ignores the selected collected-item ID and returns `ROOM_KEY_ITEM_USE_REFUSED`
/// for the item menu's refusal notice, without consuming an item or starting an event.
static s32 _dryfieldMotelRoom4RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with its requested destination unchanged.
///
/// Copies the complete eight-byte record and returns 1 for query and execution
/// requests alike, with no transition side effects. The non-null records are
/// borrowed through synchronous dispatch; `reply` must be writable and may be
/// the same object as `request`. Neither pointer is retained.
static s32 _dryfieldMotelRoom4ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_ROOM_4_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_ROOM_4_TRANSITION_ALLOWED;
}

/// Ignores room commands without starting an action.
///
/// Both command words are unused; the signed message result is always zero.
static s32 _dryfieldMotelRoom4IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandArgument)
{
    return 0;
}

/// Ignores room actions requested by direction triggers.
///
/// The borrowed request is neither read nor retained. The sender supplies zero
/// for the second payload and ignores the result, which is always zero.
static s32 _dryfieldMotelRoom4IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and its message handlers, then enters its idle state.
///
/// Runs at state 0 and advances to state 1. The room slot borrows `task`, and
/// the task borrows the overlay's message table; both must stay live for dispatch.
static void _dryfieldMotelRoom4InitTask(Task* task)
{
    task->msgTable = _gDryfieldMotelRoom4Messages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room task available for messages without doing per-frame work.
///
/// State 1 leaves the live task and its state unchanged.
static void _dryfieldMotelRoom4IdleTask(Task* unusedTask)
{
}

/// Room task handlers indexed by state: 0 initialize, 1 idle, 2 release the task.
///
/// Dispatch copies all three callback pointers by value before selecting one;
/// callers must supply a state in 0..2 and keep the overlay loaded.
static const TaskFuncTable3 _gDryfieldMotelRoom4TaskStates = {
    { _dryfieldMotelRoom4InitTask, _dryfieldMotelRoom4IdleTask, taskKill },
};

void dryfieldMotelRoom4Task(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMotelRoom4TaskStates;
    stateHandlers.funcs[task->state](task);
}
