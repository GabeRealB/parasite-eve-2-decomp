#include "rooms/shelter_1f_parking_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

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
// This room's fade symbol is the ScreenFade itself, with no following word.
#define ROOM_EVENT_FADE gRoomEventFade
#include "../../shared/room/room_events.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"

static s32 _roomVariantResolveShelter(RoomEventMsg* request, RoomEventMsg* reply);

extern ScreenFade       gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomDeparture    gRoomDeparture;
extern RoomLatchedEvent gRoomEventLatched;

static void _shelter1fParkingGarageInitializeRoom(Task* task);
static void _shelter1fParkingGarageIdle(Task* task);

static WorldCollisionGrid         _gShelter1fParkingGarageCollisionGrid;
static WorldCollisionTrigger      _gShelter1fParkingGarageViewBoundaryTriggers[4];
static WorldCollisionTrigger      _gShelter1fParkingGarageActionTriggers[5];
static WorldCoordRoomAmbientEntry _gShelter1fParkingGarageViewAmbientMinima[SHELTER_1F_PARKING_GARAGE_VIEW_COUNT + 1];
static WorldCoordRoomLights       _gShelter1fParkingGarageLights;

static s32  _shelter1fParkingGarageRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32  _shelter1fParkingGarageResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelter1fParkingGarageIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg);
static s32  _shelter1fParkingGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _shelter1fParkingGarageUndergroundDepartureTask(Task* task);

/// Bodyless resolved-departure recipe, priority `TASK_PRIORITY_SCENE` and zero metadata/payloads.
///
/// One descriptor with no terminator. Starts after the underground destination
/// has been stored in `gRoomDeparture`; that singleton and the callback must
/// remain loaded and unchanged through sound playback and session reload.
static TaskDesc _gShelter1fParkingGarageDepartureTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = roomEventDepartureTask,
    .data     = { .value = 0 },
};

/// Bodyless priority-32 recipe for the latched garage-to-airlock scene.
///
/// Spawn payloads are unused. Keep the latched destination/event records and
/// callback code loaded and stable through the staged departure.
static TaskDesc _gShelter1fParkingGarageStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Garage handlers for exit events, key-item refusal, travel actions and ignored commands.
///
/// Four handlers and a `TASK_MESSAGE_TABLE_END` record; the room task borrows
/// this read-only table and its callbacks while the overlay remains loaded.
static const TaskMessageEntry _gShelter1fParkingGarageRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fParkingGarageResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fParkingGarageRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fParkingGarageHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fParkingGarageIgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless underground-travel choice recipe, priority `TASK_PRIORITY_SCENE` and zero metadata/payloads.
///
/// One descriptor with no terminator. Starts after action 10 holds the player
/// and opens CAP command 2; confirmation fades and stages the underground
/// destination, while refusal resumes control. Keep this room loaded through reload.
static TaskDesc _gShelter1fParkingGarageUndergroundDepartureTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = _shelter1fParkingGarageUndergroundDepartureTask,
    .data     = { .value = 0 },
};

/// World-space endpoints of six ceiling glows and the garage's red point glow.
///
/// Each capsule contains two consecutive signed-halfword XYZ positions in
/// game units. Drawers borrow a pair or the disc centre only for the call;
/// the fourth halfword of each position has no projection meaning.
typedef struct {
    SVECTOR capsuleEndpoints[6][2]; // Pairs 0..2 at positive Z, pairs 3..5 at negative Z
    SVECTOR discCenter;             // Red disc centre drawn in mapped view 4
} _Shelter1fParkingGarageGlowPoints;
STATIC_ASSERT_SIZEOF(_Shelter1fParkingGarageGlowPoints, 0x68);

