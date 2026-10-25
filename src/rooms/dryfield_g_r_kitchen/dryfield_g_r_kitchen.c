#include "rooms/dryfield_g_r_kitchen.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_01500.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"
#include "../../shared/effects/glow_draw.h"
/// Selects the daytime sound bank for the shared G & R kitchen door handler.
///
/// Keep this binding through the door-handler implementation fragment.
#define DRYFIELD_TIME DRYFIELD_DAY
#include "../../shared/g_r_kitchen.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.

static s32 _roomVariantGRKitchenMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

static void _dryfieldGRKitchenInitRoomTask(Task* task);
static void _dryfieldGRKitchenIdleRoomTask(Task* task);
static void _dryfieldGRKitchenDrawDimTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale);

static s32 _dryfieldGRKitchenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldGRKitchenIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandMode);
static s32 _dryfieldGRKitchenIgnoreActionRequest(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static WorldCollisionGrid    _gDryfieldGRKitchenCollisionGrid;
static WorldCollisionTrigger _gDryfieldGRKitchenViewBoundaryTriggers[2];
static WorldCollisionTrigger _gDryfieldGRKitchenActionTriggers[7];
static WorldCoordRoomLights  _gDryfieldGRKitchenRoomLights;

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// The room receiver's four message bindings and final unsupported-message marker.
///
/// Resolves departures, refuses key items and ignores action/command requests.
/// Borrowed by the room task for its lifetime; never dispatch the end-marker ID.
static const TaskMessageEntry _gDryfieldGRKitchenMessageTable[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGRKitchenMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldGRKitchenRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldGRKitchenIgnoreActionRequest },
    { ROOM_MESSAGE_COMMAND, _dryfieldGRKitchenIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Local-space endpoints of the kitchen's four view-selected light beams.
///
/// Brighter view-2 beams run 1->0 and 3->2; dimmer view-3 beams run 4->5 and
/// 6->7. XYZ uses the room coordinate's game units. Preserve endpoint order:
/// it selects cap orientation, clipping and depth sorting. Each complete
/// word-aligned SVECTOR is borrowed only during drawing.
static const SVECTOR _gDryfieldGRKitchenLightBeamEndpoints[8] SECTION(".data") = {
    { -1473, -2181, 2023, 0 },
    { -1473, -2181, 2416, 0 },
    { -197, -2161, 2973, 0 },
    { 197, -2161, 2973, 0 },
    { -70, -2790, -370, 0 },
    { -70, -2790, 370, 0 },
    { 70, -2790, -370, 0 },
    { 70, -2790, 370, 0 },
};

WorldCollisionRoomResources gDryfieldGRKitchenRoomCollisionResources[1] = {
    { &_gDryfieldGRKitchenCollisionGrid, _gDryfieldGRKitchenViewBoundaryTriggers, _gDryfieldGRKitchenActionTriggers, NULL },
};

u8* gDryfieldGRKitchenViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldGRKitchenViewCounts[1] = { DRYFIELD_G_R_KITCHEN_VIEW_COUNT };

WorldCoordRoomLighting gDryfieldGRKitchenRoomLighting[1] = {
    { &_gDryfieldGRKitchenRoomLights, NULL },
};

/// Door sounds, map-marker IDs, and packed departure/arrival warp slots.
enum {
    DRYFIELD_G_R_KITCHEN_ARRIVAL_SOUND          = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_G_R_KITCHEN, 2),
    DRYFIELD_G_R_KITCHEN_DEPARTURE_SOUND        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_G_R_KITCHEN, 1),
    DRYFIELD_G_R_KITCHEN_SALOON_MAP_FLAG        = 478,
    DRYFIELD_G_R_KITCHEN_WATER_TOWER_MAP_FLAG   = 477,
    DRYFIELD_G_R_KITCHEN_SALOON_WARP_SLOTS      = (1 << 4) | 2,
    DRYFIELD_G_R_KITCHEN_WATER_TOWER_WARP_SLOTS = (2 << 4) | 1,
};

DirectionWarpEntry gDryfieldGRKitchenWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -1168, 0, 2135 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -1168, 0, 2135 }, { 0, 0, 0, 0 }, DRYFIELD_G_R_KITCHEN_ARRIVAL_SOUND, DRYFIELD_G_R_KITCHEN_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_G_R_KITCHEN_SALOON_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 2512 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 2512 }, { 0, 0, 0, 0 }, DRYFIELD_G_R_KITCHEN_ARRIVAL_SOUND, DRYFIELD_G_R_KITCHEN_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_G_R_KITCHEN_WATER_TOWER_MAP_FLAG },
};

