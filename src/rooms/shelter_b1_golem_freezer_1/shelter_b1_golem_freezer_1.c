#include "rooms/shelter_b1_golem_freezer_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_160700.h"
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
#include "gameplay/scene_runtime.h"
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

/// The room's message table, installed on the room task.
extern TaskMessageEntry D_shelter_b1_golem_freezer_1_8017E6A8[];

extern s16                D_shelter_b1_golem_freezer_1_8017E6D0[3];
extern WorldCollisionGrid D_shelter_b1_golem_freezer_1_8017E714;
extern SVECTOR            D_shelter_b1_golem_freezer_1_8017E738[];
extern SVECTOR            D_shelter_b1_golem_freezer_1_8017E740[];

static void func_shelter_b1_golem_freezer_1_8017D744(s32 arg0);
static void func_shelter_b1_golem_freezer_1_8017D7CC(GfxCoord* arg0, s16* arg1);
static void _shelterB1GolemFreezer1DrawFloorMist(const GfxCoord* coord, u16 frame, s16 sizeFactor, s16 angle);
static void _shelterB1GolemFreezer1IdleRoomState(Task* task);

static s32 _shelterB1GolemFreezer1RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
s32        func_shelter_b1_golem_freezer_1_8017D5D8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB1GolemFreezer1IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
s32        func_shelter_b1_golem_freezer_1_8017D624(Task*, s32, RoomEventMsg*, s32);

/// Room message used by the key-item menu to request use of an item.
enum { SHELTER_B1_GOLEM_FREEZER_1_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Layout and projection constants of the ten-cell floor-mist texture sheet.
enum {
    SHELTER_B1_GOLEM_FREEZER_1_MIST_FRAME_COUNT        = 10,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_CELLS_PER_ROW      = 5,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_CELL_PITCH_TEXELS  = 48,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_UV_SPAN_TEXELS     = SHELTER_B1_GOLEM_FREEZER_1_MIST_CELL_PITCH_TEXELS - 1,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_TOP_V              = -128,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_MIN_DEPTH          = 65,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_SHADE              = 80,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_QUARTER_TURN       = ONE / 4,
    SHELTER_B1_GOLEM_FREEZER_1_MIST_TRIG_FRACTION_BITS = 12,
};

TaskMessageEntry D_shelter_b1_golem_freezer_1_8017E6A8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_golem_freezer_1_8017D5D8 },
    { SHELTER_B1_GOLEM_FREEZER_1_MESSAGE_USE_KEY_ITEM, _shelterB1GolemFreezer1RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_golem_freezer_1_8017D624 },
    { ROOM_MESSAGE_COMMAND, _shelterB1GolemFreezer1IgnoreRoomCommand },
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

/// Rejects every key-item use request in this room with a zero reply.
///
/// Keeps the task-message ABI; neither the receiver nor either payload is read.
static s32 _shelterB1GolemFreezer1RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return 0;
}

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both on to `mapShelterRoomVariantResolve`. Always answers 1.
s32 func_shelter_b1_golem_freezer_1_8017D5D8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    return 1;
}

/// Ignores room commands and returns zero without changing room state.
///
/// Keeps the task-message ABI; the command selector and argument remain unread.
static s32 _shelterB1GolemFreezer1IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Message-table handler for message 0x13EF: when the message's `field_2` is 1
/// and the session's place is 0x15, calls `actor160700StartMeetingScript`. Always answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D624(Task* arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    if (msg->warp == 1 && gGameSession->location.loc.variant == 0x15) {
        actor160700StartMeetingScript();
    }
    return 0;
}

/// The room task's first state: installs the room's message table, takes game
/// pointer slot 7, calls `actor160700RestoreMeetingAnimation` while the session's place is 0x15,
/// runs `func_shelter_b1_golem_freezer_1_8017D744` and moves on to the next
/// state.
static void func_shelter_b1_golem_freezer_1_8017D66C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_golem_freezer_1_8017E6A8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == 0x15) {
        actor160700RestoreMeetingAnimation();
    }
    func_shelter_b1_golem_freezer_1_8017D744(0);
    arg0->state = arg0->state + 1;
}

/// Leaves the initialized room task waiting for messages without advancing its state.
static void _shelterB1GolemFreezer1IdleRoomState(Task* task)
{
    // Retain the original unused stack reservation.
    char unusedStackFrame[0x10];
}

