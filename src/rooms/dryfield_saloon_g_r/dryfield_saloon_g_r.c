#include "rooms/dryfield_saloon_g_r.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_01200.h"
#include "actors/actor_01500.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../shared/room/room_events.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_variants.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.

static void _dryfieldSaloonGRInitializeRoom(Task* task);
static void _dryfieldSaloonGRIdleTask(Task* task);

static s32 _dryfieldSaloonGRRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _dryfieldSaloonGRCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldSaloonGRIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

static WorldCollisionGrid     _gDryfieldSaloonGRCollisionGrid;
static WorldCollisionOccluder _gDryfieldSaloonGRSightOccluders[2];
static WorldCollisionTrigger  _gDryfieldSaloonGRViewBoundaryTriggers[16];
static WorldCollisionTrigger  _gDryfieldSaloonGRActionTriggers[14];
static WorldCoordRoomLights   _gDryfieldSaloonGRRoomLights;

/// Bodyless transition task spawned by this room's event gate at priority 32.
///
/// Uses the latched request and destination to play CAP and optional sounds,
/// then reload the session. Spawn payloads and metadata are unused. The gate
/// reads the recipe synchronously; room snapshots and callback code must remain
/// live through the deferred transition.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Four synchronous room-message handlers followed by the unsupported-ID sentinel.
///
/// Resolves exits, rejects key-item use, ignores room actions and handles CAP
/// commands. The room task borrows this table and its callbacks for its entire
/// registered lifetime. Payload types belong to each message's handler.
static const TaskMessageEntry _gDryfieldSaloonGRRoomMessages[5] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantSaloonMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldSaloonGRRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldSaloonGRIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldSaloonGRCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Day-saloon flare centres, beam endpoints and shaft guides in local game-coordinate units.
///
/// Points 0..10 are flare centres, 12/13 the beam endpoints, and 14/17 the
/// shaft roots with guides 15/18 and 16/19. Shaft root offsets are extended
/// fourfold. Every active point uses the light task's composed coordinate.
/// Point 11 is unread; its role and coordinate frame are unproven.
/// XYZ are signed halfwords; the fourth SDK component is unused and zero.
/// The table stays live with this overlay and drawing leaves it unchanged.
static SVECTOR _gDryfieldSaloonGREffectPoints[20] = {
    { 4915, -1870, -727, 0 },
    { 4915, -1870, -2339, 0 },
    { 4915, -1870, -4188, 0 },
    { -3905, -1870, -4460, 0 },
    { -100, -1870, 4785, 0 },
    { 1100, -1870, 4785, 0 },
    { 3500, -2097, 3000, 0 },
    { 3500, -2097, 2000, 0 },
    { 3500, -2097, 1000, 0 },
    { 3500, -2097, 0, 0 },
    { 4200, -2097, -500, 0 },
    { 4983, -2211, 4240, 0 },
    { 4983, -2211, 4467, 0 },
    { 4983, -2211, 4013, 0 },
    { 700, -1640, 1680, 0 },
    { 700, -1520, 1520, 0 },
    { 700, -1520, 1840, 0 },
    { -750, -1640, 1680, 0 },
    { -750, -1520, 1520, 0 },
    { -750, -1520, 1840, 0 },
};

/// Logical-view visibility masks for eleven flares, the beam and the twin shafts.
///
/// Bit N enables logical view N, without applying the resource-view map.
/// Entries 0..10 match flare points 0..10; entry 11 gates the beam in views
/// 4/7 and entry 12 gates both shafts in views 4/5. The signed halfword storage
/// is retained; active masks use only bits 2..7. Entry 13 is unread and its
/// role is unproven. Drawing reads the table without changing it.
static s16 _gDryfieldSaloonGREffectViewMasks[14] = {
    1 << 7,
    (1 << 2) | (1 << 3),
    1 << 2,
    1 << 3,
    (1 << 4) | (1 << 5),
    (1 << 4) | (1 << 5),
    (1 << 4) | (1 << 7),
    (1 << 4) | (1 << 7),
    1 << 7,
    1 << 7,
    1 << 7,
    (1 << 4) | (1 << 7),
    (1 << 4) | (1 << 5),
    1,
};

WorldCollisionRoomResources gDryfieldSaloonGRRoomCollisionResources[2] = {
    { &_gDryfieldSaloonGRCollisionGrid, _gDryfieldSaloonGRViewBoundaryTriggers, _gDryfieldSaloonGRActionTriggers, _gDryfieldSaloonGRSightOccluders },
    { &_gDryfieldSaloonGRCollisionGrid, _gDryfieldSaloonGRViewBoundaryTriggers, _gDryfieldSaloonGRActionTriggers, _gDryfieldSaloonGRSightOccluders },
};

/// Logical views 1..13 mapped to the saloon's camera, image and sprite slots.
///
/// Both room selectors share this unsigned byte map, indexed by view minus one.
/// Logical views 9..12 share resource slot 9; view 13 selects slot 13.
/// The final three zero bytes are outside the logical-view count; their role
/// is unproven. They are retained storage, not a map terminator.
static u8 _gDryfieldSaloonGRLogicalViewMap[16] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    9,
    9,
    9,
    13,
    0,
    0,
    0,
};

u8* gDryfieldSaloonGRRoomViewMaps[2] = {
    _gDryfieldSaloonGRLogicalViewMap,
    _gDryfieldSaloonGRLogicalViewMap,
};

ViewCount gDryfieldSaloonGRRoomViewCounts[2] = {
    DRYFIELD_SALOON_G_R_LOGICAL_VIEW_COUNT,
    DRYFIELD_SALOON_G_R_LOGICAL_VIEW_COUNT,
};

WorldCoordRoomLighting gDryfieldSaloonGRRoomLighting[2] = {
    { &_gDryfieldSaloonGRRoomLights, NULL },
    { &_gDryfieldSaloonGRRoomLights, NULL },
};

/// Each door's arrival/departure cues and map-marker nibble.
enum {
    DRYFIELD_SALOON_G_R_PARKING_LOT_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SALOON_G_R, 2),
    DRYFIELD_SALOON_G_R_PARKING_LOT_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SALOON_G_R, 1),
    DRYFIELD_SALOON_G_R_KITCHEN_ARRIVAL_SOUND       = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_SALOON_G_R, 4),
    DRYFIELD_SALOON_G_R_PARKING_LOT_MAP_FLAG        = 479,
    DRYFIELD_SALOON_G_R_KITCHEN_MAP_FLAG            = 478,
};

DirectionWarpEntry gDryfieldSaloonGRWarpEntries[2] = {
    { { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, DRYFIELD_SALOON_G_R_PARKING_LOT_ARRIVAL_SOUND, DRYFIELD_SALOON_G_R_PARKING_LOT_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DRYFIELD_SALOON_G_R_PARKING_LOT_MAP_FLAG },
    { { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, DRYFIELD_SALOON_G_R_KITCHEN_ARRIVAL_SOUND, SOUND_SALOON_G_R_DOOR_OPEN, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DRYFIELD_SALOON_G_R_KITCHEN_MAP_FLAG },
};

/// Nine grid-local normals, with ONE (4096) per unit.
static SVECTOR _gDryfieldSaloonGRCollisionGridNormals[9] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_normals.inc"
};

/// 116 signed-halfword vertex positions in room-local game units.
static SVECTOR _gDryfieldSaloonGRCollisionGridVerts[116] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_verts.inc"
};

