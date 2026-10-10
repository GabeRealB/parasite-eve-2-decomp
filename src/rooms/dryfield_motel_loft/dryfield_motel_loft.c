#include "rooms/dryfield_motel_loft.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

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
#include "main/task.h"
#include "main/task_types.h"

static s32 _dryfieldMotelLoftRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _dryfieldMotelLoftAcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldMotelLoftIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);
static s32 _dryfieldMotelLoftIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

static WorldCollisionGrid    _gDryfieldMotelLoftCollisionGrid;
static WorldCollisionTrigger _gDryfieldMotelLoftViewBoundaries[12];
static WorldCollisionTrigger _gDryfieldMotelLoftActionTriggers[2];
static WorldCoordRoomLights  _gDryfieldMotelLoftRoomLights;

/// Four room-message handlers followed by the unsupported-message terminator.
///
/// Accepts unchanged transitions, refuses key items and ignores directed actions
/// and room commands. The room task borrows the table and synchronous callbacks
/// while this overlay is loaded; each handler determines its payload types.
static TaskMessageEntry _gDryfieldMotelLoftRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldMotelLoftAcceptRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldMotelLoftRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldMotelLoftIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldMotelLoftIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

WorldCollisionRoomResources gDryfieldMotelLoftRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldMotelLoftCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldMotelLoftViewBoundaries,
        .actionTriggers       = _gDryfieldMotelLoftActionTriggers,
        .occluders            = NULL,
    },
};

u8* gDryfieldMotelLoftRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldMotelLoftRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldMotelLoftViewCameras) };

WorldCoordRoomLighting gDryfieldMotelLoftRoomLighting[1] = {
    { .lights = &_gDryfieldMotelLoftRoomLights, .ambientTable = NULL },
};

/// Endpoint and arrival selectors packed in the balcony warp trigger.
enum {
    DRYFIELD_MOTEL_LOFT_WARP_BALCONY         = 1,
    DRYFIELD_MOTEL_LOFT_BALCONY_ARRIVAL_WARP = 4,
    DRYFIELD_MOTEL_LOFT_ARRIVAL_VIEW         = 2,
    DRYFIELD_MOTEL_LOFT_MAP_FLAG             = 466,
};

/// Loft-bank scripts for entering and leaving its balcony endpoint.
enum {
    DRYFIELD_MOTEL_LOFT_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_LOFT, 2),
    DRYFIELD_MOTEL_LOFT_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_LOFT, 1),
};

DirectionWarpEntry gDryfieldMotelLoftWarpEntries[1] = {
    {
        .player         = { { .word = 0 }, 3953, 0, -2156 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 3953, 0, -2156 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_MOTEL_LOFT_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_MOTEL_LOFT_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = DRYFIELD_MOTEL_LOFT_ARRIVAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_MOTEL_LOFT_MAP_FLAG,
    },
};

/// Eight grid-local collision normals, with 4096 for unit length.
static SVECTOR _gDryfieldMotelLoftCollisionGridNormals[8] = {
#include "assets/dryfield_motel_loft_collision_003FC_normals.inc"
};

/// Thirty grid-local mesh vertices in signed game-coordinate units.
static SVECTOR _gDryfieldMotelLoftCollisionGridVerts[30] = {
#include "assets/dryfield_motel_loft_collision_003FC_verts.inc"
};

/// Seventeen indexed quads: class 1 is the room floor; the rest use class 0.
static WorldCollisionGridFace _gDryfieldMotelLoftCollisionGridFaces[17] = {
#include "assets/dryfield_motel_loft_collision_003FC_faces.inc"
};

/// Signed face-index pool for six cells, whose lists each end with -1.
///
/// Retains all 58 serialized halfwords, including values after list terminators.
static s16 _gDryfieldMotelLoftCollisionGridCells[58] = {
#include "assets/dryfield_motel_loft_collision_003FC_cells.inc"
};

// Generated offsets count s16 elements in this grid's cell-list pool.
#define GRID_CELL(i) (&_gDryfieldMotelLoftCollisionGridCells[i])
/// X-major pointers into the face-index pool for the grid's three by two cells.
static s16* _gDryfieldMotelLoftCollisionGridTable[6] = {
#include "assets/dryfield_motel_loft_collision_003FC_table.inc"
};
#undef GRID_CELL

/// The loft's collision mesh and its three by two XZ cell index.
///
/// Cells are 4000 game units wide, with origin (-6000, -2500) on XZ.
/// Room setup binds this writable descriptor to the current view; it and its
/// mesh pools must remain loaded while the grid is active.
static WorldCollisionGrid _gDryfieldMotelLoftCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldMotelLoftCollisionGridNormals,
    .vertices    = _gDryfieldMotelLoftCollisionGridVerts,
    .faces       = _gDryfieldMotelLoftCollisionGridFaces,
    .cellFaceIds = _gDryfieldMotelLoftCollisionGridTable,
    .xBias       = 6000,
    .zBias       = 2500,
    .cellCountX  = 3,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldMotelLoftCollisionGridFaces),
};

