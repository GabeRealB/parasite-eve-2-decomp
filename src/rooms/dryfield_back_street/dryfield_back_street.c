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

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_back_street_8017F964[];
extern TaskDesc   D_dryfield_back_street_8017F98C[];

/// Volume last asked of the back street's ambience, or 0 when none is playing.
/// Written by `func_dryfield_back_street_8017D5D0` and cleared by state 0 of the
/// same task.
extern s32 D_dryfield_back_street_80181054;

/// The beam's two anchors, offsets on the effect's parent frame. The code
/// reaches the second both as element 1 and under its own label.

static void func_dryfield_back_street_8017DC74(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_dryfield_back_street_8017E0A0(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_dryfield_back_street_8017E924(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_dryfield_back_street_8017EFA4(GfxCoord* arg0, s16 arg1, u8* arg2);

void func_dryfield_back_street_8017D5D0(Task*);
s32  func_dryfield_back_street_8017D748(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_back_street_8017D89C(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_back_street_8017D8A4(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_back_street_8017D8AC(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_dryfield_back_street_80180284[1];
extern GpObj4C        D_dryfield_back_street_801804C0[6];
extern GpObj4C        D_dryfield_back_street_80180688[11];
extern GpRoomCoordSet D_dryfield_back_street_80180FF8[1];

extern TaskDesc D_8014D8A4;

GpMsgEntry D_dryfield_back_street_8017F964[5] = {
    { 5102, func_dryfield_back_street_8017D748 },
    { 5105, func_dryfield_back_street_8017D89C },
    { 5103, func_dryfield_back_street_8017D8AC },
    { 5104, func_dryfield_back_street_8017D8A4 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_back_street_8017F98C[2] = {
    { 0, 32, func_dryfield_back_street_8017D5D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

SVECTOR D_dryfield_back_street_8017F9A4[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

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

GpSprtElem D_dryfield_back_street_8018038C[5] = {
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

GpSprtElem D_dryfield_back_street_80180408[5] = {
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

GpPointLight D_dryfield_back_street_80180AB8[14] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2985, 5488 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2990, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2985, 5062 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6020, -2985, 5065 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -2985, 5488 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2980, -2985, 5488 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 500, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5990, -3500, 5452 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2941, -3500, 5065 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 5000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6000, -2985, 5955 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 3000, 5000 },
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
/// record to the outgoing one and answers by editing `field_3` of the copy; a
/// non-zero `field_5` suppresses the side effects.
///
/// On stage 2 (`gGameSession->at4.loc.stage`), message 7 answers 1 while event
/// nibble 0x3C is clear and the stage byte, read once into a local, when it is
/// set. Message 9 with nibble 0x3F clear runs CAP command 2 on stage 2 (9
/// otherwise), sets nibble 2 of the record's flag index and returns 0. Any
/// other case, on stage 2, enqueues the type-7 event the ambience task uses to
/// stop sound 0x52050006, and returns 1.
s32 func_dryfield_back_street_8017D748(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->at4.loc.stage;
    if (s1 == 2) {
        if (in->prefix.packed == 7) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(0x3C) == 0) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = s1;
                }
            }
        }
    }
    if ((in->prefix.packed == 9) && (GameFlag_GetNibble(0x3F) == 0)) {
        if (in->field_5 == 0) {
            s32 cmd = 9;

            if (gGameSession->at4.loc.stage == 2) {
                cmd = 2;
            }
            Gp_RunCapCmd1(cmd);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 0;
    }
    if (in->field_5 == 0) {
        if (gGameSession->at4.loc.stage == 2) {
            SndEvt_EnqueueType7(0x52050006, 0xF);
        }
    }
    return 1;
}

s32 func_dryfield_back_street_8017D89C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8A4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8AC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
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
    Gp_State1C->roomEffectMode = 2;
}

/// A flash on an effect's anchor, lasting `spawnArg1` frames. State 1 grows a
/// warm glow twice over and a shrinking ring around it; when it ends it hands
/// the tint to `Gp_DrawFadeQuad`, and state 2 fades a flare back out before
/// the work block is released. The task also ends when the room's event state
/// reaches 4, and does nothing while it is between 1 and 3.
void func_dryfield_back_street_8017D9D0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_dryfield_back_street_8017E0A0(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_dryfield_back_street_8017E0A0(coord, (s16)((u16)work->angle * 2), rgb);
                func_dryfield_back_street_8017DC74(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_dryfield_back_street_8017EFA4(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring of sixteen gouraud quads around the coordinate's projected
/// position, when it projects. The ring runs from radius
/// `(s16)arg1 * 64 / (otz + 1)`, which is black, to
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`, which takes the colour `rgb`.
static void func_dryfield_back_street_8017DC74(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
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
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Draws a glow of eight gouraud wedges around the coordinate's projected
/// position, when it projects. The radius is `(s16)arg1 * 64 / (otz + 1)`;
/// only the centre vertex takes the colour `rgb`, so each wedge fades to black.
static void func_dryfield_back_street_8017E0A0(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
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
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A beam between two anchors, the two entries of
/// `D_dryfield_back_street_8017F9A4` on the effect's parent frame (the second
/// also reached under its own name). State 0 allocates two eight-slot
/// coordinate trails and fills both with the anchors' frames; each later frame
/// records the anchors into the next slot and draws the trails with
/// `func_dryfield_back_street_8017E924`. The work block is released after
/// `spawnArg1` frames. Nothing runs once the room's event state reaches 2.
void func_dryfield_back_street_8017E434(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_dryfield_back_street_8017F9A4[0].vx;
                objCoord->coord.t[1]   = D_dryfield_back_street_8017F9A4[0].vy;
                objCoord->coord.t[2]   = D_dryfield_back_street_8017F9A4[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_dryfield_back_street_8017F9A4[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_dryfield_back_street_8017F9A4[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_back_street_8017E924(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade. Dropped when `gte_stflg` is negative.
static void func_dryfield_back_street_8017E924(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// A burst on an effect's anchor. It spawns effect 0x60076 and then either,
/// with a non-zero `spawnArg1`, a spray of randomly moving 0x60070 sparks for
/// seven frames, or two 0x6007C effects and a widening, fading double ring for
/// seven frames; then the work block is released. The task also ends when the
/// room's event state reaches 4, and does nothing while it is between 1 and 3.
void func_dryfield_back_street_8017ED1C(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_dryfield_back_street_8017DC74(objCoord, 0x100, 0x100, rgb);
            func_dryfield_back_street_8017DC74(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a flare around the coordinate's projected position, when it
/// projects: sixteen gouraud wedges lit only at the centre, eight at the
/// radius `(s16)arg1 * 64 / (otz + 1)` in half the colour `arg2` and eight at
/// half that radius in the full colour, then four rays at quarter turns that
/// alternate between the radius and twice it, their bases on the inner
/// radius `(s16)arg1 * 8 / (otz + 1)`.
static void func_dryfield_back_street_8017EFA4(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
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
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
