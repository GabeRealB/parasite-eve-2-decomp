#include "rooms/neo_ark_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "neo_ark_garden_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
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
#include "main/gameflag_ids.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/water_effects.h"
#include "../../shared/glow_draw.h"

extern SVECTOR D_neo_ark_garden_801813D8;

static void _neoArkGardenDrawRotatingSquare(const SVECTOR* centre);

extern WorldCollisionGrid    D_neo_ark_garden_801816C4[1];
extern WorldCollisionTrigger D_neo_ark_garden_8018270C[6];
extern WorldCollisionTrigger D_neo_ark_garden_801828D4[7];
extern WorldCoordRoomLights  D_neo_ark_garden_801826F4[1];

TaskDesc D_neo_ark_garden_80181398 = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_garden_801813A4 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_garden_801813B0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_garden_8017E848 },
    { 5105, func_neo_ark_garden_8017E840 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_garden_8017E9AC },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_garden_8017E8DC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_neo_ark_garden_801813D8 = { -6360, -1370, -0x4A24, 0 };

// Indexed views below share one contiguous table.
SVECTOR D_neo_ark_garden_801813E0[4] = {
    { 1000, -1800, -20900, 0 },
    { 1000, -2100, -22600, 0 },
    { 2010, -1980, -15510, 0 },
    { 2010, -1980, -19500, 0 },
};

#include "../../shared/room_visual_effects_disc_data.inc.c"

WorldCollisionRoomResources D_neo_ark_garden_8018140C[1] = {
    { D_neo_ark_garden_801816C4, D_neo_ark_garden_8018270C, D_neo_ark_garden_801828D4, NULL },
};

WorldCoordRoomLighting D_neo_ark_garden_8018141C[1] = {
    { D_neo_ark_garden_801826F4, NULL },
};

