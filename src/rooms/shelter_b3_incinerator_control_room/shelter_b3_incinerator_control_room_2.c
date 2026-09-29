#include "rooms/shelter_b3_incinerator_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "shelter_b3_incinerator_control_room_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/task_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#define D_shelter_b3_incinerator_control_room_801818E8 (D_shelter_b3_incinerator_control_room_80181888 + 12)

/// Glow positions `func_shelter_b3_incinerator_control_room_8017FD10` draws
/// per view.
extern SVECTOR D_shelter_b3_incinerator_control_room_80181868[];

static void func_shelter_b3_incinerator_control_room_8017FEB4(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b3_incinerator_control_room_801806F8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b3_incinerator_control_room_80180B6C(SVECTOR* arg0, s32 arg1, s32 arg2);

SVECTOR D_shelter_b3_incinerator_control_room_80181868[4] = {
    { -1543, -2132, -2753, 0 },
    { -365, -2132, -2753, 0 },
    { -1543, -2042, -2677, 0 },
    { -365, -2042, -2677, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_shelter_b3_incinerator_control_room_80181888[19] = {
    { -3967, -562, -778, 0 },
    { -3967, -562, -70, 0 },
    { -3967, -562, 1570, 0 },
    { -3967, -562, 2278, 0 },
    { -3967, -680, -778, 0 },
    { -3967, -680, -70, 0 },
    { -3967, -680, 1570, 0 },
    { -3967, -680, 2278, 0 },
    { -7049, -562, 1570, 0 },
    { -7049, -562, 2278, 0 },
    { -7049, -680, 1570, 0 },
    { -7049, -680, 2278, 0 },
    { -7049, -562, -778, 0 },
    { -7049, -562, -70, 0 },
    { -7049, -680, -778, 0 },
    { -7049, -680, -70, 0 },
    { -5786, -2150, 3559, 0 },
    { -5228, -2150, 3559, 0 },
    { -4439, -1161, -1751, 0 },
};

u8* D_shelter_b3_incinerator_control_room_80181920[2] = {
    D_8010CAF8,
    D_8010CAF8,
};

GpViewCountRec D_shelter_b3_incinerator_control_room_80181928[2] = {
    { { .bytes = { 8, 0 } } },
    { { .bytes = { 8, 0 } } },
};

GpWarpRec D_shelter_b3_incinerator_control_room_8018192C[4] = {
    { { .words = { 2048, -5333, 0, 2984 } }, { 0, 0, 0, 0 }, { .words = { 2048, -5333, 0, 2984 } }, { 0, 0, 0, 0 }, 0x54290008, 0, 0, 5, 0, 0 },
    { { .words = { 3072, 820, 0, -2540 } }, { 0, 0, 0, 0 }, { .words = { 3072, 820, 0, -3100 } }, { 0, 0, 0, 0 }, 0, 0, 0, 3, 0, 447 },
    { { .words = { 0, -6048, 0, -3020 } }, { 0, 0, 0, 0 }, { .words = { 256, -5200, 0, -3020 } }, { 0, 0, 0, 0 }, 0x54290006, 0x54290005, 0x54290007, 4, 0, 459 },
    { { .words = { 2048, -5333, 0, 2984 } }, { 0, 0, 0, 0 }, { .words = { 2048, -5333, 0, 2984 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 0, 0 },
};

SVECTOR D_shelter_b3_incinerator_control_room_80181A0C[10] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_normals.inc"
};

SVECTOR D_shelter_b3_incinerator_control_room_80181A5C[27] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_verts.inc"
};

GpGridFace D_shelter_b3_incinerator_control_room_80181B34[19] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_faces.inc"
};

s16 D_shelter_b3_incinerator_control_room_80181C18[68] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b3_incinerator_control_room_80181C18[i])
s16* D_shelter_b3_incinerator_control_room_80181CA0[8] = {
#include "assets/shelter_b3_incinerator_control_room_collision_04700_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b3_incinerator_control_room_80181CC0 = { NULL, D_shelter_b3_incinerator_control_room_80181A0C, D_shelter_b3_incinerator_control_room_80181A5C, D_shelter_b3_incinerator_control_room_80181B34, D_shelter_b3_incinerator_control_room_80181CA0, 6742, 3500, 4, 2, 4000, 19 };

GpViewRec D_shelter_b3_incinerator_control_room_80181CE4[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x7530, 0 } }, 541 },
    { { { { 607, 0, 4050 }, { 1065, 3951, -159 }, { -3907, 1077, 586 } }, { -3319, 1397, 3178 } }, 212 },
    { { { { 607, 0, 4050 }, { 1065, 3951, -159 }, { -3907, 1077, 586 } }, { -3319, 1397, 3178 } }, 212 },
    { { { { -3911, 0, -1216 }, { -52, 4092, 170 }, { 1214, 178, -3907 } }, { 6542, 1367, -3406 } }, 230 },
    { { { { 3970, 0, -1006 }, { -3, 4095, -12 }, { 1006, 13, 3970 } }, { 6416, 1197, 2136 } }, 230 },
    { { { { -2634, 0, 3136 }, { 128, 4092, 108 }, { -3133, 168, -2632 } }, { 4213, 1411, -2685 } }, 498 },
    { { { { 4052, 0, -598 }, { -38, 4087, -263 }, { 597, 266, 4043 } }, { 6547, 1635, 2355 } }, 1104 },
    { { { { -1234, 0, -3905 }, { -938, 3975, 296 }, { 3791, 984, -1198 } }, { 5487, 1464, 1249 } }, 230 },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_80181E04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_80181E14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_incinerator_control_room_80181E24[13] = {
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -80, 1558, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -40, 1788, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -40, 1863, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -40, 1707, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -64, 1794, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -64, 1783, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -64, 1517, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -80, 1828, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -80, 1703, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -80, 1729, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 16, -80, 1725, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 64, -80, 1725, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 104, -80, 1725, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_80181F28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_incinerator_control_room_80181F40[18] = {
    { 143, 0x3FC0, { .fields = { 64, 72 } }, -160, -32, 1586, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, -32, 1586, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 24, 1586, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -32, 1586, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -16, 1339, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 8, 1344, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -96, -32, 1586, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, -32, 1258, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -16, 1333, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -16, 1249, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 1339, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 8, 1254, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 1310, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 24, 1256, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -24, 1399, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -40, -24, 1339, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 88, -64, 706, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 88, 16, 713, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_801820A8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 2, 0, 0, { 2, 0 } },
    { 16, 2, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b3_incinerator_control_room_801820D0[2] = {
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, 16, 753, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, -56, 753, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_801820F8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_80182110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_80182120[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b3_incinerator_control_room_80182130[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b3_incinerator_control_room_80182140[8] = {
    { { .empty = D_shelter_b3_incinerator_control_room_80181E04 }, D_shelter_b3_incinerator_control_room_80181E04, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80181E14 }, D_shelter_b3_incinerator_control_room_80181E14, NULL },
    { { .elements = D_shelter_b3_incinerator_control_room_80181E24 }, D_shelter_b3_incinerator_control_room_80181F28, NULL },
    { { .elements = D_shelter_b3_incinerator_control_room_80181F40 }, D_shelter_b3_incinerator_control_room_801820A8, NULL },
    { { .elements = D_shelter_b3_incinerator_control_room_801820D0 }, D_shelter_b3_incinerator_control_room_801820F8, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80182110 }, D_shelter_b3_incinerator_control_room_80182110, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80182120 }, D_shelter_b3_incinerator_control_room_80182120, NULL },
    { { .empty = D_shelter_b3_incinerator_control_room_80182130 }, D_shelter_b3_incinerator_control_room_80182130, NULL },
};

GpPointLight D_shelter_b3_incinerator_control_room_801821A0[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -591, -1844, -2881 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3295, -3263, 141 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3734, 3791, 3811, { 0, 0 } }, 1000, 0x2D03 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6412, -2764, -1784 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2370, 2309, 2426, { 0, 0 } }, 1000, 5940 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4412, -2882, 2029 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2128, 2126, 2128, { 0, 0 } }, 1000, 2477 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6584, 160, 1519 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2553, 2457, 2457, { 0, 0 } }, 1000, 1297 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4412, -321, 1461 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1000, 3981 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6679, 0, -121 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1000, 2100 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4352, 0, -121 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1000, 2100 },
};