ViewCamera gDryfieldMotelLoftViewCameras[7] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x6978, 0 } }, 541 },
    { { { { 824, 0, -4012 }, { -771, 4019, -158 }, { 3937, 787, 809 } }, { 180, 1430, 1300 } }, 257 },
    { { { { 1131, 0, 3936 }, { 443, 4069, -127 }, { -3911, 461, 1124 } }, { -5880, 1430, 1620 } }, 257 },
    { { { { 388, 0, 4077 }, { -370, 4079, 35 }, { -4060, -372, 386 } }, { -600, 740, 1510 } }, 257 },
    { { { { -3971, 0, -1003 }, { -512, 3522, 2027 }, { 863, 2090, -3414 } }, { 5710, 2360, -2340 } }, 257 },
    { { { { -800, 0, 4016 }, { 475, 4067, 94 }, { -3988, 484, -795 } }, { -960, 1500, -1900 } }, 257 },
    { { { { -1105, 0, -3943 }, { -458, 4068, 128 }, { 3917, 476, -1098 } }, { 4980, 1500, -2080 } }, 257 },
};

/// View 1's empty sprite list, retaining the decoded image background.
static SpriteBatch _gDryfieldMotelLoftView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list, retaining the decoded image background.
static SpriteBatch _gDryfieldMotelLoftView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list, retaining the decoded image background.
static SpriteBatch _gDryfieldMotelLoftView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Colour modulation that leaves the room-sprite texture at its original brightness.
enum { DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION = 128 };

