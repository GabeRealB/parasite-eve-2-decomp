#include "rooms/shelter_b1_transfer_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_00300.h"
#include "actors/actor_02100.h"
#include "actors/actor_02300.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

static void _shelterB1TransferTunnelInitializeRoom(Task* task);
static void _shelterB1TransferTunnelIdle(Task* task);

/// Room-controller handlers for states 0 (register), 1 (idle) and 2 (release).
///
/// The controller copies all three entries before dispatch; its state must be 0..2.
static const TaskFuncTable3 _gShelterB1TransferTunnelRoomTaskStates = {
    { _shelterB1TransferTunnelInitializeRoom, _shelterB1TransferTunnelIdle, taskKill }
};

static s32 _shelterB1TransferTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _shelterB1TransferTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1TransferTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32 _shelterB1TransferTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

/// Synchronous room messages for transitions, key items, actions and commands.
///
/// The live room task borrows this table. Unsupported IDs return zero at the
/// end marker; transition payloads borrow a request and writable reply.
static TaskMessageEntry _gShelterB1TransferTunnelRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1TransferTunnelResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1TransferTunnelRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1TransferTunnelIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1TransferTunnelIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Four capsule endpoint pairs followed by the red-disc centre, in world units.
///
/// Pairs start at indices 0, 2, 4 and 6; index 8 is the disc. Views 2 and 3
/// use the array base, while view 4 starts at index 2. Drawers borrow points
/// synchronously and require both endpoints of each capsule to be readable.
static SVECTOR _gShelterB1TransferTunnelGlowPoints[9] = {
    { 5017, -196, 1184, 0 },
    { 4172, -196, 1184, 0 },
    { 2313, -196, 1184, 0 },
    { 1527, -196, 1184, 0 },
    { 5017, -196, -1028, 0 },
    { 4172, -196, -1028, 0 },
    { 2313, -196, -1028, 0 },
    { 1527, -196, -1028, 0 },
    { 6559, -1206, -1163, 0 },
};

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0x001F,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB1TransferTunnelRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB1TransferTunnelRoomViewCounts[1] = { ARRAY_SIZE(gShelterB1TransferTunnelViewCameras) };

enum {
    SHELTER_B1_TRANSFER_TUNNEL_MAIN_CORRIDOR_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 2),
    SHELTER_B1_TRANSFER_TUNNEL_MAIN_CORRIDOR_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 1),
    SHELTER_B1_TRANSFER_TUNNEL_ACCESS_TUNNEL_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 4),
    SHELTER_B1_TRANSFER_TUNNEL_ACCESS_TUNNEL_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL, 3),
};

DirectionWarpEntry gShelterB1TransferTunnelWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 6214, 0, 60 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 5570, 0, 20 }, { 0, 0, 0, 0 }, SHELTER_B1_TRANSFER_TUNNEL_MAIN_CORRIDOR_ARRIVAL_SOUND, SHELTER_B1_TRANSFER_TUNNEL_MAIN_CORRIDOR_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_R47_1C6 },
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 971, 0, 18 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, 971, 0, 18 }, { 0, 0, 0, 0 }, SHELTER_B1_TRANSFER_TUNNEL_ACCESS_TUNNEL_ARRIVAL_SOUND, SHELTER_B1_TRANSFER_TUNNEL_ACCESS_TUNNEL_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Room-axis wall, ceiling and floor normals, with 4096 representing unit length.
static SVECTOR _gShelterB1TransferTunnelRoomCollisionNormals[6] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_normals.inc"
};

/// Collision-mesh vertices in signed world-coordinate units.
static SVECTOR _gShelterB1TransferTunnelRoomCollisionVerts[12] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_verts.inc"
};

/// Six quads: walls and ceiling in surface class 0, floor in class 1.
static WorldCollisionGridFace _gShelterB1TransferTunnelRoomCollisionFaces[6] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_faces.inc"
};

