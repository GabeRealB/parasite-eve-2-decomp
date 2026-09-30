#include "rooms/shelter_b3_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "shelter_b3_elevator_hall_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// Per-colour right shifts applied to the glowing disc's level for red, green
/// and blue, selected by the spawn argument.
extern RoomHaloShade D_shelter_b3_elevator_hall_80182B48[];

static void func_shelter_b3_elevator_hall_80181594(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b3_elevator_hall_80181818(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b3_elevator_hall_80181C3C(GfxCoord* arg0, s32 arg1, u8* rgb);
static void func_shelter_b3_elevator_hall_8018217C(GfxCoord* coord, s16 size);
static void func_shelter_b3_elevator_hall_801826A8(GfxCoord* arg0, s32 arg1);

extern TaskDesc D_80142604;

RoomHaloShade D_shelter_b3_elevator_hall_80182B48[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

u8* D_shelter_b3_elevator_hall_80182B54[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b3_elevator_hall_80182B58[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_shelter_b3_elevator_hall_80182B5C[3] = {
    { { .words = { 2048, -6047, 0, 1500 } }, { 0, 0, 0, 0 }, { .words = { 3328, -5000, 0, -200 } }, { 0, 0, 0, 0 }, 0x542A0004, 0x542A0003, 0x542A0005, 2, 0, 459 },
    { { .words = { 3072, 6749, 0, -300 } }, { 0, 0, 0, 0 }, { .words = { 2304, 6400, 0, 500 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 0, 0 },
    { { .words = { 3072, -4500, 0, 920 } }, { 0, 0, 0, 0 }, { .words = { 3072, -4500, 0, 920 } }, { 0, 0, 0, 0 }, 0x542A0002, 0, 0, 2, 0, 443 },
};

SVECTOR D_shelter_b3_elevator_hall_80182C04[9] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_normals.inc"
};

SVECTOR D_shelter_b3_elevator_hall_80182C4C[103] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_verts.inc"
};

GpGridFace D_shelter_b3_elevator_hall_80182F84[68] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_faces.inc"
};

s16 D_shelter_b3_elevator_hall_801832B4[236] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b3_elevator_hall_801832B4[i])
s16* D_shelter_b3_elevator_hall_8018348C[15] = {
#include "assets/shelter_b3_elevator_hall_collision_05F08_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b3_elevator_hall_801834C8 = { NULL, D_shelter_b3_elevator_hall_80182C04, D_shelter_b3_elevator_hall_80182C4C, D_shelter_b3_elevator_hall_80182F84, D_shelter_b3_elevator_hall_8018348C, 6978, 3041, 5, 3, 4000, 68 };

GpViewRec D_shelter_b3_elevator_hall_801834EC[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -1000, 0x7530, 0 } }, 447 },
    { { { { 3799, 0, -1531 }, { -696, 3648, -1726 }, { 1363, 1861, 3383 } }, { 6733, 2229, 2999 } }, 207 },
    { { { { 713, 0, 4033 }, { 233, 4089, -41 }, { -4026, 237, 712 } }, { -1171, 1416, 2568 } }, 230 },
    { { { { 867, 0, -4003 }, { -112, 4094, -24 }, { 4001, 114, 866 } }, { 4190, 1251, 2463 } }, 230 },
    { { { { 745, 0, -4027 }, { 54, 4095, 10 }, { 4027, -55, 745 } }, { -876, 1106, 2428 } }, 230 },
    { { { { 3923, 0, -1177 }, { -117, 4075, -392 }, { 1171, 409, 3903 } }, { -5282, 1071, 2998 } }, 207 },
    { { { { 1294, 0, -3885 }, { -479, 4064, -159 }, { 3856, 505, 1285 } }, { 6220, 1640, -296 } }, 257 },
};

SpriteBatch D_shelter_b3_elevator_hall_801835E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_elevator_hall_801835F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_elevator_hall_80183608[29] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -96, 1399, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -56, 1443, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -32, 1440, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -8, 1446, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 16, 1451, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 40, 1445, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 32, 1360, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 8, 1356, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 1306, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -40, 1339, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -64, 1331, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -96, 1322, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -96, 1252, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -64, 1287, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -40, 1260, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -16, 1264, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 8, 1268, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 1268, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 88, -96, 1250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 88, -16, 1250, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 40, -16, 1134, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 40, -96, 1202, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 64, -96, 829, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 64, -16, 917, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, 104, 40, 687, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 80, 40, 752, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 64, 40, 915, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 48, 40, 1051, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 48, 1125, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_8018384C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 1, 0 } },
    { 24, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_8018386C[2] = {
    { { 101, 12, 82, 183 }, 1408 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b3_elevator_hall_80183880[13] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 0, 0, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 2214, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -32, 2208, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 16, 2214, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -32, 8, 1443, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -32, -24, 1439, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -32, -56, 1403, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -8, 16, 1911, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -8, -16, 2022, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -8, -48, 2026, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, -40, 2202, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, -16, 2225, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, 8, 2316, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_80183984[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_801839A4[2] = {
    { { 139, 0, 180, 235 }, 2208 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b3_elevator_hall_801839B8[24] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 48, 1064, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, -8, 1070, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -56, 1072, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 56 } }, -136, 64, 666, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 64 } }, -136, 0, 703, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 56 } }, -136, -56, 754, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 64 } }, -136, -120, 740, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -96, 64, 751, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -96, 0, 789, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 56 } }, -96, -56, 790, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -96, -120, 791, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -72, -120, 910, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -72, -64, 908, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -72, -8, 907, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -72, 48, 890, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -56, -72, 986, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -56, -120, 996, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -40, -120, 1038, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -40, -72, 1002, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -40, -24, 1005, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -40, 32, 1063, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -32, 48, 1060, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -56, -16, 974, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -56, 32, 987, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_80183B98[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_80183BB8[2] = {
    { { 125, 25, 111, 173 }, 1055 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b3_elevator_hall_80183BCC[73] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 0, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 80, 72, 607, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 72, 658, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 64, 64, 664, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 64, 756, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 56, 767, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 48, 784, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 24, 40, 894, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 32, 997, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -8, 24, 1087, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 32, 1045, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 40, 963, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 48, 864, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 56, 833, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, -72, 768, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -40, 695, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -72, 706, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -72, 653, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -40, 657, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -16, 693, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 8, 697, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 32, 665, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 32, 708, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 8, 665, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -16, 662, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -40, 662, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -72, 646, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, 24, 625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 136, -40, 625, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -72, 625, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -72, 625, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 112, -40, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, 24, 625, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 625, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 24, 625, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 24, 650, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -16, 625, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -72, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -72, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, -24, 650, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 40, 704, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 48, 673, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 696, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 48, 640, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 56, 621, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 48, 653, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 56, 592, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 64, 566, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 56, 585, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 548, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 64, 536, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 56, 40, 672, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 56, 628, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -80, -24, 1025, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, -24, 1025, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -72, 0, 1025, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -24, -24, 1025, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -16, -24, 1037, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -16, 0, 1037, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 24, -16, 1050, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 16, 8, 1050, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 40, -8, 1050, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 56, 0, 1050, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 48, 24, 1050, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 40, 48 } }, -112, -104, 1250, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 48, 48 } }, -160, -104, 1250, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 40 } }, -80, -56, 1250, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 40 } }, -112, -56, 1250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 48, 40 } }, -160, -56, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 40, 40 } }, -112, -16, 1250, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 48, 40 } }, -160, -16, 1250, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 40, 32 } }, -112, 24, 1250, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 48, 32 } }, -160, 24, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_elevator_hall_80184180[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 4, 0 } },
    { 14, 26, 0, 0, { 3, 0 } },
    { 40, 13, 0, 0, { 2, 0 } },
    { 53, 11, 0, 0, { 1, 0 } },
    { 64, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b3_elevator_hall_801841B8[2] = {
    { { 0, 1, 226, 239 }, 690 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

SpriteBatch D_shelter_b3_elevator_hall_801841CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b3_elevator_hall_801841DC[7] = {
    { { .empty = D_shelter_b3_elevator_hall_801835E8 }, D_shelter_b3_elevator_hall_801835E8, NULL },
    { { .empty = D_shelter_b3_elevator_hall_801835F8 }, D_shelter_b3_elevator_hall_801835F8, NULL },
    { { .elements = D_shelter_b3_elevator_hall_80183608 }, D_shelter_b3_elevator_hall_8018384C, D_shelter_b3_elevator_hall_8018386C },
    { { .elements = D_shelter_b3_elevator_hall_80183880 }, D_shelter_b3_elevator_hall_80183984, D_shelter_b3_elevator_hall_801839A4 },
    { { .elements = D_shelter_b3_elevator_hall_801839B8 }, D_shelter_b3_elevator_hall_80183B98, D_shelter_b3_elevator_hall_80183BB8 },
    { { .elements = D_shelter_b3_elevator_hall_80183BCC }, D_shelter_b3_elevator_hall_80184180, D_shelter_b3_elevator_hall_801841B8 },
    { { .empty = D_shelter_b3_elevator_hall_801841CC }, D_shelter_b3_elevator_hall_801841CC, NULL },
};

GpPointLight D_shelter_b3_elevator_hall_80184230[5] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2088, -2723, -1583 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3528, 3488, 3508, { 0, 0 } }, 2000, 5702 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1644, -2723, -1583 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2000, 6201 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7160, -2723, -2914 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 2000, 7581 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5792, -2723, -1451 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2000, 7181 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3591, -2723, -1583 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 2000, 4000 },
};

