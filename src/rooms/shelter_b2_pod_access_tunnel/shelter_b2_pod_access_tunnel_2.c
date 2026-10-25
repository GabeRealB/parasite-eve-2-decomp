#include "rooms/shelter_b2_pod_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b2_pod_access_tunnel_private.h"

#include "actors/actor_00300.h"
#include "actors/actor_01100.h"
#include "actors/actor_02100.h"
#include "actors/actor_02300.h"
#include "actors/actor_02600.h"
#include "actors/actor_05600.h"
#include "actors/actor_403900.h"

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
#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

static WorldCollisionGrid     _gShelterB2PodAccessTunnelCollisionGrid;
static WorldCollisionOccluder _gShelterB2PodAccessTunnelSightOccluders[1];
static WorldCollisionTrigger  _gShelterB2PodAccessTunnelViewBoundaryTriggers[4];
static WorldCollisionTrigger  _gShelterB2PodAccessTunnelRoom1ActionTriggers[3];
static WorldCollisionTrigger  _gShelterB2PodAccessTunnelRoom2ActionTriggers[3];
static WorldCoordRoomLights   _gShelterB2PodAccessTunnelLightCollection;

TaskDesc gShelterB2PodAccessTunnelStagedEventTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = roomEventStagedTask,
    .data     = { .value = 0 },
};

TaskMessageEntry gShelterB2PodAccessTunnelRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, shelterB2PodAccessTunnelResolveRoomEventMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, shelterB2PodAccessTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, shelterB2PodAccessTunnelIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, shelterB2PodAccessTunnelHandleCommandMessage },
    { ROOM_MESSAGE_SOUND, shelterB2PodAccessTunnelHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gShelterB2PodAccessTunnelRideToB1TaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } },
    .callback = shelterB2PodAccessTunnelRideToB1Task,
    .data     = { .value = 0 },
};

/// Endpoint offsets used as the bases of the mapped-view beam selections.
enum {
    SHELTER_B2_POD_ACCESS_TUNNEL_BEAM_VIEW_2_BASE = 0,
    SHELTER_B2_POD_ACCESS_TUNNEL_BEAM_VIEW_3_BASE = 8,
    SHELTER_B2_POD_ACCESS_TUNNEL_BEAM_VIEW_4_BASE = 24,
};

