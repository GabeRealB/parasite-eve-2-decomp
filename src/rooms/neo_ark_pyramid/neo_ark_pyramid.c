#include "rooms/neo_ark_pyramid.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_02000.h"
#include "actors/actor_02300.h"
#include "actors/actor_02600.h"
#include "actors/actor_03800.h"
#include "actors/actor_05700.h"
#include "actors/actor_401300.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
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

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"

static void _neoArkPyramidSetRotationPuzzleAngle(s32 sweepAngle);
static void _neoArkPyramidInitializeRoom(Task* task);
static void _neoArkPyramidDrawRotationPuzzleState(Task* unusedTask);

/// Room-controller states: 0 installs messages, 1 draws the puzzle, 2 releases the task.
///
/// Copied by value before dispatch; the live task's state must be in 0..2.
static const TaskFuncTable3 _gNeoArkPyramidRoomStateHandlers = {
    { _neoArkPyramidInitializeRoom, _neoArkPyramidDrawRotationPuzzleState, taskKill }
};

static void _neoArkPyramidRotationPuzzleTask(Task* task);
static s32  _neoArkPyramidRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _neoArkPyramidResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkPyramidIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg);
static s32  _neoArkPyramidHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid     _gNeoArkPyramidCollisionGrid;
static WorldCollisionOccluder _gNeoArkPyramidSightOccluders[3];
static WorldCollisionTrigger  _gNeoArkPyramidViewBoundaryTriggers[6];
static WorldCollisionTrigger  _gNeoArkPyramidActionTriggers[7];
static WorldCoordRoomLights   _gNeoArkPyramidRoomLights;

/// Synchronous room messages for transitions, key items, the puzzle and CAP commands.
///
/// Payloads are respectively two RoomEventMsg pointers, an item ID, a borrowed
/// DirectionActionRequest and a command ID. TASK_MESSAGE_TABLE_END closes the
/// read-only table; its storage and callbacks must outlive the room receiver.
static const TaskMessageEntry _gNeoArkPyramidRoomMessages[5] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkPyramidResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkPyramidRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkPyramidHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkPyramidIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless rotation-puzzle task recipe, started with zero spawn arguments.
///
/// The room action holds and hides the player before spawning it. Priority 32
/// matches the room controller; the callback must remain loaded until the
/// puzzle restores control and exits.
static TaskDesc _gNeoArkPyramidRotationPuzzleTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = _neoArkPyramidRotationPuzzleTask,
    .data     = { .value = 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gNeoArkPyramidRoomCollisionResources[2] = {
    { .grid = &_gNeoArkPyramidCollisionGrid, .viewBoundaryTriggers = _gNeoArkPyramidViewBoundaryTriggers, .actionTriggers = _gNeoArkPyramidActionTriggers, .occluders = _gNeoArkPyramidSightOccluders },
    { .grid = &_gNeoArkPyramidCollisionGrid, .viewBoundaryTriggers = _gNeoArkPyramidViewBoundaryTriggers, .actionTriggers = _gNeoArkPyramidActionTriggers, .occluders = _gNeoArkPyramidSightOccluders },
};

WorldCoordRoomLighting gNeoArkPyramidRoomLighting[2] = {
    { &_gNeoArkPyramidRoomLights, NULL },
    { &_gNeoArkPyramidRoomLights, NULL },
};

/// Eight logical-view mappings after the altar's second sequence is solved.
///
/// Indexed by logical view - 1; entries are 1-based camera/image/sprite indices.
/// Swaps 4 and 6 to select the alternate shot. Borrowed by room variant 2.
static u8 _gNeoArkPyramidAltarSolvedViewMap[8] = {
    1,
    2,
    3,
    6,
    5,
    4,
    7,
    8,
};

u8* gNeoArkPyramidViewMaps[2] = {
    gViewIdentityMap,
    _gNeoArkPyramidAltarSolvedViewMap,
};

ViewCount gNeoArkPyramidViewCounts[2] = { 8, 8 };

/// Arrival/departure requests in the pyramid sound bank.
enum {
    NEO_ARK_PYRAMID_WARP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 2),
    NEO_ARK_PYRAMID_WARP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_PYRAMID, 1),
};

