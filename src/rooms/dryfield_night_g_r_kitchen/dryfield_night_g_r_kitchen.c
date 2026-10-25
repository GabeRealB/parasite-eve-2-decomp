#include "rooms/dryfield_night_g_r_kitchen.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00700.h"
#include "actors/actor_01600.h"
#include "actors/actor_04000.h"

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
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/room/room_variants.h"
/// Selects the nighttime sound bank for the shared G & R kitchen door handler.
///
/// Keep this binding through the door-handler implementation fragment.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/g_r_kitchen.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event the room's gate `_roomEventGate`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `_gRoomEventTaskDesc`, and the flag the gate
/// sets once it has done so.
extern RoomEventReq gRoomEventReq;

static s32 _roomVariantGRKitchenMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);

static s32 _dryfieldNightGRKitchenRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedSecondArg);
static s32 _dryfieldNightGRKitchenIgnoreCommand(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32 _dryfieldNightGRKitchenIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Door sound requests, floor cue bases and door map-marker indices.
enum {
    DRYFIELD_NIGHT_G_R_KITCHEN_WARP_ARRIVAL_SOUND   = DRYFIELD_STAGE_SOUND((GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN << 16) | 2),
    DRYFIELD_NIGHT_G_R_KITCHEN_WARP_DEPARTURE_SOUND = DRYFIELD_STAGE_SOUND((GAME_AREA_DRYFIELD_NIGHT_G_R_KITCHEN << 16) | 1),
    DRYFIELD_NIGHT_G_R_KITCHEN_FLOOR_WALK_SOUND     = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_NIGHT_G_R_KITCHEN_FLOOR_RUN_SOUND      = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_NIGHT_G_R_KITCHEN_SALOON_MAP_FLAG      = 478,
    DRYFIELD_NIGHT_G_R_KITCHEN_WATER_TOWER_MAP_FLAG = 477,
};

/// Packed GPU addresses and neutral colour modulation for view 2's sprite sources.
enum {
    DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE         = 143,
    DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT          = 0x3FC0,
    DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR = 128,
};

/// Placement-entry keys used by the kitchen's three resource lists.
enum {
    DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_SCAVENGER     = 16,
    DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_RAT           = 7,
    DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_MOTH          = 8,
    DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_BLOOD_SUCKLER = 40,
};

/// Loaded CAP command slots requested by the kitchen's facing quads.
enum {
    DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_1 = 1,
    DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_2 = 2,
    DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_4 = 4,
    DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_5 = 5,
    DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_6 = 6,
};

static WorldCollisionGrid    _gDryfieldNightGRKitchenCollisionGrid;
static WorldCollisionTrigger _gDryfieldNightGRKitchenViewBoundaryTriggers[2];
static WorldCollisionTrigger _gDryfieldNightGRKitchenActionTriggers[7];
static WorldCoordRoomLights  _gDryfieldNightGRKitchenRoomLights;

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Room-message routes for departure resolution, refused key items and ignored commands.
///
/// The live room task borrows this five-entry table, including its end marker,
/// for synchronous dispatch while the kitchen overlay and callback code are loaded.
static TaskMessageEntry _gDryfieldNightGRKitchenRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGRKitchenMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGRKitchenRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightGRKitchenIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightGRKitchenIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// World-coordinate endpoints of view 2's two light shafts, borrowed only while drawing.
static SVECTOR _gDryfieldNightGRKitchenView2LightShaftPoints[2][2] = {
    { { -1473, -2181, 2023, 0 }, { -1473, -2181, 2416, 0 } },
    { { -197, -2161, 2973, 0 }, { 197, -2161, 2973, 0 } },
};

/// World-coordinate endpoints of view 3's two light shafts, borrowed only while drawing.
static SVECTOR _gDryfieldNightGRKitchenView3LightShaftPoints[2][2] = {
    { { -70, -2790, -370, 0 }, { -70, -2790, 370, 0 } },
    { { 70, -2790, -370, 0 }, { 70, -2790, 370, 0 } },
};

WorldCoordRoomLighting gDryfieldNightGRKitchenRoomLighting[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT] = {
    { &_gDryfieldNightGRKitchenRoomLights, NULL },
};

WorldCollisionRoomResources gDryfieldNightGRKitchenRoomCollisionResources[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT] = {
    { &_gDryfieldNightGRKitchenCollisionGrid, _gDryfieldNightGRKitchenViewBoundaryTriggers, _gDryfieldNightGRKitchenActionTriggers, NULL },
};

u8* gDryfieldNightGRKitchenViewMaps[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT] = {
    gViewIdentityMap,
};

ViewCount gDryfieldNightGRKitchenViewCounts[DRYFIELD_NIGHT_G_R_KITCHEN_ROOM_COUNT] = { DRYFIELD_NIGHT_G_R_KITCHEN_VIEW_COUNT };

DirectionWarpEntry gDryfieldNightGRKitchenWarpEntries[DRYFIELD_NIGHT_G_R_KITCHEN_WARP_COUNT] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -1168, 0, 2135 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, -1168, 0, 2135 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_G_R_KITCHEN_WARP_ARRIVAL_SOUND, DRYFIELD_NIGHT_G_R_KITCHEN_WARP_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_G_R_KITCHEN_SALOON_MAP_FLAG },
    { { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 2512 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 0, 0, 2512 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_G_R_KITCHEN_WARP_ARRIVAL_SOUND, DRYFIELD_NIGHT_G_R_KITCHEN_WARP_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_G_R_KITCHEN_WATER_TOWER_MAP_FLAG },
};

