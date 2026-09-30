#include "rooms/neo_ark_island.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_island_private.h"

#include "actors/task_tables.h"

#include "actors/waypoints.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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
/// hold what `func_neo_ark_island_8017E968` copies out of the incoming
/// location, and `func_neo_ark_island_8017E844` moves those same three bytes
/// into `Mc_SaveData[0].state.at4.loc.area` / `warp` / `room`.
extern RoomEventMsg D_neo_ark_island_80184008;

static void func_neo_ark_island_8017EA94(Task* arg0);
static void func_neo_ark_island_8017EB08(Task* task);

extern GpAreaTmdRec D_neo_ark_island_80183EDC[2];
extern GpAreaTmdRec D_neo_ark_island_80183EF4[2];
extern GpAreaTmdRec D_neo_ark_island_80183F0C[2];
extern GpAreaTmdRec D_neo_ark_island_80183F24[1];
extern GpAreaTmdRec D_neo_ark_island_80183F30[2];

extern TaskDesc D_80147E48;

// Height override read by the shared waypoint actor.
ActorWaypointHeight D_neo_ark_island_80181C24 = { .storage = 300 };

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

SVECTOR D_neo_ark_island_80181D30[13] = {
#include "assets/neo_ark_island_collision_05108_normals.inc"
};

SVECTOR D_neo_ark_island_80181D98[127] = {
#include "assets/neo_ark_island_collision_05108_verts.inc"
};

GpGridFace D_neo_ark_island_80182190[61] = {
#include "assets/neo_ark_island_collision_05108_faces.inc"
};

