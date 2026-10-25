#include "rooms/shelter_1f_heliport.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_161500.h"
#include "actors/actor_260400.h"
#include "actors/actor_260500.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
#include "../../shared/room/room_events.h"
#include "../../shared/room/follow_collision.h"
#include "../../shared/actor/actor_messages.h"

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

static s32 _gShopPreviewRowId;

static WorldCollisionGrid _gShelter1fHeliportCollisionGrid;

static s32                          _gShopRefillDisplayQuantityFixed;
static const EquipmentWeaponSupply* _gShopRefillSupply;

/// The event the message handler latched for the room's event task: the spawn
/// argument of its helper task 0x31, the message, the flag saying one was
/// latched, and the event's parameters.
extern RoomFadeStorage  gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void _shelter1fHeliportInitializeRoomTask(Task* task);
static void _shelter1fHeliportUpdateRoom(Task* unusedTask);
static void _shelter1fHeliportUpdatePlacedActorVisibility(void);

#include "../../shared/room/shop.h"
#include "../../shared/room/cap_dialogue.h"

static WorldCollisionTrigger            _gShelter1fHeliportViewBoundaryTriggers[12];
static WorldCollisionTrigger            _gShelter1fHeliportActionTriggers[21];
static const WorldCoordRoomAmbientEntry _gShelter1fHeliportAmbientByView[SHELTER_1F_HELIPORT_VIEW_COUNT + 1] __attribute__((section(".data")));
static WorldCoordRoomLights             _gShelter1fHeliportRoomLights;
static s32                              _shelter1fHeliportResolveDepartureMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                              _shelter1fHeliportUseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32                              _shelter1fHeliportHandleCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32                              _shelter1fHeliportHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Room interaction IDs shared by the trigger resources and their message handlers.
enum {
    SHELTER_1F_HELIPORT_ACTION_TALK               = 1,
    SHELTER_1F_HELIPORT_ACTION_SOLDIER_TALK_B     = 2,
    SHELTER_1F_HELIPORT_ACTION_SOLDIER_TALK_A     = 3,
    SHELTER_1F_HELIPORT_ACTION_SHOP               = 4,
    SHELTER_1F_HELIPORT_ACTION_COMPANION_REMINDER = 5,
    SHELTER_1F_HELIPORT_COMMAND_LOOP_DIALOGUE     = 0x21,
    SHELTER_1F_HELIPORT_COMMAND_CHECK_TALK        = 0x22,
};

/// Direct CAP command-table indices used by interaction quads.
enum {
    SHELTER_1F_HELIPORT_TRIGGER_CAP_24 = 0x24,
    SHELTER_1F_HELIPORT_TRIGGER_CAP_26 = 0x26,
    SHELTER_1F_HELIPORT_TRIGGER_CAP_27 = 0x27,
    SHELTER_1F_HELIPORT_TRIGGER_CAP_2A = 0x2A,
};

/// Warp slots: departure descriptor in the high nibble, destination arrival in the low.
enum {
    SHELTER_1F_HELIPORT_TENT_WARP_SLOTS    = (2 << 4) | 1,
    SHELTER_1F_HELIPORT_BULWARK_WARP_SLOTS = (1 << 4) | 2,
};

/// Shared GPU texture encoding and neutral RGB modulation for the room sprites.
enum {
    SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE  = getTPage(1, 0, 960, 0),
    SHELTER_1F_HELIPORT_SPRITE_CLUT          = getClut(0, 255),
    SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR = 128,
};

#include "../../shared/room/shop_data.inc.c"

TaskDesc gShelter1fHeliportShopSessionTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _shopSessionTask,
    .data     = { .value = 0 },
};

/// Bodyless priority-32 task recipe for the latched Bulwark departure scene.
///
/// Spawning retains no descriptor pointer. The callback borrows the room's
/// singleton event/message records through CAP, fade, sound and session reload;
/// keep this overlay loaded and those records unchanged until completion.
static TaskDesc _gShelter1fHeliportDepartureEventTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = roomEventStagedTask,
    .data     = { .value = 0 },
};

/// Terminated message dispatch for departures, key items, actions and CAP commands.
///
/// The room controller borrows this table for its lifetime. Payloads belong to
/// each message's contract; unsupported IDs return zero at the final sentinel.
static TaskMessageEntry _gShelter1fHeliportRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fHeliportResolveDepartureMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fHeliportUseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fHeliportHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fHeliportHandleCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-32 task recipe for repeating the heliport CAP dialogue.
///
/// The caller holds player control and supplies command 0x21 in spawnArg1.
/// CAP choice 15 restores control and ends the task. Spawning reads the recipe
/// synchronously; this overlay and its CAP resources must outlive playback.
static TaskDesc _gShelter1fHeliportDialogueLoopTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = _capDialogueLoopTask,
    .data     = { .value = 0 },
};

enum { SHELTER_1F_HELIPORT_PLACED_ACTOR_COUNT  = 4,
       SHELTER_1F_HELIPORT_DRAW_MODES_KEEP_ROW = 0 };

/// Model draw modes for placement indices 0..3, indexed by saved view 0..11.
///
/// Only variants below 3 apply overrides. A zero first byte keeps the whole
/// row unchanged; other rows show actors or hide them with automatic buffering
/// disabled. Stored bytes are actor-message modes, not boolean visibility.
static const u8 _gShelter1fHeliportPlacedActorDrawModesByView[SHELTER_1F_HELIPORT_VIEW_COUNT][SHELTER_1F_HELIPORT_PLACED_ACTOR_COUNT] SECTION(".data") = {
    { ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER },
    { ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW },
    { ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW },
    { ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER },
    { ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER },
    { ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW },
    { SHELTER_1F_HELIPORT_DRAW_MODES_KEEP_ROW, 0, 0, 0 },
    { SHELTER_1F_HELIPORT_DRAW_MODES_KEEP_ROW, 0, 0, 0 },
    { SHELTER_1F_HELIPORT_DRAW_MODES_KEEP_ROW, 0, 0, 0 },
    { SHELTER_1F_HELIPORT_DRAW_MODES_KEEP_ROW, 0, 0, 0 },
    { SHELTER_1F_HELIPORT_DRAW_MODES_KEEP_ROW, 0, 0, 0 },
    { ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, ACTOR_MESSAGE_DRAW_SHOW, ACTOR_MESSAGE_DRAW_SHOW },
};

/// Extra companion-obstacle translation in room axes and signed game units.
///
/// X/Z stay zero. Y is zero while the companion blocks movement, or +10000
/// to move the obstacle out of the walkable room. The rebuild borrows XYZ for
/// the call; this persistent vector stays in the room overlay.
static SVECTOR _gShelter1fHeliportCompanionObstacleOffset = { 0 };

