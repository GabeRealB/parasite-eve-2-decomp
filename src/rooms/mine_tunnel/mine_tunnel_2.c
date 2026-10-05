#include "rooms/mine_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

/// The tunnel's five light anchors, one `SVECTOR` each in one run; each view
/// draws a subset of them.

// Indexed views below share one contiguous table.
extern WorldCollisionGrid         D_mine_tunnel_8017E86C[1];
extern WorldCollisionOccluder     D_mine_tunnel_8018025C[3];
extern WorldCollisionTrigger      D_mine_tunnel_8017FBD8[6];
extern WorldCollisionTrigger      D_mine_tunnel_8017FDA0[4];
extern WorldCoordRoomAmbientEntry D_mine_tunnel_8018022C[6];
extern WorldCoordRoomLights       D_mine_tunnel_8017FBC0[1];

extern SpriteBatch  D_mine_tunnel_8017E944[2];
extern SpriteBatch  D_mine_tunnel_8017EA1C[4];
extern SpriteSource D_mine_tunnel_8017E954[10];

SVECTOR D_mine_tunnel_8017E12C[5] = {
    { 11650, -1730, 3690, 0 },
    { 4690, -1700, 3570, 0 },
    { 15320, -1740, 300, 0 },
    { 7870, -1660, 190, 0 },
    { 1170, -1820, 400, 0 },
};

WorldCoordRoomLighting D_mine_tunnel_8017E154[1] = {
    { D_mine_tunnel_8017FBC0, D_mine_tunnel_8018022C },
};

WorldCollisionRoomResources D_mine_tunnel_8017E15C[1] = {
    { D_mine_tunnel_8017E86C, D_mine_tunnel_8017FBD8, D_mine_tunnel_8017FDA0, D_mine_tunnel_8018025C },
};

u8* D_mine_tunnel_8017E16C[1] = {
    gViewIdentityMap,
};

ViewCount D_mine_tunnel_8017E170[1] = { 5 };

DirectionWarpEntry D_mine_tunnel_8017E174[2] = {
    { { { .word = 3072 }, 0x3CF0, 0, 1200 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x3CF0, 0, 1200 }, { 0, 0, 0, 0 }, 0x54040002, 0x54040001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, 500, 0, 1630 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 500, 0, 1630 }, { 0, 0, 0, 0 }, 0x54040004, 0x54040003, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMineTunnelCollision012ACNormals[29] = {
#include "assets/mine_tunnel_collision_012AC_normals.inc"
};

static SVECTOR _gMineTunnelCollision012ACVerts[80] = {
#include "assets/mine_tunnel_collision_012AC_verts.inc"
};

static WorldCollisionGridFace _gMineTunnelCollision012ACFaces[37] = {
#include "assets/mine_tunnel_collision_012AC_faces.inc"
};

static s16 _gMineTunnelCollision012ACCells[158] = {
#include "assets/mine_tunnel_collision_012AC_cells.inc"
};

#define GRID_CELL(i) (&_gMineTunnelCollision012ACCells[i])
static s16* _gMineTunnelCollision012ACTable[10] = {
#include "assets/mine_tunnel_collision_012AC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_tunnel_8017E86C[1] = {
    { NULL, _gMineTunnelCollision012ACNormals, _gMineTunnelCollision012ACVerts, _gMineTunnelCollision012ACFaces, _gMineTunnelCollision012ACTable, 1000, 1000, 5, 2, 4000, 37 },
};

ViewCamera D_mine_tunnel_8017E890[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -8000, 0x4330, -2000 } }, 289 },
    { { { { 611, 0, -4050 }, { 102, 4094, 15 }, { 4048, -103, 611 } }, { -0x2E18, 900, -1000 } }, 246 },
    { { { { 1296, 0, 3885 }, { 1147, 3913, -382 }, { -3712, 1209, 1238 } }, { -0x3FAC, 1700, -500 } }, 246 },
    { { { { 1051, 0, -3958 }, { -1321, 3860, -351 }, { 3731, 1367, 991 } }, { -1921, 2345, -438 } }, 289 },
    { { { { 1074, 0, 3952 }, { 1078, 3940, -293 }, { -3802, 1117, 1033 } }, { -7838, 2262, -891 } }, 257 },
};