/// Room-grid normal pool; six vectors with 4096 representing unit length.
static SVECTOR _gDryfieldGRKitchenCollisionGridNormals[6] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_normals.inc"
};

/// Room-grid vertex pool; 26 signed-halfword positions in room-local game units.
static SVECTOR _gDryfieldGRKitchenCollisionGridVerts[26] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_verts.inc"
};

/// Sixteen room-grid quads indexing the vertex/normal pools and surface classes 0/1.
static WorldCollisionGridFace _gDryfieldGRKitchenCollisionGridFaces[16] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_faces.inc"
};

/// Two terminated face-index lists in the grid's retained 32-halfword storage.
///
/// The lists start at 0 and 18 and end at 16 and 31; entry 17 is an unreferenced
/// authored zero. Their indices stay within the 16-face pool.
static s16 _gDryfieldGRKitchenCollisionGridCells[32] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldGRKitchenCollisionGridCells` (32 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_G_R_KITCHEN_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldGRKitchenCollisionGridCells[(halfwordOffset)])

/// X-major pointers to the grid's two Z-cell face lists.
static s16* _gDryfieldGRKitchenCollisionGridTable[2] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_table.inc"
};
#undef DRYFIELD_G_R_KITCHEN_CELL_FACE_LIST_AT_OFFSET

/// Kitchen floor/wall collision mesh, indexed by one X cell and two Z cells.
///
/// The origin is (-1800, -3000) in room X/Z and cells span 4000 game units.
/// Room activation binds the view coordinate; the mutable mesh pools and cell
/// lists remain borrowed with the grid until this overlay is unloaded.
static WorldCollisionGrid _gDryfieldGRKitchenCollisionGrid = {
    NULL,
    _gDryfieldGRKitchenCollisionGridNormals,
    _gDryfieldGRKitchenCollisionGridVerts,
    _gDryfieldGRKitchenCollisionGridFaces,
    _gDryfieldGRKitchenCollisionGridTable,
    1800,
    3000,
    1,
    ARRAY_SIZE(_gDryfieldGRKitchenCollisionGridTable),
    4000,
    ARRAY_SIZE(_gDryfieldGRKitchenCollisionGridFaces),
};

ViewCamera gDryfieldGRKitchenViewCameras[DRYFIELD_G_R_KITCHEN_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 30000, 0 } }, 1053 },
    { { { { 3955, 0, 1064 }, { 591, 3405, -2198 }, { -885, 2276, 3288 } }, { -400, 2700, 1000 } }, 230 },
    { { { { -3988, 0, 933 }, { -276, 3911, -1183 }, { -891, -1215, -3808 } }, { -500, 400, -2700 } }, 230 },
};

/// Empty view-1 sprite list: keep decoded background strips, then stop.
static SpriteBatch _gDryfieldGRKitchenView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-2 sprite texture addresses and neutral modulation for opaque emission.
enum {
    DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE      = getTPage(1, 0, 960, 0), // 8-bit texture at VRAM (960, 0)
    DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE           = getClut(0, 255),
    DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR    = 128,
    DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED = 0,
};

/// View 2's seven depth-sorted texture rectangles in drawing-origin pixels.
///
/// Uses the view's texture page and row-255 palette with neutral modulation.
/// Mutable source geometry and depths are borrowed by sprite packet builders.
static SpriteSource _gDryfieldGRKitchenView2Sprites[7] = {
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 48, 48 } }, -136, -88, 625, { .fields = { 80, 0 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 40, 24 } }, -120, -40, 675, { .fields = { 88, 152 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 40, 16 } }, -112, -16, 725, { .fields = { 88, 176 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 40, 16 } }, -104, 0, 750, { .fields = { 88, 192 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 40, 24 } }, -96, 16, 750, { .fields = { 88, 128 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 24, 40 } }, -56, 32, 700, { .fields = { 104, 88 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
    { DRYFIELD_G_R_KITCHEN_VIEW2_TEXTURE_PAGE, DRYFIELD_G_R_KITCHEN_VIEW2_PALETTE, { .fields = { 32, 40 } }, -88, 40, 675, { .fields = { 96, 48 } }, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_G_R_KITCHEN_SPRITE_OPAQUE_MODULATED },
};

/// View 2 keeps decoded background strips, draws all seven sprites, then stops.
static SpriteBatch _gDryfieldGRKitchenView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldGRKitchenView2Sprites), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty view-3 sprite list: keep decoded background strips, then stop.
static SpriteBatch _gDryfieldGRKitchenView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldGRKitchenSpriteViews[DRYFIELD_G_R_KITCHEN_VIEW_COUNT] = {
    { { .empty = _gDryfieldGRKitchenView1SpriteBatches }, _gDryfieldGRKitchenView1SpriteBatches, NULL },
    { { .elements = _gDryfieldGRKitchenView2Sprites }, _gDryfieldGRKitchenView2SpriteBatches, NULL },
    { { .empty = _gDryfieldGRKitchenView3SpriteBatches }, _gDryfieldGRKitchenView3SpriteBatches, NULL },
};

/// Directed view boundaries 2->3 and 3->2, including the LAST-marked second quad.
///
/// Origins and relative vertices use room-local game units; normals use `ONE`
/// per unit. Room setup supplies coordinates, list links and enable flags.
/// Writable records must stay loaded until the view-boundary list is cleared.
static WorldCollisionTrigger _gDryfieldGRKitchenViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { -2, -1167, 381, 0 }, { { 1974, -1520, 391, 0 }, { -1973, -1520, -390, 0 }, { 1974, 1520, 391, 0 }, { -1973, 1520, -390, 0 } }, { -796, 0, 4017, 0 }, { 0, 0, ONE, 0 }, 2521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1, -1152, 479, 0 }, { { -1973, -1520, -390, 0 }, { 1974, -1520, 391, 0 }, { -1973, 1520, -390, 0 }, { 1974, 1520, 391, 0 } }, { 794, 0, -4019, 0 }, { 0, 0, ONE, 0 }, 2521, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two door warps followed by five CAP interactions, all requiring facing-quad contact.
///
/// Warps use kitchen departure slots 1/2 and saloon/water-tower arrival slots
/// 2/1. CAP slots are 2, 1, 6, 5 and 4 with zero presentation flags. The final
/// record carries LAST and participates in the list. Geometry is room-local;
/// runtime binding, links, flags and hit latches stay writable while loaded.
static WorldCollisionTrigger _gDryfieldGRKitchenActionTriggers[7] = {
    { NULL, NULL, NULL, { -1248, -48, 2336, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_SALOON_G_R, DRYFIELD_G_R_KITCHEN_SALOON_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -48, 2784, 0 }, { { 736, 0, -320, 0 }, { 736, 0, 320, 0 }, { -736, 0, -320, 0 }, { -736, 0, 320, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_WATER_TOWER, DRYFIELD_G_R_KITCHEN_WATER_TOWER_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -640, -64, -415, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 416, -64, -2400, 0 }, { { -608, 0, -576, 0 }, { 608, 0, -576, 0 }, { -608, 0, 576, 0 }, { 608, 0, 576, 0 } }, { 0, 4099, 0, 0 }, { -401, 0, 4076, 0 }, 836, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -544, -64, 944, 0 }, { { -320, 0, -592, 0 }, { 320, 0, -592, 0 }, { -320, 0, 592, 0 }, { 320, 0, 592, 0 } }, { 0, 4106, 0, 0 }, { ONE, 0, 0, 0 }, 671, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -640, -64, -1696, 0 }, { { -320, 0, -592, 0 }, { 320, 0, -592, 0 }, { -320, 0, 592, 0 }, { 320, 0, 592, 0 } }, { 0, 4106, 0, 0 }, { ONE, 0, 0, 0 }, 671, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, -1024, 0 }, { { -416, 0, -1232, 0 }, { 416, 0, -1232, 0 }, { -416, 0, 1232, 0 }, { 416, 0, 1232, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 1299, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Four white point lights contributing in every view, with mutable transform caches.
///
/// Positions and inner/outer falloff radii use room game units; RGB has 12
/// fractional bits. The fourth light has equal 100000-unit radii. Room-light
/// updates bind the view parent and shading overwrites attenuation.
static WorldCoordPointLight _gDryfieldGRKitchenPointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2400, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 3200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1800, 2800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1300, -1800, 2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 100000, 100000 },
};

/// The kitchen's four mutable point lights, with no directional or cone sources.
static WorldCoordRoomLights _gDryfieldGRKitchenRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldGRKitchenPointLights),
    _gDryfieldGRKitchenPointLights,
    0,
    NULL,
};

/// Variant 1's Mind Suckler file and spawn descriptor, followed by the resource terminator.
///
/// Placement key 15 selects decimal actor file 101500 and its first task
/// descriptor. The borrowed actor overlay must be loaded before spawning.
static AreaResource _gDryfieldGRKitchenVariant1Resources[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's Mind Suckler and rat resources, followed by the resource terminator.
///
/// Placement keys 15/7 select decimal files 101500/200700 and their first task
/// descriptors. The borrowed actor overlays must be loaded before spawning.
static AreaResource _gDryfieldGRKitchenVariant2Resources[3] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor200700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldGRKitchenAreaVariants[13] = {
    [1] = { .placements = gMapDryfieldGRKitchenVariant1Placements, .resources = _gDryfieldGRKitchenVariant1Resources },
    [2] = { .placements = gMapDryfieldGRKitchenVariant2Placements, .resources = _gDryfieldGRKitchenVariant2Resources },
};

/// Cue-2 bases in the loaded type-1 footstep bank; cue 1 uses the next entry.
enum {
    DRYFIELD_G_R_KITCHEN_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_G_R_KITCHEN_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
};

/// Surface class 1's walking, running and scripted stair-climb footstep bases.
///
/// Walking/stair climbing share entries 0x35/0x36; running uses 0x37/0x38. Companion
/// playback adds 100 to the entry. Borrowed without modification while loaded.
static const WorldCollisionFootstepSounds _gDryfieldGRKitchenClass1FootstepSounds SECTION(".data") = {
    DRYFIELD_G_R_KITCHEN_WALK_SOUND,
    DRYFIELD_G_R_KITCHEN_RUN_SOUND,
    DRYFIELD_G_R_KITCHEN_WALK_SOUND,
};

/// Classes 0 and 2..7 block probes, allow weapon impacts and apply pushback without footsteps.
static WorldCollisionSurfaceProperties _gDryfieldGRKitchenSilentSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 1 blocks probes, allows weapon impacts and applies pushback with footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldGRKitchenClass1SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldGRKitchenClass1FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldGRKitchenSurfacePropertiesByClass[8] = {
    &_gDryfieldGRKitchenSilentSurfaceProperties,
    &_gDryfieldGRKitchenClass1SurfaceProperties,
    &_gDryfieldGRKitchenSilentSurfaceProperties,
    &_gDryfieldGRKitchenSilentSurfaceProperties,
    &_gDryfieldGRKitchenSilentSurfaceProperties,
    &_gDryfieldGRKitchenSilentSurfaceProperties,
    &_gDryfieldGRKitchenSilentSurfaceProperties,
    &_gDryfieldGRKitchenSilentSurfaceProperties,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 34, 223, 253 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room/room_event.inc.c"

/// Room-task states 0 (register receiver), 1 (idle) and 2 (kill).
///
/// Copied to the dispatcher's stack before indexing by the live task state;
/// the state must stay within these three slots while this overlay is loaded.
static const TaskFuncTable3 _gDryfieldGRKitchenRoomTaskStates = {
    { _dryfieldGRKitchenInitRoomTask, _dryfieldGRKitchenIdleRoomTask, taskKill },
};

/// Refuses every key-item use request in this room.
///
/// Ignores all arguments and returns 0, selecting the inventory's "No use now"
/// notice without consuming the item or starting a room event.
static s32 _dryfieldGRKitchenRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/g_r_kitchen_door_msg.inc.c"

/// Ignores room commands from CAP playback and direction triggers.
///
/// Neither the command ID nor its mode changes room state. Returns 0;
/// the senders discard the result.
static s32 _dryfieldGRKitchenIgnoreCommand(Task* task, s32 messageId, s32 commandId, s32 commandMode)
{
    return 0;
}

/// Ignores room-action requests from direction triggers.
///
/// The request is borrowed for synchronous dispatch but is never accessed
/// or retained. Returns 0; the sender discards the result.
static s32 _dryfieldGRKitchenIgnoreActionRequest(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the kitchen's room receiver and advances it to idle state 1.
///
/// Requires the live room task in state 0. The installed message table is
/// borrowed for the room task's lifetime.
static void _dryfieldGRKitchenInitRoomTask(Task* task)
{
    task->msgTable = _gDryfieldGRKitchenMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Keeps the registered room task idle between message dispatches.
///
/// Leaves the task and its state unchanged; the installed message table
/// remains available without any per-frame room work.
static void _dryfieldGRKitchenIdleRoomTask(Task* task)
{
}

void dryfieldGRKitchenRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldGRKitchenRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_tapered_beam.inc.c"

/// Draws an additive dim grey beam between two local-space endpoints.
///
/// Borrows `coord`, `startPoint` and `endPoint` during the call. `coord->workm`
/// must already map the endpoints into world space. Inputs must be word-aligned,
/// and each endpoint must provide a complete `SVECTOR` for the GTE's two word
/// loads. World components narrow to signed 16 bits before view projection.
/// Each pixel radius is the signed low halfword of `radiusScale` times 64
/// divided by its endpoint's camera Z / 4 depth. The second depth must be at
/// least 17; the first is clamped to 16. Projection flags are not tested.
///
/// Opposing half-disc caps fade from grey 16/32 on frame parity to a black
/// rim. Two bands join their left and right edges and sort at the first
/// endpoint's depth. Angles are a quarter turn behind `_glowDrawTaperedBeam`.
/// Requires an initialized scratch stack with one free 40-byte block, a current
/// depth ordering table and room for six `POLY_G4` plus six `DR_TPAGE` packets
/// in the word-aligned frame arena. Queued storage lives until GPU completion.
static void _dryfieldGRKitchenDrawDimTaperedBeam(const GfxCoord* coord, const SVECTOR* startPoint, const SVECTOR* endPoint, s32 radiusScale)
{
    enum {
        DRYFIELD_G_R_KITCHEN_BEAM_BASE_INTENSITY = 16,
        DRYFIELD_G_R_KITCHEN_BEAM_FLICKER_STEP   = 16,
    };

    GlowWorldPointPairScratch* scratch;
    POLY_G4*                   quad;
    s32                        angle;
    s32                        rimAngle;
    s32                        intensity;
    s32                        scaledRadius;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GlowWorldPointPairScratch);

    // Place both local endpoints in world space before projecting them.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(startPoint);
    gte_rtv0();
    gte_stsv(&scratch->worldPoint0);
    scratch->worldPoint0.vx += coord->workm.t[0];
    scratch->worldPoint0.vy += coord->workm.t[1];
    scratch->worldPoint0.vz += coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(endPoint);
    gte_rtv0();
    gte_stsv(&scratch->worldPoint1);
    scratch->worldPoint1.vx += coord->workm.t[0];
    scratch->worldPoint1.vy += coord->workm.t[1];
    scratch->worldPoint1.vz += coord->workm.t[2];

    // Use the second endpoint for clipping and each depth for perspective sizing.
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint0);
    gte_rtps();
    gte_stsxy(&scratch->sx0);
    gte_stszotz(&scratch->otz0);
    gte_ldv0(&scratch->worldPoint1);
    gte_rtps();
    gte_stsxy(&scratch->sx1);
    gte_stszotz(&scratch->otz1);
    if (scratch->otz1 >= GLOW_MIN_DEPTH) {
        if (scratch->otz0 < GLOW_NEAR_DEPTH_CLAMP) {
            scratch->otz0 = GLOW_NEAR_DEPTH_CLAMP;
        }
        scaledRadius     = (s16)radiusScale * GLOW_RADIUS_SCALE;
        angle            = 0;
        intensity        = (((u8)gDisplayState.animFrame & 1) * DRYFIELD_G_R_KITCHEN_BEAM_FLICKER_STEP) + DRYFIELD_G_R_KITCHEN_BEAM_BASE_INTENSITY;
        scratch->radius0 = scaledRadius / scratch->otz0;
        scratch->radius1 = scaledRadius / scratch->otz1;
        // Join opposite half-disc caps with bands at the left and right rims.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _glowInitTaperedBeamWedge(quad, intensity);
            quad->x0 = scratch->sx0 + ((scratch->radius0 * rsin(angle - GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT);
            rimAngle = angle - GLOW_EIGHTH_TURN;
            quad->y0 = scratch->sy0 + ((scratch->radius0 * rcos(angle - GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT);
            quad->x1 = scratch->sx0 + ((scratch->radius0 * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1 = scratch->sy0 + ((scratch->radius0 * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
            quad->x2 = scratch->sx0;
            quad->y2 = scratch->sy0;
            quad->x3 = scratch->sx0 + ((scratch->radius0 * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y3 = scratch->sy0 + ((scratch->radius0 * rcos(angle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, intensity, intensity, intensity);
            setRGB3(quad, intensity, intensity, intensity);
            quad->x0 = scratch->sx0 + ((scratch->radius0 * rsin(angle * 2 - GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT);
            quad->y0 = scratch->sy0 + ((scratch->radius0 * rcos(angle * 2 - GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT);
            quad->x1 = scratch->sx1 + ((scratch->radius1 * rsin(angle * 2 - GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT);
            quad->y1 = scratch->sy1 + ((scratch->radius1 * rcos(angle * 2 - GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT);
            quad->x2 = scratch->sx0;
            quad->y2 = scratch->sy0;
            quad->x3 = scratch->sx1;
            quad->y3 = scratch->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz0),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz0);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _glowInitTaperedBeamWedge(quad, intensity);
            quad->x0 = scratch->sx1 + ((scratch->radius1 * rsin((GLOW_FULL_TURN - GLOW_QUARTER_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            quad->y0 = scratch->sy1 + ((scratch->radius1 * rcos((GLOW_FULL_TURN - GLOW_QUARTER_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            quad->x1 = scratch->sx1 + ((scratch->radius1 * rsin((GLOW_FULL_TURN - GLOW_QUARTER_TURN - GLOW_EIGHTH_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            quad->y1 = scratch->sy1 + ((scratch->radius1 * rcos((GLOW_FULL_TURN - GLOW_QUARTER_TURN - GLOW_EIGHTH_TURN) - angle)) >> GLOW_TRIG_SHIFT);
            quad->x2 = scratch->sx1;
            quad->y2 = scratch->sy1;
            quad->x3 = scratch->sx1 + ((scratch->radius1 * rsin(GLOW_HALF_TURN - angle)) >> GLOW_TRIG_SHIFT);
            quad->y3 = scratch->sy1 + ((scratch->radius1 * rcos(GLOW_HALF_TURN - angle)) >> GLOW_TRIG_SHIFT);
            angle   += GLOW_QUARTER_TURN;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz1),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz1);
        } while (angle < GLOW_HALF_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowWorldPointPairScratch);
}

void dryfieldGRKitchenDrawLightBeamsTask(Task* task)
{
    enum {
        DRYFIELD_G_R_KITCHEN_BRIGHT_BEAM_VIEW  = 2,
        DRYFIELD_G_R_KITCHEN_DIM_BEAM_VIEW     = 3,
        DRYFIELD_G_R_KITCHEN_BEAM_RADIUS_SCALE = 256, // Pixel radius = scale * 64 / (camera Z / 4)
    };

    const GfxCoord* coord;

    coord = task->extra.coordBody->coord;
    // Endpoint order controls cap orientation, clipping and band sorting.
    if (gGameSession->location.view == DRYFIELD_G_R_KITCHEN_BRIGHT_BEAM_VIEW) {
        _glowDrawTaperedBeam(coord, &_gDryfieldGRKitchenLightBeamEndpoints[1], &_gDryfieldGRKitchenLightBeamEndpoints[0], DRYFIELD_G_R_KITCHEN_BEAM_RADIUS_SCALE);
        _glowDrawTaperedBeam(coord, &_gDryfieldGRKitchenLightBeamEndpoints[3], &_gDryfieldGRKitchenLightBeamEndpoints[2], DRYFIELD_G_R_KITCHEN_BEAM_RADIUS_SCALE);
    } else if (gGameSession->location.view == DRYFIELD_G_R_KITCHEN_DIM_BEAM_VIEW) {
        _dryfieldGRKitchenDrawDimTaperedBeam(coord, &_gDryfieldGRKitchenLightBeamEndpoints[4], &_gDryfieldGRKitchenLightBeamEndpoints[5], DRYFIELD_G_R_KITCHEN_BEAM_RADIUS_SCALE);
        _dryfieldGRKitchenDrawDimTaperedBeam(coord, &_gDryfieldGRKitchenLightBeamEndpoints[6], &_gDryfieldGRKitchenLightBeamEndpoints[7], DRYFIELD_G_R_KITCHEN_BEAM_RADIUS_SCALE);
    }
}