/// World-space endpoint pairs of the tunnel's 28 light-fixture beams.
///
/// Adjacent points form one capsule, in signed game-coordinate units. The first
/// 28 endpoints lie at Y=200 and the next 28 at Y=410. Mapped views 2, 3/6 and
/// 4/7 select overlapping subsets from bases 0, 8 and 24; every pair lies within
/// this complete array. Storage remains live for the room's beam task.
static SVECTOR _gShelterB2PodAccessTunnelBeamEndpoints[56] = {
    { 920, 200, -10710, 0 },
    { 920, 200, -9710, 0 },
    { 920, 200, -9105, 0 },
    { 920, 200, -8105, 0 },
    { 920, 200, -7300, 0 },
    { 920, 200, -6300, 0 },
    { 920, 200, -5660, 0 },
    { 920, 200, -4660, 0 },
    { 920, 200, -3960, 0 },
    { 920, 200, -2960, 0 },
    { 920, 200, -2300, 0 },
    { 920, 200, -1300, 0 },
    { 2623, 200, -10710, 0 },
    { 2623, 200, -9710, 0 },
    { 2623, 200, -9105, 0 },
    { 2623, 200, -8105, 0 },
    { 2623, 200, -7300, 0 },
    { 2623, 200, -6300, 0 },
    { 2623, 200, -5660, 0 },
    { 2623, 200, -4660, 0 },
    { 2623, 200, -3960, 0 },
    { 2623, 200, -2960, 0 },
    { 1250, 200, -923, 0 },
    { 2250, 200, -923, 0 },
    { 3150, 200, -923, 0 },
    { 4150, 200, -923, 0 },
    { 3150, 200, -2620, 0 },
    { 4150, 200, -2620, 0 },
    { 1020, 410, -10710, 0 },
    { 1020, 410, -9710, 0 },
    { 1020, 410, -9105, 0 },
    { 1020, 410, -8105, 0 },
    { 1020, 410, -7300, 0 },
    { 1020, 410, -6300, 0 },
    { 1020, 410, -5660, 0 },
    { 1020, 410, -4660, 0 },
    { 1020, 410, -3960, 0 },
    { 1020, 410, -2960, 0 },
    { 1020, 410, -2300, 0 },
    { 1020, 410, -1300, 0 },
    { 2523, 410, -10710, 0 },
    { 2523, 410, -9710, 0 },
    { 2523, 410, -9100, 0 },
    { 2523, 410, -8100, 0 },
    { 2523, 410, -7300, 0 },
    { 2523, 410, -6300, 0 },
    { 2523, 410, -5660, 0 },
    { 2523, 410, -4660, 0 },
    { 2523, 410, -3960, 0 },
    { 2523, 410, -2960, 0 },
    { 1250, 410, -1023, 0 },
    { 2250, 410, -1023, 0 },
    { 3150, 410, -1023, 0 },
    { 4150, 410, -1023, 0 },
    { 3150, 410, -2520, 0 },
    { 4150, 410, -2520, 0 },
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
    .alignmentFill = 0xA041,                           \
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

WorldCollisionRoomResources gShelterB2PodAccessTunnelRoomCollisionResources[2] = {
    {
        .grid                 = &_gShelterB2PodAccessTunnelCollisionGrid,
        .viewBoundaryTriggers = _gShelterB2PodAccessTunnelViewBoundaryTriggers,
        .actionTriggers       = _gShelterB2PodAccessTunnelRoom1ActionTriggers,
        .occluders            = _gShelterB2PodAccessTunnelSightOccluders,
    },
    {
        .grid                 = &_gShelterB2PodAccessTunnelCollisionGrid,
        .viewBoundaryTriggers = _gShelterB2PodAccessTunnelViewBoundaryTriggers,
        .actionTriggers       = _gShelterB2PodAccessTunnelRoom2ActionTriggers,
        .occluders            = _gShelterB2PodAccessTunnelSightOccluders,
    },
};

WorldCoordRoomLighting gShelterB2PodAccessTunnelRoomLighting[2] = {
    { &_gShelterB2PodAccessTunnelLightCollection, NULL },
    { &_gShelterB2PodAccessTunnelLightCollection, NULL },
};

/// Room 2's seven logical views mapped to camera, image and sprite indices.
///
/// Logical views 3/4 select the alternate images at mapped indices 6/7;
/// logical views 6/7 map back to 3/4. Index by logical view minus one.
/// The eighth stored byte is zero, outside the seven-view search extent.
static u8 _gShelterB2PodAccessTunnelRoom2ViewMap[8] = {
    1,
    2,
    6,
    7,
    5,
    3,
    4,
    0,
};

u8* gShelterB2PodAccessTunnelRoomViewMaps[2] = {
    gViewIdentityMap,
    _gShelterB2PodAccessTunnelRoom2ViewMap,
};

ViewCount gShelterB2PodAccessTunnelRoomViewCounts[2] = { 7, 7 };

/// One-based departure slots packed into action-trigger parameter1's high nibble.
enum {
    SHELTER_B2_POD_ACCESS_TUNNEL_WARP_SEPTIC_TANK = 1,
    SHELTER_B2_POD_ACCESS_TUNNEL_WARP_R48         = 2,
};

/// Room command carried by the pod's CAP-to-message action triggers.
enum { SHELTER_B2_POD_ACCESS_TUNNEL_COMMAND_RIDE_TO_B1 = 1 };

DirectionWarpEntry gShelterB2PodAccessTunnelWarpEntries[3] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 1700, .y = 0, .z = -10445 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { .yaw = { .word = 256 }, .x = 1050, .y = 0, .z = -10000 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { .yaw = { .word = 3072 }, .x = 5500, .y = 0, .z = -1700 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { .yaw = { .word = 3072 }, .x = 5500, .y = 0, .z = -1700 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 6),
        .blockedSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 3),
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY,
    },
    {
        .player         = { .yaw = { .word = 0 }, .x = 4800, .y = 0, .z = -2400 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { .yaw = { .word = 2560 }, .x = 4500, .y = 0, .z = -1250 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 5),
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_POD,
    },
};

/// Room-space collision normals, with 4096 representing unit length.
static SVECTOR _gShelterB2PodAccessTunnelCollisionGridNormals[5] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_normals.inc"
};

/// Room-space collision vertices in signed game-coordinate units.
static SVECTOR _gShelterB2PodAccessTunnelCollisionGridVerts[40] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_verts.inc"
};

/// Seventeen collision faces indexing the grid's vertex and normal pools.
static WorldCollisionGridFace _gShelterB2PodAccessTunnelCollisionGridFaces[17] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_faces.inc"
};