/// Four model-local obstacle normals, with 4096 representing unit length.
static SVECTOR _gShelter1fHeliportCompanionObstacleTemplateNormals[4] = {
#include "assets/shelter_1f_heliport_collision_03CEC_normals.inc"
};

/// Eight model-local obstacle corners in signed game-coordinate units.
static SVECTOR _gShelter1fHeliportCompanionObstacleTemplateVerts[8] = {
#include "assets/shelter_1f_heliport_collision_03CEC_verts.inc"
};

/// Four side quads using vertices 0..7, normals 0..3 and room surface class 0.
static WorldCollisionGridFace _gShelter1fHeliportCompanionObstacleTemplateFaces[4] = {
#include "assets/shelter_1f_heliport_collision_03CEC_faces.inc"
};

/// Single-cell face list (0..3, END) and the final retained zero halfword.
static s16 _gShelter1fHeliportCompanionObstacleTemplateCells[6] = {
#include "assets/shelter_1f_heliport_collision_03CEC_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fHeliportCompanionObstacleTemplateCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..5,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_HELIPORT_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fHeliportCompanionObstacleTemplateCells[(halfwordOffset)])
/// One-cell directory retained with the obstacle template; the rebuild uses only its mesh.
static s16* _gShelter1fHeliportCompanionObstacleTemplateTable[1] = {
#include "assets/shelter_1f_heliport_collision_03CEC_table.inc"
};
#undef SHELTER_1F_HELIPORT_CELL_FACE_LIST_AT_OFFSET

/// Model-local companion obstacle template restored into the room grid's leading slots.
///
/// Four normals, eight vertices and four quads remain unchanged by rebuilding.
/// The single 4000-unit cell has origin (-131, -227); its directory and lists
/// are retained asset data, unused by the rebuild. This template is never the
/// active room grid. The room overlay owns the record and all five pools.
static WorldCollisionGrid _gShelter1fHeliportCompanionObstacleTemplate = { NULL, _gShelter1fHeliportCompanionObstacleTemplateNormals, _gShelter1fHeliportCompanionObstacleTemplateVerts, _gShelter1fHeliportCompanionObstacleTemplateFaces, _gShelter1fHeliportCompanionObstacleTemplateTable, 131, 227, 1, 1, 4000, ARRAY_SIZE(_gShelter1fHeliportCompanionObstacleTemplateFaces) };

WorldCollisionRoomResources gShelter1fHeliportCollisionResources[1] = {
    { .grid = &_gShelter1fHeliportCollisionGrid, .viewBoundaryTriggers = _gShelter1fHeliportViewBoundaryTriggers, .actionTriggers = _gShelter1fHeliportActionTriggers, .occluders = NULL },
};

WorldCoordRoomLighting gShelter1fHeliportRoomLighting[1] = {
    { &_gShelter1fHeliportRoomLights, _gShelter1fHeliportAmbientByView },
};

u8* gShelter1fHeliportViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelter1fHeliportViewCounts[1] = { ARRAY_SIZE(gShelter1fHeliportViewCameras) };

enum { SHELTER_1F_HELIPORT_BULWARK_ARRIVAL_SOUND   = 0x55040002,
       SHELTER_1F_HELIPORT_BULWARK_DEPARTURE_SOUND = 0x55040001 };

DirectionWarpEntry gShelter1fHeliportWarpEntries[2] = {
    {
        .player         = { { .word = 3072 }, 9920, 0, 3450 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 10130, 0, 2880 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SHELTER_1F_HELIPORT_BULWARK_ARRIVAL_SOUND,
        .departureSound = SHELTER_1F_HELIPORT_BULWARK_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 11,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { { .word = 1024 }, 440, 0, 4340 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 3072 }, 10130, 0, 2880 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

enum { SHELTER_1F_HELIPORT_COLLISION_CELLS_X = 3,
       SHELTER_1F_HELIPORT_COLLISION_CELLS_Z = 3 };

/// Room-space collision normals; the first four are rebuilt for the companion obstacle.
static SVECTOR _gShelter1fHeliportCollisionGridNormals[12] = {
#include "assets/shelter_1f_heliport_collision_043B4_normals.inc"
};

/// Room-space collision vertices; the first eight are rebuilt for the companion obstacle.
static SVECTOR _gShelter1fHeliportCollisionGridVerts[75] = {
#include "assets/shelter_1f_heliport_collision_043B4_verts.inc"
};

/// Indexed room collision faces, with four leading slots reserved for the companion obstacle.
static WorldCollisionGridFace _gShelter1fHeliportCollisionGridFaces[40] = {
#include "assets/shelter_1f_heliport_collision_043B4_faces.inc"
};

/// Serialized face-index lists, each terminated by `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gShelter1fHeliportCollisionGridCells[172] = {
#include "assets/shelter_1f_heliport_collision_043B4_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fHeliportCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..171,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_HELIPORT_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fHeliportCollisionGridCells[(halfwordOffset)])
/// X-major cell directory; every cell includes the four companion-obstacle face slots.
static s16* _gShelter1fHeliportCollisionGridTable[SHELTER_1F_HELIPORT_COLLISION_CELLS_X * SHELTER_1F_HELIPORT_COLLISION_CELLS_Z] = {
#include "assets/shelter_1f_heliport_collision_043B4_table.inc"
};
#undef SHELTER_1F_HELIPORT_CELL_FACE_LIST_AT_OFFSET

/// Complete room collision grid, including the mutable companion-obstacle slots.
///
/// The 3 by 3 XZ cells are 4000 game units wide, with room origin (0, -2800).
/// The rebuild replaces only the first four normals/faces and eight vertices;
/// cell lists and the remaining room geometry stay intact. Activation borrows
/// the current view coordinate; the record and pools live with this overlay.
static WorldCollisionGrid _gShelter1fHeliportCollisionGrid = { NULL, _gShelter1fHeliportCollisionGridNormals, _gShelter1fHeliportCollisionGridVerts, _gShelter1fHeliportCollisionGridFaces, _gShelter1fHeliportCollisionGridTable, 0, 2800, SHELTER_1F_HELIPORT_COLLISION_CELLS_X, SHELTER_1F_HELIPORT_COLLISION_CELLS_Z, 4000, ARRAY_SIZE(_gShelter1fHeliportCollisionGridFaces) };

ViewCamera gShelter1fHeliportViewCameras[SHELTER_1F_HELIPORT_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -4500, 0x3CE8, -3400 } }, 235 },
    { { { { 338, 0, -4082 }, { -81, 4095, -6 }, { 4081, 81, 337 } }, { -550, 1690, -2890 } }, 230 },
    { { { { 604, 0, 4051 }, { 79, 4095, -11 }, { -4050, 80, 604 } }, { -7570, 1700, -2750 } }, 230 },
    { { { { 3910, 0, 1218 }, { -32, 4094, 105 }, { -1217, -110, 3909 } }, { -2572, 1124, -1708 } }, 269 },
    { { { { -4049, 0, 618 }, { -54, 4079, -359 }, { -615, -363, -4033 } }, { -8235, 928, -4313 } }, 289 },
    { { { { 576, -68, 4054 }, { -143, 4092, 89 }, { -4052, -154, 573 } }, { -3633, 810, -4286 } }, 289 },
    { { { { 3673, -67, 1810 }, { -125, 4073, 407 }, { -1807, -420, 3651 } }, { -1386, 1256, -5618 } }, 289 },
    { { { { 2691, -68, 3086 }, { -5, 4094, 95 }, { -3087, -66, 2690 } }, { -2397, 1279, -5079 } }, 320 },
    { { { { -2704, -68, 3075 }, { -153, 4092, -44 }, { -3071, -144, -2705 } }, { -2303, 1279, -7476 } }, 348 },
    { { { { -4043, -68, 647 }, { -23, 4085, 285 }, { -650, 278, -4034 } }, { -1300, 1420, -7060 } }, 380 },
    { { { { 381, -42, -4077 }, { -3172, 2570, -323 }, { 2562, 3188, 206 } }, { -5360, 5890, -3200 } }, 230 },
    { { { { -1836, -49, 3660 }, { 2496, 2978, 1292 }, { -2677, 2810, -1305 } }, { -3140, 1450, -1920 } }, 329 },
};

/// View 1's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 21 textured rectangles, in view-centred pixels with unscaled OT depths.
static SpriteSource _gShelter1fHeliportView2SpriteSources[21] = {
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 72, 1239, { .fields = { 96, 240 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 24 } }, 88, -40, 1539, { .fields = { 104, 216 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 24 } }, 88, -16, 1500, { .fields = { 104, 192 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 8, 1500, { .fields = { 104, 240 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 24 } }, 96, -48, 150, { .fields = { 112, 232 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 24 } }, 96, -24, 1500, { .fields = { 80, 24 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 24 } }, 96, 0, 1500, { .fields = { 88, 216 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 24 } }, 96, 24, 1500, { .fields = { 80, 0 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, -56, 1350, { .fields = { 96, 128 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, -24, 1375, { .fields = { 96, 160 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, 8, 1375, { .fields = { 96, 96 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 128, -64, 1175, { .fields = { 112, 200 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, 40, 1375, { .fields = { 112, 168 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 128, -32, 1175, { .fields = { 96, 0 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 128, 0, 1200, { .fields = { 96, 64 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 32 } }, 128, 32, 1200, { .fields = { 96, 32 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 24 } }, 128, 64, 1193, { .fields = { 88, 192 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 40 } }, 144, -72, 1100, { .fields = { 112, 48 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 40 } }, 144, -32, 1100, { .fields = { 112, 88 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 40 } }, 144, 8, 1100, { .fields = { 112, 128 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 48 } }, 144, 48, 1100, { .fields = { 112, 0 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's image-strip selector, complete source range and batch sentinel.
static SpriteBatch _gShelter1fHeliportView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelter1fHeliportView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's 13 textured rectangles, in view-centred pixels with unscaled OT depths.
static SpriteSource _gShelter1fHeliportView11SpriteSources[13] = {
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 72, 1100, { .fields = { 120, 144 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 72, 1075, { .fields = { 120, 152 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 8 } }, 120, 80, 1175, { .fields = { 120, 112 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 8 } }, 128, 80, 1100, { .fields = { 120, 120 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 8 } }, 136, 80, 1050, { .fields = { 120, 128 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 104, 1050, { .fields = { 112, 80 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 88, 1025, { .fields = { 112, 64 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, 72, 1000, { .fields = { 112, 96 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 64, 900, { .fields = { 112, 136 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 16 } }, 128, 104, 1150, { .fields = { 112, 16 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 104, 1250, { .fields = { 112, 0 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 16, 16 } }, 128, 88, 1150, { .fields = { 112, 48 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
    { SHELTER_1F_HELIPORT_SPRITE_TEXTURE_PAGE, SHELTER_1F_HELIPORT_SPRITE_CLUT, { .fields = { 8, 16 } }, 120, 88, 1175, { .fields = { 120, 32 } }, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, SHELTER_1F_HELIPORT_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 11's image-strip selector, complete source range and batch sentinel.
static SpriteBatch _gShelter1fHeliportView11SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelter1fHeliportView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's empty sprite list: retain image strips, then stop at the sentinel.
static SpriteBatch _gShelter1fHeliportView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelter1fHeliportSpriteViews[SHELTER_1F_HELIPORT_VIEW_COUNT] = {
    { { .empty = _gShelter1fHeliportView1SpriteBatches }, _gShelter1fHeliportView1SpriteBatches, NULL },
    { { .elements = _gShelter1fHeliportView2SpriteSources }, _gShelter1fHeliportView2SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView3SpriteBatches }, _gShelter1fHeliportView3SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView4SpriteBatches }, _gShelter1fHeliportView4SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView5SpriteBatches }, _gShelter1fHeliportView5SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView6SpriteBatches }, _gShelter1fHeliportView6SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView7SpriteBatches }, _gShelter1fHeliportView7SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView8SpriteBatches }, _gShelter1fHeliportView8SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView9SpriteBatches }, _gShelter1fHeliportView9SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView10SpriteBatches }, _gShelter1fHeliportView10SpriteBatches, NULL },
    { { .elements = _gShelter1fHeliportView11SpriteSources }, _gShelter1fHeliportView11SpriteBatches, NULL },
    { { .empty = _gShelter1fHeliportView12SpriteBatches }, _gShelter1fHeliportView12SpriteBatches, NULL },
};

/// Room-axis point light, active in every view, with 4500..5000 game-unit falloff.
///
/// Gameplay parents and composes its coordinate and rewrites attenuation for
/// shading queries; retain writable overlay storage while the room is active.
static WorldCoordPointLight _gShelter1fHeliportPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5450, -4000, 3600 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, 4500, 5000 },
};

/// Four room-axis cone lights active in every view, with Q12 aims and intensities.
///
/// Full cone opening is 887 angle units (ONE per turn); inner/outer distances
/// are game units. Gameplay builds rotations, parents/composes the coordinates
/// and overwrites attenuation, so the array stays writable while loaded.
static WorldCoordSpotLight _gShelter1fHeliportConeLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 700, -1250, -1510 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2469, 809, 3165, 0 }, 0x4E20, 0x4E20, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 450, -1500, 9740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2574, 780, -3089, 0 }, 0x3A98, 0x3A98, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7900, -1750, 6890 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2537, 992, -3058, 0 }, 0x4E20, 0x4E20, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7550, -1500, -6360 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3194, 3112 }, { 0, 0 } }, { 2627, 0, 3142, 0 }, 0x4E20, 0x4E20, 887 },
};

/// The heliport's single light collection: one point, four cones, no directional lights.
static WorldCoordRoomLights _gShelter1fHeliportRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelter1fHeliportPointLights),
    .pointLights           = _gShelter1fHeliportPointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelter1fHeliportConeLights),
    .coneLights            = _gShelter1fHeliportConeLights,
};

/// Twelve directed view-boundary quads, including the LAST-marked final quad.
///
/// Parameter bytes name source/destination logical views (2, 3, 4, 5 or 11).
/// Setup binds coordinates and list links to the live view; retain writable
/// room storage until the triggers are unlinked on room replacement.
static WorldCollisionTrigger _gShelter1fHeliportViewBoundaryTriggers[12] = {
    { NULL, NULL, NULL, { 4017, -3248, 3516, 0 }, { { 131, -3712, -3671, 0 }, { -135, -3712, 3667, 0 }, { 131, 3712, -3671, 0 }, { -135, 3712, 3667, 0 } }, { 4104, 0, 148, 0 }, { 0, 0, ONE, 0 }, 5221, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4122, -3200, 3577, 0 }, { { -135, -3712, 3667, 0 }, { 131, -3712, -3670, 0 }, { -135, 3712, 3667, 0 }, { 131, 3712, -3670, 0 } }, { -4104, 0, -150, 0 }, { 0, 0, ONE, 0 }, 5221, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 94, -3232, 5853, 0 }, { { 1452, -3712, -676, 0 }, { -1451, -3712, 677, 0 }, { 1452, 3712, -676, 0 }, { -1451, 3712, 677, 0 } }, { 1733, 0, 3719, 0 }, { 0, 0, ONE, 0 }, 4039, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 110, -3232, 6014, 0 }, { { -1468, -3712, 669, 0 }, { 1468, -3712, -669, 0 }, { -1468, 3712, 669, 0 }, { 1468, 3712, -669, 0 } }, { -1702, 0, -3733, 0 }, { 0, 0, ONE, 0 }, 4039, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6829, -3296, 606, 0 }, { { 1337, -3712, 569, 0 }, { -1337, -3712, -569, 0 }, { 1337, 3712, 569, 0 }, { -1337, 3712, -569, 0 } }, { -1611, 0, 3782, 0 }, { 0, 0, ONE, 0 }, 3982, 0, 11, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6813, -3168, 766, 0 }, { { -1306, -3712, -569, 0 }, { 1307, -3712, 570, 0 }, { -1306, 3712, -569, 0 }, { 1307, 3712, 570, 0 } }, { 1642, 0, -3771, 0 }, { 0, 0, ONE, 0 }, 3974, 0, 5, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9152, -3232, 575, 0 }, { { -1160, -3712, 828, 0 }, { 1160, -3712, -827, 0 }, { -1160, 3712, 828, 0 }, { 1160, 3712, -827, 0 } }, { -2388, 0, -3347, 0 }, { 0, 0, ONE, 0 }, 3974, 0, 5, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9183, -3200, 350, 0 }, { { 1154, -3712, -836, 0 }, { -1153, -3712, 836, 0 }, { 1154, 3712, -836, 0 }, { -1153, 3712, 836, 0 } }, { 2411, 0, 3327, 0 }, { 0, 0, ONE, 0 }, 3974, 0, 11, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2847, -3200, 5856, 0 }, { { 1449, -3712, 711, 0 }, { -1448, -3712, -710, 0 }, { 1449, 3712, 711, 0 }, { -1448, 3712, -710, 0 } }, { -1807, 0, 3682, 0 }, { 0, 0, ONE, 0 }, 4047, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2846, -3265, 6046, 0 }, { { -1448, -3712, -710, 0 }, { 1449, -3712, 711, 0 }, { -1448, 3712, -710, 0 }, { 1449, 3712, 711, 0 } }, { 1806, 0, -3684, 0 }, { 0, 0, ONE, 0 }, 4047, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6653, -2944, 3869, 0 }, { { 134, -3712, -3670, 0 }, { -133, -3712, 3670, 0 }, { 134, 3712, -3670, 0 }, { -133, 3712, 3670, 0 } }, { 4105, 0, 149, 0 }, { 0, 0, ONE, 0 }, 5221, 0, 11, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6782, -2912, 4031, 0 }, { { -178, -3712, 3669, 0 }, { 178, -3712, -3668, 0 }, { -178, 3712, 3669, 0 }, { 178, 3712, -3668, 0 } }, { -4092, 0, -199, 0 }, { 0, 0, ONE, 0 }, 5221, 0, 2, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and departure regions, including the LAST-marked final region.
///
/// Selectors distinguish room action IDs, direct CAP indices, room command
/// messages, warps and latched room-event regions. Quad coordinates and radii
/// use room game units; normals/facing vectors use ONE per unit. Runtime setup
/// binds the view coordinate and mutates links, flags and hits until unlinking.
static WorldCollisionTrigger _gShelter1fHeliportActionTriggers[21] = {
    { NULL, NULL, NULL, { 416, -48, 4704, 0 }, { { -448, 0, -608, 0 }, { 448, 0, -608, 0 }, { -448, 0, 608, 0 }, { 448, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { ONE, 0, 0, 0 }, 754, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_TENT, SHELTER_1F_HELIPORT_TENT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1168, 0 }, { { -336, 0, -688, 0 }, { 1424, 0, -688, 0 }, { -336, 0, 1392, 0 }, { 1424, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { -ONE, 0, 0, 0 }, 1991, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_SHOP, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 832, -64, 3040, 0 }, { { -1008, 0, -848, 0 }, { 1104, 0, -848, 0 }, { -1008, 0, 1104, 0 }, { 1104, 0, 1104, 0 } }, { 0, 4094, 0, 0 }, { -ONE, 0, 0, 0 }, 1557, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_SOLDIER_TALK_B, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2880, -64, 5056, 0 }, { { -1552, 0, -1328, 0 }, { 752, 0, -1328, 0 }, { -1552, 0, 816, 0 }, { 752, 0, 816, 0 } }, { 0, 4113, 0, 0 }, { -ONE, 0, 0, 0 }, 2039, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_SOLDIER_TALK_A, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 6944, 0 }, { { -624, 0, -1168, 0 }, { 1296, 0, -1168, 0 }, { -624, 0, 400, 0 }, { 1296, 0, 400, 0 } }, { 0, 4108, 0, 0 }, { -ONE, 0, 0, 0 }, 1740, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_TALK, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1968, -64, 7760, 0 }, { { -1120, 0, -288, 0 }, { 1120, 0, -288, 0 }, { -1120, 0, 288, 0 }, { 1120, 0, 288, 0 } }, { 0, 4114, 0, 0 }, { 600, 0, -4053, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_COMMAND_LOOP_DIALOGUE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -64, 1504, 0 }, { { -688, 0, -496, 0 }, { 1264, 0, -496, 0 }, { -688, 0, 1168, 0 }, { 1264, 0, 1168, 0 } }, { 0, 4102, 0, 0 }, { -ONE, 0, 0, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_COMMAND_CHECK_TALK, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3344, -64, 5232, 0 }, { { -512, 0, -736, 0 }, { 512, 0, -736, 0 }, { -512, 0, 736, 0 }, { 512, 0, 736, 0 } }, { 0, 4099, 0, 0 }, { -ONE, 0, 0, 0 }, 896, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_COMPANION_REMINDER, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4416, -64, 5232, 0 }, { { -512, 0, -752, 0 }, { 512, 0, -752, 0 }, { -512, 0, 752, 0 }, { 512, 0, 752, 0 } }, { 0, ONE, 0, 0 }, { ONE, 0, 0, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_COMPANION_REMINDER, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3872, -64, 4432, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_COMPANION_REMINDER, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3888, -64, 5920, 0 }, { { -1024, 0, -1904, 0 }, { 1024, 0, -1904, 0 }, { -1024, 0, -688, 0 }, { 1024, 0, -688, 0 } }, { 0, 4101, 0, 0 }, { -ONE, 0, 0, 0 }, 2157, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_HELIPORT_ACTION_COMPANION_REMINDER, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2880, -64, 4992, 0 }, { { 0, 0, -1120, 0 }, { 896, 0, -1120, 0 }, { 0, 0, 960, 0 }, { 896, 0, 960, 0 } }, { 0, ONE, 0, 0 }, { -ONE, 0, 0, 0 }, 1431, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, 4928, 0 }, { { -512, 0, -1120, 0 }, { 640, 0, -1120, 0 }, { -512, 0, 960, 0 }, { 640, 0, 960, 0 } }, { 0, 4108, 0, 0 }, { ONE, 0, 0, 0 }, 1286, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3168, -64, 3872, 0 }, { { -512, 0, -192, 0 }, { 1920, 0, -192, 0 }, { -512, 0, 960, 0 }, { 1920, 0, 960, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, -4091, 0 }, 2141, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9759, -64, 1519, 0 }, { { -648, 0, -163, 0 }, { 97, 0, -661, 0 }, { -96, 0, 662, 0 }, { 649, 0, 164, 0 } }, { 0, 4106, 0, 0 }, { -3703, 0, 1751, 0 }, 668, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_BULWARK, SHELTER_1F_HELIPORT_BULWARK_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5616, -64, 5296, 0 }, { { -3280, 0, -368, 0 }, { 3280, 0, -368, 0 }, { -3280, 0, 368, 0 }, { 3280, 0, 368, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -ONE, 0 }, 3298, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_TRIGGER_CAP_24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2784, -64, 6592, 0 }, { { -448, 0, -1376, 0 }, { 448, 0, -1376, 0 }, { -448, 0, 1376, 0 }, { 448, 0, 1376, 0 } }, { 0, 4107, 0, 0 }, { -ONE, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_TRIGGER_CAP_24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5040, -64, 1216, 0 }, { { -2080, 0, -368, 0 }, { 2080, 0, -368, 0 }, { -2080, 0, 368, 0 }, { 2080, 0, 368, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 2111, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_TRIGGER_CAP_26, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8208, -64, -2592, 0 }, { { -816, 0, -368, 0 }, { 816, 0, -368, 0 }, { -816, 0, 368, 0 }, { 816, 0, 368, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, ONE, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_TRIGGER_CAP_27, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2880, -64, 3200, 0 }, { { -448, 0, -1376, 0 }, { 448, 0, -1376, 0 }, { -448, 0, 1376, 0 }, { 448, 0, 1376, 0 } }, { 0, 4107, 0, 0 }, { -ONE, 0, 0, 0 }, 1442, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_1F_HELIPORT_TRIGGER_CAP_2A, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3904, -64, 6080, 0 }, { { -1248, 0, -2624, 0 }, { 1248, 0, -2624, 0 }, { -1248, 0, -832, 0 }, { 1248, 0, -832, 0 } }, { 0, 4117, 0, 0 }, { ONE, 0, 0, 0 }, 2896, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Header count followed by ambient RGB minima for logical views 1..12.
///
/// RGB uses 16 units per 8-bit level. Stored luma is (3R + 4G + B)/8;
/// lighting reads RGB only. Views 3/4 are brighter; the others use level 1.
/// Retains its authored data-section position beside the room resources.
static const WorldCoordRoomAmbientEntry _gShelter1fHeliportAmbientByView[SHELTER_1F_HELIPORT_VIEW_COUNT + 1] __attribute__((section(".data"))) = {
    { .viewCount = ARRAY_SIZE(_gShelter1fHeliportAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1838, 1819, 1496, 1785 } },
    { .color = { 1854, 1818, 1567, 1800 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Variant 1's resource recipes for Soldier B and Jodie, followed by END.
///
/// Placement IDs 116/115 select decimal files 615/605 with group-base selectors
/// 10/20 and task slot 0. The loader/spawner borrows this table and actor exports.
static AreaResource _gShelter1fHeliportVariant1Resources[3] = {
    { 116, 615, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor161500SoldierBTasks },
    { 115, 605, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor260500JodieTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's resource recipes for Soldier B and Rupert, followed by END.
///
/// Placement IDs 116/144 select decimal files 615/604 with group-base selectors
/// 10/20 and task slot 0. The loader/spawner borrows this table and actor exports.
static AreaResource _gShelter1fHeliportVariant2Resources[3] = {
    { 116, 615, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor161500SoldierBTasks },
    { 144, 604, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor260400RupertBroderickTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gShelter1fHeliportAreaVariants[13] = {
    [1] = { .placements = gMapNeoArkHeliportVariant1Placements, .resources = _gShelter1fHeliportVariant1Resources },
    [2] = { .placements = gMapNeoArkHeliportVariant2Placements, .resources = _gShelter1fHeliportVariant2Resources },
};

/// Cue-2 bases in the loaded type-1 sound bank; cue 1 uses the next entry.
enum {
    SHELTER_1F_HELIPORT_SURFACE_1_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SHELTER_1F_HELIPORT_SURFACE_1_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
};

/// Surface class 1's paired footstep cues; walking and scripted jumps share a pair.
///
/// Cue 1 adds one to the base. The gameplay cue reader adds 100 for companions;
/// the heliport follower instead maps the walk base to its actor-bank pair.
/// Borrows the current type-1 bank while this overlay's surface table is active.
/// Retains its authored data-section position beside the surface properties.
static const WorldCollisionFootstepSounds _gShelter1fHeliportSurface1FootstepSounds __attribute__((section(".data"))) = {
    .walk         = SHELTER_1F_HELIPORT_SURFACE_1_WALK_CUE_BASE,
    .run          = SHELTER_1F_HELIPORT_SURFACE_1_RUN_CUE_BASE,
    .scriptedJump = SHELTER_1F_HELIPORT_SURFACE_1_WALK_CUE_BASE,
};

/// Shared blocking, impact-enabled, pushback-enabled policy without footstep cues.
static WorldCollisionSurfaceProperties _gShelter1fHeliportDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface class 1's blocking, impact-enabled, pushback-enabled policy with footstep cues.
static WorldCollisionSurfaceProperties _gShelter1fHeliportSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelter1fHeliportSurface1FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelter1fHeliportSurfacePropertiesByClass[8] = {
    &_gShelter1fHeliportDefaultSurfaceProperties,
    &_gShelter1fHeliportSurface1Properties,
    &_gShelter1fHeliportDefaultSurfaceProperties,
    &_gShelter1fHeliportDefaultSurfaceProperties,
    &_gShelter1fHeliportDefaultSurfaceProperties,
    &_gShelter1fHeliportDefaultSurfaceProperties,
    &_gShelter1fHeliportDefaultSurfaceProperties,
    &_gShelter1fHeliportDefaultSurfaceProperties,
};

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
static s32 _gShopRefillDisplayQuantityFixed = 0;

/// Borrowed weapon-supply catalog row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalog must remain loaded throughout the session.
static const EquipmentWeaponSupply* _gShopRefillSupply = NULL;

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest departure-event gate call spawned a staged event (0/1).
///
/// Cleared even by queries and already-recorded departures; this byte does
/// not track the task's lifetime. No recovered consumer reads it.
static s8 _gShelter1fHeliportEventStarted = 0;

/// Retained byte initialized to 2; its role and object boundary are unproven.
u8 D_shelter_1f_heliport_80182CB1 = 2;

/// Retained byte initialized to 68; its role and object boundary are unproven.
u8 D_shelter_1f_heliport_80182CB2 = 68;

/// Retained byte initialized to 224; its role and object boundary are unproven.
u8 D_shelter_1f_heliport_80182CB3 = 224;

RoomLatchedEvent gRoomEventLatched = { 0 };

/// Retained byte following this room's weapon-supply refill title terminator.
///
/// One-byte string-literal binding for `shop.inc.c`; label drawing does not read it.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\x32"
#include "../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

#include "../../shared/room/room_event_staged_task.inc.c"

/// Controller states 0 initialize, 1 maintain placed-actor draw modes, 2 tear down.
///
/// The dispatcher copies all three callbacks by value before indexing them;
/// the task state must remain in 0..2. This table lives with the room overlay.
static const TaskFuncTable3 _gShelter1fHeliportRoomStateHandlers = {
    {
        _shelter1fHeliportInitializeRoomTask,
        _shelter1fHeliportUpdateRoom,
        taskKill,
    },
};

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 when the room handles the departure, including an eligible query;
/// returns 1 for ordinary departure when a nonzero event flag is already set.
/// Every call clears the latest-start byte. Only `ROOM_EVENT_EXECUTE` copies
/// both records, writes 1 to a nonzero flag and raises that byte after spawning.
/// Borrows complete records for this call; flag IDs must be 0..503. The room's
/// singleton copies and CAP/sound resources must remain live until its task ends;
/// do not latch another event while that task still uses them.
static inline s32 _shelter1fHeliportStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    _gShelter1fHeliportEventStarted = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *message;
            gRoomEventLatched             = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelter1fHeliportDepartureEventTaskDesc, 0, 0, 0);
            _gShelter1fHeliportEventStarted = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

/// Stops both heliport ambience scripts while retaining their release envelopes.
///
/// Queues exact sound-ID stops for heliport bank entries 6 and 7; neither waits
/// for silence. Departure execution calls this before transferring the room.
static inline void _shelter1fHeliportStopAmbience(void)
{
    sndEvtRequestScriptStop(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_1, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_2, SOUND_SCRIPT_STOP_KEEP_RELEASE);
}

/// Resolves heliport departures and stages the Bulwark departure scene.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete request and writable reply,
/// which may alias. Copies all eight bytes before resolving the destination.
/// Returns 2 when the initial variant-1 talk blocks departure or an event owns
/// it, otherwise 1. Queries preserve destination selectors and do not stop
/// ambience or start CAP/tasks, but the event gate clears its latest-start byte.
/// Executing Tent/Bulwark departures stop ambience; the Bulwark scene retains
/// copies of both records. Requires live session, flags and room resources.
/// The receiver and message ID are unused.
static s32 _shelter1fHeliportResolveDepartureMessage(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_1F_HELIPORT_VARIANT_INITIAL               = 1,
           SHELTER_1F_HELIPORT_TALK_UNSTARTED                = 0,
           SHELTER_1F_HELIPORT_CAP_TALK_REQUIRED             = 0x2B,
           SHELTER_1F_HELIPORT_CAP_BULWARK_DEPARTURE         = 0x29,
           SHELTER_1F_HELIPORT_BULWARK_DEPARTURE_STAGE_SOUND = 0x55040001,
           SHELTER_1F_HELIPORT_EVENT_NO_FLAG                 = 0,
           SHELTER_1F_HELIPORT_EVENT_FADE                    = 1,
           SHELTER_1F_HELIPORT_DEPARTURE_DIRECT              = 1,
           SHELTER_1F_HELIPORT_DEPARTURE_HANDLED             = 2 };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_1F_TENT && request->queryOnly == ROOM_EVENT_EXECUTE) {
        _shelter1fHeliportStopAmbience();
    }
    if (request->areaId == GAME_AREA_SHELTER_1F_BULWARK) {
        if (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS) == SHELTER_1F_HELIPORT_TALK_UNSTARTED && gGameSession->location.variant == SHELTER_1F_HELIPORT_VARIANT_INITIAL) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(SHELTER_1F_HELIPORT_CAP_TALK_REQUIRED);
            }
            return SHELTER_1F_HELIPORT_DEPARTURE_HANDLED;
        }
        event.capCmd   = SHELTER_1F_HELIPORT_CAP_BULWARK_DEPARTURE;
        event.stageSnd = SHELTER_1F_HELIPORT_BULWARK_DEPARTURE_STAGE_SOUND;
        event.flagId   = SHELTER_1F_HELIPORT_EVENT_NO_FLAG;
        event.fade     = SHELTER_1F_HELIPORT_EVENT_FADE;
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            _shelter1fHeliportStopAmbience();
        }
        return _shelter1fHeliportStartEvent(reply, &event);
    }
    return SHELTER_1F_HELIPORT_DEPARTURE_DIRECT;
}

void shelter1fHeliportUpdateCompanionObstacle(s32 unusedEventArg)
{
    enum { SHELTER_1F_HELIPORT_SOLDIER_REQUEST_PENDING = 1,
           SHELTER_1F_HELIPORT_HIDDEN_OBSTACLE_Y       = 10000 };
    Task* obstacleTask;
    Task* companionTask;

    obstacleTask  = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    companionTask = obstacleTask;
    if (obstacleTask == NULL) {
        obstacleTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    // Move an inactive obstacle out of the walkable room without changing cells.
    if (companionTask != NULL && gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE) == SHELTER_1F_HELIPORT_SOLDIER_REQUEST_PENDING) {
        _gShelter1fHeliportCompanionObstacleOffset.vy = 0;
    } else {
        _gShelter1fHeliportCompanionObstacleOffset.vy = SHELTER_1F_HELIPORT_HIDDEN_OBSTACLE_Y;
    }
    _followCollisionRebuildObstacle(obstacleTask->extra.tmd->coords, &_gShelter1fHeliportCompanionObstacleOffset);
}

/// Tests the borrowed pending-trigger list for a hit room-event region.
///
/// The list must be live and NULL-terminated. Does not consume hits or retain nodes.
/// Returns 1 at the first nonzero hit with exact room-action control and room
/// event ID 0xFF, or 0 after the list. No extra facing or enabled-bit test runs.
static inline s32 _shelter1fHeliportRoomTriggerHit(void)
{
    const WorldCollisionTrigger* trigger;
    for (trigger = gWorldCollisionActionTriggerListHead; trigger != NULL; trigger = trigger->next) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return true;
        }
    }
    return false;
}

/// Starts the pending companion-request handover when item 0x124 is used nearby.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` supplies a collected-item ID and unused second word.
/// Requires request state 1, a live companion and a hit pending room-event region.
/// Accepting hides the HUD, marks the session active, selects companion schedule 9
/// and spawns Soldier B's handover task. Returns the item-menu used-notice reply;
/// the actor task marks the item identified and completes the request later.
/// Otherwise returns the refused reply. The trigger list is borrowed only here;
/// keep the actor/room overlays and scene resources loaded through the handover.
/// The receiver and message ID are unused.
static s32 _shelter1fHeliportUseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    enum { SHELTER_1F_HELIPORT_REQUEST_ITEM_ID             = 0x124,
           SHELTER_1F_HELIPORT_REQUEST_PENDING             = 1,
           SHELTER_1F_HELIPORT_COMPANION_HANDOVER_SCHEDULE = 9,
           SHELTER_1F_HELIPORT_EVENT_ACTIVE                = 1 };

    if (itemId == SHELTER_1F_HELIPORT_REQUEST_ITEM_ID && gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE) == SHELTER_1F_HELIPORT_REQUEST_PENDING && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        if (_shelter1fHeliportRoomTriggerHit() != 0) {
            gGameSession->hideHud    = true;
            gGameSession->eventState = SHELTER_1F_HELIPORT_EVENT_ACTIVE;
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, SHELTER_1F_HELIPORT_COMPANION_HANDOVER_SCHEDULE);
            taskSpawnFromTableOnDefaultList(&gActor161500CompleteCompanionRequestTaskDesc, 0, 0, 0);
            return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Handles the heliport's repeating-dialogue and talk-progress CAP commands.
///
/// `ROOM_MESSAGE_COMMAND` supplies `commandId`; 0x21 holds player control and
/// starts repeating CAP playback, and 0x22 selects CAP 0x22 or 0x25 according
/// to Soldier B's second talk count (at least 2 in variant 1, otherwise 1).
/// The latter starts only when CAP is idle. Other commands are ignored; all
/// return zero. Requires live player and loaded room CAP/task resources until
/// playback ends. The receiver, message ID and second word are unused.
static s32 _shelter1fHeliportHandleCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { SHELTER_1F_HELIPORT_CAP_TALK_NOT_READY       = 0x25,
           SHELTER_1F_HELIPORT_TALKS_REQUIRED           = 1,
           SHELTER_1F_HELIPORT_VARIANT_1_TALKS_REQUIRED = 2,
           SHELTER_1F_HELIPORT_VARIANT_INITIAL          = 1 };
    s32 requiredTalkCount;

    switch (commandId) {
        case SHELTER_1F_HELIPORT_COMMAND_LOOP_DIALOGUE:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&_gShelter1fHeliportDialogueLoopTaskDesc, 0, SHELTER_1F_HELIPORT_COMMAND_LOOP_DIALOGUE, 0);
            break;
        case SHELTER_1F_HELIPORT_COMMAND_CHECK_TALK:
            requiredTalkCount = SHELTER_1F_HELIPORT_TALKS_REQUIRED;
            if (gGameSession->location.variant == SHELTER_1F_HELIPORT_VARIANT_INITIAL) {
                requiredTalkCount = SHELTER_1F_HELIPORT_VARIANT_1_TALKS_REQUIRED;
            }
            capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B) >= requiredTalkCount ? SHELTER_1F_HELIPORT_COMMAND_CHECK_TALK : SHELTER_1F_HELIPORT_CAP_TALK_NOT_READY, CAP_EVENT_NO_FLAGS);
            break;
    }
    return 0;
}