GpRoomCoordSet D_shelter_b3_elevator_hall_80184410 = { 0, NULL, 5, D_shelter_b3_elevator_hall_80184230, 0, NULL };

GpObj4C D_shelter_b3_elevator_hall_80184428[8] = {
    { NULL, NULL, NULL, { 3519, 128, -2001, 0 }, { { 0, -4304, 2096, 0 }, { 0, -4304, -2096, 0 }, { 0, 4304, 2096, 0 }, { 0, 4304, -2096, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 3360, 0, -2161, 0 }, { { 0, -4304, -1664, 0 }, { 0, -4304, 1664, 0 }, { 0, 4304, -1664, 0 }, { 0, 4304, 1664, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1664, 0, -1888, 0 }, { { 0, -4304, -1664, 0 }, { 0, -4304, 1664, 0 }, { 0, 4304, -1664, 0 }, { 0, 4304, 1664, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1536, 0, -1856, 0 }, { { 0, -4304, 2096, 0 }, { 0, -4304, -2096, 0 }, { 0, 4304, 2096, 0 }, { 0, 4304, -2096, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -6176, 0, -1313, 0 }, { { 2304, -4304, 432, 0 }, { -2304, -4304, -432, 0 }, { 2304, 4304, 432, 0 }, { -2304, 4304, -432, 0 } }, { -758, 0, 4038, 0 }, { 0, 0, 4096, 0 }, 4884, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -6192, 0, -1104, 0 }, { { -2128, -4304, -432, 0 }, { 2128, -4304, 432, 0 }, { -2128, 4304, -432, 0 }, { 2128, 4304, 432, 0 } }, { 816, 0, -4025, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 6272, 0, -1088, 0 }, { { -2128, -4304, 464, 0 }, { 2128, -4304, -464, 0 }, { -2128, 4304, 464, 0 }, { 2128, 4304, -464, 0 } }, { -878, 0, -4025, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 6208, 0, -1216, 0 }, { { 2128, -4304, -464, 0 }, { -2128, -4304, 464, 0 }, { 2128, 4304, -464, 0 }, { -2128, 4304, 464, 0 } }, { 877, 0, 4024, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 6, 5, 129, 0 },
};

GpAreaTmdRec D_shelter_b3_elevator_hall_80184688[2] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b3_elevator_hall_801846A0[3] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b3_elevator_hall_801846C4[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_elevator_hall_801846DC[3] = {
    { 44, 0, 0, 3800, 0, -2000, 2400, 0, 0, 2, 0 },
    { 44, 0, 0, -1500, 0, -2300, 1750, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b3_elevator_hall_8018470C[4] = {
    { 24, 0, 0, -6500, 0, -800, 500, 0, 0, 2, 0 },
    { 24, 0, 0, 5500, 0, -500, 300, 0, 0, 2, 0 },
    { 49, 2, 0, 800, 0, -1500, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b3_elevator_hall_8018474C[3] = {
    { 3, 0, 0, -5000, 0, -2000, 1024, 0, 0, 2, 0 },
    { 3, 0, 1, 6000, 0, -2000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b3_elevator_hall_8018477C[12] = {
    { NULL, NULL },
    { D_shelter_b3_elevator_hall_801846DC, D_shelter_b3_elevator_hall_80184688 },
    { D_shelter_b3_elevator_hall_8018470C, D_shelter_b3_elevator_hall_801846A0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_elevator_hall_8018474C, D_shelter_b3_elevator_hall_801846C4 },
};

GpObj4C D_shelter_b3_elevator_hall_801847DC[4] = {
    { NULL, NULL, NULL, { -6096, 0, 1696, 0 }, { { -752, 0, -352, 0 }, { 752, 0, -352, 0 }, { -752, 0, 352, 0 }, { 752, 0, 352, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 829, 0, 41, 19, 2, 0 },
    { NULL, NULL, NULL, { 6848, -48, 176, 0 }, { { 176, 0, -624, 0 }, { 176, 0, 625, 0 }, { -176, 0, -624, 0 }, { -176, 0, 625, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 646, 257, 21, 64, 2, 0 },
    { NULL, NULL, NULL, { 7392, -128, 0, 0 }, { { 0, 2992, -1024, 0 }, { 0, 2992, 1024, 0 }, { 0, -2992, -1024, 0 }, { 0, -2992, 1024, 0 } }, { -4104, 0, 0, 0 }, { -4096, 0, 0, 0 }, 3156, 0x8000, 43, 35, 2, 0 },
    { NULL, NULL, NULL, { -4799, -48, 1136, 0 }, { { 432, 0, -720, 0 }, { 432, 0, 720, 0 }, { -432, 0, -720, 0 }, { -432, 0, 720, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 839, 0, 26, 49, 130, 0 },
};

GpObj3A D_shelter_b3_elevator_hall_8018490C[2] = {
    { NULL, NULL, { -2176, -688, 672, 0 }, { { -2208, -4272, -1632, 0 }, { 2208, -4272, 1632, 0 }, { -2208, 4272, -1632, 0 }, { 2208, 4272, 1632, 0 } }, { 2434, 0, -3295, 0 }, { -52, 19 }, 1, 0 },
    { NULL, NULL, { 2719, -608, 703, 0 }, { { -2419, -4272, 1651, 0 }, { 2420, -4272, -1650, 0 }, { -2419, 4272, 1651, 0 }, { 2420, 4272, -1650, 0 } }, { -2311, 0, -3388, 0 }, { 50, 20 }, 129, 0 },
};

s32 D_shelter_b3_elevator_hall_80184984[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

s32 D_shelter_b3_elevator_hall_80184990[3] = {
    0x10000041,
    0x10000043,
    0x10000055,
};

s32 D_shelter_b3_elevator_hall_8018499C[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849A8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849B0[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_80184984 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849B8[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849C0[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_8018499C },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849C8[1] = {
    { 0, 1, 0, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849D0[1] = {
    { 0, 0, 1, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec D_shelter_b3_elevator_hall_801849D8[1] = {
    { 0, 1, 0, 0, D_shelter_b3_elevator_hall_80184990 },
};

GpRoomParamRec* D_shelter_b3_elevator_hall_801849E0[8] = {
    D_shelter_b3_elevator_hall_801849A8,
    D_shelter_b3_elevator_hall_801849B0,
    D_shelter_b3_elevator_hall_801849B8,
    D_shelter_b3_elevator_hall_801849C0,
    D_shelter_b3_elevator_hall_801849C8,
    D_shelter_b3_elevator_hall_801849D0,
    D_shelter_b3_elevator_hall_801849D8,
    D_shelter_b3_elevator_hall_801849A8,
};

RoomEventMsg D_shelter_b3_elevator_hall_80184A00 = { 0 };

u8 D_shelter_b3_elevator_hall_80184A08[4] = {
    0,
    18,
    230,
    216,
};

RoomEventReq D_shelter_b3_elevator_hall_80184A0C = { 0 }; /// A glowing disc anchored to its parent at the work block's position. In
/// state 1 it grows, and every fourth tick spawns the effect `D_80115730`
/// names at a random joint of the player's model, adopting it as a child
/// task; state 2 adds a flickering half-bright wider disc; state 3 drifts the
/// disc away while it fades inside a widening ring, then releases the work
/// block. The spawn argument picks the colour. The task pauses while the
/// room's event state is set and releases its block when that reaches 4.
void func_shelter_b3_elevator_hall_80180E18(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
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
            col[0] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].b;
            func_shelter_b3_elevator_hall_80181C3C(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].b;
            func_shelter_b3_elevator_hall_80181C3C(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b3_elevator_hall_80181C3C(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b3_elevator_hall_80182B48[arg0->spawnArg1.value].b;
            func_shelter_b3_elevator_hall_80181C3C(coord, mem->angle, col);
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
            func_shelter_b3_elevator_hall_80181818(coord, (s16)(mem->period + 0x80), 0x100, col);
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

/// Moves the effect's coordinate toward the coordinate in the spawn argument.
/// State 0 takes their world displacement into the effect's own frame and
/// scales it by 0xCC/0x1000; state 1 applies that step every tick and draws a
/// sprite every other tick, advancing its frame. The work block is released
/// after 20 ticks, or when the room's event state reaches 4.
void func_shelter_b3_elevator_hall_80181370(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
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
                    func_shelter_b3_elevator_hall_80181594(coord, ++work->index, 0x200, 0x80);
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
static void func_shelter_b3_elevator_hall_80181594(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
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

/// Draws a gouraud ring of sixteen `POLY_G4` segments around the projected
/// origin of `arg0`'s world matrix, when it projects. The ring runs between
/// the radii `arg1` and `arg1 + arg2`, both scaled by depth; its edge at
/// `arg1 + arg2` takes the colour `rgb` and its edge at `arg1` is black.
static void func_shelter_b3_elevator_hall_80181818(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Draws a gouraud disc of eight `POLY_G4` wedges around the projected origin
/// of `arg0`'s world matrix, when it projects: radius `arg1` scaled by depth,
/// the colour `rgb` at the centre fading to black at the rim.
static void func_shelter_b3_elevator_hall_80181C3C(GfxCoord* arg0, s32 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

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
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
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

/// A burst: each tick it draws a disc growing with the effect's angle and the
/// glow at that size, and while its echo level lasts a widening ring fading
/// out around them. The work block is released once the disc's level runs
/// down, or when the room's event state reaches 4.
void func_shelter_b3_elevator_hall_80181FD0(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.coordBody->coord;
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
        func_shelter_b3_elevator_hall_80181C3C(coord, (s16)(step * 2), rgb);
        func_shelter_b3_elevator_hall_8018217C(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b3_elevator_hall_80181818(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
static void func_shelter_b3_elevator_hall_8018217C(GfxCoord* coord, s16 size)
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
            func_shelter_b3_elevator_hall_801826A8(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b3_elevator_hall_801826A8(GfxCoord* arg0, s32 arg1)
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