/// Six terminated face-index lists concatenated in X-major cell order.
static s16 _gShelterB1TransferTunnelRoomCollisionCells[24] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1TransferTunnelRoomCollisionCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..23,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_TRANSFER_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1TransferTunnelRoomCollisionCells[(halfwordOffset)])
/// The 3-by-2 cell directory; each entry borrows a list ending at -1.
static s16* _gShelterB1TransferTunnelRoomCollisionTable[6] = {
#include "assets/shelter_b1_transfer_tunnel_collision_0552C_table.inc"
};
#undef SHELTER_B1_TRANSFER_TUNNEL_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1TransferTunnelRoomCollision = { NULL, _gShelterB1TransferTunnelRoomCollisionNormals, _gShelterB1TransferTunnelRoomCollisionVerts, _gShelterB1TransferTunnelRoomCollisionFaces, _gShelterB1TransferTunnelRoomCollisionTable, 642, 1736, 3, 2, 4000, ARRAY_SIZE(_gShelterB1TransferTunnelRoomCollisionFaces) };

ViewCamera gShelterB1TransferTunnelViewCameras[4] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -3722, 8164, -144 } }, 235 },
    { { { { 440, 0, -4072 }, { -84, 4095, -9 }, { 4071, 85, 439 } }, { -3438, 1111, 45 } }, 225 },
    { { { { 510, 0, -4064 }, { 176, 4092, 22 }, { 4060, -177, 510 } }, { -982, 799, 335 } }, 235 },
    { { { { 1227, 0, 3907 }, { -265, 4086, 83 }, { -3898, -278, 1224 } }, { -4511, 770, 374 } }, 235 },
};

/// View 1's empty sprite list, selecting decoded background image strips.
static SpriteBatch _gShelterB1TransferTunnelView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's empty sprite list, selecting decoded background image strips.
static SpriteBatch _gShelterB1TransferTunnelView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's empty sprite list, selecting decoded background image strips.
static SpriteBatch _gShelterB1TransferTunnelView3SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's empty sprite list, selecting decoded background image strips.
static SpriteBatch _gShelterB1TransferTunnelView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB1TransferTunnelSpriteViews[4] = {
    { { .empty = _gShelterB1TransferTunnelView1SpriteBatches }, _gShelterB1TransferTunnelView1SpriteBatches, NULL },
    { { .empty = _gShelterB1TransferTunnelView2SpriteBatches }, _gShelterB1TransferTunnelView2SpriteBatches, NULL },
    { { .empty = _gShelterB1TransferTunnelView3SpriteBatches }, _gShelterB1TransferTunnelView3SpriteBatches, NULL },
    { { .empty = _gShelterB1TransferTunnelView4SpriteBatches }, _gShelterB1TransferTunnelView4SpriteBatches, NULL },
};

/// Four authored point lights, active in every room view.
///
/// Positions and falloff radii use world units; RGB intensity has 12 fractional
/// bits. Room binding and lighting queries update transforms and attenuation.
static WorldCoordPointLight _gShelterB1TransferTunnelPointLights[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5609, -223, 118 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 799, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3663, -526, 7 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2720, 2900, 3122 }, { 0, 0 } }, 1460, 3061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2537, -85, -1 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2416, 2539, 2560 }, { 0, 0 } }, 1721, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1219, -223, -250 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1101, 2000 },
};

WorldCoordRoomLights gShelterB1TransferTunnelRoomLights = { 0, NULL, ARRAY_SIZE(_gShelterB1TransferTunnelPointLights), _gShelterB1TransferTunnelPointLights, 0, NULL };