/// Unit room-space normals used by the kitchen collision mesh (ONE per unit).
static SVECTOR _gDryfieldNightGRKitchenCollisionGridNormals[6] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_normals.inc"
};

/// Collision-mesh vertices in signed room-coordinate units.
static SVECTOR _gDryfieldNightGRKitchenCollisionGridVerts[26] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_verts.inc"
};

/// Indexed collision quads; class 1 is the floor and class 0 covers the other faces.
static WorldCollisionGridFace _gDryfieldNightGRKitchenCollisionGridFaces[16] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_faces.inc"
};

/// Two terminated face-index lists, with one retained zero between them.
static s16 _gDryfieldNightGRKitchenCollisionGridCells[32] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightGRKitchenCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..31,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define DRYFIELD_NIGHT_G_R_KITCHEN_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightGRKitchenCollisionGridCells[(halfwordOffset)])
/// X-major cell pointers for the grid's one X cell and two Z cells.
static s16* _gDryfieldNightGRKitchenCollisionGridTable[2] = {
#include "assets/dryfield_night_g_r_kitchen_collision_00F94_table.inc"
};
#undef DRYFIELD_NIGHT_G_R_KITCHEN_CELL_FACE_LIST_AT_OFFSET

/// The kitchen collision mesh, selected through a 1-by-2 XZ grid of 4000-unit cells.
///
/// Origin is (-1800, -3000) in room XZ. Gameplay borrows and binds this mutable
/// record to the view; all mesh pools and cell lists live with the room overlay.
static WorldCollisionGrid _gDryfieldNightGRKitchenCollisionGrid = {
    NULL,
    _gDryfieldNightGRKitchenCollisionGridNormals,
    _gDryfieldNightGRKitchenCollisionGridVerts,
    _gDryfieldNightGRKitchenCollisionGridFaces,
    _gDryfieldNightGRKitchenCollisionGridTable,
    1800,
    3000,
    1,
    2,
    4000,
    ARRAY_SIZE(_gDryfieldNightGRKitchenCollisionGridFaces),
};