/// 61 collision quads indexing the vertex/normal pools and surface classes 0..3.
static WorldCollisionGridFace _gDryfieldSaloonGRCollisionGridFaces[61] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_faces.inc"
};

/// Twelve END-terminated face-index lists in 280 retained signed halfwords.
///
/// Unreferenced authored zeros at indices 17, 75 and 161 are retained.
/// Every live list index is in 0..60; WORLD_COLLISION_GRID_CELL_END ends a list.
static s16 _gDryfieldSaloonGRCollisionGridCells[280] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldSaloonGRCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..279 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_SALOON_G_R_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldSaloonGRCollisionGridCells[(halfwordOffset)])
/// X-major pointers to three rows of four Z-cell face lists.
static s16* _gDryfieldSaloonGRCollisionGridTable[12] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_table.inc"
};
#undef DRYFIELD_SALOON_G_R_CELL_FACE_LIST_AT_OFFSET

/// Saloon floor/wall collision mesh shared by both daytime room selectors.
///
/// Three X cells and four Z cells start at (-4500, -5400), with square sides
/// of 4000 game units. Activation binds the mutable descriptor to the current
/// view. The descriptor and its mesh/cell storage stay live with this overlay.
static WorldCollisionGrid _gDryfieldSaloonGRCollisionGrid = {
    NULL,
    _gDryfieldSaloonGRCollisionGridNormals,
    _gDryfieldSaloonGRCollisionGridVerts,
    _gDryfieldSaloonGRCollisionGridFaces,
    _gDryfieldSaloonGRCollisionGridTable,
    4500,
    5400,
    3,
    ARRAY_SIZE(_gDryfieldSaloonGRCollisionGridTable) / 3,
    4000,
    ARRAY_SIZE(_gDryfieldSaloonGRCollisionGridFaces),
};

ViewCamera gDryfieldSaloonGRViewCameras[DRYFIELD_SALOON_G_R_CAMERA_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 30000, -680 } }, 519 },
    { { { { -3183, 0, -2576 }, { -1148, 3666, 1418 }, { 2306, 1824, -2850 } }, { 315, 2637, 16 } }, 257 },
    { { { { -4045, 0, -638 }, { -348, 3430, 2210 }, { 534, 2237, -3388 } }, { 724, 3602, -2788 } }, 230 },
    { { { { 4063, 0, -511 }, { -261, 3522, -2074 }, { 439, 2090, 3494 } }, { 628, 3274, 3211 } }, 230 },
    { { { { 3790, 0, -1551 }, { -715, 3635, -1746 }, { 1377, 1887, 3364 } }, { 1033, 2561, -668 } }, 230 },
    { { { { -171, 0, 4092 }, { 3010, 2774, 126 }, { -2771, 3013, -116 } }, { -1881, 2483, -5950 } }, 188 },
    { { { { 3128, 0, -2643 }, { -887, 3858, -1050 }, { 2490, 1375, 2946 } }, { -1421, 2204, 1801 } }, 230 },
    { { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -620, 1520, -5560 } }, 230 },
    { { { { 702, 0, 4035 }, { 1935, 3594, -337 }, { -3540, 1964, 616 } }, { 2150, 1500, -3200 } }, 289 },
    { { { { 2919, 0, 2873 }, { 1386, 3587, -1408 }, { -2516, 1976, 2557 } }, { 1395, 2355, -2735 } }, 257 },
    { { { { 2919, 0, 2873 }, { 1386, 3587, -1408 }, { -2516, 1976, 2557 } }, { 1395, 2355, -2735 } }, 257 },
    { { { { 2919, 0, 2873 }, { 1386, 3587, -1408 }, { -2516, 1976, 2557 } }, { 1395, 2355, -2735 } }, 257 },
    { { { { 2919, 0, 2873 }, { 1429, 3553, -1451 }, { -2492, 2037, 2532 } }, { 1395, 2500, -2735 } }, 257 },
};