WorldCollisionTrigger gShelterB1TransferTunnelViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { 5257, -576, 234, 0 }, { { 124, -2016, -2746, 0 }, { -123, -2016, 2746, 0 }, { 124, 2016, -2746, 0 }, { -123, 2016, 2746, 0 } }, { 4104, 0, 184, 0 }, { 0, 0, ONE, 0 }, 3405, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5441, -576, 272, 0 }, { { -164, -2016, 2742, 0 }, { 165, -2016, -2742, 0 }, { -164, 2016, 2742, 0 }, { 165, 2016, -2742, 0 } }, { -4100, 0, -247, 0 }, { 0, 0, ONE, 0 }, 3405, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2784, -512, 160, 0 }, { { -164, -2016, 2742, 0 }, { 165, -2016, -2742, 0 }, { -164, 2016, 2742, 0 }, { 165, 2016, -2742, 0 } }, { -4100, 0, -247, 0 }, { 0, 0, ONE, 0 }, 3405, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2560, -576, 128, 0 }, { { 124, -2016, -2746, 0 }, { -123, -2016, 2746, 0 }, { 124, 2016, -2746, 0 }, { -123, 2016, 2746, 0 } }, { 4104, 0, 184, 0 }, { 0, 0, ONE, 0 }, 3405, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gShelterB1TransferTunnelWarpTriggers[2] = {
    { NULL, NULL, NULL, { 6416, -48, 160, 0 }, { { -592, 0, -1024, 0 }, { 592, 0, -1024, 0 }, { -592, 0, 1024, 0 }, { 592, 0, 1024, 0 } }, { 0, 4109, 0, 0 }, { -ONE, 0, 0, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, (1 << 4) | 4, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 864, -48, 96, 0 }, { { -592, 0, -1024, 0 }, { 592, 0, -1024, 0 }, { -592, 0, 1024, 0 }, { 592, 0, 1024, 0 } }, { 0, 4109, 0, 0 }, { ONE, 0, 0, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, (2 << 4) | 4, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER      = 21,
    SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_ACTOR_200300 = 3,
    SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_ROOK_GOLEM   = 23,
};

/// Watcher and actor-200300 resources for area variant 1, followed by the end marker.
static AreaResource _gShelterB1TransferTunnelVariant1Resources[3] = {
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_ACTOR_200300, 3, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor200300Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Watcher and Rook GOLEM resources for area variant 11, followed by the end marker.
static AreaResource _gShelterB1TransferTunnelVariant11Resources[3] = {
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_ROOK_GOLEM, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Six elevated Watchers and one actor-200300 placement for area variant 1.
///
/// The eighth record ends the list. Positions use world units and yaw uses
/// 4096 units per turn; actor-specific variant, mode and row selectors are retained.
static AreaPlacement _gShelterB1TransferTunnelVariant1Placements[8] = {
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 3, 0, 600, -1900, 770, 1024, 0, 0, 2, 4 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 3, 0, 600, -1900, -700, 1024, 0, 0, 2, 4 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 1900, -1900, 970, 2048, 0, 0, 2, 2 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 1900, -1900, -800, 0, 0, 0, 2, 2 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 2700, -1900, 970, 2048, 0, 0, 2, 1 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 2700, -1900, -800, 0, 0, 0, 2, 1 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_ACTOR_200300, 0, 0, 4500, 0, 80, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Six elevated Watchers and one Rook GOLEM placement for area variant 11.
///
/// The eighth record ends the list. Positions use world units and yaw uses
/// 4096 units per turn; actor-specific variant, mode and row selectors are retained.
static AreaPlacement _gShelterB1TransferTunnelVariant11Placements[8] = {
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 3, 0, 600, -1900, 770, 1024, 0, 0, 2, 4 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 3, 0, 600, -1900, -700, 1024, 0, 0, 2, 4 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 1900, -1900, 970, 2048, 0, 0, 2, 2 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 1900, -1900, -800, 0, 0, 0, 2, 2 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 2700, -1900, 970, 2048, 0, 0, 2, 1 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_WATCHER, 1, 0, 2700, -1900, -800, 0, 0, 0, 2, 1 },
    { SHELTER_B1_TRANSFER_TUNNEL_RESOURCE_ROOK_GOLEM, 4, 1, 1500, 0, 0, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB1TransferTunnelAreaVariants[22] = {
    { NULL, NULL },
    { _gShelterB1TransferTunnelVariant1Placements, _gShelterB1TransferTunnelVariant1Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gShelterB1TransferTunnelVariant11Placements, _gShelterB1TransferTunnelVariant11Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

enum {
    SHELTER_B1_TRANSFER_TUNNEL_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B1_TRANSFER_TUNNEL_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Floor footstep cue-2 bases in the loaded type-1 bank; cue 1 adds one.
///
/// Walking and scripted jumping share a pair; running selects the next pair.
static WorldCollisionFootstepSounds _gShelterB1TransferTunnelFloorFootstepSounds = {
    SHELTER_B1_TRANSFER_TUNNEL_FOOTSTEP_WALK,
    SHELTER_B1_TRANSFER_TUNNEL_FOOTSTEP_RUN,
    SHELTER_B1_TRANSFER_TUNNEL_FOOTSTEP_WALK,
};

/// Blocking surface policy for classes 0 and 2..7, without footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1TransferTunnelDefaultSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    NULL,
};

/// Blocking floor policy for class 1, with walking and running footstep cues.
static WorldCollisionSurfaceProperties _gShelterB1TransferTunnelFloorSurface = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gShelterB1TransferTunnelFloorFootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1TransferTunnelSurfacePropertiesByClass[8] = {
    &_gShelterB1TransferTunnelDefaultSurface,
    &_gShelterB1TransferTunnelFloorSurface,
    &_gShelterB1TransferTunnelDefaultSurface,
    &_gShelterB1TransferTunnelDefaultSurface,
    &_gShelterB1TransferTunnelDefaultSurface,
    &_gShelterB1TransferTunnelDefaultSurface,
    &_gShelterB1TransferTunnelDefaultSurface,
    &_gShelterB1TransferTunnelDefaultSurface,
};

/// Refuses every key item without changing the room, returning zero to the item menu.
///
/// `itemId` is a collected item ID; neither payload word is read or retained.
static s32 _shelterB1TransferTunnelRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Accepts a Mine/Shelter transition and resolves its destination room variant.
///
/// Copies the complete eight-byte request into the reply before resolving it.
/// Queries preserve the requested room; execution uses current game progress.
/// Both records must be live and may alias. Neither pointer is retained, and
/// task and messageId are unused. Returns 1 to permit the ordinary transition.
static s32 _shelterB1TransferTunnelResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_B1_TRANSFER_TUNNEL_ROOM_EVENT_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_B1_TRANSFER_TUNNEL_ROOM_EVENT_ACCEPTED;
}

/// Ignores room commands from CAP and direction triggers, returning zero.
///
/// `commandId` and `commandArg` are receiver-specific integers; neither is read.
static s32 _shelterB1TransferTunnelIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room action requests from direction triggers, returning zero.
///
/// The borrowed request is neither dereferenced nor retained.
static s32 _shelterB1TransferTunnelIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Installs and publishes the room controller for message dispatch.
///
/// Requires entry state 0 and the loaded room message table. Registers this
/// live task in `GAME_TASK_SLOT_ROOM`, then advances to idle state 1. Allocates
/// no work; the task and table must remain live while room messages are sent.
static void _shelterB1TransferTunnelInitializeRoom(Task* task)
{
    task->msgTable = _gShelterB1TransferTunnelRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state += 1;
}

/// Keeps the initialized room task available for messages without advancing its state.
static void _shelterB1TransferTunnelIdle(Task* task)
{
}

void shelterB1TransferTunnelRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB1TransferTunnelRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Binds shared actor effect requests to this room's counted bank-6 tasks.
///
/// Installs seven packed bank/type IDs for `effectSpawn` after the room-effect
/// controller clears its selectors. Each ID persists until overwritten or the
/// controller is reinitialized. Keep this overlay loaded while the selected
/// effects are requested or running.
static inline void _shelterB1TransferTunnelBindEffects(void)
{
    gRoomEffectMoteId         = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_MOTE;
    gRoomEffectHaloId         = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_SPARK_EMITTER;
    gRoomEffectFlashId        = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_TRANSFER_TUNNEL_SPARK_BURST;
}

void shelterB1TransferTunnelDrawGlowsTask(Task* task)
{
    enum { GLOWS_INITIALIZE,
           GLOWS_DRAW,
           GLOWS_RADIUS_SCALE = 512 };

    u8 viewIndex;

    if (task->state == GLOWS_INITIALIZE) {
        _shelterB1TransferTunnelBindEffects();
        task->state = GLOWS_DRAW;
    }
    // The mapped view selects which fixed world-space light glows are visible.
    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2: {
            const SVECTOR* glowPoints = _gShelterB1TransferTunnelGlowPoints;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOWS_RADIUS_SCALE, 0);
            glowDrawRedDisc(&glowPoints[8], GLOWS_RADIUS_SCALE);
        } break;
        case 3: {
            const SVECTOR* glowPoints = _gShelterB1TransferTunnelGlowPoints;
            glowDrawDimGreyCapsule(&glowPoints[0], GLOWS_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[4], GLOWS_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawRedDisc(&glowPoints[8], GLOWS_RADIUS_SCALE);
        } break;
        case 4: {
            const SVECTOR* glowPoints = &_gShelterB1TransferTunnelGlowPoints[2];
            glowDrawDimGreyCapsule(&glowPoints[0], GLOWS_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[4], GLOWS_RADIUS_SCALE, GLOW_HALF_TURN);
        } break;
    }
}

#include "../../shared/effects/glow_draw_cone.inc.c"

#include "../../shared/effects/glow_draw_red_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB1TransferTunnelRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB1TransferTunnelRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB1TransferTunnelRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
