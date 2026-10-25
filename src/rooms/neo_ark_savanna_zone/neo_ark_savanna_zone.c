#include "rooms/neo_ark_savanna_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_02000.h"
#include "actors/actor_02500.h"
#include "actors/actor_02600.h"
#include "actors/actor_03800.h"
#include "actors/actor_05600.h"
#include "actors/actor_05700.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/room/room_events.h"

static RoomEventMsg     _gRoomEventStagedMsg;
static s8               _gNeoArkSavannaZoneEventStartRequested;
extern RoomLatchedEvent gRoomEventLatched;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage gRoomEventFade;

static WorldCollisionGrid               _gNeoArkSavannaZoneCollisionGrid;
static WorldCollisionOccluder           _gNeoArkSavannaZoneSightOccluders[1];
static WorldCollisionTrigger            _gNeoArkSavannaZoneViewBoundaryTriggers[4];
static WorldCollisionTrigger            _gNeoArkSavannaZoneActionTriggers[5];
static const WorldCoordRoomAmbientEntry _gNeoArkSavannaZoneAmbientTable[NEO_ARK_SAVANNA_ZONE_VIEW_COUNT + 1];
static WorldCoordRoomLights             _gNeoArkSavannaZoneRoomLights;
static s32                              _neoArkSavannaZoneResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32                              _neoArkSavannaZoneRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32                              _neoArkSavannaZoneIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32                              _neoArkSavannaZoneIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Bodyless staged-departure task recipe for the savanna room.
///
/// Spawns entry 0 at `TASK_PRIORITY_SCENE` with zero metadata and payloads.
/// The descriptor is read synchronously; the room-owned destination/event
/// snapshots and callback code remain loaded through deferred scene reload.
static TaskDesc _gNeoArkSavannaZoneStagedEventTaskDesc = { .header = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } }, .callback = roomEventStagedTask, .data = { .value = 0 } };

/// Message routes borrowed by the savanna room controller.
///
/// Resolves departure requests, refuses key-item use and ignores direction
/// actions and CAP room commands. Resolve requests borrow `RoomEventMsg`
/// request/reply pointers; actions borrow a `DirectionActionRequest`, while
/// item and CAP selectors are integers. Four routes precede the END record.
/// Keep the table and its callback code loaded while the room task uses them.
static TaskMessageEntry _gNeoArkSavannaZoneRoomMessages[5] = {
    { .messageId = ROOM_EVENT_MESSAGE_RESOLVE, .handler = _neoArkSavannaZoneResolveRoomEvent },
    { .messageId = ROOM_MESSAGE_USE_KEY_ITEM, .handler = _neoArkSavannaZoneRejectKeyItemUse },
    { .messageId = DIRECTION_MESSAGE_ROOM_ACTION, .handler = _neoArkSavannaZoneIgnoreRoomAction },
    { .messageId = ROOM_MESSAGE_COMMAND, .handler = _neoArkSavannaZoneIgnoreRoomCommand },
    { .messageId = TASK_MESSAGE_TABLE_END, .handler = NULL },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gNeoArkSavannaZoneRoomCollisionResources[1] = {
    { .grid = &_gNeoArkSavannaZoneCollisionGrid, .viewBoundaryTriggers = _gNeoArkSavannaZoneViewBoundaryTriggers, .actionTriggers = _gNeoArkSavannaZoneActionTriggers, .occluders = _gNeoArkSavannaZoneSightOccluders },
};

WorldCoordRoomLighting gNeoArkSavannaZoneRoomLighting[1] = {
    { &_gNeoArkSavannaZoneRoomLights, _gNeoArkSavannaZoneAmbientTable },
};

u8* gNeoArkSavannaZoneViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkSavannaZoneViewCounts[1] = { NEO_ARK_SAVANNA_ZONE_VIEW_COUNT };

DirectionWarpEntry gNeoArkSavannaZoneWarpEntries[2] = {
    { .player = { { .word = 1024 }, 510, 0, 1500 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 1024 }, 510, 0, 1500 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 2), .departureSound = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 1), .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 4, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
    { .player = { { .word = 3072 }, 0x34BC, 0, 1500 }, .unknown_10 = { 0, 0, 0, 0 }, .companion = { { .word = 3072 }, 0x34BC, 0, 1500 }, .unknown_24 = { 0, 0, 0, 0 }, .arrivalSound = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 4), .departureSound = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SAVANNA_ZONE, 3), .blockedSound = DIRECTION_WARP_SOUND_NONE, .initialView = 2, .flags = DIRECTION_WARP_FLAG_NONE, .mapFlagId = DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Grid-local unit normals, scaled by 4096 and indexed by the grid's faces.
