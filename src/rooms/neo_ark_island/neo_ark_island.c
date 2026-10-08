#include "rooms/neo_ark_island.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "common.h"

#include "neo_ark_island_private.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// Staging save location the island commits: area / warp / room
/// hold what `neoArkIslandResolveRoomTransition` copies out of the resolved
/// destination, and `neoArkIslandGalleryDepartureTask` moves those same three bytes
/// into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area` / `warp` / `room`.
extern RoomEventMsg D_neo_ark_island_80184008;

static void _neoArkIslandInitializeRoom(Task* task);
static void _neoArkIslandRoomIdleState(Task* unusedTask);

extern AreaResource D_neo_ark_island_80183EDC[2];
extern AreaResource D_neo_ark_island_80183EF4[2];
extern AreaResource D_neo_ark_island_80183F0C[2];
extern AreaResource D_neo_ark_island_80183F24[1];
extern AreaResource D_neo_ark_island_80183F30[2];

extern TaskDesc D_actor_100400_80147E48;

// Height override read by the shared waypoint actor.
s16 D_neo_ark_island_80181C24 = 300;

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_island_80181C28[8] = {
    { 300, 1500, 5900, 0 },
    { 5200, 1500, 2500, 0 },
    { 300, 1500, 700, 0 },
    { 5200, 1500, 2500, 0 },
    { 0, 1500, 2200, 0 },
    { 6000, 1500, 2200, 0 },
    { 6000, 1500, 5000, 0 },
    { 300, 1500, 2500, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_island_80181C68[8] = {
    { 5200, 3000, 2000, 0 },
    { 300, 3000, 800, 0 },
    { 300, 3000, 5000, 0 },
    { 5200, 3000, 5500, 0 },
    { 300, 3000, 2100, 0 },
    { 7000, 3000, 3500, 0 },
    { 6800, 3000, 7000, 0 },
    { 8500, 3000, 0x2AF8, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_island_80181CA8[8] = {
    { -1700, 4000, 6200, 0 },
    { 0, 4000, 4600, 0 },
    { 6600, 4000, 4500, 0 },
    { 7500, 4000, 7300, 0 },
    { 0, 4000, 4000, 0 },
    { 5000, 4000, 5200, 0 },
    { -1250, 4000, 3800, 0 },
    { -4400, 4000, 5700, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_neo_ark_island_80181CE8[4] = {
    D_neo_ark_island_80181C28,
    D_neo_ark_island_80181C68,
    D_neo_ark_island_80181CA8,
    D_neo_ark_island_80181C28,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_neo_ark_island_80181CF8[7] = {
    { 0, 0, 0, 0 },
    { 300, 0, 5700, 0 },
    { 300, 0, 2300, 0 },
    { 300, 0, 300, 0 },
    { 5300, 0, 1500, 0 },
    { 5300, 0, 4900, 0 },
    { 0, 0, 0, -1 },
};

static SVECTOR _gNeoArkIslandCollision05108Normals[13] = {
#include "assets/neo_ark_island_collision_05108_normals.inc"
};

static SVECTOR _gNeoArkIslandCollision05108Verts[127] = {
#include "assets/neo_ark_island_collision_05108_verts.inc"
};

static WorldCollisionGridFace _gNeoArkIslandCollision05108Faces[61] = {
#include "assets/neo_ark_island_collision_05108_faces.inc"
};

static s16 _gNeoArkIslandCollision05108Cells[262] = {
#include "assets/neo_ark_island_collision_05108_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkIslandCollision05108Cells[i])
static s16* _gNeoArkIslandCollision05108Table[20] = {
#include "assets/neo_ark_island_collision_05108_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_island_801826C8[1] = {
    { NULL, _gNeoArkIslandCollision05108Normals, _gNeoArkIslandCollision05108Verts, _gNeoArkIslandCollision05108Faces, _gNeoArkIslandCollision05108Table, 5100, 7000, 4, 5, 4000, 61 },
};

ViewCamera D_neo_ark_island_801826EC[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4555, 0x7530, 800 } }, 447 },
    { { { { 4047, 0, 627 }, { 37, 4088, -243 }, { -626, 246, 4040 } }, { -3460, 985, -1480 } }, 257 },
    { { { { 4045, 0, 640 }, { 178, 3932, -1130 }, { -614, 1144, 3884 } }, { -3600, 1865, 2360 } }, 257 },
    { { { { 4091, 0, 182 }, { 148, 2392, -3321 }, { -106, 3324, 2390 } }, { -4500, 5495, 7970 } }, 230 },
    { { { { 4007, 0, -846 }, { -825, 918, -3905 }, { 189, 3991, 898 } }, { -5920, 1425, 1855 } }, 257 },
};

SpriteBatch D_neo_ark_island_801827A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_island_801827B0[33] = {
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, 8, 1814, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 16, 1314, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 1450, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -64, 24, 1425, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 24, 1425, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -80, 32, 1375, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 8, 32, 1375, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -96, 40, 1125, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 0, 40, 1125, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -104, 48, 975, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, 0, 48, 975, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 120, 8 } }, -128, 56, 900, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 120, 8 } }, -8, 56, 900, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 8 } }, -144, 64, 800, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 120, 8 } }, -8, 64, 800, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -160, 72, 725, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -56, 72, 725, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 40, 72, 725, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -160, 80, 675, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -64, 80, 675, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 40, 80, 675, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -160, 88, 637, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -64, 88, 637, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 40, 88, 637, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -160, 96, 625, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -64, 96, 625, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, 40, 96, 625, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -160, 104, 562, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -64, 104, 562, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 8 } }, 40, 104, 562, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -160, 112, 500, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -64, 112, 500, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, 40, 112, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_island_80182A44[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_island_80182A64[68] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 56, 775, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 56, 775, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 80, 64, 775, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 775, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 80, 72, 775, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 80, 80, 775, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 88, 775, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 96, 725, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 104, 700, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 675, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 610, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -32, 2375, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, -32, 2375, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -24, -24, 2250, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -32, -16, 2125, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -40, -8, 1875, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -48, 0, 1650, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, 0, 1650, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, 8, 1475, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 8, 8, 1475, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -64, 16, 1350, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 0, 16, 1350, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -72, 24, 1250, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 0, 24, 1250, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 32, 1150, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -8, 32, 1150, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -88, 40, 1100, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -8, 40, 1100, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -96, 48, 1000, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -8, 48, 1000, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -104, 56, 950, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -16, 56, 950, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -112, 64, 900, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -24, 64, 900, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -128, 80, 825, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -48, 80, 825, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, 80, 825, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -112, 72, 875, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -48, 72, 875, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, 72, 875, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -136, 88, 775, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -56, 88, 775, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 16, 88, 775, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -152, 96, 725, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -72, 96, 725, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 16, 96, 725, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 104, 700, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -72, 104, 700, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 16, 104, 700, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -160, 112, 675, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -72, 112, 675, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 16, 112, 675, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 0, 657, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -136, 0, 671, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -128, 0, 674, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -120, 0, 689, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -112, 16, 693, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 80, 724, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 72, 8, 613, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 80, 8, 595, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 88, 8, 592, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 96, 8, 581, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 567, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -16, 871, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 16, 946, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 48, 774, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, 80, 644, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 72, -48, 2500, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_island_80182FB4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 3, 0 } },
    { 11, 41, 0, 0, { 0, 0 } },
    { 52, 15, 0, 0, { 2, 0 } },
    { 67, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_island_80182FE4[140] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -120, 2233, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, -96, 2264, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, -88, 2213, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, -80, 2146, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, -112, 2295, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, -104, 2321, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -96, 2422, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 64, -88, 2342, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -80, 2233, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -72, 2217, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 128, -24, 1500, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, 8, 1500, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 104, 16, 1500, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 104, 24, 1500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 112, 32, 1500, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 112, 40, 1500, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 128, 48, 1500, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 64, 1000, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -104, 72, 1000, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -112, 80, 1000, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -104, 88, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -104, 96, 1000, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -112, 104, 1000, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -104, 112, 1000, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 88, 1263, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 96, 1293, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 16, 80, 1125, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, 88, 1125, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 96, 1125, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 16, 104, 1125, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 16, 112, 1125, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -16, 1500, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 112, 0, 1524, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -8, 1539, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, -8, 1875, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, 0, 1875, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -112, 2125, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -104, 2125, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -160, -96, 2187, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -120, -104, 2125, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -120, -112, 2062, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -160, -88, 2250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -160, -80, 2250, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -160, -72, 2250, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -160, -64, 2125, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -56, 2000, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -48, 2000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -40, 2000, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -160, -32, 2000, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -136, -24, 2000, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -55, -120, 2650, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -71, -112, 2525, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -71, -104, 2500, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -63, -96, 2400, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -63, -88, 2325, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -63, -80, 2250, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -79, -72, 2187, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -95, -64, 2100, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -23, -64, 2100, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 49, -64, 2100, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 17, -72, 2150, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, 17, -80, 2250, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, 17, -88, 2300, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, 17, -96, 2375, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 81, -104, 2321, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 89, -112, 2295, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 105, -120, 2227, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -111, -56, 2025, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -31, -56, 2025, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 57, -56, 2025, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -111, -48, 1975, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -31, -48, 1975, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 57, -48, 1975, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -119, -40, 1937, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -31, -40, 1937, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 57, -40, 1937, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -119, -32, 1875, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -31, -32, 1875, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 57, -32, 1875, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -119, -24, 1837, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -31, -24, 1837, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 57, -24, 1837, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -127, -16, 1750, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -23, -16, 1750, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 81, -16, 1750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -127, -8, 1725, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -31, -8, 1725, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 73, -8, 1725, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -135, 0, 1700, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -39, 0, 1700, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 65, 0, 1700, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -143, 8, 1650, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -47, 8, 1650, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, 57, 8, 1650, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -143, 16, 1600, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -47, 16, 1600, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 57, 16, 1600, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -151, 24, 1575, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 8 } }, -55, 24, 1575, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 57, 24, 1575, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -151, 32, 1550, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -55, 32, 1550, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 57, 32, 1550, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -151, 40, 1525, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -63, 40, 1525, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 49, 40, 1525, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -151, 48, 1450, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -63, 48, 1450, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, 41, 48, 1450, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -143, 56, 1425, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 41, 56, 1425, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -47, 56, 1425, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -143, 64, 1400, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -47, 64, 1400, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 41, 64, 1400, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -143, 72, 1375, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -55, 72, 1500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 33, 72, 1498, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -135, 80, 1500, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -47, 80, 1500, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 33, 80, 1468, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -135, 88, 1250, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -47, 88, 1375, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 33, 88, 1301, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -119, 96, 1312, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -23, 96, 1312, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -111, 104, 1375, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -23, 104, 1375, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -103, 112, 1375, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -23, 112, 1375, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -120, 2168, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -104, 2250, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -112, 2250, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -120, 2250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -96, 2250, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -96, 2250, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -96, 2250, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -96, 2250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -112, 2250, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -112, 2250, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_island_80183AD4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 3, 0 } },
    { 36, 14, 0, 0, { 0, 0 } },
    { 50, 80, 0, 0, { 2, 0 } },
    { 130, 10, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_island_80183B04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_island_80183B14[5] = {
    { { .empty = D_neo_ark_island_801827A0 }, D_neo_ark_island_801827A0, NULL },
    { { .elements = D_neo_ark_island_801827B0 }, D_neo_ark_island_80182A44, NULL },
    { { .elements = D_neo_ark_island_80182A64 }, D_neo_ark_island_80182FB4, NULL },
    { { .elements = D_neo_ark_island_80182FE4 }, D_neo_ark_island_80183AD4, NULL },
    { { .empty = D_neo_ark_island_80183B04 }, D_neo_ark_island_80183B04, NULL },
};

WorldCoordLight D_neo_ark_island_80183B50[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1028, 1028, 1028 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_island_80183CB0[1] = {
    { ARRAY_SIZE(D_neo_ark_island_80183B50), D_neo_ark_island_80183B50, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_island_80183CC8[4] = {
    { NULL, NULL, NULL, { 3104, -993, 3616, 0 }, { { -3760, -1904, 0, 0 }, { 3760, -1904, 0, 0 }, { -3760, 1904, 0, 0 }, { 3760, 1904, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3103, -961, 3328, 0 }, { { 3344, -1904, 0, 0 }, { -3344, -1904, 0, 0 }, { 3344, 1904, 0, 0 }, { -3344, 1904, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 800, -1089, -320, 0 }, { { 3344, -1904, 0, 0 }, { -3344, -1904, 0, 0 }, { 3344, 1904, 0, 0 }, { -3344, 1904, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 479, -993, -129, 0 }, { { -3760, -1904, 0, 0 }, { 3760, -1904, 0, 0 }, { -3760, 1904, 0, 0 }, { 3760, 1904, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_island_80183DF8[3] = {
    { NULL, NULL, NULL, { 2976, -48, 6768, 0 }, { { -1024, 0, -496, 0 }, { 1024, 0, -496, 0 }, { -1024, 0, 496, 0 }, { 1024, 0, 496, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6272, -48, -1488, 0 }, { { -768, 0, -608, 0 }, { 768, 0, -608, 0 }, { -768, 0, 608, 0 }, { 768, 0, 608, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 979, WORLD_COLLISION_TRIGGER_ACTION_WARP, 30, 33, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 3424, -64, -6304, 0 }, { { -768, 0, -416, 0 }, { 768, 0, -416, 0 }, { -768, 0, 416, 0 }, { 768, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_island_80183EDC[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_island_80183EF4[2] = {
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_island_80183F0C[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_island_80183F24[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_island_80183F30[2] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor05700GolemPawnRookTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_island_80183F48[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B550, D_neo_ark_island_80183EDC },
    { D_map_neo_ark_8017B590, D_neo_ark_island_80183EF4 },
    { D_map_neo_ark_8017B5C0, D_neo_ark_island_80183F0C },
    { D_map_neo_ark_8017B600, D_neo_ark_island_80183F24 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017B610, D_neo_ark_island_80183F30 },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_neo_ark_island_80183FB0 = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionFootstepSounds D_neo_ark_island_80183FBC = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionSurfaceProperties D_neo_ark_island_80183FC8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_island_80183FD0[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_island_80183FD8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_island_80183FB0 },
};

WorldCollisionSurfaceProperties D_neo_ark_island_80183FE0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_island_80183FBC },
};

WorldCollisionSurfaceProperties* D_neo_ark_island_80183FE8[8] = {
    D_neo_ark_island_80183FC8,
    D_neo_ark_island_80183FD0,
    D_neo_ark_island_80183FC8,
    D_neo_ark_island_80183FD8,
    D_neo_ark_island_80183FE0,
    D_neo_ark_island_80183FC8,
    D_neo_ark_island_80183FC8,
    D_neo_ark_island_80183FC8,
};

RoomEventMsg D_neo_ark_island_80184008;

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

/// State handlers of the room's entry task, indexed by its state through
/// `neoArkIslandRoomTask`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_island_8017D614 = {
    { _neoArkIslandInitializeRoom, _neoArkIslandRoomIdleState, taskKill }
};

void neoArkIslandGalleryDepartureTask(Task* task)
{
    enum {
        NEO_ARK_ISLAND_DEPARTURE_START          = 0,
        NEO_ARK_ISLAND_DEPARTURE_WAIT           = 1,
        NEO_ARK_ISLAND_DEPARTURE_REPLY          = 2,
        NEO_ARK_ISLAND_DEPARTURE_DELAY          = 3,
        NEO_ARK_ISLAND_DEPARTURE_COMMIT         = 4,
        NEO_ARK_ISLAND_DEPARTURE_CAP_COMMAND    = 1,
        NEO_ARK_ISLAND_DEPARTURE_ACCEPTED_KEY   = 10,
        NEO_ARK_ISLAND_DEPARTURE_SPRITE_VARIANT = 1,
    };

    switch (task->state) {
        case NEO_ARK_ISLAND_DEPARTURE_START:
            capSpawnEventIfIdle(NEO_ARK_ISLAND_DEPARTURE_CAP_COMMAND, CAP_EVENT_NO_FLAGS);
            task->state++;
            return;
        case NEO_ARK_ISLAND_DEPARTURE_WAIT:
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            return;
        case NEO_ARK_ISLAND_DEPARTURE_REPLY:
            if (capGetVariantKey() != NEO_ARK_ISLAND_DEPARTURE_ACCEPTED_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                return;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            task->state++;
            return;
        case NEO_ARK_ISLAND_DEPARTURE_DELAY:
            task->state++;
            return;
        case NEO_ARK_ISLAND_DEPARTURE_COMMIT:
            // Commit the staged destination only after the prompt and delay tick.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = NEO_ARK_ISLAND_DEPARTURE_SPRITE_VARIANT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_neo_ark_island_80184008.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_neo_ark_island_80184008.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_neo_ark_island_80184008.areaId)[1];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}

s32 neoArkIslandRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 neoArkIslandResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        NEO_ARK_ISLAND_TRANSITION_DEFERRED = 0,
        NEO_ARK_ISLAND_TRANSITION_ALLOWED  = 1,
    };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId == GAME_AREA_NEO_ARK_SUBMARINE_GALLERY) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            // Preserve the resolved destination while the departure prompt holds control.
            D_neo_ark_island_80184008.warp              = (u8)reply->areaId;
            D_neo_ark_island_80184008.field_4           = reply->warp;
            ((u8*)&D_neo_ark_island_80184008.areaId)[1] = reply->room;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(&D_neo_ark_island_80181B78, 0, 0, 0);
        }
        return NEO_ARK_ISLAND_TRANSITION_DEFERRED;
    }
    return NEO_ARK_ISLAND_TRANSITION_ALLOWED;
}

s32 neoArkIslandIgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedSecondArg)
{
    return 0;
}

s32 neoArkIslandIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

s32 neoArkIslandSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        NEO_ARK_ISLAND_SOUND_CUE_3      = 3,
        NEO_ARK_ISLAND_SOUND_CUE_101    = 101,
        NEO_ARK_ISLAND_CAP_VARIANT_NONE = 0,
        NEO_ARK_ISLAND_SCRIPT_CUE_3     = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 3),
        NEO_ARK_ISLAND_SCRIPT_CUE_101   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 4),
    };

    switch (cueKey) {
        case NEO_ARK_ISLAND_SOUND_CUE_3:
            sndEvtRequestScriptStart(NEO_ARK_ISLAND_SCRIPT_CUE_3, 0, 0);
            break;
        case NEO_ARK_ISLAND_SOUND_CUE_101:
            // A CAP sequence's selected variant suppresses the completion cue.
            if (capGetVariantKey() == NEO_ARK_ISLAND_CAP_VARIANT_NONE) {
                sndEvtRequestScriptStart(NEO_ARK_ISLAND_SCRIPT_CUE_101, 0, 0);
            }
            break;
    }
    return 0;
}

/// Installs the island's room-message receiver and starts its two ambience scripts.
///
/// Enables room sound cues after CAP completion and advances to the idle state.
/// The session's room slot borrows the live task; teardown does not clear it.
static void _neoArkIslandInitializeRoom(Task* task)
{
    enum { NEO_ARK_ISLAND_CAP_COMPLETION_SOUNDS_ENABLED = 1 };

    task->msgTable = D_neo_ark_island_80181B48;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_ISLAND_AMBIENCE_1, 0, 0);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_ISLAND_AMBIENCE_2, 0, 0);
    task->state = task->state + 1;
    D_80115598  = NEO_ARK_ISLAND_CAP_COMPLETION_SOUNDS_ENABLED;
}

/// Keeps the island room task available for messages after initialization.
///
/// State 1 performs no per-frame work and leaves `unusedTask` unchanged.
static void _neoArkIslandRoomIdleState(Task* unusedTask)
{
}

void neoArkIslandRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_neo_ark_island_8017D614;
    handlers.funcs[task->state](task);
}
