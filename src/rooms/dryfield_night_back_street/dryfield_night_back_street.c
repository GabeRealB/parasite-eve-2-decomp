#include "rooms/dryfield_night_back_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

#define D_dryfield_night_back_street_8018036C (D_dryfield_night_back_street_8018034C + 4)
#define D_dryfield_night_back_street_8018037C (D_dryfield_night_back_street_8018034C + 6)
#define D_dryfield_night_back_street_8018038C (D_dryfield_night_back_street_8018034C + 8)

/// The room's message table, installed on the room entry task.
extern TaskMessageEntry D_dryfield_night_back_street_80180324[];

// Indexed views below share one contiguous table.
s32 func_dryfield_night_back_street_8017D5D0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_back_street_8017D724(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_back_street_8017D72C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_back_street_8017D734(Task*, s32, TaskMessageArg, TaskMessageArg);

extern WorldCollisionGrid         D_dryfield_night_back_street_80180B34[1];
extern WorldCollisionTrigger      D_dryfield_night_back_street_80180D70[6];
extern WorldCollisionTrigger      D_dryfield_night_back_street_80180F38[10];
extern WorldCoordRoomAmbientEntry D_dryfield_night_back_street_801815C8[6];
extern WorldCoordRoomLights       D_dryfield_night_back_street_80181470[1];

extern TaskDesc D_8014D8A4;

TaskMessageEntry D_dryfield_night_back_street_80180324[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_back_street_8017D5D0 },
    { 5105, func_dryfield_night_back_street_8017D724 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_back_street_8017D734 },
    { 5104, func_dryfield_night_back_street_8017D72C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_dryfield_night_back_street_8018034C[10] = {
    { -9920, -2300, 5110, 0 },
    { -9920, -2300, 3900, 0 },
    { -9850, -2330, 5110, 0 },
    { -9850, -2330, 3900, 0 },
    { -8190, -1800, 5850, 0 },
    { -6800, -1800, 5850, 0 },
    { -2110, -1800, 6000, 0 },
    { -820, -1800, 6000, 0 },
    { 8870, -1800, 6000, 0 },
    { 10150, -1800, 6000, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_dryfield_night_back_street_801803AC[1] = {
    { D_dryfield_night_back_street_80180B34, D_dryfield_night_back_street_80180D70, D_dryfield_night_back_street_80180F38, NULL },
};

WorldCoordRoomLighting D_dryfield_night_back_street_801803BC[1] = {
    { D_dryfield_night_back_street_80181470, D_dryfield_night_back_street_801815C8 },
};

u8* D_dryfield_night_back_street_801803C4[1] = {
    gViewIdentityMap,
};

GpViewCountRec D_dryfield_night_back_street_801803C8[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_dryfield_night_back_street_801803CC[4] = {
    { { .words = { 1024, -9597, 0, 4667 } }, { 0, 0, 0, 0 }, { .words = { 1024, -8696, 0, 5479 } }, { 0, 0, 0, 0 }, 0x53050002, 0x53050001, 0, 2, 0, 470 },
    { { .words = { 2048, -7539, 0, 5536 } }, { 0, 0, 0, 0 }, { .words = { 1024, -8028, 0, 5175 } }, { 0, 0, 0, 0 }, 0x53050004, 0x53050003, 0x53050005, 2, 0, 469 },
    { { .words = { 2048, -1423, 0, 5449 } }, { 0, 0, 0, 0 }, { .words = { 2048, -2346, 0, 5223 } }, { 0, 0, 0, 0 }, 0x53050002, 0x53050001, 0x53050005, 3, 0, 468 },
    { { .words = { 2048, 9463, 2, 5532 } }, { 0, 0, 0, 0 }, { .words = { 3072, 9800, 2, 4668 } }, { 0, 0, 0, 0 }, 0x53050002, 0x53050001, 0x53050005, 5, 0, 467 },
};

SVECTOR D_dryfield_night_back_street_801804AC[19] = {
#include "assets/dryfield_night_back_street_collision_03574_normals.inc"
};

SVECTOR D_dryfield_night_back_street_80180544[72] = {
#include "assets/dryfield_night_back_street_collision_03574_verts.inc"
};

WorldCollisionGridFace D_dryfield_night_back_street_80180784[32] = {
#include "assets/dryfield_night_back_street_collision_03574_faces.inc"
};

s16 D_dryfield_night_back_street_80180904[224] = {
#include "assets/dryfield_night_back_street_collision_03574_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_back_street_80180904[i])
s16* D_dryfield_night_back_street_80180AC4[28] = {
#include "assets/dryfield_night_back_street_collision_03574_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_back_street_80180B34[1] = {
    { NULL, D_dryfield_night_back_street_801804AC, D_dryfield_night_back_street_80180544, D_dryfield_night_back_street_80180784, D_dryfield_night_back_street_80180AC4, 0x2AFE, 100, 7, 4, 4000, 32 },
};

GpViewRec D_dryfield_night_back_street_80180B58[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -850, 0x7148, -4850 } }, 329 },
    { { { { 696, 0, 4036 }, { -1070, 3949, 184 }, { -3892, -1085, 671 } }, { 4016, 385, -4128 } }, 230 },
    { { { { 681, 0, 4038 }, { 402, 4075, -67 }, { -4018, 408, 677 } }, { -2416, 1408, -4201 } }, 257 },
    { { { { 615, 0, -4049 }, { -873, 3999, -132 }, { 3954, 883, 600 } }, { 2448, 1600, -4230 } }, 230 },
    { { { { 173, 0, -4092 }, { -1464, 3824, -62 }, { 3821, 1465, 162 } }, { -2046, 2454, -4167 } }, 257 },
};

SpriteBatch D_dryfield_night_back_street_80180C0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_back_street_80180C1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_back_street_80180C2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_back_street_80180C3C[5] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 1295, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -72, 1258, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 152 } }, 80, -72, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, 96, -88, 1022, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 216 } }, 120, -96, 785, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_back_street_80180CA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_back_street_80180CB8[5] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -16, 1714, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -16, 1680, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -24, 1678, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -16, 1669, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -8, 1663, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_back_street_80180D1C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_back_street_80180D34[5] = {
    { { .empty = D_dryfield_night_back_street_80180C0C }, D_dryfield_night_back_street_80180C0C, NULL },
    { { .empty = D_dryfield_night_back_street_80180C1C }, D_dryfield_night_back_street_80180C1C, NULL },
    { { .empty = D_dryfield_night_back_street_80180C2C }, D_dryfield_night_back_street_80180C2C, NULL },
    { { .elements = D_dryfield_night_back_street_80180C3C }, D_dryfield_night_back_street_80180CA0, NULL },
    { { .elements = D_dryfield_night_back_street_80180CB8 }, D_dryfield_night_back_street_80180D1C, NULL },
};