/// State handlers of the room task `func_shelter_b1_golem_freezer_1_8017D6EC`
/// runs: its setup, an idle state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b1_golem_freezer_1_8017D5C4 = {
    { func_shelter_b1_golem_freezer_1_8017D66C, _shelterB1GolemFreezer1IdleRoomState, taskKill }
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
    Task* slot   = sceneFindPlacedActor(0);
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
            effectSpawn(EFFECT_GOLEM_FREEZER_FLOOR_MIST, NULL, (GOLEM_RAND() & 0x10FF) + 0x85400, &pos);
        }
    }
    switch (viewGetMappedIndex() & 0xFF) {
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

/// Initializes a mist puff's constant displacement in its coordinate's XZ plane.
///
/// `work` borrows writable effect storage; `speed` is 1..255 local-coordinate
/// units per task tick. Stores the requested magnitude in `step` and the
/// displacement in `move`. Approximate Q12 normalization followed by GTE
/// scaling rounds each component down, so its length need not equal `speed`.
///
/// Consumes two successive shared LCG draws for X and Z in -127..128, with
/// Y zero. Their correlation excludes a zero direction. Only `step` and the
/// three `move` components change; the vector's pad is preserved. Clobbers
/// GTE state and retains no pointer.
static inline void _shelterB1GolemFreezer1InitializeMistDrift(EffectWork* work, s16 speed)
{
    enum {
        SHELTER_B1_GOLEM_FREEZER_1_MIST_DIRECTION_MIDPOINT  = 0x80,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_DIRECTION_MASK      = 0xFF,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_RANDOM_SAMPLE_SHIFT = 16,
    };
    u32 directionXState;
    u32 directionZState;

    // Draw a nonzero horizontal bearing before normalizing it in place.
    work->step      = speed;
    work->move.vy   = 0;
    directionXState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = directionXState;
    work->move.vx   = SHELTER_B1_GOLEM_FREEZER_1_MIST_DIRECTION_MIDPOINT - ((directionXState >> SHELTER_B1_GOLEM_FREEZER_1_MIST_RANDOM_SAMPLE_SHIFT) & SHELTER_B1_GOLEM_FREEZER_1_MIST_DIRECTION_MASK);
    directionZState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = directionZState;
    work->move.vz   = SHELTER_B1_GOLEM_FREEZER_1_MIST_DIRECTION_MIDPOINT - ((directionZState >> SHELTER_B1_GOLEM_FREEZER_1_MIST_RANDOM_SAMPLE_SHIFT) & SHELTER_B1_GOLEM_FREEZER_1_MIST_DIRECTION_MASK);
    VectorNormalSS(&work->move, &work->move);

    // Convert the Q12 bearing to whole coordinate units for each task tick.
    gte_lddp(work->step);
    gte_ldsv(&work->move);
    gte_gpf12();
    gte_stsv(&work->move);
}

void shelterB1GolemFreezer1FloorMistTask(Task* task)
{
    enum {
        SHELTER_B1_GOLEM_FREEZER_1_MIST_STATE_INIT          = 0,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_STATE_DRIFT         = 1,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_SIZE_MASK           = 0xFFF,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_ANGLE_MASK          = ONE - 1,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_PERIOD_PRESENT_MASK = 0xF000,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_PERIOD_SHIFT        = 12,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_PERIOD_MASK         = 0x7,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_DEFAULT_PERIOD      = 1,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_SPEED_PRESENT_MASK  = 0xFF0000,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_SPEED_SHIFT         = 16,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_SPEED_MASK          = 0xFF,
        SHELTER_B1_GOLEM_FREEZER_1_MIST_DEFAULT_SPEED       = 64,
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s16         speed;

    work->age++;
    if (task->state == SHELTER_B1_GOLEM_FREEZER_1_MIST_STATE_INIT) {
        // Decode the spawn word and choose one screen rotation for the puff's lifetime.
        work->scale     = task->spawnArg1.value & SHELTER_B1_GOLEM_FREEZER_1_MIST_SIZE_MASK;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & SHELTER_B1_GOLEM_FREEZER_1_MIST_ANGLE_MASK;

        // Bit 15 participates in the presence test but not the decoded period.
        if (task->spawnArg1.value & SHELTER_B1_GOLEM_FREEZER_1_MIST_PERIOD_PRESENT_MASK) {
            work->period = (task->spawnArg1.value >> SHELTER_B1_GOLEM_FREEZER_1_MIST_PERIOD_SHIFT) & SHELTER_B1_GOLEM_FREEZER_1_MIST_PERIOD_MASK;
        } else {
            work->period = SHELTER_B1_GOLEM_FREEZER_1_MIST_DEFAULT_PERIOD;
        }

        work->age   = 0;
        task->state = SHELTER_B1_GOLEM_FREEZER_1_MIST_STATE_DRIFT;

        if (task->spawnArg1.value & SHELTER_B1_GOLEM_FREEZER_1_MIST_SPEED_PRESENT_MASK) {
            speed = (task->spawnArg1.value >> SHELTER_B1_GOLEM_FREEZER_1_MIST_SPEED_SHIFT) & SHELTER_B1_GOLEM_FREEZER_1_MIST_SPEED_MASK;
        } else {
            speed = SHELTER_B1_GOLEM_FREEZER_1_MIST_DEFAULT_SPEED;
        }

        _shelterB1GolemFreezer1InitializeMistDrift(work, speed);
    }

    // Draw the composed centre before changing the next tick's local translation.
    _shelterB1GolemFreezer1DrawFloorMist(coord, work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    // Age zero advances cell zero immediately; the remaining cells last a full period.
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= SHELTER_B1_GOLEM_FREEZER_1_MIST_FRAME_COUNT) {
            effectKillTask(work, task);
        }
    }
}

/// Stores the rightward and upward pixel offsets to one mist billboard corner.
///
/// `projection` borrows a live, word-aligned scratch block with positive
/// `depth` in SZ3/4 units. `sizeFactor * 47 / depth` is the signed half-diagonal
/// in integer pixels, truncated toward zero before Q12 rotation. Rotation
/// products must fit s32; the right shift rounds down. The caller supplies
/// size factors in 0..4095 and depths of at least 65, keeping products in range.
///
/// `cornerAngle` uses 4096 units per turn and need not be normalized. For a
/// positive size, zero points up and a quarter turn points right; the caller
/// subtracts the upward offset from screen Y. Only `extent.corner.x` and
/// `extent.corner.y` change. No storage is allocated or pointer retained.
static inline void _shelterB1GolemFreezer1ComputeMistCornerOffset(EffectShapeScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    q19_12 angleSine;
    q19_12 angleCosine;
    s32    halfDiagonalPixels;

    angleSine                   = rsin(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * SHELTER_B1_GOLEM_FREEZER_1_MIST_UV_SPAN_TEXELS) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * angleSine) >> SHELTER_B1_GOLEM_FREEZER_1_MIST_TRIG_FRACTION_BITS;
    angleCosine                 = rcos(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * SHELTER_B1_GOLEM_FREEZER_1_MIST_UV_SPAN_TEXELS) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * angleCosine) >> SHELTER_B1_GOLEM_FREEZER_1_MIST_TRIG_FRACTION_BITS;
}