/// Starts the actor conversation selected by a heliport direction action.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request and sends a zero
/// second word. Actions 1..5 select the variant's talker, Soldier B talk B,
/// talk A, shop dialogue and companion-request reminder. Other IDs do nothing.
/// Reads only the unsigned action ID; control and argument are ignored. Returns
/// zero for every action and retains no request pointer. Selected actor packages
/// and their room/CAP resources must stay loaded until the event scripts end.
/// The receiver, message ID and second word are unused.
static s32 _shelter1fHeliportHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { SHELTER_1F_HELIPORT_VARIANT_INITIAL = 1,
           SHELTER_1F_HELIPORT_VARIANT_LATER   = 2 };

    switch (request->actionId) {
        case SHELTER_1F_HELIPORT_ACTION_TALK:
            if (gGameSession->location.variant == SHELTER_1F_HELIPORT_VARIANT_INITIAL) {
                actor260500StartHeliportConversation();
            }
            if (gGameSession->location.variant == SHELTER_1F_HELIPORT_VARIANT_LATER) {
                actor260400StartHeliportConversation();
            }
            break;
        case SHELTER_1F_HELIPORT_ACTION_SOLDIER_TALK_B:
            actor161500StartSoldierBTalkB();
            break;
        case SHELTER_1F_HELIPORT_ACTION_SOLDIER_TALK_A:
            actor161500StartSoldierBTalkA();
            break;
        case SHELTER_1F_HELIPORT_ACTION_SHOP:
            actor161500StartSoldierBShopConversation();
            break;
        case SHELTER_1F_HELIPORT_ACTION_COMPANION_REMINDER:
            actor161500StartCompanionRequestReminder();
            break;
    }
    return 0;
}

