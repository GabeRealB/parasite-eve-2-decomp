#include "rooms/neo_ark_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_garden_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
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
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"

/// The block the garden's ambience task reaches through `Task::spawnArg2`.
/// Only `soundDelay` is read here; what precedes it belongs to whoever owns the
/// block, and the type's true size is not known.
typedef struct NeoArkGardenAmbience {
    u8  pad_0[0x24];
    s16 soundDelay; // Frames left before the view's loops are re-enqueued; set to 4 on every view change
} NeoArkGardenAmbience;

extern SVECTOR D_neo_ark_garden_801813D8;

/// Per-channel right shifts turning a glow's brightness into its colour,
/// indexed by the tint in `Task::spawnArg1`.
extern s16 D_neo_ark_garden_80181400[][3];

static void func_neo_ark_garden_8017EFB8(SVECTOR* arg0, s16 arg1, s32 arg2);
static void func_neo_ark_garden_8017F42C(SVECTOR* arg0);
static void func_neo_ark_garden_8017FF0C(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_neo_ark_garden_80180190(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_garden_801805B4(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_garden_80180AF4(GfxCoord* coord, s16 size);
static void func_neo_ark_garden_80181020(GfxCoord* arg0, s32 arg1);

extern GpGridParams   D_neo_ark_garden_801816C4[1];
extern GpObj4C        D_neo_ark_garden_8018270C[6];
extern GpObj4C        D_neo_ark_garden_801828D4[7];
extern GpRoomCoordSet D_neo_ark_garden_801826F4[1];

TaskDesc D_neo_ark_garden_80181398 = { 0, 192, func_neo_ark_garden_8017D64C, { .model = NULL } };

TaskDesc D_neo_ark_garden_801813A4 = { 0, 192, func_neo_ark_garden_8017E2A0, { .model = NULL } };

GpMsgEntry D_neo_ark_garden_801813B0[5] = {
    { 5102, func_neo_ark_garden_8017E848 },
    { 5105, func_neo_ark_garden_8017E840 },
    { 5103, func_neo_ark_garden_8017E9AC },
    { 5104, func_neo_ark_garden_8017E8DC },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_neo_ark_garden_801813D8 = { -6360, -1370, -0x4A24, 0 };

// Indexed views below share one contiguous table.
SVECTOR D_neo_ark_garden_801813E0[4] = {
    { 1000, -1800, -20900, 0 },
    { 1000, -2100, -22600, 0 },
    { 2010, -1980, -15510, 0 },
    { 2010, -1980, -19500, 0 },
};

s16 D_neo_ark_garden_80181400[2][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

GpRoomObjRec D_neo_ark_garden_8018140C[1] = {
    { D_neo_ark_garden_801816C4, D_neo_ark_garden_8018270C, D_neo_ark_garden_801828D4, NULL },
};

GpRoomCoordRec D_neo_ark_garden_8018141C[1] = {
    { D_neo_ark_garden_801826F4, NULL },
};

u8* D_neo_ark_garden_80181424[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_garden_80181428[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_neo_ark_garden_8018142C[3] = {
    { { .words = { 2048, -3040, 0, -0x319C } }, { 0, 0, 0, 0 }, { .words = { 2048, -3040, 0, -0x319C } }, { 0, 0, 0, 0 }, 0x550F0006, 0x550F0005, 0, 2, 0, 0 },
    { { .words = { 3072, 1500, 0, -0x445C } }, { 0, 0, 0, 0 }, { .words = { 3072, 1500, 0, -0x445C } }, { 0, 0, 0, 0 }, 0x550F0002, 0x550F0001, 0x550F0009, 4, 0, 439 },
    { { .words = { 0, -7543, 0, -0x4876 } }, { 0, 0, 0, 0 }, { .words = { 0, -7543, 0, -0x4876 } }, { 0, 0, 0, 0 }, 0, 0, 0, 3, 0, 437 },
};

SVECTOR D_neo_ark_garden_801814D4[5] = {
#include "assets/neo_ark_garden_collision_04104_normals.inc"
};

SVECTOR D_neo_ark_garden_801814FC[22] = {
#include "assets/neo_ark_garden_collision_04104_verts.inc"
};

GpGridFace D_neo_ark_garden_801815AC[9] = {
#include "assets/neo_ark_garden_collision_04104_faces.inc"
};

s16 D_neo_ark_garden_80181618[62] = {
#include "assets/neo_ark_garden_collision_04104_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_garden_80181618[i])
s16* D_neo_ark_garden_80181694[12] = {
#include "assets/neo_ark_garden_collision_04104_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_garden_801816C4[1] = {
    { NULL, D_neo_ark_garden_801814D4, D_neo_ark_garden_801814FC, D_neo_ark_garden_801815AC, D_neo_ark_garden_80181694, 0x2710, 0x4E20, 4, 3, 4000, 9 },
};

GpViewRec D_neo_ark_garden_801816E8[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x61A8, 0x4A38 } }, 329 },
    { { { { -3663, 0, -1832 }, { -716, 3770, 1431 }, { 1687, 1601, -3371 } }, { 4920, 2785, 9295 } }, 257 },
    { { { { -3518, 0, 2096 }, { 457, 3997, 768 }, { -2045, 894, -3433 } }, { 5020, 1660, 0x2DB9 } }, 243 },
    { { { { -2502, 0, -3242 }, { -329, 4074, 253 }, { 3226, 415, -2489 } }, { 2000, 1500, 0x3A98 } }, 230 },
    { { { { -2755, 0, 3030 }, { 0, 4096, 0 }, { -3030, 0, -2755 } }, { 4825, 1200, 0x3C14 } }, 541 },
    { { { { 2204, 0, -3451 }, { -1390, 3749, -887 }, { 3159, 1649, 2018 } }, { 8900, 2385, 0x474F } }, 329 },
    { { { { -4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, -4096 } }, { 0, 817, 0x45F1 } }, 329 },
};

SpriteBatch D_neo_ark_garden_801817E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_garden_801817F4[62] = {
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -112, 56, 1187, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -112, 48, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -112, 40, 1312, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -112, 32, 1375, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -96, 24, 1437, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 1500, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 8, 1750, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 250, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 250, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -112, 56, 250, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, 48, 250, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, 64, 250, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -136, -8, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -120, -8, 750, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -104, 0, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -88, 0, 750, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, 8, 750, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, 16, 750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -40, 16, 750, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, 24, 750, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, 24, 750, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 8, 32, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 24, 32, 750, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 40, 32, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, 48, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 72, 48, 750, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 40, 750, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 104, 40, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -128, -16, 1375, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -120, -24, 1375, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, -24, 1375, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -104, -24, 1375, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -96, -24, 1375, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -88, -16, 1375, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -16, 1400, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 1437, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -24, 1437, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -24, 1437, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, -40, 1625, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -40, 1625, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, -40, 1625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, -32, 1625, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, -32, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -56, -40, 1625, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 88, 48, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 104, -8, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 120, -8, 1000, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 136, 0, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, 8, 1000, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -32, 750, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, -32, 750, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -32, 1625, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -112, -32, 1625, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -32, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, -32, 1625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 0, 1625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 40, 750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -144, 40, 750, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -128, 40, 750, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 40, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -96, 40, 1625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_garden_80181CCC[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 3, 0 } },
    { 7, 6, 0, 0, { 4, 0 } },
    { 13, 16, 0, 0, { 1, 0 } },
    { 29, 10, 0, 0, { 5, 0 } },
    { 39, 6, 0, 0, { 0, 0 } },
    { 45, 5, 0, 0, { 6, 0 } },
    { 50, 12, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_garden_80181D14[68] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 24, 600, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 112, 600, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, 72, 600, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, 64, 600, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 32, 600, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, 32, 600, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -80, 24, 600, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 32, 600, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 48, 600, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 72, 600, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 72, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -40, 48, 625, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, 56, 625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, 64, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -8, 56, 625, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 0, 64, 625, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 48, 650, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 48, 650, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 8, 56, 650, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 48, 650, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 48, 650, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 24, 650, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 88, 24, 650, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, 24, 650, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 64, 650, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 72, 650, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 72, 650, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 72, 650, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 40, 675, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, 88, 575, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 575, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 24, 575, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -128, 24, 575, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -160, 8, 575, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 24, 575, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 575, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, 72, 575, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 24, 575, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 40, 575, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 72, 575, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 64, 575, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, 16, 575, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 16, 575, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 575, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -48, 16, 575, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 80, 675, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 64, 675, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 48, 675, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 8, 675, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 8, 675, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 8, 750, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 48, 675, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, 48, 750, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 16, 8, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, 0, 750, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 750, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 750, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 800, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 32, 800, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 48, 800, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, 0, 812, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -8, 825, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -8, 850, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 24, 850, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, -24, 875, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, -8, 850, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 112, -24, 875, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 120, -24, 875, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_garden_80182264[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 40, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_garden_80182284[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -40, 795, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, -40, 795, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -40, 795, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -40, 795, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, -40, 795, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -40, 795, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -32, 1432, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -32, 1477, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -32, 1524, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -32, 1575, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, -88, 793, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -88, 793, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -88, 793, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, -88, 793, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -88, 793, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -144, -88, 793, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, -88, 793, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -72, 1375, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -72, 1375, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -72, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -72, 1375, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -32, 1504, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -48, 1468, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -48, 1405, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -48, 1376, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -48, 1358, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -56, 1716, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, -56, 1656, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -56, 1626, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, -56, 1465, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -56, 1459, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_garden_801824F0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_garden_80182510[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_garden_80182520[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_garden_80182530[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_garden_80182540[7] = {
    { { .empty = D_neo_ark_garden_801817E4 }, D_neo_ark_garden_801817E4, NULL },
    { { .elements = D_neo_ark_garden_801817F4 }, D_neo_ark_garden_80181CCC, NULL },
    { { .elements = D_neo_ark_garden_80181D14 }, D_neo_ark_garden_80182264, NULL },
    { { .elements = D_neo_ark_garden_80182284 }, D_neo_ark_garden_801824F0, NULL },
    { { .empty = D_neo_ark_garden_80182510 }, D_neo_ark_garden_80182510, NULL },
    { { .empty = D_neo_ark_garden_80182520 }, D_neo_ark_garden_80182520, NULL },
    { { .empty = D_neo_ark_garden_80182530 }, D_neo_ark_garden_80182530, NULL },
};

GpLight D_neo_ark_garden_80182594[4] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2052, 2052, 2052, { 0, 0 } },
};

GpRoomCoordSet D_neo_ark_garden_801826F4[1] = {
    { 4, D_neo_ark_garden_80182594, 0, NULL, 0, NULL },
};

GpObj4C D_neo_ark_garden_8018270C[6] = {
    { NULL, NULL, NULL, { -5313, -1089, -0x3FC1, 0 }, { { -171, -1904, -3345, 0 }, { 158, -1904, 3331, 0 }, { -171, 1904, -3345, 0 }, { 158, 1904, 3331, 0 } }, { 4093, 0, -202, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -5185, -993, -0x3EE1, 0 }, { { 180, -1904, 3750, 0 }, { -190, -1904, -3760, 0 }, { 180, 1904, 3750, 0 }, { -190, 1904, -3760, 0 } }, { -4095, 0, 201, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -353, -800, -0x4671, 0 }, { { -460, -1904, -2051, 0 }, { 461, -1904, 2052, 0 }, { -460, 1904, -2051, 0 }, { 461, 1904, 2052, 0 } }, { 3996, 0, -899, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { -225, -928, -0x46C2, 0 }, { { 425, -1904, 1829, 0 }, { -424, -1904, -1828, 0 }, { 425, 1904, 1829, 0 }, { -424, 1904, -1828, 0 } }, { -4000, 0, 928, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 1663, -1024, -0x3BE1, 0 }, { { -1640, -1904, -913, 0 }, { 1640, -1904, 914, 0 }, { -1640, 1904, -913, 0 }, { 1640, 1904, 914, 0 } }, { 1997, 0, -3588, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 1696, -1024, -0x3C81, 0 }, { { 1640, -1904, 914, 0 }, { -1640, -1904, -913, 0 }, { 1640, 1904, 914, 0 }, { -1640, 1904, -913, 0 } }, { -1999, 0, 3586, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 2, 4, 129, 0 },
};

GpObj4C D_neo_ark_garden_801828D4[7] = {
    { NULL, NULL, NULL, { -2928, -48, -0x3130, 0 }, { { -1392, 0, -560, 0 }, { 1392, 0, -560, 0 }, { -1392, 0, 560, 0 }, { 1392, 0, 560, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1498, 0, 27, 18, 2, 0 },
    { NULL, NULL, NULL, { 1360, -59, -0x44A1, 0 }, { { 432, 0, -688, 0 }, { 432, 0, 688, 0 }, { -432, 0, -688, 0 }, { -432, 0, 688, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 812, 0, 33, 33, 2, 0 },
    { NULL, NULL, NULL, { -736, -64, -0x48A0, 0 }, { { -1520, 0, -832, 0 }, { 1520, 0, -832, 0 }, { -1520, 0, 832, 0 }, { 1520, 0, 832, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1731, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { -7456, -64, -0x48E0, 0 }, { { -1520, 0, -704, 0 }, { 1520, 0, -704, 0 }, { -1520, 0, 704, 0 }, { 1520, 0, 704, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 1673, 2, 5, 255, 2, 0 },
    { NULL, NULL, NULL, { -8304, -64, -0x42C0, 0 }, { { -896, 0, -1280, 0 }, { 896, 0, -1280, 0 }, { -896, 0, 1280, 0 }, { 896, 0, 1280, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1562, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 1376, -64, -0x3FC0, 0 }, { { 432, 0, -464, 0 }, { 432, 0, 592, 0 }, { -432, 0, -464, 0 }, { -432, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 732, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { -4896, -64, -0x4920, 0 }, { { 1008, 0, -592, 0 }, { 1008, 0, 592, 0 }, { -1008, 0, -592, 0 }, { -1008, 0, 592, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1166, 2, 7, 255, 130, 0 },
};

/// Garden ambience task tick. On its first tick it installs three effect ids
/// and moves `state` to 1. `spawnArg1` holds the view seen on the previous
/// tick; whenever `Gp_GetViewIndex()` differs from it, `soundDelay` restarts at
/// 4, and once it has run down the current view's pair of 0x550F0003 /
/// 0x550F0004 loops is enqueued every tick. In views 2, 4 and 5 the first such
/// tick with `state` still 1 also plays them once through
/// `SndEvt_EnqueueType6` and moves `state` to 2. Views 2 and 4 additionally
/// roll two 1-in-4 chances per tick, while no event is running, to spawn
/// effect 0x60070 at the first two points of `D_neo_ark_garden_801813E0`;
/// view 4 also updates the last two points, and view 3 draws the marker at
/// `D_neo_ark_garden_801813D8`.
void func_neo_ark_garden_8017EA9C(Task* task)
{
    NeoArkGardenAmbience* work;
    u32                   rnd;

    work = task->spawnArg2.pointer;
    if (task->state == 0) {
        task->state = 1;
        D_80115734  = 0x60228;
        D_80115730  = 0x60233;
        D_80115754  = 0x6023E;
    }
    if (task->spawnArg1.value != (Gp_GetViewIndex() & 0xFF)) {
        work->soundDelay = 4;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -8, 0x32);
                    SndEvt_EnqueueType6(0x550F0004, 0, 0x32);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -8, 0x32);
                SndEvt_EnqueueTypeA(0x550F0004, 0, 0x32);
            } else {
                work->soundDelay--;
            }
            if (Gp_State1C->eventState == 0) {
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 3:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, -0xF, 0x4C);
                SndEvt_EnqueueTypeA(0x550F0004, -0xE, 0x4C);
            } else {
                work->soundDelay--;
            }
            func_neo_ark_garden_8017EFB8(&D_neo_ark_garden_801813D8, 0x600, 0xC0);
            break;
        case 4:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -0xC, 0);
                    SndEvt_EnqueueType6(0x550F0004, 0xC, 0);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -0xC, 0);
                SndEvt_EnqueueTypeA(0x550F0004, 0xC, 0);
            } else {
                work->soundDelay--;
            }
            func_neo_ark_garden_8017F42C(&D_neo_ark_garden_801813E0[2]);
            func_neo_ark_garden_8017F42C(&D_neo_ark_garden_801813E0[3]);
            if (Gp_State1C->eventState == 0) {
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 5:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -0xE, 0x40);
                    SndEvt_EnqueueType6(0x550F0004, -0xD, 0x40);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -0xE, 0x40);
                SndEvt_EnqueueTypeA(0x550F0004, -0xD, 0x40);
            } else {
                work->soundDelay--;
            }
            break;
        case 6:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, 0xD, 0x4C);
                SndEvt_EnqueueTypeA(0x550F0004, 0xF, 0x4C);
            } else {
                work->soundDelay--;
            }
            break;
        case 7:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, -0xC, 0);
                SndEvt_EnqueueTypeA(0x550F0004, -0xC, 0);
            } else {
                work->soundDelay--;
            }
            break;
    }
    task->spawnArg1.value = Gp_GetViewIndex() & 0xFF;
}

/// Draws a pulsing red star at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`; nothing is drawn when the projection flags an error.
/// Two gouraud `POLY_G4` halves of a diamond and two `LINE_G3` diagonals
/// surround the projected point, with radius `(s16)arg2 * 32` over its depth.
/// The lit vertices take a red of `rsin(animFrame * arg1) / 34 + 0x78`, so
/// `arg1` sets the pulse rate. The work block lives on the scratchpad stack.
static void func_neo_ark_garden_8017EFB8(SVECTOR* arg0, s16 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    block = SCRATCH_PUSH(RoomDraw13Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * arg1);
        radius        = ((s16)arg2 * 32) / block->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, pulse, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - block->radius) + (block->radius * twice);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, pulse, 0, 0);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Queues one textured quad (tpage 0xAC, clut 0x43C0, 64x64 texels). The
/// unit corners are scaled by 250 in the y/z plane, rotated by the matrix
/// `Gfx_RotMatrixX` builds from `gDisplayState.animFrame << 7`, moved to `arg0`, and
/// projected through `gGfxViewCoord.workm`. Nothing is queued when the GTE flag
/// word is negative.
static void func_neo_ark_garden_8017F42C(SVECTOR* arg0)
{
    MATRIX         m;
    GpQuadScratch* block;
    s32            i;
    POLY_FT4*      prim;

    Gfx_RotMatrixX(&m, gDisplayState.animFrame << 7, 1);
    block = SCRATCH_PUSH(GpQuadScratch);
    for (i = 0; i < 4; i++) {
        block->vec[i].vx = 0;
        block->vec[i].vy = (s16)D_80111E38[i].x * 250;
        block->vec[i].vz = (s16)D_80111E38[i].y * 250;
        gte_SetRotMatrix(&m);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        block->vec[i].vx += arg0->vx;
        block->vec[i].vy += arg0->vy;
        block->vec[i].vz += arg0->vz;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2D);
        prim->tpage = 0xAC;
        prim->clut  = 0x43C0;
        prim->u0    = 0;
        prim->v0    = 0;
        prim->u1    = 0x3F;
        prim->v1    = 0;
        prim->u2    = 0;
        prim->v2    = 0x3F;
        prim->u3    = 0x3F;
        prim->v3    = 0x3F;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpQuadScratch);
}

/// Glow effect task. It does nothing while the event state is 1 to 3 and
/// releases its work block once it reaches 4. State 0 parents the coordinate to `GpEffWork::parent`
/// with no rotation at `GpEffWork::pos`. States 1 and 2 grow the brightness
/// to 0xC0 and the size to 0x200 and draw a glow disc tinted through
/// `D_neo_ark_garden_80181400`; state 1 also spawns effect `D_80115730` on a
/// random bone of the player every fourth tick, and state 2 adds a larger
/// half-bright disc on odd ticks. State 3 drifts the coordinate away along a
/// rotated step, draws an expanding ring and fades the glow, releasing the
/// work block once it has faded. State 4 releases it at once.
void func_neo_ark_garden_8017F790(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.disp2d->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->parent                    = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->composeStamp              = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_neo_ark_garden_801805B4(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1.value][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_neo_ark_garden_80180190(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task that moves the task's coordinate toward the coordinate in
/// `Task::spawnArg1` while no event is running. State 0 takes their world
/// displacement into the coordinate's parent frame and scales it by
/// 0xCC / 0x1000; state 1 adds that step to the coordinate every tick and, on
/// odd ticks, draws a sprite there through `func_neo_ark_garden_8017FF0C` with
/// an advancing phase. The work block is released after 20 ticks, or once the
/// event state reaches 4.
void func_neo_ark_garden_8017FCE8(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.disp2d->coord;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0]  += work->pos.vx;
                coord->coord.t[1]  += work->pos.vy;
                coord->coord.t[2]  += work->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_neo_ark_garden_8017FF0C(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_neo_ark_garden_8017FF0C(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = arg0->workm.t[1];
    vz                                      = arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg        = (s16)arg2;
        t           = sarg * 24;
        block->step = (t - sarg) / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a gouraud ring at the coordinate's world position: projects it
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues sixteen
/// `POLY_G4` segments. One edge of the ring lies at on-screen radius
/// `(s16)arg1 * 64 / (otz + 1)` and is black; the other lies at
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)` and takes the RGB triple `rgb`.
static void func_neo_ark_garden_80180190(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// Draws a gouraud glow disc at the coordinate's world position: projects it
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues eight
/// `POLY_G4` wedges around the projected centre. `arg1` is a half-extent;
/// the on-screen radius is `arg1 * 64 / (otz + 1)`. Only the centre vertex
/// takes the RGB triple `rgb`, so each wedge fades to black.
static void func_neo_ark_garden_801805B4(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    s32            otz;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz         = block->otz + 1;
        block->otz  = otz;
        block->step = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Burst effect task. Each tick it grows the burst's size by 0x10 and draws
/// an orange glow disc of twice that size and the glow sprite of
/// `func_neo_ark_garden_80180AF4` at the coordinate. While the ring's
/// brightness (`period`) lasts it draws a widening, fading ring instead of
/// fading the burst; after that the burst's brightness (`scale`) fades by
/// 0x18 a tick and the work block is released once it runs out. It does
/// nothing while the event state is 1 to 3 and releases the block once it
/// reaches 4.
void func_neo_ark_garden_80180948(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.disp2d->coord;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_neo_ark_garden_801805B4(coord, (s16)(step * 2), rgb);
        func_neo_ark_garden_80180AF4(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_neo_ark_garden_80180190(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_neo_ark_garden_80180AF4(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    shifted                               = intensity << 0x10;
    light->head.g                         = shifted >> 0x11;
    light->head.b                         = shifted >> 0x12;
    light->head.u.at.local.t[0]           = coord->coord.t[0];
    light->head.u.at.local.t[1]           = coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                           = random;
    block                                 = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                         = coord->workm.t[0];
    block->vec.vy                         = coord->workm.t[1];
    block->vec.vz                         = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_neo_ark_garden_80181020(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_neo_ark_garden_80181020(GfxCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
