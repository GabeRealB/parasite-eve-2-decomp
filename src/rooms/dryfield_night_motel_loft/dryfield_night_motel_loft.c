#include "rooms/dryfield_night_motel_loft.h"

#include "types.h"

#include "dryfield_night_motel_loft_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

extern GpObj4C D_dryfield_night_motel_loft_801803F4[14];

/// The room's 0x7DB payload buffer.
extern ActorCommand D_dryfield_night_motel_loft_8018092C;

extern GpAreaTmdRec D_dryfield_night_motel_loft_8018081C[2];
extern GpAreaTmdRec D_dryfield_night_motel_loft_80180834[3];
extern GpAreaTmdRec D_dryfield_night_motel_loft_80180858[2];
extern GpAreaTmdRec D_dryfield_night_motel_loft_80180870[2];

ActorTransform D_dryfield_night_motel_loft_8017FB84[2] = {
    { { 0, 0, 0xFFFF, 0 }, { 0, 0, 0, 0 } },
    { { 0xFFFF, 0, 0, 0 }, { -1, 0, 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017FBB4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017FBC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_loft_8017FBD4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_motel_loft_8017FBE4[14] = {
    { { .empty = D_dryfield_night_motel_loft_8017F2D0 }, D_dryfield_night_motel_loft_8017F2D0, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017F2E0 }, D_dryfield_night_motel_loft_8017F2E0, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F2F0 }, D_dryfield_night_motel_loft_8017F390, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F3A8 }, D_dryfield_night_motel_loft_8017F4AC, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F4C4 }, D_dryfield_night_motel_loft_8017F654, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F66C }, D_dryfield_night_motel_loft_8017F770, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F788 }, D_dryfield_night_motel_loft_8017F97C, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F9AC }, D_dryfield_night_motel_loft_8017FB64, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017F2E0 }, D_dryfield_night_motel_loft_8017F2E0, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F2F0 }, D_dryfield_night_motel_loft_8017F390, NULL },
    { { .elements = D_dryfield_night_motel_loft_8017F788 }, D_dryfield_night_motel_loft_8017F97C, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017FBB4 }, D_dryfield_night_motel_loft_8017FBB4, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017FBC4 }, D_dryfield_night_motel_loft_8017FBC4, NULL },
    { { .empty = D_dryfield_night_motel_loft_8017FBD4 }, D_dryfield_night_motel_loft_8017FBD4, NULL },
};

GpPointLight D_dryfield_night_motel_loft_8017FC8C[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4605, -1890, 1498 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2703, 2293 }, { 0, 0 } }, 2081, 4561 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4638, -1878, 1027 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2703, 2293 }, { 0, 0 } }, 1441, 2901 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2362, -1878, 5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 2048, 2867 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4144, -2651, 5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 2048, 2867 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8, -1878, -1245 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2703, 2293 }, { 0, 0 } }, 1841, 4096 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4603, -1878, -1241 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2703, 2293 }, { 0, 0 } }, 1381, 3701 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -14, -1878, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2703, 2293 }, { 0, 0 } }, 1562, 3501 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5195, -1878, -1241 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2703, 2293 }, { 0, 0 } }, 2284, 5083 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0xC350, -0x61A8, -0x2710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 163, 819 }, { 0, 0 } }, 0x493E0, 0x61A80 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7, -1878, 5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1000, 1000, 1000 }, { 0, 0 } }, 4096, 7772 },
};

GpRoomCoordSet D_dryfield_night_motel_loft_8018004C[1] = {
    { 0, NULL, 10, D_dryfield_night_motel_loft_8017FC8C, 0, NULL },
};

