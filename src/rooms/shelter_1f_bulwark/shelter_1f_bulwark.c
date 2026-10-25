#include "rooms/shelter_1f_bulwark.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_02300.h"
#include "actors/actor_05700.h"

#include "gameplay/companion_load.h"
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
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
// This room's fade symbol is the ScreenFade itself, with no following word.
#define ROOM_EVENT_FADE gRoomEventFade
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/streamed_scene.h"

extern ScreenFade       gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void _shelter1fBulwarkInitializeRoom(Task* task);
static void _shelter1fBulwarkIdle(Task* task);

static WorldCollisionGrid     _gShelter1fBulwarkCollisionGrid;
static WorldCollisionOccluder _gShelter1fBulwarkOccluders[2];
static WorldCollisionTrigger  _gShelter1fBulwarkViewBoundaries[2];
static WorldCollisionTrigger  _gShelter1fBulwarkActionTriggers[8];
static WorldCoordRoomLights   _gShelter1fBulwarkRoomLights;

static s32  _shelter1fBulwarkResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelter1fBulwarkRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _shelter1fBulwarkIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 mode);
static s32  _shelter1fBulwarkIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 unused);
static void _shelter1fBulwarkHeliportDepartureTask(Task* task);
static void _shelter1fBulwarkPlayDepartureMovieTask(Task* movieTask);
static void _shelter1fBulwarkReloadAfterDepartureMovieTask(Task* task);

/// Bodyless recipe for the latched CAP/sound/fade transition and saved-location reload.
///
/// Spawn entry 0 with zero payloads after filling the room's staged message and
/// latched event. The room globals and callback must remain live through reload.
static TaskDesc _gShelter1fBulwarkStagedEventTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 }
};

/// Bulwark handlers for exit events, key-item refusal and ignored actions/commands.
///
/// Four handlers and a `TASK_MESSAGE_TABLE_END` record; the room task borrows
/// this read-only table and its callbacks while the overlay remains loaded.
static const TaskMessageEntry _gShelter1fBulwarkRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fBulwarkResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fBulwarkRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fBulwarkIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fBulwarkIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless first-heliport-departure recipe, priority `TASK_PRIORITY_SCENE` and zero metadata/payloads.
///
/// Spawn entry 0 after accepting the chapter-6 departure and holding the player.
/// One descriptor with no terminator. The callback hands CAP/fade playback to
/// the movie coordinator; keep the room loaded until the final session reload.
static TaskDesc _gShelter1fBulwarkHeliportDepartureTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = _shelter1fBulwarkHeliportDepartureTask,
    .data     = { .value = 0 },
};

/// Bodyless recipes for the departure coordinator (0) and its display movie task (1).
///
/// Both start with zero payloads. The coordinator hands presentation to entry 1,
/// then reloads at Neo Ark area 26 after playback and game-resource restoration.
/// There is no terminator; keep this room's callback code loaded while they run.
static TaskDesc _gShelter1fBulwarkDepartureMovieTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelter1fBulwarkReloadAfterDepartureMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _shelter1fBulwarkPlayDepartureMovieTask, { .value = 0 } },
};

/// Fixed world-space glow centres for mapped view 2, in world-coordinate units.
///
/// Entries 0..1 use red/green factors 2:1; entries 2..3 use equal RGB factors.
/// Read-only positions retain their original data-section placement.
static const SVECTOR _gShelter1fBulwarkView2GlowPoints[4] SECTION(".data") = {
    { 3560, -6380, 2770, 0 },
    { 3560, -5130, -2810, 0 },
    { 2910, -4050, 1890, 0 },
    { 2910, -4050, -1930, 0 },
};

/// Fixed world-space red glow centre for mapped view 3, in world-coordinate units.
static const SVECTOR _gShelter1fBulwarkView3GlowPoint SECTION(".data") = { -4180, -1940, 2720, 0 };

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gShelter1fBulwarkCollisionResources[1] = {
    { &_gShelter1fBulwarkCollisionGrid, _gShelter1fBulwarkViewBoundaries, _gShelter1fBulwarkActionTriggers, _gShelter1fBulwarkOccluders },
};