/// Face-index lists for the six grid cells, each terminated by -1.
static s16 _gShelterB2PodAccessTunnelCollisionGridCells[72] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB2PodAccessTunnelCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..71
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B2_POD_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2PodAccessTunnelCollisionGridCells[(halfwordOffset)])
/// X-major 2-by-3 directory into the collision grid's face-index lists.
static s16* _gShelterB2PodAccessTunnelCollisionGridTable[6] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_table.inc"
};
#undef SHELTER_B2_POD_ACCESS_TUNNEL_CELL_FACE_LIST_AT_OFFSET

/// Tunnel collision mesh and its 2-by-3 XZ spatial index, shared by both rooms.
///
/// Cells span 4000 game units from origin (500, -11150). Activation borrows
/// the current view coordinate; the grid and its writable mesh pools remain
/// resident until the room is unloaded.
static WorldCollisionGrid _gShelterB2PodAccessTunnelCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB2PodAccessTunnelCollisionGridNormals,
    .vertices    = _gShelterB2PodAccessTunnelCollisionGridVerts,
    .faces       = _gShelterB2PodAccessTunnelCollisionGridFaces,
    .cellFaceIds = _gShelterB2PodAccessTunnelCollisionGridTable,
    .xBias       = -500,
    .zBias       = 11150,
    .cellCountX  = 2,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB2PodAccessTunnelCollisionGridFaces),
};

ViewCamera gShelterB2PodAccessTunnelViewCameras[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4000, 0x36B0, 5500 } }, 230 },
    { { { { -3971, 0, -1003 }, { -45, 4091, 179 }, { 1002, 185, -3967 } }, { -949, 1422, 2295 } }, 230 },
    { { { { 3920, 0, -1185 }, { -147, 4064, -488 }, { 1176, 510, 3890 } }, { -899, 1609, 7345 } }, 230 },
    { { { { -672, 0, -4040 }, { -824, 4009, 137 }, { 3955, 835, -658 } }, { -779, 1872, 1145 } }, 225 },
    { { { { -3803, 0, -1520 }, { -283, 4024, 709 }, { 1493, 764, -3736 } }, { -4209, 1711, 1892 } }, 246 },
    { { { { 3920, 0, -1185 }, { -147, 4064, -488 }, { 1176, 510, 3890 } }, { -899, 1609, 7345 } }, 230 },
    { { { { -672, 0, -4040 }, { -824, 4009, 137 }, { 3955, 835, -658 } }, { -779, 1872, 1145 } }, 225 },
};

/// Image-strip background with no source sprites for mapped view 1.
///
/// A zero-count first batch preserves decoded background strips; the next
/// record ends the list. The source slot retains this same list's address.
static SpriteBatch _gShelterB2PodAccessTunnelView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-strip background with no source sprites for mapped view 2.
///
/// A zero-count first batch preserves decoded background strips; the next
/// record ends the list. The source slot retains this same list's address.
static SpriteBatch _gShelterB2PodAccessTunnelView2SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sixty depth-sorted texture rectangles for mapped view 3.
///
/// Four batches cover all elements. Positions and sizes are screen pixels;
/// texture pages 141..143 and CLUT 0x3FC0 select the view's loaded atlas.
static SpriteSource _gShelterB2PodAccessTunnelView3SpriteSources[60] = {
    { 142, 0x3FC0, { .fields = { 40, 64 } }, 48, -120, 918, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, 88, -120, 1250, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, 128, -120, 1202, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 48, -56, 943, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, 88, -56, 1170, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -56, 1170, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 128, -48, 1154, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 144, -48, 1170, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 72, 16, 900, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 48, 16, 1098, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, 32, 998, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 40, 954, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, 48, 838, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 112, 56, 784, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 722, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 676, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 1186, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 48, 570, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1186, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 40, 678, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 1234, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, 32, 738, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 862, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 88, 24, 809, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, -112, 1110, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -56, 1139, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -16, 1166, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 152 } }, -88, -80, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 152 } }, -24, -80, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 152 } }, 24, -80, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, 56, -80, 854, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 56, -16, 882, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 56, 24, 879, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -136, 48, 0, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -80, 0, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -56, 721, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -32, 712, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 720, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 732, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -120, 16, 729, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 690, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -120, -24, 698, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -120, -48, 691, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -120, -80, 684, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -80, 653, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -48, 682, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -136, -24, 683, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 673, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, 16, 714, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 24, 0, 1211, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 1070, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 56, 0, 909, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 16 } }, 72, 0, 0, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 80, 16, 860, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 96, 24, 746, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 72, 16, 877, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 24, 810, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 32, 723, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 665, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 32, 613, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
};