/// Fixed garage glow geometry; view 2 draws pairs 1/2/4/5, view 4 draws 0/3 and the disc.
static _Shelter1fParkingGarageGlowPoints _gShelter1fParkingGarageGlowPoints = {
    .capsuleEndpoints = {
        { { 400, -3090, 1790, 0 }, { 1600, -3090, 1790, 0 } },
        { { 4400, -3090, 1790, 0 }, { 5600, -3090, 1790, 0 } },
        { { 8400, -3090, 1790, 0 }, { 9600, -3090, 1790, 0 } },
        { { 400, -3090, -1790, 0 }, { 1600, -3090, -1790, 0 } },
        { { 4400, -3090, -1790, 0 }, { 5600, -3090, -1790, 0 } },
        { { 8400, -3090, -1790, 0 }, { 9600, -3090, -1790, 0 } },
    },
    .discCenter = { 2000, -2130, 2230, 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gShelter1fParkingGarageCollisionRooms[1] = {
    { &_gShelter1fParkingGarageCollisionGrid, _gShelter1fParkingGarageViewBoundaryTriggers, _gShelter1fParkingGarageActionTriggers, NULL },
};

WorldCoordRoomLighting gShelter1fParkingGarageRoomLighting[1] = {
    { &_gShelter1fParkingGarageLights, _gShelter1fParkingGarageViewAmbientMinima },
};

u8* gShelter1fParkingGarageViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelter1fParkingGarageViewCounts[1] = { SHELTER_1F_PARKING_GARAGE_VIEW_COUNT };

DirectionWarpEntry gShelter1fParkingGarageWarps[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 8564, 0, -1341 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 9100, 0, -1560 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 3), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 4), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_UNDERGROUND_PARKING },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 2000, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 2000, 0, 1700 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 2), SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 1), DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Q12 face normals of the garage collision mesh.
static SVECTOR _gShelter1fParkingGarageCollisionGridNormals[11] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_normals.inc"
};

/// Signed room-local game-coordinate vertices of the garage collision mesh.
static SVECTOR _gShelter1fParkingGarageCollisionGridVerts[34] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_verts.inc"
};

/// Thirteen indexed garage collision faces; the floor uses surface class 1.
static WorldCollisionGridFace _gShelter1fParkingGarageCollisionGridFaces[13] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_faces.inc"
};

/// Signed face-index lists for collision cells, ending at WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelter1fParkingGarageCollisionGridCells[88] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelter1fParkingGarageCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..87,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_1F_PARKING_GARAGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelter1fParkingGarageCollisionGridCells[(halfwordOffset)])
/// X-major 4-by-4 cell directory; NULL entries have no collision faces.
static s16* _gShelter1fParkingGarageCollisionGridTable[16] = {
#include "assets/shelter_1f_parking_garage_collision_03A28_table.inc"
};
#undef SHELTER_1F_PARKING_GARAGE_CELL_FACE_LIST_AT_OFFSET

/// Garage floor/wall mesh indexed by a 4-by-4 XZ grid of 4000-game-unit cells.
///
/// The grid origin is (-500, -6250). Gameplay binds the mutable view coordinate;
/// all mesh pools and cell face lists live in this room overlay.
static WorldCollisionGrid _gShelter1fParkingGarageCollisionGrid = {
    NULL,
    _gShelter1fParkingGarageCollisionGridNormals,
    _gShelter1fParkingGarageCollisionGridVerts,
    _gShelter1fParkingGarageCollisionGridFaces,
    _gShelter1fParkingGarageCollisionGridTable,
    500,
    6250,
    4,
    4,
    4000,
    ARRAY_SIZE(_gShelter1fParkingGarageCollisionGridFaces),
};

ViewCamera gShelter1fParkingGarageViewCameras[SHELTER_1F_PARKING_GARAGE_VIEW_COUNT] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 1016, 0, -3967 }, { 94, 4094, 24 }, { 3966, -97, 1016 } }, { -140, 1010, 1370 } }, 257 },
    { { { { 270, 0, -4087 }, { -2037, 3551, -134 }, { 3543, 2041, 234 } }, { -4450, 3000, -1030 } }, 257 },
    { { { { 829, 0, 4011 }, { 1836, 3641, -379 }, { -3565, 1875, 737 } }, { -6440, 3350, 790 } }, 257 },
};