static SVECTOR _gNeoArkSavannaZoneCollisionGridNormals[5] = {
#include "assets/neo_ark_savanna_zone_collision_02610_normals.inc"
};

/// Collision mesh vertices in signed room-coordinate units.
static SVECTOR _gNeoArkSavannaZoneCollisionGridVerts[12] = {
#include "assets/neo_ark_savanna_zone_collision_02610_verts.inc"
};

/// Indexed collision faces with room-local surface classes 0..3.
static WorldCollisionGridFace _gNeoArkSavannaZoneCollisionGridFaces[11] = {
#include "assets/neo_ark_savanna_zone_collision_02610_faces.inc"
};

/// Four serialized face-index lists, each ending at `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gNeoArkSavannaZoneCollisionGridCells[32] = {
#include "assets/neo_ark_savanna_zone_collision_02610_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gNeoArkSavannaZoneCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define NEO_ARK_SAVANNA_ZONE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkSavannaZoneCollisionGridCells[(halfwordOffset)])
/// X-major directory into the grid's four face-index lists.
static s16* _gNeoArkSavannaZoneCollisionGridTable[4] = {
#include "assets/neo_ark_savanna_zone_collision_02610_table.inc"
};
#undef NEO_ARK_SAVANNA_ZONE_CELL_FACE_LIST_AT_OFFSET

/// Savanna collision mesh and its 4-by-1 XZ grid of 4000-unit cells.
///
/// The grid origin is (-50, -50) in room XZ coordinates. Activation binds the
/// view coordinate; the descriptor and mesh pools stay live with this overlay.
static WorldCollisionGrid _gNeoArkSavannaZoneCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkSavannaZoneCollisionGridNormals,
    .vertices    = _gNeoArkSavannaZoneCollisionGridVerts,
    .faces       = _gNeoArkSavannaZoneCollisionGridFaces,
    .cellFaceIds = _gNeoArkSavannaZoneCollisionGridTable,
    .xBias       = 50,
    .zBias       = 50,
    .cellCountX  = ARRAY_SIZE(_gNeoArkSavannaZoneCollisionGridTable),
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkSavannaZoneCollisionGridFaces),
};

/// Camera projection distance in pixels and neutral sprite modulation level.
enum {
    NEO_ARK_SAVANNA_ZONE_VIEW_SCREEN_DISTANCE = 329,
    NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR = 128,
};

ViewCamera gNeoArkSavannaZoneViewCameras[NEO_ARK_SAVANNA_ZONE_VIEW_COUNT] = {
    { { { { 4095, 0, 0 }, { 0, 0, -ONE }, { 0, 4095, 0 } }, { -7500, 0x77D8, -1950 } }, NEO_ARK_SAVANNA_ZONE_VIEW_SCREEN_DISTANCE },
    { { { { -676, 0, -4039 }, { -670, 4039, 112 }, { 3983, 679, -667 } }, { -6330, 1950, -2240 } }, NEO_ARK_SAVANNA_ZONE_VIEW_SCREEN_DISTANCE },
    { { { { -365, 0, -4079 }, { -37, 4095, 3 }, { 4079, 38, -365 } }, { -1580, 1430, -2240 } }, NEO_ARK_SAVANNA_ZONE_VIEW_SCREEN_DISTANCE },
    { { { { -753, 0, 4026 }, { 207, 4090, 38 }, { -4020, 211, -752 } }, { -9980, 1620, -2260 } }, NEO_ARK_SAVANNA_ZONE_VIEW_SCREEN_DISTANCE },
};