WorldCoordRoomLighting gShelter1fBulwarkRoomLighting[1] = {
    { &_gShelter1fBulwarkRoomLights, NULL },
};

u8* gShelter1fBulwarkRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelter1fBulwarkRoomViewCounts[1] = { ARRAY_SIZE(gShelter1fBulwarkCameras) };

/// Map-marker nibble shared by the two sides of the vehicular-airlock exit.
enum { SHELTER_1F_BULWARK_AIRLOCK_MAP_FLAG_ID = 0x1AC };

DirectionWarpEntry gShelter1fBulwarkWarpEntries[2] = {
    { { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 4000, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 3130, 0, 0 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 2), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, SHELTER_1F_BULWARK_AIRLOCK_MAP_FLAG_ID },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3800, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 3 * ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -3800, 0, 0 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 4), SOUND_SHELTER_1F_BULWARK_TO_HELIPORT, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-space unit normals of the Bulwark collision grid, with ONE for 1.0.
static SVECTOR _gShelter1fBulwarkCollisionGridNormals[6] = {
#include "assets/shelter_1f_bulwark_collision_03088_normals.inc"
};

/// Room-space collision-grid vertices, in signed world-coordinate units.
static SVECTOR _gShelter1fBulwarkCollisionGridVerts[22] = {
#include "assets/shelter_1f_bulwark_collision_03088_verts.inc"
};

/// Indexed collision faces; class 1 is the floor and class 0 has no footstep cues.
static WorldCollisionGridFace _gShelter1fBulwarkCollisionGridFaces[12] = {
#include "assets/shelter_1f_bulwark_collision_03088_faces.inc"
};

/// Serialized signed face-index lists for six cells, each ending with -1.
static s16 _gShelter1fBulwarkCollisionGridCells[64] = {
#include "assets/shelter_1f_bulwark_collision_03088_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fBulwarkCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..63,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_BULWARK_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fBulwarkCollisionGridCells[(halfwordOffset)])
/// X-major 3-by-2 cell directory borrowing the grid's face-index lists.
static s16* _gShelter1fBulwarkCollisionGridTable[6] = {
#include "assets/shelter_1f_bulwark_collision_03088_table.inc"
};
#undef SHELTER_1F_BULWARK_CELL_FACE_LIST_AT_OFFSET

/// Bulwark collision mesh with six 4000-unit XZ cells and twelve quad faces.
///
/// Grid origin is (-4250, -3500) in room X/Z. Setup binds the mutable descriptor
/// to the current view; its pools and cell lists stay live while the grid is active.
static WorldCollisionGrid _gShelter1fBulwarkCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelter1fBulwarkCollisionGridNormals,
    .vertices    = _gShelter1fBulwarkCollisionGridVerts,
    .faces       = _gShelter1fBulwarkCollisionGridFaces,
    .cellFaceIds = _gShelter1fBulwarkCollisionGridTable,
    .xBias       = 4250,
    .zBias       = 3500,
    .cellCountX  = 3,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelter1fBulwarkCollisionGridFaces),
};

ViewCamera gShelter1fBulwarkCameras[3] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0x38D6, 0 } }, 257 },
    { { { { 888, 0, -3998 }, { 1332, 3861, 296 }, { 3769, -1364, 837 } }, { 2810, 260, 870 } }, 257 },
    { { { { 902, 0, 3995 }, { 3827, 1174, -864 }, { -1145, 3924, 258 } }, { 1110, 5890, 370 } }, 207 },
};

/// Empty sprite list for mapped view 1, retaining decoded background strips.
static SpriteBatch _gShelter1fBulwarkView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 2, retaining decoded background strips.
static SpriteBatch _gShelter1fBulwarkView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Neutral colour modulation copied into directly emitted sprite packets.
enum { SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB = 128 };

