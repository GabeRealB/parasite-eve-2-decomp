#include "rooms/dryfield_motel_lobby.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/room_common.h"

#include "../../shared/telephone.h"

static void _dryfieldMotelLobbyInitRoomTask(Task* task);
static void _dryfieldMotelLobbyIdleRoomTask(Task* unusedTask);

static WorldCollisionGrid    _gDryfieldMotelLobbyCollisionGrid;
static WorldCollisionTrigger _gDryfieldMotelLobbyViewBoundaryTriggers[4];
static WorldCollisionTrigger _gDryfieldMotelLobbyActionTriggers[7];
static WorldCoordRoomLights  _gDryfieldMotelLobbyLights;
static s32                   _dryfieldMotelLobbyRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32                   _dryfieldMotelLobbyAcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                   _dryfieldMotelLobbyIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);
static s32                   _dryfieldMotelLobbyIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

/// Lobby warp selectors and door/footstep sound-script requests.
enum {
    DRYFIELD_MOTEL_LOBBY_ENTRY_VIEW             = 2,
    DRYFIELD_MOTEL_LOBBY_PARKING_LOT_WARP_SLOTS = (1 << 4) | 2, // Lobby departure slot 1, parking-lot arrival slot 2
    DRYFIELD_MOTEL_LOBBY_DOOR_MAP_FLAG          = 481,
    DRYFIELD_MOTEL_LOBBY_SOUND_ARRIVAL          = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_LOBBY, 2),
    DRYFIELD_MOTEL_LOBBY_SOUND_DEPARTURE        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_LOBBY, 1),
    DRYFIELD_MOTEL_LOBBY_SOUND_WALK             = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    DRYFIELD_MOTEL_LOBBY_SOUND_RUN              = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F,
};

/// Dimensions of the lobby's X-major collision-cell table, in cells and world units.
enum {
    DRYFIELD_MOTEL_LOBBY_GRID_CELLS_X   = 2,
    DRYFIELD_MOTEL_LOBBY_GRID_CELLS_Z   = 2,
    DRYFIELD_MOTEL_LOBBY_GRID_CELL_SIZE = 4000,
};

/// Sprite colour modulation and the split between view 2's two source ranges.
enum {
    DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION = 128,
    DRYFIELD_MOTEL_LOBBY_VIEW2_FIRST_BATCH_COUNT   = 18,
};

/// CAP command slots selected by the lobby's examination triggers.
///
/// Command 3 opens the telephone menu; the other slots play examination text.
enum {
    DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_1 = 1,
    DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_2 = 2,
    DRYFIELD_MOTEL_LOBBY_CAP_TELEPHONE     = 3,
    DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_4 = 4,
    DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_5 = 5,
    DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_6 = 6,
};

#include "../../shared/telephone_data.inc.c"

/// Synchronous room-message handlers, borrowed while the lobby task is registered.
///
/// Accepts unchanged room transitions, refuses key items and ignores directed
/// actions and commands. The final marker makes unsupported messages return zero.
static TaskMessageEntry _gDryfieldMotelLobbyRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelLobbyAcceptRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelLobbyRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelLobbyIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelLobbyIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gDryfieldMotelLobbyRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldMotelLobbyCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldMotelLobbyViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldMotelLobbyActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldMotelLobbyRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldMotelLobbyRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldMotelLobbyViewCameras) };

WorldCoordRoomLighting gDryfieldMotelLobbyRoomLighting[1] = {
    { .lights = &_gDryfieldMotelLobbyLights, .ambientTable = NULL },
};