/// View 1's empty sprite list; the zero first count retains decoded image strips.
static SpriteBatch _gNeoArkSavannaZoneView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's depth-sorted texture fragments, in pixels relative to the view centre.
static SpriteSource _gNeoArkSavannaZoneView2SpriteSources[24] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -160, 16, 825, { .fields = { 80, 64 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -128, 16, 825, { .fields = { 96, 224 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -128, -8, 875, { .fields = { 56, 64 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, -8, 875, { .fields = { 64, 248 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -160, -32, 925, { .fields = { 72, 152 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, -8, 1237, { .fields = { 80, 176 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -160, -40, 1237, { .fields = { 96, 32 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -128, -40, 1237, { .fields = { 96, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -96, -24, 1237, { .fields = { 48, 128 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -160, -72, 1237, { .fields = { 96, 128 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -128, -72, 1237, { .fields = { 64, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -152, -96, 1237, { .fields = { 72, 224 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -128, -104, 1237, { .fields = { 72, 128 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -152, 8, 1025, { .fields = { 96, 96 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -120, 8, 1025, { .fields = { 96, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -88, 8, 1025, { .fields = { 112, 64 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -144, -8, 1025, { .fields = { 56, 176 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -112, 0, 1025, { .fields = { 56, 192 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -96, 40, 962, { .fields = { 64, 144 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -160, 48, 962, { .fields = { 64, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -128, 48, 962, { .fields = { 64, 32 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -160, 16, 962, { .fields = { 64, 96 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -128, 16, 962, { .fields = { 96, 192 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, 8, 962, { .fields = { 64, 88 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's four source ranges, preceded by the image-strip selector and terminated by END.
static SpriteBatch _gNeoArkSavannaZoneView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 8, 0, 0, { 0, 0 } },
    { 13, 5, 0, 0, { 2, 0 } },
    { 18, ARRAY_SIZE(_gNeoArkSavannaZoneView2SpriteSources) - 18, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's depth-sorted texture fragments, in pixels relative to the view centre.
static SpriteSource _gNeoArkSavannaZoneView3SpriteSources[35] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -120, 625, { .fields = { 8, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -80, 625, { .fields = { 8, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -40, 625, { .fields = { 8, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, -120, 625, { .fields = { 8, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, -80, 625, { .fields = { 96, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, -40, 625, { .fields = { 8, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -120, 0, 625, { .fields = { 88, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -80, -40, 625, { .fields = { 0, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, -80, -80, 625, { .fields = { 112, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 40 } }, -80, -120, 625, { .fields = { 96, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 24 } }, 0, -120, 625, { .fields = { 96, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, 40, -120, 625, { .fields = { 112, 176 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 40, -96, 625, { .fields = { 24, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -112, 96, 712, { .fields = { 112, 248 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 56, 712, { .fields = { 88, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, 56, 712, { .fields = { 88, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 16, 712, { .fields = { 88, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, 16, 712, { .fields = { 88, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -24, 712, { .fields = { 88, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, -24, 712, { .fields = { 88, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -64, 712, { .fields = { 48, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, -64, 712, { .fields = { 48, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -80, -64, 712, { .fields = { 64, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -104, 712, { .fields = { 48, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -120, -88, 712, { .fields = { 56, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 16 } }, -160, -120, 712, { .fields = { 96, 224 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -120, -120, 712, { .fields = { 88, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -80, -24, 1062, { .fields = { 120, 248 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, -32, 1062, { .fields = { 8, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 8, 1062, { .fields = { 48, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -160, 48, 1062, { .fields = { 48, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -120, 48, 1062, { .fields = { 88, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -120, 8, 1062, { .fields = { 48, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 40, 32 } }, -120, -24, 1062, { .fields = { 96, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -80, 24, 1062, { .fields = { 104, 152 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 3's three source ranges, preceded by the image-strip selector and terminated by END.
static SpriteBatch _gNeoArkSavannaZoneView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 14, 0, 0, { 2, 0 } },
    { 27, ARRAY_SIZE(_gNeoArkSavannaZoneView3SpriteSources) - 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's depth-sorted texture fragments, in pixels relative to the view centre.
static SpriteSource _gNeoArkSavannaZoneView4SpriteSources[29] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, 0, 1312, { .fields = { 8, 112 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -120, 1312, { .fields = { 8, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -80, 1312, { .fields = { 8, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -40, 1312, { .fields = { 48, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 144, 0, 1312, { .fields = { 32, 144 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 80, -120, 1312, { .fields = { 48, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 80, -80, 1312, { .fields = { 48, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 80, -40, 1312, { .fields = { 48, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, -40, 1312, { .fields = { 120, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 72, -64, 1312, { .fields = { 40, 208 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 40, -120, 1312, { .fields = { 88, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, -96, 1312, { .fields = { 104, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 8, -80, 1312, { .fields = { 16, 176 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, 32, 1700, { .fields = { 88, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -8, 1700, { .fields = { 88, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -48, 1700, { .fields = { 88, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 120, -72, 1700, { .fields = { 8, 232 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 80, -72, 1700, { .fields = { 0, 208 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 80, -48, 1700, { .fields = { 48, 0 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 80, -8, 1700, { .fields = { 48, 40 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 88, 32, 1700, { .fields = { 16, 112 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, -32, 1700, { .fields = { 16, 144 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 64, -64, 1700, { .fields = { 16, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 48, 1050, { .fields = { 8, 120 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, 32, 1050, { .fields = { 88, 160 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -8, 1050, { .fields = { 88, 200 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 120, -24, 1050, { .fields = { 48, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, 8, 1050, { .fields = { 88, 240 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 80, 40, 1050, { .fields = { 8, 80 } }, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, NEO_ARK_SAVANNA_ZONE_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 4's three source ranges, preceded by the image-strip selector and terminated by END.
static SpriteBatch _gNeoArkSavannaZoneView4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 10, 0, 0, { 2, 0 } },
    { 23, ARRAY_SIZE(_gNeoArkSavannaZoneView4SpriteSources) - 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkSavannaZoneSpriteViews[NEO_ARK_SAVANNA_ZONE_VIEW_COUNT] = {
    { { .empty = _gNeoArkSavannaZoneView1SpriteBatches }, _gNeoArkSavannaZoneView1SpriteBatches, NULL },
    { { .elements = _gNeoArkSavannaZoneView2SpriteSources }, _gNeoArkSavannaZoneView2SpriteBatches, NULL },
    { { .elements = _gNeoArkSavannaZoneView3SpriteSources }, _gNeoArkSavannaZoneView3SpriteBatches, NULL },
    { { .elements = _gNeoArkSavannaZoneView4SpriteSources }, _gNeoArkSavannaZoneView4SpriteBatches, NULL },
};

/// Two directional lights admitted in every view, with RGB intensities scaled by 4096.
///
/// The local translation holds the light direction. Coordinate composition and
/// shading update the parent, cached transform and attenuation in these records.
static WorldCoordLight _gNeoArkSavannaZoneDirectionalLights[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 952, -472, 229 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5837, 5715, 5574 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -834, -528, -362 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4243, 4202, 4138 }, { 0, 0 } },
};

/// The room's two directional lights, with no point or cone lights.
static WorldCoordRoomLights _gNeoArkSavannaZoneRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gNeoArkSavannaZoneDirectionalLights),
    .directionalLights     = _gNeoArkSavannaZoneDirectionalLights,
    .pointLightCount       = 0,
    .pointLights           = NULL,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// 4 directed camera boundaries in room-local game units.
///
/// Source/destination views are 1-based: 3->4, 4->3, 3->2, 2->3.
/// Room binding installs the view coordinate and enables all records through LAST.
/// Normals use 4096 per unit; keep the writable list live until it is unlinked.
static WorldCollisionTrigger _gNeoArkSavannaZoneViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 5503, -2305, 1552, 0 }, { { 55, -2608, -3196, 0 }, { -59, -2608, 3190, 0 }, { 55, 2609, -3196, 0 }, { -59, 2609, 3190, 0 } }, { 4098, 0, 73, 0 }, { 0, 0, ONE, 0 }, 4096, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5727, -2305, 1744, 0 }, { { -59, -2608, 3336, 0 }, { 53, -2608, -3342, 0 }, { -59, 2608, 3336, 0 }, { 53, 2608, -3342, 0 } }, { -4095, 0, -69, 0 }, { 0, 0, ONE, 0 }, 4222, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x277F, -2256, 1727, 0 }, { { -55, -2688, 3339, 0 }, { 56, -2688, -3338, 0 }, { -55, 2688, 3339, 0 }, { 56, 2688, -3338, 0 } }, { -4103, 0, -69, 0 }, { 0, 0, ONE, 0 }, 4283, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9951, -2224, 1695, 0 }, { { 57, -2688, -3193, 0 }, { -56, -2688, 3194, 0 }, { 57, 2688, -3193, 0 }, { -56, 2688, 3194, 0 } }, { 4095, 0, 72, 0 }, { 0, 0, ONE, 0 }, 4159, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// IDs stored in action records; warp pairs pack departure then arrival in four-bit nibbles.
enum {
    NEO_ARK_SAVANNA_ZONE_TRIGGER_WARP_TO_SHRINE          = (1 << 4) | 2,
    NEO_ARK_SAVANNA_ZONE_TRIGGER_WARP_TO_SOUTH_PROMENADE = (2 << 4) | 1,
    NEO_ARK_SAVANNA_ZONE_TRIGGER_CAP_COMMAND_1           = 1,
    NEO_ARK_SAVANNA_ZONE_TRIGGER_CAP_COMMAND_5           = 5,
    NEO_ARK_SAVANNA_ZONE_TRIGGER_CAP_COMMAND_4           = 4,
};

/// 5 interaction quads: 2 warps, 3 CAP commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
static WorldCollisionTrigger _gNeoArkSavannaZoneActionTriggers[5] = {
    { NULL, NULL, NULL, { 736, -48, 1376, 0 }, { { -672, 0, -736, 0 }, { 672, 0, -736, 0 }, { -672, 0, 736, 0 }, { 672, 0, 736, 0 } }, { 0, 4101, 0, 0 }, { ONE, 0, 0, 0 }, 995, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SHRINE, NEO_ARK_SAVANNA_ZONE_TRIGGER_WARP_TO_SHRINE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x33C0, -48, 1440, 0 }, { { -672, 0, -1136, 0 }, { 672, 0, -1136, 0 }, { -672, 0, 1136, 0 }, { 672, 0, 1136, 0 } }, { 0, 4103, 0, 0 }, { -ONE, 0, 0, 0 }, 1317, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_NEO_ARK_SOUTH_PROMENADE, NEO_ARK_SAVANNA_ZONE_TRIGGER_WARP_TO_SOUTH_PROMENADE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7776, -64, 272, 0 }, { { -1440, 0, -432, 0 }, { 1440, 0, -432, 0 }, { -1440, 0, 432, 0 }, { 1440, 0, 432, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, ONE, 0 }, 1498, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SAVANNA_ZONE_TRIGGER_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6992, -64, 256, 0 }, { { -6992, 0, -432, 0 }, { 6992, 0, -432, 0 }, { -6992, 0, 432, 0 }, { 6992, 0, 432, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, ONE, 0 }, 7001, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SAVANNA_ZONE_TRIGGER_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6976, -64, 2720, 0 }, { { -6992, 0, -432, 0 }, { 6992, 0, -432, 0 }, { -6992, 0, 432, 0 }, { 6992, 0, 432, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -ONE, 0 }, 7001, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SAVANNA_ZONE_TRIGGER_CAP_COMMAND_4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Desert Chaser and scorpion resources, keyed by placement entry 1 and 25.
static AreaResource _gNeoArkSavannaZoneVariant1Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, gActor02500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's Black Beetle resource, keyed by placement entry 38.
static AreaResource _gNeoArkSavannaZoneVariant2Resources[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Desert Chaser and Maggot Caterpillar resources, keyed by entry 1 and 26.
static AreaResource _gNeoArkSavannaZoneVariant3Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 26, 26, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 4's Desert Chaser and scorpion resources for its separate placement layout.
static AreaResource _gNeoArkSavannaZoneVariant4Resources[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor00100DesertChaserTaskDesc },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, gActor02500Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 5's two Pawn GOLEM resource recipes, keyed by placement entry 20 and 56.
static AreaResource _gNeoArkSavannaZoneVariant5Resources[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102000PawnGolemTasks },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor205600PawnGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Placement-entry keys, decimal file numbers and table selectors for these layouts.
enum {
    NEO_ARK_SAVANNA_ZONE_RESOURCE_PAWN_GOLEM = 56,
    NEO_ARK_SAVANNA_ZONE_RESOURCE_ROOK_GOLEM = 57,
    NEO_ARK_SAVANNA_ZONE_FILE_PAWN_GOLEM     = 56,
    NEO_ARK_SAVANNA_ZONE_FILE_ROOK_GOLEM     = 57,
};

/// Pawn and grenade-launcher Rook GOLEM recipes for placement variant 11.
///
/// Keys 56 and 57 load files 105600 and 205700 and spawn descriptor zero.
/// Paired with `gMapNeoArkSavannaZoneVariant11Placements`; AREA_PLACEMENT_END ends the list.
/// Retain room recipes, placements and actor descriptors through loading,
/// spawning and model-flag updates.
static AreaResource _gNeoArkSavannaZoneVariant11Resources[3] = {
    { .entryId = NEO_ARK_SAVANNA_ZONE_RESOURCE_PAWN_GOLEM, .fileNumber = NEO_ARK_SAVANNA_ZONE_FILE_PAWN_GOLEM, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor105600PawnGolemTasks },
    { .entryId = NEO_ARK_SAVANNA_ZONE_RESOURCE_ROOK_GOLEM, .fileNumber = NEO_ARK_SAVANNA_ZONE_FILE_ROOK_GOLEM, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor05700RookGolemTasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gNeoArkSavannaZoneAreaVariants[NEO_ARK_SAVANNA_ZONE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkSavannaZoneVariant1Placements, .resources = _gNeoArkSavannaZoneVariant1Resources },
    [2]                    = { .placements = gMapNeoArkSavannaZoneVariant2Placements, .resources = _gNeoArkSavannaZoneVariant2Resources },
    [3]                    = { .placements = gMapNeoArkSavannaZoneVariant3Placements, .resources = _gNeoArkSavannaZoneVariant3Resources },
    [4]                    = { .placements = gMapNeoArkSavannaZoneVariant4Placements, .resources = _gNeoArkSavannaZoneVariant4Resources },
    [5]                    = { .placements = gMapNeoArkSavannaZoneVariant5Placements, .resources = _gNeoArkSavannaZoneVariant5Resources },
    [11]                   = { .placements = gMapNeoArkSavannaZoneVariant11Placements, .resources = _gNeoArkSavannaZoneVariant11Resources },
};

/// Room-space sight-blocking quads borrowed by the active collision list.
///
/// The sole LAST record is linked and enabled during room binding.
/// Origins and relative corners use game units; normals use 4096 per unit.
/// Keep this mutable array loaded until the sight list is cleared. The stored
/// low flag bit is retained; its role is unproven.
static WorldCollisionOccluder _gNeoArkSavannaZoneSightOccluders[1] = {
    { .next = NULL, .prevLink = NULL, .origin = { 6928, -3008, 1552, 0 }, .vertices = { { -7952, 0, -2576, 0 }, { 7952, 0, -2576, 0 }, { -7952, 0, 2576, 0 }, { 7952, 0, 2576, 0 } }, .normal = { 0, 4103, 0, 0 }, .radius = 8318, .flags = 1 | WORLD_COLLISION_OCCLUDER_LAST, .unknown_3B = 0 },
};

/// Minimum ambient colours for views 1..4, preceded by their element count.
///
/// RGB minima use 16 units per 8-bit colour level. The luma component is retained
/// but not read by lighting. Keep the authored table in the overlay's data segment.
static const WorldCoordRoomAmbientEntry _gNeoArkSavannaZoneAmbientTable[NEO_ARK_SAVANNA_ZONE_VIEW_COUNT + 1] __attribute__((section(".data"))) = {
    { .viewCount = ARRAY_SIZE(_gNeoArkSavannaZoneAmbientTable) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 422, 394, 324, 395 } },
    { .color = { 422, 394, 324, 395 } },
    { .color = { 422, 394, 324, 395 } },
};

enum {
    NEO_ARK_SAVANNA_ZONE_SURFACE_2_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x39,
    NEO_ARK_SAVANNA_ZONE_SURFACE_2_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x3B,
    NEO_ARK_SAVANNA_ZONE_SURFACE_3_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x31,
    NEO_ARK_SAVANNA_ZONE_SURFACE_3_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x33,
};

/// Surface class 2's walking, running and scripted-jump cue-2 sound bases.
///
/// Cue 1 adds one; companion cues add 100. Playback borrows the loaded type-1
/// sound bank, and scripted jumps share walking's pair. Retain the data placement.
static const WorldCollisionFootstepSounds _gNeoArkSavannaZoneSurface2FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_SAVANNA_ZONE_SURFACE_2_FOOTSTEP_WALK,
    .run          = NEO_ARK_SAVANNA_ZONE_SURFACE_2_FOOTSTEP_RUN,
    .scriptedJump = NEO_ARK_SAVANNA_ZONE_SURFACE_2_FOOTSTEP_WALK,
};

/// Surface class 3's walking, running and scripted-jump cue-2 sound bases.
///
/// Cue 1 adds one; companion cues add 100. Playback borrows the loaded type-1
/// sound bank, and scripted jumps share walking's pair. Retain the data placement.
static const WorldCollisionFootstepSounds _gNeoArkSavannaZoneSurface3FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_SAVANNA_ZONE_SURFACE_3_FOOTSTEP_WALK,
    .run          = NEO_ARK_SAVANNA_ZONE_SURFACE_3_FOOTSTEP_RUN,
    .scriptedJump = NEO_ARK_SAVANNA_ZONE_SURFACE_3_FOOTSTEP_WALK,
};

/// Classes 0 and 4..7: probe blocking, weapon impacts and pushback, without footsteps.
static WorldCollisionSurfaceProperties _gNeoArkSavannaZoneSolidSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1: probe blocking and pushback, with weapon impacts and footsteps disabled.
static WorldCollisionSurfaceProperties _gNeoArkSavannaZoneNoWeaponImpactSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 2: probe blocking, weapon impacts and pushback, with its own footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSavannaZoneSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkSavannaZoneSurface2FootstepSounds,
};

/// Class 3: probe blocking, weapon impacts and pushback, with its own footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSavannaZoneSurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkSavannaZoneSurface3FootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkSavannaZoneSurfaceProperties[NEO_ARK_SAVANNA_ZONE_SURFACE_CLASS_COUNT] = {
    &_gNeoArkSavannaZoneSolidSurface,
    &_gNeoArkSavannaZoneNoWeaponImpactSurface,
    &_gNeoArkSavannaZoneSurface2Properties,
    &_gNeoArkSavannaZoneSurface3Properties,
    &_gNeoArkSavannaZoneSolidSurface,
    &_gNeoArkSavannaZoneSolidSurface,
    &_gNeoArkSavannaZoneSolidSurface,
    &_gNeoArkSavannaZoneSolidSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest savanna event-start call requested a staged task.
///
/// Every call clears this byte to 0; the execute branch sets 1 after requesting
/// a spawn, even if allocation fails. Queries and already-seen events leave 0.
/// The staged task never clears it, and no current consumer reads it.
static s8 _gNeoArkSavannaZoneEventStartRequested = 0;

RoomLatchedEvent gRoomEventLatched = { 0 };

static void _neoArkSavannaZoneInitializeRoom(Task* task);
static void _neoArkSavannaZoneSetupIdleState(Task* task);

#include "../../shared/room/room_event_staged_task.inc.c"

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static inline s32 _neoArkSavannaZoneStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    _gNeoArkSavannaZoneEventStartRequested = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *transition;
            gRoomEventLatched             = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&_gNeoArkSavannaZoneStagedEventTaskDesc, 0, 0, 0);
            _gNeoArkSavannaZoneEventStartRequested = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Resolves savanna exits and stages each destination's first departure event.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed eight-byte request and
/// writable reply storage, which may alias. Resolves the copied destination
/// before checking South Promenade or Shrine: each has its own CAP command,
/// sound and seen flag, with no extra fade. Returns 2 while that event is
/// eligible (including queries), or 1 if already seen or for any other exit.
/// Only execute mode latches the records and spawns the event; the room and
/// map overlays and playback resources must remain loaded until it finishes.
static s32 _neoArkSavannaZoneResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_SAVANNA_ZONE_SOUTH_PROMENADE_COMMAND    = 3,
           NEO_ARK_SAVANNA_ZONE_SHRINE_COMMAND             = 2,
           NEO_ARK_SAVANNA_ZONE_SOUTH_PROMENADE_SOUND      = 0x55120003,
           NEO_ARK_SAVANNA_ZONE_SHRINE_SOUND               = 0x55120001,
           NEO_ARK_SAVANNA_ZONE_SOUTH_PROMENADE_EVENT_FLAG = 0x15E,
           NEO_ARK_SAVANNA_ZONE_SHRINE_EVENT_FLAG          = 0x15F,
           NEO_ARK_SAVANNA_ZONE_DEPARTURE_NO_FADE          = 0,
           NEO_ARK_SAVANNA_ZONE_EXIT_ALLOWED               = 1 };

    RoomLatchedEvent event;
    s32              capCommandIndex;
    s32              soundId;
    s16              eventFlagId;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_NEO_ARK_SOUTH_PROMENADE) {
        goto resolveShrineEvent;
    }
    soundId         = NEO_ARK_SAVANNA_ZONE_SOUTH_PROMENADE_SOUND;
    capCommandIndex = NEO_ARK_SAVANNA_ZONE_SOUTH_PROMENADE_COMMAND;
    event.stageSnd  = soundId;
    eventFlagId     = NEO_ARK_SAVANNA_ZONE_SOUTH_PROMENADE_EVENT_FLAG;
startDepartureEvent:
    event.capCmd = capCommandIndex;
    event.flagId = eventFlagId;
    event.fade   = NEO_ARK_SAVANNA_ZONE_DEPARTURE_NO_FADE;
    return _neoArkSavannaZoneStartEvent(reply, &event);
resolveShrineEvent:
    if (request->areaId == GAME_AREA_NEO_ARK_SHRINE) {
        soundId         = NEO_ARK_SAVANNA_ZONE_SHRINE_SOUND;
        capCommandIndex = NEO_ARK_SAVANNA_ZONE_SHRINE_COMMAND;
        event.stageSnd  = soundId;
        eventFlagId     = NEO_ARK_SAVANNA_ZONE_SHRINE_EVENT_FLAG;
        goto startDepartureEvent;
    }
    return NEO_ARK_SAVANNA_ZONE_EXIT_ALLOWED;
}

/// Refuses every collected key-item use with zero, leaving the item unused.
static s32 _neoArkSavannaZoneRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Ignores CAP room commands and returns zero without retaining either argument.
static s32 _neoArkSavannaZoneIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores direction actions and returns zero without reading the borrowed request.
static s32 _neoArkSavannaZoneIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs the savanna message receiver and advances to its idle state.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _neoArkSavannaZoneInitializeRoom(Task* task)
{
    task->msgTable = _gNeoArkSavannaZoneRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room's message task idle in state 1 until an external state change or teardown.
static void _neoArkSavannaZoneSetupIdleState(Task* task)
{
}

/// Savanna room-controller handlers in `Task::state` order.
///
/// States 0, 1 and 2 install the message receiver, idle and release the task.
/// The dispatcher copies all three pointers by value before indexing; valid
/// states are 0..2, with no sentinel or bounds check. Keep callback code loaded.
static const TaskFuncTable3 _gNeoArkSavannaZoneRoomStateHandlers = { {
    _neoArkSavannaZoneInitializeRoom,
    _neoArkSavannaZoneSetupIdleState,
    taskKill,
} };

void neoArkSavannaZoneRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkSavannaZoneRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

void neoArkSavannaZoneConfigureEffectsTask(Task* task)
{
    enum { CONFIGURE_EFFECTS_INITIALIZE,
           CONFIGURE_EFFECTS_IDLE };

    if (task->state == CONFIGURE_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId               = EFFECT_NEO_ARK_SAVANNA_ZONE_FLASH;
        gRoomEffectTwinTrailId           = EFFECT_NEO_ARK_SAVANNA_ZONE_TWIN_TRAIL;
        gRoomEffectSparkBurstId          = EFFECT_NEO_ARK_SAVANNA_ZONE_SPARK_BURST;
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        task->state                      = CONFIGURE_EFFECTS_IDLE;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkSavannaZoneRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkSavannaZoneRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkSavannaZoneRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