#include "../../shared/room/cap_dialogue_loop.inc.c"

/// Initializes the heliport controller, actor presentation and ambience.
///
/// Enter with the bodyless room task in state 0, an initialized scene manager,
/// live session/save, player model and room collision/CAP/sound resources.
/// Variants 1 and 2 require their respective talker packages; a live companion
/// also requires Soldier B's package and player/companion roots in one frame.
/// Publishes this task before actor scripts can address the room, restores
/// placement and companion request state, updates collision/visibility, resets
/// model buffers, starts both ambience scripts and advances to state 1.
/// Keep the room and imported actor code loaded while its callbacks/tasks run.
static void _shelter1fHeliportInitializeRoomTask(Task* task)
{
    enum { SHELTER_1F_HELIPORT_VARIANT_INITIAL = 1,
           SHELTER_1F_HELIPORT_VARIANT_LATER   = 2 };

    // Publish the message receiver before restoring actor event scripts.
    task->msgTable = _gShelter1fHeliportRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.variant == SHELTER_1F_HELIPORT_VARIANT_INITIAL) {
        actor260500RestoreHeliportPlacement();
    }
    if (gGameSession->location.variant == SHELTER_1F_HELIPORT_VARIANT_LATER) {
        actor260400RestoreHeliportPlacement();
    }
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        actor161500RestoreCompanionRequestScene();
    }
    shelter1fHeliportUpdateCompanionObstacle(0);
    _shelter1fHeliportUpdatePlacedActorVisibility();
    // Finish restored presentation before resetting buffers and starting ambience.
    gpuResetAndInvalidateModelBuffers();
    tmdResetAuxHeapAndRestoreBuffers();
    sndEvtRequestScriptStart(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_1, 0, 0);
    sndEvtRequestScriptStart(SOUND_SHELTER_1F_HELIPORT_AMBIENCE_2, 0, 0);
    task->state = task->state + 1;
}

