#include "rooms/neo_ark_submarine_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "neo_ark_submarine_tunnel_private.h"

#include "actors/actor_00400.h"
#include "actors/actor_02400.h"
#include "actors/actor_451100.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/screen/screen_wave.h"

static WorldCollisionGrid     _gNeoArkSubmarineTunnelCollisionGrid;
static WorldCollisionOccluder _gNeoArkSubmarineTunnelSightOccluders[3];
static WorldCollisionTrigger  _gNeoArkSubmarineTunnelViewBoundaryTriggers[6];
static WorldCollisionTrigger  _gNeoArkSubmarineTunnelActionTriggers[11];
static WorldCoordRoomLights   _gNeoArkSubmarineTunnelRoomLights;

u8 gNeoArkSubmarineTunnelEntryActionHandled = 0;

#include "../../shared/room/room_visual_effects_disc_data.inc.c"

WorldCoordRoomLighting gNeoArkSubmarineTunnelRoomLighting[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT] = {
    { .lights = &_gNeoArkSubmarineTunnelRoomLights, .ambientTable = NULL },
};

WorldCollisionRoomResources gNeoArkSubmarineTunnelRoomCollisionResources[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT] = {
    {
        .grid                 = &_gNeoArkSubmarineTunnelCollisionGrid,
        .viewBoundaryTriggers = _gNeoArkSubmarineTunnelViewBoundaryTriggers,
        .actionTriggers       = _gNeoArkSubmarineTunnelActionTriggers,
        .occluders            = _gNeoArkSubmarineTunnelSightOccluders,
    },
};

u8* gNeoArkSubmarineTunnelViewMaps[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT] = {
    gViewIdentityMap,
};

ViewCount gNeoArkSubmarineTunnelViewCounts[NEO_ARK_SUBMARINE_TUNNEL_ROOM_COUNT] = { NEO_ARK_SUBMARINE_TUNNEL_VIEW_COUNT };

DirectionWarpEntry gNeoArkSubmarineTunnelWarpEntries[NEO_ARK_SUBMARINE_TUNNEL_WARP_COUNT] = {
    { { { .word = 3072 }, 6300, 2330, 0 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4544, 2944, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6300, 2330, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -5250, 3000, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

s16 gNeoArkSubmarineTunnelDiverWaterY = 0;

enum { NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_COUNT = 8 };

/// Diver patrol ring 0: room XZ positions and submerged Y swim heights.
///
/// Selected by spawn-argument bits 4..5 equal to 0; indices wrap through eight
/// points. Scripted tunnel patrol reads Y directly; normal patrol adds water Y.
/// The SDK pad halfword is unused. Borrowed until the room's Divers are released.
static SVECTOR _gNeoArkSubmarineTunnelDiverPatrolWaypoints0[NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_COUNT] = {
    { 3456, -800, 2112, 0 },
    { -3648, -850, 2112, 0 },
    { -5344, -850, -1440, 0 },
    { -3808, -880, -2688, 0 },
    { -512, -800, 1984, 0 },
    { 4832, -800, -1440, 0 },
    { 2720, -800, -2560, 0 },
    { -5376, -800, -1216, 0 },
};

/// Diver patrol ring 1: ring 0's XZ path in reverse, with Y at the water surface.
///
/// Selected by spawn-argument bits 4..5 equal to 1; eight signed-coordinate
/// points, with an unused SDK pad halfword. Borrowed through Diver teardown.
static SVECTOR _gNeoArkSubmarineTunnelDiverPatrolWaypoints1[NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_COUNT] = {
    { -5376, 0, -1216, 0 },
    { 2720, 0, -2560, 0 },
    { 4832, 0, -1440, 0 },
    { -512, 0, 1984, 0 },
    { -3808, 0, -2688, 0 },
    { -5344, 0, -1440, 0 },
    { -3648, 0, 2112, 0 },
    { 3456, 0, 2112, 0 },
};

/// Diver patrol ring 2: eight room-coordinate points at surface Y.
///
/// Selected by spawn-argument bits 4..5 equal to 2; indices wrap 0..7.
/// The SDK pad halfword is unused. Borrowed through Diver teardown.
static SVECTOR _gNeoArkSubmarineTunnelDiverPatrolWaypoints2[NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_COUNT] = {
    { -7168, 0, 5696, 0 },
    { -3904, 0, 4224, 0 },
    { -4832, 0, 3440, 0 },
    { -1344, 0, 5951, 0 },
    { 352, 0, 3775, 0 },
    { -5344, 0, 3440, 0 },
    { -1376, 0, 5439, 0 },
    { -2880, 0, 6079, 0 },
};

/// Diver patrol ring 3: eight room-coordinate points at surface Y.
///
/// Selected by spawn-argument bits 4..5 equal to 3; indices wrap 0..7.
/// The SDK pad halfword is unused. Borrowed through Diver teardown.
static SVECTOR _gNeoArkSubmarineTunnelDiverPatrolWaypoints3[NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_COUNT] = {
    { 0x319C, 0, 4000, 0 },
    { 0x3840, 0, 4000, 0 },
    { 0x2840, 0, 4072, 0 },
    { 8160, 0, 4736, 0 },
    { 5024, 0, 4736, 0 },
    { 3872, 0, 4112, 0 },
    { 6400, 0, 3232, 0 },
    { 8320, 0, 1632, 0 },
};

SVECTOR* gNeoArkSubmarineTunnelDiverWaypointSets[NEO_ARK_SUBMARINE_TUNNEL_DIVER_WAYPOINT_SET_COUNT] = {
    _gNeoArkSubmarineTunnelDiverPatrolWaypoints0,
    _gNeoArkSubmarineTunnelDiverPatrolWaypoints1,
    _gNeoArkSubmarineTunnelDiverPatrolWaypoints2,
    _gNeoArkSubmarineTunnelDiverPatrolWaypoints3,
};

enum {
    NEO_ARK_SUBMARINE_TUNNEL_GRID_CELL_COUNT_X = 6,
    NEO_ARK_SUBMARINE_TUNNEL_GRID_CELL_COUNT_Z = 1,
};

/// Collision-face normals in room coordinates, with unit length scaled by ONE.
static SVECTOR _gNeoArkSubmarineTunnelCollisionGridNormals[] = {
#include "assets/neo_ark_submarine_tunnel_collision_04F1C_normals.inc"
};

/// Collision vertices in signed room-coordinate units.
static SVECTOR _gNeoArkSubmarineTunnelCollisionGridVerts[] = {
#include "assets/neo_ark_submarine_tunnel_collision_04F1C_verts.inc"
};

/// Indexed collision quads carrying the tunnel's surface classes 0 and 4.
static WorldCollisionGridFace _gNeoArkSubmarineTunnelCollisionGridFaces[] = {
#include "assets/neo_ark_submarine_tunnel_collision_04F1C_faces.inc"
};

/// Serialized face-index lists, terminated by WORLD_COLLISION_GRID_CELL_END.
///
/// Retains the complete authored storage, including words outside the selected lists.
static s16 _gNeoArkSubmarineTunnelCollisionGridCells[] = {
#include "assets/neo_ark_submarine_tunnel_collision_04F1C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gNeoArkSubmarineTunnelCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..101,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define NEO_ARK_SUBMARINE_TUNNEL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gNeoArkSubmarineTunnelCollisionGridCells[(halfwordOffset)])
/// X-major pointers to the tunnel grid's six-by-one cell face-index lists.
static s16* _gNeoArkSubmarineTunnelCollisionGridTable[NEO_ARK_SUBMARINE_TUNNEL_GRID_CELL_COUNT_X * NEO_ARK_SUBMARINE_TUNNEL_GRID_CELL_COUNT_Z] = {
#include "assets/neo_ark_submarine_tunnel_collision_04F1C_table.inc"
};
#undef NEO_ARK_SUBMARINE_TUNNEL_CELL_FACE_LIST_AT_OFFSET

/// The tunnel room's collision mesh and XZ cell index.
///
/// Activation binds the writable descriptor to the current view coordinate.
/// Cells span X [-10000, 14000) and Z [-1000, 3000), in room-coordinate units.
/// The descriptor and all mesh/index arrays remain live while the room is loaded.
static WorldCollisionGrid _gNeoArkSubmarineTunnelCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkSubmarineTunnelCollisionGridNormals,
    .vertices    = _gNeoArkSubmarineTunnelCollisionGridVerts,
    .faces       = _gNeoArkSubmarineTunnelCollisionGridFaces,
    .cellFaceIds = _gNeoArkSubmarineTunnelCollisionGridTable,
    .xBias       = 10000,
    .zBias       = 1000,
    .cellCountX  = NEO_ARK_SUBMARINE_TUNNEL_GRID_CELL_COUNT_X,
    .cellCountZ  = NEO_ARK_SUBMARINE_TUNNEL_GRID_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkSubmarineTunnelCollisionGridFaces),
};

ViewCamera gNeoArkSubmarineTunnelViewCameras[NEO_ARK_SUBMARINE_TUNNEL_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 0, 0xC350, 0 } }, 911 },
    { { { { 378, 0, -4078 }, { 741, 4027, 68 }, { 4010, -744, 372 } }, { -750, -2400, 200 } }, 257 },
    { { { { 652, 0, -4043 }, { 348, 4080, 56 }, { 4028, -352, 650 } }, { 2380, -2390, 300 } }, 257 },
    { { { { 652, 0, 4043 }, { -348, 4080, 56 }, { -4028, -352, 650 } }, { -2380, -2390, 300 } }, 257 },
    { { { { 378, 0, 4078 }, { -741, 4027, 68 }, { -4010, -744, 372 } }, { 750, -2400, 200 } }, 257 },
    { { { { 3110, 0, -2664 }, { -2032, 2649, -2372 }, { 1723, 3123, 2012 } }, { -3955, 65, 1270 } }, 257 },
    { { { { 851, 0, -4006 }, { -861, 4000, -182 }, { 3912, 880, 831 } }, { 2900, -1250, 500 } }, 380 },
    { { { { 723, 0, -4031 }, { 892, 3994, 160 }, { 3931, -907, 705 } }, { 650, -2000, 250 } }, 380 },
    { { { { 1283, 0, 3889 }, { -124, 4093, 41 }, { -3887, -130, 1282 } }, { -3070, -1600, 680 } }, 380 },
    { { { { 1027, 0, 3965 }, { 1153, 3918, -298 }, { -3793, 1191, 983 } }, { -560, -1900, 495 } }, 380 },
};