DirectionWarpEntry gDryfieldMotelLobbyWarpEntries[1] = {
    {
        .player         = { { .word = 0 }, 2940, 0, 316 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 2940, 0, 316 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_MOTEL_LOBBY_SOUND_ARRIVAL,
        .departureSound = DRYFIELD_MOTEL_LOBBY_SOUND_DEPARTURE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_MOTEL_LOBBY_ENTRY_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_MOTEL_LOBBY_DOOR_MAP_FLAG,
    },
};

static SVECTOR _gDryfieldMotelLobbyCollisionGridNormals[9] = {
#include "assets/dryfield_motel_lobby_collision_02624_normals.inc"
};

static SVECTOR _gDryfieldMotelLobbyCollisionGridVerts[42] = {
#include "assets/dryfield_motel_lobby_collision_02624_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelLobbyCollisionGridFaces[25] = {
#include "assets/dryfield_motel_lobby_collision_02624_faces.inc"
};

static s16 _gDryfieldMotelLobbyCollisionGridCells[64] = {
#include "assets/dryfield_motel_lobby_collision_02624_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelLobbyCollisionGridCells[i])
static s16* _gDryfieldMotelLobbyCollisionGridTable[DRYFIELD_MOTEL_LOBBY_GRID_CELLS_X * DRYFIELD_MOTEL_LOBBY_GRID_CELLS_Z] = {
#include "assets/dryfield_motel_lobby_collision_02624_table.inc"
};
#undef GRID_CELL

/// The lobby's room-coordinate collision mesh and four XZ face-index lists.
///
/// Contains 25 faces, nine normals and 42 vertices. Cell lists end at -1;
/// their 64-halfword storage includes retained words between live lists.
/// Activation binds the writable descriptor to the current view; its pools
/// remain borrowed for the lobby overlay's loaded lifetime.
static WorldCollisionGrid _gDryfieldMotelLobbyCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelLobbyCollisionGridNormals,
    .vertices    = _gDryfieldMotelLobbyCollisionGridVerts,
    .faces       = _gDryfieldMotelLobbyCollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelLobbyCollisionGridTable,
    .xBias       = 0,
    .zBias       = 0,
    .cellCountX  = DRYFIELD_MOTEL_LOBBY_GRID_CELLS_X,
    .cellCountZ  = DRYFIELD_MOTEL_LOBBY_GRID_CELLS_Z,
    .cellSize    = DRYFIELD_MOTEL_LOBBY_GRID_CELL_SIZE,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelLobbyCollisionGridFaces),
};

ViewCamera gDryfieldMotelLobbyViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3000, 25000, -3000 } }, 680 },
    { { { { -3992, 0, -915 }, { -291, 3882, 1272 }, { 867, 1305, -3784 } }, { -1790, 2360, -5690 } }, 230 },
    { { { { 4037, 0, -691 }, { -204, 3912, -1195 }, { 660, 1212, 3856 } }, { -1790, 2360, -340 } }, 230 },
    { { { { 3142, 0, -2627 }, { -1618, 3226, -1936 }, { 2069, 2523, 2475 } }, { -3510, 2960, -1660 } }, 230 },
    { { { { -3394, 0, 2291 }, { 1829, 2466, 2710 }, { -1379, 3270, -2044 } }, { -4690, 1730, -2640 } }, 289 },
};

