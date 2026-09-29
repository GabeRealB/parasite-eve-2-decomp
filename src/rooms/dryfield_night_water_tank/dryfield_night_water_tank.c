#include "rooms/dryfield_night_water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

extern GpGridParams D_dryfield_night_water_tank_8017E08C;
extern GpGridParams D_dryfield_night_water_tank_8017F4B0;

/// 0xFF-terminated `GpAreaApplyRec` list the room applies when the scripted end
/// of the visit fires.
extern GpAreaApplyRec D_dryfield_night_water_tank_801808B0[];

/// Main-executable halfword the second state waits on before it may advance.

/// Script blobs handed to `func_800E8634` (which forwards them to `Task_Spawn`)
/// as raw addresses.
extern s32 D_80137C28;
extern s32 D_80138570;

/// Message table of the night water-tank room, 0x13EE..0x13F1 with the
/// 0x7FFFFFFF terminator: `func_dryfield_night_water_tank_8017D714`,
/// `..._8017D70C`, `..._8017D76C` and `..._8017D73C`.
extern GpMsgEntry D_dryfield_night_water_tank_8017DFE8[];

/// Task descriptor tables spawned by the room entry task, each one entry and
/// the 0xFFFF terminator: `8017E010` runs the exit task
/// `func_dryfield_night_water_tank_8017D5D0`, `8017EE28` the tank model's
/// update `func_dryfield_night_water_tank_8017DB8C`.
extern TaskDesc D_dryfield_night_water_tank_8017E010[];
extern TaskDesc D_dryfield_night_water_tank_8017EE28[];

/// Absolute import: 0x8013224C has no name in main or gameplay, so the call is
/// emitted against bare address, the way the other rooms' `func_8013...` are.
extern void func_8013224C(void);

/// Absolute import: the shared room script descriptor 0x8013788C, spawned by
/// entry 0 in the handler below.
extern TaskDesc D_8013788C;

/// Model/lighting records the handler below toggles on message 3 and 4.
extern GpEvsCmd D_dryfield_night_water_tank_8017DDD8[];
extern GpEvsCmd D_dryfield_night_water_tank_8017DEE0[];

/// The tank's wobble spring: `8017EE40` is the accumulated yaw handed to
/// `Gfx_RotMatrixY` (`>> 8`), `8017EE44` its velocity, `8017EE48` the yaw it
/// steps toward and `8017EE4C` the target that step chases.
extern s32 D_dryfield_night_water_tank_8017EE40;
extern s32 D_dryfield_night_water_tank_8017EE44;
extern s32 D_dryfield_night_water_tank_8017EE48;
extern s32 D_dryfield_night_water_tank_8017EE4C;

static void func_dryfield_night_water_tank_8017D9DC(s32 arg0);

extern GpGridParams   D_dryfield_night_water_tank_8017F4B0;
extern GpObj3A        D_dryfield_night_water_tank_801807CC[2];
extern GpObj4C        D_dryfield_night_water_tank_8018038C[4];
extern GpObj4C        D_dryfield_night_water_tank_801804BC[8];
extern GpRoomCoordSet D_dryfield_night_water_tank_80180374[1];
extern TmdSource      D_dryfield_night_water_tank_8017EE04;
void                  func_dryfield_night_water_tank_8017DB8C(Task*);

