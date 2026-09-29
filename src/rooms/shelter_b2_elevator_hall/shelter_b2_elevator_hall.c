#include "common.h"
#include "mapui/map_shelter.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017dcb8.h"
#include "rooms/shelter_b2_elevator_hall.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/room_effects.h"
#include "gameplay/loading.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/scene.h"
#include "gameplay/world_state.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "overlay.h"

#include "gameplay/collision.h"
#include "gameplay/direction_input.h"
#include "gameplay/room.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

#include "actors/task_tables.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b2_elevator_hall_80184D84[4];

extern TaskDesc D_shelter_b2_elevator_hall_8018379C;
extern SVECTOR  D_shelter_b2_elevator_hall_801837D8[];
extern SVECTOR  D_shelter_b2_elevator_hall_801837F8[];
extern SVECTOR  D_shelter_b2_elevator_hall_80183808[];
extern SVECTOR  D_shelter_b2_elevator_hall_80183868[];
extern SVECTOR  D_shelter_b2_elevator_hall_801838A8[];
extern SVECTOR  D_shelter_b2_elevator_hall_801838B0[];

static void func_shelter_b2_elevator_hall_8017DCBC(Task* task);
static void func_shelter_b2_elevator_hall_8017DD00(Task* task);
static void func_shelter_b2_elevator_hall_8017DFB8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_elevator_hall_8017E7FC(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_elevator_hall_8017F4A4(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_shelter_b2_elevator_hall_80180464(GpCoord* coord, s16 size);
static void func_shelter_b2_elevator_hall_80180990(GpCoord* arg0, s32 arg1);
static void func_shelter_b2_elevator_hall_80180D08(GpCoord* arg0, s16 arg1, u8* arg2);
static void func_shelter_b2_elevator_hall_80181AA0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b2_elevator_hall_80181ECC(GpCoord* arg0, s16 arg1, u8* arg2);
static void func_shelter_b2_elevator_hall_80182750(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b2_elevator_hall_80182DD0(GpCoord* arg0, s16 arg1, u8* arg2);

void func_shelter_b2_elevator_hall_8017D774(Task *);
void func_shelter_b2_elevator_hall_8017D8E4(Task *);
s32 func_shelter_b2_elevator_hall_8017DAD4(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_shelter_b2_elevator_hall_8017DC70(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b2_elevator_hall_8017DC78(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b2_elevator_hall_8017DC80(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_b2_elevator_hall_8017DC88(Task *, s32, s32, GpMessageArg);

TaskDesc D_shelter_b2_elevator_hall_80183790 = { 0, 32, func_shelter_b2_elevator_hall_8017D774, { .model = NULL } };

TaskDesc D_shelter_b2_elevator_hall_8018379C = { 0, 32, func_shelter_b2_elevator_hall_8017D8E4, { .model = NULL } };

GpMsgEntry D_shelter_b2_elevator_hall_801837A8[6] = {
    { 5102, func_shelter_b2_elevator_hall_8017DAD4 },
    { 5105, func_shelter_b2_elevator_hall_8017DC70 },
    { 5103, func_shelter_b2_elevator_hall_8017DC80 },
    { 5104, func_shelter_b2_elevator_hall_8017DC78 },
    { 5106, func_shelter_b2_elevator_hall_8017DC88 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_b2_elevator_hall_801837D8[4] = {
    { -8291, -620, -1755, 0 },
    { -7626, -620, -1755, 0 },
    { -5333, -620, -1755, 0 },
    { -4665, -620, -1755, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801837F8[2] = {
    { -1331, -620, -1755, 0 },
    { -663, -620, -1755, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_80183808[12] = {
    { 1669, -620, -1755, 0 },
    { 2328, -620, -1755, 0 },
    { 5664, -620, -1755, 0 },
    { 6336, -620, -1755, 0 },
    { 8672, -620, -1755, 0 },
    { 9336, -620, -1755, 0 },
    { -1331, -620, 1757, 0 },
    { -663, -620, 1757, 0 },
    { 1669, -620, 1757, 0 },
    { 2328, -620, 1757, 0 },
    { -9760, -620, -235, 0 },
    { -9760, -620, 368, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_80183868[8] = {
    { 0x280E, -620, 2564, 0 },
    { 0x280E, -620, 3170, 0 },
    { 0x2749, -2342, 3789, 0 },
    { 0x2749, -2342, 4279, 0 },
    { 0x2735, -2342, -255, 0 },
    { 0x2735, -2342, -744, 0 },
    { -5622, -2399, 1652, 0 },
    { -5249, -2399, 1652, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801838A8[1] = {
    { 9936, -1250, 3234, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801838B0[1] = {
    { 9901, -1195, 3234, 0 },
};

RoomHaloShade D_shelter_b2_elevator_hall_801838B8[3] = {
    { 0, 1, 2 },
    { 2, 1, 0 },
    { 0, 2, 1 },
};

SVECTOR D_shelter_b2_elevator_hall_801838CC[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

u8 * D_shelter_b2_elevator_hall_801838DC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b2_elevator_hall_801838E0[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_shelter_b2_elevator_hall_801838E4[3] = {
    { { .words = { 2048, -5465, 0, 1040 } }, { 0, 0, 0, 0 }, { .words = { 2048, -5465, 0, 1040 } }, { 0, 0, 0, 0 }, 0x541B0006, 0x541B0005, 0, 2, 0, 455 },
    { { .words = { 3072, 9400, 0, -600 } }, { 0, 0, 0, 0 }, { .words = { 3072, 9400, 0, -600 } }, { 0, 0, 0, 0 }, 0x541B0002, 0x541B0001, 0, 5, 0, 443 },
    { { .words = { 3072, 9484, 0, 3743 } }, { 0, 0, 0, 0 }, { .words = { 3072, 9484, 0, 3743 } }, { 0, 0, 0, 0 }, 0x541B0004, 0x541B0003, 0x541B0008, 6, 0, 457 },
};

SVECTOR D_shelter_b2_elevator_hall_8018398C[7] = {
    { 0, 4096, 0, 0 },
    { 4096, 0, 0, 0 },
    { -4096, 0, 0, 0 },
    { 3483, 0, -2155, 0 },
    { 0, 0, 4096, 0 },
    { 0, 0, -4096, 0 },
    { 0, -4096, 0, 0 },
};

SVECTOR D_shelter_b2_elevator_hall_801839C4[48] = {
    { 0x28B5, -2852, 5252, 0 },
    { -9350, -2852, 5252, 0 },
    { -9350, -2852, -1481, 0 },
    { 0x28B5, -2852, -1481, 0 },
    { -9139, -2873, -1382, 0 },
    { -9139, -2873, 1364, 0 },
    { -9139, 0, 1364, 0 },
    { -9139, 0, -1382, 0 },
    { 9910, -2873, 4918, 0 },
    { 9910, -2873, -1382, 0 },
    { 9910, 0, -1382, 0 },
    { 9910, 0, 4918, 0 },
    { 4648, -2873, 1535, 0 },
    { 4648, 0, 1535, 0 },
    { 4542, 0, 1364, 0 },
    { 4542, -2873, 1364, 0 },
    { 4648, -2873, 4918, 0 },
    { 4648, 0, 4918, 0 },
    { 6752, 1175, 4119, 0 },
    { 6752, -825, 4119, 0 },
    { 6752, -825, 6119, 0 },
    { 6752, 1175, 6119, 0 },
    { 4610, 1175, 4119, 0 },
    { 4610, -825, 4119, 0 },
    { 9910, 0, 1364, 0 },
    { 9096, 0, 1364, 0 },
    { 9096, 0, 4918, 0 },
    { 7867, 0, 4918, 0 },
    { 7867, 0, 1364, 0 },
    { 4481, 0, 4918, 0 },
    { 4481, 0, 1364, 0 },
    { -6038, 0, 1364, 0 },
    { -6038, 0, 684, 0 },
    { -9139, 0, 684, 0 },
    { -6038, 0, -951, 0 },
    { -9139, 0, -222, 0 },
    { -6038, 0, -1382, 0 },
    { 4481, 0, 684, 0 },
    { 4648, 0, -951, 0 },
    { 4648, 0, -1382, 0 },
    { 7867, 0, 679, 0 },
    { 9096, 0, 673, 0 },
    { 9910, 0, 614, 0 },
    { 7867, 0, -950, 0 },
    { 9096, 0, -950, 0 },
    { 9910, 0, -947, 0 },
    { 7867, 0, -1382, 0 },
    { 9096, 0, -1382, 0 },
};

GpGridFace D_shelter_b2_elevator_hall_80183B44[28] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 5, 6, 4, 7 }, 1, 0 },
    { { 9, 10, 8, 11 }, 2, 0 },
    { { 13, 14, 12, 15 }, 3, 0 },
    { { 4, 7, 9, 10 }, 4, 0 },
    { { 15, 14, 5, 6 }, 5, 0 },
    { { 8, 11, 16, 17 }, 5, 0 },
    { { 19, 20, 18, 21 }, 1, 0 },
    { { 23, 19, 22, 18 }, 5, 0 },
    { { 12, 16, 13, 17 }, 1, 0 },
    { { 25, 26, 24, 11 }, 6, 1 },
    { { 26, 25, 27, 28 }, 6, 2 },
    { { 27, 28, 29, 30 }, 6, 1 },
    { { 32, 33, 31, 6 }, 6, 1 },
    { { 34, 35, 32, 33 }, 6, 1 },
    { { 35, 34, 7, 36 }, 6, 1 },
    { { 37, 32, 30, 31 }, 6, 1 },
    { { 38, 34, 37, 32 }, 6, 2 },
    { { 34, 38, 36, 39 }, 6, 1 },
    { { 28, 40, 30, 37 }, 6, 1 },
    { { 25, 41, 28, 40 }, 6, 2 },
    { { 41, 25, 42, 24 }, 6, 1 },
    { { 40, 43, 37, 38 }, 6, 1 },
    { { 41, 44, 40, 43 }, 6, 2 },
    { { 44, 41, 45, 42 }, 6, 1 },
    { { 43, 46, 38, 39 }, 6, 1 },
    { { 47, 44, 10, 45 }, 6, 1 },
    { { 44, 47, 43, 46 }, 6, 2 },
};

s16 D_shelter_b2_elevator_hall_80183C94[11] = {
    0,
    1,
    4,
    5,
    13,
    14,
    15,
    16,
    17,
    18,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183CAC[8] = {
    0,
    1,
    5,
    13,
    14,
    16,
    17,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183CBC[10] = {
    0,
    4,
    5,
    13,
    14,
    15,
    16,
    17,
    18,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183CD0[5] = {
    0,
    5,
    16,
    17,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183CDC[11] = {
    0,
    3,
    4,
    5,
    12,
    16,
    17,
    18,
    19,
    22,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183CF4[9] = {
    0,
    5,
    6,
    8,
    9,
    12,
    16,
    17,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183D08[18] = {
    0,
    3,
    4,
    5,
    8,
    9,
    11,
    12,
    16,
    17,
    18,
    19,
    20,
    22,
    23,
    25,
    27,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183D2C[14] = {
    0,
    3,
    5,
    6,
    7,
    8,
    9,
    11,
    12,
    16,
    17,
    19,
    22,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183D48[18] = {
    0,
    2,
    4,
    7,
    8,
    10,
    11,
    12,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    -1,
};

s16 D_shelter_b2_elevator_hall_80183D6C[16] = {
    0,
    2,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    19,
    20,
    21,
    22,
    23,
    24,
    -1,
};

s16 * D_shelter_b2_elevator_hall_80183D8C[10] = {
    D_shelter_b2_elevator_hall_80183C94,
    D_shelter_b2_elevator_hall_80183CAC,
    D_shelter_b2_elevator_hall_80183CBC,
    D_shelter_b2_elevator_hall_80183CD0,
    D_shelter_b2_elevator_hall_80183CDC,
    D_shelter_b2_elevator_hall_80183CF4,
    D_shelter_b2_elevator_hall_80183D08,
    D_shelter_b2_elevator_hall_80183D2C,
    D_shelter_b2_elevator_hall_80183D48,
    D_shelter_b2_elevator_hall_80183D6C,
};

GpGridParams D_shelter_b2_elevator_hall_80183DB4 = { NULL, D_shelter_b2_elevator_hall_8018398C, D_shelter_b2_elevator_hall_801839C4, D_shelter_b2_elevator_hall_80183B44, D_shelter_b2_elevator_hall_80183D8C, 9350, 1481, 5, 2, 4000, 28 };

GpViewRec D_shelter_b2_elevator_hall_80183DD8[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -20, 0x7530, -890 } }, 358 },
    { { { { 1273, 0, 3893 }, { 27, 4095, -8 }, { -3893, 28, 1273 } }, { 2681, 1099, 981 } }, 230 },
    { { { { 1056, 0, 3957 }, { 303, 4083, -81 }, { -3945, 314, 1053 } }, { -2887, 1466, 974 } }, 230 },
    { { { { 1008, 0, -3969 }, { -238, 4088, -60 }, { 3962, 246, 1006 } }, { 2609, 1438, 888 } }, 230 },
    { { { { 1006, 0, -3970 }, { -325, 4082, -82 }, { 3957, 335, 1003 } }, { -1476, 1537, 851 } }, 230 },
    { { { { 3845, 0, -1410 }, { -107, 4084, -293 }, { 1406, 312, 3834 } }, { -4795, 1541, 1484 } }, 230 },
    { { { { -1552, 0, -3790 }, { -270, 4085, 110 }, { 3780, 292, -1548 } }, { -8355, 1552, -250 } }, 257 },
};

GpSprtCmd D_shelter_b2_elevator_hall_80183ED4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b2_elevator_hall_80183EE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b2_elevator_hall_80183EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_elevator_hall_80183F04[10] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -96, -8, 1410, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, -48, 1362, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -64, 0, 1605, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -64, -48, 1559, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, -48, 1616, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, 0, 1672, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -24, -8, 1660, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, -48, 1646, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -48, 1815, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -8, 1799, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b2_elevator_hall_80183FCC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b2_elevator_hall_80183FE4[2] = {
    { { 147, 70, 103, 94 }, 1850 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b2_elevator_hall_80183FF8[12] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 72, 822, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -160, -120, 645, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -112, -120, 780, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -112, -64, 809, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, -24, 825, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 654, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, -64, 651, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 658, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 667, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, 24, 835, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 72, 834, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 72, 822, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b2_elevator_hall_801840E8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b2_elevator_hall_80184100[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b2_elevator_hall_80184110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b2_elevator_hall_80184120[7] = {
    { { .empty = D_shelter_b2_elevator_hall_80183ED4 }, D_shelter_b2_elevator_hall_80183ED4, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80183EE4 }, D_shelter_b2_elevator_hall_80183EE4, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80183EF4 }, D_shelter_b2_elevator_hall_80183EF4, NULL },
    { { .elements = D_shelter_b2_elevator_hall_80183F04 }, D_shelter_b2_elevator_hall_80183FCC, D_shelter_b2_elevator_hall_80183FE4 },
    { { .elements = D_shelter_b2_elevator_hall_80183FF8 }, D_shelter_b2_elevator_hall_801840E8, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80184100 }, D_shelter_b2_elevator_hall_80184100, NULL },
    { { .empty = D_shelter_b2_elevator_hall_80184110 }, D_shelter_b2_elevator_hall_80184110, NULL },
};

GpPointLight D_shelter_b2_elevator_hall_80184174[14] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -681, -659, -1106 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -681, -659, 1137 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1648, -659, 1137 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1047, -659, -1192 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6077, -659, -1034 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8608, -659, -855 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8600, -659, 2166 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4834, -659, 2957 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7805, -6652, 1436 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3279, 3368, 3448, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2330, -6652, -263 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3267, 3308, 3588, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5813, -5652, -97 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3366, 3466, 3708, { 0, 0 } }, 2000, 0x2801 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8513, -659, 35 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7513, -659, -1106 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4617, -659, -1134 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 900, 1900 },
};

GpRoomCoordSet D_shelter_b2_elevator_hall_801846B4 = { 0, NULL, 14, D_shelter_b2_elevator_hall_80184174, 0, NULL };

GpObj4C D_shelter_b2_elevator_hall_801846CC[8] = {
    { NULL, NULL, NULL, { -4678, 64, -197, 0 }, { { 115, -2016, 2533, 0 }, { -124, -2016, -2543, 0 }, { 115, 2016, 2533, 0 }, { -124, 2016, -2543, 0 } }, { -4101, 0, 192, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -4865, 0, -144, 0 }, { { -141, -2016, -2631, 0 }, { 126, -2016, 2616, 0 }, { -141, 2016, -2631, 0 }, { 126, 2016, 2616, 0 } }, { 4090, 0, -209, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 64, 0, 0, 0 }, { { -5, -2016, -2627, 0 }, { 5, -2016, 2627, 0 }, { -5, 2016, -2627, 0 }, { 5, 2016, 2627, 0 } }, { 4095, 0, -8, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 256, 0, 0, 0 }, { { -5, -2016, 2541, 0 }, { 5, -2016, -2541, 0 }, { -5, 2016, 2541, 0 }, { 5, 2016, -2541, 0 } }, { -4106, 0, -9, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 4256, 0, 0, 0 }, { { -259, -2016, 2523, 0 }, { 249, -2016, -2533, 0 }, { -259, 2016, 2523, 0 }, { 249, 2016, -2533, 0 } }, { -4084, 0, -412, 0 }, { 0, 0, 4096, 0 }, 3238, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 4065, 0, 0, 0 }, { { 249, -2016, -2620, 0 }, { -257, -2016, 2609, 0 }, { 249, 2016, -2620, 0 }, { -257, 2016, 2609, 0 } }, { 4076, 0, 394, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 7680, 0, 2320, 0 }, { { -3429, -2016, -371, 0 }, { 3429, -2016, 371, 0 }, { -3429, 2016, -371, 0 }, { 3429, 2016, 371, 0 } }, { 441, 0, -4083, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 7808, 0, 2097, 0 }, { { 3515, -2016, 381, 0 }, { -3515, -2016, -381, 0 }, { 3515, 2016, 381, 0 }, { -3515, 2016, -381, 0 } }, { -443, 0, 4080, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 5, 129, 0 },
};

GpObj3A D_shelter_b2_elevator_hall_8018492C[1] = {
    { NULL, NULL, { 2560, -1344, 3856, 0 }, { { -1664, 2368, 2096, 0 }, { 1664, 2368, -2096, 0 }, { -1664, -2368, 2096, 0 }, { 1664, -2368, -2096, 0 } }, { 3208, 0, 2546, 0 }, { -19, 13 }, 129, 0 },
};

GpObj4C D_shelter_b2_elevator_hall_80184968[3] = {
    { NULL, NULL, NULL, { -5632, -48, 1072, 0 }, { { -832, 0, -464, 0 }, { 833, 0, -464, 0 }, { -832, 0, 464, 0 }, { 833, 0, 464, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 951, 0, 33, 17, 2, 0 },
    { NULL, NULL, NULL, { 9712, -48, -464, 0 }, { { 384, 0, -624, 0 }, { 384, 0, 624, 0 }, { -384, 0, -624, 0 }, { -384, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 732, 0, 26, 33, 2, 0 },
    { NULL, NULL, NULL, { 9648, -48, 4272, 0 }, { { 384, 0, -752, 0 }, { 384, 0, 752, 0 }, { -384, 0, -752, 0 }, { -384, 0, 752, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 844, 0, 28, 49, 130, 0 },
};

GpAreaTmdRec D_shelter_b2_elevator_hall_80184A4C[2] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_elevator_hall_80184A64[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_elevator_hall_80184A7C[2] = {
    { 49, 49, 0, 0, { 0, 0 }, D_80147400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_elevator_hall_80184A94[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_elevator_hall_80184AB8[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b2_elevator_hall_80184ADC[8] = {
    { 26, 0, 0, 7500, 0, 4150, 1800, 0, 0, 2, 0 },
    { 26, 0, 0, 7000, 0, 1950, 1300, 0, 0, 2, 0 },
    { 26, 0, 0, 5300, 0, 3600, 1700, 0, 0, 2, 0 },
    { 26, 0, 0, 5400, 0, 1700, 1500, 0, 0, 2, 0 },
    { 26, 0, 0, 1850, 0, 350, 1250, 0, 0, 2, 0 },
    { 26, 0, 0, 300, 0, 600, 1200, 0, 0, 2, 0 },
    { 26, 0, 0, 300, 0, -800, 900, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_elevator_hall_80184B5C[3] = {
    { 3, 0, 0, 6500, 0, 3300, 2048, 0, 0, 2, 0 },
    { 3, 0, 1, -4000, 0, 0, 1024, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_elevator_hall_80184B8C[4] = {
    { 49, 2, 0, -7800, 0, -200, 1200, 0, 0, 2, 0 },
    { 49, 2, 0, 250, 0, 600, 1600, 0, 0, 2, 0 },
    { 49, 2, 0, 4350, 0, 500, 1800, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_elevator_hall_80184BCC[6] = {
    { 21, 3, 0, 7500, -1800, -1400, 0, 0, 0, 2, 0 },
    { 21, 3, 0, 5500, -1800, -1400, 0, 0, 0, 2, 0 },
    { 21, 3, 0, 7500, -1800, 4900, 2048, 0, 0, 2, 0 },
    { 21, 3, 0, 5500, -1800, 4900, 2048, 0, 0, 2, 0 },
    { 57, 3, 1, 6500, 0, 3000, 2048, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_elevator_hall_80184C2C[5] = {
    { 21, 3, 0, 8500, -1800, 4900, 2048, 0, 0, 2, 7 },
    { 21, 3, 0, 6500, -1800, 4900, 2048, 0, 0, 2, 7 },
    { 57, 9, 1, -2000, 0, 0, 1024, 0, 2, 4, 0 },
    { 57, 0, 0, 9600, 0, -500, 3072, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b2_elevator_hall_80184C7C[22] = {
    { NULL, NULL },
    { D_shelter_b2_elevator_hall_80184ADC, D_shelter_b2_elevator_hall_80184A4C },
    { D_shelter_b2_elevator_hall_80184B5C, D_shelter_b2_elevator_hall_80184A64 },
    { D_shelter_b2_elevator_hall_80184B8C, D_shelter_b2_elevator_hall_80184A7C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_elevator_hall_80184BCC, D_shelter_b2_elevator_hall_80184A94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_elevator_hall_80184C2C, D_shelter_b2_elevator_hall_80184AB8 },
};

s32 D_shelter_b2_elevator_hall_80184D2C[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

s32 D_shelter_b2_elevator_hall_80184D38[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b2_elevator_hall_80184D44[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b2_elevator_hall_80184D4C[1] = {
    { 0, 0, 1, 0, D_shelter_b2_elevator_hall_80184D2C },
};

GpRoomParamRec D_shelter_b2_elevator_hall_80184D54[1] = {
    { 0, 0, 1, 0, D_shelter_b2_elevator_hall_80184D38 },
};

GpRoomParamRec * D_shelter_b2_elevator_hall_80184D5C[8] = {
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D4C,
    D_shelter_b2_elevator_hall_80184D54,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
    D_shelter_b2_elevator_hall_80184D44,
};

RoomEventMsg D_shelter_b2_elevator_hall_80184D7C = { 0 };

u8 D_shelter_b2_elevator_hall_80184D84[4] = {
    0,
    253,
    152,
    217,
};

RoomEventReq D_shelter_b2_elevator_hall_80184D88 = { 0 };

/// Gates an event on a game-flag nibble and a collected item: returns 1 when
/// the nibble already shows the event done, 0 (running the request's refusal
/// cap command) when the item is missing, and 2 when it fires, which unless
/// `msg` is a dry run records the request, sets the nibble and spawns the
/// event task.
static s32 func_shelter_b2_elevator_hall_8017D610(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                = req->flagId;
    D_shelter_b2_elevator_hall_80184D84[0] = 0;
    neg                                 = flag < 0;
    got                                 = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_shelter_b2_elevator_hall_80184D7C = *msg;
                D_shelter_b2_elevator_hall_80184D88 = *req;
                id                                  = req->flagId;
                mode                                = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_elevator_hall_80183790, 0, 0, 0);
                D_shelter_b2_elevator_hall_80184D84[0] = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

void func_shelter_b2_elevator_hall_8017D774(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_elevator_hall_80184D88.field_0);
            if (D_shelter_b2_elevator_hall_80184D88.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_elevator_hall_80184D88.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_elevator_hall_80184D88.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_elevator_hall_80184D88.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_elevator_hall_80184D88.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_elevator_hall_80184D88.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant   = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b2_elevator_hall_80184D7C.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b2_elevator_hall_80184D7C.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_shelter_b2_elevator_hall_80184D7C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Elevator task: sends player-weapon message 0 and waits for the cap to go
/// idle, then picks the destination from the cap event key (0xB: area 9 warp 3,
/// 0xC: area 0x1B warp 2, 0xD: area 0x2A warp 3; any other key sends message 1
/// and ends the task). Once the voice line in `spawnArg1` has finished it
/// resolves the destination through `func_map_shelter_80179A04`, stores the resolved warp
/// and room in the save location and spawns task 0x11.
void func_shelter_b2_elevator_hall_8017D8E4(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StateF0.field_4 = 1;
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_StateF0.field_4 = 0;
                goto next;
            }
            break;
        case 2:
            Gp_StateF0.field_4 = 1;
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData[0].state.at4.loc.area = 9;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData[0].state.at4.loc.area = 0x1B;
                    Mc_SaveData[0].state.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData[0].state.at4.loc.area = 0x2A;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                default:
                    Gp_MsgPlayerWeapon(1);
                    Gp_StateF0.field_4 = 0;
                    taskKill(task);
                    break;
            }
            goto next;
        case 3:
            if (SndVoice_HasActiveId(task->spawnArg1.value) != 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.field_3 = 1;
            msg.field_5 = 0;
            msg.prefix.packed   = Mc_SaveData[0].state.at4.loc.area;
            msg.field_2 = Mc_SaveData[0].state.at4.loc.warp;
            msg2        = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.roomVariant   = 1;
            Mc_SaveData[0].state.at4.loc.warp = msg2.field_2;
            Mc_SaveData[0].state.at4.loc.room = msg2.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Messages 0x21 and 0x1C build a request for the gate
/// `func_shelter_b2_elevator_hall_8017D610` (nibble 0xAB with no item, and
/// nibble 0xA9 with item 0x21, which also sets item-seen bit 0x121 when the
/// gate reports the event fired). Message 0x1A
/// answers 0 and, unless `in->field_5` asks for a dry run, either sets the
/// message's nibble and runs CAP command 4 while nibble 0xBA is clear, or runs
/// CAP command 5 and spawns the room's task once it is set. Anything else
/// answers 1.
s32 func_shelter_b2_elevator_hall_8017DAD4(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->prefix.packed == 0x21) {
        req.field_0 = 1;
        req.field_4 = 1;
        req.field_8 = 0x541B0007;
        req.field_C = 0x541B0005;
        req.flagId  = 0xAB;
        req.itemId  = 0;
        return func_shelter_b2_elevator_hall_8017D610(&req, out);
    }
    if (in->prefix.packed == 0x1C) {
        req.field_0 = 3;
        req.field_4 = 2;
        req.field_8 = 0x541B0009;
        req.field_C = 0x541B0003;
        req.flagId  = 0xA9;
        req.itemId  = 0x21;
        ret         = func_shelter_b2_elevator_hall_8017D610(&req, out);
        if (D_shelter_b2_elevator_hall_80184D84[0] != 0) {
            Gp_SetItemSeenBit(0x121, 1);
        }
        return ret;
    }
    if (in->prefix.packed == 0x1A) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(4);
            }
            return 0;
        }
        if (in->field_5 == 0) {
            Gp_RunCapCmd(5, 0);
            Task_SpawnFromTable(&D_shelter_b2_elevator_hall_8018379C, 0, 0x541B0001, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b2_elevator_hall_8017DC70(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC78(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC80(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC88(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 1) {
        SndEvt_EnqueueType6(0x541B0000 | 1, 0, 0);
    }
    return 0;
}

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b2_elevator_hall_8017DD08` from a stack copy.
static const TaskFuncTable3 D_shelter_b2_elevator_hall_8017D5F0 = {
    {
        func_shelter_b2_elevator_hall_8017DCBC,
        func_shelter_b2_elevator_hall_8017DD00,
        taskKill,
    },
};

/// Installs the room's message table on `task` and advances it.
static void func_shelter_b2_elevator_hall_8017DCBC(Task* task)
{
    task->msgTable = D_shelter_b2_elevator_hall_801837A8;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Empty middle state of the room's state table.
static void func_shelter_b2_elevator_hall_8017DD00(Task* task)
{
}

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b2_elevator_hall_8017DD08(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_elevator_hall_8017D5F0;
    sp.funcs[task->state](task);
}

void func_shelter_b2_elevator_hall_8017DD60(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024A;
        D_80115744  = 0x60256;
        D_8011573C  = 0x60261;
        D_80115720  = 0x6026D;
        D_80115758  = 0x601D0;
        D_8011572C  = 0x601EC;
        D_80115750  = 0x60208;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b2_elevator_hall_801837D8;
            func_shelter_b2_elevator_hall_8017DFB8(&p[0], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[2], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[16], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[24], 0x200, 0x412);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b2_elevator_hall_801837F8;
            func_shelter_b2_elevator_hall_8017DFB8(&p[0], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[8], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[12], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[20], 0x200, 0x412);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b2_elevator_hall_80183808;
            func_shelter_b2_elevator_hall_8017DFB8(&p[0], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[8], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[12], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[16], 0x200, 0x412);
            break;
        }
        case 5: {
            SVECTOR* p;
            if (GameFlag_GetNibble(0xA9) != 0) {
                func_shelter_b2_elevator_hall_8017E7FC(D_shelter_b2_elevator_hall_801838A8, 0x100, 0x504C);
            } else {
                func_shelter_b2_elevator_hall_8017E7FC(D_shelter_b2_elevator_hall_801838B0, 0x100, 0x5C40);
            }
            p = D_shelter_b2_elevator_hall_80183868;
            func_shelter_b2_elevator_hall_8017DFB8(&p[0], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[2], 0x200, 0x412);
            func_shelter_b2_elevator_hall_8017DFB8(&p[4], 0x200, 0x412);
            break;
        }
        case 6: {
            SVECTOR* p;
            if (GameFlag_GetNibble(0xA9) != 0) {
                func_shelter_b2_elevator_hall_8017E7FC(D_shelter_b2_elevator_hall_801838A8, 0x100, 0x504C);
            } else {
                func_shelter_b2_elevator_hall_8017E7FC(D_shelter_b2_elevator_hall_801838B0, 0x100, 0x5C40);
            }
            p = D_shelter_b2_elevator_hall_80183868;
            func_shelter_b2_elevator_hall_8017DFB8(&p[0], 0x180, 0x111);
            func_shelter_b2_elevator_hall_8017DFB8(&p[2], 0x200, 0x412);
            break;
        }
    }
}

/// Projects `arg0[0]` and `arg0[1]` through `gGfxViewCoord.workm` and, when both
/// project, sweeps half a turn of gouraud `POLY_G4` wedges around the line
/// between them: a cap on each end and a band joining the two, with radii
/// `(s16)arg1 * 64 / otz` at each end. The lit vertices take the colour packed
/// in `arg2`'s low twelve bits (4 bits per channel, moved into the high
/// nibble), with bit 0 of the frame counter blended in as 8.
static void func_shelter_b2_elevator_hall_8017DFB8(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Projects `arg0` through `gGfxViewCoord.workm` and, when it projects, queues a
/// sixteen-wedge gouraud disc plus a four-pointed inner cross. On-screen radii
/// are `(s16)arg1 * 64 / otz` (outer) and `(s16)arg1 * 8 / otz` (inner). `arg2`
/// packs the tint as four nibbles `[shift][r][g][b]`; bit 0 of the frame
/// counter, shifted by the top nibble, is added to every channel, so the disc
/// flickers on alternate frames. Each outer wedge draws at half brightness and
/// full size, then at full brightness and half size; the cross uses the halved
/// colour.
static void func_shelter_b2_elevator_hall_8017E7FC(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                packed;
    s32                blend;
    s32                size;
    s32                otz;
    s32                rOuter;
    s32                rInner;
    s32                ang;
    s32                t;
    s32                t2;
    s32                r;
    s32                g;
    s32                b;
    s32                rh;
    s32                gh;
    s32                bh;

    {
        void** scratch;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        block   = (RoomDraw05Scratch*)(*scratch = head - 0x14);
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        size          = (s16)arg1;
        otz           = block->otz + 1;
        rOuter        = (size * 64) / otz;
        block->otz    = otz;
        ds            = &gDisplayState;
        blend         = ds->animFrame;
        block->rOuter = rOuter;
        rInner        = (size * 8) / block->otz;
        packed        = arg2 << 16;
        blend         = blend & 1;
        blend         = blend << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = rInner;
        ang           = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            rh = (u8)r >> 1;
            gh = (u8)g >> 1;
            bh = (u8)b >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rh, gh, bh);
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
            setRGB2(prim, r, g, b);
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

        r   = (u8)rh;
        g   = (u8)gh;
        b   = (u8)bh;
        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang - 0x400)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ang - 0x400)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x400)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x400)) >> 13);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang + 0x400)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang + 0x400)) >> 11);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ang + 0x800)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ang + 0x800)) >> 12);
            ang     += 0x800;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

void func_shelter_b2_elevator_hall_8017F1D8(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b2_elevator_hall_8017F4A4(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b2_elevator_hall_8017F4A4(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void func_shelter_b2_elevator_hall_8017F4A4(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;

    row           = arg2 >> 12;
    arg2         &= 0xFFF;
    pal           = arg3 >> 12;
    arg3         &= 0xFF;
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
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
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a ring of sixteen gouraud `POLY_G4` wedges between radii
/// `(s16)arg1 * 64 / (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`,
/// black on the first and `rgb` on the second. Callers truncate `arg1` to 16
/// bits themselves.
static void func_shelter_b2_elevator_hall_8017F768(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   outer;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
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
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues a fan of eight gouraud `POLY_G4`
/// wedges around it, of radius `arg1 * 64 / (otz + 1)`: black at the rim and
/// coloured `arg2` at the centre.
static void func_shelter_b2_elevator_hall_8017FB8C(GpCoord* arg0, s16 arg1, u8* arg2)
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
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
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

void func_shelter_b2_elevator_hall_8017FF20(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GpCoord*    coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->m00_m01      = 0x1000;
                rot->m02_m10      = 0;
                rot->m11_m12      = 0x1000;
                rot->m20_m21      = 0;
                rot->m22          = 0x1000;
                coord->coord.t[0] = mem->pos.vx;
                coord->coord.t[1] = mem->pos.vy;
                coord->coord.t[2] = mem->pos.vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1.value)->field_2;
                mem->index      = shift;
                arg0->spawnArg1.value = ((GpEffSpawnArg*)&arg0->spawnArg1.value)->field_0;
                arg0->state     = 1;
                mem->step       = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]           = mem->scale >> D_shelter_b2_elevator_hall_801838B8[mem->index].r;
                rgb[1]           = mem->scale >> D_shelter_b2_elevator_hall_801838B8[mem->index].g;
                rgb[2]           = mem->scale >> D_shelter_b2_elevator_hall_801838B8[mem->index].b;
                func_shelter_b2_elevator_hall_8017FB8C(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b2_elevator_hall_8017FB8C(coord, mem->angle + 0x100, rgb);
                }
                func_shelter_b2_elevator_hall_8017F768(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b2_elevator_hall_801838B8[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b2_elevator_hall_801838B8[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b2_elevator_hall_801838B8[mem->index].b;
                    func_shelter_b2_elevator_hall_80180D08(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_shelter_b2_elevator_hall_801802B8(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
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
        func_shelter_b2_elevator_hall_8017FB8C(coord, step * 2, rgb);
        func_shelter_b2_elevator_hall_80180464(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b2_elevator_hall_8017F768(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
static void func_shelter_b2_elevator_hall_80180464(GpCoord* coord, s16 size)
{
    GpCoord        ground;
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

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = shifted >> 0x11;
    light->head.b               = shifted >> 0x12;
    light->head.u.at.local.t[0] = coord->coord.t[0];
    light->head.u.at.local.t[1] = coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    Gp_LcgState                 = random;
    block                       = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx               = coord->workm.t[0];
    block->vec.vy               = coord->workm.t[1];
    block->vec.vz               = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b2_elevator_hall_80180990(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b2_elevator_hall_80180990(GpCoord* arg0, s32 arg1)
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a gouraud starburst: a sixteen-wedge disc of radius
/// `arg1 * 64 / (otz + 1)` at half the colour `arg2`, the same disc at half
/// size and full colour, and a four-pointed cross at half colour, all fading
/// to black at the rim.
static void func_shelter_b2_elevator_hall_80180D08(GpCoord* arg0, s16 arg1, u8* arg2)
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

void func_shelter_b2_elevator_hall_801816C8(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}

/// A flash effect task. State 1 ramps its level up over `spawnArg1` ticks,
/// drawing two fans and an inward-shrinking ring in a colour derived from the
/// level, and queues a fade quad in that colour when it peaks; state 2 fades
/// out through the star draw before the work block is released.
void func_shelter_b2_elevator_hall_801817FC(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    u8         rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        switch (arg0->state) {
            case 0:
                mem->scale  = 0;
                mem->angle  = 0x80;
                mem->step   = 0x100 / arg0->spawnArg1.value;
                arg0->state = 1;
                break;
            case 1:
                mem->scale += mem->step;
                mem->angle += mem->step;
                arg0->spawnArg1.value--;
                rgb[0] = mem->scale;
                rgb[1] = mem->scale >> 2;
                rgb[2] = mem->scale >> 1;
                func_shelter_b2_elevator_hall_80181ECC(coord, mem->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b2_elevator_hall_80181ECC(coord, (u16)mem->angle * 2, rgb);
                func_shelter_b2_elevator_hall_80181AA0(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    rgb[0]      = mem->scale;
                    rgb[1]      = mem->scale >> 2;
                    rgb[2]      = mem->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale;
                    rgb[1] = mem->scale >> 2;
                    rgb[2] = mem->scale >> 1;
                    func_shelter_b2_elevator_hall_80182DD0(coord, mem->angle * 3, rgb);
                    mem->scale -= 0x10;
                    mem->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(mem, arg0);
                break;
        }
    }
}

/// The same ring as `func_shelter_b2_elevator_hall_8017F768`, built in a
/// scratch block with its fields in a different order. Callers truncate `arg1`
/// to 16 bits themselves.
static void func_shelter_b2_elevator_hall_80181AA0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// A second, instruction-for-instruction copy of
/// `func_shelter_b2_elevator_hall_8017FB8C`.
static void func_shelter_b2_elevator_hall_80181ECC(GpCoord* arg0, s16 arg1, u8* arg2)
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
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
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

void func_shelter_b2_elevator_hall_80182260(Task* task)
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
                objCoord->coord.t[0] = D_shelter_b2_elevator_hall_801838CC[0].vx;
                objCoord->coord.t[1] = D_shelter_b2_elevator_hall_801838CC[0].vy;
                objCoord->coord.t[2] = D_shelter_b2_elevator_hall_801838CC[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b2_elevator_hall_801838CC[1];
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
                {
                    SVECTOR* edge = &D_shelter_b2_elevator_hall_801838CC[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
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
                func_shelter_b2_elevator_hall_80182750(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a trail of seven gouraud quads between two eight-slot rings of
/// coordinates, walking back from slot `arg2` and fading with age. `arg3`
/// packs the colour as 2-bit channel multipliers at bits 8, 4 and 0.
static void func_shelter_b2_elevator_hall_80182750(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
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

void func_shelter_b2_elevator_hall_80182B48(Task* task)
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
            func_shelter_b2_elevator_hall_80181AA0(objCoord, 0x100, 0x100, rgb);
            func_shelter_b2_elevator_hall_80181AA0(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// A second, instruction-for-instruction copy of
/// `func_shelter_b2_elevator_hall_80180D08`.
static void func_shelter_b2_elevator_hall_80182DD0(GpCoord* arg0, s16 arg1, u8* arg2)
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
