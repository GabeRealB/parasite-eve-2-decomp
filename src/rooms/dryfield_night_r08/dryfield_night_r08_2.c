#include "rooms/dryfield_night_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"

#define D_dryfield_night_r08_801805BC (D_dryfield_night_r08_801805AC + 2)
#define D_dryfield_night_r08_801805CC (D_dryfield_night_r08_801805AC + 4)
#define D_dryfield_night_r08_801805DC (D_dryfield_night_r08_801805AC + 6)
#define D_dryfield_night_r08_80180664 (D_dryfield_night_r08_801805AC + 23)
#define D_dryfield_night_r08_8018066C (D_dryfield_night_r08_801805AC + 24)

extern SVECTOR D_dryfield_night_r08_8018056C[];
/// The two offsets from the parent object the beam trail task starts from:
/// `[0]` places the object and `[1]`, also reached under its own name
/// `D_dryfield_night_r08_80180674`, seeds the second ring.

static void func_dryfield_night_r08_8017DB4C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_dryfield_night_r08_8017E334(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_r08_8017E854(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_dryfield_night_r08_8017EC80(GpCoord* arg0, s16 arg1, u8* rgb);
static void func_dryfield_night_r08_8017F504(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
static void func_dryfield_night_r08_8017FB84(GpCoord* arg0, s16 arg1, u8* arg2);

extern GpGridParams   D_dryfield_night_r08_80181474[1];
extern GpRoomCoordSet D_dryfield_night_r08_8018189C[1];

SVECTOR D_dryfield_night_r08_8018056C[8] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_dryfield_night_r08_801805AC[25] = {
    { -3246, -2265, -10632, 0 },
    { -3246, -2265, -9370, 0 },
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
    { 3260, -2265, -10632, 0 },
    { 3260, -2265, -9370, 0 },
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -13478, 0 },
    { -1293, -4768, -11488, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -13478, 0 },
    { 1293, -4768, -11488, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
    { 0, 190, -15, 0 },
};

SVECTOR D_dryfield_night_r08_80180674 = { 0, 1085, 180, 0 };

GpRoomCoordRec D_dryfield_night_r08_8018067C[1] = {
    { D_dryfield_night_r08_8018189C, NULL },
};

GpRoomObjRec D_dryfield_night_r08_80180684[1] = {
    { D_dryfield_night_r08_80181474, NULL, NULL, NULL },
};

u8 * D_dryfield_night_r08_80180694[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_r08_80180698[1] = {
    { { .bytes = { 9, 0 } } },
};

GpWarpRec D_dryfield_night_r08_8018069C[1] = {
    { { .words = { 0, 0, 0, -0x4650 } }, { 0, 0, 0, 0 }, { .words = { 0, 0, 0, -0x4650 } }, { 0, 0, 0, 0 }, 0, 0, 0, 1, 0, 0 },
};

SVECTOR D_dryfield_night_r08_801806D4[34] = {
    { 4096, 8, 27, 0 },
    { 0, 0, 4096, 0 },
    { -4096, 51, 26, 0 },
    { 0, 4096, 0, 0 },
    { 3634, 1890, 0, 0 },
    { 4041, -670, 0, 0 },
    { 0, -3953, -1072, 0 },
    { -3634, 1890, 0, 0 },
    { -4048, -623, 0, 0 },
    { 3634, 1890, 0, 0 },
    { 3040, 0, -2745, 0 },
    { -3634, 1890, 0, 0 },
    { 3634, 1890, 0, 0 },
    { 4041, -670, 0, 0 },
    { 0, 0, -4096, 0 },
    { -2767, 6, -3020, 0 },
    { -3634, 1890, 0, 0 },
    { 3634, 1890, 0, 0 },
    { 4041, -670, 0, 0 },
    { -4048, -623, 0, 0 },
    { -4096, 0, 0, 0 },
    { 0, 4024, 762, 0 },
    { -3634, 1890, 0, 0 },
    { 0, 4096, 0, 0 },
    { 4096, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 3366, 0, -2334, 0 },
    { 2758, 0, 3028, 0 },
    { -2385, 0, 3330, 0 },
    { -2980, 0, -2811, 0 },
    { 3077, 0, -2703, 0 },
    { -3066, 0, 2716, 0 },
    { 3159, 0, 2607, 0 },
    { -3072, 0, -2709, 0 },
};

SVECTOR D_dryfield_night_r08_801807E4[172] = {
    { -884, -2148, -0x3C9A, 0 },
    { -888, 154, -0x3C9A, 0 },
    { -862, 154, -0x4C1B, 0 },
    { -834, -2147, -0x4C1B, 0 },
    { 852, 154, -0x4C1B, 0 },
    { 823, -2147, -0x4C1B, 0 },
    { 877, 154, -0x3C9A, 0 },
    { 873, -2148, -0x3C9A, 0 },
    { 818, 1302, -0x3C68, 0 },
    { 818, 140, -0x3C68, 0 },
    { -838, 140, -0x3C68, 0 },
    { -838, 1302, -0x3C68, 0 },
    { -3663, -2215, 753, 0 },
    { -3663, -2215, -907, 0 },
    { -2500, -4450, -907, 0 },
    { -2500, -4450, 753, 0 },
    { -3246, 297, 753, 0 },
    { -3246, 297, -907, 0 },
    { 4227, 1302, -0x3C68, 0 },
    { 4227, 140, -0x3C68, 0 },
    { 2500, -4450, 753, 0 },
    { 2500, -4450, -907, 0 },
    { 3663, -2215, -907, 0 },
    { 3663, -2215, 753, 0 },
    { 3284, 247, -907, 0 },
    { 3284, 247, 753, 0 },
    { -4205, -2157, -0x3C68, 0 },
    { -4205, 140, -0x3C68, 0 },
    { -838, -2157, -0x3C68, 0 },
    { -3663, -2215, -3067, 0 },
    { -2500, -4450, -3067, 0 },
    { -4205, 1302, -0x3C68, 0 },
    { -838, -5619, -0x3C68, 0 },
    { 818, -2157, -0x3C68, 0 },
    { 818, -5619, -0x3C68, 0 },
    { 2500, -4450, -3067, 0 },
    { 3663, -2215, -3067, 0 },
    { 4227, -2157, -0x3C68, 0 },
    { 4227, -5619, -0x3C68, 0 },
    { -4205, -5619, -0x3C68, 0 },
    { -3663, -2215, -8919, 0 },
    { -2500, -4450, -8919, 0 },
    { -3246, 297, -3067, 0 },
    { -3246, 297, -8919, 0 },
    { -4205, -5619, 543, 0 },
    { 4227, -5619, 543, 0 },
    { 4227, 1302, 543, 0 },
    { -4205, 1302, 543, 0 },
    { 2500, -4450, -8919, 0 },
    { 3663, -2215, -8919, 0 },
    { 3284, 247, -8919, 0 },
    { 3284, 247, -3067, 0 },
    { 3983, -2244, -908, 0 },
    { 3983, 183, -908, 0 },
    { 2801, 183, -908, 0 },
    { 3322, -2244, -908, 0 },
    { -2500, -4450, -0x3D90, 0 },
    { -2500, -4450, -0x2B48, 0 },
    { -3663, -2215, -0x2B48, 0 },
    { -3663, -2215, -0x3D90, 0 },
    { -3246, 297, -0x2B48, 0 },
    { -3246, 297, -0x3D90, 0 },
    { 3284, 247, -0x3D90, 0 },
    { 3284, 247, -0x2B48, 0 },
    { 3663, -2215, -0x2B48, 0 },
    { 3663, -2215, -0x3D90, 0 },
    { 3983, -2244, -3072, 0 },
    { 3983, 183, -3072, 0 },
    { 3322, -2244, -3072, 0 },
    { 2801, 183, -3072, 0 },
    { 2500, -4450, -0x2B48, 0 },
    { 2500, -4450, -0x3D90, 0 },
    { 3983, -2244, -8905, 0 },
    { 3322, -2244, -8905, 0 },
    { 3322, -2244, -0x2B57, 0 },
    { 3983, -2244, -0x2B57, 0 },
    { 2801, 183, -0x2B57, 0 },
    { 3983, 183, -0x2B57, 0 },
    { 3983, 183, -8905, 0 },
    { 2801, 183, -8905, 0 },
    { -3322, -2244, -8905, 0 },
    { -3983, -2244, -8905, 0 },
    { -3983, -2244, -0x2B57, 0 },
    { -3322, -2244, -0x2B57, 0 },
    { -2801, 183, -0x2B57, 0 },
    { -3983, 183, -0x2B57, 0 },
    { -3983, 183, -8905, 0 },
    { -2801, 183, -8905, 0 },
    { -3322, -2244, -908, 0 },
    { -3983, -2244, -908, 0 },
    { -3983, -2244, -3072, 0 },
    { -3322, -2244, -3072, 0 },
    { -2801, 183, -3072, 0 },
    { -3983, 183, -3072, 0 },
    { -3983, 183, -908, 0 },
    { -2801, 183, -908, 0 },
    { -5173, 0, -873, 0 },
    { -730, 0, -873, 0 },
    { -730, 0, -3116, 0 },
    { -5173, 0, -3116, 0 },
    { -5398, 0, -8876, 0 },
    { -730, 0, -8876, 0 },
    { -730, 0, -0x2B53, 0 },
    { -5398, 0, -0x2B53, 0 },
    { -730, 0, 800, 0 },
    { 709, 0, 800, 0 },
    { 709, 0, -873, 0 },
    { 709, 0, -8876, 0 },
    { 4936, 0, -8876, 0 },
    { 4936, 0, -0x2B53, 0 },
    { 709, 0, -0x2B53, 0 },
    { 709, 0, -3116, 0 },
    { 4936, 0, -873, 0 },
    { 4936, 0, -3116, 0 },
    { 709, 0, -0x4D03, 0 },
    { -730, 0, -0x4D03, 0 },
    { -770, 0, -1122, 0 },
    { -5173, 0, -1122, 0 },
    { -5173, -1175, -1122, 0 },
    { -770, -1175, -1122, 0 },
    { -770, -1175, -2866, 0 },
    { -5173, -1175, -2866, 0 },
    { -5173, 0, -2866, 0 },
    { -770, 0, -2866, 0 },
    { -606, -1175, -885, 0 },
    { -606, 0, -885, 0 },
    { -606, -1175, -3016, 0 },
    { -606, 0, -3016, 0 },
    { 553, -1175, -885, 0 },
    { 553, 0, -885, 0 },
    { 553, 0, 481, 0 },
    { 553, -1175, 481, 0 },
    { -606, 0, 481, 0 },
    { -606, -1175, 481, 0 },
    { 762, -1175, -2866, 0 },
    { 553, -1175, -3016, 0 },
    { 553, 0, -3016, 0 },
    { 762, 0, -2866, 0 },
    { 4936, 0, -2866, 0 },
    { 4936, -1175, -2866, 0 },
    { 762, -1175, -1122, 0 },
    { 4936, -1175, -1122, 0 },
    { 4936, 0, -1122, 0 },
    { 762, 0, -1122, 0 },
    { -797, 0, -9125, 0 },
    { -5398, 0, -9125, 0 },
    { -5398, -1175, -9125, 0 },
    { -797, -1175, -9125, 0 },
    { -797, -1175, -0x2A5A, 0 },
    { -5398, -1175, -0x2A5A, 0 },
    { -5398, 0, -0x2A5A, 0 },
    { -797, 0, -0x2A5A, 0 },
    { 759, -1175, -9125, 0 },
    { 4936, -1175, -9125, 0 },
    { 4936, 0, -9125, 0 },
    { 759, 0, -9125, 0 },
    { 553, 0, -8907, 0 },
    { 553, -1175, -8907, 0 },
    { -606, -1175, -8907, 0 },
    { -606, 0, -8907, 0 },
    { 759, 0, -0x2A5A, 0 },
    { 4936, 0, -0x2A5A, 0 },
    { 4936, -1175, -0x2A5A, 0 },
    { 759, -1175, -0x2A5A, 0 },
    { 553, 0, -0x2B42, 0 },
    { 553, -1175, -0x2B42, 0 },
    { -606, -1175, -0x2B42, 0 },
    { -606, 0, -0x2B42, 0 },
    { 553, -1175, -0x4B84, 0 },
    { 553, 0, -0x4B84, 0 },
    { -606, -1175, -0x4B84, 0 },
    { -606, 0, -0x4B84, 0 },
};

GpGridFace D_dryfield_night_r08_80180D44[82] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 0, 3, 7, 5 }, 3, 0 },
    { { 9, 10, 8, 11 }, 1, 0 },
    { { 13, 14, 12, 15 }, 4, 0 },
    { { 17, 13, 16, 12 }, 5, 0 },
    { { 19, 9, 18, 8 }, 1, 0 },
    { { 1, 10, 6, 9 }, 6, 0 },
    { { 21, 22, 20, 23 }, 7, 0 },
    { { 22, 24, 23, 25 }, 8, 0 },
    { { 27, 10, 26, 28 }, 1, 0 },
    { { 29, 30, 13, 14 }, 9, 0 },
    { { 10, 27, 11, 31 }, 1, 0 },
    { { 28, 33, 32, 34 }, 1, 0 },
    { { 28, 10, 0, 1 }, 10, 0 },
    { { 35, 36, 21, 22 }, 11, 0 },
    { { 33, 37, 34, 38 }, 1, 0 },
    { { 26, 28, 39, 32 }, 1, 0 },
    { { 40, 41, 29, 30 }, 12, 0 },
    { { 43, 40, 42, 29 }, 13, 0 },
    { { 45, 46, 44, 47 }, 14, 0 },
    { { 7, 6, 33, 9 }, 15, 0 },
    { { 48, 49, 35, 36 }, 16, 0 },
    { { 49, 50, 36, 51 }, 8, 0 },
    { { 53, 54, 52, 55 }, 14, 0 },
    { { 57, 58, 56, 59 }, 17, 0 },
    { { 58, 57, 40, 41 }, 12, 0 },
    { { 58, 60, 59, 61 }, 18, 0 },
    { { 63, 64, 62, 65 }, 19, 0 },
    { { 67, 53, 66, 52 }, 20, 0 },
    { { 69, 67, 68, 66 }, 1, 0 },
    { { 28, 0, 33, 7 }, 21, 0 },
    { { 9, 19, 33, 37 }, 1, 0 },
    { { 64, 70, 65, 71 }, 22, 0 },
    { { 70, 64, 48, 49 }, 16, 0 },
    { { 73, 74, 72, 75 }, 23, 0 },
    { { 76, 77, 74, 75 }, 1, 0 },
    { { 77, 78, 75, 72 }, 20, 0 },
    { { 78, 79, 72, 73 }, 14, 0 },
    { { 81, 82, 80, 83 }, 23, 0 },
    { { 83, 82, 84, 85 }, 1, 0 },
    { { 82, 81, 85, 86 }, 24, 0 },
    { { 81, 80, 86, 87 }, 14, 0 },
    { { 89, 90, 88, 91 }, 23, 0 },
    { { 91, 90, 92, 93 }, 1, 0 },
    { { 90, 89, 93, 94 }, 24, 0 },
    { { 89, 88, 94, 95 }, 14, 0 },
    { { 55, 68, 52, 66 }, 23, 0 },
    { { 97, 98, 96, 99 }, 25, 2 },
    { { 101, 102, 100, 103 }, 25, 2 },
    { { 105, 106, 104, 97 }, 25, 2 },
    { { 108, 109, 107, 110 }, 25, 2 },
    { { 106, 111, 97, 98 }, 25, 2 },
    { { 112, 113, 106, 111 }, 25, 2 },
    { { 111, 107, 98, 101 }, 25, 2 },
    { { 107, 110, 101, 102 }, 25, 2 },
    { { 115, 102, 114, 110 }, 25, 2 },
    { { 117, 118, 116, 119 }, 14, 1 },
    { { 121, 122, 120, 123 }, 1, 1 },
    { { 125, 116, 124, 119 }, 26, 1 },
    { { 120, 123, 126, 127 }, 27, 1 },
    { { 129, 130, 128, 131 }, 20, 1 },
    { { 130, 132, 131, 133 }, 14, 1 },
    { { 135, 136, 134, 137 }, 28, 1 },
    { { 138, 139, 137, 134 }, 1, 1 },
    { { 141, 142, 140, 143 }, 14, 1 },
    { { 145, 146, 144, 147 }, 14, 1 },
    { { 149, 150, 148, 151 }, 1, 1 },
    { { 153, 154, 152, 155 }, 14, 1 },
    { { 157, 152, 156, 155 }, 29, 1 },
    { { 159, 144, 158, 147 }, 30, 1 },
    { { 161, 162, 160, 163 }, 1, 1 },
    { { 160, 163, 164, 165 }, 31, 1 },
    { { 148, 151, 166, 167 }, 32, 1 },
    { { 168, 169, 165, 164 }, 20, 1 },
    { { 171, 169, 170, 168 }, 1, 1 },
    { { 171, 170, 167, 166 }, 24, 1 },
    { { 156, 136, 157, 135 }, 20, 1 },
    { { 158, 126, 159, 127 }, 24, 1 },
    { { 129, 128, 143, 140 }, 33, 1 },
    { { 124, 133, 125, 132 }, 24, 1 },
};

s16 D_dryfield_night_r08_8018111C[18] = {
    0,
    1,
    3,
    4,
    8,
    11,
    13,
    14,
    15,
    18,
    26,
    28,
    32,
    57,
    75,
    76,
    77,
    -1,
};

s16 D_dryfield_night_r08_80181140[24] = {
    0,
    3,
    4,
    8,
    11,
    13,
    14,
    15,
    18,
    26,
    27,
    28,
    32,
    40,
    41,
    42,
    50,
    56,
    57,
    68,
    74,
    75,
    77,
    -1,
};

s16 D_dryfield_night_r08_80181170[23] = {
    19,
    20,
    26,
    27,
    28,
    40,
    41,
    42,
    43,
    50,
    52,
    55,
    56,
    57,
    67,
    68,
    70,
    71,
    74,
    77,
    78,
    79,
    -1,
};

s16 D_dryfield_night_r08_801811A0[20] = {
    12,
    19,
    20,
    27,
    40,
    42,
    43,
    44,
    45,
    46,
    49,
    50,
    53,
    55,
    59,
    61,
    67,
    78,
    79,
    -1,
};

s16 D_dryfield_night_r08_801811C8[25] = {
    5,
    6,
    12,
    19,
    20,
    21,
    44,
    45,
    46,
    47,
    49,
    51,
    53,
    54,
    55,
    58,
    59,
    60,
    61,
    62,
    63,
    79,
    80,
    81,
    -1,
};

s16 D_dryfield_night_r08_801811FC[13] = {
    5,
    6,
    12,
    21,
    44,
    46,
    47,
    49,
    51,
    58,
    63,
    81,
    -1,
};

s16 D_dryfield_night_r08_80181218[24] = {
    0,
    1,
    2,
    3,
    4,
    7,
    8,
    11,
    13,
    14,
    15,
    17,
    18,
    22,
    26,
    29,
    32,
    33,
    34,
    57,
    75,
    76,
    77,
    -1,
};

s16 D_dryfield_night_r08_80181248[34] = {
    0,
    2,
    3,
    4,
    7,
    8,
    11,
    13,
    14,
    15,
    17,
    18,
    22,
    26,
    27,
    28,
    29,
    32,
    33,
    34,
    35,
    36,
    37,
    50,
    52,
    56,
    57,
    68,
    72,
    73,
    74,
    75,
    77,
    -1,
};

s16 D_dryfield_night_r08_8018128C[35] = {
    19,
    20,
    23,
    24,
    26,
    27,
    28,
    29,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    43,
    50,
    52,
    55,
    56,
    57,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    77,
    78,
    79,
    -1,
};

s16 D_dryfield_night_r08_801812D4[28] = {
    12,
    16,
    19,
    20,
    23,
    24,
    31,
    35,
    39,
    48,
    49,
    50,
    52,
    53,
    54,
    55,
    56,
    59,
    61,
    64,
    65,
    67,
    69,
    70,
    71,
    78,
    79,
    -1,
};

s16 D_dryfield_night_r08_8018130C[37] = {
    5,
    6,
    9,
    10,
    12,
    16,
    19,
    20,
    21,
    23,
    24,
    25,
    30,
    31,
    44,
    45,
    47,
    48,
    49,
    51,
    53,
    54,
    55,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    78,
    79,
    80,
    81,
    -1,
};

s16 D_dryfield_night_r08_80181358[18] = {
    5,
    9,
    10,
    16,
    21,
    25,
    49,
    51,
    53,
    54,
    58,
    60,
    62,
    63,
    66,
    80,
    81,
    -1,
};

s16 D_dryfield_night_r08_8018137C[10] = {
    2,
    3,
    7,
    17,
    29,
    33,
    34,
    57,
    75,
    -1,
};

s16 D_dryfield_night_r08_80181390[14] = {
    7,
    17,
    29,
    33,
    34,
    35,
    36,
    37,
    38,
    52,
    57,
    72,
    75,
    -1,
};

s16 D_dryfield_night_r08_801813AC[17] = {
    23,
    24,
    29,
    34,
    35,
    36,
    37,
    38,
    39,
    52,
    55,
    56,
    69,
    70,
    72,
    73,
    -1,
};

s16 D_dryfield_night_r08_801813D0[17] = {
    16,
    23,
    24,
    30,
    31,
    35,
    36,
    38,
    39,
    48,
    52,
    54,
    55,
    65,
    69,
    78,
    -1,
};

s16 D_dryfield_night_r08_801813F4[18] = {
    9,
    10,
    16,
    21,
    23,
    24,
    25,
    30,
    31,
    48,
    51,
    53,
    54,
    64,
    65,
    66,
    80,
    -1,
};

s16 D_dryfield_night_r08_80181418[10] = {
    9,
    10,
    16,
    21,
    25,
    30,
    48,
    54,
    66,
    -1,
};

s16 * D_dryfield_night_r08_8018142C[18] = {
    D_dryfield_night_r08_8018111C,
    D_dryfield_night_r08_80181140,
    D_dryfield_night_r08_80181170,
    D_dryfield_night_r08_801811A0,
    D_dryfield_night_r08_801811C8,
    D_dryfield_night_r08_801811FC,
    D_dryfield_night_r08_80181218,
    D_dryfield_night_r08_80181248,
    D_dryfield_night_r08_8018128C,
    D_dryfield_night_r08_801812D4,
    D_dryfield_night_r08_8018130C,
    D_dryfield_night_r08_80181358,
    D_dryfield_night_r08_8018137C,
    D_dryfield_night_r08_80181390,
    D_dryfield_night_r08_801813AC,
    D_dryfield_night_r08_801813D0,
    D_dryfield_night_r08_801813F4,
    D_dryfield_night_r08_80181418,
};

GpGridParams D_dryfield_night_r08_80181474[1] = {
    { NULL, D_dryfield_night_r08_801806D4, D_dryfield_night_r08_801807E4, D_dryfield_night_r08_80180D44, D_dryfield_night_r08_8018142C, 5398, 0x4D03, 3, 6, 4000, 82 },
};

GpViewRec D_dryfield_night_r08_80181498[9] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -4086, 0, 279 }, { 19, 4085, 285 }, { -278, 286, -4076 } }, { -92, 1285, 0x2769 } }, 380 },
    { { { { 4017, 0, -798 }, { -146, 4026, -738 }, { 784, 752, 3949 } }, { 853, 1783, 0x391E } }, 148 },
    { { { { 4026, 0, -753 }, { -13, 4095, -74 }, { 753, 75, 4025 } }, { 199, 1212, 0x30DF } }, 282 },
    { { { { -4095, 0, 2 }, { 0, 4095, 4 }, { -2, 4, -4095 } }, { 4, 1285, 8562 } }, 251 },
    { { { { 4071, 0, -448 }, { 66, 4050, 606 }, { 443, -609, 4026 } }, { 1561, 288, 0x38BC } }, 257 },
    { { { { -833, 0, 4010 }, { 882, 3995, 183 }, { -3912, 901, -812 } }, { -3391, 1940, 9236 } }, 246 },
    { { { { 3753, 0, -1640 }, { 59, 4093, 135 }, { 1639, -148, 3750 } }, { 717, 966, 0x3CDD } }, 257 },
    { { { { 3903, 0, -1241 }, { -46, 4093, -147 }, { 1240, 154, 3900 } }, { 530, 1231, 0x2C30 } }, 329 },
};