/// Background-image-only batch list for mapped view 1; selects no source sprites.
static SpriteBatch _gNeoArkSubmarineTunnelView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 2.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView2SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -40, 1450, { .fields = { 48, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -40, 1212, { .fields = { 40, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 353, { .fields = { 56, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 387, { .fields = { 72, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -120, 437, { .fields = { 72, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -120, 487, { .fields = { 40, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -120, 562, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 625, { .fields = { 120, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, -120, 750, { .fields = { 120, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -120, 700, { .fields = { 88, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, -120, 750, { .fields = { 88, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 750, { .fields = { 80, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 750, { .fields = { 104, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 750, { .fields = { 104, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -120, 750, { .fields = { 104, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 687, { .fields = { 96, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -120, 700, { .fields = { 96, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 522, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -120, 437, { .fields = { 112, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -120, 362, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -120, 287, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 250, { .fields = { 112, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 500, { .fields = { 120, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -80, 625, { .fields = { 112, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -80, 687, { .fields = { 48, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -80, 750, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -80, 812, { .fields = { 32, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -80, 875, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -80, 750, { .fields = { 32, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 0, -80, 750, { .fields = { 32, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -80, 750, { .fields = { 72, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -80, 1125, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -80, 1000, { .fields = { 72, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -80, 875, { .fields = { 64, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -80, 750, { .fields = { 56, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -80, 500, { .fields = { 56, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, -80, 250, { .fields = { 56, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -160, -40, 500, { .fields = { 112, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -144, -40, 625, { .fields = { 112, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -128, -40, 687, { .fields = { 16, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -40, 750, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -40, 875, { .fields = { 48, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -40, 1000, { .fields = { 24, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -40, 975, { .fields = { 24, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -40, 1025, { .fields = { 40, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -40, 1375, { .fields = { 104, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -40, 1437, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -32, -40, 1450, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -24, -40, 1450, { .fields = { 120, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -40, 1203, { .fields = { 16, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -40, 1450, { .fields = { 64, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -40, 1375, { .fields = { 40, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -40, 1250, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, -40, 1125, { .fields = { 40, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -40, 1000, { .fields = { 8, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -40, 875, { .fields = { 40, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, -40, 750, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, -40, 500, { .fields = { 96, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -56, 900, { .fields = { 80, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, -80, 975, { .fields = { 120, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -32, -80, 750, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -56, 900, { .fields = { 64, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 32, -80, 750, { .fields = { 120, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -56, 900, { .fields = { 72, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -56, 900, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 48, -80, 750, { .fields = { 104, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -80, 975, { .fields = { 24, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -80, 975, { .fields = { 64, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 16, 16 } }, -136, -120, 750, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -120, -120, 812, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -104, -104, 875, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -88, -80, 937, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -72, -64, 937, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -64, -56, 937, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -56, -48, 1375, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, -48, -40, 1437, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -40, -24, 1500, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -32, -16, 1500, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 48, -24, 1500, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 56, -40, 1437, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 64, -56, 1375, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 72, -72, 1187, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 80, -88, 1062, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 88, -104, 1037, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 96, -120, 450, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 104, -120, 450, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 40, 24, 1750, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -160, -48, 437, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -144, -32, 500, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -128, -24, 562, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -112, -16, 750, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, -96, -8, 1500, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -80, 0, 1750, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -64, 0, 2000, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -56, 0, 2250, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, -48, 8, 2500, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -40, 16, 2500, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -32, 16, 2750, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -24, 16, 3000, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 48, 16, 1625, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 56, 8, 1500, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 64, 0, 1437, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, 72, -8, 1375, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 80, -16, 1250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 112 } }, 144, -80, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, 128, -72, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 112, -48, 625, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, 96, -40, 750, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 88, -24, 1000, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -160, -120, 330, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -152, -120, 330, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 128, -120, 287, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -120, 262, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -72, -56, 1375, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, -64, -72, 1000, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, -56, -88, 1000, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -40, -104, 1000, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -32, -104, 1000, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -24, -112, 1375, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -16, -112, 1375, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 0, -112, 1375, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 16, -112, 1375, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 32, -112, 1375, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 40, -104, 1375, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 48, -104, 1375, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 56, -96, 1437, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 64, -88, 1375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 80, -56, 1125, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -40, -16, 1500, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -32, -40, 1500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -24, -48, 1500, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -16, -48, 1500, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -8, -56, 1500, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, 0, -56, 1500, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 24, -56, 1500, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, 40, -48, 1500, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 48, -40, 1500, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 56, -24, 1500, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -48, -96, 1000, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -48, -72, 1000, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 72, -80, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 72, -32, 1250, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 2, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 68, 0, 0, { 3, 0 } },
    { 68, 18, 0, 0, { 0, 0 } },
    { 86, 23, 0, 0, { 2, 0 } },
    { 109, ARRAY_SIZE(_gNeoArkSubmarineTunnelView2SpriteSources) - 109, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 3.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView3SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 375, { .fields = { 88, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 437, { .fields = { 88, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -120, 500, { .fields = { 88, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -120, 562, { .fields = { 96, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -120, 625, { .fields = { 96, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 750, { .fields = { 72, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, -120, 750, { .fields = { 72, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -120, 825, { .fields = { 80, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, -120, 950, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 1000, { .fields = { 80, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 1000, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 1000, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -120, 1000, { .fields = { 104, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 1000, { .fields = { 120, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -120, 1000, { .fields = { 104, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 1000, { .fields = { 104, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -120, 375, { .fields = { 72, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -120, 375, { .fields = { 40, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -120, 300, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 250, { .fields = { 48, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 375, { .fields = { 48, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, -80, 437, { .fields = { 120, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, -80, 500, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -80, 562, { .fields = { 24, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -80, 625, { .fields = { 40, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -80, 750, { .fields = { 32, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, -80, 1000, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -80, 1200, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, -80, 1237, { .fields = { 48, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -80, 1000, { .fields = { 48, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -80, 1250, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -80, 187, { .fields = { 24, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, -80, 125, { .fields = { 120, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, -80, 125, { .fields = { 8, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, -80, 125, { .fields = { 88, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, -40, 750, { .fields = { 0, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -64, -40, 1000, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -40, 1200, { .fields = { 56, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -32, -40, 1250, { .fields = { 64, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -24, -40, 1500, { .fields = { 104, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -40, 2000, { .fields = { 80, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -8, -40, 1600, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -40, 1600, { .fields = { 8, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, -40, 1875, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1000, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -40, 1250, { .fields = { 24, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, -80, 1125, { .fields = { 64, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -8, -80, 1187, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1125, { .fields = { 40, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 0, -80, 1250, { .fields = { 32, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -80, 1250, { .fields = { 8, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 32, -80, 1250, { .fields = { 8, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, -80, 1250, { .fields = { 8, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 48, -80, 1000, { .fields = { 24, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -80, 1250, { .fields = { 8, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, -80, 1250, { .fields = { 120, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 0, -48, 1600, { .fields = { 16, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -56, 1500, { .fields = { 8, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, -56, 1500, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -56, 1500, { .fields = { 64, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 32, -56, 1500, { .fields = { 56, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 40, -56, 1250, { .fields = { 120, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 48, -48, 1250, { .fields = { 120, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 0, -24, 1875, { .fields = { 112, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, -40, 2000, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 16, -40, 2000, { .fields = { 120, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 40, -40, 2000, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 48, -32, 2000, { .fields = { 120, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, -40, 2000, { .fields = { 112, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 104, -120, 625, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, -112, -120, 625, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -96, -120, 687, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -80, -104, 812, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -64, -88, 1125, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -48, -72, 1250, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -32, -56, 1250, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -24, -48, 1250, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -16, -40, 1500, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, -32, 1750, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 0, -24, 2000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 56, -32, 2000, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 64, -56, 2000, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 72, -72, 1750, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, -96, 1500, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, -112, 1250, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 96, -120, 750, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -48, -120, 875, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, -80, -112, 875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, -72, -120, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, -64, -120, 875, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -56, -120, 875, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -32, -64, 1275, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -24, -72, 1525, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -16, -80, 1200, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -8, -88, 1200, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 0, -88, 1081, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 8, -96, 1125, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 24, -96, 1125, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 40, -96, 1200, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 64, -80, 2000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, 72, -64, 1500, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -8, -48, 1625, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 16, -40, 3000, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 24, 16 } }, 16, -64, 1625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 8 } }, 24, -40, 3000, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 72, -120, 642, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 80, -120, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 88, -120, 750, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 96, -120, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, 104, -80, 250, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 56, -96, 1200, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 56, -56, 1625, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 48, -32, 3000, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 48, -56, 1625, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 40, -56, 1625, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 40, -40, 3000, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 8, -56, 1625, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 0, -56, 1625, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 8, -40, 3000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, 0, -24, 3000, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 8, 8, 3000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -160, -64, 500, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, -144, -56, 625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -128, -48, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -112, -40, 875, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -96, -32, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -80, -32, 1125, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, -32, 1250, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -48, -16, 1500, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, -8, 1625, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -24, -8, 1750, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, 0, 2000, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, 0, 2500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 48, 0, 3000, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 0, 0, 3000, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 56, -8, 2500, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 64, -16, 2000, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, 72, -24, 1500, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 80, -40, 1250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 136 } }, 144, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 120 } }, 128, -104, 375, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, 112, -88, 500, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 96, -64, 750, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, 88, -48, 1000, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 3, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 69, 0, 0, { 3, 0 } },
    { 69, 17, 0, 0, { 0, 0 } },
    { 86, 34, 0, 0, { 2, 0 } },
    { 120, ARRAY_SIZE(_gNeoArkSubmarineTunnelView3SpriteSources) - 120, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 4.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView4SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, -72, 250, { .fields = { 24, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, -120, 250, { .fields = { 112, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, -120, 375, { .fields = { 104, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, -120, 500, { .fields = { 56, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, -120, 562, { .fields = { 80, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -96, -120, 737, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 858, { .fields = { 40, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 773, { .fields = { 56, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 48, -120, 875, { .fields = { 88, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 64, -120, 875, { .fields = { 96, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 80, -120, 750, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -120, 625, { .fields = { 112, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, -120, 562, { .fields = { 56, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, -120, 500, { .fields = { 0, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, -120, 437, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -72, 375, { .fields = { 32, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -72, 562, { .fields = { 32, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, -72, 612, { .fields = { 72, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 144, -72, 437, { .fields = { 24, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -72, 500, { .fields = { 64, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -72, 562, { .fields = { 24, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -72, 625, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, -72, 875, { .fields = { 32, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -72, 1250, { .fields = { 104, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -48, -72, 1375, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -40, -72, 1375, { .fields = { 40, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, -72, 1375, { .fields = { 8, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -24, -72, 1375, { .fields = { 120, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -72, 1375, { .fields = { 32, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 16, -72, 1500, { .fields = { 80, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 24, -72, 1375, { .fields = { 64, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, -72, 1250, { .fields = { 64, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, -72, 1125, { .fields = { 72, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -72, 1062, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -72, 1000, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -72, 875, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -72, 750, { .fields = { 8, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -8, -72, 1375, { .fields = { 32, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -8, -32, 1834, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 8, -48, 1625, { .fields = { 24, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 0, -64, 1750, { .fields = { 88, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, -120, 786, { .fields = { 0, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, -72, 1312, { .fields = { 80, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -80, -72, 1250, { .fields = { 72, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -80, -120, 717, { .fields = { 112, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -48, 1375, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -64, 1375, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -72, 1000, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -56, -80, 1000, { .fields = { 120, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -48, -88, 1000, { .fields = { 112, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -88, 1000, { .fields = { 80, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -16, -88, 1000, { .fields = { 120, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 0, -80, 1088, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, -72, 1103, { .fields = { 8, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -120, 847, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -120, 871, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, -120, 893, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -120, 919, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -120, 915, { .fields = { 8, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 0, -120, 918, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 8, -120, 915, { .fields = { 80, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 8 } }, -112, -120, 875, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, -120, 1000, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -96, -112, 1250, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -88, -96, 1312, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -80, -72, 1312, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -72, -56, 1375, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -64, -32, 1625, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 96, -120, 500, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 80, -120, 750, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 64, -104, 1000, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 48, -88, 1250, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 32, -72, 1500, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 24, -56, 1750, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 16, -48, 1750, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 8, -40, 2000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 0, -32, 2250, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, -24, 2500, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -16, 8, 3000, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 136 } }, -160, -120, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 120 } }, -144, -104, 750, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, -128, -88, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -112, -64, 1250, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -96, -48, 1500, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -80, -24, 1750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -72, -16, 2000, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -64, -8, 2250, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, -56, 0, 2500, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -64, 375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, 128, -56, 437, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 112, -48, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, 96, -40, 750, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 80, -32, 1000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 64, -32, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 48, -32, 1500, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 32, -16, 1750, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, -8, 1875, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 16, -8, 2000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, 0, 3000, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 0, 0, 2500, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 8, 0, 2250, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 40, -120, 900, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -112, -80, 625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -80, -120, 750, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -104, -120, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -96, -120, 1000, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -88, -120, 750, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -88, -72, 750, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -96, -72, 1000, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -104, -72, 750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -80, -64, 1400, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, -72, -80, 2000, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -64, -88, 1475, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -64, -40, 2125, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -56, -32, 2000, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -56, -56, 1625, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -48, -56, 1625, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -48, -40, 2250, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -40, -64, 1625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 8 } }, -40, -40, 2250, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -24, -40, 2250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, -56, 1625, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -16, -40, 2250, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -8, -32, 2250, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -8, -56, 1625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -48, 2125, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, -56, -88, 1046, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -48, -96, 1050, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -32, -96, 1071, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -16, -96, 1071, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 0, -88, 1125, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 8, -80, 1500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 16, -72, 1625, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, 24, -64, 1400, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 48, -120, 900, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 56, -120, 900, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, 64, -120, 900, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 72, -112, 900, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 56, -64, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 64, -64, 900, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 72, -64, 900, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 4, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 61, 0, 0, { 3, 0 } },
    { 61, 17, 0, 0, { 0, 0 } },
    { 78, 23, 0, 0, { 2, 0 } },
    { 101, ARRAY_SIZE(_gNeoArkSubmarineTunnelView4SpriteSources) - 101, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 5.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView5SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 262, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 287, { .fields = { 48, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -120, 362, { .fields = { 32, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -120, 425, { .fields = { 32, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -120, 525, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 750, { .fields = { 16, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -120, 750, { .fields = { 32, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, -120, 750, { .fields = { 16, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 775, { .fields = { 32, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 775, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 750, { .fields = { 48, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -120, 725, { .fields = { 48, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 775, { .fields = { 40, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, -120, 625, { .fields = { 64, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -120, 550, { .fields = { 64, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -120, 475, { .fields = { 64, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -120, 425, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -120, 387, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 350, { .fields = { 88, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 187, { .fields = { 56, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -80, 500, { .fields = { 24, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, -80, 750, { .fields = { 40, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, -80, 1000, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -80, 875, { .fields = { 24, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -96, -80, 1250, { .fields = { 112, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -80, 750, { .fields = { 96, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -32, -80, 875, { .fields = { 48, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -16, -80, 875, { .fields = { 16, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 0, -80, 875, { .fields = { 120, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -80, 875, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -80, 875, { .fields = { 112, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -80, 775, { .fields = { 0, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, -80, 812, { .fields = { 0, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -80, 687, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -80, 687, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -80, 625, { .fields = { 88, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, -80, 562, { .fields = { 24, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -80, 562, { .fields = { 112, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, -32, 1250, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, -32, 1250, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -32, 1375, { .fields = { 8, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -32, 1500, { .fields = { 16, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -32, 1750, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, -32, 2000, { .fields = { 24, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 16, -32, 1000, { .fields = { 104, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 24, -32, 1500, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -32, 1375, { .fields = { 40, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -32, 1250, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -32, 1125, { .fields = { 32, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 56, -32, 1062, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 64, -32, 1000, { .fields = { 112, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 72, -32, 1000, { .fields = { 96, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, -32, 750, { .fields = { 80, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -40, 750, { .fields = { 80, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, -40, 687, { .fields = { 0, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, -48, 625, { .fields = { 8, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 144, -48, 625, { .fields = { 0, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -120, 750, { .fields = { 112, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, -72, 900, { .fields = { 72, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, -112, -120, 450, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -104, -120, 750, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 32 } }, -96, -104, 1250, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 32 } }, -88, -88, 1375, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 32 } }, -80, -72, 1400, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 32 } }, -72, -56, 1425, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -64, -40, 1875, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -56, -24, 2250, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, 120, -120, 450, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 104, -120, 500, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, 88, -104, 750, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 72, -80, 837, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 64, -64, 825, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 56, -56, 825, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 48, -48, 1250, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 40, -40, 1312, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 32, -24, 1500, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 24, -16, 1625, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -48, 24, 3000, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 120 } }, -160, -88, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 104 } }, -144, -72, 250, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, -128, -56, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, -112, -40, 750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -96, -24, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -80, -8, 1500, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -72, 0, 2000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, -64, 8, 2250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -56, 16, 2500, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -48, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 128, -32, 562, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, 112, -24, 750, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 96, -16, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, 80, -8, 1750, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 64, 0, 2000, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 48, 0, 2250, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 40, 8, 2500, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 32, 16, 2500, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 24, 16, 2750, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 16, 16, 3000, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -160, -120, 258, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -144, -120, 283, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 16 } }, 144, -120, 355, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 80 } }, 152, -120, 330, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -88, -56, 1375, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, -80, -80, 1400, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -64, -96, 812, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -56, -104, 812, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -48, -104, 812, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -40, -112, 775, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -32, -112, 775, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -16, -112, 800, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 0, -112, 800, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, 16, -112, 775, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 24, -104, 775, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, 32, -104, 750, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 40, -96, 900, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 48, -88, 1000, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 56, -72, 1075, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 64, -56, 1062, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -64, -24, 2062, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, -56, -40, 2125, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -48, -48, 1100, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -40, -56, 1100, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -24, -56, 1125, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -8, -56, 1125, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 24 } }, 8, -48, 1125, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 16, -48, 1250, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 24, -40, 1562, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 32, -16, 1500, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -72, -88, 812, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -72, -48, 1375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 5, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 59, 0, 0, { 3, 0 } },
    { 59, 18, 0, 0, { 0, 0 } },
    { 77, 21, 0, 0, { 2, 0 } },
    { 98, ARRAY_SIZE(_gNeoArkSubmarineTunnelView5SpriteSources) - 98, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-image-only batch list for mapped view 6; selects no source sprites.
static SpriteBatch _gNeoArkSubmarineTunnelView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 7.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, -120, 512, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -24, 512, { .fields = { 80, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -120, 555, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -32, 555, { .fields = { 112, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -120, 600, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, -32, 600, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, -120, 650, { .fields = { 64, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -40, 650, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -120, 1000, { .fields = { 40, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, -40, 1000, { .fields = { 80, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -120, 1250, { .fields = { 8, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -80, -48, 1250, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -64, -120, 1500, { .fields = { 72, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -64, -56, 1500, { .fields = { 64, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -48, -120, 1750, { .fields = { 56, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -48, -64, 1750, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -32, -120, 2000, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -32, -64, 2000, { .fields = { 40, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -16, -120, 2250, { .fields = { 40, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -16, -72, 2250, { .fields = { 40, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 0, -120, 2500, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 0, -72, 2500, { .fields = { 16, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 2750, { .fields = { 16, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 16, -80, 2750, { .fields = { 120, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 2000, { .fields = { 8, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, -120, 2000, { .fields = { 0, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, -120, 800, { .fields = { 16, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 88, -120, 775, { .fields = { 48, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -72, 775, { .fields = { 32, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 96, -120, 750, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 96, -64, 750, { .fields = { 56, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 104, -120, 675, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 104, -40, 675, { .fields = { 104, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 112, -120, 550, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 112, -40, 550, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 120, -120, 525, { .fields = { 104, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, -56, 525, { .fields = { 24, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 120, 16, 525, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, -120, 450, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, -48, 450, { .fields = { 0, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 128, 24, 450, { .fields = { 56, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 136, -120, 425, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, 136, -32, 425, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 425, { .fields = { 56, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 144, -120, 250, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, 144, -32, 250, { .fields = { 120, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 144, 40, 250, { .fields = { 80, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, 40, 237, { .fields = { 48, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -32, 237, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 152, -120, 237, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 8 } }, -8, -80, 2750, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, -160, -72, 750, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, -144, -72, 750, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 8 } }, -128, -72, 750, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 8 } }, -112, -72, 1000, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 72 } }, -96, -120, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 64 } }, -96, -48, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 64 } }, -80, -48, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 72 } }, -80, -120, 1500, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -64, -120, 1750, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 16 } }, -64, -80, 1750, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, -48, -80, 2000, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 80 } }, -32, -104, 2250, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 56 } }, -24, -72, 2500, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 48 } }, -24, -120, 2500, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 24 } }, -8, -120, 2750, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 64 } }, 0, -96, 2750, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 40 } }, 8, -120, 3000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 40 } }, 8, -80, 3000, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 32 } }, 24, -80, 3000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 40 } }, 24, -120, 3000, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, 40, -120, 2025, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, 56, -120, 2025, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 24 } }, 72, -120, 2025, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 48 } }, 88, -120, 1905, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 40 } }, 88, -72, 2012, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 56 } }, 96, -120, 1100, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 48 } }, 96, -64, 1127, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 72 } }, 104, -120, 927, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 72 } }, 104, -48, 695, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 72 } }, 112, -48, 665, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 72 } }, 112, -120, 660, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 72 } }, 120, -120, 567, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 72 } }, 120, -48, 545, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 72 } }, 128, -48, 481, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 72 } }, 128, -120, 475, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 56 } }, 136, -120, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 56 } }, 136, -64, 245, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 40 } }, 136, 80, 350, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 80 } }, 144, -120, 245, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 88 } }, 144, -40, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 72 } }, 144, 48, 262, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 72 } }, 152, 48, 252, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 88 } }, 152, -40, 247, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 80 } }, 152, -120, 235, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 7, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 50, 0, 0, { 1, 0 } },
    { 50, 0, 0, 0, { 2, 0 } },
    { 50, ARRAY_SIZE(_gNeoArkSubmarineTunnelView7SpriteSources) - 50, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 8.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView8SpriteSources[] = {
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -160, -120, 625, { .fields = { 88, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -160, -16, 625, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -144, -120, 750, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -144, -16, 750, { .fields = { 112, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -128, -120, 875, { .fields = { 40, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -128, -16, 875, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -112, -120, 1000, { .fields = { 56, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -112, -16, 1000, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -96, -120, 1125, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -96, -16, 1125, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -80, -120, 1250, { .fields = { 8, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -80, -16, 1250, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -64, -120, 1375, { .fields = { 24, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -64, -16, 1375, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -48, -16, 1500, { .fields = { 104, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -48, -120, 1500, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -32, -120, 1625, { .fields = { 120, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -32, -16, 1625, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -16, -16, 1750, { .fields = { 72, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 0, -16, 1000, { .fields = { 88, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -16, -120, 1750, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 0, -120, 650, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 80 } }, 16, -120, 650, { .fields = { 120, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 80 } }, 32, -120, 650, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 80 } }, 48, -120, 650, { .fields = { 120, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, 64, -120, 650, { .fields = { 8, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, -32, 1000, { .fields = { 48, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 16, -40, 1000, { .fields = { 8, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 64 } }, 8, -16, 1000, { .fields = { 88, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 16, -16, 1500, { .fields = { 24, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -40, 1000, { .fields = { 56, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 1000, { .fields = { 96, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, -40, 1000, { .fields = { 24, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 80, -120, 625, { .fields = { 8, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 96, -120, 625, { .fields = { 24, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 112, -120, 600, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 112, -16, 1000, { .fields = { 80, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 64 } }, 80, -32, 1000, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 96, -24, 1000, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 104, -24, 1000, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 120, -120, 600, { .fields = { 96, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 120, -16, 750, { .fields = { 104, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -16, 562, { .fields = { 72, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 128, -120, 605, { .fields = { 48, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 136, -120, 573, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 136, -16, 500, { .fields = { 40, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 144, -16, 500, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 144, -120, 537, { .fields = { 64, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 152, -120, 500, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 152, -16, 500, { .fields = { 56, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 32 } }, -160, -72, 750, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -144, -56, 875, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -128, -48, 1000, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -112, -40, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, -104, 32, 1125, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 72 } }, -96, 16, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 80 } }, -96, -64, 1250, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 72 } }, -80, 16, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 64 } }, -80, -48, 1375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 40 } }, -80, -88, 1375, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 48 } }, -64, -48, 1500, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, -64, -104, 1500, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, -48, -112, 1625, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 48 } }, -32, -120, 1750, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, -16, -120, 1875, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 32 } }, 0, -120, 677, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 16, -120, 677, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 32, -120, 677, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 48, -120, 677, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 32 } }, 64, -120, 677, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 40 } }, 80, -120, 677, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, 96, -120, 677, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, 112, -112, 625, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 72 } }, 120, -104, 620, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 64 } }, 128, -96, 612, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 48 } }, 128, -32, 612, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 64 } }, 136, -88, 600, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 64 } }, 136, -24, 600, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 48 } }, 136, 40, 662, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, 144, -80, 562, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, 144, -24, 587, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, 144, 32, 562, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 80 } }, 152, 8, 550, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 72 } }, 152, -64, 537, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, -64, 0, 1500, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, -48, 8, 1625, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 48 } }, -32, 40, 1750, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, -32, -16, 1750, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 64 } }, -16, -32, 1875, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, 0, -40, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, 16, -48, 1125, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 32, -48, 1125, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 48, -48, 1100, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, 64, -48, 1100, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, 80, -40, 1075, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, -16, 32, 1875, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 64 } }, 0, 24, 1525, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 0, 0, 1450, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, 16, -8, 1562, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 32, -8, 1575, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, 48, -8, 1537, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, 64, -8, 1537, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 56 } }, 80, -8, 1525, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 48 } }, 96, -32, 1057, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 72 } }, 96, 16, 1450, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 64 } }, 104, 24, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 48 } }, 104, -24, 1037, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 104 } }, 112, -16, 1012, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 88 } }, 120, 0, 875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 24 } }, 128, 64, 825, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 8, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView8SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 50, 0, 0, { 1, 0 } },
    { 50, 0, 0, 0, { 2, 0 } },
    { 50, ARRAY_SIZE(_gNeoArkSubmarineTunnelView8SpriteSources) - 50, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted textured room rectangles for mapped view 9.
///
/// Semitransparent rectangles precede opaque rectangles; positions and sizes
/// are in pixels relative to the view centre. Keep the room and view textures loaded.
static SpriteSource _gNeoArkSubmarineTunnelView9SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 144, -120, 625, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 144, -16, 625, { .fields = { 112, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 128, -16, 750, { .fields = { 96, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 128, -120, 750, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 112, -120, 875, { .fields = { 80, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 112, -16, 875, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 96, -24, 1000, { .fields = { 64, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 96, -120, 1000, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 80, -120, 1125, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 80, -24, 1125, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 64, -32, 1250, { .fields = { 48, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 64, -120, 1250, { .fields = { 0, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 48, -120, 1375, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 48, -32, 1375, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 32, -32, 1500, { .fields = { 16, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 32, -120, 1500, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 1625, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 16, -32, 1625, { .fields = { 112, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 0, -32, 1750, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 0, -120, 1750, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -16, -120, 750, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -16, -32, 875, { .fields = { 96, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -32, -32, 1250, { .fields = { 80, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -48, -32, 1250, { .fields = { 0, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -64, -32, 1250, { .fields = { 16, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -120, 750, { .fields = { 80, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -64, -72, 1250, { .fields = { 56, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -80, -72, 1125, { .fields = { 104, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -72, 1125, { .fields = { 96, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -104, -72, 1125, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, -64, 1125, { .fields = { 8, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -120, 725, { .fields = { 56, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 48 } }, -96, -120, 725, { .fields = { 88, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, -120, 700, { .fields = { 48, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 8, 56 } }, -112, -120, 700, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -120, -120, 500, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -128, -120, 275, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 88 } }, -136, -120, 275, { .fields = { 88, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -136, -32, 275, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, -144, -120, 275, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 88 } }, -144, -48, 275, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, -144, 40, 275, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 88 } }, -152, 32, 125, { .fields = { 80, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 80 } }, -152, -48, 125, { .fields = { 32, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, -152, -120, 25, { .fields = { 24, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 80 } }, -160, -120, 25, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 88 } }, -160, -40, 25, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, -160, 48, 25, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -48, -120, 750, { .fields = { 104, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 64 } }, -32, -120, 750, { .fields = { 120, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -32, -56, 1250, { .fields = { 56, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, -64, 1250, { .fields = { 16, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 16, 16 } }, 144, -40, 900, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, 128, -40, 1025, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, 112, -40, 1150, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, 96, -32, 1275, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, 80, -32, 1400, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, 64, -32, 1525, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 64 } }, 48, 0, 1400, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 64 } }, 48, -64, 1400, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 80 } }, 32, -16, 1525, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 72 } }, 32, -88, 1525, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 64 } }, 16, -104, 1650, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 40 } }, 0, -112, 1775, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 32 } }, -16, -120, 812, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -32, -120, 812, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -48, -120, 812, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -64, -120, 812, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 24 } }, -80, -120, 787, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, -96, -120, 750, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 40 } }, -112, -112, 720, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 8 } }, 16, -16, 1650, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 16 } }, 0, -16, 1775, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 48 } }, -16, 0, 1187, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 48 } }, -16, -48, 1187, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 48 } }, -24, 0, 1325, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 64 } }, -32, -64, 1337, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 64 } }, -48, -24, 1675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 48 } }, -48, -72, 1332, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, -64, -24, 1675, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 56 } }, -64, -80, 1312, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 40 } }, -80, -80, 1175, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 40 } }, -96, -80, 1175, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, -112, -72, 1175, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, -112, -40, 1675, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 16 } }, -96, -40, 1675, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 16, 32 } }, -80, -40, 1675, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 48 } }, -120, -120, 575, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, -120, -72, 1165, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, -128, -16, 285, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 8, 56 } }, -128, -72, 285, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 48 } }, -128, -120, 285, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -136, -120, 285, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -136, -40, 285, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -136, 40, 285, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -144, 40, 275, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -144, -40, 275, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -144, -120, 275, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -152, -120, 250, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -152, -40, 250, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -152, 40, 250, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -160, 40, 225, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -160, -40, 225, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 8, 80 } }, -160, -120, 225, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
};

/// Sprite ranges for mapped view 9, ending at SPRITE_BATCH_END.
///
/// The initial empty range keeps the decoded background image visible.
/// Later ranges index the matching source array in elements.
static SpriteBatch _gNeoArkSubmarineTunnelView9SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 52, 0, 0, { 1, 0 } },
    { 52, 0, 0, 0, { 2, 0 } },
    { 52, ARRAY_SIZE(_gNeoArkSubmarineTunnelView9SpriteSources) - 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Background-image-only batch list for mapped view 10; selects no source sprites.
static SpriteBatch _gNeoArkSubmarineTunnelView10SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkSubmarineTunnelSpriteViews[NEO_ARK_SUBMARINE_TUNNEL_VIEW_COUNT] = {
    { { .empty = _gNeoArkSubmarineTunnelView1SpriteBatches }, _gNeoArkSubmarineTunnelView1SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView2SpriteSources }, _gNeoArkSubmarineTunnelView2SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView3SpriteSources }, _gNeoArkSubmarineTunnelView3SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView4SpriteSources }, _gNeoArkSubmarineTunnelView4SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView5SpriteSources }, _gNeoArkSubmarineTunnelView5SpriteBatches, NULL },
    { { .empty = _gNeoArkSubmarineTunnelView6SpriteBatches }, _gNeoArkSubmarineTunnelView6SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView7SpriteSources }, _gNeoArkSubmarineTunnelView7SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView8SpriteSources }, _gNeoArkSubmarineTunnelView8SpriteBatches, NULL },
    { { .elements = _gNeoArkSubmarineTunnelView9SpriteSources }, _gNeoArkSubmarineTunnelView9SpriteBatches, NULL },
    { { .empty = _gNeoArkSubmarineTunnelView10SpriteBatches }, _gNeoArkSubmarineTunnelView10SpriteBatches, NULL },
};

/// Four directional lights admitted in every tunnel view.
///
/// Direction vectors use room coordinates and RGB intensities have 12 fractional bits.
/// Coordinate composition and shading overwrite the transforms and attenuation.
static WorldCoordLight _gNeoArkSubmarineTunnelDirectionalLights[] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 100, 100, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1642, 2052, 1642 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -100, 100, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1642, 2052, 1642 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 100, -100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1642, 2052, 1642 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 100, 100 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } },
};

/// Thirteen point lights admitted in every tunnel view.
///
/// Positions and falloff radii use room-coordinate units: full strength within
/// 1000 units, fading to zero at 2000. RGB intensities have 12 fractional bits;
/// coordinate composition and shading overwrite the transforms and attenuation.
static WorldCoordPointLight _gNeoArkSubmarineTunnelPointLights[] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x2710, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2710, -2000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8500, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7000, 0, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1825, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3670, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5470, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1830, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3630, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5530, 1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7000, 0, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8500, -1000, 0 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 2052, 2052 }, { 0, 0 } }, 1000, 2000 },
};

/// The tunnel room's four directional and thirteen point lights, with no cone lights.
///
/// Borrows writable light arrays whose transforms and attenuation change during play.
static WorldCoordRoomLights _gNeoArkSubmarineTunnelRoomLights = {
    .directionalLightCount = ARRAY_SIZE(_gNeoArkSubmarineTunnelDirectionalLights),
    .directionalLights     = _gNeoArkSubmarineTunnelDirectionalLights,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkSubmarineTunnelPointLights),
    .pointLights           = _gNeoArkSubmarineTunnelPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed view boundaries for 2 <-> 3 <-> 4 <-> 5, with LAST on the sixth quad.
///
/// Room setup binds the coordinate, links and enables every record. Positions
/// and radii use room-coordinate units; normals use ONE per unit. Mutable
/// list links and hit latches stay live until unlinked before the room unloads.
static WorldCollisionTrigger _gNeoArkSubmarineTunnelViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -3699, 1825, -52, 0 }, { { 12, -1904, -3017, 0 }, { -12, -1904, 3018, 0 }, { 12, 1904, -3017, 0 }, { -12, 1904, 3018, 0 } }, { ONE, 0, 16, 0 }, { 0, 0, ONE, 0 }, 3565, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3528, 1857, -133, 0 }, { { -18, -1904, 2884, 0 }, { 18, -1904, -2884, 0 }, { -18, 1904, 2884, 0 }, { 18, 1904, -2884, 0 } }, { -4109, 0, -27, 0 }, { 0, 0, ONE, 0 }, 3453, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 64, 1919, -176, 0 }, { { 0, -1904, 3056, 0 }, { 0, -1904, -3056, 0 }, { 0, 1904, 3056, 0 }, { 0, 1904, -3056, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3593, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -128, 1951, -176, 0 }, { { 0, -1904, -3040, 0 }, { 0, -1904, 3040, 0 }, { 0, 1904, -3040, 0 }, { 0, 1904, 3040, 0 } }, { 4109, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3584, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3360, 1824, -160, 0 }, { { 0, -1904, -3040, 0 }, { 0, -1904, 3040, 0 }, { 0, 1904, -3040, 0 }, { 0, 1904, 3040, 0 } }, { 4109, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3584, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3520, 1856, -96, 0 }, { { 0, -1904, 3056, 0 }, { 0, -1904, -3056, 0 }, { 0, 1904, 3056, 0 }, { 0, 1904, -3056, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, ONE, 0 }, 3593, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_DIVER      = 4,
    NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_AMOEBA     = 24,
    NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_AYA_WALKER = 132,
    NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_NO9_WALKER = 34,
    NEO_ARK_SUBMARINE_TUNNEL_DIVER_FILE          = 4,
    NEO_ARK_SUBMARINE_TUNNEL_AMOEBA_FILE         = 24,
    NEO_ARK_SUBMARINE_TUNNEL_SCENE_ACTOR_FILE    = 511,
};

/// Variant 1's Bog Diver load/spawn recipe, ending at AREA_PLACEMENT_END.
///
/// Placement entry 4 loads file 100400 and spawns body descriptor 0 for both
/// Divers. The resource list is borrowed during loading and actor spawning;
/// its task table belongs to the loaded actor package.
static AreaResource _gNeoArkSubmarineTunnelVariant1Resources[] = {
    {
        .entryId        = NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_DIVER,
        .fileNumber     = NEO_ARK_SUBMARINE_TUNNEL_DIVER_FILE,
        .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
        .taskIndex      = ACTOR_00400_BODY_TASK_INDEX,
        .taskTable      = gActor100400Tasks,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Amoeba load/spawn recipe, ending at AREA_PLACEMENT_END.
///
/// Placement entry 24 loads file 102400 and spawns descriptor 0 for both
/// Amoebas. The resource list is borrowed during loading and actor spawning;
/// its task table belongs to the loaded actor package.
static AreaResource _gNeoArkSubmarineTunnelVariant2Resources[] = {
    {
        .entryId        = NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_AMOEBA,
        .fileNumber     = NEO_ARK_SUBMARINE_TUNNEL_AMOEBA_FILE,
        .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR,
        .taskIndex      = 0,
        .taskTable      = gActor102400Tasks,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 3's Aya-body and No. 9 GOLEM scene-walker recipes, then AREA_PLACEMENT_END.
///
/// Entry 132 loads actor file 451100; entry 34 loads its additional 651100
/// textures. Both spawn descriptor 0 from actor_451100's respective task tables.
/// Keep this list live during loading/spawning and that actor package live
/// through walker teardown.
static AreaResource _gNeoArkSubmarineTunnelVariant3Resources[] = {
    {
        .entryId        = NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_AYA_WALKER,
        .fileNumber     = NEO_ARK_SUBMARINE_TUNNEL_SCENE_ACTOR_FILE,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40,
        .taskIndex      = 0,
        .taskTable      = &gActor451100QuietWalkTaskDesc,
    },
    {
        .entryId        = NEO_ARK_SUBMARINE_TUNNEL_RESOURCE_NO9_WALKER,
        .fileNumber     = NEO_ARK_SUBMARINE_TUNNEL_SCENE_ACTOR_FILE,
        .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60,
        .taskIndex      = 0,
        .taskTable      = gActor451100PairWalkTaskDescs,
    },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 4's present but empty resource list: only AREA_PLACEMENT_END.
///
/// Paired with an empty placement list; it queues no files and spawns no actors.
static AreaResource _gNeoArkSubmarineTunnelVariant4Resources[] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gNeoArkSubmarineTunnelAreaVariants[NEO_ARK_SUBMARINE_TUNNEL_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkSubmarineTunnelVariant1Placements, .resources = _gNeoArkSubmarineTunnelVariant1Resources },
    [2]                    = { .placements = gMapNeoArkSubmarineTunnelVariant2Placements, .resources = _gNeoArkSubmarineTunnelVariant2Resources },
    [3]                    = { .placements = gMapNeoArkSubmarineTunnelVariant3Placements, .resources = _gNeoArkSubmarineTunnelVariant3Resources },
    [4]                    = { .placements = gMapNeoArkSubmarineTunnelVariant4Placements, .resources = _gNeoArkSubmarineTunnelVariant4Resources },
};

// Stair parameters pack surface class 4 above a low-nibble step count; yaw
// parameters use 256 units per turn. Warp bytes pack departure/arrival slots.
enum {
    NEO_ARK_SUBMARINE_TUNNEL_STAIR_DESCEND           = 0x100,
    NEO_ARK_SUBMARINE_TUNNEL_STAIR_ASCENT            = (4 << 4) | 9,
    NEO_ARK_SUBMARINE_TUNNEL_STAIR_DESCENT           = (4 << 4) | 3,
    NEO_ARK_SUBMARINE_TUNNEL_STAIR_YAW_POSITIVE_X    = 64,
    NEO_ARK_SUBMARINE_TUNNEL_STAIR_YAW_NEGATIVE_X    = 192,
    NEO_ARK_SUBMARINE_TUNNEL_WARP_TO_FOREST_ZONE     = (1 << 4) | 2,
    NEO_ARK_SUBMARINE_TUNNEL_WARP_TO_PAVILION        = (2 << 4) | 1,
    NEO_ARK_SUBMARINE_TUNNEL_ACTION_ADVANCE_PROGRESS = 1,
    NEO_ARK_SUBMARINE_TUNNEL_ACTION_EVENT_OR_WARP_1  = 2,
    NEO_ARK_SUBMARINE_TUNNEL_ACTION_ENTRY_OR_WARP_2  = 3,
    NEO_ARK_SUBMARINE_TUNNEL_CAP_POSITIVE_Z          = 4,
    NEO_ARK_SUBMARINE_TUNNEL_CAP_NEGATIVE_Z          = 5,
};

/// Eleven tunnel action quads for stairs, exits, entry scenes and side-wall CAP commands.
///
/// Final quad carries LAST. Setup binds, links and enables these writable
/// records; hits and links survive until cleared/unlinked. Positions and radii
/// use room-coordinate units, normals use ONE per unit, and facing yaws use
/// 256 units per turn. Keep the room overlay live while any record is linked.
static WorldCollisionTrigger _gNeoArkSubmarineTunnelActionTriggers[11] = {
    { NULL, NULL, NULL, { 5040, 2947, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_FACING, NEO_ARK_SUBMARINE_TUNNEL_STAIR_ASCENT, NEO_ARK_SUBMARINE_TUNNEL_STAIR_YAW_POSITIVE_X, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5088, 2956, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_FACING, NEO_ARK_SUBMARINE_TUNNEL_STAIR_ASCENT, NEO_ARK_SUBMARINE_TUNNEL_STAIR_YAW_NEGATIVE_X, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5984, 736, 0, 0 }, { { 0, -2896, -1024, 0 }, { 0, 2896, -1024, 0 }, { 0, -2896, 1024, 0 }, { 0, 2896, 1024, 0 } }, { -4111, 0, 0, 0 }, { -ONE, 0, 0, 0 }, 3061, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_NEO_ARK_FOREST_ZONE, NEO_ARK_SUBMARINE_TUNNEL_WARP_TO_FOREST_ZONE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6144, 752, 0, 0 }, { { 0, 3008, -1024, 0 }, { 0, -3008, -1024, 0 }, { 0, 3008, 1024, 0 }, { 0, -3008, 1024, 0 } }, { ONE, 0, 0, 0 }, { ONE, 0, 0, 0 }, 3176, WORLD_COLLISION_TRIGGER_ACTION_WARP | WORLD_COLLISION_TRIGGER_AUTOMATIC, GAME_AREA_NEO_ARK_PAVILION, NEO_ARK_SUBMARINE_TUNNEL_WARP_TO_PAVILION, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4032, 2944, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_SUBMARINE_TUNNEL_ACTION_ADVANCE_PROGRESS, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 6080, 2256, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_FACING | NEO_ARK_SUBMARINE_TUNNEL_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_SUBMARINE_TUNNEL_STAIR_DESCENT, NEO_ARK_SUBMARINE_TUNNEL_STAIR_YAW_NEGATIVE_X, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5088, 2931, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_SUBMARINE_TUNNEL_ACTION_EVENT_OR_WARP_1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6112, 2272, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_FACING | NEO_ARK_SUBMARINE_TUNNEL_STAIR_DESCEND | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_SUBMARINE_TUNNEL_STAIR_DESCENT, NEO_ARK_SUBMARINE_TUNNEL_STAIR_YAW_POSITIVE_X, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4992, 2944, 0, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -ONE, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, NEO_ARK_SUBMARINE_TUNNEL_ACTION_ENTRY_OR_WARP_2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, 2944, 608, 0 }, { { -6544, 0, -416, 0 }, { 6544, 0, -416, 0 }, { -6544, 0, 416, 0 }, { 6544, 0, 416, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -ONE, 0 }, 6556, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_TUNNEL_CAP_POSITIVE_Z, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, 2944, -448, 0 }, { { -6544, 0, -416, 0 }, { 6544, 0, -416, 0 }, { -6544, 0, 416, 0 }, { 6544, 0, 416, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, ONE, 0 }, 6556, WORLD_COLLISION_TRIGGER_ACTION_CAP, NEO_ARK_SUBMARINE_TUNNEL_CAP_NEGATIVE_Z, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Three room-space sight-blocking quads, with LAST on the third record.
///
/// Positions and radii use room-coordinate units; normals use ONE per unit.
/// Setup links and enables these writable records. Retained low flag bit 0
/// has no established meaning; keep all records live until their list is cleared.
static WorldCollisionOccluder _gNeoArkSubmarineTunnelSightOccluders[3] = {
    { NULL, NULL, { 0, 576, 0, 0 }, { { -0x2720, 0, -1024, 0 }, { 0x2720, 0, -1024, 0 }, { -0x2720, 0, 1024, 0 }, { 0x2720, 0, 1024, 0 } }, { 0, 4109, 0, 0 }, 10032, 1, 0 },
    { NULL, NULL, { 0, 1904, 896, 0 }, { { -0x2720, -1456, -224, 0 }, { 0x2720, -1456, -224, 0 }, { -0x2720, 1456, 224, 0 }, { 0x2720, 1456, 224, 0 } }, { 0, 622, -4048, 0 }, 10084, 1, 0 },
    { NULL, NULL, { 0, 1920, -832, 0 }, { { -0x2720, -1459, 208, 0 }, { 0x2720, -1459, 208, 0 }, { -0x2720, 1458, -209, 0 }, { 0x2720, 1458, -209, 0 } }, { 0, -580, -4055, 0 }, 10084, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

enum {
    NEO_ARK_SUBMARINE_TUNNEL_SURFACE_4_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 21,
    NEO_ARK_SUBMARINE_TUNNEL_SURFACE_4_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 23,
};

/// Surface-class-4 footstep cue bases from the loaded type-1 sound-script bank.
///
/// Walking and scripted jumps share a pair; running selects the next pair.
/// The player adds one for cue 1 and 100 for companion cues. Keep the room loaded.
static const WorldCollisionFootstepSounds _gNeoArkSubmarineTunnelSurface4FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_SUBMARINE_TUNNEL_SURFACE_4_WALK_SOUND,
    .run          = NEO_ARK_SUBMARINE_TUNNEL_SURFACE_4_RUN_SOUND,
    .scriptedJump = NEO_ARK_SUBMARINE_TUNNEL_SURFACE_4_WALK_SOUND,
};

/// Blocking, impact-enabled surface policies for classes without footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkSubmarineTunnelDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Surface-class-4 collision policies and paired footstep cues.
///
/// Blocks probes, permits weapon impacts and applies collision pushback.
static WorldCollisionSurfaceProperties _gNeoArkSubmarineTunnelSurface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkSubmarineTunnelSurface4FootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkSubmarineTunnelSurfaceProperties[NEO_ARK_SUBMARINE_TUNNEL_SURFACE_CLASS_COUNT] = {
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
    &_gNeoArkSubmarineTunnelSurface4Properties,
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
    &_gNeoArkSubmarineTunnelDefaultSurfaceProperties,
};

ScreenWaveCtx* gScreenWaveContext = NULL;

ScreenWaveOscillator gScreenWaveColumnWaves[13];

ScreenWaveOscillator gScreenWaveRowWaves[32];

ScreenWaveCtx gScreenWaveSpawnCtx;

void neoArkSubmarineTunnelConfigureEffectsTask(Task* task)
{
    enum { EFFECTS_UNREGISTERED,
           EFFECTS_REGISTERED };

    if (task->state == EFFECTS_UNREGISTERED) {
        gRoomEffectGlowDiscId     = EFFECT_NEO_ARK_SUBMARINE_TUNNEL_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_NEO_ARK_SUBMARINE_TUNNEL_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_NEO_ARK_SUBMARINE_TUNNEL_ORANGE_BURST_2;
        task->state               = EFFECTS_REGISTERED;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flying_tasks.inc.c"

void neoArkSubmarineTunnelRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void neoArkSubmarineTunnelRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room/room_visual_effects_burst.inc.c"

void neoArkSubmarineTunnelRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_burst_draw.inc.c"
