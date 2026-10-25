#include "rooms/neo_ark_pavilion.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00400.h"
#include "actors/actor_01100.h"
#include "actors/actor_03800.h"
#include "actors/actor_05700.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
/// Binds the shared water-spray definition to this package's public void (Task*)
/// callback. Define before including its fragment; the room header declares it.
#define WATER_SPRAY_TASK neoArkPavilionWaterSprayTask
/// Binds the cached-coordinate ripple fragment to this package's public
/// void (Task*) callback. Define before its fragment; the room header declares
/// the export that gameplay's effect table imports. Takes no macro arguments.
#define WATER_RIPPLE_CACHED_COORD_TASK neoArkPavilionWaterRippleTask
#include "../../shared/effects/water_effects.h"
#include "../../shared/room/room_events.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The save-location record and event the message handler latched for the
/// room's event task.
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static s32 _neoArkPavilionRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedMessageArg);
static s32 _neoArkPavilionResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _neoArkPavilionHandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg);
static s32 _neoArkPavilionIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedMessageArg);

static WorldCollisionGrid     _gNeoArkPavilionCollisionGrid;
static WorldCollisionOccluder _gNeoArkPavilionOccluders[1];
static WorldCollisionTrigger  _gNeoArkPavilionViewBoundaryTriggers[2];
static WorldCollisionTrigger  _gNeoArkPavilionRoom1ActionTriggers[4];
static WorldCollisionTrigger  _gNeoArkPavilionRoom2ActionTriggers[4];
static WorldCoordRoomLights   _gNeoArkPavilionRoomLights;

TaskDesc gNeoArkPavilionWaterRefractionTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterRefractionTask, { .value = 0 } };

/// Retained bodyless, priority-192 water-band distortion recipe without a caller.
///
/// Its callback draws only submarine-tunnel and submarine-gallery views;
/// pavilion setup spawns the separate refraction recipe at index 0.
static TaskDesc _gNeoArkPavilionWaterDistortionTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, waterDistortBandTask, { .value = 0 } };

/// Bodyless, priority-32 controller for the latched pavilion departure scene.
///
/// Spawn index 0 with zero arguments after storing the destination and event.
/// The snapshots, CAP resources and room overlay remain live through reload.
static TaskDesc _gNeoArkPavilionStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Room-message dispatch for transition resolution, key items, actions and CAP commands.
///
/// The room task borrows this end-ID/null-terminated table while this overlay
/// is loaded. Key-item and room-action requests are refused with a zero reply.
static TaskMessageEntry _gNeoArkPavilionRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkPavilionResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkPavilionRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkPavilionIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkPavilionHandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

WorldCollisionRoomResources gNeoArkPavilionRoomCollisionResources[2] = {
    { &_gNeoArkPavilionCollisionGrid, _gNeoArkPavilionViewBoundaryTriggers, _gNeoArkPavilionRoom1ActionTriggers, _gNeoArkPavilionOccluders },
    { &_gNeoArkPavilionCollisionGrid, _gNeoArkPavilionViewBoundaryTriggers, _gNeoArkPavilionRoom2ActionTriggers, _gNeoArkPavilionOccluders },
};

WorldCoordRoomLighting gNeoArkPavilionRoomLighting[2] = {
    { &_gNeoArkPavilionRoomLights, NULL },
    { &_gNeoArkPavilionRoomLights, NULL },
};

/// Room 2's seven logical-view mappings followed by one retained zero byte.
///
/// Indexed by logical view minus one; the eighth byte is outside the room's
/// seven-view search count and is not a terminator.
static u8 _gNeoArkPavilionRoom2ViewMap[8] = {
    1,
    4,
    5,
    2,
    3,
    6,
    7,
    0,
};

u8* gNeoArkPavilionViewMaps[2] = {
    gViewIdentityMap,
    _gNeoArkPavilionRoom2ViewMap,
};

/// Logical slots searched in each pavilion room's view map, excluding retained bytes.
enum { NEO_ARK_PAVILION_LOGICAL_VIEW_COUNT = 7 };

ViewCount gNeoArkPavilionViewCounts[2] = {
    NEO_ARK_PAVILION_LOGICAL_VIEW_COUNT,
    NEO_ARK_PAVILION_LOGICAL_VIEW_COUNT,
};

/// Pavilion area-bank requests used by the bridge and island warp endpoints.
enum {
    NEO_ARK_PAVILION_WARP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 4),
    NEO_ARK_PAVILION_WARP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PAVILION, 3)
};

DirectionWarpEntry gNeoArkPavilionWarpEntries[3] = {
    {
        .player         = { { .word = 3072 }, 2000, 0, 9504 },
        .companion      = { { .word = 3072 }, 2000, 0, 9504 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 0 }, -2700, 0, 7650 },
        .companion      = { { .word = 0 }, -2700, 0, 7650 },
        .arrivalSound   = NEO_ARK_PAVILION_WARP_ARRIVAL_SOUND,
        .departureSound = NEO_ARK_PAVILION_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 0 }, 3000, 0, 7500 },
        .companion      = { { .word = 0 }, 3000, 0, 7500 },
        .arrivalSound   = NEO_ARK_PAVILION_WARP_ARRIVAL_SOUND,
        .departureSound = NEO_ARK_PAVILION_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

s16 gNeoArkPavilionDiverWaterY = 300;

/// Each Bog Diver patrol cursor addresses a ring modulo eight.
enum { NEO_ARK_PAVILION_DIVER_PATROL_WAYPOINT_COUNT = 8 };

/// Eight-point patrol ring for selectors 0 and 3, 1500 units below the water surface.
///
/// X/Z are room coordinates; Y is added to `gNeoArkPavilionDiverWaterY`.
static SVECTOR _gNeoArkPavilionDiverPatrolWaypoints0[NEO_ARK_PAVILION_DIVER_PATROL_WAYPOINT_COUNT] = {
    { 3700, 1500, 5900, 0 },
    { -1600, 1500, 6000, 0 },
    { -6400, 1500, 6000, 0 },
    { -6200, 1500, 0x29CC, 0 },
    { -3900, 1500, 0x3458, 0 },
    { 4000, 1500, 0x332C, 0 },
    { 7328, 1500, 0x2BC0, 0 },
    { 8000, 1500, 5800, 0 },
};

/// Eight-point patrol ring for selector 1, 3000 units below the water surface.
///
/// X/Z are room coordinates; Y is added to `gNeoArkPavilionDiverWaterY`.
static SVECTOR _gNeoArkPavilionDiverPatrolWaypoints1[NEO_ARK_PAVILION_DIVER_PATROL_WAYPOINT_COUNT] = {
    { -6400, 3000, 0x35E8, 0 },
    { -6400, 3000, 0x2710, 0 },
    { -8000, 3000, 5000, 0 },
    { -9400, 3000, 0x2AF8, 0 },
    { -5000, 3000, 0x38A4, 0 },
    { 7000, 3000, 0x34BC, 0 },
    { -0x34A0, 3000, 7136, 0 },
    { 6700, 3000, 0x3DB8, 0 },
};

/// Eight-point patrol ring for selector 2, 4500 units below the water surface.
///
/// X/Z are room coordinates; Y is added to `gNeoArkPavilionDiverWaterY`.
static SVECTOR _gNeoArkPavilionDiverPatrolWaypoints2[NEO_ARK_PAVILION_DIVER_PATROL_WAYPOINT_COUNT] = {
    { -7000, 4500, 3300, 0 },
    { -0x2710, 4500, 7100, 0 },
    { -8600, 4500, 0x3520, 0 },
    { -8000, 4500, 9000, 0 },
    { -6800, 4500, 8500, 0 },
    { -6400, 4500, 3000, 0 },
    { -8100, 4500, 3800, 0 },
    { -0x2B5C, 4500, 5000, 0 },
};

SVECTOR* gNeoArkPavilionDiverPatrolRings[4] = {
    _gNeoArkPavilionDiverPatrolWaypoints0,
    _gNeoArkPavilionDiverPatrolWaypoints1,
    _gNeoArkPavilionDiverPatrolWaypoints2,
    _gNeoArkPavilionDiverPatrolWaypoints0,
};