/// Eight opaque sprite rectangles for mapped view 3, in view-centred pixels.
///
/// Texture coordinates are texels; sorting depths are unscaled OT values.
/// Sources remain writable for the sprite system while this overlay is loaded.
static SpriteSource _gShelter1fBulwarkView3SpriteSources[8] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -80, 32, 1225, { .fields = { 120, 152 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -104, 40, 1225, { .fields = { 72, 144 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 16 } }, -120, 48, 1225, { .fields = { 64, 112 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -56, 48, 1400, { .fields = { 112, 128 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -56, 64, 1400, { .fields = { 112, 32 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -48, 96, 1425, { .fields = { 112, 88 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 56, 32 } }, -112, 64, 1225, { .fields = { 72, 0 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 64, 24 } }, -112, 96, 1425, { .fields = { 64, 64 } }, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, SHELTER_1F_BULWARK_SPRITE_NEUTRAL_RGB, 0 },
};

/// View 3's eight-sprite range after an empty batch retaining background strips.
static SpriteBatch _gShelter1fBulwarkView3SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gShelter1fBulwarkView3SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelter1fBulwarkSpriteViews[3] = {
    { { .empty = _gShelter1fBulwarkView1SpriteBatches }, _gShelter1fBulwarkView1SpriteBatches, NULL },
    { { .empty = _gShelter1fBulwarkView2SpriteBatches }, _gShelter1fBulwarkView2SpriteBatches, NULL },
    { { .elements = _gShelter1fBulwarkView3SpriteSources }, _gShelter1fBulwarkView3SpriteBatches, NULL },
};

/// Seven authored point lights contributing in every view of the Bulwark.
///
/// Positions and falloff radii are world units, RGB uses ONE for 1.0. Setup
/// parents their transforms to the view; composition and shading mutate caches.
static WorldCoordPointLight _gShelter1fBulwarkPointLights[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3994, -1930, 2718 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 0, 0 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2500, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 2500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2500, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 2500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -3000, -1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2000, -3000, 1500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2293, 2048 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -6000, -2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 1638, 1228 }, { 0, 0 } }, 6000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -6000, 2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 1638, 1228 }, { 0, 0 } }, 6000, 7000 },
};

/// Room light collection containing seven point lights and no directional or cone lights.
static WorldCoordRoomLights _gShelter1fBulwarkRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelter1fBulwarkPointLights),
    .pointLights           = _gShelter1fBulwarkPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed view boundaries between logical views 2 and 3, ending with LAST.
