#include "rooms/dryfield_parking_lot.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_02500.h"
#include "actors/actor_402200.h"

#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/dryfield_night_parking_lot.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/parking_lot.h"

static s32 _parkingLotSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

extern RoomEventActiveBytes gRoomEventActive;

/// Descriptor of the event task the event gate spawns.

/// The request the event gate latched for its event task.
extern RoomEventReq gRoomEventReq;

static WorldCollisionGrid     _gDryfieldParkingLotCollisionGrid;
static WorldCollisionOccluder _gDryfieldParkingLotSightOccluders[2];
static WorldCollisionTrigger  _gDryfieldParkingLotViewBoundaryTriggers[10];
static WorldCollisionTrigger  _gDryfieldParkingLotActionTriggers[11];
static WorldCoordRoomLights   _gDryfieldParkingLotRoomLights;
static s32                    _dryfieldParkingLotRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32                    _dryfieldParkingLotIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32                    _dryfieldParkingLotIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void                   _dryfieldParkingLotIdle(Task* task);

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Synchronous room handlers for transitions, item use, actions, commands and sounds.
///
/// The live room task borrows this table. Item use, actions and commands return
/// zero; transition and sound requests use the parking-lot handlers. The last
/// record makes unsupported message IDs return zero. Keep this overlay loaded.
static TaskMessageEntry _gDryfieldParkingLotRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, parkingLotResolveMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldParkingLotRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldParkingLotIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldParkingLotIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, _parkingLotSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Effect gates for mapped views 1..7, followed by a halfword of unproven role.
///
/// Indexed by mapped view minus one: views 2..5 enable footstep dust and related
/// effects. The final halfword has no established consumer or object boundary;
/// it is retained verbatim and does not establish an eighth view.
static u16 _gDryfieldParkingLotViewEffectGates[8] = {
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_ENABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    ROOM_EFFECT_VIEW_DISABLED,
    0xE5E6,
};

WorldCollisionRoomResources gDryfieldParkingLotRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldParkingLotCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldParkingLotViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldParkingLotActionTriggers,
        .occluders            = _gDryfieldParkingLotSightOccluders,
    },
};

u8* gDryfieldParkingLotRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldParkingLotRoomViewCounts[1] = { DRYFIELD_PARKING_LOT_VIEW_COUNT };

WorldCoordRoomLighting gDryfieldParkingLotRoomLighting[1] = {
    { &_gDryfieldParkingLotRoomLights, NULL },
};

/// Fixed daytime sound requests and map-marker nibble IDs for warp endpoints.
enum {
    DRYFIELD_PARKING_LOT_MAIN_STREET_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 2),
    DRYFIELD_PARKING_LOT_MAIN_STREET_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 1),
    DRYFIELD_PARKING_LOT_DOOR_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 8),
    DRYFIELD_PARKING_LOT_DOOR_BLOCKED_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 0xA),
    DRYFIELD_PARKING_LOT_TOILET_ARRIVAL_SOUND        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 6),
    DRYFIELD_PARKING_LOT_TOILET_DEPARTURE_SOUND      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 5),
    DRYFIELD_PARKING_LOT_MAIN_STREET_MAP_FLAG        = 486,
    DRYFIELD_PARKING_LOT_MOTEL_LOBBY_MAP_FLAG        = 481,
    DRYFIELD_PARKING_LOT_TOILET_MAP_FLAG             = 480,
    DRYFIELD_PARKING_LOT_SALOON_G_R_MAP_FLAG         = 479,
};