/// Queues one shaded, additive, semitransparent frame of the floor-mist billboard.
///
/// `coord` borrows a composed translation in the input space of `GsWSMATRIX`;
/// projection uses its low signed 16 bits. `frame` is 0..9 in a five-column sheet
/// of 48-by-48 texel cells. `sizeFactor * 47 / (SZ3/4)` is the integer pixel
/// half-diagonal before rotation; `angle` uses 4096 units per turn.
///
/// Requires initialized GTE settings, one word-aligned `EffectShapeScratch` block
/// of free scratch stack, and packet-arena space for one `POLY_FT4`. Negative GTE
/// FLAG or depth below 65 rejects the sprite before allocating a packet. Scratch
/// is released on every path; queued packets live until the arena is reused.
static void _shelterB1GolemFreezer1DrawFloorMist(const GfxCoord* coord, u16 frame, s16 sizeFactor, s16 angle)
{
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    s32                 leftU;
    s32                 topV;
    s32                 rightU;
    s32                 bottomV;
    s32                 cornerAngle;
    s32                 perpendicularAngle;

    // Reserve one projection block and narrow the composed centre to GPU coordinate width.
    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    projection->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy = (u16)coord->workm.t[1];
    projection->worldPoint.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        if (projection->depth >= SHELTER_B1_GOLEM_FREEZER_1_MIST_MIN_DEPTH) {
            quad           = gGpuPrimCursor;
            cornerAngle    = angle;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setSemiTrans(quad, 1);
            setRGB0(quad, SHELTER_B1_GOLEM_FREEZER_1_MIST_SHADE, SHELTER_B1_GOLEM_FREEZER_1_MIST_SHADE, SHELTER_B1_GOLEM_FREEZER_1_MIST_SHADE);
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(256, 271);
            leftU       = (frame % SHELTER_B1_GOLEM_FREEZER_1_MIST_CELLS_PER_ROW) * SHELTER_B1_GOLEM_FREEZER_1_MIST_CELL_PITCH_TEXELS;
            topV        = (frame / SHELTER_B1_GOLEM_FREEZER_1_MIST_CELLS_PER_ROW) * SHELTER_B1_GOLEM_FREEZER_1_MIST_CELL_PITCH_TEXELS;
            rightU      = leftU + SHELTER_B1_GOLEM_FREEZER_1_MIST_UV_SPAN_TEXELS;
            bottomV     = topV + SHELTER_B1_GOLEM_FREEZER_1_MIST_TOP_V + SHELTER_B1_GOLEM_FREEZER_1_MIST_UV_SPAN_TEXELS;
            topV        = topV + SHELTER_B1_GOLEM_FREEZER_1_MIST_TOP_V;
            setUV4(quad, leftU, topV, rightU, topV, leftU, bottomV, rightU, bottomV);
            // Two perpendicular half-diagonals supply the opposite corner pairs.
            _shelterB1GolemFreezer1ComputeMistCornerOffset(projection, sizeFactor, cornerAngle);
            quad->x0           = projection->screenX + (u16)projection->extent.corner.x;
            quad->x3           = projection->screenX - (u16)projection->extent.corner.x;
            quad->y0           = projection->screenY - (u16)projection->extent.corner.y;
            perpendicularAngle = cornerAngle + SHELTER_B1_GOLEM_FREEZER_1_MIST_QUARTER_TURN;
            quad->y3           = projection->screenY + (u16)projection->extent.corner.y;
            _shelterB1GolemFreezer1ComputeMistCornerOffset(projection, sizeFactor, perpendicularAngle);
            quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
            quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
            quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
            quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