///
/// Room setup binds and links both mutable quads. Positions and radii are world
/// units, normals use ONE for 1.0; runtime links and hit latches start clear.
static WorldCollisionTrigger _gShelter1fBulwarkViewBoundaries[2] = {
    { NULL, NULL, NULL, { -620, -3248, 96, 0 }, { { -313, -3712, -2468, 0 }, { 310, -3712, 2465, 0 }, { -313, 3712, -2468, 0 }, { 310, 3712, 2465, 0 } }, { 4075, 0, -516, 0 }, { 0, 0, ONE, 0 }, 4463, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -478, -3312, 191, 0 }, { { 291, -3712, 2444, 0 }, { -302, -3712, -2455, 0 }, { 291, 3712, 2444, 0 }, { -302, 3712, -2455, 0 } }, { -4074, 0, 492, 0 }, { 0, 0, ONE, 0 }, 4434, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed departure/arrival slots for the room's two warp actions.
enum {
    SHELTER_1F_BULWARK_WARP_TO_AIRLOCK  = (1 << 4) | 3,
    SHELTER_1F_BULWARK_WARP_TO_HELIPORT = (2 << 4) | 1,
};

/// Eight facing-gated action quads: two exits and six CAP interactions.
///
/// Warps use Bulwark endpoint 1 to airlock arrival 3 and endpoint 2 to heliport
/// arrival 1. CAP entries select commands 2..5 with zero presentation flags.
/// Setup binds and links the mutable records; the final included quad carries LAST.
static WorldCollisionTrigger _gShelter1fBulwarkActionTriggers[8] = {
    { NULL, NULL, NULL, { 3696, -53, 0, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK, SHELTER_1F_BULWARK_WARP_TO_AIRLOCK, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3776, -53, 112, 0 }, { { -496, 0, -1776, 0 }, { 496, 0, -1776, 0 }, { -496, 0, 1776, 0 }, { 496, 0, 1776, 0 } }, { 0, 4101, 0, 0 }, { ONE, 0, 0, 0 }, 1841, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, -64, -1200, 0 }, { { -3552, 0, -368, 0 }, { 3552, 0, -368, 0 }, { -3552, 0, 368, 0 }, { 3552, 0, 368, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, ONE, 0 }, 3565, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -304, -64, 992, 0 }, { { -1744, 0, -368, 0 }, { 1744, 0, -368, 0 }, { -1744, 0, 368, 0 }, { 1744, 0, 368, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, -ONE, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1600, -64, 2144, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2656, -64, -2176, 0 }, { { -496, 0, -1408, 0 }, { 496, 0, -1408, 0 }, { -496, 0, 1408, 0 }, { 496, 0, 1408, 0 } }, { 0, 4105, 0, 0 }, { -ONE, 0, 0, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3808, -64, -2704, 0 }, { { -496, 0, -496, 0 }, { 496, 0, -496, 0 }, { -496, 0, 496, 0 }, { 496, 0, 496, 0 } }, { 0, 4095, 0, 0 }, { ONE, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_HELIPORT, SHELTER_1F_BULWARK_WARP_TO_HELIPORT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, 1024, 0 }, { { -1328, 0, -368, 0 }, { 1328, 0, -368, 0 }, { -1328, 0, 368, 0 }, { 1328, 0, 368, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -ONE, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Default variant's two Rook Golem resource recipes, followed by AREA_PLACEMENT_END.
///
/// Placement IDs 23 and 57 use actor packages 102300 and 205700 respectively,
/// selecting task entry 0. The loader borrows these recipes and their task tables.
static AreaResource _gShelter1fBulwarkVariant1Resources[3] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102300RookGolemTasks },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor05700RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gShelter1fBulwarkAreaVariants[SHELTER_1F_BULWARK_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkBulwarkDefaultPlacements, .resources = _gShelter1fBulwarkVariant1Resources },
};

/// Two room-space quads blocking sight, ending with LAST on the second record.
///
/// Setup links and enables both records; keep them loaded until unlinked.
/// The retained low flag bit and final byte have no established interpretation.
static WorldCollisionOccluder _gShelter1fBulwarkOccluders[2] = {
    { NULL, NULL, { 1840, -16, 2576, 0 }, { { -2864, 1936, -1104, 0 }, { 2864, 1936, 1104, 0 }, { -2864, -1936, -1104, 0 }, { 2864, -1936, 1104, 0 } }, { -1477, 0, 3827, 0 }, 3620, 1, 0 },
    { NULL, NULL, { 2624, 0, -3120, 0 }, { { -3216, 1936, 896, 0 }, { 3216, 1936, -896, 0 }, { -3216, -1936, 896, 0 }, { 3216, -1936, -896, 0 } }, { 1102, 0, 3957, 0 }, 3857, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Loaded type-1-bank cue bases for walking/scripted jumps and running on the floor.
enum {
    SHELTER_1F_BULWARK_FLOOR_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x41,
    SHELTER_1F_BULWARK_FLOOR_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x43,
};

/// Floor footstep pairs, with scripted jumps sharing the walking pair.
///
/// Cue 2 uses the base, cue 1 base+1; companions add 100. The surface borrows
/// this immutable record while the room is loaded; retain its data-section position.
static const WorldCollisionFootstepSounds _gShelter1fBulwarkFloorFootstepSounds SECTION(".data") = {
    .walk         = SHELTER_1F_BULWARK_FLOOR_WALK_CUE_BASE,
    .run          = SHELTER_1F_BULWARK_FLOOR_RUN_CUE_BASE,
    .scriptedJump = SHELTER_1F_BULWARK_FLOOR_WALK_CUE_BASE,
};

/// Blocking surface policy without footstep cues, shared by classes 0 and 2..7.
///
/// field_0 is retained as zero; its role is unproven.
static WorldCollisionSurfaceProperties _gShelter1fBulwarkSilentSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking floor policy for class 1, with paired walking and running footstep cues.
///
/// field_0 is retained as zero; its role is unproven.
static WorldCollisionSurfaceProperties _gShelter1fBulwarkFloorSurface = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelter1fBulwarkFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelter1fBulwarkSurfaceProperties[8] = {
    &_gShelter1fBulwarkSilentSurface,
    &_gShelter1fBulwarkFloorSurface,
    &_gShelter1fBulwarkSilentSurface,
    &_gShelter1fBulwarkSilentSurface,
    &_gShelter1fBulwarkSilentSurface,
    &_gShelter1fBulwarkSilentSurface,
    &_gShelter1fBulwarkSilentSurface,
    &_gShelter1fBulwarkSilentSurface,
};

ScreenFade gRoomEventFade = { 0 };

/// Mutable 30-frame departure blackout lent to the fade task before the heliport movie.
///
/// This is separate from the ordinary staged-event fade. Keep the room overlay
/// loaded and the record intact until session reload discards the old task list.
static ScreenFade _gShelter1fBulwarkHeliportFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest staged-event gate call attempted an event spawn (0 no, 1 yes).
///
/// Every call clears this byte, including queries. Execution raises it after
/// the unchecked spawn; the task does not clear it on completion. No reader
/// is recovered, so this records the latest call rather than a running lifetime.
static s8 _gShelter1fBulwarkStagedEventStarted = 0;

/// Retained, unreferenced bytes after the event-start byte; their role and grouping are unproven.
static u8 D_shelter_1f_bulwark_80180ECD = 33;

static u8 D_shelter_1f_bulwark_80180ECE = 178;

static u8 D_shelter_1f_bulwark_80180ECF = 137;

RoomLatchedEvent gRoomEventLatched;

#include "../../shared/room/room_event_staged_task.inc.c"

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static inline s32 _shelter1fBulwarkStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    _gShelter1fBulwarkStagedEventStarted = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *transition;
            gRoomEventLatched             = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&_gShelter1fBulwarkStagedEventTaskDesc, 0, 0, 0);
            _gShelter1fBulwarkStagedEventStarted = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

/// Resolves Bulwark exits and starts their chapter or door scenes.
///
/// Borrows eight-byte request/reply records, which may alias, and copies the
/// whole request before resolving the room. Returns 0 for the first heliport
/// chapter departure, 1 for an ordinary exit, or 2 for a room-managed scene.
/// A blocked heliport starts its CAP prompt even on a query; other scene
/// starts require execute mode and retain copies through the deferred tasks.
static s32 _shelter1fBulwarkResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_1F_BULWARK_HELIPORT_CHAPTER      = 6,
        SHELTER_1F_BULWARK_CAP_HELIPORT          = 1,
        SHELTER_1F_BULWARK_CAP_VEHICULAR_AIRLOCK = 6,
        SHELTER_1F_BULWARK_SOUND_TO_AIRLOCK      = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_BULWARK, 1),
        SHELTER_1F_BULWARK_FADE_REQUIRED         = 1,
        SHELTER_1F_BULWARK_NO_FADE               = 0,
        SHELTER_1F_BULWARK_NO_EVENT_FLAG         = 0,
    };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_1F_HELIPORT) {
        if (gameFlagGetNibble(GAME_FLAG_BULWARK_HELIPORT_UNBLOCKED) == 0) {
            capSpawnEventIfIdle(SHELTER_1F_BULWARK_CAP_HELIPORT, CAP_EVENT_NO_FLAGS);
            return ROOM_VARIANT_TRANSITION_HANDLED;
        }
        if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) < SHELTER_1F_BULWARK_HELIPORT_CHAPTER) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(EVENT_FLAG_STORY_CHAPTER, SHELTER_1F_BULWARK_HELIPORT_CHAPTER);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(&_gShelter1fBulwarkHeliportDepartureTaskDesc, 0, 0, 0);
            }
            return ROOM_VARIANT_TRANSITION_REFUSED;
        }
        event.capCmd   = SHELTER_1F_BULWARK_CAP_HELIPORT;
        event.stageSnd = SOUND_SHELTER_1F_BULWARK_TO_HELIPORT;
        event.flagId   = SHELTER_1F_BULWARK_NO_EVENT_FLAG;
        event.fade     = SHELTER_1F_BULWARK_FADE_REQUIRED;
        return _shelter1fBulwarkStartEvent(reply, &event);
    }
    if (request->areaId == GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK) {
        event.capCmd   = SHELTER_1F_BULWARK_CAP_VEHICULAR_AIRLOCK;
        event.stageSnd = SHELTER_1F_BULWARK_SOUND_TO_AIRLOCK;
        event.flagId   = GAME_FLAG_BULWARK_TO_VEHICULAR_AIRLOCK_SCENE;
        event.fade     = SHELTER_1F_BULWARK_NO_FADE;
        return _shelter1fBulwarkStartEvent(reply, &event);
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Bulwark room states: 0 initialize messages, 1 idle, 2 release.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check; keep the room loaded through dispatch.
static const TaskFuncTable3 _gShelter1fBulwarkRoomStateHandlers = {
    {
        _shelter1fBulwarkInitializeRoom,
        _shelter1fBulwarkIdle,
        taskKill,
    },
};

/// Starts the heliport departure's 30-frame fade and sound, resetting elapsed ticks.
///
/// The room-owned fade must remain live through the spawned fade task.
static inline void _shelter1fBulwarkStartHeliportFade(Task* task)
{
    enum {
        SHELTER_1F_BULWARK_FADE_FRAMES    = 30,
        SHELTER_1F_BULWARK_FADE_TASK_BANK = 1,
        SHELTER_1F_BULWARK_FADE_TASK_SLOT = 0x31,
    };

    _gShelter1fBulwarkHeliportFade.blend      = SCREEN_FADE_SUBTRACT;
    _gShelter1fBulwarkHeliportFade.phase      = SCREEN_FADE_RUNNING;
    _gShelter1fBulwarkHeliportFade.rampFrames = SHELTER_1F_BULWARK_FADE_FRAMES;
    taskSpawn(SHELTER_1F_BULWARK_FADE_TASK_BANK, SHELTER_1F_BULWARK_FADE_TASK_SLOT, 0, &_gShelter1fBulwarkHeliportFade);
    task->killCountdown = 0;
    sndEvtRequestScriptStart(SOUND_SHELTER_1F_BULWARK_TO_HELIPORT, 0, 0);
}

/// Runs the first heliport departure's CAP scene, fade and movie handoff.
///
/// Start bodyless in state 0 after taking player control. Waits for CAP,
/// queues battle escape, fades for 30 ticks and waits 31 callbacks, then
/// commits chapter 6 and spawns the movie/reload coordinator. States 4 and 6
/// are one-tick delays; state 7 destroys this task. Room resources must remain
/// live throughout; the task's signed countdown is reused as elapsed ticks.
static void _shelter1fBulwarkHeliportDepartureTask(Task* task)
{
    enum {
        SHELTER_1F_BULWARK_DEPARTURE_START_CAP         = 0,
        SHELTER_1F_BULWARK_DEPARTURE_WAIT_CAP          = 1,
        SHELTER_1F_BULWARK_DEPARTURE_START_FADE        = 2,
        SHELTER_1F_BULWARK_DEPARTURE_WAIT_FADE         = 3,
        SHELTER_1F_BULWARK_DEPARTURE_BEFORE_MOVIE      = 4,
        SHELTER_1F_BULWARK_DEPARTURE_START_MOVIE       = 5,
        SHELTER_1F_BULWARK_DEPARTURE_AFTER_MOVIE_START = 6,
        SHELTER_1F_BULWARK_DEPARTURE_RELEASE           = 7,
        SHELTER_1F_BULWARK_DEPARTURE_CAP               = 1,
        SHELTER_1F_BULWARK_CAP_KEEP_ACTORS_PAUSED      = 1,
        SHELTER_1F_BULWARK_DEPARTURE_CHAPTER           = 6,
        SHELTER_1F_BULWARK_FADE_WAIT_TICKS             = 31,
    };

    switch (task->state) {
        case SHELTER_1F_BULWARK_DEPARTURE_START_CAP:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(SHELTER_1F_BULWARK_DEPARTURE_CAP, CAP_PLAYBACK_IN_PLACE);
            gCapPreserveActorControl = SHELTER_1F_BULWARK_CAP_KEEP_ACTORS_PAUSED;
            task->state++;
            break;
        case SHELTER_1F_BULWARK_DEPARTURE_WAIT_CAP:
            if (capIsBusy() != 0) {
                break;
            }
            sceneQueueBattleEscapeResult();
            task->state++;
            break;
        case SHELTER_1F_BULWARK_DEPARTURE_START_FADE:
            _shelter1fBulwarkStartHeliportFade(task);
            task->state++;
            break;
        case SHELTER_1F_BULWARK_DEPARTURE_WAIT_FADE:
            task->killCountdown++;
            if (task->killCountdown < SHELTER_1F_BULWARK_FADE_WAIT_TICKS) {
                break;
            }
            task->state++;
            break;
        case SHELTER_1F_BULWARK_DEPARTURE_START_MOVIE:
            // The coordinator owns the movie and the eventual saved-location reload.
            gameFlagSetNibble(EVENT_FLAG_STORY_CHAPTER, SHELTER_1F_BULWARK_DEPARTURE_CHAPTER);
            taskSpawnFromTable(_gShelter1fBulwarkDepartureMovieTaskDescs, 0, 0, 0);
            /* fallthrough */
        case SHELTER_1F_BULWARK_DEPARTURE_BEFORE_MOVIE:
        case SHELTER_1F_BULWARK_DEPARTURE_AFTER_MOVIE_START:
            task->state++;
            break;
        case SHELTER_1F_BULWARK_DEPARTURE_RELEASE:
            taskKill(task);
            break;
    }
}

/// Rejects every key-item use request in the Bulwark.
///
/// Returns zero so the inventory reports that the selected item cannot be used
/// here. Neither payload is consumed and no room state changes.
static s32 _shelter1fBulwarkRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return 0;
}

