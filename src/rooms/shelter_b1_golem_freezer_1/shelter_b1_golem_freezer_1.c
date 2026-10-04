#include "rooms/shelter_b1_golem_freezer_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

#define GOLEM_RAND() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

extern void func_actor_160700_80131E70(void);
extern void func_actor_160700_80131E24(void);

/// The room's message table, installed on the room task.
extern TaskMessageEntry D_shelter_b1_golem_freezer_1_8017E6A8[];

extern s16                D_shelter_b1_golem_freezer_1_8017E6D0[3];
extern WorldCollisionGrid D_shelter_b1_golem_freezer_1_8017E714;
extern SVECTOR            D_shelter_b1_golem_freezer_1_8017E738[];
extern SVECTOR            D_shelter_b1_golem_freezer_1_8017E740[];

static void func_shelter_b1_golem_freezer_1_8017D744(s32 arg0);
static void func_shelter_b1_golem_freezer_1_8017D7CC(GfxCoord* arg0, s16* arg1);
static void func_shelter_b1_golem_freezer_1_8017E254(GfxCoord* coord, u16 arg1, s16 arg2, s16 arg3);

s32 func_shelter_b1_golem_freezer_1_8017D5D0(Task*, s32, s32, s32);
s32 func_shelter_b1_golem_freezer_1_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_golem_freezer_1_8017D61C(Task*, s32, s32, s32);
s32 func_shelter_b1_golem_freezer_1_8017D624(Task*, s32, RoomEventMsg*, s32);

TaskMessageEntry D_shelter_b1_golem_freezer_1_8017E6A8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_golem_freezer_1_8017D5D8 },
    { 5105, func_shelter_b1_golem_freezer_1_8017D5D0 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_golem_freezer_1_8017D624 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_golem_freezer_1_8017D61C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s16 D_shelter_b1_golem_freezer_1_8017E6D0[3] = {
    0,
    0,
    0,
};

static SVECTOR _gShelterB1GolemFreezer1Collision01154Normals[1] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01154_normals.inc"
};

static SVECTOR _gShelterB1GolemFreezer1Collision01154Verts[4] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01154_verts.inc"
};

static WorldCollisionGridFace _gShelterB1GolemFreezer1Collision01154Faces[1] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01154_faces.inc"
};

static s16 _gShelterB1GolemFreezer1Collision01154Cells[2] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01154_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1GolemFreezer1Collision01154Cells[i])
static s16* _gShelterB1GolemFreezer1Collision01154Table[1] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01154_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_golem_freezer_1_8017E714 = { NULL, _gShelterB1GolemFreezer1Collision01154Normals, _gShelterB1GolemFreezer1Collision01154Verts, _gShelterB1GolemFreezer1Collision01154Faces, _gShelterB1GolemFreezer1Collision01154Table, 679, -454, 1, 1, 4000, 1 };

SVECTOR D_shelter_b1_golem_freezer_1_8017E738[1] = {
    { 385, -3495, 555, 0 },
};

SVECTOR D_shelter_b1_golem_freezer_1_8017E740[10] = {
    { 7185, -2160, 10, 0 },
    { 1000, 0, 0, 0 },
    { 3000, 0, 0, 0 },
    { 5000, 0, 0, 0 },
    { 7000, 0, 0, 0 },
    { 0, 0, 1500, 0 },
    { 2000, 0, 1500, 0 },
    { 4000, 0, 1500, 0 },
    { 6000, 0, 1500, 0 },
    { 6500, 0, 2500, 0 },
};

u8* D_shelter_b1_golem_freezer_1_8017E790[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_golem_freezer_1_8017E794[2] = { 7, 0 };

DirectionWarpEntry D_shelter_b1_golem_freezer_1_8017E798[1] = {
    { { { .word = 3072 }, 6763, -80, 1000 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 6763, -80, 1000 }, { 0, 0, 0, 0 }, 0x54150002, 0x54150001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, 433 },
};

static SVECTOR _gShelterB1GolemFreezer1Collision01400Normals[9] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01400_normals.inc"
};