/// Opaque/transparent split indices in the two populated sprite views.
enum {
    SHELTER_1F_PARKING_GARAGE_VIEW2_OPAQUE_SPRITES = 10,
    SHELTER_1F_PARKING_GARAGE_VIEW3_OPAQUE_SPRITES = 21,
};

/// Empty mapped-view-1 sprite list; the zero first count enables background strips.
static SpriteBatch _gShelter1fParkingGarageView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped-view-2 foreground rectangles: ten opaque and three semitransparent sprites.
///
/// Positions and dimensions are pixels relative to the view centre; depths
/// are OT sorting values. Batch ranges select elements with no source sentinel.
static SpriteSource _gShelter1fParkingGarageView2SpriteSources[13] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 48, 8, 2250, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 40, -16, 2125, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 40, 16, 2125, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -8, -16, 1975, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -8, 0, 1975, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 0, 8, 1875, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 16, 8, 1875, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -24, 8, 1875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 32, 8, 1875, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, -16, 2000, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -8, -16, 1937, { .fields = { 104, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, 8, -16, 1925, { .fields = { 88, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 40, -16, 1950, { .fields = { 120, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Background-strip selector, opaque range, transparent range and end for view 2.
static SpriteBatch _gShelter1fParkingGarageView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_1F_PARKING_GARAGE_VIEW2_OPAQUE_SPRITES, 0, 0, { 1, 0 } },
    { SHELTER_1F_PARKING_GARAGE_VIEW2_OPAQUE_SPRITES, ARRAY_SIZE(_gShelter1fParkingGarageView2SpriteSources) - SHELTER_1F_PARKING_GARAGE_VIEW2_OPAQUE_SPRITES, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped-view-3 foreground rectangles: 21 opaque and seven semitransparent sprites.
///
/// Positions and dimensions are pixels relative to the view centre; depths
/// are OT sorting values. Batch ranges select elements with no source sentinel.
static SpriteSource _gShelter1fParkingGarageView3SpriteSources[28] = {
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 72, 8 } }, 40, -64, 1125, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 32, -56, 1375, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 104, -56, 1125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 24, -48, 1500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 32, -48, 1375, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 40, -48, 950, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 72, -48, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 104, -48, 950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 112, -48, 950, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 120, -48, 950, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 128, -40, 950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, 64, 0, 875, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 40, 64 } }, 64, 56, 875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, 104, 56, 875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 152, 40, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 144, 32, 750, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, 136, 16, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 128, 0, 875, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 120, 0, 875, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 112, 0, 875, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 104, 0, 875, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 128, -40, 950, { .fields = { 64, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 120, -40, 950, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 112, -40, 966, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 104, -40, 870, { .fields = { 56, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 96, -40, 900, { .fields = { 56, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 88, -40, 900, { .fields = { 56, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(1, 0, 960, 0), getClut(0, 256), { .fields = { 56, 32 } }, 32, -40, 900, { .fields = { 8, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Background-strip selector, opaque range, transparent range and end for view 3.
static SpriteBatch _gShelter1fParkingGarageView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, SHELTER_1F_PARKING_GARAGE_VIEW3_OPAQUE_SPRITES, 0, 0, { 1, 0 } },
    { SHELTER_1F_PARKING_GARAGE_VIEW3_OPAQUE_SPRITES, ARRAY_SIZE(_gShelter1fParkingGarageView3SpriteSources) - SHELTER_1F_PARKING_GARAGE_VIEW3_OPAQUE_SPRITES, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty mapped-view-4 sprite list; the zero first count enables background strips.
static SpriteBatch _gShelter1fParkingGarageView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelter1fParkingGarageSpriteViews[SHELTER_1F_PARKING_GARAGE_VIEW_COUNT] = {
    { { .empty = _gShelter1fParkingGarageView1SpriteBatches }, _gShelter1fParkingGarageView1SpriteBatches, NULL },
    { { .elements = _gShelter1fParkingGarageView2SpriteSources }, _gShelter1fParkingGarageView2SpriteBatches, NULL },
    { { .elements = _gShelter1fParkingGarageView3SpriteSources }, _gShelter1fParkingGarageView3SpriteBatches, NULL },
    { { .empty = _gShelter1fParkingGarageView4SpriteBatches }, _gShelter1fParkingGarageView4SpriteBatches, NULL },
};

/// Four all-view point lights: three amber ceiling lights and one red light.
///
/// Positions and falloff distances use world game units; RGB intensities use
/// Q12. Gameplay reparents/composes coordinates and overwrites attenuation.
static WorldCoordPointLight _gShelter1fParkingGaragePointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1000, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2500, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2000, -2130, 1990 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 1638, 1638 }, { 0, 0 } }, 1000, 2001 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9000, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2500, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 4096 },
};

/// The garage's counted point-light collection, with no directional or cone lights.
static WorldCoordRoomLights _gShelter1fParkingGarageLights = {
    0,
    NULL,
    ARRAY_SIZE(_gShelter1fParkingGaragePointLights),
    _gShelter1fParkingGaragePointLights,
    0,
    NULL,
};

/// Directed boundaries linking views 2/4 and 2/3; the final quad carries LAST.
///
/// Gameplay borrows mutable nodes, parents them to the view and links/enables
/// every entry. Quad geometry uses room game units and Q12 normals.
static WorldCollisionTrigger _gShelter1fParkingGarageViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 3453, -3344, 314, 0 }, { { -503, -3680, -4249, 0 }, { 498, -3680, 4245, 0 }, { -503, 3680, -4249, 0 }, { 498, 3680, 4245, 0 } }, { 4068, 0, -480, 0 }, { 0, 0, 4096, 0 }, 5632, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3640, -3392, 347, 0 }, { { 479, -3696, 4156, 0 }, { -488, -3696, -4164, 0 }, { 479, 3696, 4156, 0 }, { -488, 3696, -4164, 0 } }, { -4077, 0, 473, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7056, -3297, 2400, 0 }, { { 432, -3760, 1920, 0 }, { -432, -3760, -1920, 0 }, { 432, 3760, 1920, 0 }, { -432, 3760, -1920, 0 } }, { -3997, 0, 899, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6943, -3394, 2560, 0 }, { { -448, -3696, -2000, 0 }, { 448, -3696, 2000, 0 }, { -448, 3697, -2000, 0 }, { 448, 3697, 2000, 0 } }, { 4004, 0, -898, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Underground travel action selected by both quads beside its departure point.
enum { SHELTER_1F_PARKING_GARAGE_ACTION_UNDERGROUND_TRAVEL = 10 };

/// Five facing-gated quads for underground travel, the airlock warp and CAP scenes.
///
/// Two quads send room action 10 with argument 20; the others select airlock
/// area 5 (departure slot 2, arrival slot 1) and CAP commands 1/4. Mutable
/// list nodes remain room-owned until unlinked; the final entry carries LAST.
static WorldCollisionTrigger _gShelter1fParkingGarageActionTriggers[5] = {
    { NULL, NULL, NULL, { 8560, -64, -928, 0 }, { { -1680, 0, -480, 0 }, { 1680, 0, -480, 0 }, { -1680, 0, 480, 0 }, { 1680, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1745, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_PARKING_GARAGE_ACTION_UNDERGROUND_TRAVEL, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2016, -48, 1568, 0 }, { { -496, 0, -256, 0 }, { 496, 0, -256, 0 }, { -496, 0, 256, 0 }, { 496, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 557, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_1F_AIRLOCK, (2 << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -96, -64, -80, 0 }, { { -496, 0, -1904, 0 }, { 496, 0, -1904, 0 }, { -496, 0, 1904, 0 }, { 496, 0, 1904, 0 } }, { 0, 4100, 0, 0 }, { 4090, 0, 200, 0 }, 1966, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8544, 0, 1024, 0 }, { { -1680, 0, -480, 0 }, { 1680, 0, -480, 0 }, { -1680, 0, 480, 0 }, { 1680, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1745, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_1F_PARKING_GARAGE_ACTION_UNDERGROUND_TRAVEL, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2870, -64, 0, 0 }, { { -576, 0, -2656, 0 }, { 576, 0, -2656, 0 }, { -576, 0, 2656, 0 }, { 576, 0, 2656, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 2709, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// View-count header followed by four ambient RGB/luma minima, at 16 units per level.
///
/// View 1 uses (16,16,16); views 2..4 use (250,250,250). Lighting only reads
/// this room-owned table; the fourth halfword retains the weighted luma.
static WorldCoordRoomAmbientEntry _gShelter1fParkingGarageViewAmbientMinima[SHELTER_1F_PARKING_GARAGE_VIEW_COUNT + 1] = {
    { .viewCount = ARRAY_SIZE(_gShelter1fParkingGarageViewAmbientMinima) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 250, 250, 250, 250 } },
    { .color = { 250, 250, 250, 250 } },
};

/// Terminator-only actor-resource list for the default placement variant.
///
/// The variant supplies no spawnable or loading-only actor resources.
static AreaResource _gShelter1fParkingGarageEmptyAreaResources[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gShelter1fParkingGarageAreaVariants[SHELTER_1F_PARKING_GARAGE_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkParkingGarageDefaultPlacements, .resources = _gShelter1fParkingGarageEmptyAreaResources },
};

/// Paired-cue bases in the currently loaded type-1 sound bank.
enum {
    SHELTER_1F_PARKING_GARAGE_WALK_FOOTSTEP_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 65,
    SHELTER_1F_PARKING_GARAGE_RUN_FOOTSTEP_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 67,
};

/// Class-1 footstep cue bases in the currently loaded type-1 sound bank.
///
/// Walking and scripted jumps use entry 65; running uses entry 67. Cue 1 adds
/// one to the base and companions add 100; surface records borrow this table.
static WorldCollisionFootstepSounds _gShelter1fParkingGarageFloorFootstepSounds = {
    SHELTER_1F_PARKING_GARAGE_WALK_FOOTSTEP_BASE,
    SHELTER_1F_PARKING_GARAGE_RUN_FOOTSTEP_BASE,
    SHELTER_1F_PARKING_GARAGE_WALK_FOOTSTEP_BASE,
};

/// Blocking, impact-enabled surface with pushback and no footsteps (classes 0/2..7).
static WorldCollisionSurfaceProperties _gShelter1fParkingGarageBlockingSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-1 floor policy: blocks probes, enables impacts/pushback and supplies footsteps.
static WorldCollisionSurfaceProperties _gShelter1fParkingGarageFloorSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelter1fParkingGarageFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelter1fParkingGarageSurfaceProperties[8] = {
    &_gShelter1fParkingGarageBlockingSurface,
    &_gShelter1fParkingGarageFloorSurface,
    &_gShelter1fParkingGarageBlockingSurface,
    &_gShelter1fParkingGarageBlockingSurface,
    &_gShelter1fParkingGarageBlockingSurface,
    &_gShelter1fParkingGarageBlockingSurface,
    &_gShelter1fParkingGarageBlockingSurface,
    &_gShelter1fParkingGarageBlockingSurface,
};

ScreenFade gRoomEventFade = { 0 };

/// Mutable 30-frame blackout for confirmed travel to the underground parking area.
///
/// The fade task borrows this record for its subtractive ramp and hold until
/// session reload. Keep the room loaded and do not reuse it during departure.
static ScreenFade _gShelter1fParkingGarageUndergroundDepartureFade = { 0 };

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
static u8 _gShelter1fParkingGarageStagedEventStarted = 0;

/// Retained, unreferenced bytes after the event-start byte; their role and grouping are unproven.
static u8 D_shelter_1f_parking_garage_80181985 = 25;

static u8 D_shelter_1f_parking_garage_80181986 = 36;

static u8 D_shelter_1f_parking_garage_80181987 = 75;

RoomDeparture gRoomDeparture;

RoomLatchedEvent gRoomEventLatched;

static void _glowDrawAngledCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 when the room handles the departure, including an eligible query;
/// returns 1 for ordinary departure when a nonzero event flag is already set.
/// Every call clears the latest-start byte. Only `ROOM_EVENT_EXECUTE` copies
/// both records, writes 1 to a nonzero flag and raises that byte after spawning.
/// Borrows complete records for this call; flag IDs must be 0..503. The room's
/// singleton copies and CAP/sound resources must remain live until its task ends;
/// do not latch another event while that task still uses them.
static inline s32 _shelter1fParkingGarageStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    _gShelter1fParkingGarageStagedEventStarted = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *message;
            gRoomEventLatched             = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelter1fParkingGarageStagedEventTaskDesc, 0, 0, 0);
            _gShelter1fParkingGarageStagedEventStarted = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

#include "../../shared/room/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room/room_event_departure_task.inc.c"

#include "../../shared/room/room_event_staged_task.inc.c"

#include "../../shared/room/room_variants_resolve_departure.inc.c"

/// Waits for the garage travel choice, fades out and stages the underground departure.
///
/// Starts bodyless in state 0. Choice 11 starts a 30-tick subtractive fade;
/// other choices resume actor/player control and end the task. On expiry,
/// advances companion schedule 9 to 10, resolves the Mine/Shelter destination
/// and starts the departure task. The final call still decrements the countdown
/// after requesting teardown. Room, fade and destination resources must remain live.
static void _shelter1fParkingGarageUndergroundDepartureTask(Task* task)
{
    enum {
        DEPARTURE_WAIT_CAP,
        DEPARTURE_CHECK_CHOICE,
        DEPARTURE_WAIT_FADE,
        DEPARTURE_CONFIRMED_CHOICE          = 11,
        DEPARTURE_FADE_FRAMES               = 30,
        DEPARTURE_FADE_TASK_BANK            = 1,
        DEPARTURE_FADE_TASK_TYPE            = 0x31,
        DEPARTURE_COMPANION_SCHEDULE_BEFORE = 9,
        DEPARTURE_COMPANION_SCHEDULE_AFTER  = 10,
        DEPARTURE_DEFAULT_ROOM              = 1,
        DEPARTURE_UNDERGROUND_ARRIVAL_WARP  = 2,
        DEPARTURE_SOUND                     = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 4)
    };
    RoomDeparture       departure;
    RoomVariantResolver resolveVariant;

    switch (task->state) {
        case DEPARTURE_WAIT_CAP:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case DEPARTURE_CHECK_CHOICE:
            if (capGetVariantKey() == DEPARTURE_CONFIRMED_CHOICE) {
                _gShelter1fParkingGarageUndergroundDepartureFade.blend      = SCREEN_FADE_SUBTRACT;
                _gShelter1fParkingGarageUndergroundDepartureFade.phase      = SCREEN_FADE_RUNNING;
                _gShelter1fParkingGarageUndergroundDepartureFade.rampFrames = DEPARTURE_FADE_FRAMES;
                taskSpawn(DEPARTURE_FADE_TASK_BANK, DEPARTURE_FADE_TASK_TYPE, 0, &_gShelter1fParkingGarageUndergroundDepartureFade);
                task->killCountdown = DEPARTURE_FADE_FRAMES;
                task->state++;
            } else {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
        case DEPARTURE_WAIT_FADE:
            if (task->killCountdown == 0) {
                if (gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == DEPARTURE_COMPANION_SCHEDULE_BEFORE) {
                    gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, DEPARTURE_COMPANION_SCHEDULE_AFTER);
                }
                resolveVariant      = _roomVariantResolveShelter;
                departure.stage     = GAME_STAGE_MINE_SHELTER;
                departure.area      = GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING;
                departure.roomState = DEPARTURE_DEFAULT_ROOM;
                departure.warp      = DEPARTURE_UNDERGROUND_ARRIVAL_WARP;
                departure.sndEvent  = DEPARTURE_SOUND;
                departure.facing    = ROOM_DEPARTURE_SKIP_FACING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                // Resolve the destination before publishing the singleton departure.
                _roomVariantResolveDeparture(&departure, resolveVariant);
                gRoomDeparture = departure;
                taskSpawnFromTable(&_gShelter1fParkingGarageDepartureTaskDesc, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Refuses every key-item use in the parking garage without changing the room.
///
/// Receives an integer item ID and an unused second argument. All parameters
/// are ignored; the zero reply selects the key-item menu's "cannot use now" notice.
static s32 _shelter1fParkingGarageRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a destination variant and gates the garage-to-airlock departure scene.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; borrows complete eight-byte request
/// and writable reply records, which may alias. Returns 1 for ordinary passage
/// or an already-seen scene, otherwise 2 for a room-handled departure. Queries
/// never latch the scene. An executing first visit copies the records for the
/// staged controller; keep the room and map overlays loaded until it finishes.
static s32 _shelter1fParkingGarageResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        TRANSITION_DIRECT   = 1,
        AIRLOCK_CAP_COMMAND = 3,
        AIRLOCK_SOUND       = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_PARKING_GARAGE, 1),
        AIRLOCK_FADE_NONE   = 0
    };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_1F_AIRLOCK) {
        return TRANSITION_DIRECT;
    }
    event.capCmd   = AIRLOCK_CAP_COMMAND;
    event.stageSnd = AIRLOCK_SOUND;
    event.flagId   = GAME_FLAG_1F_GARAGE_TO_AIRLOCK_SCENE;
    event.fade     = AIRLOCK_FADE_NONE;
    return _shelter1fParkingGarageStartEvent(reply, &event);
}

/// Ignores CAP room commands and returns zero without changing the room.
///
/// Both integer argument words and the receiver/message ID are unused.
static s32 _shelter1fParkingGarageIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg)
{
    return 0;
}

/// Starts the underground-parking travel prompt for room action 10.
///
/// Borrows the four-byte `DirectionActionRequest` only during dispatch; its
/// argument and the zero second word are unused. Holds the player, starts CAP
/// command 2 and spawns the choice/departure task. Every action returns zero.
static s32 _shelter1fParkingGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { ACTION_UNDERGROUND_DEPARTURE = 10,
           CAP_UNDERGROUND_PROMPT       = 2 };

    if (request->actionId == ACTION_UNDERGROUND_DEPARTURE) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        capRunCommandWithTransition(CAP_UNDERGROUND_PROMPT);
        taskSpawnFromTable(&_gShelter1fParkingGarageUndergroundDepartureTaskDesc, 0, 0, 0);
    }
    return 0;
}

/// Installs the room message receiver and plays the warp-1 arrival CAP command.
///
/// State 0 publishes a borrowed live task in `GAME_TASK_SLOT_ROOM`, then enters
/// idle state 1. Requires initialized session/CAP resources and this room loaded.
static void _shelter1fParkingGarageInitializeRoom(Task* task)
{
    enum { ARRIVAL_CAP_WARP    = 1,
           ARRIVAL_CAP_COMMAND = 5 };
    task->msgTable = _gShelter1fParkingGarageRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.warp == ARRIVAL_CAP_WARP) {
        capRunCommandWithTransition(ARRIVAL_CAP_COMMAND);
    }
    task->state = task->state + 1;
}

/// Garage room states: 0 initialize messages/arrival CAP, 1 idle, 2 release.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check; keep the room loaded through dispatch.
static const TaskFuncTable3 _gShelter1fParkingGarageRoomStateHandlers = { {
    _shelter1fParkingGarageInitializeRoom,
    _shelter1fParkingGarageIdle,
    taskKill,
} };

/// Keeps the room controller alive to receive messages without advancing its state.
static void _shelter1fParkingGarageIdle(Task* task)
{
    // Preserve the idle callback's stack reservation; the bytes are never read.
    char reservedFrame[0x10];
}

void shelter1fParkingGarageRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelter1fParkingGarageRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Binds actor-spawned effects to the parking garage's room implementations.
///
/// Installs packed bank/type IDs for the Pawn/Rook GOLEMs' silence-scream flash,
/// sword trail and grenade-impact burst. Call after room-effect controller
/// initialization clears the selectors and before actors spawn these effects.
/// This room overlay must remain loaded while the selected effect tasks run.
static inline void _shelter1fParkingGarageBindActorEffects(void)
{
    gRoomEffectFlashId      = EFFECT_SHELTER_1F_PARKING_GARAGE_FLASH;
    gRoomEffectTwinTrailId  = EFFECT_SHELTER_1F_PARKING_GARAGE_TWIN_TRAIL;
    gRoomEffectSparkBurstId = EFFECT_SHELTER_1F_PARKING_GARAGE_SPARK_BURST;
}

void shelter1fParkingGarageDrawViewGlowsTask(Task* task)
{
    enum {
        GLOWS_INITIALIZE            = 0,
        GLOWS_DRAW                  = 1,
        GLOWS_CAPSULE_RADIUS_SCALE  = 0x200,
        GLOWS_DISC_RADIUS_SCALE     = 0x300,
        GLOWS_CAPSULE_COLOR_FACTORS = (2 << 8) | (1 << 4),
        GLOWS_DISC_COLOR_FACTORS    = 2 << 8,
    };
    u8 mappedView;

    // Bind actor effect IDs once, after the room-effect controller resets them.
    if (task->state == GLOWS_INITIALIZE) {
        _shelter1fParkingGarageBindActorEffects();
        task->state = GLOWS_DRAW;
    }

    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2: {
            const SVECTOR(*capsulePoints)[2] = &_gShelter1fParkingGarageGlowPoints.capsuleEndpoints[1];
            _glowDrawAngledCapsule(capsulePoints[0], GLOWS_CAPSULE_RADIUS_SCALE, GLOW_HALF_TURN, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(capsulePoints[1], GLOWS_CAPSULE_RADIUS_SCALE, GLOW_HALF_TURN, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(capsulePoints[3], GLOWS_CAPSULE_RADIUS_SCALE, 0, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(capsulePoints[4], GLOWS_CAPSULE_RADIUS_SCALE, 0, GLOWS_CAPSULE_COLOR_FACTORS);
            break;
        }
        case 4: {
            const SVECTOR* discPoint = &_gShelter1fParkingGarageGlowPoints.discCenter;
            _glowDrawFactorDisc(discPoint, GLOWS_DISC_RADIUS_SCALE, GLOWS_DISC_COLOR_FACTORS);
            // Recover the complete glow geometry from its disc centre.
            _glowDrawAngledCapsule(PARENT_OF(discPoint, _Shelter1fParkingGarageGlowPoints, discCenter)->capsuleEndpoints[0], GLOWS_CAPSULE_RADIUS_SCALE, GLOW_HALF_TURN, GLOWS_CAPSULE_COLOR_FACTORS);
            _glowDrawAngledCapsule(PARENT_OF(discPoint, _Shelter1fParkingGarageGlowPoints, discCenter)->capsuleEndpoints[3], GLOWS_CAPSULE_RADIUS_SCALE, 0, GLOWS_CAPSULE_COLOR_FACTORS);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_angled_capsule.inc.c"

#include "../../shared/effects/glow_draw_factor_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelter1fParkingGarageRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelter1fParkingGarageRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelter1fParkingGarageRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