GpRoomCoordSet D_shelter_b3_incinerator_control_room_801824A0 = { 0, NULL, 8, D_shelter_b3_incinerator_control_room_801821A0, 0, NULL };

GpObj4C D_shelter_b3_incinerator_control_room_801824B8[4] = {
    { NULL, NULL, NULL, { -5537, -1536, 687, 0 }, { { -2101, -2016, -224, 0 }, { 2099, -2016, 222, 0 }, { -2101, 2016, -224, 0 }, { 2099, 2016, 222, 0 } }, { 435, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -5456, -1536, 592, 0 }, { { 2067, -2016, 219, 0 }, { -2069, -2016, -221, 0 }, { 2067, 2016, 219, 0 }, { -2069, 2016, -221, 0 } }, { -435, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -4305, -1248, -3633, 0 }, { { -318, -2016, -2024, 0 }, { 315, -2016, 2021, 0 }, { -318, 2016, -2024, 0 }, { 315, 2016, 2021, 0 } }, { 4050, 0, -635, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -4145, -1280, -3648, 0 }, { { 298, -2016, 1911, 0 }, { -301, -2016, -1914, 0 }, { 298, 2016, 1911, 0 }, { -301, 2016, -1914, 0 } }, { -4053, 0, 633, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, 129, 0 },
};

GpAreaTmdRec D_shelter_b3_incinerator_control_room_801825E8[2] = {
    { 101, 426, 0, 0, { 0, 0 }, D_80135E24 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b3_incinerator_control_room_80182600[1] = {
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b3_incinerator_control_room_80182610[11] = {
    { NULL, NULL },
    { D_shelter_b3_incinerator_control_room_80182600, D_shelter_b3_incinerator_control_room_801825E8 },
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

GpObj4C D_shelter_b3_incinerator_control_room_80182668[5] = {
    { NULL, NULL, NULL, { 1201, -48, -2720, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, 257, 38, 64, 2, 0 },
    { NULL, NULL, NULL, { 1792, -304, -2704, 0 }, { { 0, -2560, -1328, 0 }, { 0, 2560, -1328, 0 }, { 0, -2560, 1328, 0 }, { 0, 2560, 1328, 0 } }, { -4099, 0, 0, 0 }, { -4096, 0, 0, 0 }, 2873, 0x8000, 43, 33, 2, 0 },
    { NULL, NULL, NULL, { -5903, -48, -3136, 0 }, { { 752, 0, -400, 0 }, { 752, 0, 400, 0 }, { -752, 0, -400, 0 }, { -752, 0, 400, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 851, 0, 42, 49, 2, 0 },
    { NULL, NULL, NULL, { -5440, -64, 3136, 0 }, { { -1424, 0, -384, 0 }, { 1424, 0, -384, 0 }, { -1424, 0, 384, 0 }, { 1424, 0, 384, 0 } }, { 0, 4113, 0, 0 }, { 0, 0, -4096, 0 }, 1470, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -4576, -64, -1584, 0 }, { { -400, 0, -752, 0 }, { 400, 0, -752, 0 }, { -400, 0, 752, 0 }, { 400, 0, 752, 0 } }, { 0, 4115, 0, 0 }, { -4096, 0, 0, 0 }, 851, 2, 1, 255, 130, 0 },
};

GpObj4C D_shelter_b3_incinerator_control_room_801827E4[6] = {
    { NULL, NULL, NULL, { 720, -48, -2720, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 1792, -304, -2704, 0 }, { { 0, -2560, -1328, 0 }, { 0, 2560, -1328, 0 }, { 0, -2560, 1328, 0 }, { 0, 2560, 1328, 0 } }, { -4099, 0, 0, 0 }, { -4096, 0, 0, 0 }, 2873, 0x8000, 43, 33, 2, 0 },
    { NULL, NULL, NULL, { -5600, -48, -3136, 0 }, { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1093, 0, 42, 49, 2, 0 },
    { NULL, NULL, NULL, { -4608, -64, -1504, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -5504, -64, 3168, 0 }, { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1093, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -4288, -64, -2080, 0 }, { { 832, 0, -1008, 0 }, { 832, 0, 1008, 0 }, { -832, 0, -1008, 0 }, { -832, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1305, 2, 1, 255, 132, 0 },
};

GpObj3A D_shelter_b3_incinerator_control_room_801829AC[1] = {
    { NULL, NULL, { -1312, -95, 368, 0 }, { { -2752, 4576, -2416, 0 }, { 2752, 4576, 2416, 0 }, { -2752, -4576, -2416, 0 }, { 2752, -4576, 2416, 0 } }, { -2705, 0, 3080, 0 }, { -28, 22 }, 129, 0 },
};

s32 D_shelter_b3_incinerator_control_room_801829E8[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

s32 D_shelter_b3_incinerator_control_room_801829F4[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

GpRoomParamRec D_shelter_b3_incinerator_control_room_80182A00[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b3_incinerator_control_room_80182A08[1] = {
    { 0, 0, 1, 0, D_shelter_b3_incinerator_control_room_801829E8 },
};

GpRoomParamRec D_shelter_b3_incinerator_control_room_80182A10[1] = {
    { 0, 0, 1, 0, D_shelter_b3_incinerator_control_room_801829F4 },
};

GpRoomParamRec D_shelter_b3_incinerator_control_room_80182A18[1] = {
    { 0, 0, 0, 0, D_shelter_b3_incinerator_control_room_801829E8 },
};

GpRoomParamRec* D_shelter_b3_incinerator_control_room_80182A20[8] = {
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A08,
    D_shelter_b3_incinerator_control_room_80182A10,
    D_shelter_b3_incinerator_control_room_80182A18,
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A00,
    D_shelter_b3_incinerator_control_room_80182A00,
};

GpAreaApplyRec D_shelter_b3_incinerator_control_room_80182A40[5] = {
    { 4, 39, 2, 0 },
    { 4, 40, 2, 0 },
    { 4, 43, 7, 33 },
    { 4, 44, 7, 33 },
    { 255, 0, 0, 0 },
};

Task* D_shelter_b3_incinerator_control_room_80182A54 = NULL;

RoomCutsceneRec D_shelter_b3_incinerator_control_room_80182A58;

/// Draws the glows of the current camera view at the room's fixed world
/// points; views without an entry draw nothing.
void func_shelter_b3_incinerator_control_room_8017FD10(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
        case 3:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181868[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181868[2], 0x180, 0x111);
            break;
        case 4:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[8], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[10], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_801806F8(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
        case 5:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[2], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[6], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[12], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[14], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[16], 0x180, 0x421);
            break;
        case 6:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_801818E8[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_801818E8[2], 0x180, 0x111);
            break;
        case 8:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_80180B6C(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
    }
}

/// Draws a glowing capsule between the world points `arg0` and `arg0 + 1`,
/// projected through `gGfxViewCoord.workm`. Each end is a disc of radius
/// `(s16)arg1 * 64` over its depth; for each 0x400 step across half a turn
/// from the screen-space angle between the ends, one gouraud `POLY_G4` wedge
/// is queued at each end and one band joins them. The lit vertices, at the
/// centres, take the colour packed in `arg2` (one nibble per channel),
/// flickering with the animation frame. Nothing is drawn when either
/// projection flags an error.
static void func_shelter_b3_incinerator_control_room_8017FEB4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
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
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
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

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
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
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1` scales
/// `gDisplayState.animFrame` into `rsin` so the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
static void func_shelter_b3_incinerator_control_room_801806F8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
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

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        block   = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
static void func_shelter_b3_incinerator_control_room_80180B6C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
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
            setRGB2(prim, 0, color, color);
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

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
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
            setRGB2(prim, 0, color, color);
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
    SCRATCH_POP_BYTES(0x14);
}