/// Initial claim state and list terminator for the writable Diver emergence spots.
enum {
    NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE = 0,
    NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_END  = -1
};

SVECTOR gNeoArkPavilionDiverSurfaceSpots[8] = {
    { 0, 0, 0, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { 6600, 0, 4000, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { -6000, 0, 7000, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { -6000, 0, 0x2710, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { -4000, 0, 0x32C8, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { 5800, 0, 0x32C8, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { 6600, 0, 5800, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_FREE },
    { 0, 0, 0, NEO_ARK_PAVILION_DIVER_SURFACE_SPOT_END },
};

/// Room-space normals for the pavilion collision grid, with 4096 per unit.
static SVECTOR _gNeoArkPavilionCollisionGridNormals[8] = {
#include "assets/neo_ark_pavilion_collision_06C24_normals.inc"
};

/// Signed room-coordinate vertices for the pavilion collision grid.
static SVECTOR _gNeoArkPavilionCollisionGridVerts[102] = {
#include "assets/neo_ark_pavilion_collision_06C24_verts.inc"
};

/// Indexed pavilion collision quads using room-local surface classes 0, 1 and 4.
static WorldCollisionGridFace _gNeoArkPavilionCollisionGridFaces[52] = {
#include "assets/neo_ark_pavilion_collision_06C24_faces.inc"
};

/// Face-index lists for the six pavilion cells, each ending with -1.
///
/// The complete authored storage includes unreferenced zeros at entries 69,
/// 97 and 155; these are outside the terminated lists.
static s16 _gNeoArkPavilionCollisionGridCells[156] = {
#include "assets/neo_ark_pavilion_collision_06C24_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkPavilionCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_PAVILION_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkPavilionCollisionGridCells[(halfwordOffset)])
/// X-major table of the grid's 3 by 2 cell lists.
static s16* _gNeoArkPavilionCollisionGridTable[6] = {
#include "assets/neo_ark_pavilion_collision_06C24_table.inc"
};
#undef NEO_ARK_PAVILION_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by both pavilion rooms, with 4000-unit square XZ cells.
///
/// The cell origin is (-5100, 6900). Activation borrows the current view
/// coordinate; the record and its mutable mesh pools live with this overlay.
static WorldCollisionGrid _gNeoArkPavilionCollisionGrid = {
    NULL,
    _gNeoArkPavilionCollisionGridNormals,
    _gNeoArkPavilionCollisionGridVerts,
    _gNeoArkPavilionCollisionGridFaces,
    _gNeoArkPavilionCollisionGridTable,
    5100,
    -6900,
    3,
    2,
    4000,
    ARRAY_SIZE(_gNeoArkPavilionCollisionGridFaces),
};

ViewCamera gNeoArkPavilionViewCameras[7] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x444B, -9500 } }, 329 },
    { { { { -619, 0, -4048 }, { -1181, 3917, 180 }, { 3872, 1195, -592 } }, { 3570, 2500, -0x280A } }, 257 },
    { { { { -703, 0, 4035 }, { 1103, 3939, 192 }, { -3881, 1120, -677 } }, { -3560, 2500, -0x28EB } }, 257 },
    { { { { -619, 0, -4048 }, { -1181, 3917, 180 }, { 3872, 1195, -592 } }, { 3570, 2500, -0x280A } }, 257 },
    { { { { -703, 0, 4035 }, { 1103, 3939, 192 }, { -3881, 1120, -677 } }, { -3560, 2500, -0x28EB } }, 257 },
    { { { { 0, 0, -ONE }, { -680, 4039, 0 }, { 4039, 680, 0 } }, { -2925, 905, -0x2B52 } }, 329 },
    { { { { -4064, 0, 507 }, { 197, 3773, 1580 }, { -467, 1592, -3744 } }, { -4725, 2460, -5305 } }, 289 },
};

/// Neutral texture-colour modulation in the pavilion's source sprites.
enum { NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR = 128 };

/// Mapped view 1's image-only batch list, with no source sprites.
///
/// The zero-count first record preserves image strips; the following record
/// terminates the list. Its address also occupies the view's empty source slot.
static SpriteBatch _gNeoArkPavilionView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 2's 149 depth-sorted texture rectangles over the decoded image.
///
/// XY is in pixels relative to the view centre; dimensions are pixel counts.
/// Depth is an unscaled sorting value. Texture addresses and authored RGB/code bytes are
/// retained in writable room-overlay storage.
static SpriteSource _gNeoArkPavilionView2SpriteSources[149] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -8, 2183, { .fields = { 16, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 8, 1125, { .fields = { 104, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 8, 1125, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 48, 1125, { .fields = { 48, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 975, { .fields = { 8, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -72, 1125, { .fields = { 80, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 755, { .fields = { 16, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, -112, 978, { .fields = { 120, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -72, 810, { .fields = { 64, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -144, -64, 825, { .fields = { 112, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -136, -64, 829, { .fields = { 80, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, -104, 6299, { .fields = { 104, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, -112, 1816, { .fields = { 64, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -104, 1950, { .fields = { 56, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -80, 1968, { .fields = { 80, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -88, 1959, { .fields = { 96, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -96, 1950, { .fields = { 80, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, -40, 2067, { .fields = { 40, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, -32, 2100, { .fields = { 24, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -96, 7554, { .fields = { 80, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -88, 2099, { .fields = { 88, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -104, 5189, { .fields = { 48, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -48, 2225, { .fields = { 32, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -104, 2030, { .fields = { 56, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 64, -104, 1856, { .fields = { 96, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 72, -104, 1804, { .fields = { 88, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -112, 9551, { .fields = { 80, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -104, 1099, { .fields = { 16, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -72, 1250, { .fields = { 56, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1250, { .fields = { 120, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, -8, 1250, { .fields = { 80, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, -8, 1250, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 1187, { .fields = { 80, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 1088, { .fields = { 40, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -160, 48, 1125, { .fields = { 48, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -152, 24, 1125, { .fields = { 88, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 1125, { .fields = { 72, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 32, 1087, { .fields = { 40, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 32, 1162, { .fields = { 24, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -120, 24, 1350, { .fields = { 112, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 8, 1475, { .fields = { 0, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, 0, 1687, { .fields = { 104, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -24, 2000, { .fields = { 0, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -40, 2062, { .fields = { 120, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -40, 2062, { .fields = { 120, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, -32, 2062, { .fields = { 120, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 2500, { .fields = { 80, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -16, 2500, { .fields = { 112, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, -32, 2500, { .fields = { 112, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -32, 1875, { .fields = { 0, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -32, 2062, { .fields = { 32, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 1750, { .fields = { 112, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -24, 1625, { .fields = { 8, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1562, { .fields = { 104, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -16, 1500, { .fields = { 8, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 104, -8, 1425, { .fields = { 112, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 112, 0, 1375, { .fields = { 112, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 0, 1312, { .fields = { 112, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 8, 1225, { .fields = { 40, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1250, { .fields = { 56, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 16, 1250, { .fields = { 48, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 32, 1250, { .fields = { 24, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -56, 1650, { .fields = { 112, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -40, -72, 1280, { .fields = { 120, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -32, -24, 2437, { .fields = { 64, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, -24, 2437, { .fields = { 24, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, -24, 2392, { .fields = { 32, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -32, 1862, { .fields = { 96, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 16, -72, 1675, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 24, -72, 1475, { .fields = { 120, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -64, 1325, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -40, 1875, { .fields = { 72, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -48, 1875, { .fields = { 72, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 1875, { .fields = { 96, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -80, 1212, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -80, 1212, { .fields = { 80, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 1500, { .fields = { 96, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -87, -7, 2250, { .fields = { 96, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 9, -7, 2330, { .fields = { 32, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -95, 1, 2187, { .fields = { 24, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 1, 1, 2299, { .fields = { 16, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -103, 9, 2077, { .fields = { 16, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 1, 9, 2144, { .fields = { 8, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -111, 17, 2000, { .fields = { 40, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 1, 17, 1993, { .fields = { 24, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -111, 25, 1849, { .fields = { 32, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -7, 25, 1861, { .fields = { 24, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -119, 33, 1740, { .fields = { 40, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -39, 33, 1676, { .fields = { 80, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 41, 33, 1740, { .fields = { 56, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -127, 41, 1649, { .fields = { 56, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -39, 41, 1617, { .fields = { 64, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 49, 41, 1617, { .fields = { 8, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -127, 49, 1517, { .fields = { 120, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -39, 49, 1517, { .fields = { 120, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 49, 49, 1517, { .fields = { 120, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -135, 57, 1458, { .fields = { 112, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -39, 57, 1426, { .fields = { 112, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 57, 57, 1426, { .fields = { 112, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -135, 65, 1399, { .fields = { 112, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -39, 65, 1399, { .fields = { 112, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 65, 65, 1399, { .fields = { 120, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -143, 73, 1317, { .fields = { 112, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -39, 73, 1285, { .fields = { 104, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 73, 73, 1285, { .fields = { 8, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -151, 81, 1283, { .fields = { 96, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -39, 81, 1283, { .fields = { 96, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 81, 1283, { .fields = { 120, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -151, 89, 1224, { .fields = { 96, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -39, 89, 1224, { .fields = { 96, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 89, 1224, { .fields = { 112, 8 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -159, 97, 1154, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 128, 8 } }, -55, 97, 1154, { .fields = { 80, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 97, 1154, { .fields = { 120, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -159, 105, 1085, { .fields = { 104, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 128, 8 } }, -55, 105, 1085, { .fields = { 80, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 105, 1085, { .fields = { 120, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 73, 113, 860, { .fields = { 120, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 128, 8 } }, -55, 113, 887, { .fields = { 80, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -159, 113, 887, { .fields = { 16, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -40, 3750, { .fields = { 24, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -40, 3750, { .fields = { 16, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 3750, { .fields = { 104, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -48, 3750, { .fields = { 96, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, -48, 3750, { .fields = { 96, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, -48, 3750, { .fields = { 104, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 3750, { .fields = { 96, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -48, 3750, { .fields = { 80, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -48, 3750, { .fields = { 80, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -48, 3750, { .fields = { 88, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -48, 3750, { .fields = { 88, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, -48, 3750, { .fields = { 88, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 136, -24, 3000, { .fields = { 88, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 144, -32, 3000, { .fields = { 120, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -40, 3000, { .fields = { 48, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -40, -64, 1500, { .fields = { 88, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -32, -72, 1500, { .fields = { 72, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -24, -80, 1848, { .fields = { 72, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, -16, -80, 1500, { .fields = { 88, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -80, 1500, { .fields = { 32, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -80, 1500, { .fields = { 40, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 24, -64, 1500, { .fields = { 96, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, 32, -56, 1375, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 8, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -8, -24, 2571, { .fields = { 8, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -24, 1694, { .fields = { 16, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -24, 1875, { .fields = { 16, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -72, 1662, { .fields = { 8, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -72, 1572, { .fields = { 16, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Mapped view 2's source ranges, covering all 149 sprites and ending at SPRITE_BATCH_END.
///
/// The leading zero-count record preserves the decoded background image.
/// Ranges remain writable for visibility and packet-selection changes.
static SpriteBatch _gNeoArkPavilionView2SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 3, 0 } },
    { 32, 30, 0, 0, { 0, 0 } },
    { 62, 15, 0, 0, { 5, 0 } },
    { 77, 43, 0, 0, { 1, 0 } },
    { 120, 15, 0, 0, { 4, 0 } },
    { 135, ARRAY_SIZE(_gNeoArkPavilionView2SpriteSources) - 135, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 3's 97 depth-sorted texture rectangles over the decoded image.
///
/// XY is in pixels relative to the view centre; dimensions are pixel counts.
/// Depth is an unscaled sorting value. Texture addresses and authored RGB/code bytes are
/// retained in writable room-overlay storage.
static SpriteSource _gNeoArkPavilionView3SpriteSources[97] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -55, -8, 2285, { .fields = { 24, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -71, 0, 2250, { .fields = { 104, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 9, 0, 2250, { .fields = { 96, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -79, 8, 2000, { .fields = { 96, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 9, 8, 2000, { .fields = { 96, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -87, 16, 1875, { .fields = { 96, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 1, 16, 1875, { .fields = { 72, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -103, 24, 1700, { .fields = { 104, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -31, 24, 1700, { .fields = { 112, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 33, 24, 1700, { .fields = { 112, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -111, 32, 1575, { .fields = { 112, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -39, 32, 1575, { .fields = { 104, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 41, 32, 1575, { .fields = { 112, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -127, 40, 1475, { .fields = { 112, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -47, 40, 1475, { .fields = { 104, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 41, 40, 1475, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -135, 48, 1375, { .fields = { 104, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -47, 48, 1375, { .fields = { 104, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 41, 48, 1375, { .fields = { 112, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -151, 56, 1300, { .fields = { 88, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -55, 56, 1300, { .fields = { 88, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 41, 56, 1300, { .fields = { 96, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -151, 64, 1225, { .fields = { 88, 8 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -55, 64, 1225, { .fields = { 88, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 41, 64, 1225, { .fields = { 96, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 72, 1150, { .fields = { 72, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -63, 72, 1150, { .fields = { 120, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 49, 72, 1150, { .fields = { 16, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 80, 1100, { .fields = { 8, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 80, 1100, { .fields = { 0, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 49, 80, 1100, { .fields = { 16, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 88, 1075, { .fields = { 0, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -63, 88, 1075, { .fields = { 112, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 49, 88, 1075, { .fields = { 8, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 96, 1050, { .fields = { 8, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -63, 96, 1050, { .fields = { 120, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 49, 96, 1050, { .fields = { 16, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 104, 1000, { .fields = { 56, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 104, 1000, { .fields = { 24, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 49, 104, 1000, { .fields = { 56, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 49, 112, 950, { .fields = { 64, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 112, 950, { .fields = { 48, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -159, 112, 950, { .fields = { 24, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, -8, 2125, { .fields = { 16, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 0, 2000, { .fields = { 16, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 8, 2125, { .fields = { 16, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -160, 16, 1750, { .fields = { 24, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -160, 24, 1625, { .fields = { 32, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 32, 1663, { .fields = { 8, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 40, 1101, { .fields = { 48, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -40, 2150, { .fields = { 56, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -40, 2134, { .fields = { 64, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 64, 1163, { .fields = { 96, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 88, 1125, { .fields = { 64, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 40, 1125, { .fields = { 120, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 56, 1125, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 0, 1125, { .fields = { 88, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, -48, 812, { .fields = { 40, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 144, -48, 1125, { .fields = { 120, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 40, 1125, { .fields = { 112, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 16, 876, { .fields = { 96, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -48, 1125, { .fields = { 104, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -40, 2079, { .fields = { 80, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -48, 1913, { .fields = { 104, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -48, 4604, { .fields = { 72, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -48, 2245, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -160, -16, 1187, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 24, 1187, { .fields = { 80, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 819, { .fields = { 104, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 56, 1125, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, 32, 1125, { .fields = { 120, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 32, 1125, { .fields = { 112, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 1176, { .fields = { 96, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 32, 1284, { .fields = { 80, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 24, 1386, { .fields = { 72, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 8, 1592, { .fields = { 72, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1870, { .fields = { 96, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -24, 2187, { .fields = { 72, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -8, 2187, { .fields = { 32, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -24, 2187, { .fields = { 48, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -24, -8, 2187, { .fields = { 120, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -24, 2187, { .fields = { 56, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -32, 2187, { .fields = { 64, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -24, 2187, { .fields = { 64, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -32, 2078, { .fields = { 80, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 1964, { .fields = { 72, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -24, 1821, { .fields = { 72, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -16, 1726, { .fields = { 88, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, -16, 1687, { .fields = { 88, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -16, 1687, { .fields = { 88, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -16, 1452, { .fields = { 64, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 0, 1394, { .fields = { 56, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 0, 1307, { .fields = { 56, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1277, { .fields = { 64, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 8, 1196, { .fields = { 56, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -152, 16, 1187, { .fields = { 88, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 56, 1187, { .fields = { 112, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 3's source ranges, covering all 97 sprites and ending at SPRITE_BATCH_END.
///
/// The leading zero-count record preserves the decoded background image.
/// Ranges remain writable for visibility and packet-selection changes.
static SpriteBatch _gNeoArkPavilionView3SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 43, 0, 0, { 3, 0 } },
    { 43, 9, 0, 0, { 0, 0 } },
    { 52, 16, 0, 0, { 2, 0 } },
    { 68, ARRAY_SIZE(_gNeoArkPavilionView3SpriteSources) - 68, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 4's 157 depth-sorted texture rectangles over the decoded image.
///
/// XY is in pixels relative to the view centre; dimensions are pixel counts.
/// Depth is an unscaled sorting value. Texture addresses and authored RGB/code bytes are
/// retained in writable room-overlay storage.
static SpriteSource _gNeoArkPavilionView4SpriteSources[157] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -160, 8, 1125, { .fields = { 112, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, 8, 1125, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 48, 1125, { .fields = { 32, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 1125, { .fields = { 64, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -160, -72, 1125, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -152, -72, 813, { .fields = { 96, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, -64, 829, { .fields = { 88, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -120, 755, { .fields = { 16, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -152, -112, 978, { .fields = { 104, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, -104, 6299, { .fields = { 104, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, -112, 1816, { .fields = { 104, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -104, 1950, { .fields = { 72, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, -88, 1962, { .fields = { 96, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, -96, 1950, { .fields = { 72, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -40, 2058, { .fields = { 24, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, -32, 2074, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -8, 2158, { .fields = { 0, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 16, -96, 7554, { .fields = { 72, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -88, 2099, { .fields = { 88, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -104, 2066, { .fields = { 80, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -56, 2225, { .fields = { 80, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -104, 2030, { .fields = { 80, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -104, 1854, { .fields = { 88, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, -112, 9551, { .fields = { 80, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -104, 1099, { .fields = { 16, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -72, 1250, { .fields = { 72, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, -8, 1250, { .fields = { 96, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, -8, 1250, { .fields = { 88, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 136, 32, 1250, { .fields = { 112, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 1187, { .fields = { 112, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 1087, { .fields = { 96, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -160, 48, 1125, { .fields = { 72, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -152, 24, 1125, { .fields = { 96, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 1125, { .fields = { 88, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, 32, 1086, { .fields = { 40, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 32, 1171, { .fields = { 32, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -120, 24, 1347, { .fields = { 88, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, 8, 1465, { .fields = { 112, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, 0, 1686, { .fields = { 96, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, -24, 1950, { .fields = { 120, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -40, 2000, { .fields = { 0, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -40, 2000, { .fields = { 120, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, -32, 2000, { .fields = { 112, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 2500, { .fields = { 80, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -16, 2500, { .fields = { 64, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, -32, 2500, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -32, 2125, { .fields = { 112, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, -32, 2091, { .fields = { 0, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -24, 2000, { .fields = { 8, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -24, 1875, { .fields = { 8, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1543, { .fields = { 120, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -16, 1856, { .fields = { 56, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 104, -8, 1771, { .fields = { 120, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, -8, 2000, { .fields = { 0, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, -8, 2000, { .fields = { 0, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, -16, 2000, { .fields = { 56, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, -16, 2000, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, -16, 2000, { .fields = { 64, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 152, -16, 2000, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, -8, 2250, { .fields = { 32, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, -8, 2330, { .fields = { 24, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -96, 0, 2187, { .fields = { 16, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 0, 0, 2299, { .fields = { 0, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 8, 2077, { .fields = { 0, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 0, 8, 2144, { .fields = { 120, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -112, 16, 2000, { .fields = { 0, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 0, 16, 1993, { .fields = { 104, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -112, 24, 1849, { .fields = { 120, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -32, 24, 1717, { .fields = { 8, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 40, 24, 1861, { .fields = { 16, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -120, 32, 1740, { .fields = { 120, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -32, 32, 1676, { .fields = { 16, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 40, 32, 1740, { .fields = { 8, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -128, 40, 1649, { .fields = { 120, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -40, 40, 1617, { .fields = { 120, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 48, 40, 1553, { .fields = { 0, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -128, 48, 1517, { .fields = { 120, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -40, 48, 1517, { .fields = { 112, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 56, 48, 1517, { .fields = { 0, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -136, 56, 1458, { .fields = { 120, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -48, 56, 1426, { .fields = { 96, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 56, 56, 1426, { .fields = { 40, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -136, 64, 1399, { .fields = { 48, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -48, 64, 1399, { .fields = { 40, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, 56, 64, 1399, { .fields = { 40, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -144, 72, 1317, { .fields = { 48, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -48, 72, 1285, { .fields = { 24, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, 56, 72, 1285, { .fields = { 24, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -152, 80, 1283, { .fields = { 24, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -48, 80, 1283, { .fields = { 16, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 80, 1283, { .fields = { 32, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -152, 88, 1224, { .fields = { 96, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -48, 88, 1224, { .fields = { 88, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 88, 1224, { .fields = { 104, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -160, 96, 1154, { .fields = { 96, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -56, 96, 1154, { .fields = { 80, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 96, 1154, { .fields = { 104, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -160, 104, 1085, { .fields = { 88, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -56, 104, 1085, { .fields = { 40, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 104, 1085, { .fields = { 56, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -160, 112, 887, { .fields = { 96, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 120, 8 } }, -56, 112, 887, { .fields = { 72, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 64, 112, 887, { .fields = { 96, 8 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -56, 1654, { .fields = { 120, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -8, 1330, { .fields = { 112, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -72, 1242, { .fields = { 64, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, -72, 1224, { .fields = { 104, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -24, -80, 1218, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -80, 1219, { .fields = { 16, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -80, 1241, { .fields = { 72, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -16, 1377, { .fields = { 48, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -72, 1483, { .fields = { 56, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -64, 1286, { .fields = { 16, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -16, 1499, { .fields = { 64, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -40, 1933, { .fields = { 104, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -24, -48, 1849, { .fields = { 72, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -48, 1874, { .fields = { 72, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -32, -16, 2336, { .fields = { 24, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -24, 2447, { .fields = { 56, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -24, 2447, { .fields = { 40, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -8, -24, 2419, { .fields = { 48, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 0, -32, 2267, { .fields = { 72, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -32, 1862, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 16, -56, 1646, { .fields = { 120, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -16, 1756, { .fields = { 24, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -40, 3625, { .fields = { 112, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -40, 3625, { .fields = { 32, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 3625, { .fields = { 48, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -48, 3625, { .fields = { 96, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, -48, 3625, { .fields = { 96, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, -48, 3625, { .fields = { 88, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, -48, 3625, { .fields = { 80, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -48, 3625, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -48, 3625, { .fields = { 72, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -48, 3625, { .fields = { 80, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, -48, 3625, { .fields = { 80, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, -48, 3625, { .fields = { 96, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -24, 3125, { .fields = { 8, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 144, -32, 3125, { .fields = { 120, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -40, 3125, { .fields = { 48, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 32, -56, 1335, { .fields = { 104, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 24, -64, 1471, { .fields = { 120, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -72, 1572, { .fields = { 8, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -72, 1662, { .fields = { 16, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 0, -80, 1629, { .fields = { 16, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -8, -80, 1650, { .fields = { 32, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -16, -80, 1646, { .fields = { 32, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, -80, 1674, { .fields = { 40, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -32, -72, 1671, { .fields = { 24, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -40, -64, 1859, { .fields = { 88, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -32, -24, 2491, { .fields = { 40, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 24 } }, -24, -24, 2452, { .fields = { 88, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 40 } }, -16, -32, 2452, { .fields = { 112, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -32, 2556, { .fields = { 48, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -32, 2257, { .fields = { 40, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 8, -24, 1862, { .fields = { 8, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 16, -24, 1694, { .fields = { 32, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Mapped view 4's source ranges, covering all 157 sprites and ending at SPRITE_BATCH_END.
///
/// The leading zero-count record preserves the decoded background image.
/// Ranges remain writable for visibility and packet-selection changes.
static SpriteBatch _gNeoArkPavilionView4SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 3, 0 } },
    { 29, 30, 0, 0, { 0, 0 } },
    { 59, 44, 0, 0, { 5, 0 } },
    { 103, 22, 0, 0, { 1, 0 } },
    { 125, 15, 0, 0, { 4, 0 } },
    { 140, ARRAY_SIZE(_gNeoArkPavilionView4SpriteSources) - 140, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5's 115 depth-sorted texture rectangles over the decoded image.
///
/// XY is in pixels relative to the view centre; dimensions are pixel counts.
/// Depth is an unscaled sorting value. Texture addresses and authored RGB/code bytes are
/// retained in writable room-overlay storage.
static SpriteSource _gNeoArkPavilionView5SpriteSources[115] = {
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -56, -8, 2287, { .fields = { 32, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, 0, 2250, { .fields = { 112, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 8, 0, 2250, { .fields = { 104, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -80, 8, 2000, { .fields = { 104, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 8, 8, 2000, { .fields = { 104, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -88, 16, 1875, { .fields = { 96, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 8, 16, 1875, { .fields = { 104, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -104, 24, 1700, { .fields = { 0, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -32, 24, 1700, { .fields = { 120, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 48, 24, 1700, { .fields = { 16, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -112, 32, 1575, { .fields = { 8, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -40, 32, 1575, { .fields = { 64, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 40, 32, 1575, { .fields = { 72, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -128, 40, 1475, { .fields = { 64, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -48, 40, 1475, { .fields = { 48, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 48, 40, 1475, { .fields = { 72, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -136, 48, 1375, { .fields = { 64, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -56, 48, 1375, { .fields = { 40, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 48, 48, 1375, { .fields = { 80, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -152, 56, 1300, { .fields = { 64, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -64, 56, 1300, { .fields = { 48, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 40, 56, 1300, { .fields = { 64, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -152, 64, 1225, { .fields = { 64, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -64, 64, 1225, { .fields = { 48, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 40, 64, 1225, { .fields = { 64, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 72, 1150, { .fields = { 64, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 72, 1150, { .fields = { 48, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 32, 72, 1150, { .fields = { 48, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 80, 1100, { .fields = { 72, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 80, 1100, { .fields = { 48, 8 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 32, 80, 1100, { .fields = { 48, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 88, 1075, { .fields = { 64, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 88, 1075, { .fields = { 56, 200 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, 32, 88, 1075, { .fields = { 48, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 96, 1050, { .fields = { 64, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 96, 1050, { .fields = { 48, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, 32, 96, 1050, { .fields = { 48, 24 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 104, 1000, { .fields = { 72, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 104, 1000, { .fields = { 56, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 32, 104, 1000, { .fields = { 32, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, 32, 112, 950, { .fields = { 32, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -72, 112, 950, { .fields = { 40, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 112, 950, { .fields = { 24, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -96, 1835, { .fields = { 120, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -96, 1634, { .fields = { 56, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 1775, { .fields = { 48, 40 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 879, { .fields = { 96, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -16, 1250, { .fields = { 48, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -160, 24, 1250, { .fields = { 112, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -152, 24, 1250, { .fields = { 104, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 64, 1250, { .fields = { 32, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 1104, { .fields = { 48, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, -104, 1172, { .fields = { 32, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -96, 1903, { .fields = { 32, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -104, 8645, { .fields = { 72, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -56, 4822, { .fields = { 112, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -96, 2141, { .fields = { 72, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -40, 2271, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, -88, 2118, { .fields = { 24, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -96, 2072, { .fields = { 32, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -96, 0x27C8, { .fields = { 16, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -88, 1962, { .fields = { 16, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 8694, { .fields = { 32, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -40, 2079, { .fields = { 72, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -40, 1917, { .fields = { 80, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, -104, 1938, { .fields = { 104, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -104, 1743, { .fields = { 56, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -112, 7828, { .fields = { 48, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -120, 7779, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -64, 809, { .fields = { 32, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -64, 785, { .fields = { 32, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, -64, 1125, { .fields = { 96, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, 88, 1125, { .fields = { 88, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 40, 1125, { .fields = { 120, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 0, 1125, { .fields = { 88, 56 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 56, 1125, { .fields = { 96, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 56, 1125, { .fields = { 80, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 0, 1125, { .fields = { 80, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, 0, 1125, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -32, 2148, { .fields = { 48, 248 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -32, 2983, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -40, 2251, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -24, 2051, { .fields = { 24, 184 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -152, 16, 1250, { .fields = { 72, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 56, 1250, { .fields = { 48, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 8, 1196, { .fields = { 24, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 8, 1277, { .fields = { 32, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 0, 1307, { .fields = { 24, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 0, 1394, { .fields = { 24, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -16, 1452, { .fields = { 80, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, -16, 1625, { .fields = { 64, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -16, 1625, { .fields = { 64, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -16, 1726, { .fields = { 40, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -24, 1821, { .fields = { 56, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 2125, { .fields = { 72, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -32, 2125, { .fields = { 64, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -32, 2275, { .fields = { 48, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -24, 2259, { .fields = { 40, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 2245, { .fields = { 0, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -8, 2228, { .fields = { 16, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 2983, { .fields = { 112, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -8, 2741, { .fields = { 16, 136 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -8, 2155, { .fields = { 24, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -8, 2741, { .fields = { 64, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -24, 2107, { .fields = { 40, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -24, 2098, { .fields = { 56, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -24, 1870, { .fields = { 88, 168 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 8, 1592, { .fields = { 40, 152 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 24, 1386, { .fields = { 40, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 40, 1176, { .fields = { 88, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 32, 1284, { .fields = { 64, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 32, 1125, { .fields = { 120, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, 32, 1125, { .fields = { 120, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 56, 1125, { .fields = { 112, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 819, { .fields = { 96, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 5's source ranges, covering all 115 sprites and ending at SPRITE_BATCH_END.
///
/// The leading zero-count record preserves the decoded background image.
/// Ranges remain writable for visibility and packet-selection changes.
static SpriteBatch _gNeoArkPavilionView5SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 43, 0, 0, { 3, 0 } },
    { 43, 36, 0, 0, { 0, 0 } },
    { 79, 4, 0, 0, { 2, 0 } },
    { 83, ARRAY_SIZE(_gNeoArkPavilionView5SpriteSources) - 83, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's 43 depth-sorted texture rectangles over the decoded image.
///
/// XY is in pixels relative to the view centre; dimensions are pixel counts.
/// Depth is an unscaled sorting value. Texture addresses and authored RGB/code bytes are
/// retained in writable room-overlay storage.
static SpriteSource _gNeoArkPavilionView6SpriteSources[43] = {
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -160, -56, 375, { .fields = { 80, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -160, 32, 375, { .fields = { 48, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -144, -56, 375, { .fields = { 64, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -144, 32, 375, { .fields = { 32, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -128, -56, 375, { .fields = { 48, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -128, 32, 375, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -112, -56, 375, { .fields = { 96, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -112, 32, 375, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -56, 375, { .fields = { 80, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -32, 375, { .fields = { 96, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -96, 32, 375, { .fields = { 32, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -80, 32, 375, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -80, -56, 375, { .fields = { 96, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -64, -56, 375, { .fields = { 112, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -64, 32, 375, { .fields = { 16, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -48, -56, 375, { .fields = { 0, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 16, 88 } }, -48, 32, 375, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -32, -56, 375, { .fields = { 0, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -32, 32, 375, { .fields = { 16, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -56, 375, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, 32, 375, { .fields = { 80, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -56, 375, { .fields = { 48, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, 32, 375, { .fields = { 64, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -56, 375, { .fields = { 80, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, 32, 375, { .fields = { 112, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -56, 375, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, 32, 375, { .fields = { 96, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 48, -56, 375, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 48, 32, 375, { .fields = { 48, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -56, 375, { .fields = { 96, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -24, 375, { .fields = { 64, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 64, 32, 375, { .fields = { 112, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -56, 375, { .fields = { 0, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 80, 32, 375, { .fields = { 96, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, -56, 375, { .fields = { 56, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -24, 375, { .fields = { 80, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 96, 32, 375, { .fields = { 112, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, -56, 375, { .fields = { 0, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, 32, 375, { .fields = { 32, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 128, -56, 375, { .fields = { 32, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 128, 32, 375, { .fields = { 16, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 144, -40, 375, { .fields = { 112, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 144, 32, 456, { .fields = { 16, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 6's source ranges, covering all 43 sprites and ending at SPRITE_BATCH_END.
///
/// The leading zero-count record preserves the decoded background image.
/// Ranges remain writable for visibility and packet-selection changes.
static SpriteBatch _gNeoArkPavilionView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPavilionView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 7's 48 depth-sorted texture rectangles over the decoded image.
///
/// XY is in pixels relative to the view centre; dimensions are pixel counts.
/// Depth is an unscaled sorting value. Texture addresses and authored RGB/code bytes are
/// retained in writable room-overlay storage.
static SpriteSource _gNeoArkPavilionView7SpriteSources[48] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, -96, 1125, { .fields = { 112, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -88, 1125, { .fields = { 104, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -120, -40, 1000, { .fields = { 112, 120 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -160, 80, 625, { .fields = { 48, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -160, 64, 625, { .fields = { 64, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -96, 64, 625, { .fields = { 56, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 625, { .fields = { 96, 72 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -160, 48, 625, { .fields = { 64, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -96, 48, 625, { .fields = { 56, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -24, 48, 625, { .fields = { 40, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -160, 32, 625, { .fields = { 56, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -88, 32, 625, { .fields = { 48, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -8, 32, 625, { .fields = { 48, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, -160, 16, 750, { .fields = { 48, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -80, 16, 750, { .fields = { 40, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 104, 16 } }, 8, 16, 750, { .fields = { 24, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -160, 0, 750, { .fields = { 40, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 88, 16 } }, -72, 0, 750, { .fields = { 40, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 96, 16 } }, 16, 0, 750, { .fields = { 32, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -160, -16, 875, { .fields = { 104, 32 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -72, -16, 875, { .fields = { 104, 0 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, 16, -16, 875, { .fields = { 80, 128 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, 72, -80, 1125, { .fields = { 96, 48 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -88, 1125, { .fields = { 8, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -64, 1000, { .fields = { 112, 16 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -48, 1000, { .fields = { 120, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 96, -32, 875, { .fields = { 120, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -16, 750, { .fields = { 8, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 750, { .fields = { 0, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -160, -32, 875, { .fields = { 88, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -72, -32, 875, { .fields = { 88, 144 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 80, 16 } }, 16, -32, 875, { .fields = { 48, 240 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -48, 1000, { .fields = { 0, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -112, -48, 1000, { .fields = { 88, 192 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -32, -48, 1000, { .fields = { 104, 176 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 32, -48, 1000, { .fields = { 104, 160 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -152, -56, 1000, { .fields = { 120, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -64, 1000, { .fields = { 0, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, -72, 1000, { .fields = { 112, 96 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -112, -56, 1000, { .fields = { 64, 104 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -40, -56, 1000, { .fields = { 72, 208 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 24, -56, 1000, { .fields = { 80, 64 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -72, -64, 1000, { .fields = { 56, 88 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -8, -64, 1000, { .fields = { 64, 216 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -80, 1125, { .fields = { 16, 224 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, -72, 1125, { .fields = { 96, 80 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, -80, 1125, { .fields = { 104, 232 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, -88, 1125, { .fields = { 96, 112 } }, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, NEO_ARK_PAVILION_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 7's source ranges, covering all 48 sprites and ending at SPRITE_BATCH_END.
///
/// The leading zero-count record preserves the decoded background image.
/// Ranges remain writable for visibility and packet-selection changes.
static SpriteBatch _gNeoArkPavilionView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPavilionView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkPavilionSpriteViews[7] = {
    { { .empty = _gNeoArkPavilionView1SpriteBatches }, _gNeoArkPavilionView1SpriteBatches, NULL },
    { { .elements = _gNeoArkPavilionView2SpriteSources }, _gNeoArkPavilionView2SpriteBatches, NULL },
    { { .elements = _gNeoArkPavilionView3SpriteSources }, _gNeoArkPavilionView3SpriteBatches, NULL },
    { { .elements = _gNeoArkPavilionView4SpriteSources }, _gNeoArkPavilionView4SpriteBatches, NULL },
    { { .elements = _gNeoArkPavilionView5SpriteSources }, _gNeoArkPavilionView5SpriteBatches, NULL },
    { { .elements = _gNeoArkPavilionView6SpriteSources }, _gNeoArkPavilionView6SpriteBatches, NULL },
    { { .elements = _gNeoArkPavilionView7SpriteSources }, _gNeoArkPavilionView7SpriteBatches, NULL },
};

/// Four Q12 directional lights admitted by every view in both pavilion rooms.
///
/// Room-light setup parents and composes these writable coordinates; queries
/// update their attenuation. RGB intensity is 2052 in each channel.
static WorldCoordLight _gNeoArkPavilionDirectionalLights[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 500, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -500, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, -500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
};

/// Shared pavilion light collection: four directional lights, no point or cone lights.
static WorldCoordRoomLights _gNeoArkPavilionRoomLights = {
    ARRAY_SIZE(_gNeoArkPavilionDirectionalLights),
    _gNeoArkPavilionDirectionalLights,
    0,
    NULL,
    0,
    NULL,
};

/// Opposed view-boundary quads switching logical views 2 to 3 and 3 to 2.
///
/// Both pavilion rooms borrow these mutable records through the LAST-marked
/// second entry. Origins/corners use room units and normals use Q12.
static WorldCollisionTrigger _gNeoArkPavilionViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { -323, -1568, 9452, 0 }, { { -4, -1904, -3817, 0 }, { 4, -1904, 3818, 0 }, { -4, 1904, -3817, 0 }, { 4, 1904, 3818, 0 } }, { ONE, 0, -5, 0 }, { 0, 0, ONE, 0 }, 4252, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -57, -1600, 9531, 0 }, { { -1, -1904, 3908, 0 }, { 1, -1904, -3908, 0 }, { -1, 1904, 3908, 0 }, { 1, 1904, -3908, 0 } }, { -4101, 0, -2, 0 }, { 0, 0, ONE, 0 }, 4344, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource key and library file number of variant 1's Bog Diver placements.
enum {
    NEO_ARK_PAVILION_BOG_DIVER_ENTRY_ID    = 4,
    NEO_ARK_PAVILION_BOG_DIVER_FILE_NUMBER = 4
};

/// Placement variant 1's Bog Diver resource, followed by the resource-list end marker.
///
/// Entry 4 selects file 4 in the base-10 actor library and body descriptor 0
/// in actor RAM slot 1. The map-owned placement list supplies three instances.
static AreaResource _gNeoArkPavilionVariant1Resources[2] = {
    { NEO_ARK_PAVILION_BOG_DIVER_ENTRY_ID, NEO_ARK_PAVILION_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Placement keys and library file numbers for the pavilion's additional enemy recipes.
enum {
    NEO_ARK_PAVILION_BLACK_BEETLE_ENTRY_ID    = 38,
    NEO_ARK_PAVILION_BLACK_BEETLE_FILE_NUMBER = 38,
    NEO_ARK_PAVILION_MOSSBACK_ENTRY_ID        = 49,
    NEO_ARK_PAVILION_MOSSBACK_FILE_NUMBER     = 49,
    NEO_ARK_PAVILION_ROOK_ENTRY_ID            = 57,
    NEO_ARK_PAVILION_ROOK_FILE_NUMBER         = 57,
    NEO_ARK_PAVILION_ENEMY_BODY_TASK_INDEX    = 0
};

/// Variant 2 recipes for two Bog Divers and three Black Beetles, then the end marker.
///
/// Entry 4 loads actor_100400 in slot 1; entry 38 loads actor_203800 in slot 2.
/// Both select body descriptor 0. Placements borrow the map overlay; recipes
/// and actor descriptors must remain loaded during loading/spawning.
static AreaResource _gNeoArkPavilionVariant2Resources[3] = {
    { NEO_ARK_PAVILION_BOG_DIVER_ENTRY_ID, NEO_ARK_PAVILION_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { NEO_ARK_PAVILION_BLACK_BEETLE_ENTRY_ID, NEO_ARK_PAVILION_BLACK_BEETLE_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_20, NEO_ARK_PAVILION_ENEMY_BODY_TASK_INDEX, { 0, 0 }, &gActor203800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Bog Diver recipe for three water-mode placements, then the end marker.
///
/// Entry/file 4 selects actor_100400 body descriptor 0 in slot 1. This room's
/// recipe and the borrowed map placements/actor descriptors remain live during use.
static AreaResource _gNeoArkPavilionVariant3Resources[2] = {
    { NEO_ARK_PAVILION_BOG_DIVER_ENTRY_ID, NEO_ARK_PAVILION_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4 recipes for one Bog Diver and one Brute Mossback, then the end marker.
///
/// Entry 4 loads actor_100400 in slot 1; entry 49 loads actor_204900 in slot 2.
/// Both select body descriptor 0. The Mossback table is borrowed from the
/// loaded actor_204900 image. Keep this recipe and the map placements live during use.
static AreaResource _gNeoArkPavilionVariant4Resources[3] = {
    { NEO_ARK_PAVILION_BOG_DIVER_ENTRY_ID, NEO_ARK_PAVILION_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { NEO_ARK_PAVILION_MOSSBACK_ENTRY_ID, NEO_ARK_PAVILION_MOSSBACK_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_20, NEO_ARK_PAVILION_ENEMY_BODY_TASK_INDEX, { 0, 0 }, gActor01100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 5's Bog Diver recipe for four wounded placements, then the end marker.
///
/// Entry/file 4 selects actor_100400 body descriptor 0 in slot 1; placement
/// mode selects one grounded and three floating Divers. Keep room, map and actor storage live.
static AreaResource _gNeoArkPavilionVariant5Resources[2] = {
    { NEO_ARK_PAVILION_BOG_DIVER_ENTRY_ID, NEO_ARK_PAVILION_BOG_DIVER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's grenade-launcher Rook GOLEM recipe, then the resource-list end marker.
///
/// Entry/file 57 selects actor_105700 body descriptor 0 in slot 1; the single
/// map placement selects patrol mode. Keep room, map and actor storage live during use.
static AreaResource _gNeoArkPavilionVariant11Resources[2] = {
    { NEO_ARK_PAVILION_ROOK_ENTRY_ID, NEO_ARK_PAVILION_ROOK_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, NEO_ARK_PAVILION_ENEMY_BODY_TASK_INDEX, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gNeoArkPavilionAreaVariants[13] = {
    [0]  = { NULL, NULL },
    [1]  = { gMapNeoArkPavilionVariant1Placements, _gNeoArkPavilionVariant1Resources },
    [2]  = { gMapNeoArkPavilionVariant2Placements, _gNeoArkPavilionVariant2Resources },
    [3]  = { gMapNeoArkPavilionVariant3Placements, _gNeoArkPavilionVariant3Resources },
    [4]  = { gMapNeoArkPavilionVariant4Placements, _gNeoArkPavilionVariant4Resources },
    [5]  = { gMapNeoArkPavilionVariant5Placements, _gNeoArkPavilionVariant5Resources },
    [6]  = { NULL, NULL },
    [7]  = { NULL, NULL },
    [8]  = { NULL, NULL },
    [9]  = { NULL, NULL },
    [10] = { NULL, NULL },
    [11] = { gMapNeoArkPavilionVariant11Placements, _gNeoArkPavilionVariant11Resources },
    [12] = { NULL, NULL },
};

/// Packed departure/arrival slots and CAP payloads of the pavilion's action quads.
enum {
    NEO_ARK_PAVILION_TUNNEL_WARP_SLOTS      = (1 << 4) | 2,
    NEO_ARK_PAVILION_BRIDGE_WARP_SLOTS      = (2 << 4) | 1,
    NEO_ARK_PAVILION_ISLAND_WARP_SLOTS      = (3 << 4) | 1,
    NEO_ARK_PAVILION_CAP_BRIDGE_INTERACTION = 2,
    NEO_ARK_PAVILION_CAP_ISLAND_INTERACTION = 3,
    NEO_ARK_PAVILION_CAP_FLAGS_NONE         = 0,
    NEO_ARK_PAVILION_CAP_PREPARE_WEAPON     = 1,
    NEO_ARK_PAVILION_ROOM_COMMAND_PROGRESS  = 1
};

/// Room 1's facing-gated tunnel/bridge warps, CAP command 3 and progress interaction.
///
/// The last quad routes command 1 to the room handler; the island-side quad
/// runs CAP 3 with weapon preparation and paused actors. Setup links all four
/// mutable quads through the LAST-marked final entry; coordinates use room units.
static WorldCollisionTrigger _gNeoArkPavilionRoom1ActionTriggers[4] = {
    { NULL, NULL, NULL, { 2240, -48, 9504, 0 }, { { -320, 0, -480, 0 }, { 320, 0, -480, 0 }, { -320, 0, 480, 0 }, { 320, 0, 480, 0 } }, { 0, 4116, 0, 0 }, { -ONE, 0, 0, 0 }, 576, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, NEO_ARK_PAVILION_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3488, -48, 7456, 0 }, { { -1088, 0, -288, 0 }, { 1088, 0, -288, 0 }, { -1088, 0, 288, 0 }, { 1088, 0, 288, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, ONE, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_BRIDGE, NEO_ARK_PAVILION_BRIDGE_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2896, -48, 7456, 0 }, { { -1072, 0, -256, 0 }, { 1072, 0, -256, 0 }, { -1072, 0, 256, 0 }, { 1072, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, ONE, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_PAVILION_CAP_ISLAND_INTERACTION, NEO_ARK_PAVILION_CAP_PREPARE_WEAPON, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4400, -64, 0x2BA0, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -ONE, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_PAVILION_ROOM_COMMAND_PROGRESS, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room 2's facing-gated tunnel/island warps, CAP command 2 and progress interaction.
///
/// The bridge-side quad runs CAP 2 without presentation flags; the last quad
/// routes command 1 to the room handler. Setup links all four mutable quads
/// through the LAST-marked final entry; coordinates use room units.
static WorldCollisionTrigger _gNeoArkPavilionRoom2ActionTriggers[4] = {
    { NULL, NULL, NULL, { 2240, -48, 9504, 0 }, { { -320, 0, -480, 0 }, { 320, 0, -480, 0 }, { -320, 0, 480, 0 }, { 320, 0, 480, 0 } }, { 0, 4116, 0, 0 }, { -ONE, 0, 0, 0 }, 576, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL, NEO_ARK_PAVILION_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3472, -48, 7456, 0 }, { { -1136, 0, -288, 0 }, { 1136, 0, -288, 0 }, { -1136, 0, 288, 0 }, { 1136, 0, 288, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, ONE, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_PAVILION_CAP_BRIDGE_INTERACTION, NEO_ARK_PAVILION_CAP_FLAGS_NONE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3008, -48, 7456, 0 }, { { -1024, 0, -256, 0 }, { 1024, 0, -256, 0 }, { -1024, 0, 256, 0 }, { 1024, 0, 256, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, ONE, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_ISLAND, NEO_ARK_PAVILION_ISLAND_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4400, -64, 0x2BA0, 0 }, { { -464, 0, -672, 0 }, { 464, 0, -672, 0 }, { -464, 0, 672, 0 }, { 464, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { -ONE, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_PAVILION_ROOM_COMMAND_PROGRESS, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Single line-of-sight blocking quad shared by both pavilion rooms.
///
/// Origin/corners use room units and the plane normal uses Q12. Setup links
/// and enables the LAST-marked record; list links/flags remain mutable.
/// The stored low flag bit has no established meaning.
static WorldCollisionOccluder _gNeoArkPavilionOccluders[1] = {
    { NULL, NULL, { 3520, -200, 9440, 0 }, { { -1024, 1224, 0, 0 }, { 1024, 1224, 0, 0 }, { -1024, -1912, 0, 0 }, { 1024, -536, 0, 0 } }, { 0, 0, ONE, 0 }, 2157, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Cue-2 footstep bases in the current type-1 sound bank.
enum {
    NEO_ARK_PAVILION_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 13,
    NEO_ARK_PAVILION_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 15
};

/// Surface class 4's floor footstep bases; scripted jumps use the walking pair.
///
/// Cue 1 adds one to the selected base, and companion footsteps add 100.
/// Surface records borrow this storage while the pavilion overlay is loaded.
static WorldCollisionFootstepSounds _gNeoArkPavilionFloorFootstepSounds = {
    NEO_ARK_PAVILION_FOOTSTEP_WALK,
    NEO_ARK_PAVILION_FOOTSTEP_RUN,
    NEO_ARK_PAVILION_FOOTSTEP_WALK,
};

/// Blocking, impact-enabled policy without footstep cues, shared by classes 0, 2, 3, 5, 6 and 7.
static WorldCollisionSurfaceProperties _gNeoArkPavilionSolidSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 1 policy: probes pass and weapon impacts are ignored; pushback still applies.
static WorldCollisionSurfaceProperties _gNeoArkPavilionProbePassSurface = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 4 floor policy: blocking, impact-enabled contacts with pushback and footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkPavilionFloorSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gNeoArkPavilionFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkPavilionSurfaceProperties[8] = {
    &_gNeoArkPavilionSolidSurface,
    &_gNeoArkPavilionProbePassSurface,
    &_gNeoArkPavilionSolidSurface,
    &_gNeoArkPavilionSolidSurface,
    &_gNeoArkPavilionFloorSurface,
    &_gNeoArkPavilionSolidSurface,
    &_gNeoArkPavilionSolidSurface,
    &_gNeoArkPavilionSolidSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest event-start call attempted to spawn the staged controller.
///
/// Cleared by every call, including queries and already-seen replies; set
/// after execute mode latches the snapshots and calls the spawner (0 no,
/// 1 yes). Spawn success is not checked, and teardown does not reset it.
/// No reader is established in this image.
static s8 _gNeoArkPavilionEventStartRequested = 0;

/// Retained initialized byte with no established consumer; its role is unproven.
static u8 D_neo_ark_pavilion_80187A1D = 38;

/// Retained initialized byte with no established consumer; its role is unproven.
static u8 D_neo_ark_pavilion_80187A1E = 203;

/// Retained initialized byte with no established consumer; its role is unproven.
static u8 D_neo_ark_pavilion_80187A1F = 37;

RoomLatchedEvent gRoomEventLatched;

static void _neoArkPavilionInitializeRoom(Task* task);
static void _neoArkPavilionRoomIdleState(Task* task);

#include "../../shared/effects/water.inc.c"

#include "../../shared/room/room_event_staged_task.inc.c"

/// Refuses every key-item-use request in the pavilion with a zero reply.
///
/// All callback arguments are ignored; no item is consumed or retained.
static s32 _neoArkPavilionRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedMessageArg)
{
    return 0;
}

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static inline s32 _neoArkPavilionStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    _gNeoArkPavilionEventStartRequested = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *transition;
            gRoomEventLatched             = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&_gNeoArkPavilionStagedEventTaskDesc, 0, 0, 0);
            _gNeoArkPavilionEventStartRequested = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Resolves Neo Ark room variants and gates the submarine-tunnel departure scene.
///
/// Borrows a complete request and writable reply, which may alias. Returns 1
/// when no scene is needed and 2 while the one-shot departure scene is eligible.
/// Eligible execution latches the resolved reply, commits the scene flag and
/// queues CAP command 4 without a stage sound or fade; queries do not start it.
/// Requires the map overlay and the room resources through deferred playback.
static s32 _neoArkPavilionResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { TRANSITION_ALLOWED             = 1,
           CAP_SUBMARINE_TUNNEL_DEPARTURE = 4,
           DEPARTURE_NO_SOUND             = 0,
           DEPARTURE_NO_FADE              = 0 };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_NEO_ARK_SUBMARINE_TUNNEL) {
        return TRANSITION_ALLOWED;
    }
    event.capCmd   = CAP_SUBMARINE_TUNNEL_DEPARTURE;
    event.stageSnd = DEPARTURE_NO_SOUND;
    event.flagId   = GAME_FLAG_PAVILION_TO_SUB_TUNNEL_SCENE;
    event.fade     = DEPARTURE_NO_FADE;
    return _neoArkPavilionStartEvent(reply, &event);
}

/// Selects the pavilion's progress-dependent CAP event for room command 1.
///
/// Queues CAP 1 while the alternate-dialogue flag is clear, otherwise CAP 5,
/// only while CAP is idle; the event pauses actors. Other commands do nothing.
/// Returns zero. Only `commandIndex` is read; loaded CAP resources must outlive
/// playback. The flag's wider story meaning is unproven.
static s32 _neoArkPavilionHandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum { COMMAND_CHECK_PROGRESS = 1,
           CAP_DEFAULT_DIALOGUE   = 1,
           CAP_ALTERNATE_DIALOGUE = 5 };

    if (commandIndex == COMMAND_CHECK_PROGRESS) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_141) != 0 ? CAP_ALTERNATE_DIALOGUE : CAP_DEFAULT_DIALOGUE, CAP_EVENT_PAUSE_ACTORS);
    }
    return 0;
}

/// Ignores room-action requests and returns zero without starting an action.
///
/// The borrowed request and the other callback arguments are never read or retained.
static s32 _neoArkPavilionIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedMessageArg)
{
    return 0;
}

/// Installs the pavilion's room-message receiver and starts its two ambience scripts.
///
/// Advances to idle. The session's room slot borrows the live task;
/// teardown does not clear the registration.
static void _neoArkPavilionInitializeRoom(Task* task)
{
    task->msgTable = _gNeoArkPavilionRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_PAVILION_AMBIENCE_1, 0, 0);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_PAVILION_AMBIENCE_2, 0, 0);
    task->state = task->state + 1;
}

/// Keeps the initialized room task idle while its message table remains installed.
static void _neoArkPavilionRoomIdleState(Task* task)
{
}

/// Room-task state handlers: 0 initializes ambience/messages, 1 idles, 2 tears down.
///
/// Copied by value before unchecked state dispatch. The callbacks and room
/// storage must remain loaded; this table stays read-only and has no terminator.
static const TaskFuncTable3 _gNeoArkPavilionRoomStates = {
    { _neoArkPavilionInitializeRoom, _neoArkPavilionRoomIdleState, taskKill }
};

void neoArkPavilionRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gNeoArkPavilionRoomStates;
    handlers.funcs[task->state](task);
}

#include "../../shared/effects/water_ripple_task_fixed_coord.inc.c"

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task_u16_fixed_coord.inc.c"

#include "../../shared/effects/water_spin_u16.inc.c"

void neoArkPavilionInstallRoomEffectsTask(Task* task)
{
    enum { NEO_ARK_PAVILION_EFFECTS_INSTALL,
           NEO_ARK_PAVILION_EFFECTS_INSTALLED };

    if (task->state == NEO_ARK_PAVILION_EFFECTS_INSTALL) {
        gRoomEffectFlashId        = EFFECT_NEO_ARK_PAVILION_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_NEO_ARK_PAVILION_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_NEO_ARK_PAVILION_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_NEO_ARK_PAVILION_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_NEO_ARK_PAVILION_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_NEO_ARK_PAVILION_ORANGE_BURST_2;
        gRoomEffectWaterRippleId  = EFFECT_NEO_ARK_PAVILION_WATER_RIPPLE;
        gRoomEffectWaterSprayId   = EFFECT_NEO_ARK_PAVILION_WATER_SPRAY;
        task->state               = NEO_ARK_PAVILION_EFFECTS_INSTALLED;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkPavilionRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkPavilionRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkPavilionRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

    void neoArkPavilionRoomVisualEffectsGlowDiscTask(Task * task)
    {
        _roomVisualEffectsGlowDiscTask(task);
    }

    void neoArkPavilionRoomVisualEffectsFlyingSparkTask(Task * task)
    {
        _roomVisualEffectsFlyingSparkTask(task);
    }

#include "../../shared/room/room_visual_effects_burst.inc.c"

    void neoArkPavilionRoomVisualEffectsFlyingOrangeBurstTask(Task * task)
    {
        _roomVisualEffectsFlyingOrangeBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
