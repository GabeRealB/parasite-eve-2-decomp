#include "rooms/shelter_b2_pod_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b2_pod_access_tunnel_private.h"

#include "actors/task_tables.h"

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
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

static WorldCollisionGrid     _gShelterB2PodAccessTunnelCollisionGrid;
extern WorldCollisionOccluder D_shelter_b2_pod_access_tunnel_80185664[1];
extern WorldCollisionTrigger  D_shelter_b2_pod_access_tunnel_80184FD8[4];
extern WorldCollisionTrigger  D_shelter_b2_pod_access_tunnel_80185108[3];
extern WorldCollisionTrigger  D_shelter_b2_pod_access_tunnel_801851EC[3];
static WorldCoordRoomLights   _gShelterB2PodAccessTunnelLightCollection;

TaskDesc D_shelter_b2_pod_access_tunnel_80183BC0 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_pod_access_tunnel_80183BCC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, shelterB2PodAccessTunnelResolveRoomEventMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, shelterB2PodAccessTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, shelterB2PodAccessTunnelIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, shelterB2PodAccessTunnelHandleCommandMessage },
    { ROOM_MESSAGE_SOUND, shelterB2PodAccessTunnelHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b2_pod_access_tunnel_80183BFC = { { { TASK_BODY_NONE, 32 } }, shelterB2PodAccessTunnelRideToB1Task, { .value = 0 } };

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

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0xA041 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_shelter_b2_pod_access_tunnel_80183DEC[2] = {
    { &_gShelterB2PodAccessTunnelCollisionGrid, D_shelter_b2_pod_access_tunnel_80184FD8, D_shelter_b2_pod_access_tunnel_80185108, D_shelter_b2_pod_access_tunnel_80185664 },
    { &_gShelterB2PodAccessTunnelCollisionGrid, D_shelter_b2_pod_access_tunnel_80184FD8, D_shelter_b2_pod_access_tunnel_801851EC, D_shelter_b2_pod_access_tunnel_80185664 },
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

ViewCount D_shelter_b2_pod_access_tunnel_80183E2C[2] = { 7, 7 };

DirectionWarpEntry D_shelter_b2_pod_access_tunnel_80183E30[3] = {
    { { { .word = 0 }, 1700, 0, -0x28CD }, { 0, 0, 0, 0 }, { { .word = 256 }, 1050, 0, -0x2710 }, { 0, 0, 0, 0 }, 0x54230002, 0x54230001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 5500, 0, -1700 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 5500, 0, -1700 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x54230006, 0x54230003, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY },
    { { { .word = 0 }, 4800, 0, -2400 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 4500, 0, -1250 }, { 0, 0, 0, 0 }, 0x54230005, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POD },
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

#define GRID_CELL(i) (&_gShelterB2PodAccessTunnelCollisionGridCells[i])
/// X-major 2-by-3 directory into the collision grid's face-index lists.
static s16* _gShelterB2PodAccessTunnelCollisionGridTable[6] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_table.inc"
};
#undef GRID_CELL

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

WorldCollisionTrigger D_shelter_b2_pod_access_tunnel_80184FD8[4] = {
    { NULL, NULL, NULL, { 1760, -1648, -4513, 0 }, { { -1824, -2224, 64, 0 }, { 1824, -2224, -64, 0 }, { -1824, 2224, 64, 0 }, { 1824, 2224, -64, 0 } }, { -144, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1744, -1632, -4625, 0 }, { { 1808, -2224, -80, 0 }, { -1808, -2224, 80, 0 }, { 1808, 2224, -80, 0 }, { -1808, 2224, 80, 0 } }, { 180, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3520, -1632, -1360, 0 }, { { -285, -2224, -1838, 0 }, { 254, -2224, 1809, 0 }, { -285, 2224, -1838, 0 }, { 254, 2224, 1809, 0 } }, { 4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3606, -1664, -1488, 0 }, { { 250, -2224, 1567, 0 }, { -271, -2224, -1585, 0 }, { 250, 2224, 1567, 0 }, { -271, 2224, -1585, 0 } }, { -4049, 0, 668, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_pod_access_tunnel_80185108[3] = {
    { NULL, NULL, NULL, { 1728, -48, -0x28CF, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -48, -1728, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 48, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4592, -64, -2784, 0 }, { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_pod_access_tunnel_801851EC[3] = {
    { NULL, NULL, NULL, { 1728, -48, -0x28CF, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -48, -1728, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 48, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4592, -64, -2784, 0 }, { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
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

AreaResource D_shelter_b2_pod_access_tunnel_801853B4[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor403900GolemTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801853CC[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 4, 0, 1200, -2200, -800, 2048, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 4, 0, 1950, -2200, -800, 2048, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 4, 0, 2700, -2200, -800, 2048, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 0, 0, 1600, 0, -5200, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018541C[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1900, -800, 2048, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1400, -800, 2048, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 2, 0, 3200, 0, -1300, 4000, 0, 2, 4, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MOSSBACK, 2, 0, 1950, 0, -4700, 3900, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018546C[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 0, 0, 1200, 0, -2800, 1700, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 0, 0, 2500, 0, -1000, 1300, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_MAGGOT, 0, 0, 1100, 0, -1100, 1900, 0, 0, 2, 0 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LARGE_MOSSBACK, 1, 0, 1600, 0, -1950, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801854BC[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BRAIN_STINGER, 0, 0, 1700, 0, -5000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801854DC[2] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM, 7, 1, 1700, 0, -8600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801854FC[4] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1900, -600, 2048, 0, 0, 2, 3 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 34, 2048, 1000, -1400, -600, 2048, 0, 0, 2, 3 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_BEAM_SWORD_ROOK_GOLEM, 0, 0, 5200, 0, -1750, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018553C[5] = {
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 1, 0, 1200, -2200, -800, 2048, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 1, 0, 1950, -2200, -800, 2048, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_WATCHER, 1, 0, 2700, -2200, -800, 2048, 0, 0, 2, 1 },
    { SHELTER_B2_POD_ACCESS_TUNNEL_RESOURCE_LAUNCHER_PAWN_GOLEM, 0, 0, 1700, 0, -8000, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018558C[2] = {
    { 39, 0, 0, 7000, 0, 1500, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_pod_access_tunnel_801855AC[23] = {
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_801853CC, _gShelterB2PodAccessTunnelVariant1Resources },
    { D_shelter_b2_pod_access_tunnel_8018541C, _gShelterB2PodAccessTunnelVariant2Resources },
    { D_shelter_b2_pod_access_tunnel_8018546C, _gShelterB2PodAccessTunnelVariant3Resources },
    { D_shelter_b2_pod_access_tunnel_801854BC, _gShelterB2PodAccessTunnelVariant4Resources },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_801854DC, _gShelterB2PodAccessTunnelVariant7Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_801854FC, _gShelterB2PodAccessTunnelVariant11Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_8018553C, _gShelterB2PodAccessTunnelVariant21Resources },
    { D_shelter_b2_pod_access_tunnel_8018558C, D_shelter_b2_pod_access_tunnel_801853B4 },
};

WorldCollisionOccluder D_shelter_b2_pod_access_tunnel_80185664[1] = {
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

#include "../../shared/glow_draw_cone.inc.c"

/// Names this room's externally linked drift-effect callback, declared in its public header.
///
/// Bind a function identifier with signature `void (Task*)` immediately before
/// the drift fragment. It uses the name once for the definition and clears the
/// binding afterwards; there are no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK shelterB2PodAccessTunnelEffectSpriteDriftTask
#include "../../shared/effect_sprite_drift.inc.c"

#include "../../shared/effect_sprite_draw_banked.inc.c"

#include "../../shared/effect_sprite_draw_rotated.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB2PodAccessTunnelRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void shelterB2PodAccessTunnelRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