/// Image-strip background followed by four sprite ranges for mapped view 3.
///
/// Ranges [0,24), [24,27), [27,49) and [49,60) cover its source array;
/// `SPRITE_BATCH_END` ends the list after the four live batches.
static SpriteBatch _gShelterB2PodAccessTunnelView3SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 3, 0 } },
    { 24, 3, 0, 0, { 0, 0 } },
    { 27, 22, 0, 0, { 2, 0 } },
    { 49, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-strip background with no source sprites for mapped view 4.
///
/// A zero-count first batch preserves decoded background strips; the next
/// record ends the list. The source slot retains this same list's address.
static SpriteBatch _gShelterB2PodAccessTunnelView4SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-strip background with no source sprites for mapped view 5.
///
/// A zero-count first batch preserves decoded background strips; the next
/// record ends the list. The source slot retains this same list's address.
static SpriteBatch _gShelterB2PodAccessTunnelView5SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Fifty-four depth-sorted texture rectangles for alternate mapped view 6.
///
/// Four batches cover all elements. Positions and sizes are screen pixels;
/// texture pages 142/143 and CLUT 0x3FC0 select the alternate view's atlas.
static SpriteSource _gShelterB2PodAccessTunnelView6SpriteSources[54] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 48, -120, 918, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 88, -120, 1250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, -120, 1202, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 48, -56, 943, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, -56, 1170, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -56, 1170, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 128, -48, 1154, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -48, 1170, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 16, 900, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 48, 16, 1098, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, 32, 998, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 40, 954, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, 48, 838, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, 56, 784, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 722, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 676, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 1186, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 144, 48, 570, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1186, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 40, 678, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 1234, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 112, 32, 738, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 862, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 24, 809, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, -112, 1110, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -56, 1139, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -16, 1166, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 56, -80, 854, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 56, -16, 882, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 56, 24, 879, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -32, 712, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 720, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 732, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 16, 729, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 690, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -24, 698, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -48, 691, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, -80, 684, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -80, 653, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -48, 682, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -24, 683, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 673, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, 16, 714, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -48, 721, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 24, 0, 1211, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 1070, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 0, 909, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, 16, 877, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 24, 810, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 32, 723, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 665, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 32, 613, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 16, 860, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 24, 758, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
};

/// Image-strip background followed by four sprite ranges for mapped view 6.
///
/// Ranges [0,24), [24,27), [27,44) and [44,54) cover its source array;
/// `SPRITE_BATCH_END` ends the list after the four live batches.
static SpriteBatch _gShelterB2PodAccessTunnelView6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 3, 0 } },
    { 24, 3, 0, 0, { 0, 0 } },
    { 27, 17, 0, 0, { 2, 0 } },
    { 44, 10, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Image-strip background with no source sprites for mapped view 7.
///
/// A zero-count first batch preserves decoded background strips; the next
/// record ends the list. The source slot retains this same list's address.
static SpriteBatch _gShelterB2PodAccessTunnelView7SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB2PodAccessTunnelSpriteViews[7] = {
    { { .empty = _gShelterB2PodAccessTunnelView1SpriteBatches }, _gShelterB2PodAccessTunnelView1SpriteBatches, NULL },
    { { .empty = _gShelterB2PodAccessTunnelView2SpriteBatches }, _gShelterB2PodAccessTunnelView2SpriteBatches, NULL },
    { { .elements = _gShelterB2PodAccessTunnelView3SpriteSources }, _gShelterB2PodAccessTunnelView3SpriteBatches, NULL },
    { { .empty = _gShelterB2PodAccessTunnelView4SpriteBatches }, _gShelterB2PodAccessTunnelView4SpriteBatches, NULL },
    { { .empty = _gShelterB2PodAccessTunnelView5SpriteBatches }, _gShelterB2PodAccessTunnelView5SpriteBatches, NULL },
    { { .elements = _gShelterB2PodAccessTunnelView6SpriteSources }, _gShelterB2PodAccessTunnelView6SpriteBatches, NULL },
    { { .empty = _gShelterB2PodAccessTunnelView7SpriteBatches }, _gShelterB2PodAccessTunnelView7SpriteBatches, NULL },
};

/// Eight room-owned point lights contributing in every logical view.
///
/// Positions and inner/outer falloff radii are world-coordinate units; RGB
/// intensities have 12 fractional bits. Lighting updates their transforms and
/// attenuation in place while the room remains loaded.
static WorldCoordPointLight _gShelterB2PodAccessTunnelPointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -3949 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -5618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -6721 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -8147 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -9711 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4126, -1155, -2029 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3378, 3157, 3496 }, { 0, 0 } }, 1200, 2620 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3177, 0, -1757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -2395 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
};