GpSprtCmd D_dryfield_night_r08_801815DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_801815EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_801815FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_8018160C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_8018161C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_r08_8018162C[9] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -96, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -160, 0, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -136, 48, 500, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -80, 48, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -24, 48, 500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 48, 450, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, 56, 412, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, 56, 375, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_r08_801816E0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_801816F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_80181708[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_r08_80181718[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_r08_80181728[9] = {
    { { .empty = D_dryfield_night_r08_801815DC }, D_dryfield_night_r08_801815DC, NULL },
    { { .empty = D_dryfield_night_r08_801815EC }, D_dryfield_night_r08_801815EC, NULL },
    { { .empty = D_dryfield_night_r08_801815FC }, D_dryfield_night_r08_801815FC, NULL },
    { { .empty = D_dryfield_night_r08_8018160C }, D_dryfield_night_r08_8018160C, NULL },
    { { .empty = D_dryfield_night_r08_8018161C }, D_dryfield_night_r08_8018161C, NULL },
    { { .elements = D_dryfield_night_r08_8018162C }, D_dryfield_night_r08_801816E0, NULL },
    { { .empty = D_dryfield_night_r08_801816F8 }, D_dryfield_night_r08_801816F8, NULL },
    { { .empty = D_dryfield_night_r08_80181708 }, D_dryfield_night_r08_80181708, NULL },
    { { .empty = D_dryfield_night_r08_80181718 }, D_dryfield_night_r08_80181718, NULL },
};

GpLight D_dryfield_night_r08_80181794[3] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2582, 402, 6341 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 5324, 5324, 5324, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CC5, -2046, -6920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 8192, 8192, 8192, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2899, 1476, -7757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 6144, 6144, 6144, { 0, 0 } },
};