static SVECTOR _gShelterB1GolemFreezer1Collision01400Verts[28] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01400_verts.inc"
};

static WorldCollisionGridFace _gShelterB1GolemFreezer1Collision01400Faces[12] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01400_faces.inc"
};

static s16 _gShelterB1GolemFreezer1Collision01400Cells[24] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01400_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1GolemFreezer1Collision01400Cells[i])
static s16* _gShelterB1GolemFreezer1Collision01400Table[2] = {
#include "assets/shelter_b1_golem_freezer_1_collision_01400_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_golem_freezer_1_8017E9C0 = { NULL, _gShelterB1GolemFreezer1Collision01400Normals, _gShelterB1GolemFreezer1Collision01400Verts, _gShelterB1GolemFreezer1Collision01400Faces, _gShelterB1GolemFreezer1Collision01400Table, -470, -280, 2, 1, 4000, 12 };

ViewCamera D_shelter_b1_golem_freezer_1_8017E9E4[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3740, 0x61A8, -960 } }, 853 },
    { { { { 586, 0, -4053 }, { -114, 4094, -16 }, { 4052, 115, 586 } }, { -420, 1500, -330 } }, 257 },
    { { { { 531, 0, 4061 }, { 103, 4094, -13 }, { -4060, 104, 531 } }, { -6710, 1500, -330 } }, 257 },
    { { { { 3120, 0, 2652 }, { -153, 4089, 180 }, { -2648, -236, 3115 } }, { -6050, 1500, 3100 } }, 289 },
    { { { { 1015, 0, -3968 }, { -3004, 2675, -768 }, { 2592, 3101, 663 } }, { -4450, 3360, -460 } }, 257 },
    { { { { 3084, 0, 2694 }, { 720, 3946, -824 }, { -2596, 1095, 2972 } }, { -3520, 1560, 1190 } }, 312 },
    { { { { 2473, 0, -3264 }, { -442, 4058, -335 }, { 3234, 555, 2450 } }, { 880, 1490, 900 } }, 447 },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017EAE0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_golem_freezer_1_8017EAF0[30] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -120, 875, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, -80, 875, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -128, -40, 899, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, 8, 1000, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 48, 1075, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -120, 1075, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -88, 1075, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -56, 1075, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -24, 1125, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, 8, 1125, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 40, 1125, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, 64, 1125, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -120, 950, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -96, 958, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -72, 999, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -48, 1133, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -24, 1150, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -8, 1200, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -8, 1250, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 8, 1200, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 32, 1200, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 56, 1200, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 32, 1267, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -120, 866, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -96, 956, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -72, 1226, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -48, 1143, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -112, 1203, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -96, 1108, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -80, 1156, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017ED48[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017ED60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017ED70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017ED80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017ED90[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_golem_freezer_1_8017EDA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_golem_freezer_1_8017EDB0[7] = {
    { { .empty = D_shelter_b1_golem_freezer_1_8017EAE0 }, D_shelter_b1_golem_freezer_1_8017EAE0, NULL },
    { { .elements = D_shelter_b1_golem_freezer_1_8017EAF0 }, D_shelter_b1_golem_freezer_1_8017ED48, NULL },
    { { .empty = D_shelter_b1_golem_freezer_1_8017ED60 }, D_shelter_b1_golem_freezer_1_8017ED60, NULL },
    { { .empty = D_shelter_b1_golem_freezer_1_8017ED70 }, D_shelter_b1_golem_freezer_1_8017ED70, NULL },
    { { .empty = D_shelter_b1_golem_freezer_1_8017ED80 }, D_shelter_b1_golem_freezer_1_8017ED80, NULL },
    { { .empty = D_shelter_b1_golem_freezer_1_8017ED90 }, D_shelter_b1_golem_freezer_1_8017ED90, NULL },
    { { .empty = D_shelter_b1_golem_freezer_1_8017EDA0 }, D_shelter_b1_golem_freezer_1_8017EDA0, NULL },
};

WorldCoordPointLight D_shelter_b1_golem_freezer_1_8017EE04[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2916, -3502, -3635 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1474, 2048, 1865 }, { 0, 0 } }, 6500, 8500 },
};