/// Maintains the room's view-selected actor visibility while its controller idles.
///
/// State 1 leaves the task and state intact for messages and external teardown.
/// `unusedTask` is required by the state-handler interface and is ignored.
static void _shelter1fHeliportUpdateRoom(Task* unusedTask)
{
    _shelter1fHeliportUpdatePlacedActorVisibility();
}

void shelter1fHeliportRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelter1fHeliportRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Applies this view's model-draw modes to the room's four placed actors.
///
/// Only variants below 3 and live-save views 0..11 participate. A zero first
/// mode leaves every actor unchanged; otherwise all four bytes are forwarded
/// to placement indices 0..3 (1 show, 2 hide and skip automatic buffering). Missing
/// placements are ignored by the scene API. Retains no actor pointers.
static void _shelter1fHeliportUpdatePlacedActorVisibility(void)
{
    enum { SHELTER_1F_HELIPORT_VISIBILITY_VARIANT_LIMIT = 3,
           SHELTER_1F_HELIPORT_VISIBILITY_KEEP          = 0 };
    s32 placeIndex;
    s32 viewIndex = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;

    if (gGameSession->location.variant < SHELTER_1F_HELIPORT_VISIBILITY_VARIANT_LIMIT && viewIndex < (s32)ARRAY_SIZE(_gShelter1fHeliportPlacedActorDrawModesByView)) {
        if (_gShelter1fHeliportPlacedActorDrawModesByView[viewIndex][0] != SHELTER_1F_HELIPORT_VISIBILITY_KEEP) {
            for (placeIndex = 0; placeIndex < (s32)ARRAY_SIZE(_gShelter1fHeliportPlacedActorDrawModesByView[viewIndex]); placeIndex++) {
                sceneSetPlacedActorDrawMode(placeIndex, _gShelter1fHeliportPlacedActorDrawModesByView[viewIndex][placeIndex]);
            }
        }
    }
}

