#include "rooms/shelter_b1_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b1_control_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

#define D_shelter_b1_control_room_80181C3C (D_shelter_b1_control_room_80181BD4 + 13)

static void func_shelter_b1_control_room_8017F39C(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_control_room_8017FBE0(SVECTOR* arg0, s32 arg1, s32 arg2);

// Indexed views below share one contiguous table.
void func_shelter_b1_control_room_8017EF24(Task*);
void func_shelter_b1_control_room_8017F100(Task*);

extern SVECTOR D_shelter_b1_control_room_80181D20[7];
extern SVECTOR D_shelter_b1_control_room_80181D58[56];

TaskDesc D_shelter_b1_control_room_80181BBC[2] = {
    { 0, 192, func_shelter_b1_control_room_8017F100, { .model = NULL } },
    { 0, 192, func_shelter_b1_control_room_8017EF24, { .model = NULL } },
};

SVECTOR D_shelter_b1_control_room_80181BD4[18] = {
    { 8480, -3330, -2490, 0 },
    { 9080, -3330, -2490, 0 },
    { 9280, -3330, -2490, 0 },
    { 9880, -3330, -2490, 0 },
    { 10090, -3330, -2490, 0 },
    { 10680, -3330, -2490, 0 },
    { 8480, -3330, -6090, 0 },
    { 9080, -3330, -6090, 0 },
    { 9280, -3330, -6090, 0 },
    { 9880, -3330, -6090, 0 },
    { 10090, -3330, -6090, 0 },
    { 10680, -3330, -6090, 0 },
    { 10970, -3000, -3040, 0 },
    { 1180, -850, -2530, 0 },
    { 2180, -850, -2530, 0 },
    { 3180, -850, -2530, 0 },
    { 4180, -850, -2530, 0 },
    { 5180, -850, -2530, 0 },
};

s16 D_shelter_b1_control_room_80181C64[2][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

u8* D_shelter_b1_control_room_80181C70[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_control_room_80181C74[1] = {
    { { .bytes = { 8, 0 } } },
};

GpWarpRec D_shelter_b1_control_room_80181C78[3] = {
    { { .words = { 3072, 0x283C, -600, -3700 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x283C, -600, -3700 } }, { 0, 0, 0, 0 }, 0x54120002, 0x54120001, 0, 2, 0, 0 },
    { { .words = { 2048, 9120, -672, -3000 } }, { 0, 0, 0, 0 }, { .words = { 2048, 9120, -672, -3000 } }, { 0, 0, 0, 0 }, 0x54120004, 0x54120003, 0x54120005, 2, 0, 453 },
    { { .words = { 3072, 0x283C, -600, -3700 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x283C, -600, -3700 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_shelter_b1_control_room_80181D20[7] = {
#include "assets/shelter_b1_control_room_collision_04B38_normals.inc"
};

SVECTOR D_shelter_b1_control_room_80181D58[56] = {
#include "assets/shelter_b1_control_room_collision_04B38_verts.inc"
};

GpGridFace D_shelter_b1_control_room_80181F18[29] = {
#include "assets/shelter_b1_control_room_collision_04B38_faces.inc"
};

s16 D_shelter_b1_control_room_80182074[60] = {
#include "assets/shelter_b1_control_room_collision_04B38_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_control_room_80182074[i])
s16* D_shelter_b1_control_room_801820EC[3] = {
#include "assets/shelter_b1_control_room_collision_04B38_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_control_room_801820F8 = { NULL, D_shelter_b1_control_room_80181D20, D_shelter_b1_control_room_80181D58, D_shelter_b1_control_room_80181F18, D_shelter_b1_control_room_801820EC, 350, 6000, 3, 1, 4000, 29 };

GpViewRec D_shelter_b1_control_room_8018211C[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4900, 0x61A8, 4250 } }, 541 },
    { { { { 1218, 0, -3910 }, { 280, 4085, 87 }, { 3900, -294, 1214 } }, { -3730, 1370, 5270 } }, 257 },
    { { { { 1209, 0, -3913 }, { 41, 4095, 12 }, { 3912, -43, 1209 } }, { -50, 1330, 5390 } }, 257 },
    { { { { 3774, 0, 1590 }, { -11, 4095, 26 }, { -1590, -28, 3774 } }, { -5160, 2130, 4920 } }, 289 },
    { { { { 2189, 0, 3461 }, { -2880, 2271, 1822 }, { -1919, -3408, 1214 } }, { -1730, 20, 4840 } }, 329 },
    { { { { 1221, 0, 3909 }, { -29, 4095, 9 }, { -3909, -30, 1221 } }, { -6380, 1330, 5390 } }, 257 },
    { { { { 4095, 0, -35 }, { -6, 4017, -799 }, { 34, 799, 4016 } }, { -3280, 2730, 3840 } }, 289 },
    { { { { 4095, 0, -35 }, { -6, 4017, -799 }, { 34, 799, 4016 } }, { -3280, 2730, 3840 } }, 289 },
};

GpSprtCmd D_shelter_b1_control_room_8018223C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_control_room_8018224C[99] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 16, 598, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 56, 512, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -8, 667, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -8, 656, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -8, 644, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -8, 633, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, -8, 621, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 8, 623, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 8, 651, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 0, 682, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 0, 639, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 579, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 32, 551, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 40, 530, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 48, 511, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 684, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 681, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 48, 680, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 665, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 32, 648, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 32, 631, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 32, 616, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 40, 614, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 40, 646, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 56, 440, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 72, 439, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 438, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 56, 447, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 56, 455, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 463, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 56, 474, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 56, 481, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 500, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 582, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 72, 689, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 88, 669, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 72, 502, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 88, 501, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 499, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 104, 488, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 482, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 472, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 658, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 88, 635, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 462, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 104, 454, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 72, 625, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 625, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 104, 446, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 24, 813, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 0, 852, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 0, 829, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 0, 811, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 0, 794, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 758, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 846, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 24, 844, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 24, 823, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 814, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 707, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 694, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 24, 697, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 24, 656, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 32, 630, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 32, 739, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 40, 611, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 714, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 40, 639, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 40, 625, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 48, 786, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 64, 782, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 48, 684, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 625, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 56, 661, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 56, 625, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 625, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 72, 617, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 80, 606, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 613, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 48, 595, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 56, 592, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 72, 590, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 56, 849, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 88, 590, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 104, 585, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 72, 850, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 80, 598, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 80, 610, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 629, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, -120, 1277, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, -120, 1288, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, -120, 1351, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -96, 1318, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, -96, 1303, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -96, 1423, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -72, 1491, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, -72, 1337, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -72, 1483, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -56, 1457, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_control_room_80182A08[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 49, 0, 0, { 1, 0 } },
    { 49, 40, 0, 0, { 2, 0 } },
    { 89, 10, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_control_room_80182A30[43] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -8, 1561, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -8, 1515, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -8, 1542, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 8, 1335, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 1335, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 40, 1330, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 64, 1319, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 8, 1450, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 24, 1359, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -8, 1533, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 0, 1465, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 8, 1409, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 16, 1399, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 1399, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 8, 1450, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 24, 1379, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 1388, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1394, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -8, 1734, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1725, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 24, 1536, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 40, 1545, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 56, 1527, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -8, 1625, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 8, 1625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 24, 1507, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -8, 1625, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 0, 1555, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 8, 1500, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 16, 1543, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 1685, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 32, 1486, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 48, 1485, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -96, 2017, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -96, 2048, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 2186, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -64, 2306, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 2141, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -64, 2161, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -48, 2342, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -96, 2059, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -80, 2208, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -64, 2286, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_control_room_80182D8C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 15, 0, 0, { 2, 0 } },
    { 33, 10, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_control_room_80182DB4[29] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 56, 850, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -120, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -88, 500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -56, 500, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, -24, 500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 500, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 40, 500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 72, 500, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 64, 864, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 814, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 64, 823, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, 96, 804, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 533, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 96, 509, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 72, 533, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 96, 502, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 72, 533, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 96, 502, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 80, 500, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 96, 502, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 80, 500, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 500, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 80, 500, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 96, 500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, 88, 500, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 88, 500, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 88, 500, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 120, 96, 500, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_control_room_80182FF8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_control_room_80183010[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -16, 286, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -40, 350, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -32, 275, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -24, 269, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -24, 276, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -16, 281, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -24, 282, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -16, 255, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, -8, 253, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, -8, 259, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -8, 288, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 0, 242, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -56, 0, 265, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -16, 322, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 8, 342, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -72, 24, 245, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -32, 24, 201, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 8, 24, 201, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 215, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 32, 345, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 56, 396, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_control_room_801831B4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_control_room_801831CC[22] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -40, 1284, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -40, 1301, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -40, 1320, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, -16, 1350, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -16, 1369, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -8, 1364, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 0, 1337, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, 16, 1282, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 1284, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 16, 1318, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 32, 1288, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 56, 1307, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1276, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 16, 1295, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -16, 1337, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 1437, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 1435, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 16, 1433, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 32, 1277, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -8, 1362, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 40, 1431, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 56, 1386, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b1_control_room_80183384[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_control_room_8018339C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b1_control_room_801833AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_control_room_801833BC[8] = {
    { { .empty = D_shelter_b1_control_room_8018223C }, D_shelter_b1_control_room_8018223C, NULL },
    { { .elements = D_shelter_b1_control_room_8018224C }, D_shelter_b1_control_room_80182A08, NULL },
    { { .elements = D_shelter_b1_control_room_80182A30 }, D_shelter_b1_control_room_80182D8C, NULL },
    { { .elements = D_shelter_b1_control_room_80182DB4 }, D_shelter_b1_control_room_80182FF8, NULL },
    { { .elements = D_shelter_b1_control_room_80183010 }, D_shelter_b1_control_room_801831B4, NULL },
    { { .elements = D_shelter_b1_control_room_801831CC }, D_shelter_b1_control_room_80183384, NULL },
    { { .empty = D_shelter_b1_control_room_8018339C }, D_shelter_b1_control_room_8018339C, NULL },
    { { .empty = D_shelter_b1_control_room_801833AC }, D_shelter_b1_control_room_801833AC, NULL },
};

GpPointLight D_shelter_b1_control_room_8018341C[2] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2870, -650, -4290 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1802, 2621, 2375, { 0, 0 } }, 5000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9530, -2550, -4380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1228, 2129, 1966, { 0, 0 } }, 2000, 2750 },
};

GpRoomCoordSet D_shelter_b1_control_room_801834DC = { 0, NULL, 2, D_shelter_b1_control_room_8018341C, 0, NULL };

GpObj4C D_shelter_b1_control_room_801834F4[4] = {
    { NULL, NULL, NULL, { 6251, -1536, -4211, 0 }, { { -2, -2016, -2627, 0 }, { 2, -2016, 2628, 0 }, { -2, 2016, -2627, 0 }, { 2, 2016, 2628, 0 } }, { 4096, 0, -4, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 6331, -1504, -4308, 0 }, { { -5, -2016, 2589, 0 }, { 5, -2016, -2589, 0 }, { -5, 2016, 2589, 0 }, { 5, 2016, -2589, 0 } }, { -4099, 0, -9, 0 }, { 0, 0, 4096, 0 }, 3278, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2848, -1536, -4224, 0 }, { { 252, -2016, -2617, 0 }, { -258, -2016, 2612, 0 }, { 252, 2016, -2617, 0 }, { -258, 2016, 2612, 0 } }, { 4076, 0, 397, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 3, 6, 1, 0 },
    { NULL, NULL, NULL, { 2975, -1600, -4225, 0 }, { { -259, -2016, 2614, 0 }, { 258, -2016, -2616, 0 }, { -259, 2016, 2614, 0 }, { 258, 2016, -2616, 0 } }, { -4078, 0, -404, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 6, 3, 129, 0 },
};

GpObj4C D_shelter_b1_control_room_80183624[8] = {
    { NULL, NULL, NULL, { 0x28A0, -640, -3712, 0 }, { { -448, 0, -1024, 0 }, { 448, 0, -1024, 0 }, { -448, 0, 1024, 0 }, { 448, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1115, 0, 25, 18, 2, 0 },
    { NULL, NULL, NULL, { 9248, -640, -3168, 0 }, { { -928, 0, 448, 0 }, { -928, 0, -448, 0 }, { 928, 0, 448, 0 }, { 928, 0, -448, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 1024, 0, 19, 33, 2, 0 },
    { NULL, NULL, NULL, { 6720, -640, -4288, 0 }, { { -224, 0, -320, 0 }, { 224, 0, -320, 0 }, { -224, 0, 320, 0 }, { 224, 0, 320, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 390, 257, 35, 192, 2, 0 },
    { NULL, NULL, NULL, { 5760, -48, -4320, 0 }, { { -224, 0, -320, 0 }, { 224, 0, -320, 0 }, { -224, 0, 320, 0 }, { 224, 0, 320, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 390, 1, 35, 64, 2, 0 },
    { NULL, NULL, NULL, { 2624, -64, -2976, 0 }, { { -2176, 0, 320, 0 }, { -2176, 0, -320, 0 }, { 2176, 0, 320, 0 }, { 2176, 0, -320, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 2187, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 2560, -64, -5600, 0 }, { { -2176, 0, 320, 0 }, { -2176, 0, -320, 0 }, { 2176, 0, 320, 0 }, { 2176, 0, -320, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, 4096, 0 }, 2187, 2, 5, 255, 2, 0 },
    { NULL, NULL, NULL, { 1504, -64, -4336, 0 }, { { -448, 0, -368, 0 }, { 448, 0, -368, 0 }, { -448, 0, 368, 0 }, { 448, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 579, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -4288, 0 }, { { -320, 0, -1696, 0 }, { 320, 0, -1696, 0 }, { -320, 0, 1696, 0 }, { 320, 0, 1696, 0 } }, { 0, 4112, 0, 0 }, { 4096, 0, 0, 0 }, 1722, 2, 9, 255, 130, 0 },
};

GpAreaTmdRec D_shelter_b1_control_room_80183884[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 24, 24, 1, 0, { 0, 0 }, D_8014E47C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_801838A8[2] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_801838C0[2] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_801838D8[2] = {
    { 34, 504, 0, 0, { 0, 0 }, D_8013C8F4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_801838F0[2] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b1_control_room_80183908[8] = {
    { 21, 3, 0, -350, -3000, -3800, 1024, 0, 255, 2, 1 },
    { 21, 3, 0, -350, -3000, -4800, 1024, 0, 255, 2, 1 },
    { 21, 1, 0, -350, -2200, -3000, 1024, 0, 255, 2, 1 },
    { 21, 1, 0, -350, -2200, -5600, 1024, 0, 255, 2, 1 },
    { 24, 0, 0, 3500, 0, -3400, 2000, 0, 0, 4, 0 },
    { 24, 0, 0, 3400, 0, -5100, 2450, 0, 0, 4, 0 },
    { 24, 0, 0, 2800, 0, -4300, 1024, 0, 0, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_80183988[4] = {
    { 21, 0, 0, -600, -2000, -3500, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, -600, -2000, -5000, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, 0x2A94, -2300, -4800, 3072, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_801839C8[4] = {
    { 21, 0, 0, -600, -2000, -3500, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, -600, -2000, -5000, 1024, 0, 0, 2, 0 },
    { 21, 0, 0, 0x2A94, -2300, -4800, 3072, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_80183A08[2] = {
    { 34, 0, 0, 4258, 0, 2700, 0, 0, 4, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b1_control_room_80183A28[7] = {
    { 21, 2, 0, -350, -3000, -3800, 1024, 0, 0, 2, 4 },
    { 21, 2, 0, -350, -3000, -4800, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -2900, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -5700, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -3600, 1024, 0, 0, 2, 4 },
    { 21, 1, 0, -350, -2200, -5000, 1024, 0, 0, 2, 4 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_control_room_80183A98[22] = {
    { NULL, NULL },
    { D_shelter_b1_control_room_80183908, D_shelter_b1_control_room_80183884 },
    { D_shelter_b1_control_room_80183988, D_shelter_b1_control_room_801838A8 },
    { D_shelter_b1_control_room_801839C8, D_shelter_b1_control_room_801838C0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_control_room_80183A08, D_shelter_b1_control_room_801838D8 },
    { NULL, NULL },
    { D_shelter_b1_control_room_80183A28, D_shelter_b1_control_room_801838F0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

GpRoomBoundVec D_shelter_b1_control_room_80183B48[9] = {
    { 8, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 582, 925, 780, 778 },
    { 579, 901, 799, 767 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 614, 899, 793, 778 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_shelter_b1_control_room_80183B90[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

s32 D_shelter_b1_control_room_80183B9C[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

GpRoomParamRec D_shelter_b1_control_room_80183BA8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_control_room_80183BB0[1] = {
    { 0, 0, 1, 0, D_shelter_b1_control_room_80183B90 },
};

GpRoomParamRec D_shelter_b1_control_room_80183BB8[1] = {
    { 0, 0, 1, 0, D_shelter_b1_control_room_80183B9C },
};

GpRoomParamRec* D_shelter_b1_control_room_80183BC0[8] = {
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BB0,
    D_shelter_b1_control_room_80183BB8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
    D_shelter_b1_control_room_80183BA8,
};

GpAreaApplyRec D_shelter_b1_control_room_80183BE0[2] = {
    { 4, 18, 12, 0 },
    { 255, 0, 0, 0 },
};

/// Streamed-scene task. It blanks the display and queues CD command 0x61 on
/// the stream slot of the current location with its view replaced by 0x64,
/// then shows the display once the command queue signals it. The scene runs
/// until the CD is idle or the pad check aborts it, which is recorded in
/// `spawnArg1`. After the stream state is restored an aborted scene kills the
/// task at once, and a finished one after 0x3D more ticks; either way the
/// display heap is reset.
void func_shelter_b1_control_room_8017EF24(Task* arg0)
{
    u8          slotParam[4];
    s32         state;
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot(key.raw.data, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        state                 = task->state;
        task->spawnArg1.value = 0;
        task->state           = state + 1;
        return;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    task->spawnArg1.value = 1;
    task->state           = task->state + 1;
    return;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
    goto advance;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1.value != 0) {
        goto kill;
    }
advance:
    task->state = task->state + 1;
    return;

L_case6:
    task->killCountdown = task->killCountdown + 1;
    if (task->killCountdown < 0x3D) {
        return;
    }
kill:
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_shelter_b1_control_room_8017F100(Task* arg0)
{
    Display_SpawnWithOt(D_shelter_b1_control_room_80181BBC, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_shelter_b1_control_room_8017F150(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_80115734  = 0x60276;
        D_80115730  = 0x60277;
        D_80115754  = 0x60278;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[0], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[2], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[4], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[6], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[8], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[10], 0x100, 0x243);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181BD4[12], 0x180, 0x421);
            break;
        case 3:
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[0], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[2], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[4], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[6], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[8], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&D_shelter_b1_control_room_80181BD4[10], 0x100, 0x243);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181BD4[12], 0x180, 0x421);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181BD4[14], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181BD4[15], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181BD4[16], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181BD4[17], 0x200, 0x23);
            break;
        case 4:
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[0], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[1], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[2], 0x200, 0x23);
            break;
        case 6:
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[0], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[1], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[2], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&D_shelter_b1_control_room_80181C3C[3], 0x200, 0x23);
            break;
    }
}

/// Draws a glowing bar between the world points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn when either
/// projection flags an error. Each end gets a half-disc of gouraud wedges of
/// radius `(s16)arg1 * 64` over its depth, joined by quads across the bar. The
/// lit vertices take the colour packed in `arg2`, one nibble per channel in
/// the high nibble, with bit 3 following the animation frame.
static void func_shelter_b1_control_room_8017F39C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      side;
    u8                       r;
    u8                       g;
    u8                       b;

    p1    = arg0 + 1;
    block = SCRATCH_PUSH(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    side           = angStart + (ang - angStart) * 2;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(side)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(side)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(side)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(side)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP(OverlayPointPairScratch);
}

/// Draws a glowing disc at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`, unless the projection flags an error: four gouraud
/// wedges of radius `(s16)arg1 * 64` over the depth, black at the rim. The
/// centre takes the colour packed in `arg2`, one nibble per channel in the
/// high nibble, with bit 3 following the animation frame.
static void func_shelter_b1_control_room_8017FBE0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / block->otz;
        ang           = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}