WorldCollisionTrigger D_dryfield_night_back_street_80180D70[6] = {
    { NULL, NULL, NULL, { -6272, -3296, 4752, 0 }, { { 0, -4320, -1584, 0 }, { 0, -4320, 1584, 0 }, { 0, 4320, -1584, 0 }, { 0, 4320, 1584, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5888, -3504, 4672, 0 }, { { 0, -4528, 1584, 0 }, { 0, -4528, -1584, 0 }, { 0, 4528, 1584, 0 }, { 0, 4528, -1584, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -4160, 4576, 0 }, { { -236, -5184, 1562, 0 }, { 228, -5184, -1572, 0 }, { -236, 5184, 1562, 0 }, { 228, 5184, -1572, 0 } }, { -4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 5418, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -320, -4128, 4608, 0 }, { { 229, -5152, -1571, 0 }, { -235, -5152, 1563, 0 }, { 229, 5152, -1571, 0 }, { -235, 5152, 1563, 0 } }, { 4053, 0, 600, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, -4096, 4704, 0 }, { { 0, -5120, -1584, 0 }, { 0, -5120, 1584, 0 }, { 0, 5120, -1584, 0 }, { 0, 5120, 1584, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5345, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4416, -4352, 4736, 0 }, { { 0, -5376, 1584, 0 }, { 0, -5376, -1584, 0 }, { 0, 5376, 1584, 0 }, { 0, 5376, -1584, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_back_street_80180F38[10] = {
    { NULL, NULL, NULL, { -9648, -55, 4496, 0 }, { { -400, 0, -560, 0 }, { 400, 0, -560, 0 }, { -400, 0, 560, 0 }, { 400, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7872, -55, 5776, 0 }, { { 832, 0, -352, 0 }, { 832, 0, 352, 0 }, { -832, 0, -352, 0 }, { -832, 0, 352, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 902, WORLD_COLLISION_TRIGGER_ACTION_WARP, 6, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -48, 5744, 0 }, { { 688, 0, -352, 0 }, { 688, 0, 352, 0 }, { -688, 0, -352, 0 }, { -688, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9424, -48, 5712, 0 }, { { 784, 0, -384, 0 }, { 784, 0, 384, 0 }, { -784, 0, -384, 0 }, { -784, 0, 384, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -64, 5024, 0 }, { { -1072, 0, -416, 0 }, { 1072, 0, -416, 0 }, { -1072, 0, 416, 0 }, { 1072, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6113, -64, 5569, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8288, -64, 2144, 0 }, { { -1696, 0, -208, 0 }, { 1760, 0, -208, 0 }, { -800, 0, 1008, 0 }, { 736, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4092, 0 }, 1768, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4576, -64, 5536, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 5664, 0 }, { { 1056, 0, -384, 0 }, { 1056, 0, 384, 0 }, { -1056, 0, -384, 0 }, { -1056, 0, 384, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2740, -64, 5360, 0 }, { { -304, 0, -720, 0 }, { 656, 0, -720, 0 }, { -656, 0, 720, 0 }, { 304, 0, 720, 0 } }, { 0, 4112, 0, 0 }, { -4091, 0, 200, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_night_back_street_80181230[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6797, -1800, 5711 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3522, 3112 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8205, -1800, 5708 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3522, 3112 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -807, -1800, 5862 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3522, 3112 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8855, -1800, 5862 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3522, 3112 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x27B0, -1800, 5862 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3522, 3112 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9762, -2100, 4476 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3522, 3112 }, { 0, 0 } }, 1000, 3000 },
};

WorldCoordRoomLights D_dryfield_night_back_street_80181470[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_back_street_80181230), D_dryfield_night_back_street_80181230, 0, NULL },
};

AreaResource D_dryfield_night_back_street_80181488[2] = {
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_10, 0, { 0, 0 }, D_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_801814A0[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_8014D8A4 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_8016BE28 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_801814C4[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_8014D8A4 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_801814E8[2] = {
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_10, 0, { 0, 0 }, D_801491F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_80181500[2] = {
    { 37, 37, AREA_RESOURCE_FILE_GROUP_BASE_10, 0, { 0, 0 }, D_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_back_street_80181518[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B048, D_dryfield_night_back_street_80181488 },
    { NULL, NULL },
    { D_map_dryfield_full_8017B0B8, D_dryfield_night_back_street_801814A0 },
    { D_map_dryfield_full_8017B128, D_dryfield_night_back_street_801814C4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B1A8, D_dryfield_night_back_street_801814E8 },
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
    { D_map_dryfield_full_8017B1C8, D_dryfield_night_back_street_80181500 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_back_street_801815C8[6] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_back_street_801815C8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 615, 617 } },
    { .color = { 616, 618, 617, 617 } },
    { .color = { 616, 618, 618, 617 } },
    { .color = { 616, 617, 617, 616 } },
};

WorldCollisionFootstepSounds D_dryfield_night_back_street_801815F8 = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_dryfield_night_back_street_80181604[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_back_street_8018160C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_back_street_801815F8 },
};

WorldCollisionSurfaceProperties D_dryfield_night_back_street_80181614[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_back_street_801815F8 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_back_street_8018161C[8] = {
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_8018160C,
    D_dryfield_night_back_street_80181614,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
};

static void func_dryfield_night_back_street_8017D73C(Task* task);
static void func_dryfield_night_back_street_8017D780(Task* task);

/// Message handler for the back street's two events. Copies the incoming
/// record to the outgoing one and answers by editing `room` of the copy; a
/// non-zero `queryOnly` suppresses the side effects, as for every handler.
///
/// The response byte is the session's stage (`gGameSession.location.loc.stage`), read once
/// into a local and reused: the stage-2-only message 7 keeps that byte when
/// event nibble 0x3C is set and answers 1 when it is clear.
///
/// Message 9 is the room's progress gate -- with nibble 0x3F clear it runs CAP
/// command 2 on stage 2 (9 otherwise), writes the record's nibble and answers
/// 0. Any other message plays the "refused" sound when the session is on stage
/// 2 and answers 1.
///
/// The stage load is scheduled above the prologue, so this function's `.text`
/// starts 8 bytes before its `addiu $sp` - the `text` cut in the manifest.
s32 func_dryfield_night_back_street_8017D5D0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    GameSession* session = gGameSession;
    u8           response;

    *out     = *in;
    response = session->location.loc.stage;
    if (response == 2) {
        if (in->areaId == 7 && in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (GameFlag_GetNibble(0x3C) == 0) {
                out->room = 1;
            } else {
                out->room = response;
            }
        }
    }
    if (in->areaId == 9 && GameFlag_GetNibble(0x3F) == 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(gGameSession->location.loc.stage == 2 ? 2 : 9);
            Gp_SetNibbleIf(in->flagId, 2);
        }
        return 0;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE && gGameSession->location.loc.stage == 2) {
        SndEvt_EnqueueType7(0x52050006, 0xF);
    }
    return 1;
}

s32 func_dryfield_night_back_street_8017D724(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D72C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D734(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7 and moves on to the next state.
static void func_dryfield_night_back_street_8017D73C(Task* task)
{
    task->msgTable = D_dryfield_night_back_street_80180324;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room entry task's idle state.
static void func_dryfield_night_back_street_8017D780(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_night_back_street_8017D5C4 = {
    { func_dryfield_night_back_street_8017D73C, func_dryfield_night_back_street_8017D780, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_back_street_8017D788(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_back_street_8017D5C4;
    sp.funcs[task->state](task);
}

/// The room's light points. Two shafts come first, each a pair of ends at
/// 0x8018034C and 0x8018035C, then glow points in pairs: those of camera view
/// 2 at 0x8018036C, of view 3 at 0x8018037C and of views 4 and 5 at
/// 0x8018038C. The code names only the glow pairs, so the shafts are reached
/// as elements -4 and -2 of the view-2 array.
///
/// Per-frame room task. On its first run it stores the effect ids 0x6000A,
/// 0x60097 and 0x600E4 in three gameplay globals. Each run it sets
/// `roomEffectMode` to 2 and draws the lights of the current camera view
/// (`gGameSession->location.loc.view`): view 2 draws a sprite on each of its two
/// glow points with `glowDrawFlare` and a shaft
/// between each pair of shaft ends with
/// `glowDrawShaft`; view 3 adds its own two glows
/// to view 2's set; views 4 and 5 draw their shared two glows; every other
/// view draws nothing.
void func_dryfield_night_back_street_8017D7E0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758 = 0x6000A;
        D_8011572C = 0x60097;
        D_80115750 = 0x600E4;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->location.loc.view) {
        case 3:
            glowDrawFlare(&D_dryfield_night_back_street_8018037C[0], 1, 0x300);
            glowDrawFlare(&D_dryfield_night_back_street_8018037C[1], 1, 0x300);
            /* fallthrough */
        case 2:
            glowDrawFlare(&D_dryfield_night_back_street_8018036C[0], 0, 0x300);
            glowDrawFlare(&D_dryfield_night_back_street_8018036C[1], 0, 0x300);
            glowDrawShaft(&D_dryfield_night_back_street_8018036C[-4], 0x100);
            glowDrawShaft(&D_dryfield_night_back_street_8018036C[-2], 0x100);
            break;
        case 4:
        case 5:
            glowDrawFlare(&D_dryfield_night_back_street_8018038C[0], 1, 0x300);
            glowDrawFlare(&D_dryfield_night_back_street_8018038C[1], 1, 0x300);
            break;
    }
}

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/glow_draw_flare.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_dryfield_night_back_street_8017E390(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_dryfield_night_back_street_8017EDF4(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_night_back_street_8017F6DC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