/// Selects the heliport's mutable room grid receiving the companion obstacle.
///
/// Binds a `WorldCollisionGrid` object lvalue; the fragment takes its address
/// once per rebuild. Writable, word-aligned mesh pools reserve normals/faces
/// 0..3 and vertices 0..7 for the obstacle, disjoint from the source pools.
/// Normals use 4096 for unit length; vertices use signed room-coordinate units.
/// Every cell already references those four faces, so moving the obstacle
/// preserves the cell lists, grid descriptor and remaining room geometry.
/// The record and pools live with the room overlay through rebuilding and
/// active collision queries. Define before the rebuild fragment include and
/// undefine immediately after it.
#define FOLLOW_COLLISION_ROOM_GRID _gShelter1fHeliportCollisionGrid

/// Selects the heliport's model-local companion-obstacle template.
///
/// Binds a `WorldCollisionGrid` object lvalue, whose address the fragment takes
/// once per rebuild. Its four quad faces, four normals and eight vertices are
/// read without modifying the source. Normals use 4096 for unit length; vertices
/// use signed game-coordinate units. The mesh pools live with the overlay and
/// must be disjoint from the room grid's pools. Cell metadata is unused.
/// Define before the rebuild fragment include and undefine immediately after it.
#define FOLLOW_COLLISION_OBSTACLE_SOURCE _gShelter1fHeliportCompanionObstacleTemplate
#include "../../shared/room/follow_collision_rebuild.inc.c"
#undef FOLLOW_COLLISION_OBSTACLE_SOURCE
#undef FOLLOW_COLLISION_ROOM_GRID

void shelter1fHeliportNoOpEffectTask(Task* unusedTask)
{
}