DirectionWarpEntry gNeoArkPyramidWarpEntries[2] = {
    { .player = { { .word = 2048 }, -2944, 0, -2035 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 2048 }, -2944, 0, -2035 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = NEO_ARK_PYRAMID_WARP_ARRIVAL_SOUND, .departureSound = NEO_ARK_PYRAMID_WARP_DEPARTURE_SOUND, .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player = { { .word = 3072 }, 3200, -650, -7490 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 3072 }, 4200, -1160, -7500 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = DIRECTION_WARP_SOUND_NONE, .departureSound = DIRECTION_WARP_SOUND_NONE, .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 4, .flags = DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-local collision normals, with ONE representing unit length.
static SVECTOR _gNeoArkPyramidCollisionGridNormals[13] = {
#include "assets/neo_ark_pyramid_collision_02D04_normals.inc"
};

/// Vertices of the pyramid's collision mesh, in signed room-coordinate units.
static SVECTOR _gNeoArkPyramidCollisionGridVerts[66] = {
#include "assets/neo_ark_pyramid_collision_02D04_verts.inc"
};

/// Indexed collision triangles and quads, carrying room-local surface classes.
static WorldCollisionGridFace _gNeoArkPyramidCollisionGridFaces[36] = {
#include "assets/neo_ark_pyramid_collision_02D04_faces.inc"
};

/// Packed cell face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
///
/// Retains the complete 194-halfword asset span, including values between lists.
static s16 _gNeoArkPyramidCollisionGridCells[194] = {
#include "assets/neo_ark_pyramid_collision_02D04_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkPyramidCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_PYRAMID_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkPyramidCollisionGridCells[(halfwordOffset)])
/// X-major 5-by-3 directory of the pyramid's collision cell lists.
static s16* _gNeoArkPyramidCollisionGridTable[15] = {
#include "assets/neo_ark_pyramid_collision_02D04_table.inc"
};
#undef NEO_ARK_PYRAMID_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh shared by both pyramid room variants.
///
/// Covers five X cells by three Z cells, each 4000 game units wide, from
/// room-space origin (-4000, -11500) on X/Z. Activation binds the view transform;
/// the descriptor and mutable mesh pools remain borrowed while this room is live.
static WorldCollisionGrid _gNeoArkPyramidCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkPyramidCollisionGridNormals,
    .vertices    = _gNeoArkPyramidCollisionGridVerts,
    .faces       = _gNeoArkPyramidCollisionGridFaces,
    .cellFaceIds = _gNeoArkPyramidCollisionGridTable,
    .xBias       = 4000,
    .zBias       = 11500,
    .cellCountX  = 5,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkPyramidCollisionGridFaces),
};

ViewCamera gNeoArkPyramidViewCameras[8] = {
    { .transform = { .m = { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, .t = { -5270, 0x7530, 7060 } }, .screenDistance = 380 },
    { .transform = { .m = { { 3888, 0, 1288 }, { -275, 4001, 830 }, { -1258, -875, 3798 } }, .t = { 2039, 468, 5719 } }, .screenDistance = 230 },
    { .transform = { .m = { { 2546, 0, 3208 }, { 47, 4095, -37 }, { -3208, 60, 2545 } }, .t = { -1654, 855, 7910 } }, .screenDistance = 230 },
    { .transform = { .m = { { -1401, 0, -3848 }, { 212, 4089, -77 }, { 3842, -226, -1399 } }, .t = { 2273, 757, 5456 } }, .screenDistance = 230 },
    { .transform = { .m = { { -405, 0, -4075 }, { -324, 4083, 32 }, { 4062, 325, -404 } }, .t = { -570, 1119, 8915 } }, .screenDistance = 230 },
    { .transform = { .m = { { -1401, 0, -3848 }, { 212, 4089, -77 }, { 3842, -226, -1399 } }, .t = { 2273, 757, 5456 } }, .screenDistance = 230 },
    { .transform = { .m = { { -1554, 0, 3789 }, { 807, 4001, 331 }, { -3702, 872, -1518 } }, .t = { 765, 870, 6068 } }, .screenDistance = 257 },
    { .transform = { .m = { { -2878, 0, 2914 }, { 16, 4095, 16 }, { -2914, 23, -2878 } }, .t = { 1595, 667, 6136 } }, .screenDistance = 312 },
};

/// Shared texture encodings and neutral modulation of the mapped-view sprites.
enum {
    NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE  = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    NEO_ARK_PYRAMID_SPRITE_PALETTE       = getClut(0, 255),
    NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR = 128,
};

/// Boundaries between the two source ranges of mapped views 4 and 6.
enum {
    NEO_ARK_PYRAMID_VIEW4_FIRST_RANGE_COUNT = 34,
    NEO_ARK_PYRAMID_VIEW6_FIRST_RANGE_COUNT = 32,
};

/// Batch list for mapped view 1; its empty first batch retains background strips.
///
/// No source sprites; the second record ends the list.
static SpriteBatch _gNeoArkPyramidView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Batch list for mapped view 2; its empty first batch retains background strips.
///
/// No source sprites; the second record ends the list.
static SpriteBatch _gNeoArkPyramidView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 12 texture rectangles for mapped view 3, in screen pixels with OT depths.
///
/// Cached packets snapshot these sources; depth is read again when linking.
static SpriteSource _gNeoArkPyramidView3SpriteSources[12] = {
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 104 } }, -112, -48, 1050, { .fields = { 112, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 104 } }, -96, -48, 1075, { .fields = { 112, 104 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 88 } }, -80, -40, 1100, { .fields = { 96, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 16 } }, 56, 8, 1606, { .fields = { 96, 168 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 32 } }, 64, 0, 1611, { .fields = { 104, 176 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 40 } }, 72, 0, 1621, { .fields = { 104, 136 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 40 } }, 80, 0, 1588, { .fields = { 96, 88 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 48 } }, 88, -8, 1611, { .fields = { 104, 88 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 48, 48 } }, 96, -8, 1611, { .fields = { 80, 208 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 40 } }, 144, -8, 1611, { .fields = { 88, 128 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, 112, -16, 1611, { .fields = { 80, 184 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, 104, 40, 1611, { .fields = { 88, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Batch list for mapped view 3; its empty first batch retains background strips.
///
/// The following range draws the complete source array, then the sentinel ends it.
static SpriteBatch _gNeoArkPyramidView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPyramidView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 43 texture rectangles for mapped view 4, in screen pixels with OT depths.
///
/// Cached packets snapshot these sources; depth is read again when linking.
static SpriteSource _gNeoArkPyramidView4SpriteSources[43] = {
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, 40, 8, 1405, { .fields = { 96, 224 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, 40, 16, 1399, { .fields = { 112, 248 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, 48, 16, 1399, { .fields = { 96, 216 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, 40, 24, 1395, { .fields = { 96, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, 48, 24, 1395, { .fields = { 96, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 40, 192 } }, -160, -120, 1625, { .fields = { 88, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 184 } }, -120, -120, 1625, { .fields = { 72, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 96 } }, -104, -104, 1625, { .fields = { 64, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 72 } }, -96, -104, 1625, { .fields = { 80, 184 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, -96, -120, 1625, { .fields = { 88, 208 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 40 } }, -88, -88, 1625, { .fields = { 120, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 16 } }, -80, -80, 1625, { .fields = { 96, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 16 } }, -72, -80, 1625, { .fields = { 104, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 32 } }, -104, 24, 1625, { .fields = { 112, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, -96, 24, 1625, { .fields = { 104, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, 56, 32, 1395, { .fields = { 104, 216 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 64, 8 } }, -24, 24, 1437, { .fields = { 16, 200 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -32, 8, 1589, { .fields = { 8, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -32, 16, 1503, { .fields = { 8, 184 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 80, 8 } }, -40, 0, 1651, { .fields = { 0, 208 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -40, -8, 1745, { .fields = { 0, 136 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -40, -16, 1824, { .fields = { 8, 224 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -48, -24, 1963, { .fields = { 8, 216 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 64, 8 } }, -48, -32, 2033, { .fields = { 16, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 56, 8 } }, -48, -40, 2182, { .fields = { 24, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 64, 8 } }, -56, -48, 2324, { .fields = { 16, 248 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 56, 8 } }, -56, -56, 2624, { .fields = { 16, 96 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 48, 8 } }, -56, -64, 2681, { .fields = { 24, 120 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, 16, 32, 1426, { .fields = { 48, 112 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 40, 8 } }, -24, 32, 1426, { .fields = { 32, 104 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, 16, 40, 1393, { .fields = { 48, 128 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 40, 8 } }, -24, 40, 1425, { .fields = { 24, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, 40, 32, 1391, { .fields = { 112, 224 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, 48, 32, 1390, { .fields = { 120, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -56, -16, 1562, { .fields = { 48, 152 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, -48, -8, 1466, { .fields = { 88, 248 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -48, 0, 1389, { .fields = { 40, 24 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -48, 8, 1339, { .fields = { 48, 176 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -48, 16, 1298, { .fields = { 48, 168 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 24, 1254, { .fields = { 48, 160 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 32, 1263, { .fields = { 48, 144 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 40, 1263, { .fields = { 40, 16 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 48, 1273, { .fields = { 40, 8 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Batch list for mapped view 4; its empty first batch retains background strips.
///
/// Partitions all source elements at index 34; both ranges are visible and cached.
/// The first range retains serialized metadata bytes { 1, 0 } of unproven role.
static SpriteBatch _gNeoArkPyramidView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, NEO_ARK_PYRAMID_VIEW4_FIRST_RANGE_COUNT, 0, 0, { 1, 0 } },
    { NEO_ARK_PYRAMID_VIEW4_FIRST_RANGE_COUNT, ARRAY_SIZE(_gNeoArkPyramidView4SpriteSources) - NEO_ARK_PYRAMID_VIEW4_FIRST_RANGE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 24 texture rectangles for mapped view 5, in screen pixels with OT depths.
///
/// Cached packets snapshot these sources; depth is read again when linking.
static SpriteSource _gNeoArkPyramidView5SpriteSources[24] = {
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, -96, 24, 723, { .fields = { 112, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 96 } }, -160, -96, 1184, { .fields = { 80, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 112 } }, -144, -112, 1268, { .fields = { 96, 120 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 120 } }, -128, -120, 1336, { .fields = { 96, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 120 } }, -112, -120, 1845, { .fields = { 112, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 120 } }, -96, -120, 1098, { .fields = { 112, 120 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -160, 0, 772, { .fields = { 64, 120 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 32 } }, -160, 24, 708, { .fields = { 64, 32 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -160, 56, 639, { .fields = { 64, 144 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 40 } }, -160, 80, 606, { .fields = { 80, 96 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -144, 0, 774, { .fields = { 64, 96 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 32 } }, -144, 24, 713, { .fields = { 80, 136 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -144, 56, 642, { .fields = { 56, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 32 } }, -144, 80, 589, { .fields = { 80, 168 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -128, 0, 756, { .fields = { 64, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 32 } }, -128, 24, 719, { .fields = { 72, 200 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -128, 56, 645, { .fields = { 64, 168 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 32 } }, -128, 80, 593, { .fields = { 64, 64 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, -112, 0, 760, { .fields = { 104, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 32 } }, -112, 24, 711, { .fields = { 88, 200 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, -112, 56, 627, { .fields = { 80, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 16 } }, -112, 80, 585, { .fields = { 120, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 24 } }, -104, 0, 764, { .fields = { 88, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 32 } }, -104, 24, 715, { .fields = { 72, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Batch list for mapped view 5; its empty first batch retains background strips.
///
/// The following range draws the complete source array, then the sentinel ends it.
static SpriteBatch _gNeoArkPyramidView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPyramidView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// 41 texture rectangles for mapped view 6, in screen pixels with OT depths.
///
/// Cached packets snapshot these sources; depth is read again when linking.
static SpriteSource _gNeoArkPyramidView6SpriteSources[41] = {
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 8 } }, 40, 8, 1405, { .fields = { 112, 248 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 40, 192 } }, -160, -120, 1625, { .fields = { 88, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 184 } }, -120, -120, 1625, { .fields = { 72, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 96 } }, -104, -104, 1625, { .fields = { 64, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 72 } }, -96, -104, 1625, { .fields = { 80, 184 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, -96, -120, 1625, { .fields = { 88, 208 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 40 } }, -88, -88, 1625, { .fields = { 120, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 16 } }, -80, -80, 1625, { .fields = { 96, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 16 } }, -72, -80, 1625, { .fields = { 104, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 32 } }, -104, 24, 1625, { .fields = { 112, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, -96, 24, 1625, { .fields = { 104, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, 56, 32, 1395, { .fields = { 104, 216 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 64, 8 } }, -24, 24, 1437, { .fields = { 16, 200 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -32, 8, 1589, { .fields = { 8, 192 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -32, 16, 1503, { .fields = { 8, 184 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 80, 8 } }, -40, 0, 1651, { .fields = { 0, 208 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -40, -8, 1745, { .fields = { 0, 128 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -40, -16, 1824, { .fields = { 8, 224 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 72, 8 } }, -48, -24, 1963, { .fields = { 8, 216 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 64, 8 } }, -48, -32, 2033, { .fields = { 16, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 56, 8 } }, -48, -40, 2182, { .fields = { 24, 240 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 64, 8 } }, -56, -48, 2324, { .fields = { 16, 248 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 56, 8 } }, -56, -56, 2624, { .fields = { 16, 112 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 48, 8 } }, -56, -64, 2681, { .fields = { 24, 104 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, 16, 32, 1426, { .fields = { 48, 96 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, 16, 40, 1393, { .fields = { 48, 120 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 40, 8 } }, -24, 32, 1426, { .fields = { 32, 176 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 40, 8 } }, -24, 40, 1426, { .fields = { 32, 144 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, 40, 32, 1391, { .fields = { 112, 224 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 8, 24 } }, 48, 32, 1390, { .fields = { 120, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, 40, 16, 1399, { .fields = { 88, 216 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, 40, 24, 1395, { .fields = { 88, 232 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -56, -16, 1562, { .fields = { 48, 152 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 16, 8 } }, -48, -8, 1466, { .fields = { 88, 224 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -48, 0, 1389, { .fields = { 40, 0 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -48, 8, 1339, { .fields = { 48, 160 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -48, 16, 1298, { .fields = { 48, 168 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 24, 1254, { .fields = { 40, 24 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 32, 1263, { .fields = { 48, 136 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 40, 1263, { .fields = { 40, 8 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
    { NEO_ARK_PYRAMID_SPRITE_TEXTURE_PAGE, NEO_ARK_PYRAMID_SPRITE_PALETTE, { .fields = { 24, 8 } }, -40, 48, 1273, { .fields = { 40, 16 } }, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, NEO_ARK_PYRAMID_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Batch list for mapped view 6; its empty first batch retains background strips.
///
/// Partitions all source elements at index 32; both ranges are visible and cached.
/// The first range retains serialized metadata bytes { 1, 0 } of unproven role.
static SpriteBatch _gNeoArkPyramidView6SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, NEO_ARK_PYRAMID_VIEW6_FIRST_RANGE_COUNT, 0, 0, { 1, 0 } },
    { NEO_ARK_PYRAMID_VIEW6_FIRST_RANGE_COUNT, ARRAY_SIZE(_gNeoArkPyramidView6SpriteSources) - NEO_ARK_PYRAMID_VIEW6_FIRST_RANGE_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Batch list for mapped view 7; its empty first batch retains background strips.
///
/// No source sprites; the second record ends the list.
static SpriteBatch _gNeoArkPyramidView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Batch list for mapped view 8; its empty first batch retains background strips.
///
/// No source sprites; the second record ends the list.
static SpriteBatch _gNeoArkPyramidView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkPyramidSpriteViews[8] = {
    { { .empty = _gNeoArkPyramidView1SpriteBatches }, _gNeoArkPyramidView1SpriteBatches, NULL },
    { { .empty = _gNeoArkPyramidView2SpriteBatches }, _gNeoArkPyramidView2SpriteBatches, NULL },
    { { .elements = _gNeoArkPyramidView3SpriteSources }, _gNeoArkPyramidView3SpriteBatches, NULL },
    { { .elements = _gNeoArkPyramidView4SpriteSources }, _gNeoArkPyramidView4SpriteBatches, NULL },
    { { .elements = _gNeoArkPyramidView5SpriteSources }, _gNeoArkPyramidView5SpriteBatches, NULL },
    { { .elements = _gNeoArkPyramidView6SpriteSources }, _gNeoArkPyramidView6SpriteBatches, NULL },
    { { .empty = _gNeoArkPyramidView7SpriteBatches }, _gNeoArkPyramidView7SpriteBatches, NULL },
    { { .empty = _gNeoArkPyramidView8SpriteBatches }, _gNeoArkPyramidView8SpriteBatches, NULL },
};

/// Eleven white point lights shared by both room variants and enabled in every view.
///
/// RGB and identity rotations use ONE (12 fractional bits). Positions and inner/
/// outer falloff radii use game units; room-light setup parents them to the view,
/// and composition and lighting queries update the transforms and attenuation.
static WorldCoordPointLight _gNeoArkPyramidPointLights[11] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3490, -2000, -3010 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -4000, -6040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 2000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4120, -2000, -5600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2560, -2000, -9990 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3940, -2000, -0x28E6 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5480, -2000, -0x2B16 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1900, -2000, -7420 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2760, -2000, -5140 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1360, -2000, -6580 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4890, -2000, -4660 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -230, -2000, -7890 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 4000 },
};

/// Authored light collection shared by both pyramid room variants.
///
/// Holds only point lights: no directional or cone sources. Borrows the writable
/// light array for coordinate updates and position-dependent shading queries.
static WorldCoordRoomLights _gNeoArkPyramidRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkPyramidPointLights),
    .pointLights           = _gNeoArkPyramidPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Six directed view boundaries shared by both pyramid rooms.
///
/// Paired crossings join views 2/3, 3/4 and 4/5. Positions and radii use
/// room game units; normals use 4096 per unit. Setup binds the current view,
/// links and enables every record through LAST, including the final crossing.
/// Keep this mutable storage loaded until the view-boundary list is unlinked.
static WorldCollisionTrigger _gNeoArkPyramidViewBoundaryTriggers[6] = {
    { .origin = { -2792, -4000, -3605, 0 }, .vertices = { { 2775, -4752, 1502, 0 }, { -2776, -4752, -1503, 0 }, { 2775, 4752, 1502, 0 }, { -2776, 4752, -1503, 0 } }, .normal = { -1953, 0, 3606, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5701, .control = 0, .parameter0 = 2, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .origin = { -2914, -4033, -3426, 0 }, .vertices = { { -2775, -4752, -1502, 0 }, { 2776, -4752, 1503, 0 }, { -2775, 4752, -1502, 0 }, { 2776, 4752, 1503, 0 } }, .normal = { 1952, 0, -3608, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5701, .control = 0, .parameter0 = 3, .parameter1 = 2, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .origin = { -449, -4064, -7490, 0 }, .vertices = { { 1127, -4752, 3686, 0 }, { -1148, -4752, -3699, 0 }, { 1127, 4752, 3686, 0 }, { -1148, 4752, -3699, 0 } }, .normal = { -3931, 0, 1210, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 6122, .control = 0, .parameter0 = 3, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .origin = { -544, -3936, -7296, 0 }, .vertices = { { -1147, -4752, -3698, 0 }, { 1128, -4752, 3687, 0 }, { -1147, 4752, -3698, 0 }, { 1128, 4752, 3687, 0 } }, .normal = { 3930, 0, -1211, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 6122, .control = 0, .parameter0 = 4, .parameter1 = 3, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .origin = { 2766, -3968, -0x2BF1, 0 }, .vertices = { { 32, -4752, 2884, 0 }, { -31, -4752, -2884, 0 }, { 32, 4752, 2884, 0 }, { -31, 4752, -2884, 0 } }, .normal = { -4106, 0, 44, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5538, .control = 0, .parameter0 = 4, .parameter1 = 5, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, .hit = 0 },
    { .origin = { 2559, -4032, -0x2BC0, 0 }, .vertices = { { -31, -4752, -2884, 0 }, { 32, -4752, 2884, 0 }, { -31, 4752, -2884, 0 }, { 32, 4752, 2884, 0 } }, .normal = { 4105, 0, -46, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 5538, .control = 0, .parameter0 = 5, .parameter1 = 4, .flags = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Stair action settings: surface class 4, step counts and yaw in 16-angle-unit steps.
enum {
    NEO_ARK_PYRAMID_STAIR_DESCEND               = 0x100,
    NEO_ARK_PYRAMID_STAIR_ASCENT_SURFACE_STEPS  = (4 << 4) | 7,
    NEO_ARK_PYRAMID_STAIR_DESCENT_SURFACE_STEPS = (4 << 4) | 3,
    NEO_ARK_PYRAMID_STAIR_ASCENT_YAW            = 1024 / 16,
    NEO_ARK_PYRAMID_STAIR_DESCENT_YAW           = 3072 / 16,
    NEO_ARK_PYRAMID_TRIGGER_ROTATION_PUZZLE     = 1,
};

/// Seven action quads shared by both pyramid rooms.
///
/// Two warps, two stair directions, puzzle action 1 and CAP commands 4/5.
/// Positions and radii use room game units; normals use 4096 per unit.
/// Setup binds the current view, links and enables the records through LAST;
/// action contacts latch until consumed. Keep storage loaded until unlinked.
static WorldCollisionTrigger _gNeoArkPyramidActionTriggers[7] = {
    { .origin = { 3005, -3872, -7457, 0 }, .vertices = { { -13, -5344, 1023, 0 }, { 14, -5344, -1022, 0 }, { -13, 5344, 1023, 0 }, { 14, 5344, -1022, 0 } }, .normal = { -4109, 0, -55, 0 }, .facingNormal = { -4092, -201, -14, 0 }, .radius = 5418, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = GAME_AREA_NEO_ARK_ALTAR, .parameter1 = (2 << 4) | 1, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .origin = { 2272, -64, -7472, 0 }, .vertices = { { 416, 14, -702, 0 }, { 416, -13, 703, 0 }, { -416, 14, -702, 0 }, { -416, -13, 703, 0 } }, .normal = { 0, 4103, 71, 0 }, .facingNormal = { -4096, 0, 0, 0 }, .radius = 814, .control = WORLD_COLLISION_TRIGGER_ACTION_FACING, .parameter0 = NEO_ARK_PYRAMID_STAIR_ASCENT_SURFACE_STEPS, .parameter1 = NEO_ARK_PYRAMID_STAIR_ASCENT_YAW, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .origin = { 3104, -658, -7488, 0 }, .vertices = { { 416, -2, -974, 0 }, { 416, 3, 975, 0 }, { -416, -2, -974, 0 }, { -416, 3, 975, 0 } }, .normal = { 0, 4099, -21, 0 }, .facingNormal = { 4091, 0, -201, 0 }, .radius = 1055, .control = WORLD_COLLISION_TRIGGER_ACTION_FACING | NEO_ARK_PYRAMID_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = NEO_ARK_PYRAMID_STAIR_DESCENT_SURFACE_STEPS, .parameter1 = NEO_ARK_PYRAMID_STAIR_DESCENT_YAW, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .origin = { -2945, -48, -1888, 0 }, .vertices = { { 975, -2, 416, 0 }, { -974, 3, 416, 0 }, { 975, -2, -416, 0 }, { -974, 3, -416, 0 } }, .normal = { 10, 4099, 0, 0 }, .facingNormal = { -201, 0, -4091, 0 }, .radius = 1055, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_NEO_ARK_WOODLAND_PATH, .parameter1 = (1 << 4) | 2, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .origin = { -1632, -64, -5985, 0 }, .vertices = { { 963, -2, -443, 0 }, { -346, 3, 1002, 0 }, { 346, -2, -1001, 0 }, { -963, 3, 443, 0 } }, .normal = { 0, 4099, -11, 0 }, .facingNormal = { 3405, 0, 2275, 0 }, .radius = 1055, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, .parameter0 = NEO_ARK_PYRAMID_TRIGGER_ROTATION_PUZZLE, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .origin = { 6558, -64, -5760, 0 }, .vertices = { { 416, 14, -894, 0 }, { 416, -13, 895, 0 }, { -416, 14, -894, 0 }, { -416, -13, 895, 0 } }, .normal = { 0, 4106, 56, 0 }, .facingNormal = { -4096, 0, 0, 0 }, .radius = 985, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 4, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD, .hit = 0 },
    { .origin = { 6528, -64, -9792, 0 }, .vertices = { { 416, 14, -1278, 0 }, { 416, -13, 1279, 0 }, { -416, 14, -1278, 0 }, { -416, -13, 1279, 0 } }, .normal = { 0, 4119, 39, 0 }, .facingNormal = { -4096, 0, 0, 0 }, .radius = 1342, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 5, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, .hit = 0 },
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    NEO_ARK_PYRAMID_RESOURCE_BLACK_BEETLE = 38,
    NEO_ARK_PYRAMID_FILE_BLACK_BEETLE     = 38,
    NEO_ARK_PYRAMID_RESOURCE_MAGGOT       = 26,
    NEO_ARK_PYRAMID_FILE_MAGGOT           = 26,
};

/// Black Beetle resource recipes for placement variant 1.
///
/// Keys match `gMapNeoArkPyramidVariant1Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gNeoArkPyramidVariant1Resources[2] = {
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_BLACK_BEETLE, .fileNumber = NEO_ARK_PYRAMID_FILE_BLACK_BEETLE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Maggot resource recipes for placement variant 2.
///
/// Keys match `gMapNeoArkPyramidVariant2Placements`; AREA_PLACEMENT_END ends the list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gNeoArkPyramidVariant2Resources[2] = {
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_MAGGOT, .fileNumber = NEO_ARK_PYRAMID_FILE_MAGGOT, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor02600MaggotCaterpillarBodyTask },
    { .entryId = AREA_PLACEMENT_END },
};

/// Placement-entry keys, decimal file numbers and table selectors for these layouts.
enum {
    NEO_ARK_PYRAMID_RESOURCE_HORNED_STRANGER       = 13,
    NEO_ARK_PYRAMID_RESOURCE_PAWN_GOLEM            = 20,
    NEO_ARK_PYRAMID_RESOURCE_BEAM_SWORD_ROOK       = 23,
    NEO_ARK_PYRAMID_RESOURCE_GRENADE_LAUNCHER_ROOK = 57,
    NEO_ARK_PYRAMID_FILE_HORNED_STRANGER           = 13,
    NEO_ARK_PYRAMID_FILE_PAWN_GOLEM                = 20,
    NEO_ARK_PYRAMID_FILE_BEAM_SWORD_ROOK           = 23,
    NEO_ARK_PYRAMID_FILE_GRENADE_LAUNCHER_ROOK     = 57,
};

/// Black Beetle and Maggot recipes for placement variant 3.
///
/// Keys 38 and 26 load files 103800 and 202600 and spawn descriptor zero.
/// Paired with `gMapNeoArkPyramidVariant3Placements`; AREA_PLACEMENT_END ends the list.
/// Retain room recipes, placements and actor descriptors through loading,
/// spawning and model-flag updates.
static AreaResource _gNeoArkPyramidVariant3Resources[3] = {
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_BLACK_BEETLE, .fileNumber = NEO_ARK_PYRAMID_FILE_BLACK_BEETLE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_MAGGOT, .fileNumber = NEO_ARK_PYRAMID_FILE_MAGGOT, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor202600MaggotCaterpillarBodyTask },
    { .entryId = AREA_PLACEMENT_END },
};

/// Horned Stranger and Pawn GOLEM recipes for placement variant 4.
///
/// Keys 13 and 20 load files 401300 and 302000 and spawn descriptor zero.
/// Paired with `gMapNeoArkPyramidVariant4Placements`; AREA_PLACEMENT_END ends the list.
/// Retain room recipes, placements and actor descriptors through loading,
/// spawning and model-flag updates.
static AreaResource _gNeoArkPyramidVariant4Resources[3] = {
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_HORNED_STRANGER, .fileNumber = NEO_ARK_PYRAMID_FILE_HORNED_STRANGER, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor401300HornedStrangerTaskDesc },
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_PAWN_GOLEM, .fileNumber = NEO_ARK_PYRAMID_FILE_PAWN_GOLEM, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_30, .taskIndex = 0, .taskTable = gActor302000PawnGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Beam-sword and grenade-launcher Rook GOLEM recipes for variant 11.
///
/// Keys 23 and 57 load files 102300 and 205700 and spawn descriptor zero.
/// Paired with `gMapNeoArkPyramidVariant11Placements`; AREA_PLACEMENT_END ends the list.
/// Retain room recipes, placements and actor descriptors through loading,
/// spawning and model-flag updates.
static AreaResource _gNeoArkPyramidVariant11Resources[3] = {
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_BEAM_SWORD_ROOK, .fileNumber = NEO_ARK_PYRAMID_FILE_BEAM_SWORD_ROOK, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102300RookGolemTasks },
    { .entryId = NEO_ARK_PYRAMID_RESOURCE_GRENADE_LAUNCHER_ROOK, .fileNumber = NEO_ARK_PYRAMID_FILE_GRENADE_LAUNCHER_ROOK, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor05700RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gNeoArkPyramidPlacementVariants[13] = {
    [1]  = { .placements = gMapNeoArkPyramidVariant1Placements, .resources = _gNeoArkPyramidVariant1Resources },
    [2]  = { .placements = gMapNeoArkPyramidVariant2Placements, .resources = _gNeoArkPyramidVariant2Resources },
    [3]  = { .placements = gMapNeoArkPyramidVariant3Placements, .resources = _gNeoArkPyramidVariant3Resources },
    [4]  = { .placements = gMapNeoArkPyramidVariant4Placements, .resources = _gNeoArkPyramidVariant4Resources },
    [11] = { .placements = gMapNeoArkPyramidVariant11Placements, .resources = _gNeoArkPyramidVariant11Resources },
};

/// Three room-space line-of-sight quads shared by both pyramid rooms.
///
/// Positions use game units and normals use 4096 per unit. Setup links and
/// enables every record through LAST; the authored low flag value 1 has no
/// established interpretation. Retain this mutable storage until unlinked.
static WorldCollisionOccluder _gNeoArkPyramidSightOccluders[3] = {
    { .origin = { 6416, -1104, -6464, 0 }, .vertices = { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, .normal = { 0, 0, 4097, 0 }, .radius = 4664, .flags = 1, .unknown_3B = 0 },
    { .origin = { 6400, -1088, -8384, 0 }, .vertices = { { -2800, 1776, 0, 0 }, { 2800, 1776, 0, 0 }, { -2800, 208, 0, 0 }, { 2800, -3760, 0, 0 } }, .normal = { 0, 0, 4097, 0 }, .radius = 4664, .flags = 1, .unknown_3B = 0 },
    { .origin = { 1536, -1984, -5744, 0 }, .vertices = { { -5776, 0, 4448, 0 }, { 5776, 0, 4448, 0 }, { -5776, 0, -2464, 0 }, { 5776, 0, -6432, 0 } }, .normal = { 0, -4110, 0, 0 }, .radius = 8628, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST, .unknown_3B = 0 },
};

/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    NEO_ARK_PYRAMID_SURFACE3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
    NEO_ARK_PYRAMID_SURFACE3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3B,
    NEO_ARK_PYRAMID_STAIR_WALK_SOUND    = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3D,
    NEO_ARK_PYRAMID_STAIR_RUN_SOUND     = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3F,
};

/// Paired walking/running requests for surface class 3; jumps reuse walking.
///
/// The player selects walk or run on animation cue 2 and adds one for cue 1.
/// Type-1 requests use the currently loaded bank; the surface record borrows
/// this storage while the pyramid overlay remains loaded.
static WorldCollisionFootstepSounds _gNeoArkPyramidSurface3FootstepSounds = {
    .walk         = NEO_ARK_PYRAMID_SURFACE3_WALK_SOUND,
    .run          = NEO_ARK_PYRAMID_SURFACE3_RUN_SOUND,
    .scriptedJump = NEO_ARK_PYRAMID_SURFACE3_WALK_SOUND,
};

/// Paired type-1 requests for surface class 4, assigned by both stair triggers.
///
/// Scripted stair climbing selects the walking pair; the stored running pair
/// applies if a running cue occurs while the actor retains this surface class.
/// The surface record borrows this storage while the pyramid overlay is loaded.
static WorldCollisionFootstepSounds _gNeoArkPyramidStairFootstepSounds = {
    .walk         = NEO_ARK_PYRAMID_STAIR_WALK_SOUND,
    .run          = NEO_ARK_PYRAMID_STAIR_RUN_SOUND,
    .scriptedJump = NEO_ARK_PYRAMID_STAIR_WALK_SOUND,
};

/// Silent blocking policy shared by surface classes 0, 5, 6 and 7.
///
/// Applies pushback and is borrowed through the room surface-class directory.
static WorldCollisionSurfaceProperties _gNeoArkPyramidDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Silent blocking policy of surface class 1, stored independently of class 0.
///
/// Applies pushback and is borrowed through the room surface-class directory.
static WorldCollisionSurfaceProperties _gNeoArkPyramidSurface1Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Surface-class-2 policy: probes pass and weapon impacts are ignored.
///
/// Applies pushback and is borrowed through the room surface-class directory.
static WorldCollisionSurfaceProperties _gNeoArkPyramidSurface2Properties = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Blocking surface-class-3 policy with walking and running footstep cues.
///
/// Applies pushback and is borrowed through the room surface-class directory.
static WorldCollisionSurfaceProperties _gNeoArkPyramidSurface3Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gNeoArkPyramidSurface3FootstepSounds,
};

/// Blocking surface-class-4 policy with the stair footstep cues.
///
/// Applies pushback and is borrowed through the room surface-class directory.
static WorldCollisionSurfaceProperties _gNeoArkPyramidSurface4Properties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gNeoArkPyramidStairFootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkPyramidSurfaceProperties[8] = {
    &_gNeoArkPyramidDefaultSurfaceProperties,
    &_gNeoArkPyramidSurface1Properties,
    &_gNeoArkPyramidSurface2Properties,
    &_gNeoArkPyramidSurface3Properties,
    &_gNeoArkPyramidSurface4Properties,
    &_gNeoArkPyramidDefaultSurfaceProperties,
    &_gNeoArkPyramidDefaultSurfaceProperties,
    &_gNeoArkPyramidDefaultSurfaceProperties,
};

/// Signed screen-space rotation of the puzzle quad, in 4096 units per turn.
///
/// Unnormalized: completed twelfth-turn steps are relative to step four, with
/// the active sweep added. Retained between setter calls while the room is loaded.
static s32 _gNeoArkPyramidRotationPuzzleAngle = 0;

static void _neoArkPyramidDrawRotationPuzzleQuad(s32 angle);

/// Returns from the rotation puzzle to room view 3 and releases its task.
///
/// Requires a live player, session/save and held puzzle event after CAP ends.
/// Shows the HUD and resumes enemy updates before restoring player control and
/// automatic model buffering. The bodyless puzzle task is killed last; its
/// pointer must not be used afterwards.
static inline void _neoArkPyramidRestoreRoomControl(Task* task)
{
    enum { NEO_ARK_PYRAMID_ROTATION_ROOM_VIEW = 3,
           NEO_ARK_PYRAMID_EVENT_IDLE         = 0 };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = NEO_ARK_PYRAMID_ROTATION_ROOM_VIEW;
    gGameSession->hideHud                                  = false;
    gGameSession->eventState                               = NEO_ARK_PYRAMID_EVENT_IDLE;
    gSceneCombatState.actorControl                         = SCENE_COMBAT_ACTORS_RUNNING;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    taskKill(task);
}

/// Runs the rotation puzzle's prompt, angle sweep and return to room control.
///
/// Starts with a bodyless state-0 task after the player has been held and hidden.
/// View 8 shows the puzzle. Each accepted prompt sweeps one twelfth-turn in
/// four-unit steps (4096 units per turn), using `killCountdown` as a halfword
/// angle accumulator. Four completed steps play the completion CAP; variant
/// key 12 cancels. Both exits select view 3 and restore actors, HUD and player.
/// Requires the room, CAP and drawing resources to remain loaded until teardown.
static void _neoArkPyramidRotationPuzzleTask(Task* task)
{
    enum { NEO_ARK_PYRAMID_ROTATION_PREPARE          = 0,
           NEO_ARK_PYRAMID_ROTATION_PROMPT           = 1,
           NEO_ARK_PYRAMID_ROTATION_WAIT_PROMPT      = 2,
           NEO_ARK_PYRAMID_ROTATION_BEGIN_SWEEP      = 3,
           NEO_ARK_PYRAMID_ROTATION_SWEEP            = 4,
           NEO_ARK_PYRAMID_ROTATION_WAIT_COMPLETION  = 5,
           NEO_ARK_PYRAMID_ROTATION_EXIT             = 10,
           NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW      = 8,
           NEO_ARK_PYRAMID_ROTATION_PROMPT_COMMAND   = 1,
           NEO_ARK_PYRAMID_ROTATION_COMPLETE_COMMAND = 2,
           NEO_ARK_PYRAMID_ROTATION_CANCEL_KEY       = 12,
           NEO_ARK_PYRAMID_ROTATION_ANGLE_STEP       = 4,
           NEO_ARK_PYRAMID_ROTATION_SWEEP_LIMIT      = 342,
           NEO_ARK_PYRAMID_ROTATION_REQUIRED_STEPS   = 4,
           NEO_ARK_PYRAMID_ROTATION_KEEP_ACTORS_HELD = 1 };
    u16 sweepAngle;

    switch (task->state) {
        case NEO_ARK_PYRAMID_ROTATION_PREPARE:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW;
            gGameSession->hideHud                                  = 1;
            gGameSession->eventState                               = 1;
            gSceneCombatState.actorControl                         = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state++;
            break;
        case NEO_ARK_PYRAMID_ROTATION_PROMPT:
            capRunCommand(NEO_ARK_PYRAMID_ROTATION_PROMPT_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case NEO_ARK_PYRAMID_ROTATION_WAIT_PROMPT:
            gCapPreserveActorControl = NEO_ARK_PYRAMID_ROTATION_KEEP_ACTORS_HELD;
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case NEO_ARK_PYRAMID_ROTATION_BEGIN_SWEEP:
            if (capGetVariantKey() == NEO_ARK_PYRAMID_ROTATION_CANCEL_KEY) {
                task->state = NEO_ARK_PYRAMID_ROTATION_EXIT;
                break;
            }
            sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE, 0, 0);
            task->killCountdown = 0;
            task->state++;
            break;
        case NEO_ARK_PYRAMID_ROTATION_SWEEP:
            // Commit the completed turn before settling the displayed angle.
            sweepAngle          = task->killCountdown + NEO_ARK_PYRAMID_ROTATION_ANGLE_STEP;
            task->killCountdown = sweepAngle;
            if ((s16)sweepAngle >= NEO_ARK_PYRAMID_ROTATION_SWEEP_LIMIT) {
                gameFlagSetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT, gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) + 1);
                _neoArkPyramidSetRotationPuzzleAngle(0);
                if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) >= NEO_ARK_PYRAMID_ROTATION_REQUIRED_STEPS) {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE_DONE, 0, 0);
                    capRunCommand(NEO_ARK_PYRAMID_ROTATION_COMPLETE_COMMAND, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                } else {
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_PYRAMID_ROTATE_STOP, 0, 0);
                    task->state = NEO_ARK_PYRAMID_ROTATION_PROMPT;
                }
            } else {
                _neoArkPyramidSetRotationPuzzleAngle((s16)sweepAngle);
            }
            break;
        case NEO_ARK_PYRAMID_ROTATION_WAIT_COMPLETION:
            gCapPreserveActorControl = NEO_ARK_PYRAMID_ROTATION_KEEP_ACTORS_HELD;
            if (capIsBusy() == 0) {
                task->state = NEO_ARK_PYRAMID_ROTATION_EXIT;
            }
            break;
        case NEO_ARK_PYRAMID_ROTATION_EXIT:
            _neoArkPyramidRestoreRoomControl(task);
            break;
    }
}

/// Draws the rotation puzzle's 174-pixel square about the screen origin.
///
/// `angle` uses 4096 units per turn; sine and cosine have twelve fractional bits.
/// Requires the puzzle's 8-bit texture at VRAM (896, 0), palette at (0, 255),
/// ordering-table entry 12 and room for one `POLY_FT4` in the frame arena.
/// Raw texture colour is used without shading or semitransparency.
static void _neoArkPyramidDrawRotationPuzzleQuad(s32 angle)
{
    enum { NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT        = 87,
           NEO_ARK_PYRAMID_PUZZLE_TRIG_FRACTION_BITS = 12,
           NEO_ARK_PYRAMID_PUZZLE_UV_MIN             = 1,
           NEO_ARK_PYRAMID_PUZZLE_UV_MAX             = NEO_ARK_PYRAMID_PUZZLE_UV_MIN + 2 * NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT,
           NEO_ARK_PYRAMID_PUZZLE_TEXTURE_8_BIT      = 1,
           NEO_ARK_PYRAMID_PUZZLE_OT_INDEX           = 12 };

    POLY_FT4* quad;
    DVECTOR   sourceCorners[4];
    DVECTOR   rotatedCorners[4];
    s32       cornerIndex;

    // Rotate screen-space corners, narrowing the Q12 result to signed pixels.
    sourceCorners[0].vx = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[0].vy = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[1].vx = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[1].vy = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[2].vx = -NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[2].vy = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[3].vx = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    sourceCorners[3].vy = NEO_ARK_PYRAMID_PUZZLE_HALF_EXTENT;
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(sourceCorners); cornerIndex++) {
        rotatedCorners[cornerIndex].vx = (sourceCorners[cornerIndex].vx * rcos(angle) - sourceCorners[cornerIndex].vy * rsin(angle)) >> NEO_ARK_PYRAMID_PUZZLE_TRIG_FRACTION_BITS;
        rotatedCorners[cornerIndex].vy = (sourceCorners[cornerIndex].vx * rsin(angle) + sourceCorners[cornerIndex].vy * rcos(angle)) >> NEO_ARK_PYRAMID_PUZZLE_TRIG_FRACTION_BITS;
    }
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    setShadeTex(quad, true);
    quad->x0    = rotatedCorners[0].vx;
    quad->y0    = rotatedCorners[0].vy;
    quad->x1    = rotatedCorners[1].vx;
    quad->y1    = rotatedCorners[1].vy;
    quad->x2    = rotatedCorners[2].vx;
    quad->y2    = rotatedCorners[2].vy;
    quad->x3    = rotatedCorners[3].vx;
    quad->y3    = rotatedCorners[3].vy;
    quad->u0    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->v0    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->u1    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->v1    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->u2    = NEO_ARK_PYRAMID_PUZZLE_UV_MIN;
    quad->v2    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->u3    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->v3    = NEO_ARK_PYRAMID_PUZZLE_UV_MAX;
    quad->clut  = getClut(0, 255);
    quad->tpage = getTPage(NEO_ARK_PYRAMID_PUZZLE_TEXTURE_8_BIT, GPU_BLEND_AVERAGE, 896, 0);
    addPrim(gGpuCurrentOt + NEO_ARK_PYRAMID_PUZZLE_OT_INDEX, quad);
}

/// Refuses every key-item use in the pyramid, leaving the item and room unchanged.
///
/// Handles message 0x13F1 with an integer `itemId`; both payload words are ignored.
/// Returns zero so the item menu displays its unavailable-use response.
static s32 _neoArkPyramidRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Resolves a Neo Ark destination room for a pyramid transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
static s32 _neoArkPyramidResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_PYRAMID_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_PYRAMID_TRANSITION_ALLOWED;
}

/// Ignores CAP room commands and returns zero without changing the pyramid.
///
/// Handles `ROOM_MESSAGE_COMMAND`; the integer command and second word are ignored.
static s32 _neoArkPyramidIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    return 0;
}

/// Opens the rotation puzzle or its completed-puzzle CAP event.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request
/// and ignored second word. Action 1 settles the displayed angle, then starts
/// CAP event 3 at exactly four completed turns, or holds and hides the player
/// before spawning the puzzle task. Other actions do nothing; returns zero.
static s32 _neoArkPyramidHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { NEO_ARK_PYRAMID_ACTION_ROTATION_PUZZLE  = 1,
           NEO_ARK_PYRAMID_ROTATION_COMPLETE_TURNS = 4,
           NEO_ARK_PYRAMID_COMPLETED_PUZZLE_EVENT  = 3 };

    if (request->actionId == NEO_ARK_PYRAMID_ACTION_ROTATION_PUZZLE) {
        _neoArkPyramidSetRotationPuzzleAngle(0);
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) == NEO_ARK_PYRAMID_ROTATION_COMPLETE_TURNS) {
            capSpawnEventIfIdle(NEO_ARK_PYRAMID_COMPLETED_PUZZLE_EVENT, CAP_EVENT_PAUSE_ACTORS);
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            taskSpawnFromTable(&_gNeoArkPyramidRotationPuzzleTaskDesc, 0, 0, 0);
        }
    }
    return 0;
}

/// Sets the rotation puzzle's screen angle from completed steps and the current sweep.
///
/// Angles use 4096 units per turn. Each completed step advances one twelfth of
/// a turn, with step four aligned at zero. `sweepAngle` adds a signed angle
/// within the current step; pass zero to settle on the completed-step angle.
/// The stored angle is signed and is not normalized to a single turn.
static void _neoArkPyramidSetRotationPuzzleAngle(s32 sweepAngle)
{
    enum { NEO_ARK_PYRAMID_ROTATION_ALIGNED_STEP     = 4,
           NEO_ARK_PYRAMID_ROTATION_ANGLE_TURN_SHIFT = 12,
           NEO_ARK_PYRAMID_ROTATION_STEPS_PER_TURN   = 12 };

    // Scale the signed completed-step offset before dividing; add the sweep last.
    _gNeoArkPyramidRotationPuzzleAngle = (((gameFlagGetNibble(GAME_FLAG_NEO_ARK_PYRAMID_TURN_COUNT) - NEO_ARK_PYRAMID_ROTATION_ALIGNED_STEP) << NEO_ARK_PYRAMID_ROTATION_ANGLE_TURN_SHIFT) / NEO_ARK_PYRAMID_ROTATION_STEPS_PER_TURN) + sweepAngle;
}

/// Installs the pyramid message receiver and advances to puzzle drawing.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _neoArkPyramidInitializeRoom(Task* task)
{
    task->msgTable = _gNeoArkPyramidRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Draws the rotation puzzle during the room task's per-frame state.
///
/// Only active view 8 displays the quad, using the angle last established by
/// `_neoArkPyramidSetRotationPuzzleAngle`. Requires the frame's primitive arena,
/// ordering table and puzzle texture to be ready. The task argument is unused.
static void _neoArkPyramidDrawRotationPuzzleState(Task* unusedTask)
{
    enum { NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW = 8 };

    if (gGameSession->location.view == NEO_ARK_PYRAMID_ROTATION_PUZZLE_VIEW) {
        _neoArkPyramidDrawRotationPuzzleQuad(_gNeoArkPyramidRotationPuzzleAngle);
    }
}

void neoArkPyramidRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkPyramidRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void neoArkPyramidConfigureEffectsTask(Task* task)
{
    enum { NEO_ARK_PYRAMID_EFFECTS_INITIALIZE,
           NEO_ARK_PYRAMID_EFFECTS_CONFIGURED };

    if (task->state == NEO_ARK_PYRAMID_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId               = EFFECT_NEO_ARK_PYRAMID_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_NEO_ARK_PYRAMID_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_NEO_ARK_PYRAMID_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        task->state                      = NEO_ARK_PYRAMID_EFFECTS_CONFIGURED;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkPyramidRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkPyramidRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkPyramidRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