WorldCoordRoomLights D_shelter_b1_golem_freezer_1_8017EE64 = { 0, NULL, ARRAY_SIZE(D_shelter_b1_golem_freezer_1_8017EE04), D_shelter_b1_golem_freezer_1_8017EE04, 0, NULL };

WorldCollisionTrigger D_shelter_b1_golem_freezer_1_8017EE7C[4] = {
    { NULL, NULL, NULL, { 3643, -1712, 668, 0 }, { { -50, -2576, -1587, 0 }, { 50, -2576, 1588, 0 }, { -50, 2576, -1587, 0 }, { 50, 2576, 1588, 0 } }, { 4098, 0, -130, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3786, -1680, 763, 0 }, { { 43, -2544, 1293, 0 }, { -43, -2544, -1293, 0 }, { 43, 2544, 1293, 0 }, { -43, 2544, -1293, 0 } }, { -4103, 0, 135, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5695, -1664, 544, 0 }, { { 413, -2576, -1534, 0 }, { -413, -2576, 1534, 0 }, { 413, 2576, -1534, 0 }, { -413, 2576, 1534, 0 } }, { 3959, 0, 1065, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5855, -1664, 640, 0 }, { { -413, -2576, 1534, 0 }, { 413, -2576, -1534, 0 }, { -413, 2576, 1534, 0 }, { 413, 2576, -1534, 0 } }, { -3961, 0, -1067, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_golem_freezer_1_8017EFAC[5] = {
    { NULL, NULL, NULL, { 6736, -144, 1152, 0 }, { { -400, 0, -960, 0 }, { 400, 0, -960, 0 }, { -400, 0, 960, 0 }, { 400, 0, 960, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 784, -141, 784, 0 }, { { -320, 0, -496, 0 }, { 320, 0, -496, 0 }, { -320, 0, 496, 0 }, { 320, 0, 496, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 590, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3296, -142, 1040, 0 }, { { -2240, 0, -256, 0 }, { 2240, 0, -256, 0 }, { -2240, 0, 256, 0 }, { 2240, 0, 256, 0 } }, { 0, 4110, 0, 0 }, { -201, 0, -4091, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 992, -143, 1008, 0 }, { { -144, 0, -592, 0 }, { 1680, 0, -592, 0 }, { -144, 0, 528, 0 }, { 1680, 0, 528, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1778, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4064, -64, 416, 0 }, { { -3040, 0, -256, 0 }, { 3040, 0, -256, 0 }, { -3040, 0, 256, 0 }, { 3040, 0, 256, 0 } }, { 0, 4095, 0, 0 }, { 201, 0, 4091, 0 }, 3050, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_golem_freezer_1_8017F128[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_golem_freezer_1_8017F134[2] = {
    { 143, 607, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_160700_801416A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_golem_freezer_1_8017F14C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_golem_freezer_1_8017F15C[2] = {
    { 143, 0, 0, 1140, 0, 1060, 1251, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_golem_freezer_1_8017F17C[23] = {
    { NULL, NULL },
    { D_shelter_b1_golem_freezer_1_8017F14C, D_shelter_b1_golem_freezer_1_8017F128 },
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
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_golem_freezer_1_8017F15C, D_shelter_b1_golem_freezer_1_8017F134 },
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_shelter_b1_golem_freezer_1_8017F234[8] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b1_golem_freezer_1_8017F234) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 486, 554, 538, 526 } },
    { .color = { 458, 559, 500, 513 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 457, 557, 503, 512 } },
    { .color = { 406, 881, 1574, 789 } },
    { .color = { 797, 1115, 1678, 1066 } },
};

WorldCollisionFootstepSounds D_shelter_b1_golem_freezer_1_8017F274 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b1_golem_freezer_1_8017F280[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_golem_freezer_1_8017F288[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_golem_freezer_1_8017F274 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_golem_freezer_1_8017F290[8] = {
    D_shelter_b1_golem_freezer_1_8017F280,
    D_shelter_b1_golem_freezer_1_8017F288,
    D_shelter_b1_golem_freezer_1_8017F280,
    D_shelter_b1_golem_freezer_1_8017F280,
    D_shelter_b1_golem_freezer_1_8017F280,
    D_shelter_b1_golem_freezer_1_8017F280,
    D_shelter_b1_golem_freezer_1_8017F280,
    D_shelter_b1_golem_freezer_1_8017F280,
};

static void func_shelter_b1_golem_freezer_1_8017D66C(Task* arg0);
static void func_shelter_b1_golem_freezer_1_8017D6DC(Task* task);

/// Message-table handler for message 0x13F1: does nothing and answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D5D0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both on to `func_map_shelter_80179A04`. Always answers 1.
s32 func_shelter_b1_golem_freezer_1_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

/// Message-table handler for message 0x13F0: does nothing and answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D61C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message-table handler for message 0x13EF: when the message's `field_2` is 1
/// and the session's place is 0x15, calls `func_actor_160700_80131E70`. Always answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D624(Task* arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    if (msg->warp == 1 && gGameSession->location.loc.variant == 0x15) {
        func_actor_160700_80131E70();
    }
    return 0;
}

/// The room task's first state: installs the room's message table, takes game
/// pointer slot 7, calls `func_actor_160700_80131E24` while the session's place is 0x15,
/// runs `func_shelter_b1_golem_freezer_1_8017D744` and moves on to the next
/// state.
static void func_shelter_b1_golem_freezer_1_8017D66C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_golem_freezer_1_8017E6A8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 0x15) {
        func_actor_160700_80131E24();
    }
    func_shelter_b1_golem_freezer_1_8017D744(0);
    arg0->state = arg0->state + 1;
}

/// The room task's idle state. It reserves a stack frame it never uses.
static void func_shelter_b1_golem_freezer_1_8017D6DC(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room task `func_shelter_b1_golem_freezer_1_8017D6EC`
/// runs: its setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b1_golem_freezer_1_8017D5C4 = {
    { func_shelter_b1_golem_freezer_1_8017D66C, func_shelter_b1_golem_freezer_1_8017D6DC, taskKill }
};

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b1_golem_freezer_1_8017D5C4`, copying the table onto the stack
/// and calling the entry for the task's current state.
void func_shelter_b1_golem_freezer_1_8017D6EC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_golem_freezer_1_8017D5C4;
    sp.funcs[task->state](task);
}

static void func_shelter_b1_golem_freezer_1_8017D744(s32 arg0)
{
    Task* slot   = Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    if (slot != NULL) {
        if (gGameSession->location.loc.variant == 0x15) {
            D_shelter_b1_golem_freezer_1_8017E6D0[1] = 0;
        } else {
            D_shelter_b1_golem_freezer_1_8017E6D0[1] = 0x2710;
        }
    } else {
        D_shelter_b1_golem_freezer_1_8017E6D0[1] = 0x2710;
    }
    func_shelter_b1_golem_freezer_1_8017D7CC(task->extra.tmd->coords, D_shelter_b1_golem_freezer_1_8017E6D0);
}

static void func_shelter_b1_golem_freezer_1_8017D7CC(GfxCoord* coord, s16* arg1)
{
    MATRIX              m;
    long                flag;
    s32                 i;
    SVECTOR*            d;
    SVECTOR*            s;
    WorldCollisionGrid* dst = &D_shelter_b1_golem_freezer_1_8017E9C0;
    WorldCollisionGrid* src = &D_shelter_b1_golem_freezer_1_8017E714;

    i = 0;
    do {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
        i++;
    } while (i <= 0);

    for (i = 0; i < 4; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    m = coord->coord;

    if (arg1 != NULL) {
        m.t[0] += arg1[0];
        m.t[1] += arg1[1];
        m.t[2] += arg1[2];
    }

    d = dst->normals;
    s = src->normals;
    i = 0;
    do {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
        i++;
    } while (i <= 0);

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->vertices;
    s = src->vertices;
    for (i = 0; i < 4; i++) {
        RotTransSV(s++, d++, &flag);
    }
}

void func_shelter_b1_golem_freezer_1_8017DA7C(Task* unused)
{
    SVECTOR pos;
    s32     i;
    s32     ang;
    s32     r;

    if (!(gDisplayState.animFrame & 3)) {
        for (i = 0; i < 9; i++) {
            ang    = GOLEM_RAND() & 0xFFF;
            r      = (GOLEM_RAND() & 0x3C0) + 0x40;
            pos.vx = D_shelter_b1_golem_freezer_1_8017E738[i + 2].vx + ((r * rcos(ang)) >> 12);
            pos.vy = -(GOLEM_RAND() & 0xFF);
            pos.vz = D_shelter_b1_golem_freezer_1_8017E738[i + 2].vz + ((r * rsin(ang)) >> 12);
            Gp_SpawnEff(EFFECT_GOLEM_FREEZER_FLOOR_MIST, NULL, (GOLEM_RAND() & 0x10FF) + 0x85400, &pos);
        }
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            glowDrawDisc(D_shelter_b1_golem_freezer_1_8017E738, 0x200, 0x421);
            break;
        case 4:
            glowDrawDisc(D_shelter_b1_golem_freezer_1_8017E738, 0x200, 0x210);
            break;
        case 2:
        case 5:
            glowDrawDisc(D_shelter_b1_golem_freezer_1_8017E740, 0x200, 0x421);
            break;
    }
}

#include "../../shared/glow_draw_disc.inc.c"

void func_shelter_b1_golem_freezer_1_8017DFFC(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         vz;
    s16         f2a;
    u32         rng2;
    u32         rng3;

    work->age++;
    if (task->state == 0) {
        work->scale     = task->spawnArg1.value & 0xFFF;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & 0xFFF;

        if (task->spawnArg1.value & 0xF000) {
            work->period = (task->spawnArg1.value >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1.value & 0xFF0000) {
            f2a = (task->spawnArg1.value >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step      = f2a;
        work->move.vy   = 0;
        rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng2;
        work->move.vx   = 0x80 - (((u32)rng2 >> 16) & 0xFF);
        rng3            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng3;
        vz              = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->move.vz   = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    func_shelter_b1_golem_freezer_1_8017E254(coord, work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            effectKillTask(work, task);
        }
    }
}

static void func_shelter_b1_golem_freezer_1_8017E254(GfxCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**              scratch;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    SVECTOR*            vec;
    s32                 u0;
    s32                 v0;
    s32                 u1;
    s32                 v1;
    s32                 ang;
    s32                 ang2;
    u16                 vz;

    scratch                   = SCRATCH_STACK_CURSOR_SLOT;
    head                      = *scratch;
    (head - 1)->worldPoint.vx = (u16)coord->workm.t[0];
    block                     = head - 1;
    block->worldPoint.vy      = (u16)coord->workm.t[1];
    vz                        = (u16)coord->workm.t[2];
    *scratch                  = block;
    block->worldPoint.vz      = vz;
    vec                       = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        if (block->depth >= 0x41) {
            prim           = gGpuPrimCursor;
            ang            = arg3;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2E);
            setRGB0(prim, 0x50, 0x50, 0x50);
            prim->tpage = 0x2B;
            prim->clut  = 0x43D0;
            u0          = (arg1 % 5) * 0x30;
            v0          = (arg1 / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 - 0x51;
            v0          = v0 - 0x80;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            block->extent.corner.x = (((arg2 * 47) / block->depth) * rsin(ang)) >> 12;
            block->extent.corner.y = (((arg2 * 47) / block->depth) * rcos(ang)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            ang2                   = ang + 0x400;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((arg2 * 47) / block->depth) * rsin(ang2)) >> 12;
            block->extent.corner.y = (((arg2 * 47) / block->depth) * rcos(ang2)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