s32  func_dryfield_night_water_tank_8017D70C(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_night_water_tank_8017D714(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32  func_dryfield_night_water_tank_8017D73C(Task*, s32, s32, GpMessageArg);
s32  func_dryfield_night_water_tank_8017D76C(Task*, s32, RoomEventMsg*, GpMessageArg);
void func_dryfield_night_water_tank_8017D5D0(Task*);

GpXformArg D_dryfield_night_water_tank_8017DD94 = { { 820, -0x4010, 884, 0 }, { 0, 2560, 0, 0 } };

GpXformArg D_dryfield_night_water_tank_8017DDAC = { { 1868, -0x2EE0, 1740, 0 }, { 0, 512, 0, 0 } };

GpAnimArg D_dryfield_night_water_tank_8017DDC4 = { { .index = 1 }, 1, 0, 0, 0 };

GpEvsCmd D_dryfield_night_water_tank_8017DDD8[11] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_tank_8017DDC4 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_water_tank_8017DD94 }, { .value = 0 } },
    { 42, { .value = 0x52150001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_night_water_tank_8017DEE0[11] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_water_tank_8017DDC4 }, { .value = 0 } },
    { 24, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_night_water_tank_8017DDAC }, { .value = 0 } },
    { 42, { .value = 0x52150001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpMsgEntry D_dryfield_night_water_tank_8017DFE8[5] = {
    { 5102, func_dryfield_night_water_tank_8017D714 },
    { 5105, func_dryfield_night_water_tank_8017D70C },
    { 5103, func_dryfield_night_water_tank_8017D76C },
    { 5104, func_dryfield_night_water_tank_8017D73C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_night_water_tank_8017E010[2] = {
    { 0, 32, func_dryfield_night_water_tank_8017D5D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

SVECTOR D_dryfield_night_water_tank_8017E028[2] = {
    { 4000, 0, 882, 0 },
    { 865, 0, 4004, 0 },
};

SVECTOR D_dryfield_night_water_tank_8017E038[6] = {
    { 1179, -0x2E7E, -2286, 0 },
    { 1179, -0x3266, -2286, 0 },
    { 1016, -0x3266, -1544, 0 },
    { 1016, -0x2E7E, -1544, 0 },
    { 1910, -0x2E7E, -2444, 0 },
    { 1910, -0x3266, -2444, 0 },
};

GpGridFace D_dryfield_night_water_tank_8017E068[2] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 0, 4, 1, 5 }, 1, 0 },
};

s16 D_dryfield_night_water_tank_8017E080[3] = {
    0,
    1,
    -1,
};

s16* D_dryfield_night_water_tank_8017E088[1] = {
    D_dryfield_night_water_tank_8017E080,
};

GpGridParams D_dryfield_night_water_tank_8017E08C = { NULL, D_dryfield_night_water_tank_8017E028, D_dryfield_night_water_tank_8017E038, D_dryfield_night_water_tank_8017E068, D_dryfield_night_water_tank_8017E088, -1016, 2444, 1, 1, 4000, 2 };

TmdBone D_dryfield_night_water_tank_8017E0B0[1] = {
#include "assets/dryfield_night_water_tank_model_01844_skeleton.inc"
};

u32 D_dryfield_night_water_tank_8017E0D4[1] = {
#include "assets/dryfield_night_water_tank_model_01844_partVerts.inc"
};

SVECTOR D_dryfield_night_water_tank_8017E0D8[84] = {
#include "assets/dryfield_night_water_tank_model_01844_verts.inc"
};

SVECTOR D_dryfield_night_water_tank_8017E378[72] = {
#include "assets/dryfield_night_water_tank_model_01844_normals.inc"
};

u32 D_dryfield_night_water_tank_8017E5B8[531] = {
#include "assets/dryfield_night_water_tank_model_01844_stream.inc"
};

TmdSource D_dryfield_night_water_tank_8017EE04 = {
    0,
    3768,
    0,
    1,
    D_dryfield_night_water_tank_8017E0D4,
    D_dryfield_night_water_tank_8017E0D8,
    D_dryfield_night_water_tank_8017E378,
    D_dryfield_night_water_tank_8017E0B0,
    D_dryfield_night_water_tank_8017E5B8,
};

TaskDesc D_dryfield_night_water_tank_8017EE28[2] = {
    { 1, 192, func_dryfield_night_water_tank_8017DB8C, { .model = &D_dryfield_night_water_tank_8017EE04 } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_dryfield_night_water_tank_8017EE40 = 0;

s32 D_dryfield_night_water_tank_8017EE44 = 0;

s32 D_dryfield_night_water_tank_8017EE48 = 0;

s32 D_dryfield_night_water_tank_8017EE4C = 0;

GpRoomCoordRec D_dryfield_night_water_tank_8017EE50[1] = {
    { D_dryfield_night_water_tank_80180374, NULL },
};

GpRoomObjRec D_dryfield_night_water_tank_8017EE58[1] = {
    { &D_dryfield_night_water_tank_8017F4B0, D_dryfield_night_water_tank_8018038C, D_dryfield_night_water_tank_801804BC, D_dryfield_night_water_tank_801807CC },
};

u8 D_dryfield_night_water_tank_8017EE68[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    6,
    6,
    6,
    6,
    0,
    0,
};

u8* D_dryfield_night_water_tank_8017EE74[1] = {
    D_dryfield_night_water_tank_8017EE68,
};

GpViewCountRec D_dryfield_night_water_tank_8017EE78[1] = {
    { { .bytes = { 10, 0 } } },
};

GpWarpRec D_dryfield_night_water_tank_8017EE7C[2] = {
    { { .words = { 512, -2327, -0x2EDF, 1143 } }, { 0, 0, 0, 0 }, { .words = { 512, -2327, -0x2EDF, 1143 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
    { { .words = { 2560, 870, -0x4010, 934 } }, { 0, 0, 0, 0 }, { .words = { 2560, 870, -0x4010, 934 } }, { 0, 0, 0, 0 }, 0, 0, 0, 4, 0, 0 },
};

SVECTOR D_dryfield_night_water_tank_8017EEEC[16] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { 2896, 0, -2896, 0 },
    { 2896, 0, 2896, 0 },
    { -2896, 0, 2896, 0 },
    { -2896, 0, -2896, 0 },
    { 1295, 0, -3886, 0 },
    { 3581, 0, -1989, 0 },
    { -2896, 0, 2896, 0 },
    { -4096, 0, 0, 0 },
    { 2896, 0, -2896, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 4096, 0, 0, 0 },
    { -2812, 0, -2978, 0 },
};

SVECTOR D_dryfield_night_water_tank_8017EF6C[78] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 1300, -0x2EE0, 3300, 0 },
    { 1300, -0x2EE0, -3300, 0 },
    { -1300, -0x2EE0, -3300, 0 },
    { -1300, -0x2EE0, 3300, 0 },
    { -3300, -0x2EE0, -1300, 0 },
    { -3300, -0x2EE0, 1400, 0 },
    { 3300, -0x2EE0, 1400, 0 },
    { 3300, -0x2EE0, -1300, 0 },
    { 1220, -0x4204, 913, 0 },
    { 913, -0x4204, 1220, 0 },
    { 1279, -0x4204, 1586, 0 },
    { 1586, -0x4204, 1279, 0 },
    { 1220, -0x4010, 913, 0 },
    { 1586, -0x4010, 1279, 0 },
    { 1279, -0x4010, 1586, 0 },
    { 913, -0x4010, 1220, 0 },
    { -2000, -0x4010, 2000, 0 },
    { 2000, -0x4010, 2000, 0 },
    { 2000, -0x4010, -2000, 0 },
    { -2000, -0x4010, -2000, 0 },
    { -2300, -0x2EE0, -1100, 0 },
    { -2300, -0x3A98, -1100, 0 },
    { -1400, -0x3A98, -800, 0 },
    { -1400, -0x2EE0, -800, 0 },
    { -2800, -0x2EE0, -2000, 0 },
    { -2800, -0x3A98, -2000, 0 },
    { 800, -0x4A38, -1900, 0 },
    { 800, -0x4010, -1900, 0 },
    { 1900, -0x4010, -800, 0 },
    { 1900, -0x4A38, -800, 0 },
    { 1900, -0x4010, 800, 0 },
    { 1900, -0x4A38, 800, 0 },
    { -800, -0x4A38, 1900, 0 },
    { -800, -0x4010, 1900, 0 },
    { -1900, -0x4010, 800, 0 },
    { -1900, -0x4A38, 800, 0 },
    { 800, -0x4A38, 1900, 0 },
    { 800, -0x4010, 1900, 0 },
    { -800, -0x4010, -1900, 0 },
    { -800, -0x4A38, -1900, 0 },
    { -1900, -0x4A38, -800, 0 },
    { -1900, -0x4010, -800, 0 },
    { 1200, -0x32C8, -3000, 0 },
    { 1200, -0x2EE0, -3000, 0 },
    { 3000, -0x2EE0, -1200, 0 },
    { 3000, -0x32C8, -1200, 0 },
    { 3000, -0x2EE0, 1300, 0 },
    { 3000, -0x32C8, 1300, 0 },
    { -1200, -0x32C8, 3000, 0 },
    { -1200, -0x2EE0, 3000, 0 },
    { -3000, -0x2EE0, 1200, 0 },
    { -3000, -0x32C8, 1200, 0 },
    { 1200, -0x32C8, 3000, 0 },
    { 1200, -0x2EE0, 3000, 0 },
    { -1200, -0x2EE0, -3000, 0 },
    { -1200, -0x32C8, -3000, 0 },
    { -3000, -0x32C8, -1200, 0 },
    { -3000, -0x2EE0, -1200, 0 },
    { 800, -0x2EE0, -1900, 0 },
    { 1900, -0x2EE0, -800, 0 },
    { 1900, -0x2EE0, 800, 0 },
    { -800, -0x2EE0, 1900, 0 },
    { -1900, -0x2EE0, 800, 0 },
    { 800, -0x2EE0, 1900, 0 },
    { -800, -0x2EE0, -1900, 0 },
    { -1900, -0x2EE0, -800, 0 },
    { -1600, -0x2EE0, 450, 0 },
    { -1600, -0x3A98, 450, 0 },
    { -3500, -0x3A98, 450, 0 },
    { -3500, -0x2EE0, 450, 0 },
};

GpGridFace D_dryfield_night_water_tank_8017F1DC[38] = {
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 9, 10, 8, 11 }, 2, 3 },
    { { 12, 13, 10, 11 }, 2, 4 },
    { { 14, 15, 8, 9 }, 2, 4 },
    { { 17, 18, 16, 19 }, 2, 0 },
    { { 16, 19, 20, 21 }, 3, 0 },
    { { 19, 18, 21, 22 }, 4, 0 },
    { { 18, 17, 22, 23 }, 5, 0 },
    { { 16, 20, 17, 23 }, 6, 0 },
    { { 25, 26, 24, 27 }, 2, 5 },
    { { 29, 30, 28, 31 }, 7, 1 },
    { { 28, 32, 29, 33 }, 8, 1 },
    { { 35, 36, 34, 37 }, 9, 1 },
    { { 38, 39, 36, 37 }, 10, 1 },
    { { 41, 42, 40, 43 }, 11, 1 },
    { { 45, 41, 44, 40 }, 12, 1 },
    { { 44, 39, 45, 38 }, 6, 1 },
    { { 47, 48, 46, 49 }, 4, 1 },
    { { 46, 35, 47, 34 }, 13, 1 },
    { { 49, 48, 42, 43 }, 14, 1 },
    { { 51, 52, 50, 53 }, 9, 1 },
    { { 54, 55, 52, 53 }, 10, 1 },
    { { 57, 58, 56, 59 }, 11, 1 },
    { { 61, 57, 60, 56 }, 12, 1 },
    { { 60, 55, 61, 54 }, 15, 1 },
    { { 63, 64, 62, 65 }, 4, 1 },
    { { 62, 51, 63, 50 }, 13, 1 },
    { { 65, 64, 58, 59 }, 14, 1 },
    { { 35, 36, 66, 67 }, 11, 0 },
    { { 67, 36, 68, 38 }, 14, 0 },
    { { 41, 42, 69, 70 }, 9, 0 },
    { { 45, 41, 71, 69 }, 13, 0 },
    { { 71, 68, 45, 38 }, 4, 0 },
    { { 72, 73, 46, 49 }, 6, 0 },
    { { 46, 35, 72, 66 }, 12, 0 },
    { { 70, 42, 73, 49 }, 10, 0 },
    { { 75, 76, 74, 77 }, 13, 1 },
};

s16 D_dryfield_night_water_tank_8017F3A4[36] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    23,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    -1,
};

s16 D_dryfield_night_water_tank_8017F3EC[31] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    14,
    15,
    16,
    17,
    18,
    20,
    23,
    24,
    25,
    28,
    30,
    31,
    32,
    33,
    34,
    36,
    37,
    -1,
};

s16 D_dryfield_night_water_tank_8017F42C[30] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    13,
    14,
    16,
    17,
    18,
    19,
    21,
    22,
    25,
    26,
    27,
    29,
    30,
    32,
    33,
    34,
    35,
    -1,
};