/// Nine depth-sorted source rectangles for view 4, in view-centred pixels.
///
/// Uses the eight-bit texture page at VRAM (960, 0) and the CLUT at (0, 255).
/// The room renderer borrows this writable array while the loft is loaded.
static SpriteSource _gDryfieldMotelLoftView4SpriteSources[9] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 56, -96, 875, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, 56, 0, 875, { .fields = { 88, 112 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 120 } }, 72, -120, 750, { .fields = { 104, 120 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 112 } }, 72, 0, 750, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 40, -80, 1050, { .fields = { 64, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, 40, 0, 1050, { .fields = { 72, 96 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 32, -56, 1150, { .fields = { 72, 184 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 32, 0, 1150, { .fields = { 80, 184 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 120 } }, 24, -48, 1200, { .fields = { 120, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 4's background selector, complete source range and terminal batch.
static SpriteBatch _gDryfieldMotelLoftView4SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMotelLoftView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Eight depth-sorted source rectangles for view 5, in view-centred pixels.
///
/// Uses the eight-bit texture page at VRAM (960, 0) and the CLUT at (0, 255).
/// The room renderer borrows this writable array while the loft is loaded.
static SpriteSource _gDryfieldMotelLoftView5SpriteSources[8] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -64, -120, 750, { .fields = { 0, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 112 } }, -64, -24, 750, { .fields = { 96, 112 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -96, -120, 650, { .fields = { 32, 96 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 112 } }, -96, -24, 650, { .fields = { 96, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -128, -120, 600, { .fields = { 0, 96 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 104 } }, -128, -24, 600, { .fields = { 64, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -160, -120, 575, { .fields = { 64, 104 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, -160, -24, 575, { .fields = { 32, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 5's background selector, complete source range and terminal batch.
static SpriteBatch _gDryfieldMotelLoftView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMotelLoftView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seven depth-sorted source rectangles for view 6, in view-centred pixels.
///
/// Uses the eight-bit texture page at VRAM (960, 0) and the CLUT at (0, 255).
/// The room renderer borrows this writable array while the loft is loaded.
static SpriteSource _gDryfieldMotelLoftView6SpriteSources[7] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -16, -56, 1325, { .fields = { 120, 104 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 80 } }, -64, 8, 1125, { .fields = { 72, 80 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 80 } }, -64, -72, 1125, { .fields = { 80, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, -40, -64, 1250, { .fields = { 56, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, -40, 0, 1250, { .fields = { 72, 160 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 104 } }, -88, 16, 1000, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, -88, -80, 1000, { .fields = { 96, 104 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 6's background selector, complete source range and terminal batch.
static SpriteBatch _gDryfieldMotelLoftView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMotelLoftView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Seven depth-sorted source rectangles for view 7, in view-centred pixels.
///
/// Uses the eight-bit texture page at VRAM (960, 0) and the CLUT at (0, 255).
/// The room renderer borrows this writable array while the loft is loaded.
static SpriteSource _gDryfieldMotelLoftView7SpriteSources[7] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, 8, 1500, { .fields = { 120, 200 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 0, -64, 1425, { .fields = { 40, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 0, 0, 1425, { .fields = { 72, 168 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, 32, -72, 1250, { .fields = { 72, 96 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 96 } }, 32, 0, 1250, { .fields = { 72, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 104 } }, 64, 16, 1000, { .fields = { 104, 0 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, 64, -80, 1000, { .fields = { 104, 104 } }, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, DRYFIELD_MOTEL_LOFT_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 7's background selector, complete source range and terminal batch.
static SpriteBatch _gDryfieldMotelLoftView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldMotelLoftView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldMotelLoftSpriteViews[7] = {
    { { .empty = _gDryfieldMotelLoftView1SpriteBatches }, _gDryfieldMotelLoftView1SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelLoftView2SpriteBatches }, _gDryfieldMotelLoftView2SpriteBatches, NULL },
    { { .empty = _gDryfieldMotelLoftView3SpriteBatches }, _gDryfieldMotelLoftView3SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLoftView4SpriteSources }, _gDryfieldMotelLoftView4SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLoftView5SpriteSources }, _gDryfieldMotelLoftView5SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLoftView6SpriteSources }, _gDryfieldMotelLoftView6SpriteBatches, NULL },
    { { .elements = _gDryfieldMotelLoftView7SpriteSources }, _gDryfieldMotelLoftView7SpriteBatches, NULL },
};

/// Twelve directed view-boundary quads, with the final record included and marked LAST.
///
/// Connects views 2..7 in both directions. Room setup binds the coordinates and
/// links/enables the writable records; positions use room units and normals
/// use 4096 per unit. Unlink the borrowed records before unloading this overlay.
static WorldCollisionTrigger _gDryfieldMotelLoftViewBoundaries[12] = {
    { NULL, NULL, NULL, { 3007, -1040, -417, 0 }, { { -4, -1264, -2500, 0 }, { -4, 1264, -2500, 0 }, { -3, -1264, 2489, 0 }, { -3, 1264, 2489, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2792, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2782, -1088, -354, 0 }, { { -3, -1264, 2492, 0 }, { -3, 1264, 2492, 0 }, { -3, -1264, -2498, 0 }, { -3, 1264, -2498, 0 } }, { 4104, 0, 0, 0 }, { 0, 0, ONE, 0 }, 2792, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 928, -1024, 1199, 0 }, { { 687, -1264, 1236, 0 }, { 687, 1264, 1236, 0 }, { -687, -1264, -1235, 0 }, { -687, 1264, -1235, 0 } }, { 3587, 0, -1998, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 3, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 991, -1056, 959, 0 }, { { -685, -1264, -1237, 0 }, { -685, 1264, -1237, 0 }, { 686, -1264, 1237, 0 }, { 686, 1264, 1237, 0 } }, { -3582, 0, 1984, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 7, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1217, -1120, -1473, 0 }, { { 37, -1264, -1420, 0 }, { 37, 1264, -1420, 0 }, { -59, -1264, 1406, 0 }, { -59, 1264, 1406, 0 } }, { -4094, 0, -141, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1506, -1088, -1665, 0 }, { { 16, -1264, 1410, 0 }, { 16, 1264, 1410, 0 }, { -28, -1264, -1417, 0 }, { -28, 1264, -1417, 0 } }, { 4102, 0, -66, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4929, -1056, -1248, 0 }, { { 275, -1264, 1373, 0 }, { 275, 1264, 1373, 0 }, { -311, -1264, -1393, 0 }, { -311, 1264, -1393, 0 } }, { 4015, 0, -852, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4577, -1056, -1312, 0 }, { { -243, -1264, -1408, 0 }, { -243, 1264, -1408, 0 }, { 213, -1264, 1383, 0 }, { 213, 1264, 1383, 0 } }, { -4042, 0, 659, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5187, -1056, 703, 0 }, { { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 }, { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 } }, { -1044, 0, -3962, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5282, -1024, 512, 0 }, { { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 }, { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 } }, { 1044, 0, 3971, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1761, -1088, 1472, 0 }, { { -17, -1264, -1414, 0 }, { -17, 1264, -1414, 0 }, { 17, -1264, 1414, 0 }, { 17, 1264, 1414, 0 } }, { -ONE, 0, 46, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2177, -1056, 1471, 0 }, { { 15, -1264, 1415, 0 }, { 15, 1264, 1415, 0 }, { -14, -1264, -1414, 0 }, { -14, 1264, -1414, 0 } }, { 4095, 0, -43, 0 }, { 0, 0, ONE, 0 }, 1894, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Zero-based CAP command selected by the loft's second action quad.
enum { DRYFIELD_MOTEL_LOFT_CAP_COMMAND_INDEX = 2 };

/// A facing-gated balcony departure and a CAP command quad, ending with LAST.
///
/// The warp uses loft endpoint 1 and balcony arrival endpoint 4. The second
/// quad requests CAP command 2 with presentation mode 3. Both use room-space
/// game units and writable runtime links/hit latches; unlink before unloading.
static WorldCollisionTrigger _gDryfieldMotelLoftActionTriggers[2] = {
    { NULL, NULL, NULL, { 4480, -48, -512, 0 }, { { -448, 0, -1024, 0 }, { 448, 0, -1024, 0 }, { -448, 0, 1024, 0 }, { 448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_MOTEL_BALCONY, (DRYFIELD_MOTEL_LOFT_WARP_BALCONY << 4) | DRYFIELD_MOTEL_LOFT_BALCONY_ARRIVAL_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1264, -64, -80, 0 }, { { -5040, 0, -2672, 0 }, { 5040, 0, -2672, 0 }, { -5040, 0, 2672, 0 }, { 5040, 0, 2672, 0 } }, { 0, 4106, 0, 0 }, { -ONE, 0, 0, 0 }, 5701, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_MOTEL_LOFT_CAP_COMMAND_INDEX, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Ten white point lights for the motel loft, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits. Each light is full strength through 2000 units and
/// fades to zero at 3000 units. The loaded room overlay owns the mutable array:
/// coordinate updates parent and compose its transforms, and lighting queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldMotelLoftPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5800, -1810, 1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4496, -1799, 2067 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1884, 1884, 1884 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5758, -1799, -1000 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1944, -1799, -2150 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -66, -1799, -2150 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 508, -1799, 2058 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1884, 1884, 1884 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4505, -1799, -2154 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3201, -1799, -10 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2805, -1799, -10 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2703, 2703, 2703 },
        },
        .inner = 2000,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2716, -1799, 2056 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1884, 1884, 1884 },
        },
        .inner = 2000,
        .outer = 3000,
    },
};

/// The loft's ten point lights, with no directional or cone-light arrays.
///
/// Borrows writable point-light transforms and query attenuation while loaded.
static WorldCoordRoomLights _gDryfieldMotelLoftRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldMotelLoftPointLights),
    .pointLights           = _gDryfieldMotelLoftPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

s32 D_dryfield_motel_loft_8017E62C[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

/// Shared policy for loft surface classes 0 and 2..7, without footstep cues.
///
/// Blocks probes, permits weapon impacts and applies pushback. The first byte
/// is retained as zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldMotelLoftDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Separate policy for the loft's class-1 floor, without footstep cues.
///
/// Blocks probes, permits weapon impacts and applies pushback. The first byte
/// is retained as zero; its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldMotelLoftFloorSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gDryfieldMotelLoftSurfacePropertiesByClass[8] = {
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
    &_gDryfieldMotelLoftFloorSurfaceProperties,
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
    &_gDryfieldMotelLoftDefaultSurfaceProperties,
};

static void _dryfieldMotelLoftInitRoomTask(Task* task);
static void _dryfieldMotelLoftIdleRoomTask(Task* unusedTask);

/// Refuses every key-item-use request in the motel loft.
///
/// Ignores the collected item ID and other arguments. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, leaving the room and inventory unchanged.
static s32 _dryfieldMotelLoftRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition from the motel loft with its destination unchanged.
///
/// Borrows a readable eight-byte request and writable reply for synchronous
/// dispatch; they may be the same record. Copies the complete request for both
/// query and execution calls, retains neither pointer, and returns 1.
static s32 _dryfieldMotelLoftAcceptRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_MOTEL_LOFT_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_MOTEL_LOFT_TRANSITION_ALLOWED;
}

/// Ignores room commands in the motel loft and returns zero.
///
/// The command word and other arguments are unused; no room state changes.
static s32 _dryfieldMotelLoftIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores directed room actions in the motel loft and returns zero.
///
/// The borrowed action request is neither read nor retained; all arguments
/// are unused and no room state changes.
static s32 _dryfieldMotelLoftIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the motel loft's room-message receiver and advances to idle state 1.
///
/// Runs in room-task state 0. Borrows this overlay's message table and registers
/// the live task in `GAME_TASK_SLOT_ROOM`; keep the overlay loaded while it can
/// receive messages. Registration does not retain the task or clear on teardown.
static void _dryfieldMotelLoftInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldMotelLoftRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the motel loft's room-message receiver idle in state 1.
///
/// The task and its message table stay active between synchronous messages;
/// the separate state-2 handler performs teardown. The argument is unused.
static void _dryfieldMotelLoftIdleRoomTask(Task* unusedTask)
{
}

/// Room receiver states 0 initialization, 1 idle and 2 task release.
///
/// Copied by value before unchecked dispatch through `Task::state` (0..2).
/// The callbacks must remain loaded while the room receiver task is live.
static const TaskFuncTable3 _gDryfieldMotelLoftRoomStateHandlers = {
    { _dryfieldMotelLoftInitRoomTask, _dryfieldMotelLoftIdleRoomTask, taskKill },
};

void dryfieldMotelLoftTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldMotelLoftRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