DirectionWarpEntry gDryfieldParkingLotWarpEntries[DRYFIELD_PARKING_LOT_WARP_COUNT] = {
    [DRYFIELD_PARKING_LOT_WARP_MAIN_STREET - 1] = {
        .player         = { { .word = 3072 }, 0x2995, 0, 3077 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 0x2995, 0, 3077 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_PARKING_LOT_MAIN_STREET_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_PARKING_LOT_MAIN_STREET_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 7,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_PARKING_LOT_MAIN_STREET_MAP_FLAG,
    },
    [DRYFIELD_PARKING_LOT_WARP_MOTEL_LOBBY - 1] = {
        .player         = { { .word = 2048 }, 3200, 0, 3472 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 2048 }, 3200, 0, 3472 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_PARKING_LOT_DOOR_ARRIVAL_SOUND,
        .departureSound = SOUND_PARKING_LOT_DOOR_OPEN,
        .blockedSound   = DRYFIELD_PARKING_LOT_DOOR_BLOCKED_SOUND,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_PARKING_LOT_MOTEL_LOBBY_MAP_FLAG,
    },
    [DRYFIELD_PARKING_LOT_WARP_TOILET - 1] = {
        .player         = { { .word = 0 }, -285, 0, 351 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, -285, 0, 351 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_PARKING_LOT_TOILET_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_PARKING_LOT_TOILET_DEPARTURE_SOUND,
        .blockedSound   = DRYFIELD_PARKING_LOT_DOOR_BLOCKED_SOUND,
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_PARKING_LOT_TOILET_MAP_FLAG,
    },
    [DRYFIELD_PARKING_LOT_WARP_SALOON_G_R - 1] = {
        .player         = { { .word = 2048 }, -6840, 0, -450 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 2048 }, -6840, 0, -450 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_PARKING_LOT_DOOR_ARRIVAL_SOUND,
        .departureSound = SOUND_PARKING_LOT_DOOR_OPEN,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_PARKING_LOT_SALOON_G_R_MAP_FLAG,
    },
    [DRYFIELD_PARKING_LOT_WARP_MOTEL_BALCONY - 1] = {
        .player         = { { .word = 0 }, 4771, -450, 1414 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 4491, 0, 2215 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_SCRIPTED_PLAYER,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Collision-mesh normals in room axes, with 4096 representing unit length.
static SVECTOR _gDryfieldParkingLotCollisionGridNormals[18] = {
#include "assets/dryfield_parking_lot_collision_0131C_normals.inc"
};

/// Collision-mesh vertices in signed grid-local world units.
static SVECTOR _gDryfieldParkingLotCollisionGridVerts[132] = {
#include "assets/dryfield_parking_lot_collision_0131C_verts.inc"
};

/// Indexed collision faces, carrying the room's surface classes 0..7.
static WorldCollisionGridFace _gDryfieldParkingLotCollisionGridFaces[55] = {
#include "assets/dryfield_parking_lot_collision_0131C_faces.inc"
};

/// Packed signed face-index lists for all 45 cells; each ends with -1.
static s16 _gDryfieldParkingLotCollisionGridCells[436] = {
#include "assets/dryfield_parking_lot_collision_0131C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldParkingLotCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..435 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_PARKING_LOT_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldParkingLotCollisionGridCells[(halfwordOffset)])
/// X-major directory of the grid's 9 by 5 face-index lists.
static s16* _gDryfieldParkingLotCollisionGridTable[45] = {
#include "assets/dryfield_parking_lot_collision_0131C_table.inc"
};
#undef DRYFIELD_PARKING_LOT_CELL_FACE_LIST_AT_OFFSET

/// The daytime parking lot's 55-face mesh and 9 by 5 broad-phase grid.
///
/// The XZ origin is (-17800, -8900) world units; cells have side length 4000.
/// Room activation binds the view transform. This record and its mutable pools
/// remain borrowed until the active grid is replaced or cleared.
static WorldCollisionGrid _gDryfieldParkingLotCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldParkingLotCollisionGridNormals,
    .vertices    = _gDryfieldParkingLotCollisionGridVerts,
    .faces       = _gDryfieldParkingLotCollisionGridFaces,
    .cellFaceIds = _gDryfieldParkingLotCollisionGridTable,
    .xBias       = 17800,
    .zBias       = 8900,
    .cellCountX  = 9,
    .cellCountZ  = 5,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldParkingLotCollisionGridFaces),
};

ViewCamera gDryfieldParkingLotViewCameras[DRYFIELD_PARKING_LOT_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x7530, 0 } }, 380 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 0x27F9, 2236, 6567 } }, 230 },
    { { { { 2646, 0, -3126 }, { -3085, 656, -2612 }, { 501, 4043, 424 } }, { 3012, 5975, 537 } }, 230 },
    { { { { 456, 0, 4070 }, { -262, 4087, 29 }, { -4062, -264, 455 } }, { -3566, 1036, -1232 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
};

/// View 1's empty sprite list: keep decoded image strips, then end the list.
static SpriteBatch _gDryfieldParkingLotView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded atlas pages, palette and neutral RGB modulation for the room sprites.
///
/// The two 8-bit texture pages start at VRAM X 896 and 960, Y 0; the CLUT
/// starts at X 0, Y 255. RGB 128 leaves sampled colours at their authored level.
enum {
    DRYFIELD_PARKING_LOT_SPRITE_TEXTURE_8BIT  = 1,
    DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE     = getTPage(DRYFIELD_PARKING_LOT_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE    = getTPage(DRYFIELD_PARKING_LOT_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_PARKING_LOT_SPRITE_CLUT          = getClut(0, 255),
    DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR = 128,
};

/// View 2's ten texture rectangles, with pixel positions and unscaled sorting depths.
static SpriteSource _gDryfieldParkingLotView2SpriteSources[10] = {
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, -160, 32, 500, { .fields = { 16, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 80 } }, -104, 40, 500, { .fields = { 64, 80 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 104 } }, -80, 16, 500, { .fields = { 72, 120 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, -56, 32, 500, { .fields = { 88, 88 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 96 } }, 0, 24, 500, { .fields = { 40, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 112 } }, 40, 8, 625, { .fields = { 96, 120 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 120 } }, 72, 0, 650, { .fields = { 80, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 80 } }, 0, -32, 2750, { .fields = { 72, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 88 } }, 40, -32, 3000, { .fields = { 112, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 80 } }, 96, -24, 6250, { .fields = { 88, 176 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's image strips, seven-sprite batch and final three-sprite batch.
///
/// Source ranges cover the full view-2 array and end at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldParkingLotView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, ARRAY_SIZE(_gDryfieldParkingLotView2SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's pixel clip, restored to full-screen drawing at sorting depth 2250.
///
/// The rectangle is relative to the draw buffer; the final record ends the list.
static SpriteDrawArea _gDryfieldParkingLotView2SpriteDrawAreas[2] = {
    { { 136, 0, 183, 238 }, 2250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// View 3's 24 texture rectangles, with pixel positions and unscaled sorting depths.
static SpriteSource _gDryfieldParkingLotView3SpriteSources[24] = {
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 120, 8 } }, -128, 72, 625, { .fields = { 32, 112 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 136, 8 } }, -40, 80, 625, { .fields = { 120, 248 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 88, 8 } }, 72, 88, 625, { .fields = { 64, 120 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 24 } }, 0, -80, 625, { .fields = { 16, 72 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 24 } }, -32, -88, 625, { .fields = { 8, 24 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 24 } }, -64, -104, 625, { .fields = { 80, 56 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 24 } }, 8, -32, 1350, { .fields = { 40, 24 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 8 } }, 8, -40, 1125, { .fields = { 40, 48 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, -40, 925, { .fields = { 16, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 16 } }, 0, -56, 925, { .fields = { 120, 168 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 24 } }, 16, -80, 925, { .fields = { 8, 48 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, -96, 925, { .fields = { 16, 184 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 96 } }, 40, -112, 925, { .fields = { 112, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 40 } }, 104, 16, 925, { .fields = { 72, 208 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 56 } }, -96, -40, 750, { .fields = { 32, 56 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -120, 625, { .fields = { 8, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 32 } }, -112, -120, 625, { .fields = { 32, 168 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 56 } }, 56, -120, 925, { .fields = { 24, 112 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 56 } }, 56, -64, 925, { .fields = { 56, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 56 } }, 112, -64, 925, { .fields = { 80, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 56 } }, 112, -120, 925, { .fields = { 80, 152 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 24 } }, 64, -8, 925, { .fields = { 24, 200 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 24 } }, 112, -8, 925, { .fields = { 24, 224 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 16, 1000, { .fields = { 88, 80 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's image strips and one batch covering every source sprite, then the end marker.
static SpriteBatch _gDryfieldParkingLotView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldParkingLotView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's nine texture rectangles, with pixel positions and unscaled sorting depths.
static SpriteSource _gDryfieldParkingLotView4SpriteSources[9] = {
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 56, 48 } }, -160, 72, 550, { .fields = { 0, 144 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 80, 16 } }, 48, -88, 925, { .fields = { 104, 192 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, -72, 925, { .fields = { 112, 168 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 8, 96 } }, -80, -32, 1250, { .fields = { 96, 152 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 112 } }, -96, -32, 1250, { .fields = { 56, 144 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 144 } }, -128, -32, 1000, { .fields = { 40, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 152 } }, -160, -32, 1000, { .fields = { 72, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 96 } }, -88, -48, 1975, { .fields = { 80, 152 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 168 } }, -72, -112, 2000, { .fields = { 104, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 4's image strips and three source ranges of three, four and two sprites.
///
/// Source ranges cover the full view-4 array and end at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldParkingLotView4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 4, 0, 0, { 2, 0 } },
    { 7, ARRAY_SIZE(_gDryfieldParkingLotView4SpriteSources) - 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's seven texture rectangles, with pixel positions and unscaled sorting depths.
static SpriteSource _gDryfieldParkingLotView5SpriteSources[7] = {
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 200 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 136 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 16 } }, -128, 88, 750, { .fields = { 72, 184 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 16, 64 } }, -144, -112, 650, { .fields = { 112, 136 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 5's image strips and one batch covering every source sprite, then the end marker.
static SpriteBatch _gDryfieldParkingLotView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldParkingLotView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 21 texture rectangles, with pixel positions and unscaled sorting depths.
static SpriteSource _gDryfieldParkingLotView6SpriteSources[21] = {
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, -80, 528, { .fields = { 72, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, -56, 531, { .fields = { 120, 176 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, -56, 993, { .fields = { 120, 216 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, -16, 525, { .fields = { 120, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, -16, 751, { .fields = { 120, 136 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, 24, 518, { .fields = { 80, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, 24, 510, { .fields = { 80, 216 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 80, 64, 515, { .fields = { 80, 176 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 40, 40 } }, 120, 64, 507, { .fields = { 80, 136 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, -120, 1012, { .fields = { 112, 48 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, -72, 1012, { .fields = { 80, 144 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, -24, 1012, { .fields = { 80, 192 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 16, 24, 1012, { .fields = { 80, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, 24, 1012, { .fields = { 80, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -24, 1012, { .fields = { 80, 48 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -72, 1012, { .fields = { 32, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -120, 1012, { .fields = { 32, 192 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_LEFT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -120, 1012, { .fields = { 112, 0 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -72, 1012, { .fields = { 32, 144 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -24, 1012, { .fields = { 32, 48 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_PARKING_LOT_SPRITE_RIGHT_PAGE, DRYFIELD_PARKING_LOT_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 925, { .fields = { 32, 96 } }, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, DRYFIELD_PARKING_LOT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 6's image strips, nine-sprite batch and final twelve-sprite batch.
///
/// Source ranges cover the full view-6 array and end at SPRITE_BATCH_END.
static SpriteBatch _gDryfieldParkingLotView6SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, ARRAY_SIZE(_gDryfieldParkingLotView6SpriteSources) - 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list: keep decoded image strips, then end the list.
static SpriteBatch _gDryfieldParkingLotView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldParkingLotSpriteViews[DRYFIELD_PARKING_LOT_VIEW_COUNT] = {
    { { .empty = _gDryfieldParkingLotView1SpriteBatches }, _gDryfieldParkingLotView1SpriteBatches, NULL },
    { { .elements = _gDryfieldParkingLotView2SpriteSources }, _gDryfieldParkingLotView2SpriteBatches, _gDryfieldParkingLotView2SpriteDrawAreas },
    { { .elements = _gDryfieldParkingLotView3SpriteSources }, _gDryfieldParkingLotView3SpriteBatches, NULL },
    { { .elements = _gDryfieldParkingLotView4SpriteSources }, _gDryfieldParkingLotView4SpriteBatches, NULL },
    { { .elements = _gDryfieldParkingLotView5SpriteSources }, _gDryfieldParkingLotView5SpriteBatches, NULL },
    { { .elements = _gDryfieldParkingLotView6SpriteSources }, _gDryfieldParkingLotView6SpriteBatches, NULL },
    { { .empty = _gDryfieldParkingLotView7SpriteBatches }, _gDryfieldParkingLotView7SpriteBatches, NULL },
};

/// Directed boundaries for 2 <-> 3 <-> 4 <-> 5 <-> 6 <-> 7 camera changes.
///
/// Each source/destination pair is one-based. Room setup binds and enables all
/// ten writable records, including the LAST-marked final boundary. Origins and
/// corners use room units; normals use 4096 per unit. Storage lives until unlinked.
static WorldCollisionTrigger _gDryfieldParkingLotViewBoundaryTriggers[10] = {
    { NULL, NULL, NULL, { -2698, -2736, -1594, 0 }, { { -2084, -3856, 2134, 0 }, { 2085, -3856, -2134, 0 }, { -2084, 3856, 2134, 0 }, { 2085, 3856, -2134, 0 } }, { -2935, 0, -2867, 0 }, { 0, 0, ONE, 0 }, 4857, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2816, -2592, -1600, 0 }, { { 2078, -3616, -2142, 0 }, { -2092, -3616, 2127, 0 }, { 2078, 3616, -2142, 0 }, { -2092, 3616, 2127, 0 } }, { 2941, 0, 2873, 0 }, { 0, 0, ONE, 0 }, 4664, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2560, -2656, 1664, 0 }, { { 1854, -3680, -2342, 0 }, { -1877, -3680, 2312, 0 }, { 1854, 3680, -2342, 0 }, { -1877, 3680, 2312, 0 } }, { 3203, 0, 2568, 0 }, { 0, 0, ONE, 0 }, 4720, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2528, -2720, 1760, 0 }, { { -1873, -3744, 2314, 0 }, { 1857, -3744, -2338, 0 }, { -1873, 3744, 2314, 0 }, { 1857, 3744, -2338, 0 } }, { -3200, 0, -2566, 0 }, { 0, 0, ONE, 0 }, 4775, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 831, -2528, 2015, 0 }, { { -694, -3552, 2899, 0 }, { 690, -3552, -2903, 0 }, { -694, 3552, 2899, 0 }, { 690, 3552, -2903, 0 } }, { -3985, 0, -951, 0 }, { 0, 0, ONE, 0 }, 4636, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 798, -2416, 2046, 0 }, { { 690, -3440, -2903, 0 }, { -694, -3440, 2899, 0 }, { 690, 3440, -2903, 0 }, { -694, 3440, 2899, 0 } }, { 3998, 0, 953, 0 }, { 0, 0, ONE, 0 }, 4550, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8191, -2320, 2527, 0 }, { { -34, -3344, -2982, 0 }, { 35, -3344, 2983, 0 }, { -34, 3344, -2982, 0 }, { 35, 3344, 2983, 0 } }, { 4106, 0, -48, 0 }, { 0, 0, ONE, 0 }, 4463, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8223, -2256, 2399, 0 }, { { 35, -3280, 2983, 0 }, { -34, -3280, -2982, 0 }, { 35, 3280, 2983, 0 }, { -34, 3280, -2982, 0 } }, { -4098, 0, 47, 0 }, { 0, 0, ONE, 0 }, 4404, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4048, -2912, 3200, 0 }, { { -500, -3856, 3222, 0 }, { 501, -3856, -3222, 0 }, { -500, 3856, 3222, 0 }, { 501, 3856, -3222, 0 } }, { -4059, 0, -631, 0 }, { 0, 0, ONE, 0 }, 5042, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3968, -2848, 2864, 0 }, { { 396, -3856, -2554, 0 }, { -395, -3856, 2554, 0 }, { 396, 3856, -2554, 0 }, { -395, 3856, 2554, 0 } }, { 4054, 0, 627, 0 }, { 0, 0, ONE, 0 }, 4636, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed stair settings and CAP command slots used by this room's interactions.
///
/// A stair parameter stores its surface class in bits 4..6 and steps in bits 0..3;
/// control bit 8 selects descent. CAP slots address the loaded dialogue bundle.
enum {
    DRYFIELD_PARKING_LOT_STAIR_DESCEND_CONTROL   = 0x100,
    DRYFIELD_PARKING_LOT_STAIR_ASCENT_PARAMETER  = (3 << 4) | 4,
    DRYFIELD_PARKING_LOT_STAIR_DESCENT_PARAMETER = (3 << 4) | 2,
    DRYFIELD_PARKING_LOT_CAP_INTERACTION_4       = 4,
    DRYFIELD_PARKING_LOT_CAP_INTERACTION_5       = 5,
};

/// Five warp quads, two stair actions and four CAP interaction quads.
///
/// Room setup binds, links and enables these writable records through LAST.
/// Warp parameters pack this area's endpoint in the high nibble and the
/// destination's arrival endpoint in the low nibble. CAP commands 4 and 5
/// each have two regions. Positions use room units and normals use Q12.
static WorldCollisionTrigger _gDryfieldParkingLotActionTriggers[11] = {
    { NULL, NULL, NULL, { 0x2870, -48, 2976, 0 }, { { -560, 0, -1024, 0 }, { 560, 0, -1024, 0 }, { -560, 0, 1024, 0 }, { 560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -ONE, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MAIN_STREET, (DRYFIELD_PARKING_LOT_WARP_MAIN_STREET << 4) | 3, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3088, -48, 3744, 0 }, { { 848, 0, -560, 0 }, { 848, 0, 560, 0 }, { -848, 0, -560, 0 }, { -848, 0, 560, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 1015, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_LOBBY, (DRYFIELD_PARKING_LOT_WARP_MOTEL_LOBBY << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -560, -48, 192, 0 }, { { 720, 0, -560, 0 }, { 720, 0, 560, 0 }, { -720, 0, -560, 0 }, { -720, 0, 560, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 911, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_TOILET, (DRYFIELD_PARKING_LOT_WARP_TOILET << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6736, -48, -640, 0 }, { { 656, 0, -560, 0 }, { 656, 0, 560, 0 }, { -656, 0, -560, 0 }, { -656, 0, 560, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -ONE, 0 }, 861, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_SALOON_G_R, (DRYFIELD_PARKING_LOT_WARP_SALOON_G_R << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4752, -48, 2032, 0 }, { { 368, 0, -224, 0 }, { 368, 0, 224, 0 }, { -368, 0, -224, 0 }, { -368, 0, 224, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, ONE, 0 }, 430, WORLD_COLLISION_TRIGGER_ACTION_FACING, DRYFIELD_PARKING_LOT_STAIR_ASCENT_PARAMETER, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4640, -992, 1632, 0 }, { { -560, 1024, 0, 0 }, { 560, 1024, 0, 0 }, { -560, -1024, 0, 0 }, { 560, -1024, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, ONE, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_BALCONY, (DRYFIELD_PARKING_LOT_WARP_MOTEL_BALCONY << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1104, -64, 3024, 0 }, { { 432, 0, -1024, 0 }, { 432, 0, 1024, 0 }, { -432, 0, -1024, 0 }, { -432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4091, 0, 201, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_PARKING_LOT_CAP_INTERACTION_4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4672, -512, 1408, 0 }, { { 512, 0, -112, 0 }, { 512, 0, 112, 0 }, { -512, 0, -112, 0 }, { -512, 0, 112, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -ONE, 0 }, 523, WORLD_COLLISION_TRIGGER_ACTION_FACING | DRYFIELD_PARKING_LOT_STAIR_DESCEND_CONTROL | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_PARKING_LOT_STAIR_DESCENT_PARAMETER, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2048, -64, 2448, 0 }, { { 1232, 0, -432, 0 }, { 1232, 0, 432, 0 }, { -1232, 0, -432, 0 }, { -1232, 0, 432, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -ONE, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_PARKING_LOT_CAP_INTERACTION_4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7128, -64, -1968, 0 }, { { 1704, 0, -320, 0 }, { 1480, 0, 736, 0 }, { -1592, 0, -768, 0 }, { -1592, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 1764, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_PARKING_LOT_CAP_INTERACTION_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5520, -64, -3104, 0 }, { { 960, 0, -1680, 0 }, { 224, 0, 1680, 0 }, { -224, 0, -1680, 0 }, { -960, 0, 1680, 0 } }, { 0, ONE, 0, 0 }, { 4017, 0, 799, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_PARKING_LOT_CAP_INTERACTION_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space quads blocking actor line-of-sight queries.
///
/// Room setup links and enables both writable records, including the final
/// LAST-marked quad. Positions use room units and normals use Q12. The authored
/// low flag bit and final zero byte have no established meaning and are retained.
static WorldCollisionOccluder _gDryfieldParkingLotSightOccluders[2] = {
    { NULL, NULL, { -416, -1872, -3328, 0 }, { { 1078, 2896, -3014, 0 }, { 1078, -2896, -3014, 0 }, { -1079, 2896, 3013, 0 }, { -1079, -2896, 3013, 0 } }, { 3859, 0, 1381, 0 }, 4314, 1, 0 },
    { NULL, NULL, { -6544, -1728, 2800, 0 }, { { 2064, 2896, -2704, 0 }, { 2064, -2896, -2704, 0 }, { -2064, 2896, 2704, 0 }, { -2064, -2896, 2704, 0 } }, { 3260, 0, 2488, 0 }, 4463, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Seven writable point lights admitted in every logical room view.
///
/// Translations and falloff radii use world units; RGB intensities use Q12.
/// The lighting task parents and composes these nodes and overwrites attenuation.
static WorldCoordPointLight _gDryfieldParkingLotPointLights[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -6980, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 2949, 2785 }, { 0, 0 } }, 100000, 100000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7480, -2210, -4200 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4915, 4915, 4915 }, { 0, 0 } }, 4500, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1520, -980, 2900 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3220, -980, 2900 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2980, -980, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -180, -980, 2900 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4180, -980, -100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
};

/// Room 1's seven point lights, with no directional or cone lights.
///
/// The descriptor and its mutable lights remain live only while this overlay is loaded.
static WorldCoordRoomLights _gDryfieldParkingLotRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldParkingLotPointLights),
    .pointLights           = _gDryfieldParkingLotPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// The resource key and global-library file number of variant 2's Desert Chasers.
enum {
    DRYFIELD_PARKING_LOT_DESERT_CHASER_ENTRY_ID    = 1,
    DRYFIELD_PARKING_LOT_DESERT_CHASER_FILE_NUMBER = 1,
};

/// Variant 1's empty actor-resource list, containing only its terminator.
static AreaResource _gDryfieldParkingLotVariant1Resources[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Desert Chaser resource for placement entry 1, followed by the terminator.
///
/// Group base 40 and file number 1 load actor file 400100. Task index 0 borrows
/// the loaded Desert Chaser descriptor; the variant's two placements share it.
static AreaResource _gDryfieldParkingLotVariant2Resources[2] = {
    {
        .entryId        = DRYFIELD_PARKING_LOT_DESERT_CHASER_ENTRY_ID,
        .fileNumber     = DRYFIELD_PARKING_LOT_DESERT_CHASER_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor00100DesertChaserTaskDesc,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement-resource keys and decimal file numbers for the other actor variants.
enum {
    DRYFIELD_PARKING_LOT_SCORPION_ENTRY_ID        = 25,
    DRYFIELD_PARKING_LOT_SCORPION_FILE_NUMBER     = 25,
    DRYFIELD_PARKING_LOT_GOLEM_KNIGHT_ENTRY_ID    = 22,
    DRYFIELD_PARKING_LOT_GOLEM_KNIGHT_FILE_NUMBER = 22,
};

/// Variant 3's Desert Chaser and Scorpion load/spawn recipes, then the terminator.
///
/// Placement keys 1 and 25 select files 400100 and 302500. Both spawn descriptor
/// index 0; six Scorpion placements share their recipe and one uses the Chaser.
/// Loading and spawning borrow this table and the corresponding actor overlays.
static AreaResource _gDryfieldParkingLotVariant3Resources[3] = {
    {
        .entryId        = DRYFIELD_PARKING_LOT_DESERT_CHASER_ENTRY_ID,
        .fileNumber     = DRYFIELD_PARKING_LOT_DESERT_CHASER_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor00100DesertChaserTaskDesc,
    },
    {
        .entryId        = DRYFIELD_PARKING_LOT_SCORPION_ENTRY_ID,
        .fileNumber     = DRYFIELD_PARKING_LOT_SCORPION_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = gActor02500Tasks,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 7's GOLEM Knight load/spawn recipe for placement key 22, then the end.
///
/// File 402200 supplies descriptor index 0 for the sole placement. The room
/// table and actor overlay remain borrowed during loading, spawning and use.
static AreaResource _gDryfieldParkingLotVariant7Resources[2] = {
    {
        .entryId        = DRYFIELD_PARKING_LOT_GOLEM_KNIGHT_ENTRY_ID,
        .fileNumber     = DRYFIELD_PARKING_LOT_GOLEM_KNIGHT_FILE_NUMBER,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor402200GolemTaskDesc,
    },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldParkingLotAreaVariants[DRYFIELD_PARKING_LOT_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldParkingLotVariant1Placements, .resources = _gDryfieldParkingLotVariant1Resources },
    [2]                    = { .placements = gMapDryfieldParkingLotVariant2Placements, .resources = _gDryfieldParkingLotVariant2Resources },
    [3]                    = { .placements = gMapDryfieldParkingLotVariant3Placements, .resources = _gDryfieldParkingLotVariant3Resources },
    [7]                    = { .placements = gMapDryfieldParkingLotVariant7Placements, .resources = _gDryfieldParkingLotVariant7Resources },
};

/// Cue-2 footstep bases in the loaded type-1 bank; cue 1 selects base plus one.
enum {
    DRYFIELD_PARKING_LOT_CLASSES_5_AND_6_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x49,
    DRYFIELD_PARKING_LOT_CLASSES_5_AND_6_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x4B,
    DRYFIELD_PARKING_LOT_CLASS_2_WALK_CUE_BASE         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    DRYFIELD_PARKING_LOT_CLASS_2_RUN_CUE_BASE          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
    DRYFIELD_PARKING_LOT_CLASS_3_WALK_CUE_BASE         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_PARKING_LOT_CLASS_3_RUN_CUE_BASE          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
    DRYFIELD_PARKING_LOT_CLASS_3_JUMP_CUE_BASE         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x55,
};

/// Footstep cue bases shared by surface classes 5 and 6.
///
/// Walking and scripted jumps share entries 0x49/0x4A; running uses 0x4B/0x4C.
/// The surface records borrow these requests while the room overlay is loaded.
static WorldCollisionFootstepSounds _gDryfieldParkingLotClasses5And6FootstepSounds = {
    .walk         = DRYFIELD_PARKING_LOT_CLASSES_5_AND_6_WALK_CUE_BASE,
    .run          = DRYFIELD_PARKING_LOT_CLASSES_5_AND_6_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_PARKING_LOT_CLASSES_5_AND_6_WALK_CUE_BASE,
};

/// Class 2's footstep cue bases, with walking and scripted jumps sharing a pair.
///
/// The loaded type-1 bank supplies entries 0x3D/0x3E for walking and scripted
/// jumps, and 0x3F/0x40 for running. Each animation cue selects one of the pair.
static WorldCollisionFootstepSounds _gDryfieldParkingLotClass2FootstepSounds = {
    .walk         = DRYFIELD_PARKING_LOT_CLASS_2_WALK_CUE_BASE,
    .run          = DRYFIELD_PARKING_LOT_CLASS_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_PARKING_LOT_CLASS_2_WALK_CUE_BASE,
};

/// Class 3's separate walking, running and scripted-jump footstep cue bases.
///
/// Entries 0x51/0x52, 0x53/0x54 and 0x55/0x56 come from the loaded type-1 bank.
/// Scripted stairs select the jump pair independently of the grid's surface class.
static WorldCollisionFootstepSounds _gDryfieldParkingLotClass3FootstepSounds = {
    .walk         = DRYFIELD_PARKING_LOT_CLASS_3_WALK_CUE_BASE,
    .run          = DRYFIELD_PARKING_LOT_CLASS_3_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_PARKING_LOT_CLASS_3_JUMP_CUE_BASE,
};

/// Shared silent, probe-blocking surface for classes 0, 4 and 7.
///
/// Weapon impacts and pushback are enabled. All three directory slots borrow this record.
static WorldCollisionSurfaceProperties _gDryfieldParkingLotSilentBlockingSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 5's blocking, impact-enabled surface, with the cues shared by class 6.
///
/// Applies pushback and borrows the cue record for this overlay's loaded lifetime.
static WorldCollisionSurfaceProperties _gDryfieldParkingLotClass5SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldParkingLotClasses5And6FootstepSounds,
};

/// Class 2's blocking, impact-enabled surface with its own footstep cue bases.
///
/// Applies pushback and borrows the cue record for this overlay's loaded lifetime.
static WorldCollisionSurfaceProperties _gDryfieldParkingLotClass2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldParkingLotClass2FootstepSounds,
};

/// Class 3's blocking, impact-enabled surface with separate scripted-jump cues.
///
/// Applies pushback and borrows the cue record for this overlay's loaded lifetime.
static WorldCollisionSurfaceProperties _gDryfieldParkingLotClass3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldParkingLotClass3FootstepSounds,
};

/// Class 1's silent surface, passing probes and suppressing weapon impacts.
///
/// Pushback stays enabled independently of probe and impact policy.
static WorldCollisionSurfaceProperties _gDryfieldParkingLotClass1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 6's blocking, impact-enabled surface, with the cues shared by class 5.
///
/// Applies pushback and borrows the cue record for this overlay's loaded lifetime.
static WorldCollisionSurfaceProperties _gDryfieldParkingLotClass6SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldParkingLotClasses5And6FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldParkingLotSurfacePropertiesByClass[DRYFIELD_PARKING_LOT_SURFACE_CLASS_COUNT] = {
    &_gDryfieldParkingLotSilentBlockingSurface,
    &_gDryfieldParkingLotClass1SurfaceProperties,
    &_gDryfieldParkingLotClass2SurfaceProperties,
    &_gDryfieldParkingLotClass3SurfaceProperties,
    &_gDryfieldParkingLotSilentBlockingSurface,
    &_gDryfieldParkingLotClass5SurfaceProperties,
    &_gDryfieldParkingLotClass6SurfaceProperties,
    &_gDryfieldParkingLotSilentBlockingSurface,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 156, 190, 128 } };

RoomEventReq gRoomEventReq = { 0 };

static void _dryfieldParkingLotInitializeRoom(Task* task);

#include "../../shared/room/room_event.inc.c"

#include "../../shared/parking_lot_event_msg.inc.c"

#include "../../shared/parking_lot_sound_msg.inc.c"

/// Refuses every key-item use request in the daytime parking lot.
///
/// Ignores all arguments and returns zero, making the item menu report that
/// the selected item cannot be used here.
static s32 _dryfieldParkingLotRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { ROOM_KEY_ITEM_USE_UNAVAILABLE = 0 };

    return ROOM_KEY_ITEM_USE_UNAVAILABLE;
}

/// Ignores room commands in the daytime parking lot and returns zero.
///
/// Receives `ROOM_MESSAGE_COMMAND`; neither payload word nor the receiver is read.
static s32 _dryfieldParkingLotIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room action requests in the daytime parking lot and returns zero.
///
/// Receives `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed request is not read,
/// changed or retained, and the remaining arguments are ignored.
static s32 _dryfieldParkingLotIgnoreActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the daytime parking lot's room-message receiver and enters its idle state.
///
/// Requires the live room task in state 0. The message table and room slot
/// borrow the loaded overlay and task for subsequent synchronous dispatch.
static void _dryfieldParkingLotInitializeRoom(Task* task)
{
    task->msgTable = _gDryfieldParkingLotRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the initialized room task idle while its message table remains available.
///
/// State 1 does no per-frame work and does not advance or read the task.
static void _dryfieldParkingLotIdle(Task* task)
{
}

/// Room-task states 0..2: initialize the message receiver, wait, then teardown.
///
/// Copied as a complete 12-byte value before dispatch. Entries take a live task;
/// the final handler may release it. The room overlay must remain loaded.
static const TaskFuncTable3 _gDryfieldParkingLotRoomStateHandlers = {
    { _dryfieldParkingLotInitializeRoom, _dryfieldParkingLotIdle, taskKill },
};

void dryfieldParkingLotRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldParkingLotRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void dryfieldParkingLotUpdateViewEffectGateTask(Task* task)
{
    u8 mappedViewIndex;

    mappedViewIndex                  = viewGetMappedIndex();
    gRoomEffectState->roomEffectMode = _gDryfieldParkingLotViewEffectGates[mappedViewIndex - 1];
}