/// View 1's empty sprite list, retaining decoded image strips.
static SpriteBatch _gDryfieldMotelLobbyView1SpriteBatches[2] = {
    { 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 2's 50 textured rectangles, positioned in pixels with unscaled sorting depths.
///
/// The view's two batches select all elements. Texture pixels come from the
/// 8-bit page at (960, 0), using palette row 255; RGB 128 is neutral modulation.
static SpriteSource _gDryfieldMotelLobbyView2SpriteSources[50] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, -24, 1089, { .fields = { 80, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 32, 1167, { .fields = { 80, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -64, -8, 1114, { .fields = { 96, 240 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, -8, 1114, { .fields = { 88, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -88, -8, 1064, { .fields = { 96, 128 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -104, -16, 1023, { .fields = { 72, 168 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, -8, 950, { .fields = { 96, 112 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -120, -8, 1014, { .fields = { 88, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -120, 8, 1107, { .fields = { 72, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -136, -8, 1014, { .fields = { 88, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -136, 8, 1139, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -152, -16, 1129, { .fields = { 72, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -152, 0, 1146, { .fields = { 72, 96 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -152, 16, 1179, { .fields = { 80, 112 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, -32, 1079, { .fields = { 88, 96 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, -16, 1090, { .fields = { 88, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, 0, 1163, { .fields = { 88, 240 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, 16, 1175, { .fields = { 88, 144 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 24, 740, { .fields = { 80, 144 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -64, 48, 1187, { .fields = { 96, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, 0, 1112, { .fields = { 88, 128 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, 16, 1162, { .fields = { 120, 184 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -72, 40, 1175, { .fields = { 120, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, 0, 1075, { .fields = { 120, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, 24, 1087, { .fields = { 120, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, 48, 1087, { .fields = { 120, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 0, 1000, { .fields = { 112, 88 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 24, 1000, { .fields = { 112, 136 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 48, 1025, { .fields = { 112, 112 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -96, 72, 1025, { .fields = { 72, 128 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 0, 920, { .fields = { 72, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -112, 64, 937, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -112, 88, 937, { .fields = { 104, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 8, 842, { .fields = { 80, 48 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 24, 825, { .fields = { 88, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -128, 40, 837, { .fields = { 96, 88 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -128, 64, 862, { .fields = { 96, 24 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -128, 88, 862, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 16, 763, { .fields = { 96, 144 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 32, 750, { .fields = { 96, 48 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -144, 48, 775, { .fields = { 104, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -144, 72, 775, { .fields = { 104, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -144, 96, 775, { .fields = { 104, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 32, 725, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 48, 725, { .fields = { 80, 16 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, 64, 725, { .fields = { 104, 184 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -160, 88, 725, { .fields = { 112, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 16, 876, { .fields = { 88, 192 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 32, 906, { .fields = { 88, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 48, 912, { .fields = { 88, 224 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 2's two sprite ranges, preceded by the zero-count image-strip selector.
///
/// Both ranges are initially visible and processed by cached drawing. The
/// first range retains an unexplained 1 in the batch's serialized bytes.
static SpriteBatch _gDryfieldMotelLobbyView2SpriteBatches[4] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = DRYFIELD_MOTEL_LOBBY_VIEW2_FIRST_BATCH_COUNT, .field_6 = { 1, 0 } },
    { .firstSprite = DRYFIELD_MOTEL_LOBBY_VIEW2_FIRST_BATCH_COUNT, .spriteCount = ARRAY_SIZE(_gDryfieldMotelLobbyView2SpriteSources) - DRYFIELD_MOTEL_LOBBY_VIEW2_FIRST_BATCH_COUNT },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 3's 52 textured rectangles, positioned in pixels with unscaled sorting depths.
///
/// Its single batch selects every element. Uses the 8-bit texture page at
/// (960, 0), palette row 255 and neutral RGB modulation.
static SpriteSource _gDryfieldMotelLobbyView3SpriteSources[52] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -16, 1103, { .fields = { 88, 48 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 0, 986, { .fields = { 96, 224 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, 0, 994, { .fields = { 96, 144 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 24, 1414, { .fields = { 96, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 32, 1310, { .fields = { 96, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 40, 1230, { .fields = { 120, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 48, 1150, { .fields = { 96, 128 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, 48, 687, { .fields = { 96, 136 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 56, 650, { .fields = { 96, 168 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 64, 1041, { .fields = { 96, 200 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, 72, 976, { .fields = { 96, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 80, 925, { .fields = { 96, 192 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, 104, 791, { .fields = { 96, 184 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, 112, 753, { .fields = { 96, 216 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 112, 700, { .fields = { 96, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 64, -24, 1130, { .fields = { 104, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -16, 1237, { .fields = { 96, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 72, -16, 1066, { .fields = { 104, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 80, -8, 1052, { .fields = { 88, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 0, 1240, { .fields = { 104, 184 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 64, 0, 1146, { .fields = { 104, 136 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 88, 0, 1033, { .fields = { 88, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 72, 8, 1076, { .fields = { 112, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 80, 8, 1000, { .fields = { 80, 16 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 96, 8, 922, { .fields = { 72, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 120, 8, 750, { .fields = { 80, 112 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 80, 16, 1008, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 88, 16, 946, { .fields = { 120, 144 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 96, 16, 875, { .fields = { 72, 8 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, 24, 1253, { .fields = { 96, 96 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 64, 24, 1159, { .fields = { 112, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 96, 24, 889, { .fields = { 120, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 104, 32, 846, { .fields = { 120, 72 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 112, 40, 797, { .fields = { 104, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 120, 40, 775, { .fields = { 112, 120 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 128, 48, 750, { .fields = { 120, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 144, 48, 675, { .fields = { 88, 240 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 136, 56, 725, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 144, 56, 700, { .fields = { 112, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 96, 64, 898, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 112, 64, 806, { .fields = { 104, 56 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 152, 64, 662, { .fields = { 96, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 96, 80, 891, { .fields = { 96, 48 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 152, 80, 675, { .fields = { 104, 96 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 120, 96, 770, { .fields = { 96, 16 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, -16, 1226, { .fields = { 96, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 104, 24, 825, { .fields = { 72, 24 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 128, 24, 750, { .fields = { 72, 56 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 128, 40, 725, { .fields = { 80, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 144, 40, 678, { .fields = { 80, 72 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 112, 32, 750, { .fields = { 72, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 136, 32, 700, { .fields = { 72, 40 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 3's complete sprite range, retaining decoded image strips.
static SpriteBatch _gDryfieldMotelLobbyView3SpriteBatches[3] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldMotelLobbyView3SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 4's 71 textured rectangles, positioned in pixels with unscaled sorting depths.
///
/// Its single batch selects every element. Uses the 8-bit texture page at
/// (960, 0), palette row 255 and neutral RGB modulation.
static SpriteSource _gDryfieldMotelLobbyView4SpriteSources[71] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, -8, 837, { .fields = { 104, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, -8, 818, { .fields = { 88, 24 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, 0, 593, { .fields = { 80, 72 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -128, 8, 887, { .fields = { 72, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 8, 730, { .fields = { 64, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -56, 24, 670, { .fields = { 56, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 32, 750, { .fields = { 56, 16 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -40, 40, 612, { .fields = { 48, 64 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, 40, 620, { .fields = { 48, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -24, 56, 574, { .fields = { 48, 160 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 24, 64, 558, { .fields = { 48, 216 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, -24, 862, { .fields = { 48, 200 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -120, -24, 837, { .fields = { 48, 184 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -136, -24, 837, { .fields = { 48, 168 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -88, -8, 806, { .fields = { 48, 136 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -88, 8, 738, { .fields = { 48, 120 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 24, 680, { .fields = { 96, 200 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 48, 860, { .fields = { 96, 224 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, -8, 806, { .fields = { 48, 72 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -104, 8, 850, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, 32, 957, { .fields = { 56, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -120, -8, 837, { .fields = { 56, 56 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 16, 708, { .fields = { 56, 40 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 32, 655, { .fields = { 56, 24 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -72, 48, 602, { .fields = { 88, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 72, 775, { .fields = { 64, 184 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 88, 781, { .fields = { 64, 168 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 104, 782, { .fields = { 64, 136 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -56, 32, 655, { .fields = { 64, 120 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -40, 48, 609, { .fields = { 64, 104 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -40, 64, 569, { .fields = { 64, 88 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -24, 64, 606, { .fields = { 64, 72 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 16, 72, 551, { .fields = { 112, 240 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 32, 56, 586, { .fields = { 48, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 32, 72, 551, { .fields = { 48, 104 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 32, 88, 518, { .fields = { 112, 112 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, 48, 609, { .fields = { 48, 88 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 48, 64, 569, { .fields = { 72, 48 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, 32, 655, { .fields = { 64, 216 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 48, 609, { .fields = { 80, 104 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 72, 700, { .fields = { 64, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, 96, 750, { .fields = { 80, 128 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 80, 40, 631, { .fields = { 80, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 80, 64, 750, { .fields = { 80, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -104, 48, 942, { .fields = { 80, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -112, 32, 962, { .fields = { 72, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 72, 853, { .fields = { 72, 24 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 80, 88, 793, { .fields = { 112, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 80, 530, { .fields = { 64, 200 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 0, 96, 503, { .fields = { 96, 80 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 80, 534, { .fields = { 64, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -16, 96, 503, { .fields = { 96, 128 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 80, 16, 697, { .fields = { 88, 56 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 88, 518, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -40, 80, 534, { .fields = { 104, 40 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -56, 80, 716, { .fields = { 112, 0 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, 48, 606, { .fields = { 112, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 48, 88, 518, { .fields = { 112, 208 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, 8, 722, { .fields = { 96, 104 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, 32, 655, { .fields = { 80, 224 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, 56, 825, { .fields = { 80, 200 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, 80, 825, { .fields = { 96, 176 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 112, 8, 725, { .fields = { 96, 152 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 112, 32, 800, { .fields = { 112, 144 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 112, 64, 825, { .fields = { 56, 216 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 128, 8, 750, { .fields = { 56, 232 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 128, 24, 825, { .fields = { 40, 16 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 128, 40, 825, { .fields = { 40, 32 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -136, 0, 887, { .fields = { 96, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -120, 8, 900, { .fields = { 80, 248 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -120, 16, 975, { .fields = { 40, 48 } }, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOBBY_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 4's complete sprite range, retaining decoded image strips.
static SpriteBatch _gDryfieldMotelLobbyView4SpriteBatches[3] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldMotelLobbyView4SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 5's empty sprite list, retaining decoded image strips.
static SpriteBatch _gDryfieldMotelLobbyView5SpriteBatches[2] = {
    { 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldMotelLobbySpriteViews[5] = {
    { { .empty = _gDryfieldMotelLobbyView1SpriteBatches }, _gDryfieldMotelLobbyView1SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLobbyView2SpriteSources }, _gDryfieldMotelLobbyView2SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLobbyView3SpriteSources }, _gDryfieldMotelLobbyView3SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLobbyView4SpriteSources }, _gDryfieldMotelLobbyView4SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelLobbyView5SpriteBatches }, _gDryfieldMotelLobbyView5SpriteBatches, NULL },
};

/// Directed view boundaries for 3 to 2, 2 to 3, 4 to 3 and 3 to 4.
///
/// The fourth record carries LAST and is included in binding and collision.
/// Positions/radii use world units; normals use Q12. Room setup changes
/// coordinates, list links, enable flags and hit latches in these live records.
static WorldCollisionTrigger _gDryfieldMotelLobbyViewBoundaryTriggers[4] = {
    {
        .origin       = { 2241, -896, 3011, 0 },
        .vertices     = { { -1888, 1024, 0, 0 }, { 1888, 1024, 0, 0 }, { -1888, -1024, 0, 0 }, { 1888, -1024, 0, 0 } },
        .normal       = { 0, 0, 4100, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2141,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 2,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 2144, -864, 3392, 0 },
        .vertices     = { { 1888, 1024, 0, 0 }, { -1888, 1024, 0, 0 }, { 1888, -1024, 0, 0 }, { -1888, -1024, 0, 0 } },
        .normal       = { 0, 0, -4101, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 2141,
        .control      = 0,
        .parameter0   = 2,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 4000, -896, 5264, 0 },
        .vertices     = { { 0, 1024, 848, 0 }, { 0, 1024, -848, 0 }, { 0, -1024, 848, 0 }, { 0, -1024, -848, 0 } },
        .normal       = { 4102, 0, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1324,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
    },
    {
        .origin       = { 4095, -896, 5248, 0 },
        .vertices     = { { 2, 1024, -848, 0 }, { -1, 1024, 848, 0 }, { 2, -1024, -848, 0 }, { -1, -1024, 848, 0 } },
        .normal       = { -4103, 0, -10, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1324,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST,
    },
};

/// The parking-lot departure and six CAP examination triggers, including the telephone.
///
/// The warp packs lobby departure slot 1 and parking-lot arrival slot 2.
/// CAP slots are 5, 3 (telephone), 4, 6, 2 and 1; the last record carries LAST.
/// Records stay writable and loaded until their runtime list is cleared.
static WorldCollisionTrigger _gDryfieldMotelLobbyActionTriggers[7] = {
    {
        .origin       = { 2992, -48, 320, 0 },
        .vertices     = { { -848, 0, -256, 0 }, { 848, 0, -256, 0 }, { -848, 0, 256, 0 }, { 848, 0, 256, 0 } },
        .normal       = { 0, 4102, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 884,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_DRYFIELD_PARKING_LOT,
        .parameter1   = DRYFIELD_MOTEL_LOBBY_PARKING_LOT_WARP_SLOTS,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 5872, -64, 3104, 0 },
        .vertices     = { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } },
        .normal       = { 0, ONE, 0, 0 },
        .facingNormal = { -4091, 0, 201, 0 },
        .radius       = 523,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_5,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 4656, -64, 2280, 0 },
        .vertices     = { { -1200, 0, -744, 0 }, { 816, 0, -712, 0 }, { -432, 0, 728, 0 }, { 816, 0, 728, 0 } },
        .normal       = { 0, 4108, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1408,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_MOTEL_LOBBY_CAP_TELEPHONE,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
    },
    {
        .origin       = { 4016, -64, 3456, 0 },
        .vertices     = { { -816, 0, -640, 0 }, { 816, 0, -640, 0 }, { -816, 0, 640, 0 }, { 816, 0, 640, 0 } },
        .normal       = { 0, 4095, 0, 0 },
        .facingNormal = { 0, 0, ONE, 0 },
        .radius       = 1031,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_4,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD,
    },
    {
        .origin       = { 5808, -64, 976, 0 },
        .vertices     = { { -320, 0, -880, 0 }, { 320, 0, -880, 0 }, { -320, 0, 880, 0 }, { 320, 0, 880, 0 } },
        .normal       = { 0, 4119, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 936,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_6,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 5312, -64, 4928, 0 },
        .vertices     = { { -320, 0, -624, 0 }, { 320, 0, -624, 0 }, { -320, 0, 624, 0 }, { 320, 0, 624, 0 } },
        .normal       = { 0, 4102, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 701,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_2,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
    },
    {
        .origin       = { 5888, -64, 3984, 0 },
        .vertices     = { { -320, 0, -416, 0 }, { 320, 0, -416, 0 }, { -320, 0, 416, 0 }, { 320, 0, 416, 0 } },
        .normal       = { 0, ONE, 0, 0 },
        .facingNormal = { -ONE, 0, 0, 0 },
        .radius       = 523,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = DRYFIELD_MOTEL_LOBBY_CAP_INTERACTION_1,
        .parameter1   = 0,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
    },
};

/// Point lights contributing in every view of the Dryfield motel lobby.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full intensity). The room-light collection
/// borrows this writable array while the overlay is loaded. Coordinate updates
/// set its parents and composed matrices; lighting queries replace attenuation.
static WorldCoordPointLight _gDryfieldMotelLobbyPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -2500, 1200 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3768, 3768, 3522 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -1610, 704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -1610, 704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -1610, 704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 2000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -2500, 5450 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2949, 2949, 2703 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 3000,
        .outer = 6000,
    },
};

/// The lobby's five mutable point lights, with no directional or cone lights.
///
/// Counts measure elements. The collection borrows its room-owned array;
/// coordinate updates and shading overwrite transforms and attenuation.
static WorldCoordRoomLights _gDryfieldMotelLobbyLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelLobbyPointLights),
    .pointLights           = _gDryfieldMotelLobbyPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Paired walking, running and scripted-jump sound cues for the lobby floor.
///
/// Type-1 requests use the loaded footstep bank. Cue 2 uses the base entry,
/// cue 1 adds one; scripted jumps use the walking pair.
static WorldCollisionFootstepSounds _gDryfieldMotelLobbyFloorFootstepSounds = {
    .walk         = DRYFIELD_MOTEL_LOBBY_SOUND_WALK,
    .run          = DRYFIELD_MOTEL_LOBBY_SOUND_RUN,
    .scriptedJump = DRYFIELD_MOTEL_LOBBY_SOUND_WALK,
};

/// Shared blocking, impact-enabled, pushback surface without footstep cues.
///
/// Used by lobby surface classes 0 and 2..7. The leading stored byte has
/// no established consumer and retains its authored zero.
static WorldCollisionSurfaceProperties _gDryfieldMotelLobbySilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's main-floor collision policy and paired footstep cues.
///
/// Blocks probes, permits weapon impacts and applies pushback. The leading
/// stored byte has no established consumer and retains its authored zero.
static WorldCollisionSurfaceProperties _gDryfieldMotelLobbyFloorSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldMotelLobbyFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldMotelLobbySurfacePropertiesByClass[8] = {
    &_gDryfieldMotelLobbySilentSurfaceProperties,
    &_gDryfieldMotelLobbyFloorSurfaceProperties,
    &_gDryfieldMotelLobbySilentSurfaceProperties,
    &_gDryfieldMotelLobbySilentSurfaceProperties,
    &_gDryfieldMotelLobbySilentSurfaceProperties,
    &_gDryfieldMotelLobbySilentSurfaceProperties,
    &_gDryfieldMotelLobbySilentSurfaceProperties,
    &_gDryfieldMotelLobbySilentSurfaceProperties,
};

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\xAD\x1A"
#include "../../shared/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void dryfieldMotelLobbyTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

/// Refuses every key-item-use request in the motel lobby.
///
/// Ignores the collected item ID and other arguments. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, leaving the room and inventory unchanged.
static s32 _dryfieldMotelLobbyRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition from the motel lobby with its destination unchanged.
///
/// Borrows a readable eight-byte request and writable reply for synchronous
/// dispatch; they may be the same record. Copies the complete request for both
/// query and execution calls, retains neither pointer, and returns 1.
static s32 _dryfieldMotelLobbyAcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_LOBBY_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_LOBBY_TRANSITION_ALLOWED;
}

/// Ignores room commands in the motel lobby and returns zero.
///
/// The command word and other arguments are unused; no room state changes.
static s32 _dryfieldMotelLobbyIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores directed room actions in the motel lobby and returns zero.
///
/// The borrowed action request is neither read nor retained; all arguments
/// are unused and no room state changes.
static s32 _dryfieldMotelLobbyIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the motel lobby's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. The message table is borrowed from this overlay,
/// which must stay loaded while the registered task can receive messages.
static void _dryfieldMotelLobbyInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldMotelLobbyRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the lobby's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldMotelLobbyIdleRoomTask(Task* unusedTask)
{
}

/// Room-task handlers for state 0 (register), 1 (idle) and 2 (release).
///
/// Dispatch copies the complete three-pointer value, then indexes it without
/// a bounds check. The lobby overlay and callback code must stay loaded.
static const TaskFuncTable3 _gDryfieldMotelLobbyRoomTaskStates = {
    {
        _dryfieldMotelLobbyInitRoomTask,
        _dryfieldMotelLobbyIdleRoomTask,
        taskKill,
    },
};

void dryfieldMotelLobbyTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMotelLobbyRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