GpRoomCoordSet D_dryfield_night_r08_8018189C[1] = {
    { 3, D_dryfield_night_r08_80181794, 0, NULL, 0, NULL },
};

GpAreaTmdRec D_dryfield_night_r08_801818B4[3] = {
    { 132, 357, 4, 0, { 0, 0 }, D_8013DADC },
    { 20, 358, 4, 0, { 0, 0 }, D_80146810 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_r08_801818D8[11] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B508, D_dryfield_night_r08_801818B4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_night_r08_80181930[3] = {
    0x10000045,
    0x10000047,
    0x10000045,
};

GpRoomParamRec D_dryfield_night_r08_8018193C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_r08_80181944[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_r08_8018194C[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_night_r08_80181954[1] = {
    { 0, 0, 1, 0, D_dryfield_night_r08_80181930 },
};

GpRoomParamRec * D_dryfield_night_r08_8018195C[8] = {
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_80181944,
    D_dryfield_night_r08_8018194C,
    D_dryfield_night_r08_80181954,
    D_dryfield_night_r08_8018193C,
};

/// On the task's first tick, stores three fixed ids into `D_80115758`,
/// `D_8011572C` and `D_80115750`, then draws the placements the current camera
/// view shows with the beam and sprite drawers below. The placement names are
/// windows onto one run of 8-byte `SVECTOR`s, so `80180664` is `805BC[21]`,
/// `805AC[23]` and `805CC[19]` as well; views 3 and 9 name it directly and the
/// compiler merges their last two calls into one tail.
void func_dryfield_night_r08_8017D718(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601C7;
        D_8011572C  = 0x601E3;
        D_80115750  = 0x601FF;
        arg0->state = 1;
    }

    switch (gGameSession->at4.loc.view) {
        case 3:
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[0], 0x200, 0x800, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[2], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[4], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[6], 0x200, 0x800, 0x100);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[11], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[12], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[13], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[14], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[18], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[19], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[20], 1, 0x300);
            func_dryfield_night_r08_8017E334(D_dryfield_night_r08_80180664, 1, 0x300);
            break;
        case 2:
        case 5:
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_8018056C[0], 0x200, 0x800, 0x111);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_8018056C[2], 0x200, 0x800, 0x111);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_8018056C[4], 0x200, 0, 0x111);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_8018056C[6], 0x200, 0, 0x111);
            break;
        case 4:
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[0], 0x200, 0x800, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[4], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805BC[6], 0x200, 0x800, 0x100);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[14], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[20], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805BC[21], 1, 0x300);
            break;
        case 6:
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805AC[0], 0x200, 0x800, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805AC[2], 0x200, 0x800, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805AC[6], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805AC[8], 0x200, 0x800, 0x100);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[13], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[14], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[15], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[16], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[20], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[21], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[22], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805AC[23], 1, 0x300);
            break;
        case 7:
            func_dryfield_night_r08_8017DB4C(D_dryfield_night_r08_801805AC, 0x200, 0x800, 0x10);
            break;
        case 8:
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805CC[0], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805CC[2], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805CC[4], 0x200, 0x800, 0x100);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[9], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[10], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[11], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[12], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[16], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[17], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[18], 1, 0x300);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805CC[19], 1, 0x300);
            break;
        case 9:
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805DC[0], 0x200, 0, 0x10);
            func_dryfield_night_r08_8017DB4C(&D_dryfield_night_r08_801805DC[2], 0x200, 0x800, 0x100);
            func_dryfield_night_r08_8017E334(&D_dryfield_night_r08_801805DC[10], 1, 0x300);
            func_dryfield_night_r08_8017E334(D_dryfield_night_r08_80180664, 1, 0x300);
            break;
    }
}