u8* D_neo_ark_garden_80181424[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_garden_80181428[1] = { 7 };

DirectionWarpEntry D_neo_ark_garden_8018142C[3] = {
    { { { .word = 2048 }, -3040, 0, -0x319C }, { 0, 0, 0, 0 }, { { .word = 2048 }, -3040, 0, -0x319C }, { 0, 0, 0, 0 }, 0x550F0006, 0x550F0005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 1500, 0, -0x445C }, { 0, 0, 0, 0 }, { { .word = 3072 }, 1500, 0, -0x445C }, { 0, 0, 0, 0 }, 0x550F0002, 0x550F0001, 0x550F0009, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_ALTAR },
    { { { .word = 0 }, -7543, 0, -0x4876 }, { 0, 0, 0, 0 }, { { .word = 0 }, -7543, 0, -0x4876 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, 437 },
};

static SVECTOR _gNeoArkGardenCollision04104Normals[5] = {
#include "assets/neo_ark_garden_collision_04104_normals.inc"
};

static SVECTOR _gNeoArkGardenCollision04104Verts[22] = {
#include "assets/neo_ark_garden_collision_04104_verts.inc"
};

static WorldCollisionGridFace _gNeoArkGardenCollision04104Faces[9] = {
#include "assets/neo_ark_garden_collision_04104_faces.inc"
};

static s16 _gNeoArkGardenCollision04104Cells[62] = {
#include "assets/neo_ark_garden_collision_04104_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkGardenCollision04104Cells[i])
static s16* _gNeoArkGardenCollision04104Table[12] = {
#include "assets/neo_ark_garden_collision_04104_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_garden_801816C4[1] = {
    { NULL, _gNeoArkGardenCollision04104Normals, _gNeoArkGardenCollision04104Verts, _gNeoArkGardenCollision04104Faces, _gNeoArkGardenCollision04104Table, 0x2710, 0x4E20, 4, 3, 4000, 9 },
};

ViewCamera D_neo_ark_garden_801816E8[7] = {
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

SpriteSource D_neo_ark_garden_801817F4[62] = {
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

SpriteSource D_neo_ark_garden_80181D14[68] = {
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

SpriteSource D_neo_ark_garden_80182284[31] = {
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

SpriteView D_neo_ark_garden_80182540[7] = {
    { { .empty = D_neo_ark_garden_801817E4 }, D_neo_ark_garden_801817E4, NULL },
    { { .elements = D_neo_ark_garden_801817F4 }, D_neo_ark_garden_80181CCC, NULL },
    { { .elements = D_neo_ark_garden_80181D14 }, D_neo_ark_garden_80182264, NULL },
    { { .elements = D_neo_ark_garden_80182284 }, D_neo_ark_garden_801824F0, NULL },
    { { .empty = D_neo_ark_garden_80182510 }, D_neo_ark_garden_80182510, NULL },
    { { .empty = D_neo_ark_garden_80182520 }, D_neo_ark_garden_80182520, NULL },
    { { .empty = D_neo_ark_garden_80182530 }, D_neo_ark_garden_80182530, NULL },
};

WorldCoordLight D_neo_ark_garden_80182594[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -500, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2052, 2052, 2052 }, { 0, 0 } },
};

WorldCoordRoomLights D_neo_ark_garden_801826F4[1] = {
    { ARRAY_SIZE(D_neo_ark_garden_80182594), D_neo_ark_garden_80182594, 0, NULL, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_garden_8018270C[6] = {
    { NULL, NULL, NULL, { -5313, -1089, -0x3FC1, 0 }, { { -171, -1904, -3345, 0 }, { 158, -1904, 3331, 0 }, { -171, 1904, -3345, 0 }, { 158, 1904, 3331, 0 } }, { 4093, 0, -202, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5185, -993, -0x3EE1, 0 }, { { 180, -1904, 3750, 0 }, { -190, -1904, -3760, 0 }, { 180, 1904, 3750, 0 }, { -190, 1904, -3760, 0 } }, { -4095, 0, 201, 0 }, { 0, 0, 4096, 0 }, 4190, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -353, -800, -0x4671, 0 }, { { -460, -1904, -2051, 0 }, { 461, -1904, 2052, 0 }, { -460, 1904, -2051, 0 }, { 461, 1904, 2052, 0 } }, { 3996, 0, -899, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -225, -928, -0x46C2, 0 }, { { 425, -1904, 1829, 0 }, { -424, -1904, -1828, 0 }, { 425, 1904, 1829, 0 }, { -424, 1904, -1828, 0 } }, { -4000, 0, 928, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1663, -1024, -0x3BE1, 0 }, { { -1640, -1904, -913, 0 }, { 1640, -1904, 914, 0 }, { -1640, 1904, -913, 0 }, { 1640, 1904, 914, 0 } }, { 1997, 0, -3588, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1696, -1024, -0x3C81, 0 }, { { 1640, -1904, 914, 0 }, { -1640, -1904, -913, 0 }, { 1640, 1904, 914, 0 }, { -1640, 1904, -913, 0 } }, { -1999, 0, 3586, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_garden_801828D4[7] = {
    { NULL, NULL, NULL, { -2928, -48, -0x3130, 0 }, { { -1392, 0, -560, 0 }, { 1392, 0, -560, 0 }, { -1392, 0, 560, 0 }, { 1392, 0, 560, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1498, WORLD_COLLISION_TRIGGER_ACTION_WARP, 27, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1360, -59, -0x44A1, 0 }, { { 432, 0, -688, 0 }, { 432, 0, 688, 0 }, { -432, 0, -688, 0 }, { -432, 0, 688, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 812, WORLD_COLLISION_TRIGGER_ACTION_WARP, 33, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -736, -64, -0x48A0, 0 }, { { -1520, 0, -832, 0 }, { 1520, 0, -832, 0 }, { -1520, 0, 832, 0 }, { 1520, 0, 832, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1731, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7456, -64, -0x48E0, 0 }, { { -1520, 0, -704, 0 }, { 1520, 0, -704, 0 }, { -1520, 0, 704, 0 }, { 1520, 0, 704, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 1673, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -8304, -64, -0x42C0, 0 }, { { -896, 0, -1280, 0 }, { 896, 0, -1280, 0 }, { -896, 0, 1280, 0 }, { 896, 0, 1280, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1562, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1376, -64, -0x3FC0, 0 }, { { 432, 0, -464, 0 }, { 432, 0, 592, 0 }, { -432, 0, -464, 0 }, { -432, 0, 336, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4896, -64, -0x4920, 0 }, { { 1008, 0, -592, 0 }, { 1008, 0, 592, 0 }, { -1008, 0, -592, 0 }, { -1008, 0, 592, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Garden ambience task tick. On its first tick it installs three effect ids
/// and moves `state` to 1. `spawnArg1` holds the view seen on the previous
/// tick; whenever `viewGetMappedIndex()` differs from it, the sound delay held in
/// `EffectWork::scale` restarts at 4, and once it has run down the current
/// view's pair of 0x550F0003 / 0x550F0004 loops is enqueued every tick. In
/// views 2, 4 and 5 the first such tick with `state` still 1 also plays them
/// once through `sndEvtRequestScriptStart` and moves `state` to 2. Views 2 and 4
/// additionally roll two 1-in-4 chances per tick, while no event is running,
/// to spawn effect 0x60070 at the first two points of
/// `D_neo_ark_garden_801813E0`; view 4 also draws rotating squares at the last
/// two points, and view 3 draws the marker at `D_neo_ark_garden_801813D8`.
void func_neo_ark_garden_8017EA9C(Task* task)
{
    EffectWork* work;
    u32         rnd;

    // The room task animates nothing with its own effect block and reuses one
    // member as storage: `scale` counts down the frames left before the
    // current view's ambience loops are enqueued again. Spawn leaves it zero.
    work = task->spawnArg2.pointer;
    if (task->state == 0) {
        task->state               = 1;
        gRoomEffectGlowDiscId     = EFFECT_NEO_ARK_GARDEN_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_NEO_ARK_GARDEN_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_NEO_ARK_GARDEN_ORANGE_BURST_2;
    }
    if (task->spawnArg1.value != (viewGetMappedIndex() & 0xFF)) {
        work->scale = 4;
    }
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            if (work->scale == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -8, 0x32);
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, 0, 0x32);
                }
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -8, 0x32);
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, 0, 0x32);
            } else {
                work->scale--;
            }
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, 0, ((gRandomLcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, 0, ((gRandomLcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 3:
            if (work->scale == 0) {
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -0xF, 0x4C);
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, -0xE, 0x4C);
            } else {
                work->scale--;
            }
            glowDrawPulsingStar(&D_neo_ark_garden_801813D8, 0x600, 0xC0);
            break;
        case 4:
            if (work->scale == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -0xC, 0);
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, 0xC, 0);
                }
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -0xC, 0);
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, 0xC, 0);
            } else {
                work->scale--;
            }
            _neoArkGardenDrawRotatingSquare(&D_neo_ark_garden_801813E0[2]);
            _neoArkGardenDrawRotatingSquare(&D_neo_ark_garden_801813E0[3]);
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, 0, ((gRandomLcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, 0, ((gRandomLcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 5:
            if (work->scale == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -0xE, 0x40);
                    sndEvtRequestScriptStart(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, -0xD, 0x40);
                }
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -0xE, 0x40);
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, -0xD, 0x40);
            } else {
                work->scale--;
            }
            break;
        case 6:
            if (work->scale == 0) {
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, 0xD, 0x4C);
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, 0xF, 0x4C);
            } else {
                work->scale--;
            }
            break;
        case 7:
            if (work->scale == 0) {
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_1, -0xC, 0);
                sndEvtRequestScriptMix(SOUND_NEO_ARK_GARDEN_AMBIENCE_2, -0xC, 0);
            } else {
                work->scale--;
            }
            break;
    }
    task->spawnArg1.value = viewGetMappedIndex() & 0xFF;
}

#include "../../shared/glow_draw_pulsing_star.inc.c"

/// Projects four world corners through the current view, retaining the last FLAG word.
static inline void _neoArkGardenProjectSquareCorners(EffectQuadScratch* quadScratch)
{
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Draws a 500-world-unit textured square rotating about the world X axis.
///
/// `centre` supplies three signed 16-bit world coordinates, borrowed for this
/// call. The square lies in the YZ plane and turns once per 32 animation frames.
/// Rotated, translated corners narrow to signed 16-bit coordinates before
/// projection. Only the final three-corner projection's negative FLAG rejects
/// drawing; the last corner's SZ3 / 4 selects the ordering-table depth.
/// The current view, texture and frame primitive arena must be ready. One
/// opaque, unmodulated 64-by-64-texel quad is queued; scratch is released and
/// no pointer to `centre` is retained.
static void _neoArkGardenDrawRotatingSquare(const SVECTOR* centre)
{
    enum {
        NEO_ARK_GARDEN_ROTATING_SQUARE_HALF_EXTENT       = 250,
        NEO_ARK_GARDEN_ROTATING_SQUARE_FRAME_ANGLE_SHIFT = 7, // 128 of 4096 angle units per frame.
        NEO_ARK_GARDEN_ROTATING_SQUARE_RAW_QUAD          = 0x2D,
        NEO_ARK_GARDEN_ROTATING_SQUARE_TEXTURE_PAGE      = getTPage(1, GPU_BLEND_ADD, 768, 0),
        NEO_ARK_GARDEN_ROTATING_SQUARE_PALETTE           = getClut(0, 271),
        NEO_ARK_GARDEN_ROTATING_SQUARE_TEXTURE_SIZE      = 64
    };

    MATRIX             rotation;
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;

    gfxRotMatrixX(&rotation, gDisplayState.animFrame << NEO_ARK_GARDEN_ROTATING_SQUARE_FRAME_ANGLE_SHIFT, GRAPHICS_ROTATION_REPLACE);
    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);

    // Rotate the square in its world YZ plane, then place it at the centre.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = 0;
        quadScratch->vertices[cornerIndex].vy = D_80111E38[cornerIndex].axis0Sign * NEO_ARK_GARDEN_ROTATING_SQUARE_HALF_EXTENT;
        quadScratch->vertices[cornerIndex].vz = D_80111E38[cornerIndex].axis1Sign * NEO_ARK_GARDEN_ROTATING_SQUARE_HALF_EXTENT;
        gte_SetRotMatrix(&rotation);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += centre->vx;
        quadScratch->vertices[cornerIndex].vy += centre->vy;
        quadScratch->vertices[cornerIndex].vz += centre->vz;
    }

    _neoArkGardenProjectSquareCorners(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
        setcode(quad, NEO_ARK_GARDEN_ROTATING_SQUARE_RAW_QUAD);
        quad->tpage = NEO_ARK_GARDEN_ROTATING_SQUARE_TEXTURE_PAGE;
        quad->clut  = NEO_ARK_GARDEN_ROTATING_SQUARE_PALETTE;
        setUVWH(quad, 0, 0, NEO_ARK_GARDEN_ROTATING_SQUARE_TEXTURE_SIZE - 1, NEO_ARK_GARDEN_ROTATING_SQUARE_TEXTURE_SIZE - 1);
        setXY4(quad,
               quadScratch->screenCorners[0].vx, quadScratch->screenCorners[0].vy,
               quadScratch->screenCorners[1].vx, quadScratch->screenCorners[1].vy,
               quadScratch->screenCorners[2].vx, quadScratch->screenCorners[2].vy,
               quadScratch->screenCorners[3].vx, quadScratch->screenCorners[3].vy);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_neo_ark_garden_8017F790(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void neoArkGardenRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void neoArkGardenRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