ViewCamera gDryfieldNightGRKitchenViewCameras[DRYFIELD_NIGHT_G_R_KITCHEN_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 30000, 0 } }, 1053 },
    { { { { 3955, 0, 1064 }, { 591, 3405, -2198 }, { -885, 2276, 3288 } }, { -400, 2700, 1000 } }, 230 },
    { { { { -3988, 0, 933 }, { -276, 3911, -1183 }, { -891, -1215, -3808 } }, { -500, 400, -2700 } }, 230 },
};

/// View 1's background-strip selector and terminal batch; no source sprites.
static SpriteBatch _gDryfieldNightGRKitchenView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's seven texture rectangles, positioned in pixels with unscaled sorting depths.
///
/// Texture page and CLUT are packed GPU addresses. RGB 128 is neutral modulation;
/// zero code flags select opaque modulated direct emission. Cached packets use
/// raw textures. The room's sprite view borrows this mutable source array.
static SpriteSource _gDryfieldNightGRKitchenView2SpriteSources[7] = {
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 48, 48 } }, -136, -88, 625, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 40, 24 } }, -120, -40, 675, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 40, 16 } }, -112, -16, 725, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 40, 16 } }, -104, 0, 750, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 24, 48 } }, -56, 32, 700, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 32, 40 } }, -88, 40, 675, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_TPAGE, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_CLUT, { .fields = { 40, 24 } }, -96, 16, 750, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_G_R_KITCHEN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// View 2's background selector, complete seven-sprite range and terminal batch.
static SpriteBatch _gDryfieldNightGRKitchenView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldNightGRKitchenView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's background-strip selector and terminal batch; no source sprites.
static SpriteBatch _gDryfieldNightGRKitchenView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightGRKitchenSpriteViews[DRYFIELD_NIGHT_G_R_KITCHEN_VIEW_COUNT] = {
    { { .empty = _gDryfieldNightGRKitchenView1SpriteBatches }, _gDryfieldNightGRKitchenView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightGRKitchenView2SpriteSources }, _gDryfieldNightGRKitchenView2SpriteBatches, NULL },
    { { .empty = _gDryfieldNightGRKitchenView3SpriteBatches }, _gDryfieldNightGRKitchenView3SpriteBatches, NULL },
};

/// Four point lights contributing in every view, with Q12 RGB and world-unit radii.
///
/// Gameplay mutates their coordinate parents, composition caches and attenuation.
/// The last light has equal 100000-unit radii, retaining full strength up to that range.
static WorldCoordPointLight _gDryfieldNightGRKitchenPointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2400, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 3200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1800, 2800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1300, -1800, 2500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1000, 1000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 100000, 100000 },
};

/// The kitchen's point-light collection, with no directional or cone lights.
static WorldCoordRoomLights _gDryfieldNightGRKitchenRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldNightGRKitchenPointLights),
    _gDryfieldNightGRKitchenPointLights,
    0,
    NULL,
};