/// Draws a flickering light beam from `arg0[0]` to `arg0[1]`. Both points are
/// projected through the view matrix; unless the far end is nearer than OTZ
/// 0x11, gouraud `POLY_G4` wedges around each end (radius `(s16)arg1 * 64 /
/// otz` at that end) are joined by quads between the two, each fading from the
/// beam colour on the axis to black at the rim. `arg2` turns the wedges about
/// the axis. `arg3` packs the colour: the red factor in bits 8-15 and the
/// green and blue factors in bits 4 and 0, each multiplying an intensity that
/// alternates between 0x20 and 0x28 with the display frame counter.
static void func_dryfield_night_r08_8017DB4C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects the world point `arg0` through `gGfxViewCoord.workm` and, when its OTZ
/// is above 0x10, queues one semi-transparent `POLY_FT4` sprite centred on it:
/// tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`, and the 40-texel-wide UV cell
/// `(s16)arg1` selects. `(s16)arg2` is the half-extent; the on-screen radius is
/// `arg2 * 39 / otz`. All three colour channels take the flickering
/// `((animFrame & 1) * 16) + 0x20`.
static void func_dryfield_night_r08_8017E334(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    s32                u;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw25Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz > 0x10) {
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u           = idx * 40;
        setUV4(prim, u, 0, u + 39, 0, u, 39, u + 39, 39);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// Flash effect task on the object's coordinate. While `eventState` is non-zero
/// it draws nothing, releasing its work block once that reaches 4. Otherwise,
/// over `spawnArg1` frames it brightens a pink tint (full red, half blue,
/// quarter green) while growing two fanned glows and a ring; at the peak it
/// draws a fade quad in that tint, then dims a star glow by 0x10 a frame until
/// it has faded and releases itself.
void func_dryfield_night_r08_8017E5B0(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
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
                func_dryfield_night_r08_8017EC80(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_dryfield_night_r08_8017EC80(coord, (s16)((u16)work->angle * 2), rgb);
                func_dryfield_night_r08_8017E854(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_dryfield_night_r08_8017FB84(coord, (s16)(work->angle * 3), rgb);
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
static void func_dryfield_night_r08_8017E854(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues eight `POLY_G4` wedges fanned around the
/// projected centre with radius `(s16)arg1 * 64 / (otz + 1)`. Only the centre
/// vertex takes the `rgb` tint, so each wedge fades to black at the rim.
static void func_dryfield_night_r08_8017EC80(GpCoord* arg0, s16 arg1, u8* rgb)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Beam trail task. On its first frame it allocates two eight-slot coordinate
/// rings, places the object at the first of the two offsets
/// `D_dryfield_night_r08_8018066C` gives from the parent and fills the
/// rings with that point and the second offset. Each later frame it writes the
/// current pair into the next slot and draws the trail between the rings,
/// releasing itself once its age reaches `spawnArg1`. Nothing runs while
/// `eventState` is 2 or more.
void func_dryfield_night_r08_8017F014(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_dryfield_night_r08_8018066C[0].vx;
                objCoord->coord.t[1] = D_dryfield_night_r08_8018066C[0].vy;
                objCoord->coord.t[2] = D_dryfield_night_r08_8018066C[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_dryfield_night_r08_8018066C[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_dryfield_night_r08_80180674.vx;
                coord.coord.t[1] = D_dryfield_night_r08_80180674.vy;
                coord.coord.t[2] = D_dryfield_night_r08_80180674.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_night_r08_8017F504(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the trail between two eight-slot coordinate rings as seven `POLY_G4`
/// quads, walking back from slot `arg2`. Each quad joins two adjacent slots of
/// `arg0` and `arg1`, with brightness falling from `0x40 - 9 * i` at its
/// leading edge by nine more at its trailing one. `arg3` is the colour, three
/// 2-bit channel weights at bits 8, 4 and 0. A quad the GTE flags is dropped.
static void func_dryfield_night_r08_8017F504(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Burst effect task on the object's coordinate. While `eventState` is non-zero
/// it draws nothing, releasing its work block once that reaches 4. Its first
/// frame spawns effect 0x60076 and then either a spark (0x60070), if
/// `spawnArg1` is set, or two 0x6007C effects. The spark branch then emits one
/// randomly-directed spark a frame; the other draws a fixed ring and an
/// expanding one in a fading orange tint. Either ends once its age reaches 7.
void func_dryfield_night_r08_8017F8FC(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
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
            func_dryfield_night_r08_8017E854(objCoord, 0x100, 0x100, rgb);
            func_dryfield_night_r08_8017E854(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues a star-shaped glow of `POLY_G4` wedges
/// around the projected centre. With `r = arg1 * 64 / (otz + 1)`, a disc of
/// radius `r` at half the `rgb` tint and one of radius `r / 2` at full tint are
/// followed by four spikes, two reaching `r` and two `2 * r`, whose bases sit on
/// the radius `arg1 * 8 / (otz + 1)`. Only the centre vertex is tinted, so
/// every wedge fades to black.
static void func_dryfield_night_r08_8017FB84(GpCoord* arg0, s16 arg1, u8* arg2)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