/// Ignores CAP room commands and returns zero without changing room state.
static s32 _shelter1fBulwarkIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 mode)
{
    return 0;
}

/// Ignores room action requests and returns zero without changing room state.
///
/// The request is borrowed for synchronous dispatch and is neither read nor
/// retained; the second payload word is unused.
static s32 _shelter1fBulwarkIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* actionRequest, s32 unused)
{
    return 0;
}

/// Installs the Bulwark message receiver and advances to its idle state.
///
/// Called in state 0 with a live room task and initialized gameplay resources.
/// Registers the borrowed task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _shelter1fBulwarkInitializeRoom(Task* task)
{
    task->msgTable = _gShelter1fBulwarkRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room controller idle while its message handlers remain installed.
static void _shelter1fBulwarkIdle(Task* task)
{
}

void shelter1fBulwarkRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelter1fBulwarkRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Plays the Bulwark departure movie and resumes game presentation after restoration.
///
/// Requires a live display task in states 0..5, a loaded stream-100 slot for
/// the current room and valid session and image-memory resources. Start cancels
/// playback; either path waits for CD idle before restoring model buffers and
/// saved VRAM images. Sprite images are not reloaded. No task work is allocated.
static void _shelter1fBulwarkPlayDepartureMovieTask(Task* movieTask)
{
    u8              commandArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue;

    queue = &gCdCmdQueue;
    switch (movieTask->state) {
        case STREAMED_SCENE_PREPARE:
            SetDispMask(false);
            streamPrepareMovieWorkspace(true);
            movieTask->state++;
            break;
        case STREAMED_SCENE_QUEUE_MOVIE:
            movieLocation      = gGameSession->location;
            movieLocation.view = STREAMED_SCENE_MOVIE_ID;
            // The four-byte command block carries the loaded movie slot in byte zero.
            commandArgs[0] = streamFindMovieSlot(&movieLocation, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            movieTask->state++;
            break;
        case STREAMED_SCENE_WAIT_READY:
            if (queue->movieReady == 0) {
                break;
            }
            SetDispMask(true);
            movieTask->state++;
            break;
        case STREAMED_SCENE_PLAYING:
            if (cdCmdIsIdle()) {
                SetDispMask(false);
                movieTask->state++;
                break;
            }
            if (padIsStartPressed() == 0) {
                break;
            }
            SetDispMask(false);
            cdCmdRequestCancel();
            movieTask->state++;
            break;
        // Cancellation must drain before the movie workspace returns to game use.
        case STREAMED_SCENE_WAIT_IDLE:
            if (cdCmdIsIdle() == 0) {
                break;
            }
            streamResetGameRestore();
            movieTask->state++;
            break;
        case STREAMED_SCENE_RESTORE_GAME:
            if (streamPollGameRestore(false, false) == 0) {
                break;
            }
            taskKill(movieTask);
            displayResumeGameLoop();
            break;
    }
}

/// Transfers presentation to the departure movie, then reloads at Neo Ark area 26.
///
/// Starts bodyless in state 0. Display-only flipping suspends ordinary gameplay
/// while the movie display task runs; after gameplay resumes, states 1 and 2
/// delay the load request until state 3. Commits stage 5, area 26, warp/room 1
/// to the live save, selects sprite variant 1 and queues a reload without battle
/// escape handling. Requires the room, display and live-save resources throughout.
static void _shelter1fBulwarkReloadAfterDepartureMovieTask(Task* task)
{
    enum {
        DEPARTURE_START_MOVIE,
        DEPARTURE_RESUME_DELAY_FIRST,
        DEPARTURE_RESUME_DELAY_SECOND,
        DEPARTURE_RELOAD,
        DEPARTURE_MOVIE_TASK_INDEX = 1,
        DEPARTURE_ARRIVAL_WARP     = 1,
        DEPARTURE_ARRIVAL_ROOM     = 1,
        DEPARTURE_SPRITE_VARIANT   = 1
    };
    switch (task->state) {
        case DEPARTURE_START_MOVIE:
            // Give the movie sole presentation ownership until its restore completes.
            displaySpawnTaskFromTable(_gShelter1fBulwarkDepartureMovieTaskDescs, DEPARTURE_MOVIE_TASK_INDEX, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            viewQueueCurrentCameraAndPackets();
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            /* fallthrough */
        case DEPARTURE_RESUME_DELAY_FIRST:
        case DEPARTURE_RESUME_DELAY_SECOND:
            task->state = task->state + 1;
            break;
        case DEPARTURE_RELOAD:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_SHELTER_NEO_ARK;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_NEO_ARK_R26;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = DEPARTURE_ARRIVAL_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = DEPARTURE_ARRIVAL_ROOM;
            gDisplayState.spriteVariant                                 = DEPARTURE_SPRITE_VARIANT;
            gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(task);
            break;
    }
}

#include "../../shared/effects/glow_draw_factor_disc.inc.c"

void shelter1fBulwarkDrawGlowsTask(Task* task)
{
    enum {
        GLOWS_INITIALIZE,
        GLOWS_DRAW,
        GLOW_VIEW_2_RADIUS = 0x300, // World-unit radii before perspective projection
        GLOW_VIEW_3_RADIUS = 0x200,
        // Packed intensity factors: red bits 8..15, green 4..5, blue 0..1.
        GLOW_RED_GREEN_FACTORS = (2 << 8) | (1 << 4),
        GLOW_EQUAL_RGB_FACTORS = (1 << 8) | (1 << 4) | 1,
        GLOW_RED_FACTOR        = 2 << 8
    };
    u8 mappedViewIndex;

    // Publish this room's combat-effect entries before drawing its fixed glows.
    if (task->state == GLOWS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_SHELTER_1F_BULWARK_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_BULWARK_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_BULWARK_SPARK_BURST;
        task->state             = GLOWS_DRAW;
    }

    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 2: {
            const SVECTOR* glowPoints = _gShelter1fBulwarkView2GlowPoints;
            _glowDrawFactorDisc(&glowPoints[0], GLOW_VIEW_2_RADIUS, GLOW_RED_GREEN_FACTORS);
            _glowDrawFactorDisc(&glowPoints[1], GLOW_VIEW_2_RADIUS, GLOW_RED_GREEN_FACTORS);
            _glowDrawFactorDisc(&glowPoints[2], GLOW_VIEW_2_RADIUS, GLOW_EQUAL_RGB_FACTORS);
            _glowDrawFactorDisc(&glowPoints[3], GLOW_VIEW_2_RADIUS, GLOW_EQUAL_RGB_FACTORS);
            break;
        }
        case 3:
            _glowDrawFactorDisc(&_gShelter1fBulwarkView3GlowPoint, GLOW_VIEW_3_RADIUS, GLOW_RED_FACTOR);
            break;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelter1fBulwarkRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelter1fBulwarkRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelter1fBulwarkRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