/// The point-light collection used by both logical rooms.
///
/// Borrows all eight point lights, with no directional or cone lights.
static WorldCoordRoomLights _gShelterB2PodAccessTunnelLightCollection = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB2PodAccessTunnelPointLights),
    .pointLights           = _gShelterB2PodAccessTunnelPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Four directed view boundaries shared by both logical rooms.
///
/// Connects logical views 2 to 3, 3 to 2, 4 to 3 and 3 to 4. Setup binds the
/// quads to the current view and mutates their list links, flags and hit latches.
/// The fourth quad is included before LAST ends the array. Origins, corner
/// offsets and radii use game units; plane normals use 4096 per unit.
static WorldCollisionTrigger _gShelterB2PodAccessTunnelViewBoundaryTriggers[4] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1760, -1648, -4513, 0 },
        .vertices     = { { -1824, -2224, 64, 0 }, { 1824, -2224, -64, 0 }, { -1824, 2224, 64, 0 }, { 1824, 2224, -64, 0 } },
        .normal       = { -144, 0, -4100, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2873,
        .control      = 0,
        .parameter0   = 2,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1744, -1632, -4625, 0 },
        .vertices     = { { 1808, -2224, -80, 0 }, { -1808, -2224, 80, 0 }, { 1808, 2224, -80, 0 }, { -1808, 2224, 80, 0 } },
        .normal       = { 180, 0, 4096, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2862,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 2,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 3520, -1632, -1360, 0 },
        .vertices     = { { -285, -2224, -1838, 0 }, { 254, -2224, 1809, 0 }, { -285, 2224, -1838, 0 }, { 254, 2224, 1809, 0 } },
        .normal       = { 4054, 0, -601, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2884,
        .control      = 0,
        .parameter0   = 4,
        .parameter1   = 3,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 3606, -1664, -1488, 0 },
        .vertices     = { { 250, -2224, 1567, 0 }, { -271, -2224, -1585, 0 }, { 250, 2224, 1567, 0 }, { -271, 2224, -1585, 0 } },
        .normal       = { -4049, 0, 668, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 2733,
        .control      = 0,
        .parameter0   = 3,
        .parameter1   = 4,
        .flags        = WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Room 1's septic-tank and R48 door warps and pod-ride interaction quad.
///
/// Warp bytes pack departure/arrival slots 1/2 and 2/1; the pod sends room
/// command 1. Setup binds the current-view coordinate and links all three
/// mutable records through LAST. Positions/radii are game units; normals use
/// 4096 per unit. The room overlay must stay loaded until these are unlinked.
static WorldCollisionTrigger _gShelterB2PodAccessTunnelRoom1ActionTriggers[3] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1728, -48, -0x28CF, 0 },
        .vertices     = { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 1123,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_SHELTER_B2_SEPTIC_TANK,
        .parameter1   = (SHELTER_B2_POD_ACCESS_TUNNEL_WARP_SEPTIC_TANK << 4) | 2,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 5856, -48, -1728, 0 },
        .vertices     = { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } },
        .normal       = { 0, 4110, 0, 0 },
        .facingNormal = { -4096, 0, 0, 0 },
        .radius       = 1166,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_SHELTER_R48,
        .parameter1   = (SHELTER_B2_POD_ACCESS_TUNNEL_WARP_R48 << 4) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4592, -64, -2784, 0 },
        .vertices     = { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } },
        .normal       = { 0, 4096, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 1047,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = SHELTER_B2_POD_ACCESS_TUNNEL_COMMAND_RIDE_TO_B1,
        .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Room 2's septic-tank and R48 door warps and pod-ride interaction quad.
///
/// Same geometry and actions as room 1, with separate mutable links and hit
/// latches. Setup binds and links all three through LAST. Warp bytes pack
/// departure/arrival slots 1/2 and 2/1; the pod sends room command 1. Positions
/// and radii are game units; normals use 4096 per unit. Keep live until unlinked.
static WorldCollisionTrigger _gShelterB2PodAccessTunnelRoom2ActionTriggers[3] = {
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 1728, -48, -0x28CF, 0 },
        .vertices     = { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } },
        .normal       = { 0, 4098, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 1123,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_SHELTER_B2_SEPTIC_TANK,
        .parameter1   = (SHELTER_B2_POD_ACCESS_TUNNEL_WARP_SEPTIC_TANK << 4) | 2,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 5856, -48, -1728, 0 },
        .vertices     = { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } },
        .normal       = { 0, 4110, 0, 0 },
        .facingNormal = { -4096, 0, 0, 0 },
        .radius       = 1166,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_WARP,
        .parameter0   = GAME_AREA_SHELTER_R48,
        .parameter1   = (SHELTER_B2_POD_ACCESS_TUNNEL_WARP_R48 << 4) | 1,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD,
        .hit          = 0,
    },
    {
        .next         = NULL,
        .prevLink     = NULL,
        .coord        = NULL,
        .origin       = { 4592, -64, -2784, 0 },
        .vertices     = { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } },
        .normal       = { 0, 4096, 0, 0 },
        .facingNormal = { 0, 0, 4096, 0 },
        .radius       = 1047,
        .control      = WORLD_COLLISION_TRIGGER_ACTION_CAP,
        .parameter0   = SHELTER_B2_POD_ACCESS_TUNNEL_COMMAND_RIDE_TO_B1,
        .parameter1   = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE,
        .flags        = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST,
        .hit          = 0,
    },
};