SpriteBatch D_mine_tunnel_8017E944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_8017E954[10] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -120, 577, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 104, 532, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -120, 594, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -48, 596, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, 24, 595, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -120, 435, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -40, 437, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, 40, 439, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, 0, 973, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, 32, 973, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_8017EA1C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_8017EA3C[51] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -120, 577, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, -120, 580, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, 96, -120, 693, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 104, -88, 702, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 96, -56, 730, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 96, -24, 764, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 88, 16, 802, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 104, 56, 781, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 16, 667, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 64, 650, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -120, -48, 617, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, -48, 600, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, -24, 600, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 16, 612, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -120, -120, 1406, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -88, 1380, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -112, -72, 1425, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -112, -40, 1476, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -8, 1487, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -80, -120, 1391, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, -120, 1432, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 0, -120, 1569, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 8, -96, 1553, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 8, -64, 1613, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 8, -32, 1670, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -120, 821, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 0, 1184, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -16, 1130, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -24, 1021, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -120, 1090, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -16, 966, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, -72, 1112, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -120, 1009, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -64, 1006, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, -8, 856, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 104, -120, 784, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, -80, 195, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 195, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -48, 195, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, -48, 1137, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, -32, 1133, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -40, 1271, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, -48, 1281, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, -48, 1274, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -40, 1201, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -48, 1367, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -64, 2082, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -24, -64, 1977, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -64, 2706, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, -56, 2886, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, -56, 2742, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_8017EE38[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 11, 0, 0, { 4, 0 } },
    { 25, 11, 0, 0, { 1, 0 } },
    { 36, 3, 0, 0, { 5, 0 } },
    { 39, 7, 0, 0, { 0, 0 } },
    { 46, 2, 0, 0, { 6, 0 } },
    { 48, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_8017EE80[63] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -64, 2988, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -64, 3024, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -88, 2910, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -88, 2961, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -112, 2701, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -112, 2676, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 32, -112, 2742, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -120, 1750, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, -120, 1988, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, -96, 2022, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, -72, 2088, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -120, 1889, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, -120, 1839, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 48, -120, 1793, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 72, -120, 1750, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -96, 1783, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -72, 1800, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -48, 1875, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 32, 1065, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 24, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -80, 1384, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -56, 2339, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -56, 2220, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -56, 2226, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, -64, 2350, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -64, 2357, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -40, -32, 1472, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, 0, 1482, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, -48, 1739, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -40, -40, 1645, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 48, 808, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 112, 755, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, 56, 794, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, 64, 679, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, 96, 737, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 72, 683, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 96, 747, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 80, 646, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 88, 581, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 96, 625, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, 104, 588, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 96, 614, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 80, 667, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 56, 679, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, 40, 794, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 32, 792, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 16, 880, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 24, 842, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 32, 792, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 40, 751, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 48, 709, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 56, 675, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 80, 717, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -24, 40, 732, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 751, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 72, 669, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 72, 697, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 16, 48, 692, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, 80, 614, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 80, 640, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 56, 675, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 64, 660, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 80, 609, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_8017F36C[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 3, 0 } },
    { 7, 11, 0, 0, { 4, 0 } },
    { 18, 2, 0, 0, { 1, 0 } },
    { 20, 1, 0, 0, { 5, 0 } },
    { 21, 5, 0, 0, { 0, 0 } },
    { 26, 4, 0, 0, { 6, 0 } },
    { 30, 33, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_tunnel_8017F3B4[74] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -120, 1212, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 8, 1336, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, -40, 1274, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -80, 1237, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -136, -120, 1197, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -104, -112, 1232, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -56, -112, 1289, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -8, -112, 1346, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -112, 1408, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, -72, 1457, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, -40, 1510, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, -8, 1566, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, -24, 1318, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 32, 1179, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 863, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 934, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 934, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 16, 1325, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 40, 1210, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 40, 1130, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 16, 1338, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -16, 1314, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -16, 1319, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 1241, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, -16, 1219, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 16, 1256, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 846, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 24, 859, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 897, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 56, 929, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 72, 964, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 80, 900, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 56, 903, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 860, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 40, 852, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 32, 906, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 1028, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 24, 938, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 24, 1013, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 24, 921, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 24, 980, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 32, 877, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 40, 778, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 40, 789, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 32, 866, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 32, 866, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 80, 886, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 80, 899, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 80, 899, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 96, 853, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 96, 865, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 104, 843, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 64, 992, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 64, 953, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 48, 856, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 72, 877, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 32, 885, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 64, 872, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 32, 975, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 56, 963, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 56, 783, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 88, 810, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 104, 837, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 96, 837, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -64, 56, 990, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 80, 899, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, 80, 899, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 32, 867, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 40, 769, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 40, 843, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 40, 801, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 40, 819, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 48, 1018, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 48, 946, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_tunnel_8017F97C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 21, 0, 0, { 2, 0 } },
    { 34, 40, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mine_tunnel_8017F9A4[5] = {
    { { .empty = D_mine_tunnel_8017E944 }, D_mine_tunnel_8017E944, NULL },
    { { .elements = D_mine_tunnel_8017E954 }, D_mine_tunnel_8017EA1C, NULL },
    { { .elements = D_mine_tunnel_8017EA3C }, D_mine_tunnel_8017EE38, NULL },
    { { .elements = D_mine_tunnel_8017EE80 }, D_mine_tunnel_8017F36C, NULL },
    { { .elements = D_mine_tunnel_8017F3B4 }, D_mine_tunnel_8017F97C, NULL },
};

WorldCoordPointLight D_mine_tunnel_8017F9E0[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2DDC, -2000, 3420 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 600, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8040, -2000, 400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2539 }, { 0, 0 } }, 1000, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4720, -2000, 3590 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3358, 2293 }, { 0, 0 } }, 1000, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1600, -2000, 1040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2621 }, { 0, 0 } }, 1000, 3400 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x398A, -2000, 60 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 600, 3000 },
};