GpObj4C D_dryfield_night_motel_loft_80180064[12] = {
    { NULL, NULL, NULL, { 3007, -1040, -417, 0 }, { { -4, -1264, -2500, 0 }, { -4, 1264, -2500, 0 }, { -3, -1264, 2489, 0 }, { -3, 1264, 2489, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 2782, -1088, -354, 0 }, { { -3, -1264, 2492, 0 }, { -3, 1264, 2492, 0 }, { -3, -1264, -2498, 0 }, { -3, 1264, -2498, 0 } }, { 4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 928, -1024, 1199, 0 }, { { 687, -1264, 1236, 0 }, { 687, 1264, 1236, 0 }, { -687, -1264, -1235, 0 }, { -687, 1264, -1235, 0 } }, { 3587, 0, -1998, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 7, 1, 0 },
    { NULL, NULL, NULL, { 991, -1056, 959, 0 }, { { -685, -1264, -1237, 0 }, { -685, 1264, -1237, 0 }, { 686, -1264, 1237, 0 }, { 686, 1264, 1237, 0 } }, { -3582, 0, 1984, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 3, 1, 0 },
    { NULL, NULL, NULL, { -1217, -1120, -1473, 0 }, { { 37, -1264, -1420, 0 }, { 37, 1264, -1420, 0 }, { -59, -1264, 1406, 0 }, { -59, 1264, 1406, 0 } }, { -4094, 0, -141, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1506, -1088, -1665, 0 }, { { 16, -1264, 1410, 0 }, { 16, 1264, 1410, 0 }, { -28, -1264, -1417, 0 }, { -28, 1264, -1417, 0 } }, { 4102, 0, -66, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -4929, -1056, -1248, 0 }, { { 275, -1264, 1373, 0 }, { 275, 1264, 1373, 0 }, { -311, -1264, -1393, 0 }, { -311, 1264, -1393, 0 } }, { 4015, 0, -852, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -4577, -1056, -1312, 0 }, { { -243, -1264, -1408, 0 }, { -243, 1264, -1408, 0 }, { 213, -1264, 1383, 0 }, { 213, 1264, 1383, 0 } }, { -4042, 0, 659, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -5187, -1056, 703, 0 }, { { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 }, { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 } }, { -1044, 0, -3962, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { -5282, -1024, 512, 0 }, { { -1367, -1264, 360, 0 }, { -1367, 1264, 360, 0 }, { 1368, -1264, -360, 0 }, { 1368, 1264, -360, 0 } }, { 1044, 0, 3971, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { -1761, -1088, 1472, 0 }, { { -17, -1264, -1414, 0 }, { -17, 1264, -1414, 0 }, { 17, -1264, 1414, 0 }, { 17, 1264, 1414, 0 } }, { -4096, 0, 46, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { -2177, -1056, 1471, 0 }, { { 15, -1264, 1415, 0 }, { 15, 1264, 1415, 0 }, { -14, -1264, -1414, 0 }, { -14, 1264, -1414, 0 } }, { 4095, 0, -43, 0 }, { 0, 0, 4096, 0 }, 1894, 0, 7, 6, 129, 0 },
};

GpObj4C D_dryfield_night_motel_loft_801803F4[14] = {
    { NULL, NULL, NULL, { 4320, -56, -2096, 0 }, { { -1056, 0, -464, 0 }, { 1056, 0, -464, 0 }, { -1056, 0, 464, 0 }, { 1056, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1152, 0, 29, 20, 2, 0 },
    { NULL, NULL, NULL, { -6240, -64, 0, 0 }, { { 128, 0, -896, 0 }, { 1280, 0, -896, 0 }, { 128, 0, 768, 0 }, { 1280, 0, 768, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1562, 2, 2, 0, 4, 0 },
    { NULL, NULL, NULL, { -4624, -64, -32, 0 }, { { -528, 0, -464, 0 }, { 528, 0, -464, 0 }, { -528, 0, 464, 0 }, { 528, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 701, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -5088, -64, -1664, 0 }, { { -656, 0, -464, 0 }, { 656, 0, -464, 0 }, { -656, 0, 464, 0 }, { 656, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 199, 0, 4089, 0 }, 801, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { 4736, -64, 1200, 0 }, { { -496, 0, -608, 0 }, { 496, 0, -608, 0 }, { -496, 0, 608, 0 }, { 496, 0, 608, 0 } }, { 0, 4105, 0, 0 }, { -4076, 0, 400, 0 }, 783, 2, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { 4768, -64, -352, 0 }, { { -496, 0, -832, 0 }, { 496, 0, -832, 0 }, { -496, 0, 832, 0 }, { 496, 0, 832, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, -1, 0 }, 968, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 4736, -64, -1856, 0 }, { { -496, 0, -608, 0 }, { 496, 0, -608, 0 }, { -496, 0, 608, 0 }, { 496, 0, 608, 0 } }, { 0, 4105, 0, 0 }, { -4076, 0, -403, 0 }, 783, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { -496, -64, 784, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { -2, 0, 4095, 0 }, 1492, 2, 13, 0, 2, 0 },
    { NULL, NULL, NULL, { -3488, -64, 800, 0 }, { { -1424, 0, -400, 0 }, { 1424, 0, -400, 0 }, { -1424, 0, 400, 0 }, { 1424, 0, 400, 0 } }, { 0, 4108, 0, 0 }, { -2, 0, 4095, 0 }, 1476, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { -512, -64, -864, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 1, 0, -4096, 0 }, 1492, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { -3456, -64, -832, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 1, 0, -4096, 0 }, 1492, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { -1392, -64, 1632, 0 }, { { -944, 0, -400, 0 }, { 944, 0, -400, 0 }, { -944, 0, 400, 0 }, { 944, 0, 400, 0 } }, { 0, 4099, 0, 0 }, { 1, 0, -4096, 0 }, 1024, 2, 16, 0, 2, 0 },
    { NULL, NULL, NULL, { -4416, -64, 1632, 0 }, { { -1440, 0, -400, 0 }, { 1440, 0, -400, 0 }, { -1440, 0, 400, 0 }, { 1440, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 1, 0, -4096, 0 }, 1492, 2, 15, 0, 2, 0 },
    { NULL, NULL, NULL, { -2672, -64, -1664, 0 }, { { -1712, 0, -400, 0 }, { 1712, 0, -400, 0 }, { -1712, 0, 400, 0 }, { 1712, 0, 400, 0 } }, { 0, 4113, 0, 0 }, { -1, 0, 4096, 0 }, 1755, 2, 14, 0, 130, 0 },
};

GpAreaTmdRec D_dryfield_night_motel_loft_8018081C[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_motel_loft_80180834[3] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 16, 16, 2, 0, { 0, 0 }, D_801745DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_motel_loft_80180858[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_motel_loft_80180870[2] = {
    { 16, 16, 0, 0, { 0, 0 }, D_801445DC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_motel_loft_80180888[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CB58, D_dryfield_night_motel_loft_8018081C },
    { NULL, NULL },
    { D_map_dryfield_full_8017CB98, D_dryfield_night_motel_loft_80180834 },
    { D_map_dryfield_full_8017CBF8, D_dryfield_night_motel_loft_80180858 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CC78, D_dryfield_night_motel_loft_80180870 },
    { NULL, NULL },
};

s32 D_dryfield_night_motel_loft_801808F0[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

GpRoomParamRec D_dryfield_night_motel_loft_801808FC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_motel_loft_80180904[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_loft_801808F0 },
};

GpRoomParamRec* D_dryfield_night_motel_loft_8018090C[8] = {
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_80180904,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
    D_dryfield_night_motel_loft_801808FC,
};

ActorCommand D_dryfield_night_motel_loft_8018092C = { 0 };

static void func_dryfield_night_motel_loft_8017D808(Task* arg0);
static void func_dryfield_night_motel_loft_8017D8B0(Task* arg0);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_loft_8017D5F8(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0x1D that is not report-only (`queryOnly == 0`),
/// answers 1 while game nibble 0x61 is clear and 3 once it is set. Returns 1.
s32 func_dryfield_night_motel_loft_8017D600(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->areaId == 0x1D && in->queryOnly == ROOM_EVENT_EXECUTE) {
        nib = GameFlag_GetNibble(0x61);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->room = nib;
    }
    return 1;
}

/// Message-table handler for id 0x13F0: on command 3 (`arg2`) silences the
/// player's weapon and spawns the loft's cap-script task. Other commands do
/// nothing.
s32 func_dryfield_night_motel_loft_8017D67C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_dryfield_night_motel_loft_8017EB4C, 0, 0, 0);
    }
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_loft_8017D6BC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message-table handler for id 0x13F2: queues stage sound 0x531F0005 on
/// command 5.
s32 func_dryfield_night_motel_loft_8017D6C4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x531F0000 | 5, 0, 0);
    }
    return 0;
}

/// The loft's cap-script task: runs cap command 0x12 once game nibble 0x170 is
/// set and 3 before that, waits for it to finish, sets the nibble when the
/// script ended on event key 0x1F, then gives the player's weapon back and
/// kills itself.
void func_dryfield_night_motel_loft_8017D6F8(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_RunCapCmd(GameFlag_GetNibble(0x170) != 0 ? 0x12 : 3, 0);
            D_80115680  = 5;
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state = arg0->state + 1;
                return;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0x1F) {
                GameFlag_SetNibble(0x170, 1);
            }
            Gp_StateF0.field_4 = 0;
            Gp_MsgPlayerWeapon(1);
            taskKill(arg0);
            break;
    }
}

/// Sets the session's current room to `arg0` and mirrors it in `Mc_SaveData[0].state.location.loc.room`.
void func_dryfield_night_motel_loft_8017D7EC(u8 arg0)
{
    gGameSession->location.loc.room        = arg0;
    Mc_SaveData[0].state.location.loc.room = arg0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and, once the slot-4 task exists and game nibble 0x96 is set,
/// raises the 0x7DB payload's halfword and sends it to that task. It then sets
/// the grid for flag 0xA and advances to the next state.
static void func_dryfield_night_motel_loft_8017D808(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_motel_loft_8017EB1C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x96) != 0) {
        D_dryfield_night_motel_loft_8018092C.command = 1;
        Gp_DispatchMsgPtr(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_motel_loft_8018092C, 0);
    }
    func_dryfield_night_motel_loft_8017D9BC(Gp_GetCurBit2Flag(0xA) == 2);
    arg0->state = (s32)(arg0->state + 1);
}

/// Second state of the room task, run every frame: keeps the grid in step with
/// flag 0xA, clears bit 0x40 of `field_4A` while the flag is 2, and the first
/// time collected bit 0x117 is seen with nibble 0x96 still clear and the slot-4
/// task present, sets the nibble and starts the room's event.
static void func_dryfield_night_motel_loft_8017D8B0(Task* arg0)
{
    func_dryfield_night_motel_loft_8017D9BC(Gp_GetCurBit2Flag(0xA) == 2);
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        {
            GpObj4C* object   = &D_dryfield_night_motel_loft_801803F4[1];
            object->field_4A &= 0xBF;
        }
    }
    if (Gp_HasCollectedBit(0x117) && GameFlag_GetNibble(0x96) == 0 && Gp_LookupSlot4(0)) {
        GameFlag_SetNibble(0x96, 1);
        func_800E8614(D_dryfield_night_motel_loft_8017EB78, 0);
        func_800E3FAC(0xA2, 0x15);
        Mc_SaveData[0].state.sceneEvent = 3;
    }
}

/// The room task's three states.
static const TaskFuncTable3 D_dryfield_night_motel_loft_8017D5C4 = {
    { func_dryfield_night_motel_loft_8017D808, func_dryfield_night_motel_loft_8017D8B0, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_loft_8017D5C4`.
void func_dryfield_night_motel_loft_8017D964(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_loft_8017D5C4;
    sp.funcs[task->state](task);
}