/// Directed boundaries switching between views 2 and 3, ending on the second quad.
///
/// Gameplay binds their room-unit geometry to the view and mutates links and hit latches.
static WorldCollisionTrigger _gDryfieldNightGRKitchenViewBoundaryTriggers[2] = {
    { NULL, NULL, NULL, { -2, -1167, 381, 0 }, { { 1974, -1520, 391, 0 }, { -1973, -1520, -390, 0 }, { 1974, 1520, 391, 0 }, { -1973, 1520, -390, 0 } }, { -796, 0, 4017, 0 }, { 0, 0, ONE, 0 }, 2521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1, -1152, 479, 0 }, { { -1973, -1520, -390, 0 }, { 1974, -1520, 391, 0 }, { -1973, 1520, -390, 0 }, { 1974, 1520, 391, 0 } }, { 794, 0, -4019, 0 }, { 0, 0, ONE, 0 }, 2521, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two door warps and five CAP interactions, all requiring facing-quad contact.
///
/// Warp parameters pack the kitchen departure slot in the high nibble and the
/// destination arrival slot in the low nibble. CAP parameters select commands
/// 2, 1, 6, 5 and 4 with zero presentation flags. The seventh quad carries LAST;
/// linked records and their hit latches remain mutable while the room is loaded.
static WorldCollisionTrigger _gDryfieldNightGRKitchenActionTriggers[7] = {
    { NULL, NULL, NULL, { -1248, -48, 2336, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R, (DRYFIELD_NIGHT_G_R_KITCHEN_WARP_SALOON << 4) | 2, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 96, -48, 2784, 0 }, { { 736, 0, -320, 0 }, { 736, 0, 320, 0 }, { -736, 0, -320, 0 }, { -736, 0, 320, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -ONE, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_WATER_TOWER, (DRYFIELD_NIGHT_G_R_KITCHEN_WARP_WATER_TOWER << 4) | 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -576, -64, -352, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 336, -64, -2400, 0 }, { { -688, 0, -576, 0 }, { 688, 0, -576, 0 }, { -688, 0, 576, 0 }, { 688, 0, 576, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, ONE, 0 }, 896, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -544, -64, 960, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -608, -64, -1600, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { ONE, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 784, -64, -912, 0 }, { { -464, 0, -1296, 0 }, { 464, 0, -1296, 0 }, { -464, 0, 1296, 0 }, { 464, 0, 1296, 0 } }, { 0, 4098, 0, 0 }, { -ONE, 0, 0, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_G_R_KITCHEN_CAP_COMMAND_4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Scavenger and Rat loading/spawn recipes, followed by the end marker.
///
/// Placement entry IDs 16 and 7 select task descriptor zero in the borrowed actor tables.
static AreaResource _gDryfieldNightGRKitchenVariant1Resources[3] = {
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_SCAVENGER, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_RAT, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor200700RatTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 3's Blood Suckler, Rat and Moth recipes, followed by the end marker.
///
/// Placement IDs 40, 7 and 8 select descriptor zero. IDs 7 and 8 load file 7 in
/// group 20 but use distinct task tables; the entry ID is not a file number.
static AreaResource _gDryfieldNightGRKitchenVariant3Resources[4] = {
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_BLOOD_SUCKLER, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor04000BloodSucklerTaskDesc },
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_RAT, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor200700RatTaskDesc },
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_MOTH, 7, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor00700MothTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 11's Scavenger and Blood Suckler recipes, followed by the end marker.
///
/// Placement IDs 16 and 40 select descriptor zero from actor groups 10 and 20.
static AreaResource _gDryfieldNightGRKitchenVariant11Resources[3] = {
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_SCAVENGER, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { DRYFIELD_NIGHT_G_R_KITCHEN_RESOURCE_BLOOD_SUCKLER, 40, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor204000BloodSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightGRKitchenAreaVariants[12] = {
    [1]  = { .placements = gMapDryfieldFullGRKitchenVariant1Placements, .resources = _gDryfieldNightGRKitchenVariant1Resources },
    [3]  = { .placements = gMapDryfieldFullGRKitchenVariant3Placements, .resources = _gDryfieldNightGRKitchenVariant3Resources },
    [11] = { .placements = gMapDryfieldFullGRKitchenVariant11Placements, .resources = _gDryfieldNightGRKitchenVariant11Resources },
};

/// Class-1 floor footstep bases in the loaded type-1 bank; jumps share the walking pair.
///
/// Animation cue 2 uses each base and cue 1 adds one; companions add 100.
static WorldCollisionFootstepSounds _gDryfieldNightGRKitchenFloorFootstepSounds = {
    DRYFIELD_NIGHT_G_R_KITCHEN_FLOOR_WALK_SOUND,
    DRYFIELD_NIGHT_G_R_KITCHEN_FLOOR_RUN_SOUND,
    DRYFIELD_NIGHT_G_R_KITCHEN_FLOOR_WALK_SOUND,
};

/// Blocking, impact-enabled surface policy for classes without footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightGRKitchenSilentSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class-1 floor policy: blocks probes, allows impacts and pushback, and supplies footsteps.
static WorldCollisionSurfaceProperties _gDryfieldNightGRKitchenFloorSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldNightGRKitchenFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightGRKitchenSurfacePropertiesByClass[DRYFIELD_NIGHT_G_R_KITCHEN_SURFACE_CLASS_COUNT] = {
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
    &_gDryfieldNightGRKitchenFloorSurfaceProperties,
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
    &_gDryfieldNightGRKitchenSilentSurfaceProperties,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 0, 222, 254 } };

RoomEventReq gRoomEventReq;

static void _dryfieldNightGRKitchenInitializeRoom(Task* task);
static void _dryfieldNightGRKitchenIdleState(Task* task);

#include "../../shared/room/room_event.inc.c"

static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

/// Refuses every key-item use in the nighttime kitchen without consuming the item.
///
/// `keyItemId` is an inventory item ID; the menu treats the zero reply as unusable.
/// All arguments are ignored and no room state changes.
static s32 _dryfieldNightGRKitchenRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/g_r_kitchen_door_msg.inc.c"

/// Ignores `ROOM_MESSAGE_COMMAND`, returning zero without changing room state.
///
/// Both payload words are unused, including CAP's command selector and mode.
static s32 _dryfieldNightGRKitchenIgnoreCommand(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION`, returning zero without changing room state.
///
/// The borrowed action request is neither read nor retained; the sender's second
/// payload word is zero. All arguments are ignored.
static s32 _dryfieldNightGRKitchenIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the nighttime kitchen's room-message receiver and advances to idle.
///
/// The session's room slot borrows the live task; teardown does not clear it.
static void _dryfieldNightGRKitchenInitializeRoom(Task* task)
{
    task->msgTable = _gDryfieldNightGRKitchenRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the initialized room task idle while its installed message table remains live.
static void _dryfieldNightGRKitchenIdleState(Task* task)
{
}

/// Room-task states 0..2: install the receiver, idle, then release the task.
///
/// Dispatch takes a complete value snapshot before selecting a handler by state.
/// The callback code and this table remain live with the kitchen overlay.
static const TaskFuncTable3 _gDryfieldNightGRKitchenRoomStates = {
    { _dryfieldNightGRKitchenInitializeRoom, _dryfieldNightGRKitchenIdleState, taskKill },
};

void dryfieldNightGRKitchenRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightGRKitchenRoomStates;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/effects/glow_draw_shaft.inc.c"

void dryfieldNightGRKitchenDrawLightShaftsTask(Task* unusedTask)
{
    enum {
        // Pixel radius is this scale times 64, divided by camera Z / 4.
        DRYFIELD_NIGHT_G_R_KITCHEN_LIGHT_SHAFT_RADIUS_SCALE = 256,
    };
    u8 view;

    view = gGameSession->location.view;
    if (view == 2) {
        _glowDrawShaft(_gDryfieldNightGRKitchenView2LightShaftPoints[0], DRYFIELD_NIGHT_G_R_KITCHEN_LIGHT_SHAFT_RADIUS_SCALE);
        _glowDrawShaft(_gDryfieldNightGRKitchenView2LightShaftPoints[1], DRYFIELD_NIGHT_G_R_KITCHEN_LIGHT_SHAFT_RADIUS_SCALE);
    } else if (view == 3) {
        _glowDrawShaft(_gDryfieldNightGRKitchenView3LightShaftPoints[0], DRYFIELD_NIGHT_G_R_KITCHEN_LIGHT_SHAFT_RADIUS_SCALE);
        _glowDrawShaft(_gDryfieldNightGRKitchenView3LightShaftPoints[1], DRYFIELD_NIGHT_G_R_KITCHEN_LIGHT_SHAFT_RADIUS_SCALE);
    }
}