s16 D_neo_ark_island_8018246C[262] = {
#include "assets/neo_ark_island_collision_05108_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_island_8018246C[i])
s16* D_neo_ark_island_80182678[20] = {
#include "assets/neo_ark_island_collision_05108_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_island_801826C8[1] = {
    { NULL, D_neo_ark_island_80181D30, D_neo_ark_island_80181D98, D_neo_ark_island_80182190, D_neo_ark_island_80182678, 5100, 7000, 4, 5, 4000, 61 },
};

GpViewRec D_neo_ark_island_801826EC[5] = {
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

GpSprtElem D_neo_ark_island_801827B0[33] = {
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

GpSprtElem D_neo_ark_island_80182A64[68] = {
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

GpSprtElem D_neo_ark_island_80182FE4[140] = {
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

GpSprtRec D_neo_ark_island_80183B14[5] = {
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

GpRoomCoordSet D_neo_ark_island_80183CB0[1] = {
    { 4, D_neo_ark_island_80183B50, 0, NULL, 0, NULL },
};

GpObj4C D_neo_ark_island_80183CC8[4] = {
    { NULL, NULL, NULL, { 3104, -993, 3616, 0 }, { { -3760, -1904, 0, 0 }, { 3760, -1904, 0, 0 }, { -3760, 1904, 0, 0 }, { 3760, 1904, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3103, -961, 3328, 0 }, { { 3344, -1904, 0, 0 }, { -3344, -1904, 0, 0 }, { 3344, 1904, 0, 0 }, { -3344, 1904, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 800, -1089, -320, 0 }, { { 3344, -1904, 0, 0 }, { -3344, -1904, 0, 0 }, { 3344, 1904, 0, 0 }, { -3344, 1904, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 479, -993, -129, 0 }, { { -3760, -1904, 0, 0 }, { 3760, -1904, 0, 0 }, { -3760, 1904, 0, 0 }, { 3760, 1904, 0, 0 } }, { 0, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 4, 3, 129, 0 },
};

GpObj4C D_neo_ark_island_80183DF8[3] = {
    { NULL, NULL, NULL, { 2976, -48, 6768, 0 }, { { -1024, 0, -496, 0 }, { 1024, 0, -496, 0 }, { -1024, 0, 496, 0 }, { 1024, 0, 496, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1137, 0, 13, 19, 2, 0 },
    { NULL, NULL, NULL, { 6272, -48, -1488, 0 }, { { -768, 0, -608, 0 }, { 768, 0, -608, 0 }, { -768, 0, 608, 0 }, { 768, 0, 608, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 979, 0, 30, 33, 3, 0 },
    { NULL, NULL, NULL, { 3424, -64, -6304, 0 }, { { -768, 0, -416, 0 }, { 768, 0, -416, 0 }, { -768, 0, 416, 0 }, { 768, 0, 416, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 872, 2, 2, 0, 130, 0 },
};

GpAreaTmdRec D_neo_ark_island_80183EDC[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_island_80183EF4[2] = {
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_island_80183F0C[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_island_80183F24[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_island_80183F30[2] = {
    { 57, 57, 0, 0, { 0, 0 }, D_801491F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_island_80183F48[13] = {
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

s32 D_neo_ark_island_80183FB0[3] = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

s32 D_neo_ark_island_80183FBC[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_neo_ark_island_80183FC8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_island_80183FD0[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_neo_ark_island_80183FD8[1] = {
    { 0, 0, 1, 0, D_neo_ark_island_80183FB0 },
};

GpRoomParamRec D_neo_ark_island_80183FE0[1] = {
    { 0, 0, 1, 0, D_neo_ark_island_80183FBC },
};

GpRoomParamRec* D_neo_ark_island_80183FE8[8] = {
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

/// Water-refraction ripple over part of the screen. Only some views of areas
/// 27, 14, 15, 13, 30 and 29 have one; each picks a row range, a split row
/// with the x at which a row's strip is cut in two, a clip mode, a wave scale
/// and an ordering-table offset, and every other view returns at once. Each
/// row's camera-space vector goes through the transposed view rotation to
/// give its ordering-table depth, and one or two `POLY_FT4` strips per row
/// sample the other display buffer, displaced vertically by a `rsin` / `rcos`
/// wave that fades in over the first 8 rows of the range and of the split.
/// The phases live in `Task::killCountdown`: seeded from `rand()` on the first
/// call, advanced by 0x20 per call while `Gp_StateF0.field_4` is clear, and, for views
/// 6 and 7 of area 13, by 0x20 while it is set.
///
/// Matching note: `wave = w` is written in both arms of the scale test and the
/// pass-1 fade starts from `v = 0x79`. jump2 merges the two copies and deletes
/// the constant set, but both change register allocation and scheduling the
/// way the retail code needs.
void func_neo_ark_island_8017D650(Task* task)
{
    s32                   buf;
    s32                   sinArg;
    s32                   cosArg;
    s32                   kind;
    s32                   scale;
    s32                   zoff;
    s32                   split;
    s32                   splitX;
    s32                   otzOff;
    s32                   xLeft0;
    s32                   xRight0;
    s32                   xLeftS;
    s32                   start;
    s32                   end;
    s32                   area;
    POLY_FT4*             prim;
    OverlayRippleScratch* scratch;
    s32                   y;
    s32                   y0;
    s32                   xl;
    s32                   xr;
    s32                   passes;
    s32                   pass;
    s32                   wave;
    s32                   sinv;
    s32                   cosv;
    s32                   w;
    s32                   d;
    s32                   z;
    s32                   otz;
    s32                   v;
    s32                   dy;
    s32                   xv;
    s32                   x;
    s32                   xe;
    s32                   xMin;
    s32                   xMax;
    s32                   fadeLen;
    s32                   one;
    DisplayState*         disp;

    kind    = 0;
    scale   = 0x1000;
    zoff    = 0x21C;
    otzOff  = 0;
    xLeft0  = -0xA0;
    xRight0 = 0xA0;
    xLeftS  = -0xA0;
    split   = 0;
    splitX  = 0;
    buf     = gDisplayState.otBuffer;
    area    = gGameSession->location.loc.area;
    if (area == 27) {
        otzOff = 10;
        switch (gGameSession->location.loc.view) {
            case 2:
                start  = 0x7F;
                end    = 0xF0;
                split  = 0x9F;
                splitX = 0x23;
                break;
            case 3:
                start  = 0x4A;
                end    = 0xF0;
                split  = 0x68;
                splitX = 0x55;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x74;
                splitX = -0xD3;
                break;
            case 5:
                start  = 0x66;
                end    = 0xF0;
                split  = 0x6B;
                splitX = 0xC2;
                break;
            case 6:
                start  = 0x93;
                end    = 0xF0;
                split  = 0xA1;
                splitX = 0xBC;
                break;
            default:
                return;
        }
    } else if (area == 14) {
        switch (gGameSession->location.loc.view) {
            case 2:
                start  = 0x77;
                end    = 0xF0;
                split  = 0xA1;
                splitX = -0x2C;
                break;
            case 3:
                start  = 0x4C;
                end    = 0xF0;
                split  = 0x68;
                splitX = -0x4E;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x3E8;
                kind   = 3;
                otzOff = 10;
                scale  = 0x800;
                break;
            default:
                return;
        }
    } else if (area == 15) {
        if (gGameSession->location.loc.view == 2) {
            split  = 0x3E8;
            start  = 0x84;
            end    = 0xF0;
            splitX = 0x4B;
            scale  = 0x800;
        } else {
            return;
        }
    } else if (area == 13) {
        otzOff = 10;
        switch (gGameSession->location.loc.view) {
            case 2:
            case 4:
                start = 0x52;
                end   = 0xF0;
                kind  = 1;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 3:
            case 5:
                start = 0x4C;
                end   = 0xF0;
                kind  = 2;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 6:
                start = 0x63;
                end   = 0xF0;
                split = 0;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            case 7:
                start = 0x35;
                end   = 0xF0;
                kind  = 4;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            default:
                return;
        }
    } else if (area == 30) {
        scale  = 0x800;
        otzOff = -10;
        zoff   = 0x131A;
        switch (gGameSession->location.loc.view) {
            case 2:
                xLeft0 = 0x3B;
                start  = 0xA9;
                end    = 0xE0;
                split  = -0xC7;
                splitX = -0x43;
                break;
            case 3:
                otzOff  = 10;
                xLeft0  = -0x4F;
                xRight0 = 0x4F;
                xLeftS  = -0x3A;
                start   = 1;
                end     = 0x5E;
                split   = -0x49;
                splitX  = 0x78;
                break;
            case 4:
                xRight0 = -0x3B;
                start   = 0xA9;
                end     = 0xE0;
                split   = -0xC7;
                splitX  = 0x43;
                break;
            case 5:
                start  = 0xA1;
                end    = 0xF0;
                xLeftS = -0x8A;
                splitX = 0x114;
                split  = 0xAC;
                otzOff = 0;
                break;
            default:
                return;
        }
    } else if (area == 29) {
        zoff = 0x8C;
        switch (gGameSession->location.loc.view) {
            case 6:
                start = 0xA5;
                end   = 0xF0;
                split = 0;
                break;
            case 7:
                start = 0xA4;
                end   = 0xF0;
                split = 0;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    xMin    = -0xA0;
    xMax    = 0xA0;
    fadeLen = 8;
    one     = 1;
    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }
    prim = (POLY_FT4*)Fs_ActorLoadBase2;
    disp = &gDisplayState;
    if (disp->otBuffer != 0) {
        prim += 488;
    }
    prim--;
    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown += 0x20;
    }
    sinArg = task->killCountdown * 2;
    cosArg = task->killCountdown;
    SCRATCH_PUSH(OverlayRippleScratch);
    scratch = SCRATCH_STACK_CURSOR(OverlayRippleScratch);
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->mtx);
    scratch->origin.vx = gGfxViewCoord.workm.t[0];
    scratch->origin.vy = gGfxViewCoord.workm.t[1];
    scratch->origin.vz = gGfxViewCoord.workm.t[2];
    gfxRotateSv(&scratch->mtx, &scratch->origin);
    scratch->depth  = scratch->origin.vy + zoff;
    scratch->depth *= disp->screenDistance;
    scratch->row.vx = 0;
    scratch->row.vz = disp->screenDistance;
    gte_SetRotMatrix(&scratch->mtx);

    for (y = start; y < end; y++) {
        y0              = y - 0x78;
        scratch->row.vy = y0;
        gte_ldv0(&scratch->row);
        gte_rtv0();
        xl     = xLeft0;
        xr     = xRight0;
        passes = 1;
        if (split > 0) {
            if (y < split + 8) {
                xr = xMax;
                if (splitX > 0) {
                    xl = xLeftS;
                    xr = xl + splitX;
                } else {
                    xl = xr + splitX;
                }
                if (split < y) {
                    passes = 2;
                }
            }
        } else if (split < 0 && -split < y) {
            xr = xMax;
            if (splitX > 0) {
                xl = xLeftS;
                xr = xl + splitX;
            } else {
                xl = xr + splitX;
            }
        }
        sinv  = rsin(sinArg);
        cosv  = rcos(cosArg + 0x134);
        sinv += 0x2000;
        w     = cosv + sinv;
        w   >>= 9;
        if (scale != 0x1000) {
            w    = (w * scale) >> 12;
            wave = w;
        } else {
            wave = w;
        }
        w++;
        if (start != 1) {
            d = y - start;
            if (d < fadeLen) {
                w  = wave >> ((fadeLen - d) >> one);
                w += one;
            }
        }
        gte_stsv(&scratch->rowView);
        if (scratch->rowView.vy > 0) {
            otz   = scratch->depth / scratch->rowView.vy;
            otz >>= 2;
        } else {
            otz = 0x3FFF;
        }
        v    = (y0 + 0x78) + w;
        z    = otz;
        otz  = ((z << gDisplayState.otDepthShift) & 0x3FFF) >> 4;
        otz += otzOff;
        if (v >= 0xEF) {
            v = 0x1DC - v;
        }
        if (kind == 1) {
            if (y < 0x7D) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB3) {
                passes = 2;
                xl     = -0xA0;
                xr     = -0x59;
            } else {
                xl = -0xA0;
                xr = -0x59;
            }
        } else if (kind == 2) {
            if (y < 0x83) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB7) {
                passes = 2;
                xl     = 0x57;
                xr     = 0xA0;
            } else {
                xl = 0x57;
                xr = 0xA0;
            }
        } else if (kind == 3) {
            if (y < 0x43) {
                passes = 1;
                xl     = -0xA0;
                xr     = 0xA0;
            } else {
                passes = 2;
            }
        } else if (kind == 4) {
            passes = 1;
            xr     = 0xA0;
            xl     = -9;
            if (y >= 0x42) {
                xl = -0xA0;
                if (y < 0x4D) {
                    xl = -0x6A;
                }
            }
        }
        for (pass = 0; pass < passes; pass++) {
            dy = y - split;
            if (kind == 1) {
                if (pass != 0) {
                    xl = 0x3C;
                    xr = 0xA0;
                }
            } else if (kind == 2) {
                if (pass == 1) {
                    xl = -0xA0;
                    xr = -0x69;
                }
            } else if (kind == 3) {
                if (pass == 0) {
                    if (y < 0x43) {
                        xl = -0xA0;
                        xr = 0xA0;
                    } else {
                        xl = -0xA0;
                        xr = -0x57;
                    }
                } else {
                    if (y < 0xC1) {
                        xl = 0x5D;
                        xr = 0xA0;
                    } else {
                        xl = 0x2A;
                        xr = 0xA0;
                    }
                }
            } else if (pass == 1) {
                if (dy < fadeLen) {
                    w = wave >> ((fadeLen - dy) >> 1);
                    v = 0x79;
                    v = y0 + (v + w);
                    if (v >= 0xEF) {
                        v = 0x1DC - v;
                    }
                }
                if (splitX > 0) {
                    xv = splitX - 0x140;
                } else {
                    xv = splitX + 0x140;
                }
                xl = xMin;
                if (xv > 0) {
                    xr = xv + xl;
                } else {
                    xr = xMax;
                    xl = xv + xr;
                }
            }
            if (xr > 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                x           = xl;
                if (xl < 0) {
                    x = 0;
                }
                prim->x2 = x;
                prim->x0 = x;
                prim->u2 = x + 0x20;
                prim->u0 = x + 0x20;
                prim->x3 = xr;
                prim->x1 = xr;
                prim->u3 = xr + 0x20;
                prim->u1 = xr + 0x20;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xl <= 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                xe          = xr;
                if (xr > 0) {
                    xe = 0;
                }
                prim->u2 = xl - 0x60;
                prim->u0 = xl - 0x60;
                prim->u3 = (xl - 0x60) + (xe - xl);
                prim->u1 = (xl - 0x60) + (xe - xl);
                prim->x2 = xl;
                prim->x0 = xl;
                prim->x3 = xe;
                prim->x1 = xe;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
        }
        sinArg += 0x1F;
        if (z > 0x300) {
            cosArg += 0xC5 + (z - 0x300) / 4;
        } else {
            cosArg += 0xC5;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayRippleScratch);
}

/// Wavy screen-distortion band for some views of areas 12 and 30; every other
/// view returns at once. Each screen row between the view's start and end
/// rows gets one or two raw-textured `POLY_FT4` strips sampling the other
/// display buffer, displaced vertically by a `rsin` / `rcos` wave and faded
/// out over the band's last 16 rows, each row linked one depth nearer into
/// `gGpuCurrentOt`. One view of area 30 draws a second band. The phases live
/// in `Task::killCountdown`, seeded from `rand()` on the first call and
/// advanced every call while `Gp_StateF0.field_4` is clear.
///
/// Matching note: `spare` is never assigned, so `spare >> 16` is always zero;
/// it stands in for the stack slot the retail frame carries, which the
/// register allocator needs to see.
void func_neo_ark_island_8017E2A4(Task* task)
{
    s32              xLeft  = -0xA0;
    s32              xRight = 0xA0;
    s32              buf    = gDisplayState.otBuffer;
    s32              passes = 1;
    GameLocationKey* loc    = &gGameSession->location.loc;
    s32              area   = loc->area;
    s32              start;
    s32              end;
    POLY_FT4*        prim;
    u8*              base;
    s32              size;
    s32              sinArg;
    s32              cosArg;
    s32              otz;
    s32              pass;
    s32              y;
    s32              y0;
    s32              wave;
    s32              sinv;
    s32              cosv;
    s32              v;
    s32              x0;
    s32              x1;
    u16              spare;

    if (area == 12) {
        switch (gGameSession->location.loc.view) {
            case 2:
                start = 1;
                end   = 0x3F;
                break;
            case 3:
                start = 1;
                end   = 0x49;
                break;
            case 4:
                start = 1;
                end   = 0x49;
                break;
            case 5:
                start = 1;
                end   = 0x3F;
                break;
            case 8:
                start = 1;
                end   = 0x72;
                break;
            case 9:
                start = 1;
                end   = 0x45;
                break;
            default:
                return;
        }
    } else if (area == 30) {
        switch (gGameSession->location.loc.view) {
            case 2:
                start  = 1;
                end    = 0x40;
                xRight = -0x50;
                break;
            case 4:
                start  = 0x3B;
                end    = 0x55;
                xRight = -0x67;
                break;
            case 5:
                start  = 0x22;
                end    = 0x4A;
                xRight = -0x4E;
                passes = 2;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }

    if (area == 12 && loc->variant == 3) {
        size  = 0x30000 - Fs_ChunkOutputSizes[0];
        size &= ~7;
        base  = (u8*)Fs_ActorLoadBase0 - (size - 0x30000);
        if (size < sizeof(POLY_FT4) * 976) {
            return;
        }
        if (gDisplayState.otBuffer != 0) {
            base += size >> 1;
        }
        prim = (POLY_FT4*)base - 1;
    } else {
        prim = (POLY_FT4*)((u8*)Fs_ActorLoadBase2 + 0x9880);
        if (gDisplayState.otBuffer != 0) {
            prim += 488;
        }
        prim--;
    }

    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown++;
    }
    sinArg = task->killCountdown << 5;
    cosArg = task->killCountdown << 4;
    SCRATCH_PUSH_BYTES(0x40);
    otz = ((0x3FFF << gDisplayState.otDepthShift) & 0x3FFF) >> 4;

    for (pass = 0; pass < passes; pass++) {
        if (pass == 1) {
            start  = 0x2E;
            end    = 0x54;
            xLeft  = 1;
            xRight = 0x55;
        }
        for (y = start; y < end; y++) {
            y0     = y - 0x78;
            sinv   = rsin(sinArg);
            cosv   = rcos(cosArg + 0x134);
            sinv  += 0x2000;
            wave   = cosv + sinv;
            wave >>= 10;
            if (end - 0x10 < y) {
                wave = (wave * (end - y)) >> 4;
            }
            cosv = wave + 0x79;
            v    = y0 + cosv;
            otz--;
            if (v >= 0xEF) {
                v = 0x1DC - v;
            }
            if (v < 0) {
                v = -v;
            }
            if (xRight > 0) {
                x0 = xLeft < 0 ? 0 : xLeft;
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                prim->x2    = x0;
                prim->x0    = x0;
                prim->u2    = x0 + 0x20;
                prim->u0    = x0 + 0x20;
                prim->x3    = xRight;
                prim->x1    = xRight;
                prim->u3    = xRight + 0x20;
                prim->u1    = xRight + 0x20;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xLeft < 0) {
                x1 = xRight;
                if (x1 > 0) {
                    x1 = 0;
                }
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                prim->u2    = xLeft - 0x60;
                prim->u0    = xLeft - 0x60;
                prim->u3    = x1 - 0x60;
                prim->u1    = x1 - 0x60;
                prim->x2    = xLeft;
                prim->x0    = xLeft;
                prim->x3    = x1;
                prim->x1    = x1;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            sinArg += 0x1F + (spare >> 16);
            cosArg += 0xC5;
        }
    }
    SCRATCH_POP_BYTES(0x40);
}

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_island_8017EB10`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_island_8017D614 = {
    { func_neo_ark_island_8017EA94, func_neo_ark_island_8017EB08, taskKill }
};

/// Island arrival sequence, advanced one step per call: step 0 asks for the
/// caption, step 1 waits for the CAP system to go idle, step 2 clears the mode
/// flag and waits for the event key it answers with - anything but 0xA kills
/// the task and messages the player weapon - step 3 is the shared advance, and
/// step 4 raises the outgoing sound, commits the staged save location to
/// `Mc_SaveData` and spawns the task's successor.
void func_neo_ark_island_8017E844(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_SpawnIfCapIdle(1, 0);
            goto L_advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            goto L_advance;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            Gp_StateF0.field_4 = 0;
            goto L_advance;
        case 3:
        L_advance:
            arg0->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_neo_ark_island_80184008.warp;
            Mc_SaveData[0].state.at4.loc.warp = D_neo_ark_island_80184008.field_4;
            Mc_SaveData[0].state.at4.loc.room = ((u8*)&D_neo_ark_island_80184008.areaId)[1];
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_neo_ark_island_8017E960(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Island message handler. Message 0x1E, while the incoming location still
/// reports no pending flag, latches the save location the outgoing message
/// carries and starts the cutscene that leads to the island's arrival. Returns
/// 1 for every other message and for a location that is already latched.
s32 func_neo_ark_island_8017E968(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_neo_ark_80179B14(src, dst);
    if (src->areaId == 0x1E) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_neo_ark_island_80184008.warp              = (u8)dst->areaId;
            D_neo_ark_island_80184008.field_4           = dst->warp;
            ((u8*)&D_neo_ark_island_80184008.areaId)[1] = dst->room;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_neo_ark_island_80181B78, 0, 0, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_neo_ark_island_8017EA24(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_island_8017EA2C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Maps a cap (cutscene) script event key to the island cue it should play:
/// key 3 plays `0x550E0003` outright, key 0x65 plays `0x550E0004` only while
/// the running cap script reports no event key. Every other key, and key 0x65
/// with a script still parked on one, is ignored. Always returns 0.
s32 func_neo_ark_island_8017EA34(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    s32 id;

    switch (arg2) {
        case 3:
            id = 0x550E0003;
            goto play;
        case 0x65:
            if (Gp_GetCapEventKey() != 0) {
                break;
            }
            id = 0x550E0004;
        play:
            SndEvt_EnqueueType6(id, 0, 0);
            break;
    }
    return 0;
}

/// Room entry task tick in the family that announces the island's arrival:
/// installs the room's message table, hands the task to pointer slot 7, plays
/// the two island cues, then advances state and raises the `D_80115598` flag.
static void func_neo_ark_island_8017EA94(Task* arg0)
{
    arg0->msgTable = D_neo_ark_island_80181B48;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550E0005, 0, 0);
    SndEvt_EnqueueType6(0x550E0006, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

static void func_neo_ark_island_8017EB08(Task* task)
{
}

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_island_8017D614`, copied to the stack first.
void func_neo_ark_island_8017EB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_island_8017D614;
    sp.funcs[task->state](task);
}