s16 D_dryfield_night_water_tank_8017F468[27] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    13,
    14,
    15,
    16,
    17,
    21,
    22,
    23,
    24,
    25,
    29,
    30,
    31,
    32,
    33,
    -1,
};

s16* D_dryfield_night_water_tank_8017F4A0[4] = {
    D_dryfield_night_water_tank_8017F3A4,
    D_dryfield_night_water_tank_8017F3EC,
    D_dryfield_night_water_tank_8017F42C,
    D_dryfield_night_water_tank_8017F468,
};

GpGridParams D_dryfield_night_water_tank_8017F4B0 = { NULL, D_dryfield_night_water_tank_8017EEEC, D_dryfield_night_water_tank_8017EF6C, D_dryfield_night_water_tank_8017F1DC, D_dryfield_night_water_tank_8017F4A0, 3500, 3300, 2, 2, 4000, 38 };

GpViewRec D_dryfield_night_water_tank_8017F4D4[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x5DC0, 0 } }, 322 },
    { { { { 1429, 0, -3838 }, { -2523, 3086, -939 }, { 2892, 2692, 1077 } }, { 5050, 0x3E4E, 1880 } }, 257 },
    { { { { -853, 0, 4006 }, { 1212, 3903, 258 }, { -3818, 1239, -813 } }, { -5920, 0x37FA, -1350 } }, 257 },
    { { { { -1180, 0, 3922 }, { 173, 4092, 52 }, { -3918, 180, -1179 } }, { -4220, 0x42EA, -1350 } }, 230 },
    { { { { 2889, 0, 2902 }, { 1979, 2995, -1970 }, { -2123, 2793, 2113 } }, { -2370, 0x3719, 3415 } }, 329 },
    { { { { -2497, 0, 3246 }, { 3119, 1132, 2399 }, { -897, 3936, -690 } }, { 570, 0x42AE, 1550 } }, 380 },
};