WorldCoordRoomLights D_mine_tunnel_8017FBC0[1] = {
    { 0, NULL, ARRAY_SIZE(D_mine_tunnel_8017F9E0), D_mine_tunnel_8017F9E0, 0, NULL },
};

WorldCollisionTrigger D_mine_tunnel_8017FBD8[6] = {
    { NULL, NULL, NULL, { 0x3600, -1296, 1504, 0 }, { { 96, -2112, -2416, 0 }, { -96, -2112, 2416, 0 }, { 96, 2112, -2416, 0 }, { -96, 2112, 2416, 0 } }, { 4110, 0, 163, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3691, -1360, 1424, 0 }, { { -80, -2112, 2416, 0 }, { 80, -2112, -2416, 0 }, { -80, 2112, 2416, 0 }, { 80, 2112, -2416, 0 } }, { -4111, 0, -137, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9986, -1344, 2849, 0 }, { { -1008, -2112, 1648, 0 }, { 1008, -2112, -1648, 0 }, { -1008, 2112, 1648, 0 }, { 1008, 2112, -1648, 0 } }, { -3496, 0, -2138, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9921, -1313, 2768, 0 }, { { 960, -2112, -1600, 0 }, { -960, -2112, 1600, 0 }, { 960, 2112, -1600, 0 }, { -960, 2112, 1600, 0 } }, { 3519, 0, 2111, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4880, -1377, 2800, 0 }, { { -560, -2112, -1264, 0 }, { 560, -2112, 1264, 0 }, { -560, 2112, -1264, 0 }, { 560, 2112, 1264, 0 } }, { 3745, 0, -1660, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5073, -1344, 2768, 0 }, { { 576, -2112, 1280, 0 }, { -576, -2112, -1280, 0 }, { 576, 2112, 1280, 0 }, { -576, 2112, -1280, 0 } }, { -3748, 0, 1686, 0 }, { 0, 0, 4096, 0 }, 2534, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mine_tunnel_8017FDA0[4] = {
    { NULL, NULL, NULL, { 0x3E20, -48, 1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -48, 1648, 0 }, { { -416, 0, -592, 0 }, { 416, 0, -592, 0 }, { -416, 0, 592, 0 }, { 416, 0, 592, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 721, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3370, -64, 1312, 0 }, { { -336, 0, -1920, 0 }, { 496, 0, -1920, 0 }, { -496, 0, 1920, 0 }, { 336, 0, 1920, 0 } }, { 0, 4102, 0, 0 }, { 799, 0, -4017, 0 }, 1982, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4255, -64, 3336, 0 }, { { -2527, 0, 88, 0 }, { -127, 0, -936, 0 }, { -32, 0, 792, 0 }, { 2689, 0, 56, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 2684, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_mine_tunnel_8017FED0[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_8017FEE8[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_8017FF00[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { 37, 37, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_203700_80151DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_8017FF24[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_tunnel_8017FF48[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { 37, 37, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_203700_80151DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_mine_tunnel_8017FF6C[6] = {
    { 16, 0, 2, 6692, 0, 1952, 1024, 0, 0, 2, 0 },
    { 16, 1, 1, 6116, 0, 2784, 1024, 0, 0, 2, 0 },
    { 16, 0, 2, 0x2764, 0, 1216, 1024, 0, 0, 2, 0 },
    { 16, 1, 1, 9500, 0, 600, 1024, 0, 0, 2, 0 },
    { 16, 0, 2, 0x2884, 0, 512, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_8017FFCC[6] = {
    { 16, 0, 0, 4400, -850, 1150, 3600, 0, 0, 2, 0 },
    { 16, 0, 0, 5550, 0, 1200, 1300, 0, 0, 2, 0 },
    { 16, 0, 0, 5000, 0, 1900, 350, 0, 0, 2, 0 },
    { 16, 0, 0, 9300, 0, 2600, 2250, 0, 0, 2, 0 },
    { 16, 0, 0, 0x2F44, 0, 600, 3150, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_8018002C[11] = {
    { 25, 0, 0, 3000, 0, 650, 3350, 0, 0, 2, 2 },
    { 25, 0, 0, 3700, 0, 2750, 2850, 0, 0, 2, 2 },
    { 25, 0, 0, 7700, 0, 1050, 3000, 0, 0, 2, 2 },
    { 37, 0, 2, 9400, -2300, 2900, 0, 0, 2, 4, 5 },
    { 37, 0, 2, 9200, -2100, 2900, 0, 0, 2, 4, 5 },
    { 37, 0, 2, 8900, -2500, 2900, 0, 0, 2, 4, 5 },
    { 37, 0, 2, 8200, -2100, 2900, 0, 0, 2, 4, 5 },
    { 37, 0, 2, 7800, -1800, 2900, 0, 0, 2, 4, 5 },
    { 37, 0, 2, 7400, -2500, 2900, 0, 0, 2, 4, 5 },
    { 37, 0, 2, 7000, -2000, 2900, 0, 0, 2, 4, 5 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_801800DC[8] = {
    { 25, 0, 0, 4600, 0, 3000, 3072, 0, 0, 2, 2 },
    { 25, 0, 0, 7500, 0, 1000, 3072, 0, 0, 2, 2 },
    { 25, 0, 0, 5500, 0, 1200, 0, 0, 0, 2, 2 },
    { 15, 0, 2, 5000, -2000, 150, 2048, 0, 2, 4, 0 },
    { 15, 0, 2, 6500, -2000, 3050, 0, 0, 2, 4, 0 },
    { 15, 0, 2, 9000, -2000, 150, 2048, 0, 2, 4, 0 },
    { 15, 0, 2, 0x32C8, -1600, 200, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_tunnel_8018015C[7] = {
    { 25, 0, 0, 4600, 0, 3000, 3072, 0, 0, 2, 0 },
    { 25, 0, 0, 7500, 0, 1000, 3072, 0, 0, 2, 0 },
    { 25, 0, 0, 5500, 0, 1200, 0, 0, 0, 2, 0 },
    { 37, 0, 0, 2100, -1800, 2000, 1024, 0, 2, 4, 0 },
    { 37, 0, 0, 6100, -1800, 1400, 0, 0, 2, 4, 0 },
    { 37, 0, 0, 9600, -1800, 1800, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_mine_tunnel_801801CC[12] = {
    { NULL, NULL },
    { D_mine_tunnel_8017FF6C, D_mine_tunnel_8017FED0 },
    { D_mine_tunnel_8017FFCC, D_mine_tunnel_8017FEE8 },
    { D_mine_tunnel_8018002C, D_mine_tunnel_8017FF00 },
    { D_mine_tunnel_801800DC, D_mine_tunnel_8017FF24 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_tunnel_8018015C, D_mine_tunnel_8017FF48 },
};

WorldCoordRoomAmbientEntry D_mine_tunnel_8018022C[6] = {
    { .viewCount = ARRAY_SIZE(D_mine_tunnel_8018022C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 500, 450, 400, 462 } },
    { .color = { 580, 530, 500, 545 } },
    { .color = { 530, 530, 480, 523 } },
    { .color = { 420, 420, 350, 411 } },
};

WorldCollisionOccluder D_mine_tunnel_8018025C[3] = {
    { NULL, NULL, { 4416, 0, 416, 0 }, { { 0, -1024, 1024, 0 }, { 0, -1024, -1024, 0 }, { 0, 1024, 1024, 0 }, { 0, 1024, -1024, 0 } }, { -4096, 0, 0, 0 }, 1448, 1, 0 },
    { NULL, NULL, { 7455, 0, 3568, 0 }, { { -9, -1024, 1509, 0 }, { 10, -1024, -1509, 0 }, { -9, 1024, 1509, 0 }, { 10, 1024, -1509, 0 } }, { -4110, 0, -28, 0 }, 1819, 1, 0 },
    { NULL, NULL, { 0x2C80, 0, 640, 0 }, { { -392, -1024, 946, 0 }, { 391, -1024, -946, 0 }, { -392, 1024, 946, 0 }, { 391, 1024, -946, 0 } }, { -3784, 0, -1568, 0 }, 1448, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_mine_tunnel_80180310 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionSurfaceProperties D_mine_tunnel_8018031C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_tunnel_80180324[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_tunnel_80180310 },
};

WorldCollisionSurfaceProperties* D_mine_tunnel_8018032C[8] = {
    D_mine_tunnel_8018031C,
    D_mine_tunnel_80180324,
    D_mine_tunnel_8018031C,
    D_mine_tunnel_8018031C,
    D_mine_tunnel_8018031C,
    D_mine_tunnel_8018031C,
    D_mine_tunnel_8018031C,
    D_mine_tunnel_8018031C,
};

/// Room effect tick: sets `gRoomEffectState->roomEffectMode` to 2 and draws the
/// light anchors the current view index shows - anchor 2 in view 2, all five
/// in view 3, anchors 2 and 3 in view 4, anchors 1 and 4 in view 5, none
/// otherwise.
void func_mine_tunnel_8017D7D4(Task* unused)
{
    s32 idx;

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    idx                              = viewGetMappedIndex() & 0xFF;

    switch (idx) {
        case 2:
            glowDrawFlare(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            break;
        case 3:
            glowDrawFlare(&D_mine_tunnel_8017E12C[0], 1, 0x300);
            glowDrawFlare(&D_mine_tunnel_8017E12C[1], 1, 0x300);
            glowDrawFlare(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            glowDrawFlare(&D_mine_tunnel_8017E12C[3], 1, 0x300);
            glowDrawFlare(&D_mine_tunnel_8017E12C[4], 1, 0x300);
            break;
        case 4:
            glowDrawFlare(&D_mine_tunnel_8017E12C[2], 1, 0x300);
            glowDrawFlare(&D_mine_tunnel_8017E12C[3], 1, 0x300);
            break;
        case 5:
            glowDrawFlare(&D_mine_tunnel_8017E12C[1], 1, 0x300);
            glowDrawFlare(&D_mine_tunnel_8017E12C[4], 1, 0x300);
            break;
        case 6:
            break;
    }
}

#include "../../shared/glow_draw_flare.inc.c"