/// Placement-entry keys for the actor resources used by this room.
enum {
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER               = 21,
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK              = 11,
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT                = 26,
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LARGE_MOSSBACK        = 49,
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BRAIN_STINGER         = 3,
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM = 23,
    SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LAUNCHER_PAWN_GOLEM   = 56,
};

/// Actor file and spawn recipes for placement variant 1.
///
/// Loads Watchers in actor slot 1 and a Mossback in slot 2.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant1Resources[3] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor201100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor file and spawn recipes for placement variant 2.
///
/// Loads Watchers in actor slot 1 and Mossbacks in slot 2.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant2Resources[3] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor201100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor file and spawn recipes for placement variant 3.
///
/// Loads Maggots in actor slot 1 and a large Mossback in slot 2.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant3Resources[3] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor02600MaggotCaterpillarBodyTask },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LARGE_MOSSBACK, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor201100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor file and spawn recipes for placement variant 4.
///
/// Loads a Brain Stinger in actor slot 1.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant4Resources[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BRAIN_STINGER, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor100300Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor file and spawn recipes for placement variant 7.
///
/// Loads a Beam Sword Rook GOLEM in actor slot 1.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant7Resources[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor file and spawn recipes for placement variant 11.
///
/// Loads Watchers in actor slot 1 and a Beam Sword Rook GOLEM in slot 2.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant11Resources[3] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202300RookGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor file and spawn recipes for placement variant 21.
///
/// Loads Watchers in actor slot 1 and a Launcher Pawn GOLEM in slot 2.
/// Placements match `entryId`; `AREA_PLACEMENT_END` terminates the table.
/// The room table and loaded actor descriptors stay live through spawning.
static AreaResource _gShelterB2PodAccessTunnelVariant21Resources[3] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LAUNCHER_PAWN_GOLEM, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor205600PawnGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Placement-entry key for variant 22's Bishop GOLEM resource.
enum { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BISHOP_GOLEM = 39 };