/// Mapped view 1's empty sprite list, preserving the decoded background strips.
static SpriteBatch _gDryfieldSaloonGRView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Encoded 8-bit atlas pages, palette rows and neutral sprite modulation.
///
/// Pages start at VRAM (896, 0) and (960, 0); palettes start at X 0 in
/// rows 255 and 256. RGB 128 leaves sampled colours at their authored levels.
enum {
    DRYFIELD_SALOON_G_R_SPRITE_TEXTURE_8BIT  = 1,
    DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE     = getTPage(DRYFIELD_SALOON_G_R_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE    = getTPage(DRYFIELD_SALOON_G_R_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255   = getClut(0, 255),
    DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256   = getClut(0, 256),
    DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR = 128,
};

/// Mapped view 2's 17 texture rectangles, in pixels with unscaled sorting depths.
static SpriteSource _gDryfieldSaloonGRView2SpriteSources[17] = {
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 40 } }, -104, 48, 875, { .fields = { 88, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 32 } }, -136, 40, 875, { .fields = { 72, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 48 } }, -160, -8, 850, { .fields = { 88, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, -160, 40, 850, { .fields = { 64, 16 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 72 } }, -24, 48, 550, { .fields = { 104, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 32 } }, 0, 88, 550, { .fields = { 88, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 24 } }, 16, 96, 500, { .fields = { 72, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 32 } }, 16, 64, 525, { .fields = { 40, 184 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 48, 24 } }, 72, 80, 500, { .fields = { 64, 112 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 48, 16 } }, 72, 104, 500, { .fields = { 48, 216 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 16 } }, 120, 104, 500, { .fields = { 56, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 48 } }, 32, 8, 1075, { .fields = { 96, 184 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 64 } }, 64, -16, 1075, { .fields = { 112, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 40, -8, 1025, { .fields = { 64, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 40, -24, 1075, { .fields = { 64, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 8 } }, 24, 0, 1050, { .fields = { 88, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, 24, -16, 1050, { .fields = { 72, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 2's full source range after a background-preserving empty batch.
static SpriteBatch _gDryfieldSaloonGRView2SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gDryfieldSaloonGRView2SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 3's 14 texture rectangles, in pixels with unscaled sorting depths.
static SpriteSource _gDryfieldSaloonGRView3SpriteSources[14] = {
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 32 } }, -56, -16, 1200, { .fields = { 80, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 64 } }, -80, 8, 1050, { .fields = { 104, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 56 } }, -56, 16, 975, { .fields = { 104, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 32 } }, -32, -16, 1250, { .fields = { 48, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 56 } }, -32, 16, 975, { .fields = { 72, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 24, 8 } }, 48, 112, 719, { .fields = { 72, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 32, 16 } }, 16, 104, 730, { .fields = { 88, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 24, 16 } }, -8, 104, 516, { .fields = { 72, 168 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 32, 24 } }, -40, 96, 522, { .fields = { 72, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 24, 24 } }, -64, 96, 521, { .fields = { 80, 144 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 32, 32 } }, -96, 88, 529, { .fields = { 96, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 32, 32 } }, -128, 88, 529, { .fields = { 96, 176 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, -136, 96, 532, { .fields = { 120, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 16, 8 } }, -144, 112, 527, { .fields = { 80, 184 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 3's ranges [0, 5) and [5, 14), keeping decoded background strips.
static SpriteBatch _gDryfieldSaloonGRView3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, ARRAY_SIZE(_gDryfieldSaloonGRView3SpriteSources) - 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 4's 56 texture rectangles, in pixels with unscaled sorting depths.
static SpriteSource _gDryfieldSaloonGRView4SpriteSources[56] = {
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 56 } }, -32, -112, 1250, { .fields = { 112, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 56 } }, 16, -112, 1250, { .fields = { 120, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 80, 16 } }, -40, -56, 1250, { .fields = { 72, 152 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, -56, -32, 1325, { .fields = { 16, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 32, -32, 1325, { .fields = { 48, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 64, 16 } }, -32, -32, 1375, { .fields = { 8, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 40, -16, 1325, { .fields = { 48, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 24 } }, 40, 0, 1375, { .fields = { 80, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, -64, -16, 1325, { .fields = { 40, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, -64, 0, 1325, { .fields = { 48, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 88, 16 } }, -48, 0, 1375, { .fields = { 104, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 80, 16 } }, -40, -16, 1375, { .fields = { 112, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 64, 16 } }, -48, 16, 1375, { .fields = { 120, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 16 } }, -136, -32, 1350, { .fields = { 112, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, -120, -16, 1325, { .fields = { 0, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, -120, 0, 1325, { .fields = { 0, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 16 } }, -160, -16, 1350, { .fields = { 120, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 16 } }, -160, 0, 1325, { .fields = { 0, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 8 } }, -160, 16, 1250, { .fields = { 56, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 16 } }, -152, 24, 1250, { .fields = { 8, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 112, 40 } }, -8, 80, 705, { .fields = { 16, 168 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 88, 32 } }, -8, 48, 798, { .fields = { 16, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 40 } }, 56, 8, 925, { .fields = { 96, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 80, 32 } }, -24, 16, 1000, { .fields = { 24, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 56 } }, -24, 48, 925, { .fields = { 112, 112 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 40 } }, -48, 64, 925, { .fields = { 96, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 32 } }, -56, 32, 925, { .fields = { 80, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 88, -32, 1833, { .fields = { 112, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 88, -48, 1615, { .fields = { 80, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 88, -64, 1862, { .fields = { 64, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 80, -64, 2213, { .fields = { 64, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 80, -40, 1611, { .fields = { 64, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 96, -64, 1775, { .fields = { 72, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 96, -40, 1588, { .fields = { 64, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 96, -24, 1724, { .fields = { 56, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 104, -16, 1623, { .fields = { 64, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 104, -40, 1414, { .fields = { 72, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 104, -64, 1727, { .fields = { 72, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 112, -56, 1608, { .fields = { 72, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 112, -32, 1377, { .fields = { 72, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 112, -8, 1540, { .fields = { 40, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 120, 0, 1455, { .fields = { 40, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 120, -24, 1509, { .fields = { 72, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 120, -48, 1480, { .fields = { 88, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 32 } }, 128, -48, 1424, { .fields = { 88, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 128, -16, 1229, { .fields = { 80, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 128, 8, 1378, { .fields = { 32, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 136, 8, 1327, { .fields = { 80, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 136, -16, 1213, { .fields = { 104, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 136, -40, 1371, { .fields = { 80, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 32 } }, 144, -40, 1323, { .fields = { 104, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 144, -8, 1292, { .fields = { 88, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 144, 16, 1059, { .fields = { 8, 16 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 16 } }, 152, 24, 997, { .fields = { 8, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 24 } }, 152, 0, 1065, { .fields = { 96, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW256, { .fields = { 8, 32 } }, 152, -32, 1251, { .fields = { 104, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 4's ranges [0, 20), [20, 27) and [27, 56), keeping background strips.
static SpriteBatch _gDryfieldSaloonGRView4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 2, 0 } },
    { 20, 7, 0, 0, { 1, 0 } },
    { 27, ARRAY_SIZE(_gDryfieldSaloonGRView4SpriteSources) - 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 5's 52 texture rectangles, in pixels with unscaled sorting depths.
static SpriteSource _gDryfieldSaloonGRView5SpriteSources[52] = {
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, 16, -120, 250, { .fields = { 32, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, 16, -72, 250, { .fields = { 40, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 56 } }, 16, -24, 250, { .fields = { 64, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, 144, -88, 250, { .fields = { 24, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 56 } }, -32, 40, 250, { .fields = { 48, 112 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 56 } }, 8, 32, 250, { .fields = { 88, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 48 } }, 48, 24, 250, { .fields = { 8, 144 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 48 } }, 88, 16, 250, { .fields = { 16, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 48, 48 } }, 112, 8, 250, { .fields = { 0, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 56 } }, 128, -48, 250, { .fields = { 104, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 40 } }, -56, -88, 1125, { .fields = { 0, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 16 } }, -56, -48, 1125, { .fields = { 64, 176 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 24 } }, -48, -32, 1200, { .fields = { 88, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 24 } }, -48, -8, 1250, { .fields = { 104, 216 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 40 } }, -88, -88, 1125, { .fields = { 0, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 16 } }, -88, -48, 1125, { .fields = { 64, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 24 } }, -88, -32, 1250, { .fields = { 88, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 32 } }, -88, -8, 1250, { .fields = { 96, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 48, 32 } }, 112, -40, 1150, { .fields = { 80, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 72, 32 } }, 88, -8, 1175, { .fields = { 56, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, 0, -88, 1250, { .fields = { 56, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, 0, -72, 1250, { .fields = { 48, 216 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, 0, -56, 1250, { .fields = { 48, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, 0, -40, 1325, { .fields = { 88, 112 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, 0, -24, 1375, { .fields = { 96, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, 0, -8, 1375, { .fields = { 80, 168 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 8, -88, 1250, { .fields = { 80, 216 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 8, -72, 1250, { .fields = { 88, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 8, -56, 1312, { .fields = { 64, 16 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 8, -40, 1325, { .fields = { 64, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 8 } }, 8, -8, 1375, { .fields = { 88, 24 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 8 } }, 8, 0, 1500, { .fields = { 88, 248 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 32, -88, 1250, { .fields = { 8, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 32, -72, 1312, { .fields = { 32, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 32, -56, 1325, { .fields = { 112, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, 32, -40, 1375, { .fields = { 8, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 8 } }, 32, -8, 1500, { .fields = { 112, 248 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 8 } }, 8, -24, 1337, { .fields = { 64, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 8 } }, 32, -24, 1500, { .fields = { 64, 144 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 8 } }, 8, -16, 1375, { .fields = { 80, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 8 } }, 32, -16, 1500, { .fields = { 64, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 48, 32 } }, -96, 88, 500, { .fields = { 80, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 40 } }, -48, 80, 500, { .fields = { 56, 216 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 48 } }, -24, 72, 500, { .fields = { 40, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 56 } }, 0, 64, 500, { .fields = { 80, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 64 } }, 24, 56, 500, { .fields = { 96, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 72 } }, 56, 48, 500, { .fields = { 104, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 48 } }, 80, 40, 500, { .fields = { 56, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 56 } }, 104, 32, 500, { .fields = { 80, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 48, 32 } }, 80, 88, 625, { .fields = { 88, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 48 } }, 128, 40, 500, { .fields = { 48, 168 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 32 } }, 128, 88, 500, { .fields = { 112, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 5's ranges [0, 10), [10, 41) and [41, 52), keeping background strips.
static SpriteBatch _gDryfieldSaloonGRView5SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 31, 0, 0, { 2, 0 } },
    { 41, ARRAY_SIZE(_gDryfieldSaloonGRView5SpriteSources) - 41, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 6's 91 texture rectangles, in pixels with unscaled sorting depths.
static SpriteSource _gDryfieldSaloonGRView6SpriteSources[91] = {
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 24 } }, -144, 96, 375, { .fields = { 0, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, -128, 80, 375, { .fields = { 64, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, -112, 72, 375, { .fields = { 96, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, -96, 72, 375, { .fields = { 96, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, -80, 72, 375, { .fields = { 88, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, -64, 72, 375, { .fields = { 88, 152 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 88, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, -32, 80, 375, { .fields = { 64, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, -16, 80, 375, { .fields = { 64, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, 0, 80, 250, { .fields = { 64, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, 16, 72, 250, { .fields = { 104, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, 32, 72, 250, { .fields = { 104, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, 48, 80, 250, { .fields = { 64, 40 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, 64, 80, 250, { .fields = { 48, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, 80, 80, 250, { .fields = { 48, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, 96, 72, 250, { .fields = { 112, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 40 } }, 112, 80, 250, { .fields = { 48, 40 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 32 } }, 128, 88, 250, { .fields = { 16, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 32 } }, 144, 88, 156, { .fields = { 16, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -88, -56, 742, { .fields = { 120, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -88, -40, 750, { .fields = { 40, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 8 } }, -72, -8, 689, { .fields = { 88, 248 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, -80, -24, 843, { .fields = { 104, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -80, -64, 547, { .fields = { 56, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -72, -64, 541, { .fields = { 0, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -64, -64, 589, { .fields = { 120, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -56, -56, 598, { .fields = { 112, 16 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -64, -40, 645, { .fields = { 16, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -56, -40, 723, { .fields = { 80, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -48, -40, 686, { .fields = { 80, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -80, -40, 751, { .fields = { 120, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -72, -40, 576, { .fields = { 120, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -72, 56, 549, { .fields = { 8, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -88, 24, 460, { .fields = { 24, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -96, 16, 384, { .fields = { 16, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -104, 0, 386, { .fields = { 16, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -112, -8, 354, { .fields = { 24, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -120, -24, 301, { .fields = { 24, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -128, -40, 295, { .fields = { 32, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -136, -48, 293, { .fields = { 40, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -144, -64, 276, { .fields = { 40, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -152, -80, 261, { .fields = { 48, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 48 } }, -160, -104, 247, { .fields = { 104, 208 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, -136, -120, 299, { .fields = { 112, 216 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 16 } }, -160, -120, 258, { .fields = { 96, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, -152, -104, 267, { .fields = { 104, 152 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, -120, -120, 325, { .fields = { 104, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -104, -120, 562, { .fields = { 48, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 24 } }, -64, -24, 641, { .fields = { 0, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -48, -24, 748, { .fields = { 80, 40 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -40, -32, 698, { .fields = { 32, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 24 } }, -64, -48, 649, { .fields = { 120, 152 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 32 } }, -40, -64, 866, { .fields = { 32, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 24 } }, -40, -88, 919, { .fields = { 120, 176 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 32 } }, -40, -120, 827, { .fields = { 24, 64 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 32 } }, -72, -120, 492, { .fields = { 16, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 40 } }, -72, -88, 564, { .fields = { 56, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -88, -96, 625, { .fields = { 0, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -96, -120, 423, { .fields = { 0, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -88, -120, 453, { .fields = { 48, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -80, -120, 490, { .fields = { 80, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -160, -56, 246, { .fields = { 88, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -152, -48, 259, { .fields = { 40, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -144, -32, 271, { .fields = { 80, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -136, -16, 272, { .fields = { 16, 224 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -128, -8, 307, { .fields = { 56, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -120, 8, 328, { .fields = { 8, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 48 } }, -160, -16, 239, { .fields = { 112, 112 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 24 } }, -144, 8, 252, { .fields = { 0, 128 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 56 } }, -128, 32, 324, { .fields = { 112, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 32 } }, -160, 32, 260, { .fields = { 32, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 24 } }, -160, 64, 262, { .fields = { 112, 32 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -72, -48, 625, { .fields = { 8, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -72, -8, 625, { .fields = { 112, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -72, -24, 625, { .fields = { 112, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -80, -32, 625, { .fields = { 112, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -80, -48, 625, { .fields = { 72, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -88, -56, 625, { .fields = { 64, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -96, -80, 500, { .fields = { 80, 240 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -96, -96, 625, { .fields = { 120, 120 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 16, 16 } }, -64, 0, 731, { .fields = { 112, 104 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -80, -64, 625, { .fields = { 120, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -80, -80, 625, { .fields = { 88, 136 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -88, -72, 625, { .fields = { 120, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 56 } }, -112, 24, 358, { .fields = { 120, 56 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 48 } }, -104, 32, 386, { .fields = { 120, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 32 } }, -96, 48, 420, { .fields = { 32, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 24 } }, -88, 56, 448, { .fields = { 32, 232 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 40 } }, -80, 40, 492, { .fields = { 88, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 8, 16 } }, -64, 64, 613, { .fields = { 120, 88 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 32 } }, -160, 88, 274, { .fields = { 0, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 6's ranges [0, 19), [19, 32) and [32, 91), keeping background strips.
static SpriteBatch _gDryfieldSaloonGRView6SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 13, 0, 0, { 2, 0 } },
    { 32, ARRAY_SIZE(_gDryfieldSaloonGRView6SpriteSources) - 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 7's 19 texture rectangles, in pixels with unscaled sorting depths.
static SpriteSource _gDryfieldSaloonGRView7SpriteSources[19] = {
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 80 } }, 104, -32, 550, { .fields = { 48, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 56, 72 } }, 104, 48, 550, { .fields = { 40, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 72 } }, 64, -8, 525, { .fields = { 8, 72 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 56 } }, 64, 64, 525, { .fields = { 0, 144 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 64, 56 } }, 0, -16, 500, { .fields = { 104, 200 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 64, 48 } }, 0, 40, 500, { .fields = { 64, 144 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 64, 32 } }, 0, 88, 500, { .fields = { 40, 192 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 80 } }, -32, -32, 500, { .fields = { 96, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 72 } }, -32, 48, 500, { .fields = { 72, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 80 } }, -56, -40, 550, { .fields = { 104, 80 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 80 } }, -56, 40, 550, { .fields = { 104, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 72 } }, -88, -40, 625, { .fields = { 40, 160 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_RIGHT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 64 } }, -88, 32, 625, { .fields = { 8, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 40, 48 } }, -128, 8, 750, { .fields = { 64, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 40 } }, -112, -32, 750, { .fields = { 80, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 48 } }, -56, -80, 1550, { .fields = { 104, 96 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 32, 48 } }, -56, -32, 1550, { .fields = { 104, 48 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 48 } }, -24, -32, 1550, { .fields = { 112, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_SALOON_G_R_SPRITE_LEFT_PAGE, DRYFIELD_SALOON_G_R_SPRITE_CLUT_ROW255, { .fields = { 24, 48 } }, -24, -80, 1550, { .fields = { 88, 0 } }, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, DRYFIELD_SALOON_G_R_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Mapped view 7's ranges [0, 15) and [15, 19), keeping decoded background strips.
static SpriteBatch _gDryfieldSaloonGRView7SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, ARRAY_SIZE(_gDryfieldSaloonGRView7SpriteSources) - 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 8's empty sprite list, preserving the decoded background strips.
static SpriteBatch _gDryfieldSaloonGRView8SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 9's empty sprite list, shared by logical views 9..12.
static SpriteBatch _gDryfieldSaloonGRView9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource slot 10's empty sprite list, unselected by the current logical-view map.
static SpriteBatch _gDryfieldSaloonGRView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource slot 11's empty sprite list, unselected by the current logical-view map.
static SpriteBatch _gDryfieldSaloonGRView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Resource slot 12's empty sprite list, unselected by the current logical-view map.
static SpriteBatch _gDryfieldSaloonGRView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Mapped view 13's empty sprite list, preserving the decoded background strips.
static SpriteBatch _gDryfieldSaloonGRView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldSaloonGRSpriteViews[13] = {
    { { .empty = _gDryfieldSaloonGRView1SpriteBatches }, _gDryfieldSaloonGRView1SpriteBatches, NULL },
    { { .elements = _gDryfieldSaloonGRView2SpriteSources }, _gDryfieldSaloonGRView2SpriteBatches, NULL },
    { { .elements = _gDryfieldSaloonGRView3SpriteSources }, _gDryfieldSaloonGRView3SpriteBatches, NULL },
    { { .elements = _gDryfieldSaloonGRView4SpriteSources }, _gDryfieldSaloonGRView4SpriteBatches, NULL },
    { { .elements = _gDryfieldSaloonGRView5SpriteSources }, _gDryfieldSaloonGRView5SpriteBatches, NULL },
    { { .elements = _gDryfieldSaloonGRView6SpriteSources }, _gDryfieldSaloonGRView6SpriteBatches, NULL },
    { { .elements = _gDryfieldSaloonGRView7SpriteSources }, _gDryfieldSaloonGRView7SpriteBatches, NULL },
    { { .empty = _gDryfieldSaloonGRView8SpriteBatches }, _gDryfieldSaloonGRView8SpriteBatches, NULL },
    { { .empty = _gDryfieldSaloonGRView9SpriteBatches }, _gDryfieldSaloonGRView9SpriteBatches, NULL },
    { { .empty = _gDryfieldSaloonGRView10SpriteBatches }, _gDryfieldSaloonGRView10SpriteBatches, NULL },
    { { .empty = _gDryfieldSaloonGRView11SpriteBatches }, _gDryfieldSaloonGRView11SpriteBatches, NULL },
    { { .empty = _gDryfieldSaloonGRView12SpriteBatches }, _gDryfieldSaloonGRView12SpriteBatches, NULL },
    { { .empty = _gDryfieldSaloonGRView13SpriteBatches }, _gDryfieldSaloonGRView13SpriteBatches, NULL },
};

/// Sixteen directed view boundaries shared by both daytime saloon room selectors.
///
/// Source/destination logical views are 2..7, independent of camera mapping.
/// Room setup binds coordinates and links/enables records through the final
/// LAST-marked boundary. Positions/radii use room game units; normals use ONE.
/// Runtime links and hit latches require writable storage until unlinked.
static WorldCollisionTrigger _gDryfieldSaloonGRViewBoundaryTriggers[16] = {
    { NULL, NULL, NULL, { 2633, -1376, -1224, 0 }, { { -2531, -2400, -372, 0 }, { 2524, -2400, 366, 0 }, { -2531, 2400, -372, 0 }, { 2524, 2400, 366, 0 } }, { 592, 0, -4063, 0 }, { 0, 0, ONE, 0 }, 3500, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2782, -1248, -1346, 0 }, { { 2519, -2272, 364, 0 }, { -2534, -2272, -375, 0 }, { 2519, 2272, 364, 0 }, { -2534, 2272, -375, 0 } }, { -594, 0, 4058, 0 }, { 0, 0, ONE, 0 }, 3415, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2005, -1152, 53, 0 }, { { 1454, -2176, 297, 0 }, { -1453, -2176, -297, 0 }, { 1454, 2176, 297, 0 }, { -1453, 2176, -297, 0 } }, { -823, 0, 4019, 0 }, { 0, 0, ONE, 0 }, 2623, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1995, -1280, 172, 0 }, { { -1438, -2304, -264, 0 }, { 1439, -2304, 264, 0 }, { -1438, 2304, -264, 0 }, { 1439, 2304, 264, 0 } }, { 739, 0, -4031, 0 }, { 0, 0, ONE, 0 }, 2721, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1801, -1520, 3012, 0 }, { { -878, -2544, -837, 0 }, { 878, -2544, 837, 0 }, { -878, 2544, -837, 0 }, { 878, 2544, 837, 0 } }, { 2831, 0, -2972, 0 }, { 0, 0, ONE, 0 }, 2816, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1185, -1632, 3294, 0 }, { { -299, -2656, 1167, 0 }, { 295, -2656, -1172, 0 }, { -299, 2656, 1167, 0 }, { 295, 2656, -1172, 0 } }, { -3977, 0, -1011, 0 }, { 0, 0, ONE, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1913, -1456, 2983, 0 }, { { 801, -2480, 790, 0 }, { -800, -2480, -790, 0 }, { 801, 2480, 790, 0 }, { -800, 2480, -790, 0 } }, { -2881, 0, 2916, 0 }, { 0, 0, ONE, 0 }, 2721, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1377, -1344, 3391, 0 }, { { 300, -2368, -1106, 0 }, { -309, -2368, 1096, 0 }, { 300, 2368, -1106, 0 }, { -309, 2368, 1096, 0 } }, { 3955, 0, 1093, 0 }, { 0, 0, ONE, 0 }, 2623, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 431, -1552, 5183, 0 }, { { -1068, -2576, -9, 0 }, { 1069, -2576, 10, 0 }, { -1068, 2576, -9, 0 }, { 1069, 2576, 10, 0 } }, { 35, 0, -4101, 0 }, { 0, 0, ONE, 0 }, 2780, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 383, -1376, 5055, 0 }, { { 1069, -2400, 0, 0 }, { -1069, -2400, 0, 0 }, { 1069, 2400, 0, 0 }, { -1069, 2400, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, ONE, 0 }, 2623, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2906, -1200, 4312, 0 }, { { 1, -2224, -1068, 0 }, { -1, -2224, 1068, 0 }, { 1, 2224, -1068, 0 }, { -1, 2224, 1068, 0 } }, { ONE, 0, 3, 0 }, { 0, 0, ONE, 0 }, 2455, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3066, -1216, 4376, 0 }, { { -1, -2240, 1068, 0 }, { 1, -2240, -1068, 0 }, { -1, 2240, 1068, 0 }, { 1, 2240, -1068, 0 } }, { -4104, 0, -6, 0 }, { 0, 0, ONE, 0 }, 2468, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 158, -1216, -3715, 0 }, { { 4, -2272, -2047, 0 }, { -13, -2272, 2033, 0 }, { 4, 2272, -2047, 0 }, { -13, 2272, 2033, 0 } }, { 4099, 0, 16, 0 }, { 0, 0, ONE, 0 }, 3050, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 303, -1216, -3729, 0 }, { { 11, -2272, 1935, 0 }, { -21, -2272, -1950, 0 }, { 11, 2272, 1935, 0 }, { -21, 2272, -1950, 0 } }, { -4121, 0, 33, 0 }, { 0, 0, ONE, 0 }, 2985, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2096, -1248, -193, 0 }, { { -1157, -2304, -193, 0 }, { 1157, -2304, 194, 0 }, { -1157, 2304, -193, 0 }, { 1157, 2304, 194, 0 } }, { 675, 0, -4046, 0 }, { 0, 0, ONE, 0 }, 2585, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2016, -1280, -289, 0 }, { { 1157, -2304, 194, 0 }, { -1157, -2304, -193, 0 }, { 1157, 2304, 194, 0 }, { -1157, 2304, -193, 0 } }, { -678, 0, 4044, 0 }, { 0, 0, ONE, 0 }, 2585, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// CAP command indices, room command ID and packed warp slots used by action triggers.
///
/// Warp slots pack departure in the high nibble and arrival in the low nibble.
enum {
    DRYFIELD_SALOON_G_R_CAP_COMMAND_1          = 1,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_5          = 5,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_6          = 6,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_11         = 11,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_12         = 12,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_13         = 13,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_14         = 14,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_15         = 15,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_16         = 16,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_17         = 17,
    DRYFIELD_SALOON_G_R_CAP_COMMAND_18         = 18,
    DRYFIELD_SALOON_G_R_ROOM_COMMAND_CAP_4     = 4,
    DRYFIELD_SALOON_G_R_PARKING_LOT_WARP_SLOTS = (1 << 4) | 4,
    DRYFIELD_SALOON_G_R_KITCHEN_WARP_SLOTS     = (2 << 4) | 1,
};

/// Fourteen player-action quads shared by both daytime saloon room selectors.
///
/// The first two request the parking lot and kitchen through warp slots 1/2.
/// The third sends room command 4 only outside battle; the rest play CAP
/// commands 5, 11, 1, 12, 13, 6, 14, 16, 17, 18 and 15 with zero flags.
/// Room setup binds and links writable records through the final LAST flag.
/// Positions/radii use room game units; normals use ONE (4096) per unit.
static WorldCollisionTrigger _gDryfieldSaloonGRActionTriggers[14] = {
    { NULL, NULL, NULL, { 2464, -48, -4720, 0 }, { { 624, 0, -272, 0 }, { 624, 0, 272, 0 }, { -624, 0, -272, 0 }, { -624, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, ONE, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_PARKING_LOT, DRYFIELD_SALOON_G_R_PARKING_LOT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4480, -48, 4368, 0 }, { { -448, 0, -624, 0 }, { 448, 0, -624, 0 }, { -448, 0, 624, 0 }, { 448, 0, 624, 0 } }, { 0, 4117, 0, 0 }, { -ONE, 0, 0, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_G_R_KITCHEN, DRYFIELD_SALOON_G_R_KITCHEN_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3232, -64, 3392, 0 }, { { 624, 0, -880, 0 }, { 624, 0, 880, 0 }, { -624, 0, -880, 0 }, { -624, 0, 880, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, DRYFIELD_SALOON_G_R_ROOM_COMMAND_CAP_4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -32, -64, 1728, 0 }, { { 2032, 0, -1392, 0 }, { 2032, 0, 1392, 0 }, { -2032, 0, -1392, 0 }, { -2032, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4076, 0, 401, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4048, -64, -4672, 0 }, { { 896, 0, -848, 0 }, { 896, 0, 848, 0 }, { -896, 0, -848, 0 }, { -896, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { -1380, 0, 3856, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 6432, 0 }, { { 464, 0, -272, 0 }, { 464, 0, 272, 0 }, { -464, 0, -272, 0 }, { -464, 0, 272, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -ONE, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4416, -64, -1840, 0 }, { { 768, 0, -640, 0 }, { 768, 0, 640, 0 }, { -768, 0, -640, 0 }, { -768, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { -4076, 0, -402, 0 }, 999, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2432, -64, -1120, 0 }, { { 768, 0, -1312, 0 }, { 768, 0, 1312, 0 }, { -768, 0, -1312, 0 }, { -768, 0, 1312, 0 } }, { 0, 4099, 0, 0 }, { 4095, 0, 0, 0 }, 1519, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4448, -64, 672, 0 }, { { 768, 0, -1088, 0 }, { 768, 0, 1088, 0 }, { -768, 0, -1088, 0 }, { -768, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { -4091, 0, 200, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3680, -64, 1616, 0 }, { { 480, 0, -2016, 0 }, { 480, 0, 2016, 0 }, { -480, 0, -2016, 0 }, { -480, 0, 2016, 0 } }, { 0, 4095, 0, 0 }, { 4095, 0, 0, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2544, -64, 4928, 0 }, { { 1296, 0, -560, 0 }, { 1296, 0, 560, 0 }, { -1296, 0, -560, 0 }, { -1296, 0, 560, 0 } }, { 0, 4101, 0, 0 }, { -201, 0, -4091, 0 }, 1408, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1184, -64, 4320, 0 }, { { 752, 0, -560, 0 }, { 752, 0, 560, 0 }, { -752, 0, -560, 0 }, { -752, 0, 560, 0 } }, { 0, 4106, 0, 0 }, { -201, 0, -4091, 0 }, 936, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2784, -64, 3680, 0 }, { { 976, 0, -560, 0 }, { 976, 0, 560, 0 }, { -976, 0, -560, 0 }, { -976, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, -4091, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4416, -64, 2624, 0 }, { { 768, 0, -1088, 0 }, { 768, 0, 1088, 0 }, { -768, 0, -1088, 0 }, { -768, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { -4091, 0, 200, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_SALOON_G_R_CAP_COMMAND_15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space sight-blocking quads shared by both daytime saloon selectors.
///
/// Origins/corners and radii use game units; normals use ONE per unit. Setup
/// links and enables both records, including the LAST-marked second quad.
/// Low flag bit 0 is authored but has no established interpretation; preserve it.
/// Runtime list links require writable storage until the occluders are retired.
static WorldCollisionOccluder _gDryfieldSaloonGRSightOccluders[2] = {
    { NULL, NULL, { -1056, -1392, 5216, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, 2442, 1, 0 },
    { NULL, NULL, { 2048, -1376, 5215, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, 2442, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Seven authored point lights shared by both daytime saloon room selectors.
///
/// Positions and falloff radii use room-coordinate units; RGB intensities have
/// 12 fractional bits. Every source is enabled in all logical views.
/// Coordinate parenting/composition and query attenuation keep these writable
/// for the loaded overlay's lifetime.
static WorldCoordPointLight _gDryfieldSaloonGRPointLights[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE / 2, ONE / 2, ONE / 2 }, { 0, 0 } }, 100000, 100000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 570, -1500, 5040 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2867, ONE / 2 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1210, -1500, -2790 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2560, -1500, -3820 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, ONE, ONE }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3890, -1600, 1450 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3031, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4750, -1400, 4320 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2830, -1000, 3150 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3276, ONE / 2 }, { 0, 0 } }, 500, 2000 },
};

/// The saloon's single light collection: seven point sources and no directional or cone lights.
static WorldCoordRoomLights _gDryfieldSaloonGRRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gDryfieldSaloonGRPointLights),
    _gDryfieldSaloonGRPointLights,
    0,
    NULL,
};

/// Placement keys and decimal file numbers used by the two saloon enemy kinds.
enum {
    DRYFIELD_SALOON_G_R_MIND_SUCKLER_PLACEMENT_ENTRY = 15,
    DRYFIELD_SALOON_G_R_MIND_SUCKLER_FILE_NUMBER     = 15,
    DRYFIELD_SALOON_G_R_BONE_SUCKLER_PLACEMENT_ENTRY = 12,
    DRYFIELD_SALOON_G_R_BONE_SUCKLER_FILE_NUMBER     = 12,
};

/// Variant 1's Mind Suckler loading/spawn recipe, followed by AREA_PLACEMENT_END.
///
/// Placement entry 15 selects global file 101500 and descriptor 0. The five
/// matching map placements use this overlay's resource recipe during loading
/// and spawning; keep it loaded through those operations.
static AreaResource _gDryfieldSaloonGRVariant1Resources[2] = {
    { DRYFIELD_SALOON_G_R_MIND_SUCKLER_PLACEMENT_ENTRY, DRYFIELD_SALOON_G_R_MIND_SUCKLER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 2's Mind Suckler and Bone Suckler recipes, followed by AREA_PLACEMENT_END.
///
/// Entry 15 selects file 101500; entry 12 selects file 201200. Both use
/// descriptor 0. The two Mind Suckler and five Bone Suckler map placements
/// borrow these overlay-owned recipes during loading and spawning.
static AreaResource _gDryfieldSaloonGRVariant2Resources[3] = {
    { DRYFIELD_SALOON_G_R_MIND_SUCKLER_PLACEMENT_ENTRY, DRYFIELD_SALOON_G_R_MIND_SUCKLER_FILE_NUMBER, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { DRYFIELD_SALOON_G_R_BONE_SUCKLER_PLACEMENT_ENTRY, DRYFIELD_SALOON_G_R_BONE_SUCKLER_FILE_NUMBER, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor201200BoneSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldSaloonGRAreaVariants[DRYFIELD_SALOON_G_R_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapDryfieldSaloonGRVariant1Placements, .resources = _gDryfieldSaloonGRVariant1Resources },
    [2]                    = { .placements = gMapDryfieldSaloonGRVariant2Placements, .resources = _gDryfieldSaloonGRVariant2Resources },
};

/// Cue-2 bases in the loaded type-1 footstep bank; cue 1 uses the next entry.
enum {
    DRYFIELD_SALOON_G_R_CLASS1_WALK_SOUND        = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x35,
    DRYFIELD_SALOON_G_R_CLASS1_RUN_SOUND         = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x37,
    DRYFIELD_SALOON_G_R_CLASSES2_AND3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    DRYFIELD_SALOON_G_R_CLASSES2_AND3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
};

/// Surface class 1's walk, run and stair-climb footstep bases.
///
/// Walking and stair climbing share entries 0x35/0x36; running uses 0x37/0x38.
/// Requests select the currently loaded type-1 bank.
static const WorldCollisionFootstepSounds _gDryfieldSaloonGRClass1FootstepSounds __attribute__((section(".data"))) = {
    DRYFIELD_SALOON_G_R_CLASS1_WALK_SOUND,
    DRYFIELD_SALOON_G_R_CLASS1_RUN_SOUND,
    DRYFIELD_SALOON_G_R_CLASS1_WALK_SOUND,
};

/// Surface classes 2 and 3's shared walk, run and stair-climb footstep bases.
///
/// Walking and stair climbing share entries 1/2; running uses 3/4 in the loaded
/// type-1 bank. A surface class may come from wall contacts as well as floors.
static const WorldCollisionFootstepSounds _gDryfieldSaloonGRClasses2And3FootstepSounds __attribute__((section(".data"))) = {
    DRYFIELD_SALOON_G_R_CLASSES2_AND3_WALK_SOUND,
    DRYFIELD_SALOON_G_R_CLASSES2_AND3_RUN_SOUND,
    DRYFIELD_SALOON_G_R_CLASSES2_AND3_WALK_SOUND,
};

/// Classes 0 and 4..7 block probes, allow weapon impacts and apply pushback without footsteps.
static WorldCollisionSurfaceProperties _gDryfieldSaloonGRDefaultSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Class 1 blocks probes, allows weapon impacts and applies pushback with its own footsteps.
static WorldCollisionSurfaceProperties _gDryfieldSaloonGRClass1SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldSaloonGRClass1FootstepSounds,
};

/// Class 2 blocks probes, allows weapon impacts and applies pushback with the shared class-2/3 footsteps.
static WorldCollisionSurfaceProperties _gDryfieldSaloonGRClass2SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldSaloonGRClasses2And3FootstepSounds,
};

/// Class 3 passes probes and ignores weapon impacts while retaining pushback and class-2/3 footsteps.
static WorldCollisionSurfaceProperties _gDryfieldSaloonGRClass3SurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_PASS_PROBES,
    WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gDryfieldSaloonGRClasses2And3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldSaloonGRSurfacePropertiesByClass[8] = {
    &_gDryfieldSaloonGRDefaultSurfaceProperties,
    &_gDryfieldSaloonGRClass1SurfaceProperties,
    &_gDryfieldSaloonGRClass2SurfaceProperties,
    &_gDryfieldSaloonGRClass3SurfaceProperties,
    &_gDryfieldSaloonGRDefaultSurfaceProperties,
    &_gDryfieldSaloonGRDefaultSurfaceProperties,
    &_gDryfieldSaloonGRDefaultSurfaceProperties,
    &_gDryfieldSaloonGRDefaultSurfaceProperties,
};

/// Snapshot of the accepted transition for the deferred room event.
///
/// The gate copies all eight bytes before spawning; another accepted start
/// replaces them. The task commits area, arrival and the room byte on reload.
/// Keep this room loaded and the snapshot unchanged until that task finishes.
static RoomEventMsg _gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 32, 1, 0 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room/room_event.inc.c"

/// Daytime saloon room-task states: initialize, idle and teardown.
///
/// Indexed by `Task::state` in 0..2, with no terminator. Dispatch copies all
/// three callbacks by value; the overlay's callbacks must remain loaded.
static const TaskFuncTable3 _gDryfieldSaloonGRRoomTaskStates = {
    { _dryfieldSaloonGRInitializeRoom, _dryfieldSaloonGRIdleTask, taskKill },
};

#include "../../shared/room/room_variants_saloon.inc.c"

static void _glowDrawFlareLocal(const GfxCoord* coord, const SVECTOR* localPoint, s32 textureIndex, s32 radiusScale);

/// Refuses every key-item use request in the saloon.
///
/// Ignores the selected item ID and returns zero, so the item menu reports
/// that the item cannot be used here. Does not consume or retain any payload.
static s32 _dryfieldSaloonGRRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Plays CAP command 4 when the saloon receives room command 4.
///
/// Requires loaded, relocated saloon CAP resources. Playback uses a display
/// transition; other commands do nothing. Always returns zero, irrespective
/// of playback starting, and retains no payload.
static s32 _dryfieldSaloonGRCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { DRYFIELD_SALOON_G_R_CAP_COMMAND_4 = 4 };

    if (commandId == DRYFIELD_SALOON_G_R_CAP_COMMAND_4) {
        capRunCommandWithTransition(DRYFIELD_SALOON_G_R_CAP_COMMAND_4);
    }
    return 0;
}

/// Leaves trigger-driven room-action requests without an action in the saloon.
///
/// The synchronous sender supplies a borrowed request and a zero second
/// payload word. Neither payload is read or retained. Returns zero; the
/// direction dispatcher discards the result.
static s32 _dryfieldSaloonGRIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the daytime saloon's room-message receiver and enters its idle state.
///
/// Requires the live room task in state 0. The message table and room slot
/// borrow the loaded overlay and task for subsequent synchronous dispatch.
static void _dryfieldSaloonGRInitializeRoom(Task* task)
{
    task->msgTable = _gDryfieldSaloonGRRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Keeps the saloon room task idle while its message table remains installed.
///
/// This state performs no frame work and leaves the task's state unchanged.
static void _dryfieldSaloonGRIdleTask(Task* task)
{
}

void dryfieldSaloonGRRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldSaloonGRRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void dryfieldSaloonGRDrawLightEffectsTask(Task* task)
{
    enum {
        DRYFIELD_SALOON_G_R_FIRST_FLARE_GROUP_END       = 6,
        DRYFIELD_SALOON_G_R_FLARE_COUNT                 = 11,
        DRYFIELD_SALOON_G_R_FIRST_FLARE_TEXTURE_COLUMN  = 0,
        DRYFIELD_SALOON_G_R_SECOND_FLARE_TEXTURE_COLUMN = 2,
        DRYFIELD_SALOON_G_R_FLARE_RADIUS_SCALE          = 512,
        DRYFIELD_SALOON_G_R_SHAFT_VIEW_MASK_INDEX       = 12,
        DRYFIELD_SALOON_G_R_BEAM_VIEW_MASK_INDEX        = 11,
        DRYFIELD_SALOON_G_R_BEAM_START_POINT            = 13,
        DRYFIELD_SALOON_G_R_BEAM_END_POINT              = 12,
        DRYFIELD_SALOON_G_R_BEAM_RADIUS_SCALE           = 256,
    };

    const GfxCoord* roomCoord;
    s32             viewMask;
    s32             flareIndex;

    /// Draws a half-open group of view-visible flares using one atlas column.
    ///
    /// Captures roomCoord, viewMask, flareIndex, the room's point/mask tables
    /// and the fixed flare scale. Writes flareIndex through the range's end.
    /// Arguments must be pure signed-integer values with 0 <= firstPoint <=
    /// pointLimit <= DRYFIELD_SALOON_G_R_FLARE_COUNT: firstPoint is evaluated
    /// once, pointLimit on every loop test and textureColumn for each visible
    /// point. Expands to one for statement; invoke inside a braced block.
#define DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE(firstPoint, pointLimit, textureColumn)       \
    for (flareIndex = (firstPoint); flareIndex < (pointLimit); flareIndex++) {            \
        if (viewMask & _gDryfieldSaloonGREffectViewMasks[flareIndex]) {                   \
            _glowDrawFlareLocal(roomCoord, &_gDryfieldSaloonGREffectPoints[flareIndex],   \
                                (textureColumn), DRYFIELD_SALOON_G_R_FLARE_RADIUS_SCALE); \
        }                                                                                 \
    }

    roomCoord = task->extra.coordBody->coord;
    viewMask  = 1 << gGameSession->location.view;

    // The two flare groups use separate columns of the same texture atlas.
    DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE(0, DRYFIELD_SALOON_G_R_FIRST_FLARE_GROUP_END,
                                         DRYFIELD_SALOON_G_R_FIRST_FLARE_TEXTURE_COLUMN);
    DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE(DRYFIELD_SALOON_G_R_FIRST_FLARE_GROUP_END, DRYFIELD_SALOON_G_R_FLARE_COUNT,
                                         DRYFIELD_SALOON_G_R_SECOND_FLARE_TEXTURE_COLUMN);
#undef DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE

    // Shafts share a visibility mask; endpoint order fixes the beam's orientation.
    if (viewMask & _gDryfieldSaloonGREffectViewMasks[DRYFIELD_SALOON_G_R_SHAFT_VIEW_MASK_INDEX]) {
        _glowDrawTwinShafts(roomCoord);
    }
    if (viewMask & _gDryfieldSaloonGREffectViewMasks[DRYFIELD_SALOON_G_R_BEAM_VIEW_MASK_INDEX]) {
        _glowDrawTaperedBeam(roomCoord, &_gDryfieldSaloonGREffectPoints[DRYFIELD_SALOON_G_R_BEAM_START_POINT],
                             &_gDryfieldSaloonGREffectPoints[DRYFIELD_SALOON_G_R_BEAM_END_POINT], DRYFIELD_SALOON_G_R_BEAM_RADIUS_SCALE);
    }
}

#include "../../shared/effects/glow_draw_flare_local.inc.c"

/// Selects the day saloon's local shaft roots and guides for glow drawing.
///
/// Requires a word-aligned `SVECTOR` array or parenthesized pointer expression
/// with at least 20 complete entries. Only entries 14..19 are read: roots
/// 14/17 and guide pairs 15/18, 16/19, in the local game-coordinate units
/// of the drawer's `coord`. Tips extend each root-to-guide offset fourfold.
/// The expression is expanded repeatedly and must be stable and free of side
/// effects. Drawing leaves the table unchanged; it must stay live for each call.
/// Bind around `glow_draw_twin_shafts.inc.c` and undefine after inclusion.
#define GLOW_TWIN_SHAFT_POINTS (_gDryfieldSaloonGREffectPoints)
#include "../../shared/effects/glow_draw_twin_shafts.inc.c"
#undef GLOW_TWIN_SHAFT_POINTS

#include "../../shared/effects/glow_draw_tapered_beam.inc.c"
