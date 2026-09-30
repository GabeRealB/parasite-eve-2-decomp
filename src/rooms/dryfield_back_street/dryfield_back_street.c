#include "rooms/dryfield_back_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
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

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_back_street_8017F964[];
extern TaskDesc   D_dryfield_back_street_8017F98C[];

/// Volume last asked of the back street's ambience, or 0 when none is playing.
/// Written by `func_dryfield_back_street_8017D5D0` and cleared by state 0 of the
/// same task.
extern s32 D_dryfield_back_street_80181054;

/// The beam's two anchors, offsets on the effect's parent frame. The code
/// reaches the second both as element 1 and under its own label.

void func_dryfield_back_street_8017D5D0(Task*);
s32  func_dryfield_back_street_8017D748(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_back_street_8017D89C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_back_street_8017D8A4(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_back_street_8017D8AC(Task*, s32, TaskMessageArg, TaskMessageArg);

extern GpGridParams   D_dryfield_back_street_80180284[1];
extern GpObj4C        D_dryfield_back_street_801804C0[6];
extern GpObj4C        D_dryfield_back_street_80180688[11];
extern GpRoomCoordSet D_dryfield_back_street_80180FF8[1];

extern TaskDesc D_8014D8A4;

GpMsgEntry D_dryfield_back_street_8017F964[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_back_street_8017D748 },
    { 5105, func_dryfield_back_street_8017D89C },
    { 5103, func_dryfield_back_street_8017D8AC },
    { 5104, func_dryfield_back_street_8017D8A4 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_back_street_8017F98C[2] = {
    { 0, 32, func_dryfield_back_street_8017D5D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_dryfield_back_street_8017F9B4[1] = {
    { D_dryfield_back_street_80180284, D_dryfield_back_street_801804C0, D_dryfield_back_street_80180688, NULL },
};

u8* D_dryfield_back_street_8017F9C4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_back_street_8017F9C8[1] = {
    { { .bytes = { 5, 0 } } },
};

GpRoomCoordRec D_dryfield_back_street_8017F9CC[1] = {
    { D_dryfield_back_street_80180FF8, NULL },
};

GpWarpRec D_dryfield_back_street_8017F9D4[4] = {
    { { .words = { 1024, -9597, 0, 4667 } }, { 0, 0, 0, 0 }, { .words = { 1024, -8696, 0, 5479 } }, { 0, 0, 0, 0 }, 0x52050002, 0x52050001, 0, 2, 0, 470 },
    { { .words = { 2048, -7539, 0, 5536 } }, { 0, 0, 0, 0 }, { .words = { 1024, -8028, 0, 5175 } }, { 0, 0, 0, 0 }, 0x52050004, 0x52050003, 0x52050005, 2, 0, 469 },
    { { .words = { 2048, -1423, 0, 5449 } }, { 0, 0, 0, 0 }, { .words = { 2048, -2346, 0, 5223 } }, { 0, 0, 0, 0 }, 0x52050002, 0x52050001, 0x52050005, 3, 0, 468 },
    { { .words = { 2048, 9463, 2, 5532 } }, { 0, 0, 0, 0 }, { .words = { 3072, 9800, 2, 4668 } }, { 0, 0, 0, 0 }, 0x52050002, 0x52050001, 0x52050005, 5, 0, 467 },
};

SVECTOR D_dryfield_back_street_8017FAB4[23] = {
#include "assets/dryfield_back_street_collision_02CC4_normals.inc"
};

SVECTOR D_dryfield_back_street_8017FB6C[88] = {
#include "assets/dryfield_back_street_collision_02CC4_verts.inc"
};

GpGridFace D_dryfield_back_street_8017FE2C[39] = {
#include "assets/dryfield_back_street_collision_02CC4_faces.inc"
};

s16 D_dryfield_back_street_80180000[266] = {
#include "assets/dryfield_back_street_collision_02CC4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_back_street_80180000[i])
s16* D_dryfield_back_street_80180214[28] = {
#include "assets/dryfield_back_street_collision_02CC4_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_back_street_80180284[1] = {
    { NULL, D_dryfield_back_street_8017FAB4, D_dryfield_back_street_8017FB6C, D_dryfield_back_street_8017FE2C, D_dryfield_back_street_80180214, 0x2AFE, 100, 7, 4, 4000, 39 },
};

GpViewRec D_dryfield_back_street_801802A8[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -850, 0x7148, -4850 } }, 329 },
    { { { { 696, 0, 4036 }, { -1070, 3949, 184 }, { -3892, -1085, 671 } }, { 4016, 385, -4128 } }, 230 },
    { { { { 681, 0, 4038 }, { 402, 4075, -67 }, { -4018, 408, 677 } }, { -2416, 1408, -4201 } }, 257 },
    { { { { 615, 0, -4049 }, { -873, 3999, -132 }, { 3954, 883, 600 } }, { 2448, 1600, -4230 } }, 230 },
    { { { { 173, 0, -4092 }, { -1464, 3824, -62 }, { 3821, 1465, 162 } }, { -2046, 2454, -4167 } }, 257 },
};

SpriteBatch D_dryfield_back_street_8018035C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_back_street_8018036C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_back_street_8018037C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_back_street_8018038C[5] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 1295, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -72, 1258, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 152 } }, 80, -72, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, 96, -88, 1022, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 216 } }, 120, -96, 785, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_back_street_801803F0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_back_street_80180408[5] = {
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -8, 1663, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -16, 1680, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -16, 1714, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -24, 1678, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -16, 1669, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_back_street_8018046C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_back_street_80180484[5] = {
    { { .empty = D_dryfield_back_street_8018035C }, D_dryfield_back_street_8018035C, NULL },
    { { .empty = D_dryfield_back_street_8018036C }, D_dryfield_back_street_8018036C, NULL },
    { { .empty = D_dryfield_back_street_8018037C }, D_dryfield_back_street_8018037C, NULL },
    { { .elements = D_dryfield_back_street_8018038C }, D_dryfield_back_street_801803F0, NULL },
    { { .elements = D_dryfield_back_street_80180408 }, D_dryfield_back_street_8018046C, NULL },
};

GpObj4C D_dryfield_back_street_801804C0[6] = {
    { NULL, NULL, NULL, { -6272, -3296, 4752, 0 }, { { 0, -4320, -1584, 0 }, { 0, -4320, 1584, 0 }, { 0, 4320, -1584, 0 }, { 0, 4320, 1584, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -5888, -3504, 4672, 0 }, { { 0, -4528, 1584, 0 }, { 0, -4528, -1584, 0 }, { 0, 4528, 1584, 0 }, { 0, 4528, -1584, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -192, -4160, 4576, 0 }, { { -236, -5184, 1562, 0 }, { 228, -5184, -1572, 0 }, { -236, 5184, 1562, 0 }, { 228, 5184, -1572, 0 } }, { -4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 5418, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -320, -4128, 4608, 0 }, { { 229, -5152, -1571, 0 }, { -235, -5152, 1563, 0 }, { 229, 5152, -1571, 0 }, { -235, 5152, 1563, 0 } }, { 4053, 0, 600, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 4256, -4096, 4704, 0 }, { { 0, -5120, -1584, 0 }, { 0, -5120, 1584, 0 }, { 0, 5120, -1584, 0 }, { 0, 5120, 1584, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5345, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 4416, -4352, 4736, 0 }, { { 0, -5376, 1584, 0 }, { 0, -5376, -1584, 0 }, { 0, 5376, 1584, 0 }, { 0, 5376, -1584, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 5, 129, 0 },
};

GpObj4C D_dryfield_back_street_80180688[11] = {
    { NULL, NULL, NULL, { -9648, -55, 4496, 0 }, { { -400, 0, -560, 0 }, { 400, 0, -560, 0 }, { -400, 0, 560, 0 }, { 400, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 686, 0, 3, 18, 2, 0 },
    { NULL, NULL, NULL, { -7872, -55, 5776, 0 }, { { 832, 0, -352, 0 }, { 832, 0, 352, 0 }, { -832, 0, -352, 0 }, { -832, 0, 352, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 902, 0, 6, 33, 2, 0 },
    { NULL, NULL, NULL, { -1616, -48, 5744, 0 }, { { 688, 0, -352, 0 }, { 688, 0, 352, 0 }, { -688, 0, -352, 0 }, { -688, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 770, 0, 7, 49, 2, 0 },
    { NULL, NULL, NULL, { 9424, -48, 5712, 0 }, { { 784, 0, -384, 0 }, { 784, 0, 384, 0 }, { -784, 0, -384, 0 }, { -784, 0, 384, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, 0, 9, 65, 2, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -64, 5024, 0 }, { { -1072, 0, -416, 0 }, { 1072, 0, -416, 0 }, { -1072, 0, 416, 0 }, { 1072, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1144, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { -6113, -64, 5569, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 8288, -64, 2144, 0 }, { { -1696, 0, -208, 0 }, { 1760, 0, -208, 0 }, { -800, 0, 1008, 0 }, { 736, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4092, 0 }, 1768, 2, 4, 0, 4, 0 },
    { NULL, NULL, NULL, { 7456, -64, 5696, 0 }, { { -1248, 0, -368, 0 }, { 1248, 0, -368, 0 }, { -1248, 0, 368, 0 }, { 1248, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 200, 0, -4093, 0 }, 1299, 2, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -4576, -64, 5536, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 5152, -64, 5664, 0 }, { { 1056, 0, -384, 0 }, { 1056, 0, 384, 0 }, { -1056, 0, -384, 0 }, { -1056, 0, 384, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1123, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x2790, -64, 5520, 0 }, { { -224, 0, -560, 0 }, { 448, 0, -560, 0 }, { -448, 0, 560, 0 }, { 224, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { -4076, 0, 401, 0 }, 715, 2, 10, 0, 130, 0 },
};

GpAreaTmdRec D_dryfield_back_street_801809CC[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_back_street_801809E4[3] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 56, 56, 1, 0, { 0, 0 }, D_801602C0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_back_street_80180A08[2] = {
    { 15, 15, 0, 0, { 0, 0 }, D_8013BE28 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_back_street_80180A20[3] = {
    { 15, 0, 0, 6900, -2000, 6050, 0, 0, 0, 2, 0 },
    { 15, 0, 1, 8000, -2800, 3600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_back_street_80180A50[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AEA4, D_dryfield_back_street_801809CC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017AED4, D_dryfield_back_street_801809E4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_back_street_80180A20, D_dryfield_back_street_80180A08 },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordPointLight D_dryfield_back_street_80180AB8[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2985, 5488 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2990, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2985, 5062 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6020, -2985, 5065 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -2985, 5488 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2980, -2985, 5488 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5990, -3500, 5452 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2941, -3500, 5065 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 5000 },
};

GpRoomCoordSet D_dryfield_back_street_80180FF8[1] = {
    { 0, NULL, 14, D_dryfield_back_street_80180AB8, 0, NULL },
};

s32 D_dryfield_back_street_80181010[3] = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

GpRoomParamRec D_dryfield_back_street_8018101C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_back_street_80181024[1] = {
    { 0, 0, 1, 0, D_dryfield_back_street_80181010 },
};

GpRoomParamRec D_dryfield_back_street_8018102C[1] = {
    { 0, 1, 0, 0, D_dryfield_back_street_80181010 },
};

GpRoomParamRec* D_dryfield_back_street_80181034[8] = {
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_80181024,
    D_dryfield_back_street_8018102C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
    D_dryfield_back_street_8018101C,
};

s32 D_dryfield_back_street_80181054 = 0;

static void func_dryfield_back_street_8017D8B4(Task* task);
static void func_dryfield_back_street_8017D910(Task* task);

/// Back street ambience: state 0 clears the recorded volume and advances, state
/// 1 maps the current camera view to a target volume and stereo pan - 0x1E/+4,
/// 0x32/-8 and 0x64/-0xC for views 3/4/5, 0 and centre elsewhere - and, whenever
/// the volume differs from the recorded one, enqueues the matching fade event:
/// type 6 to start the track, type 7 to stop it, type A to retune it, then
/// records the new volume.
void func_dryfield_back_street_8017D5D0(Task* task)
{
    s32 vol;
    s32 pan;

    switch (task->state) {
        case 0:
            D_dryfield_back_street_80181054 = 0;
            task->state                     = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    switch (Gp_GetViewIndex()) {
        case 3:
            vol = 0x1E;
            pan = 4;
            break;
        case 4:
            vol = 0x32;
            pan = -8;
            break;
        case 5:
            vol = 0x64;
            pan = -0xC;
            break;
        default:
            pan = 0;
            vol = 0;
            break;
    }

    if (vol == D_dryfield_back_street_80181054) {
        return;
    }
    if (D_dryfield_back_street_80181054 == 0) {
        SndEvt_EnqueueType6(0x52050006, pan, (s8)(((0x64 - vol) * 0x7F) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x52050006, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x52050006, pan, (s8)(((0x64 - vol) * 0x7F) / 100));
    }
    D_dryfield_back_street_80181054 = vol;
}

/// Message handler for the back street's two events. Copies the incoming
/// record to the outgoing one and answers by editing `room` of the copy; a
/// non-zero `queryOnly` suppresses the side effects.
///
/// On stage 2 (`gGameSession->location.loc.stage`), message 7 answers 1 while event
/// nibble 0x3C is clear and the stage byte, read once into a local, when it is
/// set. Message 9 with nibble 0x3F clear runs CAP command 2 on stage 2 (9
/// otherwise), sets nibble 2 of the record's flag index and returns 0. Any
/// other case, on stage 2, enqueues the type-7 event the ambience task uses to
/// stop sound 0x52050006, and returns 1.
s32 func_dryfield_back_street_8017D748(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->location.loc.stage;
    if (s1 == 2) {
        if (in->areaId == 7) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(0x3C) == 0) {
                    out->room = 1;
                } else {
                    out->room = s1;
                }
            }
        }
    }
    if ((in->areaId == 9) && (GameFlag_GetNibble(0x3F) == 0)) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            s32 cmd = 9;

            if (gGameSession->location.loc.stage == 2) {
                cmd = 2;
            }
            Gp_RunCapCmd1(cmd);
            Gp_SetNibbleIf(in->flagId, 2);
        }
        return 0;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gGameSession->location.loc.stage == 2) {
            SndEvt_EnqueueType7(0x52050006, 0xF);
        }
    }
    return 1;
}

s32 func_dryfield_back_street_8017D89C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8A4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8AC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7, spawns the tasks of
/// `D_dryfield_back_street_8017F98C` (the ambience task) and moves on to the
/// next state.
static void func_dryfield_back_street_8017D8B4(Task* task)
{
    task->msgTable = D_dryfield_back_street_8017F964;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_dryfield_back_street_8017F98C, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// The room entry task's idle state.
static void func_dryfield_back_street_8017D910(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_back_street_8017D5C4 = {
    { func_dryfield_back_street_8017D8B4, func_dryfield_back_street_8017D910, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_back_street_8017D918(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_back_street_8017D5C4;
    sp.funcs[task->state](task);
}

/// Per-frame room task. On its first run it stores the effect ids 0x60296,
/// 0x60297 and 0x60298 in three gameplay globals; every run it sets
/// `roomEffectMode` to 2.
void func_dryfield_back_street_8017D970(Task* task)
{
    if (task->state == 0) {
        D_80115758  = 0x60296;
        D_8011572C  = 0x60297;
        D_80115750  = 0x60298;
        task->state = 1;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_dryfield_back_street_8017D9D0(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_dryfield_back_street_8017E434(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_back_street_8017ED1C(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