/// Bishop GOLEM file and spawn recipe for placement variant 22.
///
/// Loads actor-slot-4 file 39 and spawns its sole body descriptor at index 0.
/// Matches the placement-entry key, then ends at `AREA_PLACEMENT_END`.
/// The room resource and loaded actor descriptor must stay live through spawn.
static AreaResource _gShelterB2PodAccessTunnelVariant22Resources[2] = {
    {
        .entryId        = SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BISHOP_GOLEM,
        .fileNumber     = 39,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .unknown_6      = { 0, 0 },
        .taskTable      = &gActor403900GolemTaskDesc,
    },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Variant 1's three Watchers and one Mossback, followed by the end record.
///
/// Resource-entry keys select the corresponding variant-1 recipes. Positions
/// use game units and yaw uses 4096 units per turn; actors borrow these records
/// until release. Variant, mode and rowIndex carry actor-specific spawn settings.
static AreaPlacement _gShelterB2PodAccessTunnelVariant1Placements[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 4, 0, 1200, -2200, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 4, 0, 1950, -2200, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 4, 0, 2700, -2200, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 0, 0, 1600, 0, -5200, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 2's two Watchers and two Mossbacks, followed by the end record.
///
/// Resource-entry keys select the corresponding variant-2 recipes. Positions
/// use game units and yaw uses 4096 units per turn; actors borrow these records
/// until release. Variant, mode and rowIndex carry actor-specific spawn settings.
static AreaPlacement _gShelterB2PodAccessTunnelVariant2Placements[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1900, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1400, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 2, 0, 3200, 0, -1300, 4000, 0, 2, 4, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 2, 0, 1950, 0, -4700, 3900, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 3's three Maggots and one large Mossback, followed by the end record.
///
/// Resource-entry keys select the corresponding variant-3 recipes. Positions
/// use game units and yaw uses 4096 units per turn; actors borrow these records
/// until release. Variant, mode and rowIndex carry actor-specific spawn settings.
static AreaPlacement _gShelterB2PodAccessTunnelVariant3Placements[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 0, 0, 1200, 0, -2800, 1700, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 0, 0, 2500, 0, -1000, 1300, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 0, 0, 1100, 0, -1100, 1900, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LARGE_MOSSBACK, 1, 0, 1600, 0, -1950, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 4's single Brain Stinger at (1700, 0, -5000), followed by the end record.
///
/// Its key selects the variant-4 recipe. Positions use game units and yaw uses
/// 4096 units per turn. The actor borrows this placement until release.
static AreaPlacement _gShelterB2PodAccessTunnelVariant4Placements[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BRAIN_STINGER, 0, 0, 1700, 0, -5000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 7's single patrolling Beam Sword Rook GOLEM, followed by the end record.
///
/// Mode bit 0 enables patrol; variant 7 gives a 7000-unit leg. Its key selects
/// the variant-7 recipe. Positions use game units and yaw uses 4096 units per
/// turn. The actor borrows this placement until release.
static AreaPlacement _gShelterB2PodAccessTunnelVariant7Placements[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM, 7, 1, 1700, 0, -8600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 11's two Watchers and one Beam Sword Rook GOLEM, followed by the end record.
///
/// Resource-entry keys select the corresponding variant-11 recipes. Positions
/// use game units and yaw uses 4096 units per turn; actors borrow these records
/// until release. Variant, mode and rowIndex carry actor-specific spawn settings.
static AreaPlacement _gShelterB2PodAccessTunnelVariant11Placements[4] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1900, -600, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 3 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1400, -600, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 3 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM, 0, 0, 5200, 0, -1750, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 21's three Watchers and one Launcher Pawn GOLEM, followed by the end record.
///
/// Resource-entry keys select the corresponding variant-21 recipes. Positions
/// use game units and yaw uses 4096 units per turn; actors borrow these records
/// until release. Variant, mode and rowIndex carry actor-specific spawn settings.
static AreaPlacement _gShelterB2PodAccessTunnelVariant21Placements[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 1, 0, 1200, -2200, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 1, 0, 1950, -2200, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 1, 0, 2700, -2200, -800, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LAUNCHER_PAWN_GOLEM, 0, 0, 1700, 0, -8000, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Variant 22's single Bishop GOLEM at (7000, 0, 1500), followed by the end record.
///
/// Its key selects the variant-22 recipe. Positions use game units and yaw uses
/// 4096 units per turn. The actor borrows this placement until release.
static AreaPlacement _gShelterB2PodAccessTunnelVariant22Placements[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BISHOP_GOLEM, 0, 0, 7000, 0, 1500, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2PodAccessTunnelAreaVariants[23] = {
    [1]  = { .placements = _gShelterB2PodAccessTunnelVariant1Placements, .resources = _gShelterB2PodAccessTunnelVariant1Resources },
    [2]  = { .placements = _gShelterB2PodAccessTunnelVariant2Placements, .resources = _gShelterB2PodAccessTunnelVariant2Resources },
    [3]  = { .placements = _gShelterB2PodAccessTunnelVariant3Placements, .resources = _gShelterB2PodAccessTunnelVariant3Resources },
    [4]  = { .placements = _gShelterB2PodAccessTunnelVariant4Placements, .resources = _gShelterB2PodAccessTunnelVariant4Resources },
    [7]  = { .placements = _gShelterB2PodAccessTunnelVariant7Placements, .resources = _gShelterB2PodAccessTunnelVariant7Resources },
    [11] = { .placements = _gShelterB2PodAccessTunnelVariant11Placements, .resources = _gShelterB2PodAccessTunnelVariant11Resources },
    [21] = { .placements = _gShelterB2PodAccessTunnelVariant21Placements, .resources = _gShelterB2PodAccessTunnelVariant21Resources },
    [22] = { .placements = _gShelterB2PodAccessTunnelVariant22Placements, .resources = _gShelterB2PodAccessTunnelVariant22Resources },
};

/// Single sight-blocking quad shared by both logical rooms.
///
/// Positions and radius use game units; its plane normal uses 4096 per unit.
/// Setup links and enables this LAST record, mutating its links and flags.
/// Initial flag bit 0 is preserved by list operations; its role is unproven.
/// Keep this room-owned storage loaded until the occluder has been unlinked.
static WorldCollisionOccluder _gShelterB2PodAccessTunnelSightOccluders[1] = {
    { NULL, NULL, { 4815, -1856, -6112, 0 }, { { -1689, 2880, 2964, 0 }, { 1690, 2880, -2964, 0 }, { -1689, -2880, 2964, 0 }, { 1690, -2880, -2964, 0 } }, { 3574, 0, 2037, 0 }, 4463, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Binds seven actor effect families to this room's effect-task implementations.
///
/// The selectors pack task bank 6 and a descriptor index for `effectSpawn`.
/// Call during room setup before actor effects are spawned; this overlay must
/// remain loaded while its effect tasks run. The bindings persist until another
/// room replaces them or the gameplay effect controller clears them.
static inline void _shelterB2PodAccessTunnelInstallEffectIds(void)
{
    gRoomEffectMoteId         = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_MOTE;
    gRoomEffectHaloId         = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_SPARK_EMITTER;
    gRoomEffectFlashId        = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_SPARK_BURST;
}

void shelterB2PodAccessTunnelDrawLightBeamsTask(Task* task)
{
    enum { BEAMS_INITIALIZE,
           BEAMS_DRAW,
           BEAM_RADIUS_SCALE = 512 };

    // Bind actor-spawned effects to the implementations in this loaded room.
    if (task->state == BEAMS_INITIALIZE) {
        _shelterB2PodAccessTunnelInstallEffectIds();
        task->state = BEAMS_DRAW;
    }

    // Draw the light fixtures visible in the mapped camera view.
    switch ((u8)viewGetMappedIndex()) {
        case 2: {
            const SVECTOR* beamPoints;
            beamPoints = &_gShelterB2PodAccessTunnelBeamEndpoints[SHELTER_B2_POD_ACCESS_TUNNEL_BEAM_VIEW_2_BASE];
            glowDrawDimGreyCapsule(&beamPoints[0], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[2], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[4], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[6], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[12], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[14], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[16], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[18], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[28], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[30], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[32], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[34], BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[40], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[42], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[44], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[46], BEAM_RADIUS_SCALE, 0);
            break;
        }
        case 3:
        case 6: {
            const SVECTOR* beamPoints;
            beamPoints = &_gShelterB2PodAccessTunnelBeamEndpoints[SHELTER_B2_POD_ACCESS_TUNNEL_BEAM_VIEW_3_BASE];
            glowDrawDimGreyCapsule(&beamPoints[0], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[2], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[10], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[12], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[14], BEAM_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&beamPoints[16], BEAM_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&beamPoints[28], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[30], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[38], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[40], BEAM_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&beamPoints[42], BEAM_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&beamPoints[44], BEAM_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        }
        case 4:
        case 7: {
            const SVECTOR* beamPoints;
            beamPoints = &_gShelterB2PodAccessTunnelBeamEndpoints[SHELTER_B2_POD_ACCESS_TUNNEL_BEAM_VIEW_4_BASE];
            glowDrawDimGreyCapsule(&beamPoints[0], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[2], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[28], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&beamPoints[30], BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_cone.inc.c"

/// Names this room's externally linked drift-effect callback, declared in its public header.
///
/// Bind a function identifier with signature `void (Task*)` immediately before
/// the drift fragment. It uses the name once for the definition and clears the
/// binding afterwards; there are no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK shelterB2PodAccessTunnelEffectSpriteDriftTask
#include "../../shared/effects/effect_sprite_drift.inc.c"

#include "../../shared/effects/effect_sprite_draw_banked.inc.c"

#include "../../shared/effects/effect_sprite_draw_rotated.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB2PodAccessTunnelRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB2PodAccessTunnelRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