GpSprtCmd D_dryfield_night_water_tank_8017F5AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_water_tank_8017F5BC[46] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -120, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, -120, 1125, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, -120, 1112, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -48, -120, 937, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -32, -120, 925, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -16, -120, 912, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -80, -120, 1052, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, -120, 1012, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -120, 987, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, -120, 975, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -120, 912, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 16, -120, 925, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 32, -120, 937, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -120, 975, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, -120, 987, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 64, -120, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, -120, 1062, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 80, -120, 1112, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, -120, 1127, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -120, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, -40, 1400, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -40, 1350, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, -40, 1287, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -40, 1237, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -40, 1237, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -40, 1287, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -40, 1350, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1437, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -144, 8, 1000, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 8, 1000, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 16, 1000, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 16, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -80, 8, 1000, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 16, 1000, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, 16, 1000, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, 40, 875, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 48, 875, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 64, 875, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 64, 875, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 40, 875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 32, 875, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 24, 875, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 16, 875, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 16, 875, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 8, 875, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 0, 875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_water_tank_8017F954[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 18, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_water_tank_8017F974[69] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -96, -120, 1125, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -120, 1164, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -120, 1112, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, -120, 1077, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 48, -120, 1062, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 56, -120, 1000, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -120, 1125, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 72, -120, 1175, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 80, -120, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -88, -56, 1500, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -56, 1500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -56, 1375, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -120, 1063, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -120, 1036, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -32, -120, 1020, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -120, 1013, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 0, -120, 1018, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 1030, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -120, 1051, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -32, 1375, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -32, 1250, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -32, -32, 1250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -32, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, 0, -32, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -32, 1250, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 32, -32, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, 48, -56, 1375, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 56, -56, 1375, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 64, -56, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, -56, 1375, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -24, 750, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -24, 750, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -136, 8, 750, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 8, 750, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 8, 750, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 40, 750, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 16, 750, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 64, 750, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 48, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 16, 750, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 750, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 16, 750, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 16, 750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 48, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 750, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 56, 750, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -16, 16, 750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, 16, 750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 750, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, 24, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 32, 24, 750, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 56, 750, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, 56, 750, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 16, 750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 72, 32, 750, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, 48, 750, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 64, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 750, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, 0, 750, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, -8, 750, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 32, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 16, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 24, 750, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 144, 16, 750, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_water_tank_8017FED8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 1, 0 } },
    { 30, 39, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_water_tank_8017FEF8[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 96, 625, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 8, 625, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 48, 8, 625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 56, 24, 625, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 24, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 80, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, 56, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 56, 625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 72, 625, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_water_tank_8017FFAC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_water_tank_8017FFC4[24] = {
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -48, 250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -144, -32, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -16, 250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 0, 250, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 0, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 0, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 0, 250, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 250, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 56, 250, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, 64, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -64, 64, 250, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 64, 250, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 250, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, 0, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, 64, 250, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, 0, 250, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 0, 250, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 0, 250, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 0, 250, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 0, 250, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 64, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 64, 250, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 64, 250, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 64, 250, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_water_tank_801801A4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_water_tank_801801BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_water_tank_801801CC[6] = {
    { { .empty = D_dryfield_night_water_tank_8017F5AC }, D_dryfield_night_water_tank_8017F5AC, NULL },
    { { .elements = D_dryfield_night_water_tank_8017F5BC }, D_dryfield_night_water_tank_8017F954, NULL },
    { { .elements = D_dryfield_night_water_tank_8017F974 }, D_dryfield_night_water_tank_8017FED8, NULL },
    { { .elements = D_dryfield_night_water_tank_8017FEF8 }, D_dryfield_night_water_tank_8017FFAC, NULL },
    { { .elements = D_dryfield_night_water_tank_8017FFC4 }, D_dryfield_night_water_tank_801801A4, NULL },
    { { .empty = D_dryfield_night_water_tank_801801BC }, D_dryfield_night_water_tank_801801BC, NULL },
};

GpLight D_dryfield_night_water_tank_80180214[4] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 100, 100, 100, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 100, 100, 100, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 100, 100, 100, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -5000, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 413, 823, 823, { 0, 0 } },
};

GpRoomCoordSet D_dryfield_night_water_tank_80180374[1] = {
    { 4, D_dryfield_night_water_tank_80180214, 0, NULL, 0, NULL },
};

GpObj4C D_dryfield_night_water_tank_8018038C[4] = {
    { NULL, NULL, NULL, { -864, -0x32D0, 2464, 0 }, { { 0, -1968, -1024, 0 }, { 0, -1968, 1024, 0 }, { 0, 1968, -1024, 0 }, { 0, 1968, 1024, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2217, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -576, -0x3310, 2464, 0 }, { { 0, -1904, 1024, 0 }, { 0, -1904, -1024, 0 }, { 0, 1904, 1024, 0 }, { 0, 1904, -1024, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 768, -0x32D0, -2368, 0 }, { { 0, -1936, 1024, 0 }, { 0, -1936, -1024, 0 }, { 0, 1936, 1024, 0 }, { 0, 1936, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2187, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 544, -0x3330, -2336, 0 }, { { 0, -1904, -1024, 0 }, { 0, -1904, 1024, 0 }, { 0, 1904, -1024, 0 }, { 0, 1904, 1024, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2157, 0, 3, 2, 129, 0 },
};

GpObj4C D_dryfield_night_water_tank_801804BC[8] = {
    { NULL, NULL, NULL, { -2449, -0x2F60, 640, 0 }, { { -728, 0, -256, 0 }, { 336, 0, -584, 0 }, { -624, 0, 680, 0 }, { 1016, 0, 160, 0 } }, { 0, 4096, 0, 0 }, { 401, 0, 4076, 0 }, 1024, 0, 20, 19, 2, 0 },
    { NULL, NULL, NULL, { 2064, -0x2F20, -832, 0 }, { { -1448, 0, -160, 0 }, { 1088, 0, -680, 0 }, { -1376, 0, 776, 0 }, { 1736, 0, 64, 0 } }, { 0, 4105, 0, 0 }, { 401, 0, 4076, 0 }, 1736, 0x8005, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -1620, -0x4040, -1538, 0 }, { { 985, 0, -789, 0 }, { 1431, 0, 313, 0 }, { -780, 0, 951, 0 }, { 316, 0, 1350, 0 } }, { 0, 4107, 0, 0 }, { 401, 0, 4076, 0 }, 1459, 2, 12, 1, 4, 0 },
    { NULL, NULL, NULL, { 1488, -0x2F23, 16, 0 }, { { 231, 0, 659, 0 }, { 229, 0, -659, 0 }, { 1395, 0, 739, 0 }, { 1411, 0, -737, 0 } }, { 0, 4103, 0, 0 }, { 4094, 0, 0, 0 }, 1588, 2, 14, 0, 4, 0 },
    { NULL, NULL, NULL, { -656, -0x2F21, 2160, 0 }, { { -841, 0, 1091, 0 }, { -619, 0, -1091, 0 }, { 611, 0, 1171, 0 }, { 851, 0, -1169, 0 } }, { 0, 4100, 0, 0 }, { 4094, 0, 0, 0 }, 1442, 0x8005, 2, 0, 3, 0 },
    { NULL, NULL, NULL, { 1471, -0x2F40, 1471, 0 }, { { -485, 0, 321, 0 }, { 259, 0, -507, 0 }, { 13, 0, 1473, 0 }, { 1559, 0, -134, 0 } }, { 0, 4100, 0, 0 }, { 2750, 0, 3035, 0 }, 1562, 5, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 1039, -0x4040, 1063, 0 }, { { 756, 0, -376, 0 }, { -379, 0, 767, 0 }, { 543, 0, -928, 0 }, { -920, 0, 537, 0 } }, { 0, 4102, 0, 0 }, { -3166, 0, -2599, 0 }, 1070, 5, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 576, -0x2F20, -1984, 0 }, { { 445, 0, 788, 0 }, { -788, 0, -221, 0 }, { 1897, 0, 504, 0 }, { -50, 0, -1395, 0 } }, { 0, 4102, 0, 0 }, { 4094, 0, 0, 0 }, 1962, 5, 5, 0, 132, 0 },
};

GpAreaTmdRec D_dryfield_night_water_tank_8018071C[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_water_tank_80180728[3] = {
    { 143, 463, 0, 0, { 0, 0 }, D_801427C8 },
    { 15, 15, 1, 0, { 0, 0 }, D_80153E28 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_water_tank_8018074C[2] = {
    { 143, 463, 0, 0, { 0, 0 }, D_801427C8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_water_tank_80180764[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C228, D_dryfield_night_water_tank_8018071C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017C238, D_dryfield_night_water_tank_80180728 },
    { D_map_dryfield_full_8017C288, D_dryfield_night_water_tank_8018074C },
    { NULL, NULL },
};

GpObj3A D_dryfield_night_water_tank_801807CC[2] = {
    { NULL, NULL, { -256, -0x35A0, 0, 0 }, { { -2080, 2560, 0, 0 }, { 2080, 2560, 0, 0 }, { -2080, -2560, 0, 0 }, { 2080, -2560, 0, 0 } }, { 0, 0, 4098, 0 }, { -30, 12 }, 1, 0 },
    { NULL, NULL, { 0, -0x35A0, 0, 0 }, { { 0, 2560, 1840, 0 }, { 0, 2560, -1840, 0 }, { 0, -2560, 1840, 0 }, { 0, -2560, -1840, 0 } }, { 4113, 0, 0, 0 }, { 73, 12 }, 129, 0 },
};

s32 D_dryfield_night_water_tank_80180844[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

s32 D_dryfield_night_water_tank_80180850[3] = {
    0x10000045,
    0x10000047,
    0x10000045,
};

s32 D_dryfield_night_water_tank_8018085C[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

GpRoomParamRec D_dryfield_night_water_tank_80180868[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_water_tank_80180870[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_night_water_tank_80180878[1] = {
    { 0, 0, 1, 0, D_dryfield_night_water_tank_80180844 },
};

GpRoomParamRec D_dryfield_night_water_tank_80180880[1] = {
    { 0, 0, 1, 0, D_dryfield_night_water_tank_80180850 },
};

GpRoomParamRec D_dryfield_night_water_tank_80180888[1] = {
    { 0, 0, 1, 0, D_dryfield_night_water_tank_8018085C },
};

GpRoomParamRec* D_dryfield_night_water_tank_80180890[8] = {
    D_dryfield_night_water_tank_80180868,
    D_dryfield_night_water_tank_80180870,
    D_dryfield_night_water_tank_80180868,
    D_dryfield_night_water_tank_80180878,
    D_dryfield_night_water_tank_80180880,
    D_dryfield_night_water_tank_80180888,
    D_dryfield_night_water_tank_80180868,
    D_dryfield_night_water_tank_80180868,
};

GpAreaApplyRec D_dryfield_night_water_tank_801808B0[2] = {
    { 3, 21, 11, 0 },
    { 255, 0, 0, 0 },
};

static void func_dryfield_night_water_tank_8017D870(Task* task);
static void func_dryfield_night_water_tank_8017D94C(Task* task);

/// Exit task of the night water-tank room, in the shape the other rooms' wait
/// tasks have: three states on `Task::state`. State 0 raises bit 0x80 of
/// `gGameSession::flowFlags` once `Gp_StateF0` has reached 1, then advances;
/// state 1 advances to 2 as soon as the halfword at `Gp_StateF0.field_6` clears; state
/// 2 runs the room's ending -- apply the area records, set flags 0x7B, 0x83,
/// 0x155 and 3, spawn the script `func_800E8634` is handed -- and kills the
/// task, or, while `gGameSession::field_126` is still clear, just ticks
/// `Task::killCountdown` down and waits for another frame.
void func_dryfield_night_water_tank_8017D5D0(Task* task)
{
    SVECTOR3 unused; // never referenced; only reserves the frame slot the ROM has

    switch (task->state) {
        case 0:
            if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                gGameSession->flowFlags = gGameSession->flowFlags | 0x80;
                task->state             = task->state + 1;
                return;
            }
            return;
        case 1:
            if (Gp_StateF0.field_6 == 0) {
                task->state = 2;
                return;
            }
            break;
        case 2:
            if (gGameSession->field_126 != 0) {
                Gp_ApplyAreaRecs(D_dryfield_night_water_tank_801808B0);
                GameFlag_SetNibble(0x7B, 2);
                GameFlag_SetNibble(0x83, 1);
                func_800E8634(&D_80137C28, 0, &D_80138570);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0xE);
                taskKill(task);
                return;
            }
            task->killCountdown = task->killCountdown - 1;
            break;
    }
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// answers 0.
s32 func_dryfield_night_water_tank_8017D70C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table: copies the location
/// record it is handed onto the outgoing one and answers 1.
s32 func_dryfield_night_water_tank_8017D714(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_night_water_tank_8017D73C(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 0xE) {
        Gp_StartCapSlot(0xE, 1, 1);
    }
    return 0;
}

s32 func_dryfield_night_water_tank_8017D76C(Task* arg0, s32 arg1, RoomEventMsg* in, GpMessageArg arg3)
{
    u8 temp_v1;

    if ((gGameSession->at4.loc.place != 0xA) || (GameFlag_GetNibble(0x7B) >= 2)) {
        if (in->field_2 == 3) {
            func_800E8614(D_dryfield_night_water_tank_8017DDD8, 0);
        }
        if (in->field_2 == 4) {
            func_800E8614(D_dryfield_night_water_tank_8017DEE0, 0);
        }
    }
    if (in->field_2 == 5) {
        temp_v1 = gGameSession->at4.loc.place;
        if ((u32)(temp_v1 - 0xA) < 2U) {
            if ((temp_v1 != 0xA) || (GameFlag_GetNibble(0x7B) >= 2)) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_8013788C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(0x17);
            }
        }
    }
    return 0;
}

/// Room entry task tick, the shape the other dryfield rooms' entry tasks have:
/// publish the message table the room's handlers hang off (0x13EE..0x13F1) in
/// `Task::msgTable`, claim game pointer slot 7, spawn the tank model's task
/// from `8017EE28`, then branch on the visit sub-id
/// (`gGameSession::at4.loc.place`).
///
/// Sub-ids 0xA and 0xB -- the two visits that reach this room -- both run the
/// prop updater `func_dryfield_night_water_tank_8017D9DC` on its zero argument;
/// 0xA additionally spawns the exit task from `8017E010`, and 0xB, the visit
/// the room is announced into, hands over to `func_8013224C` instead. The state
/// advances on every path.
static void func_dryfield_night_water_tank_8017D870(Task* task)
{
    task->msgTable = D_dryfield_night_water_tank_8017DFE8;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_dryfield_night_water_tank_8017EE28, 0, 0, 0);
    if ((u32)(gGameSession->at4.loc.place - 0xA) < 2U) {
        func_dryfield_night_water_tank_8017D9DC(0);
    }
    if (gGameSession->at4.loc.place == 0xA) {
        Task_SpawnFromTable(D_dryfield_night_water_tank_8017E010, 0, 0, 0);
    }
    if (gGameSession->at4.loc.place == 0xB) {
        func_8013224C();
    }
    task->state = task->state + 1;
}

/// The room task's second state, run every frame after the entry tick: marks
/// the play time while the visit sub-id is 0xB.
static void func_dryfield_night_water_tank_8017D94C(Task* task)
{
    if (gGameSession->at4.loc.place == 0xB) {
        Gp_MarkPlayTime();
    }
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_night_water_tank_8017D984`: the entry tick, the per-frame
/// state, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_water_tank_8017D5C4 = {
    { func_dryfield_night_water_tank_8017D870, func_dryfield_night_water_tank_8017D94C, taskKill },
};

/// The room task: copies its three-state table onto the stack and runs the
/// entry for the task's current state.
void func_dryfield_night_water_tank_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_water_tank_8017D5C4;
    sp.funcs[task->state](task);
}

/// Restores the room's layout lists from their template, then offsets the six
/// `field_8` coordinates by (0, 0, -0xC8) when `arg0` is non-zero.
static void func_dryfield_night_water_tank_8017D9DC(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_water_tank_8017F4B0;
    src = &D_dryfield_night_water_tank_8017E08C;

    for (i = 0; i < 2; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
        d.vz = 0;
    } else {
        d.vx = 0;
        d.vy = 0;
        d.vz = -0xC8;
    }

    for (i = 0; i < 6; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Per-frame model update for the tank, the callback of the task entry 0 of
/// `D_dryfield_night_water_tank_8017EE28` describes (spawned by the room entry
/// task). State 0 parents the model's coordinate to `gGfxViewCoord` and places
/// it at (0xBB8, -0x34A8, -0x4D8), then advances to state 1. State 1 drives the
/// tank's slow wobble about `y`: an occasional roll re-picks the target yaw,
/// the step moves toward it 0x100 at a time, and the velocity follows 19/20 of
/// the way to that step. Every frame then sets the model's flags to 0x80 while
/// the view is 7 (0 otherwise), publishes the coordinate's `workm` translation
/// as a `VECTOR` to `func_800D7A9C`, rebuilds the coordinate's yaw matrix from
/// the accumulated angle, and clears `flg` so the world matrix is recomputed.
///
/// The coordinate's load is written through the cast expression, before the
/// object pointer is assigned: the pointer assignment has to stay a separate
/// register copy, or the overlay comes up an `addu` short.
void func_dryfield_night_water_tank_8017DB8C(Task* arg0)
{
    TmdObject* obj;
    GpCoord*   coord;
    VECTOR     vec;

    coord = arg0->extra.tmd->coords;
    obj   = arg0->extra.tmd;
    switch (arg0->state) {
        case 0:
            obj->flags        = 0;
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = 0xBB8;
            coord->coord.t[1] = -0x34A8;
            coord->coord.t[2] = -0x4D8;
            arg0->state++;
            break;
        case 1:
            if (((s32)(rand() * 100) >> 15) <= 0) {
                if (((s32)(rand() * 100) >> 15) < 0x50) {
                    D_dryfield_night_water_tank_8017EE4C = (s32)(rand() * 20) >> 7;
                } else {
                    D_dryfield_night_water_tank_8017EE4C = 0;
                }
            }
            if (D_dryfield_night_water_tank_8017EE48 < D_dryfield_night_water_tank_8017EE4C) {
                D_dryfield_night_water_tank_8017EE48 += 0x100;
            } else if (D_dryfield_night_water_tank_8017EE4C < D_dryfield_night_water_tank_8017EE48) {
                D_dryfield_night_water_tank_8017EE48 -= 0x100;
            }
            D_dryfield_night_water_tank_8017EE44 =
                (D_dryfield_night_water_tank_8017EE44 + D_dryfield_night_water_tank_8017EE48) * 19 / 20;
            D_dryfield_night_water_tank_8017EE40 += D_dryfield_night_water_tank_8017EE44;
            break;
    }
    if (gGameSession->at4.loc.view == 7) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gfx_RotMatrixY(&coord->coord, D_dryfield_night_water_tank_8017EE40 >> 8, 1);
    coord->flg = 0;
}

void func_dryfield_night_water_tank_8017DD8C(Task* unused)
{
}
